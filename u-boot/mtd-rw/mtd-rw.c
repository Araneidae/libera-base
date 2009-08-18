#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/mtd/mtd.h>



MODULE_LICENSE("GPL");


static void print_mtd(struct mtd_info *mtd, int n)
{
    if (mtd != NULL)
    {
        printk("<1> mtd%d: %08x %08x \"%s\"\n",
            n, mtd->size, mtd->flags, mtd->name);
    }
}


static void write_enable_mtd(struct mtd_info *mtd)
{
    mtd->flags |= MTD_CLEAR_BITS;
}


static void write_disable_mtd(struct mtd_info *mtd)
{
    mtd->flags &= ~MTD_CLEAR_BITS;
}


static int mtd_rw_init(void)
{
    for (int i = 0; i < MAX_MTD_DEVICES; i ++)
        print_mtd(__get_mtd_device(NULL, i), i);
    write_enable_mtd(__get_mtd_device(NULL, 0));
    write_enable_mtd(__get_mtd_device(NULL, 1));
    return 0;
}

static void mtd_rw_exit(void)
{
    write_disable_mtd(__get_mtd_device(NULL, 0));
    write_disable_mtd(__get_mtd_device(NULL, 1));
    printk("<1> Bye now.\n");
}

module_init(mtd_rw_init);
module_exit(mtd_rw_exit);

