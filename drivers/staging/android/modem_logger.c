#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/ioctl.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <linux/poll.h>

#include <mach/msm_iomap.h>

#define	MTRACE_IS_PAGE_ALIGNED(addr)	(!((addr) & (~PAGE_MASK)))

static void __iomem	*modem_logger_buffer = 0;

static int modem_logger_mmap(struct file *filp, struct vm_area_struct *vma)
{
	unsigned long	addr;
	unsigned long	length = vma->vm_end - vma->vm_start;

	printk(KERN_INFO "%s\n", __func__);

	if (!modem_logger_buffer) {
		printk(KERN_ERR "%s: No modem image available.\n", __func__);
		return -ENOMEM;
	}

	if (!MTRACE_IS_PAGE_ALIGNED(length)) {
		printk(KERN_ERR "%s: Invalid length.\n", __func__);
		return -EINVAL;
	}
	
	if (vma->vm_flags & VM_WRITE) {
		printk(KERN_ERR "%s: Permission denied.\n",__func__);
		return -EPERM;
	}

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	addr = MSM_MTRACE_BUF_PHYS;
	addr &= ~(PAGE_SIZE - 1);
        addr &= 0xffffffffUL;

	if (io_remap_pfn_range(vma, vma->vm_start, addr >> PAGE_SHIFT,
				length, vma->vm_page_prot)) {
		printk(KERN_ERR "remap_pfn_range failed in %s\n",__FILE__);
		return -EAGAIN;
	}

	return 0;
}

static int modem_logger_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int modem_logger_release(struct inode *inode , struct file *file)
{
	return 0;
}

static struct file_operations modem_logger_fops = {
	.owner = THIS_MODULE,
	.mmap = modem_logger_mmap,
	.open = modem_logger_open,
	.release = modem_logger_release,
};

static struct miscdevice modem_logger_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "log_modem",
	.fops = &modem_logger_fops,
	.parent = NULL,
};

static int __init modem_logger_init(void)
{
	int ret;

	modem_logger_buffer = ioremap(MSM_MTRACE_BUF_PHYS,MSM_MTRACE_BUF_SIZE);
	if ( unlikely(!modem_logger_buffer)) {
		printk(KERN_ERR "%s: failed to map modem image \n",__func__);
		return -ENOMEM;
	}

	ret = misc_register(&modem_logger_device);
	if( unlikely(ret)) {
		printk(KERN_ERR "%s: failed to register misc \n",__func__);
		return ret;
	}

	return 0;
}

static void __exit modem_logger_exit(void)
{
	if (modem_logger_buffer) iounmap(modem_logger_buffer);	
}

module_init(modem_logger_init);
module_exit(modem_logger_exit);
