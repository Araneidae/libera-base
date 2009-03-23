#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>

#include <asm/arch/ssp.h>

MODULE_LICENSE("Dual BSD/GPL");


struct ssp_dev ssp_dev;


static int __init msp_init(void) 
{
    printk("<1> Loading simple\n");
    int rc = ssp_init(&ssp_dev, 1, SSP_NO_IRQ);
    printk("<1> ssp_init => %d\n", rc);
    if (rc == 0)
    {
        printk("<1> device: %s\n", ssp_dev.ssp->pdev->name);
        printk("<1> mmio: %p, base: %08lx, label: %s\n",
            ssp_dev.ssp->mmio_base, ssp_dev.ssp->phys_base,
            ssp_dev.ssp->label);

        ssp_disable(&ssp_dev);
        ssp_config(&ssp_dev,
            (SSCR0_DataSize(8) & SSCR0_DSS) | SSCR0_Motorola,
            SSCR1_SPH, 0, SSCR0_SerClkDiv(32))
//        gpio stuff
        ssp_enable(&ssp_dev);
//        reset stuff
            
    }
    return rc; 
}


static void __exit msp_exit(void) 
{
    printk("<1> Unloading simple\n");
    ssp_exit(&ssp_dev);
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
