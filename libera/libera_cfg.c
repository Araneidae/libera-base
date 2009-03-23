/* $Id: libera_cfg.c 2233 2008-10-20 12:14:46Z matejk $ */

//! \file libera_cfg.c
//! Implements Libera GNU/Linux driver Configuration (CFG) device (libera.cfg).

/*
LIBERA - Libera GNU/Linux device driver
Copyright (C) 2004-2006 Instrumentation Technologies

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
or visit http://www.gnu.org
*/

#ifndef __KERNEL__
#  define __KERNEL__
#endif

#define __NO_VERSION__
#include <asm/uaccess.h>

#include "libera_kernel.h"
#include "libera.h"


extern int 
libera_cfg_get_specific(struct libera_cfg_device *dev,
			libera_cfg_request_t *req);

extern int 
libera_cfg_set_specific(struct libera_cfg_device *dev,
			libera_cfg_request_t *req);

extern libera_desc_t libera_desc;


/** Libera Configuration Device: Called on open() 
 *
 * Takes care of proper opening of the CFG device and updates the informaton
 * for access control.
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 64.
 */
static int 
libera_cfg_open(struct inode *inode, struct file *file)
{
    struct libera_cfg_device *dev =
	(struct libera_cfg_device *) file->private_data;
    
    if (!dev) return -ENODEV;
    
    mutex_lock(&dev->sem);
    /* Is it an open() for writing? */
    if (file->f_mode & FMODE_WRITE) {
	/* Is it the first open() for writing */
	if (!dev->master_filp)
	    dev->master_filp = file;
	else
	{
	    mutex_unlock(&dev->sem);
	    return -EBUSY;
	}
	dev->writers++;
    }
    if (file->f_mode & FMODE_READ)  dev->readers++;

    PDEBUG2("Opened libera.cfg (%p): readers=%d, writers=%d\n", 
	    file, dev->readers, dev->writers);

    dev->open_count++; /* internal counter */
    mutex_unlock(&dev->sem);

    return 0; /* success */
}


/** Libera Configuration Device: Called on close() 
 *
 * Takes care of proper closing of the CFG device and updates the informaton
 * for access control.
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 64.
 */
static int 
libera_cfg_release(struct inode *inode, struct file *file)
{
    struct libera_cfg_device *dev =
	(struct libera_cfg_device *) file->private_data;


    dev->open_count--; /* internal counter */

    if (file->f_mode & FMODE_READ)  dev->readers--;
    if (file->f_mode & FMODE_WRITE) {
	dev->writers--;
	/* Check whether the last writer is closing */
	if (!dev->writers)
	    dev->master_filp = NULL;
    }
  

    PDEBUG2("Closed libera.cfg (%p): readers=%d, writers=%d\n", 
	    file, dev->readers, dev->writers);

    return 0; /* success */
}


/** Libera Configuration Device: Command decoder
 *
 * Takes care of decoding and dispatching the CFG ioctl() commands.
 */
static int 
libera_cfg_cmd(struct libera_cfg_device *dev,
	       unsigned int cmd, unsigned long arg)
{   
    int ret = 0;
    libera_cfg_request_t req;
    
    switch(cmd)
    {
	
	/* Generic GET method */
    case LIBERA_IOC_GET_CFG:
	/* NOTE: Although usually a GET method would only require
	 *       copying to userland we need to read the request
	 *       first in order to get the index.
	 */
	ret = copy_from_user(&req,
			     (libera_cfg_request_t *)arg,
			     sizeof(libera_cfg_request_t));
	if (ret) return ret;
	if (req.idx >= LIBERA_CFG_PARAMS_MAX)
	    return -EINVAL;
	
	switch (req.idx) {
	    /* Libera common configuration CFG parameters */
	case LIBERA_CFG_TRIGMODE:
        case LIBERA_CFG_MCPLL:
        case LIBERA_CFG_SCPLL:
	    req.val = dev->param[req.idx];
	    break;
	    
        case LIBERA_CFG_FEATURE_CUSTOMER:
            req.val = readl(iobase + FPGA_FEATURE_CUSTOMER);
            break;
        case LIBERA_CFG_FEATURE_ITECH:
            req.val = lgbl.feature;
            break;
	default:
	    /* Libera family member specific CFG parameters */
	    ret = libera_cfg_get_specific(dev, &req);	
	}
	if (ret) return ret;
	ret = copy_to_user((unsigned char *)arg,
			   &req,
			   sizeof(libera_cfg_request_t));
	PDEBUG2("GET_CFG: idx=%u, val=%u\n", req.idx, req.val);
	break;
	
	/* Generic SET method */
    case LIBERA_IOC_SET_CFG:
	ret = copy_from_user(&req,
			     (libera_cfg_request_t *)arg,
			     sizeof(libera_cfg_request_t));
	if (ret) return ret;
	PDEBUG2("SET_CFG: idx=%u, val=%u\n", req.idx, req.val);
	if (req.idx >= LIBERA_CFG_PARAMS_MAX)
	    return -EINVAL;
	
	switch (req.idx) {
	    /* Libera common configuration CFG parameters */
	case LIBERA_CFG_TRIGMODE:
	    dev->param[req.idx] = req.val;
	    break;

        case LIBERA_CFG_MCPLL:
        case LIBERA_CFG_SCPLL:
            return -EINVAL;  // Cannot set PLL status!
            break;
	    
	default:
	    /* Libera family member specific CFG parameters */
	    ret = libera_cfg_set_specific(dev, &req);	
	}
	if (ret) return ret;
	
	/* Send notification */
	// TODO: Optimizations... what if the parameter is set to the 
	//       same value as before the SET method?
	libera_send_event(LIBERA_EVENT_CFG, req.idx);
	break;
	
	/* Libera magic number */
    case LIBERA_IOC_GET_MAGIC:
	ret = copy_to_user((int *)arg,
			   &libera_desc.magic,
			   sizeof(int));
	break;
	
	/* Libera description */
    case LIBERA_IOC_GET_DESC:
	ret = copy_to_user((libera_desc_t *)arg,
			   &libera_desc,
			   sizeof(libera_desc_t));
	break;
	
    default:
	/* NOTE: Returning -EINVAL for invalid argument would make more sense, 
	 *       but acoording to POSIX -ENOTTY should be returned in 
	 *       such cases.
	 */
	PDEBUG("CFG: Invalid ioctl() argument %d (file: %s, line: %d)\n", 
	       cmd, __FILE__, __LINE__);
	return -ENOTTY;
	
    } /* switch(cmd) */
    
    return ret;
}


/** Libera Configuration Device: Called on ioctl()
 *
 * Takes care of sanity checking of the ioctl argument and of access control.
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 128-135.
 */
static int
libera_cfg_ioctl(struct inode *inode, struct file *file,
		 unsigned int cmd, unsigned long arg)
{
    struct libera_cfg_device *dev =
	(struct libera_cfg_device *) file->private_data;
    int err = 0;

    /* Sanity checks on cmd */
    if (_IOC_TYPE(cmd) != LIBERA_IOC_MAGIC) return -ENOTTY;
    if ((_IOC_NR(cmd) & LIBERA_IOC_MASK) != LIBERA_IOC_CFG) 
    {
	ASSERT(TRUE);
	return -ENOTTY;
    }
    
    if (_IOC_DIR(cmd) & _IOC_READ)
        err = !access_ok(VERIFY_WRITE, (void *)arg, _IOC_SIZE(cmd));
    else if (_IOC_DIR(cmd) & _IOC_WRITE)
        err =  !access_ok(VERIFY_READ, (void *)arg, _IOC_SIZE(cmd));
    if (err) {
	ASSERT(TRUE);
	return -EFAULT;
    }
    
    /* Access control:
     * Only one application at a time is granted permission to change 
     * the parameters (SET_ methods). The access to the device should 
     * not be limited for reading (GET_ methods).
     */
    if (_IOC_DIR(cmd) == _IOC_WRITE) /* Is this a SET_ method? */
    {
	/* File descriptor access control */
	if (dev->master_filp != file)
	    return -EPERM;
    }
    
    /* Decode comands and take action on hardware */
    return libera_cfg_cmd(dev, cmd, arg);
}


/**  Libera Configuration Device file operations
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 66.
 */
struct file_operations libera_cfg_fops = {
    owner:          THIS_MODULE,
    llseek:         libera_llseek,      /* not specific, use default */
    read:           libera_read,        /* not specific, use default */
    write:          libera_write,       /* not specific, use default */
    ioctl:          libera_cfg_ioctl,
    open:           libera_cfg_open,
    release:        libera_cfg_release
};
