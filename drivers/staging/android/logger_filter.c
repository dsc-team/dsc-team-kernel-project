#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <mach/logfilter.h>


extern void __iomem*	log_filter_base;
extern __u32		log_filter_size;

static ssize_t logger_filter_read(struct file *file, char __user *data, size_t count, loff_t *off)
{
	int len = (count > log_filter_size) ? log_filter_size : count;
	if (copy_to_user(data,(char*)log_filter_base,len)) {
		printk(KERN_ERR "copy failed\n");
		return -EIO;
	}
	return len;
}

static ssize_t logger_filter_write(struct file *file, const char __user *data, size_t count, loff_t *off)
{
	int len = (count > log_filter_size) ? log_filter_size : count;
	if (copy_from_user((char*)log_filter_base,data,len)) {
		printk(KERN_ERR "copy failed\n");
		return -EIO;
	}
	return len;
}

static int logger_filter_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int logger_filter_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static struct file_operations logger_filter_fops = {
	.owner = THIS_MODULE,
	.read = logger_filter_read,
	.write = logger_filter_write,
	.open = logger_filter_open,
	.release = logger_filter_release,
};

static struct miscdevice logger_filter_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "log_filter",
	.fops = &logger_filter_fops,
	.parent = NULL,
};

static int __init logger_filter_init(void)
{
	struct filter_header	*header;

	int ret;
	if (!log_filter_base) {
		printk(KERN_ERR "%s: log filter doesn't exist\n",__func__);
		return -ENOSYS;
	}

	ret = misc_register(&logger_filter_device);
	if ( unlikely(ret) ) {
		printk(KERN_ERR "%s: failed to register misc device.\n",__func__);
		return ret;
	}

	header = (struct filter_header*) log_filter_base;
	printk(KERN_INFO "%s: version:%x num:%d toff:%d voff:%d\n",
		__func__, header->version, 
		header->tag_num, header->tag_offset, header->val_offset);
	printk(KERN_INFO "%s: filter size:%d\n", __func__, log_filter_size);
	
	return 0;
}

static void __exit logger_filter_exit(void)
{
	misc_deregister(&logger_filter_device);
}

module_init(logger_filter_init);
module_exit(logger_filter_exit);
