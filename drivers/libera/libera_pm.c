/* $Id: libera_pm.c 2233 2008-10-20 12:14:46Z matejk $ */

//! \file libera_pm.c
//! Implements Libera GNU/Linux driver Post Mortem (PM) device (libera.pm).

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
#include "libera_dd.h"
#include "libera.h"


/** Libera PM Device: Called on open() 
 *
 * Takes care of proper opening of the PM device and updates the informaton
 * for access control.
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 64.
 */
static int 
libera_pm_open(struct inode *inode, struct file *file)
{
    struct libera_pm_device *dev =
        (struct libera_pm_device *) file->private_data;

    if (!dev) return -ENODEV;

    mutex_lock(&dev->sem);
    /* We do not allow writing */
    if (file->f_mode & FMODE_WRITE) {
        mutex_unlock(&dev->sem);
        return -EACCES;
    }

    /* Is it the first open() for reading */
    if (!dev->master_filp) dev->master_filp = file;
    dev->readers++;

    PDEBUG2("Opened libera.pm device: readers=%d, writers=%d\n", 
           dev->readers, dev->writers);

    dev->open_count++; /* internal counter */
    mutex_unlock(&dev->sem);
    
    return 0; /* success */
}


/** Libera PM Device: Called on close() 
 *
 * Takes care of proper closing of the PM device and updates the informaton
 * for access control.
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 64.
 */
static int 
libera_pm_release(struct inode *inode, struct file *file)
{
    struct libera_pm_device *dev =
        (struct libera_pm_device *) file->private_data;


    dev->open_count--; /* internal counter */
    dev->readers--;
    
    /* Check whether the last reader is closing */
    if (!dev->readers) dev->master_filp = NULL;
    

    PDEBUG2("Closed libera.pm device: readers=%d, writers=%d\n", 
           dev->readers, dev->writers);

    return 0; /* success */
}


/** Libera PM Device: Called on read() 
 *
 * Reads Post Mortem data from internal buffer and paases it to userland. 
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 64.
 */
static ssize_t 
libera_pm_read(struct file *file, char *buf, size_t count, loff_t *f_pos)
{
    struct libera_pm_device *dev =
        (struct libera_pm_device *) file->private_data;
    
    ssize_t ret = 0;
    unsigned span_atoms;
    libera_atom_dd_t *begin;
    
    /* Lock the whole device */
    if (mutex_lock_interruptible(&dev->sem))
        return -ERESTARTSYS;
    
    /* Sanity check regarding count */
    if(count > (pmsize*sizeof(libera_atom_dd_t))) {
        PDEBUG("PM: read(): Parameter count too big.\n");
        ret = -EINVAL;
        goto out;
    }
    
    /* Filter out strange, non-atomically-dividable values. */
    if (count % sizeof(libera_atom_dd_t)) {
        PDEBUG("PM: read(): Inapropriate count size.\n");
        ret = -EINVAL;
        goto out;
    }
    span_atoms = count/sizeof(libera_atom_dd_t);
     
    begin = dev->buf + (pmsize - span_atoms);
    if (copy_to_user(buf, begin, span_atoms*sizeof(libera_atom_dd_t))) {
        ret = -EFAULT;
        goto out;
    }
    else 
    {
        buf += span_atoms*sizeof(libera_atom_dd_t);
        ret += span_atoms*sizeof(libera_atom_dd_t);
    }
    
    PDEBUG2("PM (%p): Read %d atoms from begin %p\n",
            file, span_atoms, begin);
    
 out:
    mutex_unlock(&dev->sem);
    return ret;
}


/** Libera PM Device: Called on ioctl()
 *
 * Used for PM timestamp retrieval.
 * Takes care of sanity checking of the ioctl argument and of access control.
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 128-135.
 */
static int 
libera_pm_ioctl(struct inode *inode, struct file *file,
                 unsigned int cmd, unsigned long arg)
{
    struct libera_pm_device *dev =
        (struct libera_pm_device *) file->private_data;
    
    int err = 0;
    int ret = 0;

    /* Sanity checks on cmd */
    if (_IOC_TYPE(cmd) != LIBERA_IOC_MAGIC) return -ENOTTY;
    if ((_IOC_NR(cmd) & LIBERA_IOC_MASK) != LIBERA_IOC_PM)
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
    
    /* Is this a SET_ method? */
    if (_IOC_DIR(cmd) == _IOC_WRITE)
    {
        /* File descriptor access control */
        if (dev->master_filp != file)
            return -EPERM;
    }


    /* Lock the whole device */
    if (mutex_lock_interruptible(&dev->sem))
        return -ERESTARTSYS;

    switch(cmd)
    {
        /* PM_TSTAMP: PM data timestamp */
        case LIBERA_IOC_GET_PM_TSTAMP:
        ret = copy_to_user((libera_timestamp_t *)arg,
                           &dev->tstamp,
                           sizeof(libera_timestamp_t));
        break;
        
    default:
        /* NOTE: Returning -EINVAL for invalid argument would make more sense, 
         *       but acoording to POSIX -ENOTTY should be returned in 
         *       such cases.
         */
        PDEBUG("PM: Invalid ioctl() argument (file: %s, line: %d)\n", 
               __FILE__, __LINE__);
        return -ENOTTY;

    } /* switch(cmd) */

    mutex_unlock(&dev->sem);
    
    if (err) 
        return err;
    else
        return ret;
}


/**  Libera PM Device file operations
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 66.
 */
struct file_operations libera_pm_fops = {
    owner:          THIS_MODULE,
    read:           libera_pm_read,
    ioctl:          libera_pm_ioctl,
    open:           libera_pm_open,
    release:        libera_pm_release
};
