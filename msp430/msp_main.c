/* This file is part of the MSP device driver for Libera
 * Copyright (C) 2004-2006 Instrumentation Technologies
 * Copyright (C) 2009  Michael Abbott, Diamond Light Source Ltd.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * The Libera EPICS Driver is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
 * Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Contact:
 *      Dr. Michael Abbott,
 *      Diamond Light Source Ltd,
 *      Diamond House,
 *      Chilton,
 *      Didcot,
 *      Oxfordshire,
 *      OX11 0DE
 *      michael.abbott@diamond.ac.uk
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/cdev.h>

#include <asm/uaccess.h>
#include <linux/delay.h>

#include <linux/fs.h>
#include <linux/ioport.h>

#include <asm/hardware.h>
#include <asm/io.h>
#include <asm/arch/system.h>
#include <asm/arch/regs-ssp.h>
#include <asm/arch/pxa2xx-gpio.h>

#include <asm/arch/ssp.h>


/** MSP GNU/Linux driver version */
#ifndef RELEASE_VERSION
#define RELEASE_VERSION "unknown"
#endif


/* Resistor scaling coefficients */
struct res_coeff {
    int num;
    int denom;
};


/** MSP atom */
struct msp_atom {
    int voltage[8];
};



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






/*****************************************************************************/
/*                                                                           */
/*                          MSP 430 Hardware Interface                       */
/*                                                                           */
/*****************************************************************************/


#define CPLD_IOBASE         0x10000000
#define MSP_RESET_OFFSET    0xE
#define MSP_RESET_BASE      (CPLD_IOBASE + MSP_RESET_OFFSET)



/* MSP reset control bits. */
#define MRR_PGM      0x04
#define MRR_TCK      0x02
#define MRR_NMI      0x01


/* MSP protocol defines */
#define MSP_BAUD_DIVISOR            0x0f
#define MSP_PROTO_CONV_START        0x10
#define MSP_PROTO_CONV_IN_PROGRESS  0xef
#define MSP_PROTO_CONV_TEST         0xa5


/* MSP ADC properties */
#define MSP_ADC_BIT    12
#define MSP_ADC_RANGE  (1 << MSP_ADC_BIT)  
#define MSP_ADC_VREF   2500  // mV




/* SSP device. */
static struct ssp_dev ssp;
/* MSP reset register (in CPLD address space). */
static void * msp_reset_register;



/* Efficient (non-busy) uninterruptible delay in jiffies.  This delays for
 * about 5ms in general, but is a *lot* faster than calling mdelay(1)! */
static void msp_delay_jiffies(int delay_jiff)
{
    set_current_state(TASK_UNINTERRUPTIBLE);
    schedule_timeout(delay_jiff);
}



/* Reset MSP */
static void msp_reset(void)
{
    iowrite8((MRR_PGM | MRR_TCK) & ~MRR_NMI, msp_reset_register);
    msp_delay_jiffies(1);

    iowrite8(MRR_PGM | MRR_TCK | MRR_NMI, msp_reset_register);
    msp_delay_jiffies(1);

    iowrite8((MRR_NMI | MRR_TCK) & ~MRR_PGM, msp_reset_register);
    msp_delay_jiffies(1);
}


/* Transform ADC counts to power supply voltage */
static void msp_transform(
    const unsigned long const *data, struct msp_atom *atom)
{
    int i;
    for (i = 0; i < 8; i++)
    {
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


/* Our access to the SSP uses the IO configuration set up by ssp_init(), but
 * we access the device directly as that driver's functionality is somewhat
 * broken. */
#define SSP_WRITE(reg, value) __raw_writel(value, ssp.ssp->mmio_base + reg)
#define SSP_READ(reg) __raw_readl(ssp.ssp->mmio_base + reg)

/* Exchange one word over SSP: send a word to the MSP, receive a word in
 * reply.  Because this is how we always work we know that the SSP TX FIFO
 * will be empty, and we should get a reply pretty quickly. */
static unsigned long ssp_exchange(unsigned long tx)
{
    SSP_WRITE(SSDR, tx);
    while ((SSP_READ(SSSR) & SSSR_RNE) == 0)
        msp_delay_jiffies(1);
    return SSP_READ(SSDR) & 0xffff;
}



/* Read data from MSP */
static ssize_t msp_transfer(char *buf)
{
    /* Initiate new transfer: send a conversion start command, and then wait
     * for conversion to complete. */
    ssp_exchange(MSP_PROTO_CONV_START);
    while (ssp_exchange(MSP_PROTO_CONV_TEST) == MSP_PROTO_CONV_IN_PROGRESS)
        msp_delay_jiffies(1);

    if (ssp_exchange(0) != MSP_PROTO_CONV_TEST + 1) 
        return -EBADE;

    unsigned long rx_buf[32];
    int j;
    for (j = 0; j < 16; j++) 
        rx_buf[j] = ssp_exchange(j + 1);

    /* Transform & deliver the received data */
    struct msp_atom atom;
    msp_transform(rx_buf, &atom);
    if (copy_to_user(buf, &atom, sizeof(struct msp_atom)))
        return -EFAULT;
    else
        return sizeof(struct msp_atom);
}




/* This initialises the MSP hardware resources.  These comprise:
 *  1.  The SSP (Synchronous Serial Port) interface
 *  2.  A register in the CPLD for resetting the MSP.
 * We use as much of the kernel's own SSP interface as we can, but it's a bit
 * broken. */
static int initialise_msp(void)
{
    /* First try to capture the SSP resources. */
    int ret = ssp_init(&ssp, 1, SSP_NO_IRQ);
    if (ret < 0)
    {
        printk(KERN_ERR "Unable to initialise SSP resource: %d\n", -ret);
        goto no_ssp;
    }
    /* Next, I/O memory resource allocation.  We only need the MSP reset
     * register. */
    if (request_mem_region(MSP_RESET_BASE, 1, "msp") == NULL)
    {
        printk(KERN_ERR "Unable to allocate MSP reset register\n");
        ret = -ENODEV;
        goto no_region;
    }
    /* Map the reset register. */
    msp_reset_register = ioremap_nocache(MSP_RESET_BASE, 1);
    if (msp_reset_register == NULL)
    {
        printk(KERN_ERR "Unable to remap MSP reset register\n");
        ret = -ENODEV;
        goto no_iobase;
    }
    

    /* Enable the SSP clock. */
    CKEN |= CKEN_SSP;
    /* Configure all of the SSP GPIOs. */
    pxa_gpio_mode(GPIO23_SCLK_MD);
    pxa_gpio_mode(GPIO24_SFRM_MD);
    pxa_gpio_mode(GPIO25_STXD_MD);
    pxa_gpio_mode(GPIO26_SRXD_MD);
    pxa_gpio_mode(GPIO27_SEXTCLK_MD);

    /* Configure the SSP to talk to the MSP. */
    ssp_disable(&ssp);
    ssp_config(&ssp,
        (SSCR0_DataSize(8) & SSCR0_DSS) | SSCR0_Motorola,
        SSCR1_SPH, 0, SSCR0_SerClkDiv(32));
    ssp_enable(&ssp);

    /* Finally command the MSP to reset. */
    msp_reset();
    return 0;

    
no_iobase:
    release_mem_region(MSP_RESET_BASE, 4);
no_region:
    ssp_exit(&ssp);
no_ssp:
    return ret;
}


static void uninitialise_msp(void)
{
    iounmap(msp_reset_register);
    release_mem_region(MSP_RESET_BASE, 1);
    ssp_exit(&ssp);
}




/*****************************************************************************/
/*                                                                           */
/*                           Device Driver Interface                         */
/*                                                                           */
/*****************************************************************************/


/* Class used link this device into /sys. */
static struct class * msp_class;
/* Major and minor numbers allocated to this device. */
static dev_t msp_dev;
/* Character device representation of this device. */
static struct cdev msp_cdev;


DEFINE_MUTEX(msp_mutex);




/* The only thing to do on open is to enforce read-only access. */
static int msp_open(struct inode *inode, struct file *file)
{
    if (file->f_mode & FMODE_WRITE)
        return -EACCES;    // Read only device.
    else
        return 0;
}


/* We enforce reading a single block of readings from the MSP.  Also, because
 * reading from the MSP takes such a long time, we make the lock
 * interruptible (and it might be nice to allow the msp_transfer to be
 * interruptible as well). */
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


/* MSP file operations. */
static struct file_operations msp_fops = {
    owner:          THIS_MODULE,
    open:           msp_open,
    read:           msp_read,
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

    /* Initialise the device before doing anything else. */
    int ret = initialise_msp();
    if (ret < 0)
        goto no_msp;

    /* Allocate ourself one device node. */
    ret = alloc_chrdev_region(&msp_dev, 0, 1, "msp");
    if (ret < 0)
    {
        printk(KERN_ERR "Unable to allocate dev for msp: %d\n", -ret);
        goto no_chrdev;
    }

    /* Publish the newly created file operations.  At this point it is
     * possible for the device to start being used. */
    cdev_init(&msp_cdev, &msp_fops);
    msp_cdev.owner = THIS_MODULE;
    ret = cdev_add(&msp_cdev, msp_dev, 1);
    if (ret < 0)
    {
        printk(KERN_ERR "Unable to register msp device: %d\n", -ret);
        goto no_cdev;
    }

    /* Bind device to class: this places appropriate entries in /dev. */
    msp_class = class_create(THIS_MODULE, "msp");
    if (msp_class == NULL)
    {
        printk(KERN_ERR "Unable to create msp class\n");
        ret = -ENODEV;
        goto no_class;
    }
    struct device * msp_device =
        device_create(msp_class, NULL, msp_dev, "msp0");
    if (msp_device == NULL)
    {
        printk(KERN_ERR "Unable to create msp device in class\n");
        ret = -ENODEV;
        goto no_device;
    }
    return 0;
    

no_device:
    class_destroy(msp_class);
no_class:
    cdev_del(&msp_cdev);
no_cdev:
    unregister_chrdev_region(msp_dev, 1);
no_chrdev:
    uninitialise_msp();
no_msp:

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
    device_destroy(msp_class, msp_dev);
    class_destroy(msp_class);
    cdev_del(&msp_cdev);
    unregister_chrdev_region(msp_dev, 1);
    uninitialise_msp();
}


module_init(msp_init);
module_exit(msp_exit);
