#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/mtd/mtd.h>



MODULE_LICENSE("GPL");


static void print_mtd(struct mtd_info *mtd)
{
    if (mtd == NULL)
    {
        printk("<1> mtd @ %p\n", mtd);
        return;
    }
    printk("<1> mtd @ %p\n", mtd);
    printk("<1> name = %s\n", mtd->name);
    printk("<1> flags = %08x\n", mtd->flags);
    printk("<1> size = %08x\n", mtd->size);
}


static void write_enable_mtd(struct mtd_info *mtd)
{
    mtd->flags |= MTD_CLEAR_BITS;
}


static int mtd_rw_init(void)
{
    for (int i = 0; i < MAX_MTD_DEVICES; i ++)
        print_mtd(__get_mtd_device(NULL, i));
    write_enable_mtd(__get_mtd_device(NULL, 0));
    write_enable_mtd(__get_mtd_device(NULL, 1));
    return 0;
}

static void mtd_rw_exit(void)
{
    printk("<1> Bye now.\n");
}

module_init(mtd_rw_init);
module_exit(mtd_rw_exit);

