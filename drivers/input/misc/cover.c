#include <linux/init.h>
#include <linux/module.h>
#include <mach/gpio.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <mach/austin_hwid.h>
#include <linux/reboot.h>
#include <linux/syscalls.h>
#include <linux/miscdevice.h>
#include <mach/cover.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include "../../../arch/arm/mach-msm/proc_comm.h"    		/* Jagan+ ... Jagan- */

#define COVER_DEBUG 1

#if COVER_DEBUG
#define DBGPRINTK printk
#else
#define DBGPRINTK(a...)
#endif

MODULE_LICENSE("Dual BSD/GPL");

#define COVER_DETECT_NAME  "battery cover detection"
#define DELAY_TIME_MSEC 300

int cover_det_int_gpio;
static struct work_struct cover_irqwork;

static void cover_work_func(struct work_struct *work);
static irqreturn_t cover_irq_handler(int irq, void *dev_id);
int cover_irq_setup(int cover_int_pin);
static int misc_cover_ioctl(struct inode *inode_p, struct file *fp, unsigned int cmd, unsigned long arg);
static int reset_chip(void);
void final_sync_files(void);

static struct file_operations cover_fops = {
	.ioctl = misc_cover_ioctl,
};

 
static struct miscdevice cover_dev = {
	.minor 	= MISC_DYNAMIC_MINOR,
	.name 	= "cover",
	.fops 	= &cover_fops,    
};

static void cover_work_func(struct work_struct *work)
{
	
//n0p
#if 0
	int cover_status;

    msleep(DELAY_TIME_MSEC);        
    
	cover_status = gpio_get_value(cover_det_int_gpio);

	if(cover_status == 0)
	{
	    enable_irq(MSM_GPIO_TO_INT(cover_det_int_gpio));
	}
	else
	{
	    #if defined(CONFIG_HW_AUSTIN) 
        final_sync_files();
        #endif
	    kernel_power_off("Cover Removed");
	}
#endif
}

static irqreturn_t cover_irq_handler(int irq, void *dev_id)
{
    disable_irq_nosync(MSM_GPIO_TO_INT(cover_det_int_gpio));
	schedule_work(&cover_irqwork);
	return IRQ_HANDLED;
}

int cover_irq_setup(int cover_int_pin)
{
    int rc;
    rc = request_irq(MSM_GPIO_TO_INT(cover_int_pin), cover_irq_handler, 
                  IRQF_TRIGGER_HIGH, COVER_DETECT_NAME, NULL);
    if (rc < 0)
    {
        printk("failed to request_irq, rc=%d\n", rc);    
    }                
    else
    {
        set_irq_wake(MSM_GPIO_TO_INT(cover_int_pin), 1);    
    }             
    return rc;
}

static int cover_init(void)
{
    int ret=0;
    
    printk("BootLog, +%s\n", __func__);
    
	ret = misc_register(&cover_dev);   
	if (ret)
	{
		printk("BootLog, -%s-, misc_register error, ret=%d\n", __func__, ret);
		return ret;
	}	    

#if defined(CONFIG_HW_AUSTIN)   
	if (system_rev < EVT2P2_Band125) {
	    cover_det_int_gpio = 107;
    }
    else {
        cover_det_int_gpio = 38;
    }
#elif defined(CONFIG_HW_TOUCAN)
    cover_det_int_gpio = 38;
    return 0;
#endif    

    gpio_tlmm_config(GPIO_CFG(cover_det_int_gpio, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);

	INIT_WORK(&cover_irqwork, cover_work_func);   

	cover_irq_setup(cover_det_int_gpio);     

    printk("BootLog, -%s\n", __func__);    
    return 0;
}

static void cover_exit(void)
{
    printk(KERN_INFO "Goodbye, Detect cover\n");
}

static int misc_cover_ioctl(struct inode *inode_p, struct file *fp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	DBGPRINTK(KERN_ERR "misc_cover_ioctl+\n");

	if (_IOC_TYPE(cmd) != COVER_IOC_MAGIC)
	{
		DBGPRINTK(KERN_ERR "misc_cover_ioctl::Not COVER_IOC_MAGIC\n");
		return -ENOTTY;
	}

	if (_IOC_DIR(cmd) & _IOC_READ)
	{
		ret = !access_ok(VERIFY_WRITE, (void __user*)arg, _IOC_SIZE(cmd));
		if (ret)
		{
			DBGPRINTK(KERN_ERR "misc_cover_ioctl::access_ok check err\n");
			return -EFAULT;
		}
	}

	if (_IOC_DIR(cmd) & _IOC_WRITE)
	{
		ret = !access_ok(VERIFY_READ, (void __user*)arg, _IOC_SIZE(cmd));
		if (ret)
		{
			DBGPRINTK(KERN_ERR "misc_cover_ioctl::access_ok check err\n");
			return -EFAULT;
		}
	}

	switch (cmd)
	{
		case COVER_IOC_DISABLE_DET:
			disable_irq(MSM_GPIO_TO_INT(cover_det_int_gpio));		
			break;		
		case COVER_IOC_ENABLE_DET:
			enable_irq(MSM_GPIO_TO_INT(cover_det_int_gpio));	
			break;	
		case COVER_IOC_TEST_RUN_CPO_SYNC_AND_RESETCHIP:
            #if defined(CONFIG_HW_AUSTIN) 		
			final_sync_files();	
			#endif
			reset_chip();
			break;							
		default:
			DBGPRINTK(KERN_ERR  "back cover: unknown ioctl received! cmd=%d\n", cmd);
			break;		
	}

	DBGPRINTK(KERN_ERR "misc_cover_ioctl-\n");
	return ret;
}

void final_sync_files(void)
{
#ifdef CONFIG_SOFTBANK
	mm_segment_t oldfs;
	struct file *filp_launcher_db = NULL;

	oldfs = get_fs();
	set_fs(get_ds());
    filp_launcher_db = filp_open("/data/data/jp.softbank.mb.mail/databases/mmssms.db", O_RDWR, 0);	    
	if ( IS_ERR(filp_launcher_db) ) {
		set_fs(oldfs);
		printk(KERN_CRIT "%s: Open file failed\n", __func__);
	}
	else {
		vfs_fsync(filp_launcher_db, filp_launcher_db->f_dentry, 0);
		filp_close(filp_launcher_db, NULL);
		set_fs(oldfs);
		printk(KERN_CRIT "%s: Sync Finish\n", __func__);
	}				
#endif	

    not_sync_SD_partitions();
    sys_sync();
}


static int reset_chip(void)
{
    uint32_t restart_reason;
    
    msm_proc_comm(PCOM_RESET_CHIP, &restart_reason, 0);
    for (;;)
    ;
    return 0;
}


module_init(cover_init);
module_exit(cover_exit);
