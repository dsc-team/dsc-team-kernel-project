#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/wait.h>
#include <linux/miscdevice.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/jiffies.h>
#include <linux/remote_spinlock.h>
#include <linux/debugfs.h>
#include <linux/io.h>
#include <linux/string.h>
#include <linux/jiffies.h>
#include <mach/msm_iomap.h>
#include <mach/pm_log.h>
#include "smd_private.h"

static unsigned long 	*pm_log_base;
static u64	pm_log_start[PM_LOG_NUM];
static u64	pm_log_end[PM_LOG_NUM];

static DEFINE_SPINLOCK(pm_lock);

static char pm_log_name[PM_LOG_NUM][20] = PM_LOG_NAME;

void pm_log_on(unsigned which_src)
{
	unsigned long	flags;

	spin_lock_irqsave(&pm_lock, flags);
	if (!pm_log_start[which_src]) pm_log_start[which_src] = get_jiffies_64() - INITIAL_JIFFIES;
	spin_unlock_irqrestore(&pm_lock, flags);
}

void pm_log_off(unsigned which_src)
{
	unsigned long	flags;
	unsigned long	diff;

	spin_lock_irqsave(&pm_lock, flags);
	if (pm_log_start[which_src]) {
		pm_log_end[which_src] = get_jiffies_64() - INITIAL_JIFFIES;
		diff = (unsigned long)(pm_log_end[which_src]-pm_log_start[which_src]);
		if (pm_log_base) {
			pm_log_base[which_src] += (unsigned long) jiffies_to_msecs(diff);
		}
		pm_log_start[which_src] = 0;
		pm_log_end[which_src] = 0;
	}
	spin_unlock_irqrestore(&pm_lock, flags);
}

void pm_log_event(int mode, unsigned which_src)
{
	if (which_src < PM_LOG_NUM) {
		switch( mode) {
		case PM_LOG_ON:
			pm_log_on(which_src); break;
		case PM_LOG_OFF:
			pm_log_off(which_src); break;
		}
	}
}
EXPORT_SYMBOL(pm_log_event);

#if defined(CONFIG_DEBUG_FS)

static int debug_read_pmlog(char *buf, int max)
{
	int	i = 0,j;
	if (!pm_log_base) {
		printk(KERN_ERR "%s: Can't find pm_log.\n", __func__);
		return 0;
	}

	for(j = 0; j < PM_LOG_NUM ; j++) {
		i += scnprintf(buf+i, max-i, "%20s: %8lu %8lu %8lu\n",
			pm_log_name[j],
			(unsigned long) pm_log_start[j],
			(unsigned long) pm_log_end[j],
			pm_log_base[j]
			);
	}
	return i;
}

#define MAX_DEBUG_BUFFER	1024

static char debug_buffer[MAX_DEBUG_BUFFER];

static ssize_t debug_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	int (*fill)(char *buf, int max) = file->private_data;
	int bsize = fill(debug_buffer, MAX_DEBUG_BUFFER);
	return simple_read_from_buffer(buf, count, ppos, debug_buffer, bsize);
}

static int debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static const struct file_operations debug_ops = {
	.read = debug_read,
	.open = debug_open,
};

#endif	/* CONFIG_DEBUG_FS */

static int __init pm_log_init(void)
{
	pm_log_base = (unsigned long*) smem_alloc(SMEM_SMEM_LOG_PM,
			sizeof(unsigned long)*PM_LOG_NUM);

	if (!pm_log_base) {
		printk(KERN_ERR "%s: Can't allocate pm log buffer\n", __func__);
		return -EIO;
	}

	memset(pm_log_start, 0, sizeof(u64) * PM_LOG_NUM);
	memset(pm_log_end, 0, sizeof(u64) * PM_LOG_NUM);
	memset(pm_log_base, 0, sizeof(unsigned long) * PM_LOG_NUM);

#if defined(CONFIG_DEBUG_FS)
	debugfs_create_file("pm_log", 0444, NULL, debug_read_pmlog, &debug_ops);
#endif

	return 0;
}

arch_initcall(pm_log_init);

