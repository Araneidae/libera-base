/* $Id: libera_adc.c 2233 2008-10-20 12:14:46Z matejk $ */

//! \file libera_adc.c
//! Implements Libera GNU/Linux driver ADC-rate Data device (libera.adc).

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
#include "libera_adc.h"
#include "libera.h"


/** Libera ADC Device: Called on open()
 *
 * Takes care of proper opening of the DD device and updates the informaton
 * for access control.
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 64.
 */
static int
libera_adc_open(struct inode *inode, struct file *file)
{
    struct libera_adc_device *dev =
        (struct libera_adc_device *) file->private_data;
    
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
    
    PDEBUG3("Opened libera.dd device: readers=%d, writers=%d\n",
            dev->readers, dev->writers);

    dev->open_count++; /* internal counter */
    mutex_unlock(&dev->sem);
    
    return 0; /* success */
}


/** Libera ADC Device: Called on close()
 *
 * Takes care of proper closing of the ADC device and updates the informaton
 * for access control.
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 64.
 */
static int
libera_adc_release(struct inode *inode, struct file *file)
{
    struct libera_adc_device *dev =
        (struct libera_adc_device *) file->private_data;


    dev->open_count--; /* internal counter */
    dev->readers--;
    
    /* Check whether the last reader is closing */
    if (!dev->readers) dev->master_filp = NULL;


    PDEBUG3("Closed libera.dd device: readers=%d, writers=%d\n",
            dev->readers, dev->writers);

    return 0; /* success */
}


/** Libera ADC Device: Called on lseek()
 *
 * In the future, seek will be (ab)used for providing the trigger arm
 * request information to the Libera GNU/Linux driver. Currently it is 
 * not used.
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 163.
 */
static loff_t
libera_adc_llseek(struct file *file, loff_t time, int whence)
{
    loff_t ret = 0;
    
    switch(whence) {
    case 0: /* SEEK_SET */
    case 1: /* SEEK_CUR */
    case 2: /* SEEK_END */
        break;

    default: /* can't happen */
        return -EINVAL;
    }
    
    return ret;
}


/** Libera ADC Device: Called on read()
 *
 * Reads data from proper position in the circular buffer.
 * \param count denotes the No. of aoms to read in bytes + 
 * sizeof(libera_timestamp_t).
 * The returned data, via \param buf, consists of a libera_timestamp_t
 * header, representing the start of interval expressed in MT & ST, followed
 * by the raw data from Circular Buffer.
 * Only "atom-aligned" read() requests are allowed, and max. request size
 * is 4094*32 atoms.
 *
 * On success, number of read and returned bytes is returned.
 * On failure, meaningful negative errno is returned.
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 64.
 */
static ssize_t 
libera_adc_read(struct file *file, char *buf, size_t count, loff_t *f_pos)
{
    struct libera_adc_device *dev =
        (struct libera_adc_device *) file->private_data;

    ssize_t ret = 0;
    unsigned long span_atoms;
    unsigned long i;
    libera_atom_adc_t *atom; 

    DEBUG3_ONLY(static int read_syscall_counter);
    PDEBUG3("ADC read(): system call (N=%d).\n", read_syscall_counter++);

    /* Zero-length requests are supported, but do nothing useful. */
    if (!count) return ret;

    /* Lock the whole device */
    if (mutex_lock_interruptible(&dev->sem))
        return -ERESTARTSYS;

    /* Sanity check regarding count */
    if (count > (LIBERA_ADC_BUFFER_ATOMS*sizeof(libera_atom_adc_t) )) {
        PDEBUG("ADC: read(): Parameter count too big.\n");
        ret = -EINVAL;
        goto out;
    }

    /* Filter out strange, non-atomically-dividable values. */
    if (count % sizeof(libera_atom_adc_t)) {
        PDEBUG("ADC: read(): Inapropriate count size.\n");
        ret = -EINVAL;
        goto out;
    }
    span_atoms = count/sizeof(libera_atom_adc_t);
    
    /* Read to internal buffer */
    for (i=0; i < LIBERA_ADC_BUFFER_ATOMS; i++) {
        /* Channels C & D */
        PDEBUG3("ReadingCD(%lu) from %p to %p\n", i, 
               (unsigned long *)(ADC_CHAN_CD + i*sizeof(unsigned long)),
               ((unsigned long *)(dev->buf + i)));
        *((unsigned long *)(dev->buf + i)) =
          readl(iobase + ADC_CHAN_CD + i*sizeof(unsigned long));
        /* Channels A & B */
        PDEBUG3("ReadingAB(%lu) from %p to %p\n", i, 
               (unsigned long *)(ADC_CHAN_AB + i*sizeof(unsigned long)),
               (unsigned long *)(dev->buf + i) + 1);
        *((unsigned long *)(dev->buf + i) + 1) =
          readl(iobase + ADC_CHAN_AB + i*sizeof(unsigned long));
        /* Perform data transformation */
        atom = (dev->buf + i);
        if (dev->op) {
            if (dev->op(atom, atom)) {
                ret = -EFAULT;
                goto out;
            }
        }
    }
    
    /* Copy to userland */
    for (i=0; i < span_atoms; i++) {
        if (copy_to_user(buf, dev->buf + i, sizeof(libera_atom_adc_t)))
            return -EFAULT;
        else
        {
            buf += sizeof(libera_atom_adc_t);
            ret += sizeof(libera_atom_adc_t);
        }
    }

    PDEBUG3("ADC (%p): Read %lu atoms (%lu bytes)\n", 
            file, span_atoms, (unsigned long)ret);
    
 out:
    mutex_unlock(&dev->sem);
    return ret;
}


/**  Libera ADC Device file operations
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 66.
 */
struct file_operations libera_adc_fops = {
    owner:          THIS_MODULE,
    llseek:         libera_adc_llseek,
    read:           libera_adc_read,
    open:           libera_adc_open,
    release:        libera_adc_release
};
