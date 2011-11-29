#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/ioctl.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <mach/kevent.h>

static DEFINE_SPINLOCK(kevent_lock);
static unsigned	events_map = 0;
static bool initialized = 0;
static unsigned long event_counts[KEVENT_COUNT] = {0};

static char *event_oops[] = {
	"EVENT=Oops",0
};
static char *event_modem_crash[] = {
	"EVENT=ModemCrash",0
};

static char *event_adsp_crash[] = {
	"EVENT=ADSPCrash",0
};

static char *event_dump_wakelock[] = {
	"EVENT=WakelockDump",0
};

static char **event_string[] = {
	event_oops,
	event_modem_crash, 
	event_adsp_crash,
	event_dump_wakelock,
};

static DECLARE_MUTEX(kevent_sem);

struct kevent_device{
        struct miscdevice       miscdevice;
        struct workqueue_struct *workqueue;
        struct work_struct      work;
	struct proc_dir_entry	*proc;
};

static int kevent_proc_read(char *page, char **start, off_t off,
			int count, int *eof, void *data)
{
	int len=0;
	int i;
	for(i=0;i<KEVENT_COUNT;i++) {
		len += sprintf(page+len, "%lu ", event_counts[i]);
	}
	*(page+len-1)='\n';

	if (len <= off+count) *eof = 1;
	*start = page + off;
	len -= off;
	if (len>count) len = count;
	if (len<0) len = 0;
	return len;
}

void kevent_work(struct work_struct *work)
{
	struct kevent_device	*device =
		container_of(work, struct kevent_device, work);
	int	i;
	unsigned long 	flags;
	unsigned	events;

repeat:
	spin_lock_irqsave(&kevent_lock,flags);
	events = events_map;
	events_map = 0;
	spin_unlock_irqrestore(&kevent_lock,flags);

	if (events) {
		for(i=0;events;i++) {
			if (events & 1) {
				printk(KERN_DEBUG "%s: send kobject uevent, %s\n",__func__,event_string[i][0]);
				kobject_uevent_env(
					&device->miscdevice.this_device->kobj,
					KOBJ_CHANGE,
					event_string[i]
				);
			}
			events >>= 1;
		}
		goto repeat;
	}

}

static int kevent_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int kevent_release(struct inode *inode , struct file *file)
{
	return 0;
}

static struct file_operations kevent_fops = {
	.owner = THIS_MODULE,
	.open = kevent_open,
	.release = kevent_release,
};

static struct kevent_device	kevent_device = {
	.miscdevice = {
		.minor = MISC_DYNAMIC_MINOR,
		.name = "kevent",
		.fops = &kevent_fops,
		.parent = NULL,
	},
};

int kevent_trigger(unsigned event_id)
{
	unsigned long	flags;

        if (event_id > KEVENT_COUNT) return -1;

	spin_lock_irqsave(&kevent_lock,flags);
	if (!initialized) {
		spin_unlock_irqrestore(&kevent_lock,flags);
		return -1;
	}
	events_map |= (1<<event_id);
	event_counts[event_id]++;

	printk(KERN_DEBUG "%s: queue event work\n", __func__);
	queue_work(kevent_device.workqueue, &kevent_device.work);

	spin_unlock_irqrestore(&kevent_lock,flags);

	return 0;
}
EXPORT_SYMBOL(kevent_trigger);

static int __init kevent_init(void)
{
	int		ret;
	unsigned long	flags;

	ret = misc_register(&kevent_device.miscdevice);
	if( unlikely(ret)) {
		printk(KERN_ERR "%s: failed to register misc \n",__func__);
		goto failed_register;
	}

	kevent_device.proc = create_proc_entry("kevent_counts", S_IRUSR|S_IRGRP,NULL);
	if (!kevent_device.proc) {
		printk(KERN_ERR "%s: failed to create proc entry\n",__func__);
		ret = -ENOMEM;
		goto failed_proc;
	}
	kevent_device.proc->read_proc = kevent_proc_read;

	kevent_device.workqueue = create_singlethread_workqueue("keventd");
	if (!kevent_device.workqueue) {
		printk(KERN_ERR "%s: failed to create workqueue\n",__func__);
		ret = -ENOMEM;
		goto failed_workqueue;
	}

	INIT_WORK(&kevent_device.work,kevent_work);

	spin_lock_irqsave(&kevent_lock,flags);
	initialized = 1;
	spin_unlock_irqrestore(&kevent_lock,flags);

	return 0;

failed_workqueue:
	remove_proc_entry("kevent_counts",NULL);
failed_proc:
	misc_deregister(&kevent_device.miscdevice);
failed_register:

	return ret;
}

static void __exit kevent_exit(void)
{
	unsigned long	flags;
	spin_lock_irqsave(&kevent_lock,flags);
	initialized = 1;
	spin_unlock_irqrestore(&kevent_lock,flags);

	misc_deregister(&kevent_device.miscdevice);

	flush_workqueue(kevent_device.workqueue);

	destroy_workqueue(kevent_device.workqueue);
}

module_init(kevent_init);
module_exit(kevent_exit);
