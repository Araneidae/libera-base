/* $Id: libera_dsc.c 2233 2008-10-20 12:14:46Z matejk $ */

//! \file libera_dsc.c
//! Implements Libera GNU/Linux driver Digital Signal Conditioning (DSC) device
//! (libera.dsc).

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


/** Libera DSC Device: Called on open()
 *
 * Takes care of proper opening of the DSC device and updates the informaton
 * for access control.
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 64.
 */
static int
libera_dsc_open(struct inode *inode, struct file *file)
{
        struct libera_dsc_device *dev =
        (struct libera_dsc_device *) file->private_data;

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

    PDEBUG2("Opened libera.dsc (%p): readers=%d, writers=%d\n", 
           file, dev->readers, dev->writers);

    dev->open_count++; /* internal counter */
    mutex_unlock(&dev->sem);
    
         
    return 0; /* success */
}


/** Libera DSC Device: Called on close()
 *
 * Takes care of proper opening of the DSC device and updates the informaton
 * for access control.
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 64.
 */
static int
libera_dsc_release(struct inode *inode, struct file *file)
{
    struct libera_dsc_device *dev =
        (struct libera_dsc_device *) file->private_data;


    dev->open_count--; /* internal counter */

    if (file->f_mode & FMODE_READ)  dev->readers--;
    if (file->f_mode & FMODE_WRITE) {
        dev->writers--;
        /* Check whether the last writer is closing */
        if (!dev->writers)
            dev->master_filp = NULL;
    }
  

    PDEBUG2("Closed libera.dsc (%p): readers=%d, writers=%d\n", 
           file, dev->readers, dev->writers);

    return 0; /* success */
}


/** Libera DSC Device: Called on lseek()
 *
 * Sets the DSC block position.
 * Limitations: Due to 32-bit FPGA addressing, the DSC block position
 *              must be 32-bit aligned.
 * 
 * On success the new DSC block position is returned.
 * On failure a meaningful negative errno is returned.
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 163.
 */
static loff_t
libera_dsc_llseek(struct file *file, loff_t off, int whence)
{

    loff_t newpos;
    
    switch(whence) {
    case 0: /* SEEK_SET */
        newpos = off;

        break;

    case 1: /* SEEK_CUR */
        newpos = file->f_pos + off;
        break;

    case 2: /* SEEK_END */
        newpos = DSC_BLOCKSIZE + off;
        break;

    default: /* can't happen */
        return -EINVAL;
    }

    /* Sanity checking */
    if ((newpos < 0) || (newpos > DSC_BLOCKSIZE))
        return -EINVAL;
    /* Filter out strange, non-atomically-dividable values. */
    if (newpos % sizeof(libera_U32_t))
        return -EINVAL;
    
    file->f_pos = newpos;
    PDEBUG2("DSC (%p): Seeking to %lld\n", file, newpos);

    return newpos;
}


/** Libera DSC Device: Called on read()
 *
 * Reads the DSC parameter block. 
 * Limitations: \param count must be a multiple of 4 due to 32-bit FPGA
 *              addressing.
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 64.
 */
static ssize_t
libera_dsc_read(struct file *file, char *buf, 
               size_t count, loff_t *f_pos)
{
    struct libera_dsc_device *dev =
        (struct libera_dsc_device *) file->private_data;
    
    ssize_t ret = 0;
    size_t u32_count;

    /* Lock the whole device */
    if (mutex_lock_interruptible(&dev->sem))
        return -ERESTARTSYS;

    /* Cannot read beyond the end of DSC block */
    if (*f_pos >= DSC_BLOCKSIZE)
        goto out;
    if (count > DSC_BLOCKSIZE - *f_pos)
        count = DSC_BLOCKSIZE - *f_pos;

    /* Sanity check regarding count. */
    if (count % sizeof(libera_U32_t)) {
        ret = -EINVAL;
        goto out;
    }
    u32_count = count / sizeof(libera_U32_t);
    
    libera_readlBlock((unsigned long *)
                      (iobase + DSC_BLOCK + (unsigned long)*f_pos),
                      (unsigned long *)dev->buf,
                      u32_count);
    if (copy_to_user(buf, dev->buf, count)) {
        ret = -EFAULT;
        goto out;
    }
    PDEBUG2("DSC: Read %d bytes from f_pos %lld\n", 
            count, *f_pos);
    *f_pos += count;
    ret = count;
    

 out:
    mutex_unlock(&dev->sem);
    return ret;
}




/** Libera DSC Device: Called on write()
 *
 * Writes to the DSC parameter block. 
 * Limitations: \param count must be a multiple of 4 due to 32-bit FPGA
 *              addressing.
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 64.
 */
static ssize_t
libera_dsc_write(struct file *file, const char *buf, 
                size_t count, loff_t *f_pos)
{



    struct libera_dsc_device *dev =
        (struct libera_dsc_device *) file->private_data;
    
    ssize_t ret = 0;
    size_t u32_count;

    /* Lock the whole device */
    if (mutex_lock_interruptible(&dev->sem))
        return -ERESTARTSYS;

    /* No need for write access checking, 
     * as this is done in libera_dsc_open() 
     */

    /* Write only up to the end of the DSC block */
    if (*f_pos >= DSC_BLOCKSIZE)
        goto out;
    if (count > DSC_BLOCKSIZE - *f_pos)
        // TODO: Consider -ENOSPC 
        count = DSC_BLOCKSIZE - *f_pos;

    /* Sanity check regarding count. */
    if (count % sizeof(libera_U32_t)) {
        ret = -EINVAL;
        goto out;
    }
    u32_count = count / sizeof(libera_U32_t);
    
    if (copy_from_user(dev->buf, buf, count)) {
        ret = -EFAULT;
        goto out;
    }
    libera_writelBlock((unsigned long *)
                       (iobase + DSC_BLOCK + (unsigned long)*f_pos),
                       (unsigned long *)dev->buf,
                       u32_count);
    
    PDEBUG2("DSC: Wrote %d bytes on f_pos %lld\n", 
            count, *f_pos);
    *f_pos += count;
    ret = count;
    
 out:
    mutex_unlock(&dev->sem);
    return ret;
}

/** Helper function for updating and storing X/Y BCD offsets.
 * NOTE: (ab)use of global cfg_device struct for read/write of BCD
 *       offsets is not prone to race condition since these are
 *       modified here only. Returns one if updated.
 */
int check_and_update(int bcd_val, int bcd_idx, int fix_idx)
{
    struct libera_cfg_device *cfg_dev = &libera_cfg;

    if (bcd_val != cfg_dev->param[bcd_idx]) {
        PDEBUG("Changing BCD %c offset from %d to %d.\n",
            bcd_idx == LIBERA_CFG_BCD_XOFFSET ? 'X' : 'Y',
            cfg_dev->param[bcd_idx], bcd_val);
        cfg_dev->param[bcd_idx] = bcd_val;
        writel(bcd_val + cfg_dev->param[fix_idx], iobase + FA_OFFSET +
            (fix_idx - LIBERA_CFG_CUSTOM_FIRST)*sizeof(unsigned long));
        return 1;
    }
    return 0;
}

/** Libera DSC Device: Called on ioctl()
 *
 * Used only for signaling the DSC environment variables to the driver
 * needed for sending the events.
 * Takes care of sanity checking of the ioctl argument and of access control.
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 128-135.
 */
static int 
libera_dsc_ioctl(struct inode *inode, struct file *file,
                unsigned int cmd, unsigned long arg)
{
    struct libera_dsc_device *dev =
        (struct libera_dsc_device *) file->private_data;
    int err = 0;
    int ret = 0;

    /* Sanity checks on cmd */
    if (_IOC_TYPE(cmd) != LIBERA_DSC_MAGIC) return -ENOTTY;
    if ((_IOC_NR(cmd) & LIBERA_IOC_MASK) != LIBERA_IOC_DSC)
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

    switch(cmd)
    {

    case LIBERA_DSC_SET_DSC:
        {
            libera_U32_t param;
            ret = copy_from_user(&param,
                                 (libera_U32_t *)arg,
                                 sizeof(libera_U32_t));
#ifdef DSC_IN_ALL_TARGETS
            if ( ( param == LIBERA_CFG_SW ) ||
                 ( param == LIBERA_CFG_LEVEL ) ||
                 ( param == LIBERA_CFG_AGC ) ||
                 ( param == LIBERA_CFG_DSC) ) {
                libera_send_event(LIBERA_EVENT_CFG, param);
            } else
                 ret = -EINVAL;
#else
            libera_send_event(LIBERA_EVENT_CFG, param);
#endif
        }
        break;

    case LIBERA_DSC_BCD_OFFSETS:
        {
            libera_offsets_t offsets;

            ret = copy_from_user(&offsets,
                (libera_offsets_t *)arg, sizeof(libera_offsets_t));
            PDEBUG2("BCD offset update, X: %d, Y: %d.\n", offsets.X, offsets.Y);

            check_and_update(offsets.X, LIBERA_CFG_BCD_XOFFSET, LIBERA_CFG_XOFFSET);
            check_and_update(offsets.Y, LIBERA_CFG_BCD_YOFFSET, LIBERA_CFG_YOFFSET);
            /* Must not send event here! The CFG_LEVEL event will be
             * generated and will cause CSPI to update cached parameters.
             * Otherwise there will be two events generated at almost the
             * same time and only one will be delivered.
             * libera_send_event(LIBERA_EVENT_CFG, LIBERA_CFG_XOFFSET);
             */
        }
        break;

    default:
        /* NOTE: Returning -EINVAL for invalid argument would make more sense, 
         *       but acoording to POSIX -ENOTTY should be returned in 
         *       such cases.
         */
        PDEBUG("DSC: Invalid ioctl() argument (file: %s, line: %d)\n", 
               __FILE__, __LINE__);
        return -ENOTTY;

    } /* switch(cmd) */

    return ret;
}


/**  Libera DSC Device file operations
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 66.
 */
struct file_operations libera_dsc_fops = {
    owner:          THIS_MODULE,
    llseek:         libera_dsc_llseek,
    read:           libera_dsc_read,
    write:          libera_dsc_write,
    ioctl:          libera_dsc_ioctl,
    open:           libera_dsc_open,
    release:        libera_dsc_release
};
