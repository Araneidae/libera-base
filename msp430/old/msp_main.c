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

#ifndef __KERNEL__
#  define __KERNEL__
#endif

#include <linux/version.h>
#include <asm/uaccess.h>
#include <asm/delay.h>

#include "msp.h"

#warning "The following defines will most probably be removed and those from Linux headers used instead."

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
#include <mach/pxa2xx-gpio.h>
#include <mach/system.h>
#include <mach/hardware.h>
#else
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <asm/hardware.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/arch-pxa/system.h>
#endif

#include "msp.h"
#ifndef CKEN_SSP
#define CKEN_SSP CKEN3_SSP
#define RAW_WRITE(reg, value) reg = value
#define RAW_READ(reg) reg
#else
#include <asm/io.h>
#include <asm/irq.h>
#include <mach/ssp.h>
#include <mach/pxa-regs.h>
#include <mach/regs-ssp.h>
#define MMIO_BASE 0x41000000
#define RAW_WRITE(reg, value) __raw_writel(value, MMIO_BASE+reg)
#define RAW_READ(reg) __raw_readl(MMIO_BASE+reg)
#endif


/* I/O memory base address and range - default values */
static unsigned long iobase  = MSP_IOBASE;
const  unsigned long iorange = MSP_IORANGE;


/* Global MSP device */
struct msp_device msp;
msp_atom_t atom;

/* Resistor scaling coefficients */
const res_coeff_t res_coeff[9] = {
    {101, 100},
    {101, 100},
    {120, 100},
    {1511, 1000},
    {250, 100},
    {611, 100},
    {315, 100},
    {290, 100},
    {1000, 3550},
};


/* Module properties and parameters */
#if defined(MODULE)
MODULE_AUTHOR("Ales Bardorfer, Instrumentation Technologies");
MODULE_DESCRIPTION("Instrumentation Technologies MSP driver for Libera");
MODULE_LICENSE("GPL");
MODULE_SUPPORTED_DEVICE("msp");

module_param(iobase, long, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC (iobase, "I/O memory region base address (default 0x10000000)");
#endif


#ifdef MSP_IRQ    
/* IRQ */
static int ssp_irq = IRQ_SSP;
#endif

/* Interrupt handler protection spinlock */
spinlock_t msp_irq_spinlock = SPIN_LOCK_UNLOCKED;


/** Reset MSP */
static void
msp_reset(void)
{
    PDEBUG3("Reseting MSP430\n");

    writel( ((MRR_PGM | MRR_TCK) & ~MRR_NMI), iobase + MSP_RESET);
    msp_delay_jiffies(1);

    writel( (MRR_PGM | MRR_TCK |  MRR_NMI), iobase + MSP_RESET);
    msp_delay_jiffies(1);

    writel( ((MRR_NMI | MRR_TCK) & ~MRR_PGM), iobase + MSP_RESET);
    msp_delay_jiffies(1);
}


/** Transform ADC counts to power supply voltage */
static ssize_t
msp_transform(unsigned long const *raw,
              msp_atom_t *atom,
              unsigned long skip)
{
    unsigned long *data = (unsigned long *)raw + skip; // Skip protocol header
    int i;

    // Voltages
    for (i=0; i<8; i++) {
        atom->voltage[i] = (data[2*i+1] << 8) | data[2*i];
        PDEBUG2("ADC%u = 0x%04lx\n", i, atom->voltage[i]);
        // Two-step scaling to prevent overflow
        atom->voltage[i] = atom->voltage[i]*MSP_ADC_VREF/
            MSP_ADC_RANGE;
        atom->voltage[i] = atom->voltage[i]*res_coeff[i].num/
            res_coeff[i].denom;
        
        switch (i) {
        case 6: // +-12V
            atom->voltage[i] -= atom->voltage[5]*1500/1000;
            break;
        case 7: // +-5.5V
            atom->voltage[i] -= 5500*1500/1000; // +5.5V
            break;
        }
    }

    return 0;
}


/** Wait for empty SSP TX fifo */
inline void 
ssp_sync_tx(void)
{
    while ( !(RAW_READ(SSSR) & SSSR_TFS) ) {
        PDEBUG3("%s: Waiting for RAW_READ(SSSR)_TFS\n", __FUNCTION__);
        msp_delay_jiffies(1);
    }
    while (RAW_READ(SSSR) & SSSR_BSY) {
        PDEBUG3("%s: Waiting for ~SSSR_BSY\n", __FUNCTION__);
        msp_delay_jiffies(1);
    }
}

/** Wait for non-empty SSP RX fifo */
inline void 
ssp_sync_rx(void)
{
    while ( !(RAW_READ(SSSR) & SSSR_RFS) ) {
        PDEBUG3("%s: Waiting for SSSR_RFS\n", __FUNCTION__);
        msp_delay_jiffies(1);
    }
    while (RAW_READ(SSSR) & SSSR_BSY) {
        PDEBUG3("%s: Waiting for ~SSSR_BSY\n", __FUNCTION__);
        msp_delay_jiffies(1);
    }
}

/** Exchange one word over SSP */
inline unsigned long
ssp_exchange(unsigned long tx)
{
    unsigned long rx;

    ssp_sync_tx();
    RAW_WRITE(SSDR, tx & 0xffff);
    ssp_sync_rx();
    rx = RAW_READ(SSDR) & 0xffff;

    msp_delay_jiffies(1);
    
    PDEBUG2("TX: 0x%02lx  RX: 0x%02lx\n", tx, rx);
    return rx;
}


/** Read data from MSP */
static inline ssize_t
msp_transfer(char *buf, size_t atoms)
{
    static struct msp_device *dev = &msp;
    unsigned long rx;
    unsigned long rx_eoc;
    ssize_t ret = 0;
    int i, j;

    for(i=0; i<atoms; i++) {

        /* Initiate new transfer */
        ssp_exchange(MSP_PROTO_CONV_START);
        while( (rx_eoc = ssp_exchange(MSP_PROTO_CONV_TEST)) ==
               MSP_PROTO_CONV_IN_PROGRESS ) {
            msp_delay_jiffies(1);
            PDEBUG3("%s: Waiting for EOC.\n", __FUNCTION__);
        }

        dev->RX.seq = 0;
        if ( (rx = ssp_exchange(dev->RX.seq)) !=
             (MSP_PROTO_CONV_TEST + 1) ) {
            PDEBUG("EBADE: TX: 0x%02lx  RX: 0x%02lx  RX_EOC: 0x%02lx\n",
                   dev->RX.seq, rx, rx_eoc);
            return -EBADE;
        }
            
        for (j=1; j<MSP_PROTO_END; j++) {
            dev->RX.buf[dev->RX.seq++] = ssp_exchange(j);
        }

        dev->RX.buf[dev->RX.seq++] = ssp_exchange(MSP_PROTO_END - 1);

        /* Transform & deliver the received data */
        if (msp_transform(dev->RX.buf, &atom, MSP_PROTO_SKIP))
            return -ERANGE;

        if (copy_to_user(buf, &atom, sizeof(msp_atom_t)))
            return -EFAULT;
        else {
            ret += sizeof(msp_atom_t);
            buf += sizeof(msp_atom_t);
        }
    }

    DEBUG2_ONLY({
        int k;
        MSP_LOG("MSP data:\n");
        
        for (k=0; k<8; k++) {
            MSP_LOG("0x%02lx 0x%02lx 0x%02lx 0x%02lx\n",
                    dev->RX.buf[4*k+0],
                    dev->RX.buf[4*k+1],
                    dev->RX.buf[4*k+2],
                    dev->RX.buf[4*k+3]);
        }
    });

    return ret;
}


/** Read data from MSP using IRQ */
static inline ssize_t
msp_transfer_irq(char *buf, size_t atoms)
{
    static struct msp_device *dev = &msp;
    wait_queue_t wait;
    msp_atom_t atom;
    ssize_t ret = 0;
    int i;


    for(i=0; i<atoms; i++) {

        /* Initiate new transfer */
        dev->RX.seq = 0;
        RAW_WRITE(SSDR, 0x0);

        /* Wait the MSP transfer protocol to finish */
        if (dev->RX.seq < MSP_PROTO_END)
        {
            init_waitqueue_entry(&wait,current);
            add_wait_queue(&dev->RX_queue,&wait);
            for (;;)
            {
                set_current_state(TASK_INTERRUPTIBLE);
                if (!(dev->RX.seq < MSP_PROTO_END)) {
                    break;
                }
                if (!signal_pending(current)) {
                    schedule();
                    continue;
                }
                ret = -ERESTARTSYS;
                break;
            }
            set_current_state(TASK_RUNNING);
            remove_wait_queue(&dev->RX_queue, &wait);
        }

        if (ret) return ret;
        
        /* Transform & deliver the received data */
        if (msp_transform(dev->RX.buf, &atom, MSP_PROTO_SKIP))
            return -ERANGE;

        if (copy_to_user(buf, &atom, sizeof(msp_atom_t)))
            return -EFAULT;
        else {
            ret += sizeof(msp_atom_t);
            buf += sizeof(msp_atom_t);
        }
    }

    DEBUG_ONLY({
        int k;
        MSP_LOG("MSP data:\n");
        
        for (k=0; k<8; k++) {
            MSP_LOG("0x%02lx 0x%02lx 0x%02lx 0x%02lx\n",
                    dev->RX.buf[4*k+0],
                    dev->RX.buf[4*k+1],
                    dev->RX.buf[4*k+2],
                    dev->RX.buf[4*k+3]);
        }
    });

    return ret;
}


#ifdef MSP_IRQ    
/** MSP interrupt handler 
 *
 *
 * \param irq The IRQ number.
 * \param dev_id Client data (unused).
 * \param regs Processor registers before the interrupt.
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 267.
 */
static void
msp_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
    static struct msp_device *dev = &msp;

    unsigned long flags;
    unsigned long sssr;
    DEBUG3_ONLY(static unsigned long count);
    
    /* Check if it's our IRQ, otherwise return immediately! */
    if (irq != ssp_irq) return; 
    
    spin_lock_irqsave(&msp_irq_spinlock, flags);
    
    /* Get the status */
    sssr = RAW_READ(SSSR);
    PDEBUG3("MSP IRQ(%lu): Status = 0x%08lx\n", count++, sssr);
    
    if (sssr & SSSR_RFS) {
        /* Read data in RX FIFO */
        dev->RX.buf[dev->RX.seq++] = RAW_READ(SSDR);
        udelay(25);

        if (dev->RX.seq < MSP_PROTO_END) {
            RAW_WRITE(SSDR, dev->RX.seq);
        } else
            wake_up_interruptible(&dev->RX_queue);
    }
    spin_unlock_irqrestore(&msp_irq_spinlock, flags);
}
#endif

/** Called on open() system call.
 *
 * Takes care of proper opening of the MSP device and updates the information
 * for access control.
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 63.
 */
static int 
msp_open(struct inode *inode, struct file *file)
{
    unsigned int minor = MINOR(inode->i_rdev);
    struct msp_device *dev = &msp;

    /* Check minor number overflow */
    if (minor > MSP_MAX_MINOR) 
	return -ENODEV;

    /* Make device data the private data */
    file->private_data = (void *) &msp; // dev[minor];

    down(&dev->sem);
    /* We do not allow writing */
    if (file->f_mode & FMODE_WRITE) {
	up(&dev->sem);
	return -EACCES;
	// TODO: Consider returning Operation not permitted (-EPERM)
    }

    /* Is it the first open() for reading */
    if (file->f_mode & FMODE_READ) 
    {
	if (!dev->master_filp)
	{
	    dev->master_filp = file;
	}
        dev->readers++;
    }


    PDEBUG2("Opened msp (%p): readers=%d, writers=%d\n", 
            file, dev->readers, dev->writers);

    dev->open_count++; /* internal counter */
    up(&dev->sem);
    
    return 0;          /* success */
}


/** Called on close() system call.
 *
 * Because of our dispatch mechanism on open(), this function should never 
 * be called.
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 63.
 */
static int 
msp_release(struct inode *inode, struct file *file)
{
    struct msp_device *dev =
	(struct msp_device *) file->private_data;

    
    down(&dev->sem);	
    
    dev->open_count--; /* internal counter */
    dev->readers--;
    
    /* Check whether the last reader is closing */
    if (!dev->readers) {
	dev->master_filp = NULL;
    }
    
    up(&dev->sem);

    PDEBUG2("Closed msp device: readers=%d, writers=%d\n", 
	   dev->readers, dev->writers);

    return 0; /* success */
}


/** Called on read() system call on a device.
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 63.
 */
static ssize_t 
msp_read(struct file *file, char *buf, size_t count, loff_t *f_pos)
{
    struct msp_device *dev =
    	(struct msp_device *) file->private_data;

    ssize_t ret = 0;
    unsigned long atoms;
    
    /* Lock the whole device */
    if (down_interruptible(&dev->sem))
	return -ERESTARTSYS;

    /* Sanity check regarding count */
    if (count > (MSP_MAX_INTERVAL*sizeof(msp_atom_t))) {
	PDEBUG("read(): Parameter count too big.\n");
	ret = -EINVAL;
	goto out;
    }

    /* Filter out strange, non-atomically-dividable values. */
    if (count % sizeof(msp_atom_t)) {
	PDEBUG("read(): Inapropriate count size.\n");
    	ret = -EINVAL;
	goto out;
    }
    atoms = count/sizeof(msp_atom_t);

    /* Zero-atom-length request returns no data */
    if (!atoms)
	goto out;

#ifdef MSP_IRQ
    ret = msp_transfer_irq(buf, atoms);
#else
    ret = msp_transfer(buf, atoms);
#endif
    
 out:
    PDEBUG3("read() returning %d\n", ret);
    up(&dev->sem);
    return ret; 
}


/** MSP file operations 
 *
 * For details see: Alessandro Rubini et al., Linux Device Drivers, pp. 66.
 */
static struct file_operations msp_fops = {
    owner:          THIS_MODULE,
    read:           msp_read,
    open:           msp_open,
    release:        msp_release,
};


/** Initialize MSP internal device structure.
 *
 * Initializes MSP internal device structure.
 */
static void msp_init_dev(void)
{
    struct msp_device   *dev   = &msp;
    
    /* no junk in structure */
    memset(dev,   0, sizeof(struct msp_device));

    dev->spinlock = SPIN_LOCK_UNLOCKED;
    sema_init(&dev->sem, 1);
    init_waitqueue_head(&dev->RX_queue);
    
}


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
    unsigned long sscr1 = 0x10; // SSCR1_SP
    int ret;


    printk(MSP_SYSLOG_LEVEL "MSP version %s (%s %s)\n",
	   XSTR( RELEASE_VERSION ), __DATE__, __TIME__);


    /* Initialize MSP internal dev structures */
    msp_init_dev();

    /* I/O memory resource allocation */
    iobase = (unsigned)ioremap_nocache(iobase, iorange);
    ret = check_mem_region(iobase, iorange);
    if (ret < 0) 
    {
	MSP_LOG("Unable to claim I/O memory, range 0x%lx-0x%lx\n", 
		   iobase, iobase+iorange-1);
	if (ret == -EINVAL)
	    MSP_LOG("Invalid I/O memory range.\n");
	else if (ret == -EBUSY)
	    MSP_LOG("I/O memory already in use.\n");
	else 
	    MSP_LOG("check_mem_region() returned %d.\n", ret);
	goto err_IOmem;
    } 
    else if (request_mem_region(iobase, iorange, MSP_NAME) == NULL) 
    {
	MSP_LOG("I/O memory region request failed (range 0x%lx-0x%lx)\n", 
		   iobase, iobase+iorange-1);
	ret = -ENODEV;
	goto err_IOmem;
    }
    MSP_LOG("I/O memory range 0x%lx-0x%lx\n", 
	       iobase, iobase+iorange-1);

    /* Register character device for communication via VFS */
    ret = register_chrdev(MSP_MAJOR, MSP_NAME, &msp_fops);
    if (ret < 0) {
	MSP_LOG("Unable to register major %d.\n", MSP_MAJOR);
	if (ret == -EINVAL)
	    MSP_LOG("Invalid major number.\n");
	else if (ret == -EBUSY)
	    MSP_LOG("Device with major %d already registered.\n", 
		       MSP_MAJOR);
	else 
	    MSP_LOG("register_chrdev() returned %d.\n", ret);	    
	
	goto err_ChrDev;
    }

#ifdef MSP_IRQ    
    /* IRQ */
    pxa_gpio_mode(gpio);
    set_irq_type(IRQ_GPIO(gpio),IRQT_RISING);  // <-- not yet tested  Janko
    /* Interrupt handler registration */
    ret = request_irq(ssp_irq, msp_interrupt,
		      SA_INTERRUPT, MSP_NAME, NULL);
    if (ret) {
	MSP_LOG("Can't register interrupt on IRQ %i\n", ssp_irq);
	ssp_irq = -1;
	goto err_IRQ;
    }
    else {
	MSP_LOG("Registered interrupt on IRQ %i\n", ssp_irq);
        sscr1 |= SSCR1_RIE | SSCR1_RFT_M(0);
    }
#endif


    /* HW initialization */
    CKEN |= CKEN_SSP;
    RAW_WRITE(SSCR0, SSCR0_DSS_M(8) | SSCR0_FORMAT_M_SPI | SSCR0_SCR_M(MSP_BAUD_DIVISOR));

    RAW_WRITE(SSCR1, sscr1);
    
    GPDR0 |= (1 << GPIO23_SCLK) | (1 << GPIO24_SFRM) | (1 << GPIO25_STXD);
    GPDR0 &= ~( (1 << GPIO26_SRXD) | (1 << GPIO27_SEXTCLK) );

    GAFR0_U &= GAFR_SSP_ZERO;
    GAFR0_U |= GAFR_SSP_MASTER;

    RAW_WRITE(SSCR0, SSCR0_DSS_M(8) | SSCR0_FORMAT_M_SPI | SSCR0_SCR_M(MSP_BAUD_DIVISOR) | SSCR0_SSE);

    msp_reset();

    /* Module loaded OK */
    return 0; 

#ifdef MSP_IRQ    
 err_IRQ:
#endif
    unregister_chrdev(MSP_MAJOR, MSP_NAME);

 err_ChrDev:
    release_mem_region(iobase,iorange);

 err_IOmem:
    
    return ret;
}


/** MSP driver cleanup
 *
 * Deinstalls the interrupt handler, unregisters the character device and
 * frees the I/O memory region.
 */
static void __exit msp_exit(void) 
{
#ifdef MSP_IRQ
    /* Uninstall interrupt handler */
    if (ssp_irq >= 0) {
	// TODO: Add any hardware related calls for interrupt handler removal
        free_irq(ssp_irq, NULL);
    }
#endif

    /* Unregister character device for communication via VFS */
    unregister_chrdev(MSP_MAJOR, MSP_NAME);

    /* Release I/O memory regions */
    if (iobase != 0) {
        release_mem_region(iobase, iorange);
        iounmap((void *)(iobase & PAGE_MASK));
    }
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
