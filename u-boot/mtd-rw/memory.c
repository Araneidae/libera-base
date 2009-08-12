#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <asm/uaccess.h>

#include <linux/string.h>


static int memory_major = 60;


MODULE_LICENSE("Dual BSD/GPL");


static int memory_open(struct inode *inode, struct file *filp)
{
    return 0;
}

static int memory_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static ssize_t memory_read(
    struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
    if (*f_pos > 0)
        return 0;
    else
    {
        const char * message = "Hello there\n";
        if (count > strlen(message))
            count = strlen(message);
        copy_to_user(buf, message, count);
        *f_pos += count;
        return count;
    }
}

static ssize_t memory_write(
    struct file *filp, const char *buf, size_t count, loff_t *f_pos)
{
    return -1;
}


struct file_operations memory_fops =
{
    read:    memory_read,
    write:   memory_write,
    open:    memory_open,
    release: memory_release
};

static int memory_init(void)
{
    int status = register_chrdev(memory_major, "memory", &memory_fops);
    if (status < 0)
    {
        printk("<1> memory: unable to register device\n");
        return status;
    }
    return 0;
}

static void memory_exit(void)
{
    unregister_chrdev(memory_major, "memory");
}

module_init(memory_init);
module_exit(memory_exit);
