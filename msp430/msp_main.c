/* $Id: msp_main.c 2237 2008-10-21 09:24:47Z hslmatejf $ */

//! \file msp.c 
//! Implements MSP GNU/Linux driver for Libera

/*
MSP - MSP GNU/Linux device driver for Libera
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

#include <linux/module.h>
#include <linux/device.h>
#include <linux/cdev.h>

#include <asm/uaccess.h>
#include <asm/delay.h>

#include <linux/fs.h>
#include <linux/ioport.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/arch/system.h>
#include <asm/arch/regs-ssp.h>
#include <asm/arch/pxa2xx-gpio.h>

#include <asm/arch/ssp.h>



#define SSP_WRITE(reg, value) __raw_writel(value, ssp.ssp->mmio_base + reg)
#define SSP_READ(reg) __raw_readl(ssp.ssp->mmio_base + reg)


/* Default module parameters */
#define MSP_IOBASE          0x10000000
#define MSP_IORANGE         0x00000010

/* Port for MSP reset. */
#define MSP_RESET           0xe
/* MSP reset control bits */
#define MRR_PGM      0x04
#define MRR_TCK      0x02
#define MRR_NMI      0x01


/* MSP protocol defines */
#define MSP_BAUD_DIVISOR            0x0f
#define MSP_PROTO_END               0x10
#define MSP_PROTO_CONV_START        0x10
#define MSP_PROTO_CONV_IN_PROGRESS  0xef
#define MSP_PROTO_CONV_TEST         0xa5


/* MSP ADC properties */
#define MSP_ADC_BIT    12
#define MSP_ADC_RANGE  (1 << MSP_ADC_BIT)  
#define MSP_ADC_VREF   2500  // mV



/* Major and minor numbers for device files */
/* NOTE: Major numbers in ranges 60-63, 120-127 and 240-254 
   are reserved for experimental use.
   For the inclusion in the official linux kernel one has to
   apply for the assignment of a unique major number. 
*/
#define MSP_MAJOR         241


/** MSP GNU/Linux driver version */
#ifndef RELEASE_VERSION
#define RELEASE_VERSION "unknown"
#endif

#define MSP_NAME "msp"





/* Resistor scaling coefficients */
struct res_coeff {
    int num;
    int denom;
};


/** MSP atom */
struct msp_atom {
    int voltage[8];
};



/* I/O memory base address and range - default values */
static unsigned long iobase  = MSP_IOBASE;
static const  unsigned long iorange = MSP_IORANGE;

/* SSP device. */
static struct ssp_dev ssp;


/* Global MSP device */
static struct msp_atom atom;

/* Resistor scaling coefficients */
static const struct res_coeff res_coeff[9] = {
    { 101,  100 },
    { 101,  100 },
    { 120,  100 },
    { 1511, 1000 },
    { 250,  100 },
    { 611,  100 },
    { 315,  100 },
    { 290,  100 },
    { 1000, 3550 },
};


/* Module properties and parameters */
MODULE_AUTHOR("Ales Bardorfer, Instrumentation Technologies");
MODULE_DESCRIPTION("Instrumentation Technologies MSP driver for Libera");
MODULE_LICENSE("GPL");
MODULE_SUPPORTED_DEVICE("msp");

module_param(iobase, long, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC (iobase,
    "I/O memory region base address (default 0x10000000)");








DEFINE_MUTEX(msp_mutex);


/* Class used link this device into /sys. */
static struct class * msp_class;
/* Major and minor numbers allocated to this device. */
static dev_t msp_dev;
/* Character device representation of this device. */
static struct cdev msp_cdev;




/** Efficient (non-busy) uninterruptible delay in jiffies */
static void msp_delay_jiffies(int delay_jiff)
{
    set_current_state(TASK_UNINTERRUPTIBLE);
    schedule_timeout(delay_jiff);
}




/** Reset MSP */
static void msp_reset(void)
{
    writel( ((MRR_PGM | MRR_TCK) & ~MRR_NMI), iobase + MSP_RESET);
    msp_delay_jiffies(1);

    writel( (MRR_PGM | MRR_TCK |  MRR_NMI), iobase + MSP_RESET);
    msp_delay_jiffies(1);

    writel( ((MRR_NMI | MRR_TCK) & ~MRR_PGM), iobase + MSP_RESET);
    msp_delay_jiffies(1);
}


/** Transform ADC counts to power supply voltage */
static void msp_transform(
    const unsigned long const *data, struct msp_atom *atom)
{
    int i;
    for (i = 0; i < 8; i++) {
        atom->voltage[i] = (data[2*i+1] << 8) | data[2*i];
        // Two-step scaling to prevent overflow
        atom->voltage[i] =
            (atom->voltage[i] * MSP_ADC_VREF) / MSP_ADC_RANGE;
        atom->voltage[i] =
            (atom->voltage[i] * res_coeff[i].num) / res_coeff[i].denom;
    }
    atom->voltage[6] -= (atom->voltage[5] * 1500) / 1000;
    atom->voltage[7] -= (5500 * 1500) / 1000; // +5.5V
}


/** Wait for empty SSP TX fifo */
static void ssp_sync_tx(void)
{
    while ( !(SSP_READ(SSSR) & SSSR_TFS) ) {
        msp_delay_jiffies(1);
    }
    while (SSP_READ(SSSR) & SSSR_BSY) {
        msp_delay_jiffies(1);
    }
}

/** Wait for non-empty SSP RX fifo */
static void ssp_sync_rx(void)
{
    while ( !(SSP_READ(SSSR) & SSSR_RFS) ) {
        msp_delay_jiffies(1);
    }
    while (SSP_READ(SSSR) & SSSR_BSY) {
        msp_delay_jiffies(1);
    }
}

/* Exchange one word over SSP: send a word to the MSP, receive a word in
 * reply. */
static unsigned long ssp_exchange(unsigned long tx)
{
    unsigned long rx;

    ssp_sync_tx();
    SSP_WRITE(SSDR, tx & 0xffff);
    ssp_sync_rx();
    rx = SSP_READ(SSDR) & 0xffff;

    msp_delay_jiffies(1);
    
    return rx;
}


/** Read data from MSP */
static ssize_t msp_transfer(char *buf)
{
    /* Initiate new transfer */
    ssp_exchange(MSP_PROTO_CONV_START);
    unsigned long rx_eoc;
    while (rx_eoc = ssp_exchange(MSP_PROTO_CONV_TEST),
           rx_eoc == MSP_PROTO_CONV_IN_PROGRESS ) {
        msp_delay_jiffies(1);
    }

    unsigned long rx = ssp_exchange(0);
    if (rx != MSP_PROTO_CONV_TEST + 1) {
        return -EBADE;
    }

    unsigned long rx_buf[32];
    int j;
    for (j = 0; j < MSP_PROTO_END; j++) {
        rx_buf[j] = ssp_exchange(j + 1);
    }

    /* Transform & deliver the received data */
    msp_transform(rx_buf, &atom);
    if (copy_to_user(buf, &atom, sizeof(struct msp_atom)))
        return -EFAULT;
    else
        return sizeof(struct msp_atom);
}



/** Called on open() system call.
 *
 * Takes care of proper opening of the MSP device and updates the information
 * for access control.
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 63.
 */
static int msp_open(struct inode *inode, struct file *file)
{
    if (file->f_mode & FMODE_WRITE)
        return -EACCES;    // Read only device.
    else
        return 0;
}


/** Called on read() system call on a device.
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 63.
 */
static ssize_t msp_read(
    struct file *file, char *buf, size_t count, loff_t *f_pos)
{
    /* Ensure request is long enough for an entire MSP transfer. */
    if (count < sizeof(struct msp_atom))
        return -EINVAL;     // Request size too short

    if (mutex_lock_interruptible(&msp_mutex))
        return -EINTR;
    ssize_t ret = msp_transfer(buf);
    mutex_unlock(&msp_mutex);
    
    return ret; 
}


static void initialise_msp(void)
{
    /* HW initialization */

    /* Enable the SSP clock. */
    CKEN |= CKEN_SSP;
    pxa_gpio_mode(GPIO23_SCLK_MD);
    pxa_gpio_mode(GPIO24_SFRM_MD);
    pxa_gpio_mode(GPIO25_STXD_MD);
    pxa_gpio_mode(GPIO26_SRXD_MD);
    pxa_gpio_mode(GPIO27_SEXTCLK_MD);
    
    ssp_disable(&ssp);
    ssp_config(&ssp,
        (SSCR0_DataSize(8) & SSCR0_DSS) | SSCR0_Motorola,
        SSCR1_SPH, 0, SSCR0_SerClkDiv(32));
    ssp_enable(&ssp);

    msp_reset();
}


/** MSP file operations 
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 66.
 */
static struct file_operations msp_fops = {
    owner:          THIS_MODULE,
    read:           msp_read,
    open:           msp_open,
};



/** MSP driver initialization.
 *
 * Registers the character device, requests the I/O memory region and
 * installs the interrupt handler.
 * NOTE: Goto statements are generally not a good programming
 *       practice, but module initialization is AFAIK one of the few cases
 *       where goto statements are a way to go, as no one in the kernel will 
 *       clean after ourselves in case something bad happens during the 
 *       initialization.
 */
static int __init msp_init(void) 
{
    printk(KERN_INFO "MSP version %s (%s %s)\n",
           RELEASE_VERSION, __DATE__, __TIME__);

    int ret = ssp_init(&ssp, 1, SSP_NO_IRQ);
    if (ret < 0)
        goto no_ssp;

    /* I/O memory resource allocation */
    iobase = (unsigned) ioremap_nocache(iobase, iorange);
    if ((void *)iobase == NULL)
        goto no_iobase;
    ret = check_mem_region(iobase, iorange);
    if (ret < 0) 
        goto no_class;
    
    ret = -ENODEV;  // in case of certain failures
    if (request_mem_region(iobase, iorange, MSP_NAME) == NULL)
        goto no_class;
    
    msp_class = class_create(THIS_MODULE, "msp");
    if (msp_class == NULL)
        goto no_class;

    /* Allocate ourself one device node. */
    ret = alloc_chrdev_region(&msp_dev, 0, 1, "msp");
    if (ret < 0)
        goto no_chrdev;
    
    cdev_init(&msp_cdev, &msp_fops);
    msp_cdev.owner = THIS_MODULE;
    ret = cdev_add(&msp_cdev, msp_dev, 1);
    if (ret < 0) {
        printk(KERN_ERR "Unable to register major %d, error %d\n",
            MSP_MAJOR, -ret);
        goto no_cdev;
    }

    /* Bind device to class: this places appropriate entries in /dev. */
    ret = -ENODEV;  // in case of certain failures
    struct device * msp_device =
        device_create(msp_class, NULL, msp_dev, "msp0");
    if (msp_device == NULL)
        goto no_device;
    

    /* Module loaded OK */
    initialise_msp();
    return 0;
    

no_device:
    cdev_del(&msp_cdev);
no_cdev:
    unregister_chrdev_region(msp_dev, 1);
no_chrdev:
    class_destroy(msp_class);
no_class:
    release_mem_region(iobase, iorange);
    iounmap((void *)(iobase & PAGE_MASK));
no_iobase:
    ssp_exit(&ssp);
no_ssp:

    printk(KERN_ERR "Unable to initialise MSP module, error %d\n", -ret);
    return ret;
}


/** MSP driver cleanup
 *
 * Deinstalls the interrupt handler, unregisters the character device and
 * frees the I/O memory region.
 */
static void __exit msp_exit(void) 
{
    /* Remove msp0. */
    device_destroy(msp_class, msp_dev);
    /* Delete the underlying character device. */
    cdev_del(&msp_cdev);
    /* Finally we can let the device numbers go. */
    unregister_chrdev_region(msp_dev, 1);
    /* The device class is no longer useful. */
    class_destroy(msp_class);

    /* Release I/O memory regions */
    release_mem_region(iobase, iorange);
    iounmap((void *)(iobase & PAGE_MASK));

    ssp_exit(&ssp);
}


/** Register module init and cleanup functions.
 *
 * Registers module initialization and cleanup functions.
 * NOTE: This is the modern mechanism for registering a module's 
 *       initialization and cleanup functions. Doing it this way instead of 
 *       using init_module() and cleanup_module() is crucial for possible 
 *       future inclusion into a monolythic linux kernel (not a module). 
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 35.
 */
module_init(msp_init);
module_exit(msp_exit);
