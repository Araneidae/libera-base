/* $Id: libera_dd.c 2192 2008-10-07 09:13:06Z matejk $ */

//! \file libera_dd.c
//! Implements Libera GNU/Linux driver Data on Demand
//! device (libera.dd).

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

#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <asm/io.h>

#include "libera_kernel.h"
#include "libera.h"

extern loff_t
libera_dd_llseek_specific(struct file *file, 
			  loff_t time, int whence);

extern ssize_t 
libera_dd_read_specific(struct file *file, char __user *buf, 
			size_t count, loff_t *f_pos);


/** Libera DD Device: Called on open()
 *
 * Takes care of proper opening of the DD device and updates the informaton
 * for access control.
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 64.
 */
static int
libera_dd_open(struct inode *inode, struct file *file)
{
    struct libera_dd_device *dev =
	(struct libera_dd_device *) file->private_data;
    libera_dd_local_t *dd_local = NULL;
    
    if (!dev) return -ENODEV;

    mutex_lock(&dev->sem);
    /* We do not allow writing */
    if (file->f_mode & FMODE_WRITE) {
	mutex_unlock(&dev->sem);
	return -EACCES;
    }

    dev->readers++;

    /* Get a fresh dd_local per file descriptor */ 
    dd_local = (libera_dd_local_t *)
	kmalloc(sizeof(libera_dd_local_t), GFP_KERNEL);
    if (dd_local)
    {
	/* No junk in dd_localk! */
	memset(dd_local, 0, sizeof(libera_dd_local_t));
	dd_local->dec = 1;
    }
    else
    {
	mutex_unlock(&dev->sem);
	return -ENOMEM;
    }
    
    /* Store current dd_local for our reference */
    file->f_version = (unsigned long)dd_local;

    PDEBUG3("Opened libera.dd device: readers=%d, writers=%d\n",
	    dev->readers, dev->writers);

    dev->open_count++; /* internal counter */
    mutex_unlock(&dev->sem);
    
    return 0; /* success */
}


/** Libera DD Device: Called on close()
 *
 * Takes care of proper closing of the DD device and updates the informaton
 * for access control.
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 64.
 */
static int
libera_dd_release(struct inode *inode, struct file *file)
{
    struct libera_dd_device *dev =
	(struct libera_dd_device *) file->private_data;
    libera_dd_local_t *dd_local = (libera_dd_local_t *)file->f_version;


    dev->open_count--; /* internal counter */
    dev->readers--;
    
    /* Free dd_local */
    kfree(dd_local);
    file->f_version = (unsigned long)NULL;


    PDEBUG3("Closed libera.dd device: readers=%d, writers=%d\n",
	    dev->readers, dev->writers);

    return 0; /* success */
}


/** Libera DD Device: Called on lseek()
 *
 * Seek is (ab)used for providing the timing request information to the Libera
 * GNU/Linux driver. 
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 163.
 */
static loff_t
libera_dd_llseek(struct file *file, loff_t time, int whence)
{
    return libera_dd_llseek_specific(file, time, whence);
}


/** Libera DD Device: Called on read()
 *
 * Calls the libera family member specific read() function.
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 64.
 */
static ssize_t 
libera_dd_read(struct file *file, char __user *buf, size_t count, loff_t *f_pos)
{
    return libera_dd_read_specific(file, buf, count, f_pos);
}


/** Libera DD Device: Called on ioctl()
 *
 * Used for Post Filtering and DMA seettings.
 * Takes care of sanity checking of the ioctl argument and of access control.
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 128-135.
 */
static int 
libera_dd_ioctl(struct inode *inode, struct file *file,
		 unsigned int cmd, unsigned long arg)
{
    struct libera_dd_device *dev =
	(struct libera_dd_device *) file->private_data;
    libera_dd_local_t *dd_local = (libera_dd_local_t *)file->f_version;
    
    int err = 0;
    int ret = 0;

    /* Sanity checks on cmd */
    if (_IOC_TYPE(cmd) != LIBERA_IOC_MAGIC) return -ENOTTY;
    if ((_IOC_NR(cmd) & LIBERA_IOC_MASK) != LIBERA_IOC_DD)
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
    
    /* Lock the whole device */
    if (mutex_lock_interruptible(&dev->sem))
	return -ERESTARTSYS;

    switch(cmd)
    {
	/* DEC: Decimation */
    case LIBERA_IOC_GET_DEC:
	ret = copy_to_user((libera_U32_t *)arg,
			   &dd_local->dec,
			   sizeof(libera_U32_t));
	break;
    case LIBERA_IOC_SET_DEC:
	{
	    libera_U32_t NewDec;
	    ret = copy_from_user(&NewDec,
				 (libera_U32_t *)arg,
				 sizeof(libera_U32_t));
            PDEBUG2("New decimation = %d\n", NewDec);
	    if (NewDec) 
		dd_local->dec = NewDec;
	    else
		ret = -EINVAL;
	}
	/* NOTE: The new decimation will be effective after the next
	 *       DD read() call.
	 */
	break;

	/* DD_TSTAMP: PM data timestamp */
    case LIBERA_IOC_GET_DD_TSTAMP:
	ret = copy_to_user((libera_timestamp_t *)arg,
			   &dd_local->tstamp,
			   sizeof(libera_timestamp_t));
	break;
	
    default:
	/* NOTE: Returning -EINVAL for invalid argument would make more sense, 
	 *       but acoording to POSIX -ENOTTY should be returned in 
	 *       such cases.
	 */
	PDEBUG("DD: Invalid ioctl() argument (file: %s, line: %d)\n", 
	       __FILE__, __LINE__);
	return -ENOTTY;

    } /* switch(cmd) */

    mutex_unlock(&dev->sem);
    
    if (err) 
	return err;
    else
	return ret;
}


/**  Libera DD Device file operations
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 66.
 */
struct file_operations libera_dd_fops = {
    owner:          THIS_MODULE,
    llseek:	    libera_dd_llseek,
    read:           libera_dd_read,
    ioctl:          libera_dd_ioctl,
    open:           libera_dd_open,
    release:        libera_dd_release
};
