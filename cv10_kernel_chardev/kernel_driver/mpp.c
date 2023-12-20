#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/device.h>

static int mpp_open(struct inode *inode, struct file *filp);
static int mpp_release(struct inode *inode, struct file *f);
static ssize_t mpp_read(struct file *f, char *buf, size_t size, loff_t *offset);
static ssize_t mpp_write(struct file *f, const char *buf, size_t size, loff_t *offset);
static long mpp_ioctl (struct file * f, unsigned int request, unsigned long param);

struct file_operations mpp_fops = {
    .owner = THIS_MODULE,
    .llseek = NULL,
    .read = mpp_read,
    .write = mpp_write,
    /*  .aio_read = NULL,
        .aio_write = NULL, */
    // .readdir = NULL,
    .poll = NULL,
    .unlocked_ioctl = mpp_ioctl,
    .compat_ioctl = NULL,
    .mmap = NULL,
    .open = mpp_open,
    .flush = NULL,
    .release = mpp_release,
    .fsync = NULL,
    /*  .aio_fsync = NULL, */
    .fasync = NULL,
    .lock = NULL,
    .sendpage = NULL,
    .get_unmapped_area = NULL,
    .check_flags = NULL,
    .flock = NULL,
    .splice_write = NULL,
    .splice_read = NULL,
    .setlease = NULL,
    .fallocate = NULL};

short g_Capitalise   = 0;
short g_CipherToggle = 0;

static int mpp_open(struct inode *inode, struct file *filp)
{
    printk("mpp_open\n");
    return 0;
}

static int mpp_release(struct inode *inode, struct file *f)
{
    printk("mpp_release\n");
    return 0;
}

void cipher ( char * buf, size_t size )
{
    for ( int i = 0; i < size; i++ )
        buf[i] = buf[i] ^ 0x55;
}

void capitalise ( char * buf, size_t size )
{
    for ( int i = 0; i < size; i++ )
        if ( buf[i] >= 'a' && buf[i] <= 'z' )
            buf[i] = buf[i] - 'a' + 'A';
}

static ssize_t mpp_read(struct file *f, char *buf, size_t size, loff_t *offset)
{
    printk("mpp_read\n");
    char str[] = "Hello from kernel!\n";
    
    if ( g_Capitalise ) capitalise ( str, sizeof(str) );
    else if ( g_CipherToggle ) cipher ( str, sizeof(str) );

    int not_copied_size = copy_to_user ( buf, str, sizeof(str) );
    return size - not_copied_size;
}

static ssize_t mpp_write(struct file *f, const char *buf, size_t size, loff_t *offset)
{
    printk("mpp_write\n");
    char * kbuf = kmalloc (size, GFP_KERNEL);

    copy_from_user ( (void *) kbuf, buf, size );

    if ( g_Capitalise ) capitalise ( kbuf, size );
    else if ( g_CipherToggle ) cipher ( kbuf, size );

    printk("%s", kbuf);

    kfree(kbuf);
    return size;
}


#define REQ_CAPITALISE 1
#define REQ_CIPHER     2

static long mpp_ioctl ( struct file *f, unsigned int request, unsigned long param )
{
    switch (request)
    {
    case REQ_CAPITALISE:
        g_Capitalise = param == 1 ?
                            1 :
                            0;
        break;
    case REQ_CIPHER:
        g_CipherToggle = param == 1 ?
                            1 :
                            0;
        break;
    default:
        break;
    }
    return 1;
}

static int major = 0;
static struct class* mpp_class = NULL;
static struct device* mpp_device = NULL;

static int __init mpp_module_init(void)
{
    printk("MPP module is loaded\n");
    major = register_chrdev(0, "mpp", &mpp_fops);
    mpp_class = class_create(THIS_MODULE, "mpp");
    if (IS_ERR(mpp_class))
    {
        printk("Failed to register device class\n");
        return PTR_ERR(mpp_class);
    }

    mpp_device = device_create(mpp_class, NULL, MKDEV(major, 0), NULL, "mpp");
    if (IS_ERR(mpp_device))
    {
        printk("Failed to create the device\n");
        return PTR_ERR(mpp_device);
    }
    return 0;
}

static void __exit mpp_module_exit(void)
{
    printk("MPP module is unloaded\n");
    device_destroy(mpp_class, MKDEV(major, 0));
    class_destroy(mpp_class);
    unregister_chrdev(major, "mpp");
    return;
}

module_init(mpp_module_init);
module_exit(mpp_module_exit);

MODULE_LICENSE("GPL");