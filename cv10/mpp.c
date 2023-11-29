    #include <linux/init.h>
    #include <linux/module.h>
    #include <linux/fs.h>
    #include <linux/slab.h>
    #include <linux/uaccess.h>

    static int __init mpp_module_init(void)
    {
            printk ("MPP module is loaded\n");
            return 0;
    }

    static void __exit mpp_module_exit(void)
    {
     	printk ("MPP module is unloaded\n");
    	return;
    }

    module_init(mpp_module_init);
    module_exit(mpp_module_exit);

    MODULE_LICENSE("GPL");