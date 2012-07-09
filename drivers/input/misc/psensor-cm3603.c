/*
 *  drivers/input/misc/psensor-cm3603.c
 *
 *  Copyright (c) 2008 QUALCOMM USA, INC.
 *
 *  All source code in this file is licensed under the following license
 *  except where indicated.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  version 2 as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, you can find it at http://www.fsf.org
 *
 *  Driver for QWERTY keyboard with I/O communications via
 *  the I2C Interface. The keyboard hardware is a reference design supporting
 *  the standard XT/PS2 scan codes (sets 1&2).
 */
#include <linux/module.h>
#include <linux/input.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <mach/gpio.h>
#include <mach/vreg.h>
#include <mach/psensor.h>
#include <mach/pm_log.h>
#include <mach/austin_hwid.h>
#include <linux/irq.h>
#include <linux/input.h>
#include <linux/platform_device.h>

#define PSENSOR_DEBUG 1
#define PSENSOR_TEMP_SOLUTION 1
#define PSENSOR_RESUME_SOLUTION 1

#if PSENSOR_DEBUG
#define DBGPRINTK printk
#else
#define DBGPRINTK(a...)
#endif

#if PSENSOR_TEMP_SOLUTION
#include <linux/wakelock.h>
struct wake_lock psensor_wlock;
#endif

#define DRIVER_VERSION "v1.0"

#define PSENSOR_NAME "psensor_cm3603"

MODULE_VERSION(DRIVER_VERSION);
MODULE_AUTHOR("Qisda Incorporated");
MODULE_DESCRIPTION("I2C p-sensor driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform: psensor");

#define PSENSOR_INT_GPIO 159
#define PSENSOR_ENABLE_GPIO 19
static struct work_struct psensor_irqwork;
static int psensor_param=0;
static bool dynamic_log_ebable = 0;
static int psensor_power_counter = 0;
static int last_ps_out_status = -1;

atomic_t psensor_approached = ATOMIC_INIT(0);
//Get Voice start status from audio_ctrl.c
extern atomic_t voice_started;

struct psensor_t {
        struct input_dev *input_dev;
};
static struct psensor_t psensor_data;

static int misc_psensor_open(struct inode *inode_p, struct file *fp);
static int misc_psensor_release(struct inode *inode_p, struct file *fp);
static int misc_psensor_ioctl(struct inode *inode_p, struct file *fp, unsigned int cmd, unsigned long arg);
static int psensor_irqsetup(int psensor_intpin);
static int psensor_config_gpio(void);

static struct file_operations misc_psensor_fops = {
	.owner 	= THIS_MODULE,
	.open 	= misc_psensor_open,
	.release = misc_psensor_release,
	.ioctl = misc_psensor_ioctl,
	.read = NULL,
	.write = NULL,
};

static struct miscdevice misc_psensor_device = {
	.minor 	= MISC_DYNAMIC_MINOR,
	.name 	= PSENSOR_NAME,
	.fops 	= &misc_psensor_fops,
};

static struct vreg *vreg_psensor =  NULL;

static int misc_psensor_open(struct inode *inode_p, struct file *fp)
{

	DBGPRINTK(KERN_DEBUG "misc_psensor_open+\n");
	DBGPRINTK(KERN_DEBUG "misc_psensor_open-\n");
	return 0;
}


static int misc_psensor_release(struct inode *inode_p, struct file *fp)
{

	DBGPRINTK(KERN_DEBUG "misc_psensor_release+\n");
	DBGPRINTK(KERN_DEBUG "misc_psensor_release-\n");
	return 0;
}

static int misc_psensor_ioctl(struct inode *inode_p, struct file *fp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	int ret_vreg = 0;
	unsigned int distance;
	struct psensor_t *data = &psensor_data;

	DBGPRINTK(KERN_ERR "misc_psensor_ioctl+\n");

	if (_IOC_TYPE(cmd) != PSENSOR_IOC_MAGIC)
	{
		DBGPRINTK(KERN_ERR "misc_psensor_ioctl::Not PSENSOR_IOC_MAGIC\n");
		return -ENOTTY;
	}

	if (_IOC_DIR(cmd) & _IOC_READ)
	{
		ret = !access_ok(VERIFY_WRITE, (void __user*)arg, _IOC_SIZE(cmd));
		if (ret)
		{
			DBGPRINTK(KERN_ERR "misc_psensor_ioctl::access_ok check err\n");
			return -EFAULT;
		}
	}

	if (_IOC_DIR(cmd) & _IOC_WRITE)
	{
		ret = !access_ok(VERIFY_READ, (void __user*)arg, _IOC_SIZE(cmd));
		if (ret)
		{
			DBGPRINTK(KERN_ERR "misc_psensor_ioctl::access_ok check err\n");
			return -EFAULT;
		}
	}

	switch (cmd)
	{
		case PSENSOR_IOC_ENABLE:
			psensor_power_counter++;
			DBGPRINTK(KERN_ERR "### [UP] psensor_power_counter=%d @ PSENSOR_IOC_ENABLE\n", psensor_power_counter);
	
			if(psensor_power_counter == 1)
			{
				vreg_psensor = vreg_get((void *)&misc_psensor_device, "mmc");
				if(IS_ERR(vreg_psensor))
				{
					printk(KERN_ERR "%s: vreg_psensor get Fail\n", __func__);
					vreg_psensor = NULL;
				}
				else
				{
					ret_vreg = vreg_set_level(vreg_psensor, 2600);
					if(ret_vreg)
					 printk(KERN_ERR "%s: vreg_psensor set 2.6V Fail, ret_vreg=%d\n", __func__,ret_vreg);
				}
	
				if(vreg_psensor)
				{
					vreg_enable(vreg_psensor);
					PM_LOG_EVENT(PM_LOG_ON, PM_LOG_SENSOR_PROXIMITY);
				}
				else
				{
					psensor_power_counter=0;
					printk(KERN_ERR "%s: Error!! psensor_power_counter=%d\n", __func__, psensor_power_counter);
				}
	
				gpio_set_value(PSENSOR_ENABLE_GPIO,0);
		
				last_ps_out_status = -1;
				enable_irq(MSM_GPIO_TO_INT(PSENSOR_INT_GPIO));
				enable_irq_wake(MSM_GPIO_TO_INT(PSENSOR_INT_GPIO));
				
				distance = gpio_get_value(PSENSOR_INT_GPIO);				
				input_report_abs(data->input_dev, ABS_DISTANCE, distance);
				input_sync(data->input_dev);
				if(distance)
				 set_irq_type(MSM_GPIO_TO_INT(PSENSOR_INT_GPIO), IRQF_TRIGGER_FALLING);
				else
				 set_irq_type(MSM_GPIO_TO_INT(PSENSOR_INT_GPIO), IRQF_TRIGGER_RISING);
				
			}
			break;

		case PSENSOR_IOC_DISABLE:
			psensor_power_counter--;
			DBGPRINTK(KERN_ERR "### [Down] psensor_power_counter=%d @ PSENSOR_IOC_DISABLE \n", psensor_power_counter);

			if(psensor_power_counter == 0)
			{
				disable_irq_wake(MSM_GPIO_TO_INT(PSENSOR_INT_GPIO));
				disable_irq_nosync(MSM_GPIO_TO_INT(PSENSOR_INT_GPIO));

				gpio_set_value(PSENSOR_ENABLE_GPIO,1);
		
				vreg_psensor = vreg_get((void *)&misc_psensor_device, "mmc");
				if(IS_ERR(vreg_psensor))
				{
					printk(KERN_ERR "%s: vreg_psensor get Fail\n", __func__);
					vreg_psensor = NULL;
				}
				if(vreg_psensor)
				{
					vreg_disable(vreg_psensor);
					psensor_power_counter=0;
					PM_LOG_EVENT(PM_LOG_OFF, PM_LOG_SENSOR_PROXIMITY);
				}
		
				atomic_set(&psensor_approached, 0);

				last_ps_out_status = -1;
			}
			else if(psensor_power_counter < 0)
			{
				printk(KERN_ERR "%s: Error!! psensor_power_counter=%d\n", __func__, psensor_power_counter);
				psensor_power_counter=0;
			}
			break;

		case PSENSOR_IOC_GET_STATUS:
			if ( (system_rev==EVT2_Band125) || (system_rev==EVT2_Band18) )
			{
				distance = 1;
				DBGPRINTK(KERN_INFO  "PSENSOR_IOC_GET_STATUS, distance=1 at EVT2\n");
			}
			else
			{
				distance = gpio_get_value(PSENSOR_INT_GPIO);
				DBGPRINTK(KERN_INFO  "PSENSOR_IOC_GET_STATUS, distance=%d\n", distance);
			}

			if (copy_to_user((void __user*) arg, &distance, _IOC_SIZE(cmd)))
			{
				DBGPRINTK(KERN_ERR "misc_psensor_ioctl::PSENSOR_IOC_GET_STATUS:copy_to_user fail-\n");
				ret = -EFAULT;
			}
			break;

		default:
			DBGPRINTK(KERN_ERR  "P-sensor: unknown ioctl received! cmd=%d\n", cmd);
			break;
	}

	DBGPRINTK(KERN_ERR "misc_psensor_ioctl-\n");

	return ret;
}

static irqreturn_t psensor_irqhandler(int irq, void *dev_id)
{
	schedule_work(&psensor_irqwork);
	return IRQ_HANDLED;
}

static int psensor_irqsetup(int psensor_intpin)
{
	int rc;

	set_irq_flags(MSM_GPIO_TO_INT(psensor_intpin), IRQF_VALID | IRQF_NOAUTOEN);
	rc = request_irq(MSM_GPIO_TO_INT(psensor_intpin), &psensor_irqhandler,
			     IRQF_TRIGGER_FALLING, PSENSOR_NAME, NULL);
	if (rc < 0) {
		printk(KERN_ERR
		       "Could not register for  %s interrupt "
		       "(rc = %d)\n", PSENSOR_NAME, rc);
		rc = -EIO;
	}
	return rc;
}


static int psensor_release_gpio(int psensor_intpin)
{
	gpio_free(psensor_intpin);
	return 0;
}


static void psensor_work_func(struct work_struct *work)
{
	int ps_out_status;
	struct psensor_t *data = &psensor_data;

#if PSENSOR_TEMP_SOLUTION
	wake_lock(&psensor_wlock);
#endif
	ps_out_status = gpio_get_value(PSENSOR_INT_GPIO);

	if(dynamic_log_ebable)
		printk(KERN_ERR "PS_INT!! ps_out_status=%d\n", ps_out_status);

	if(ps_out_status != last_ps_out_status)
	{
		if(ps_out_status == 0)
		{
			if ( (system_rev==EVT2_Band125) || (system_rev==EVT2_Band18) )
			{
				set_irq_type(MSM_GPIO_TO_INT(PSENSOR_INT_GPIO), IRQF_TRIGGER_FALLING);
			        last_ps_out_status = ps_out_status;
				DBGPRINTK(KERN_ERR "PS_OUT low...closed, but force report leaved for EVT2-1 !!\n");
				ps_out_status = 1;
			}
			else
			{
				if (atomic_read(&voice_started))  atomic_set(&psensor_approached, 1);
				set_irq_type(MSM_GPIO_TO_INT(PSENSOR_INT_GPIO), IRQF_TRIGGER_RISING);
			        last_ps_out_status = ps_out_status;
				DBGPRINTK(KERN_ERR "PS_OUT low...closed !!\n");
		        }
		}
		else
		{
			atomic_set(&psensor_approached, 0);
			set_irq_type(MSM_GPIO_TO_INT(PSENSOR_INT_GPIO), IRQF_TRIGGER_FALLING);
			last_ps_out_status = ps_out_status;
			DBGPRINTK(KERN_ERR "PS_OUT high...leaved !!\n");
		}
		input_report_abs(data->input_dev, ABS_DISTANCE, ps_out_status);
       		input_sync(data->input_dev);
	}
	else
	{
		printk(KERN_ERR "### Psensor INT bounce.\n");
	}

#if PSENSOR_TEMP_SOLUTION
	wake_lock_timeout(&psensor_wlock, HZ*1);
#endif
}

static int psensor_config_gpio(void)
{
	DBGPRINTK(KERN_INFO "set PS_INT GPIO %d as IN_NoPull\n", PSENSOR_INT_GPIO);
	gpio_tlmm_config(GPIO_CFG(PSENSOR_INT_GPIO, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);

	DBGPRINTK(KERN_INFO "set PS_ENA GPIO %d as GPIO_PULL_UP\n", PSENSOR_ENABLE_GPIO);
	gpio_tlmm_config(GPIO_CFG(PSENSOR_ENABLE_GPIO, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_set_value(PSENSOR_ENABLE_GPIO,1);

	return 0;
}


static int psensor_param_set(const char *val, struct kernel_param *kp)
{
	int ret;

	printk(KERN_ERR	"%s +\n", __func__);

	ret = param_set_int(val, kp);

	if(psensor_param==0)
	{
		printk(KERN_ERR "do nothing...\n");
	}
	else  if(psensor_param==1)
	{
		printk(KERN_ERR  "psensor_param_set(), ps_sd_status=%d, ps_out_status=%d\n", gpio_get_value(PSENSOR_ENABLE_GPIO), gpio_get_value(PSENSOR_INT_GPIO));
	}
	else if(psensor_param==2)
	{
		printk(KERN_ERR "%s: psensor_power_counter=%d\n", __func__, psensor_power_counter);
	}
	else if(psensor_param==90)
	{
		printk(KERN_ERR "### turn off Dynamic Debug log ###\n");
		dynamic_log_ebable = 0;
	}
	else if(psensor_param==91)
	{
		printk(KERN_ERR "### turn on Dynamic Debug log ###\n");
		dynamic_log_ebable = 1;
	}
	else
	{
		printk(KERN_ERR "else........do nothing...\n");
	}
	
	printk(KERN_ERR	"%s -\n", __func__);
	return ret;
}

module_param_call(psensor, psensor_param_set, param_get_long,
		  &psensor_param, S_IWUSR | S_IRUGO);

#if PSENSOR_RESUME_SOLUTION
#ifdef CONFIG_PM
static struct platform_device *psensor_pm_device;
#define PSENSOR_PM_NAME  "psensor_pm"

static int psensor_pm_suspend(struct platform_device *pdev, pm_message_t state)
{
    return 0;
}

static int psensor_pm_resume(struct platform_device *pdev)
{
	struct psensor_t *data = &psensor_data;
	unsigned int distance;

	if (psensor_power_counter>0)
	{
		distance = gpio_get_value(PSENSOR_INT_GPIO);
		if ( (system_rev==EVT2_Band125) || (system_rev==EVT2_Band18) )
			distance=1;
		input_report_abs(data->input_dev, ABS_DISTANCE, distance);
		input_sync(data->input_dev);
		if(distance)
			set_irq_type(MSM_GPIO_TO_INT(PSENSOR_INT_GPIO), IRQF_TRIGGER_FALLING);
		else
			set_irq_type(MSM_GPIO_TO_INT(PSENSOR_INT_GPIO), IRQF_TRIGGER_RISING);
		last_ps_out_status = distance; //update last ps_out status.
	}
	return 0;
}

static int psensor_pm_probe(struct platform_device *pd)
{
	return 0;
}

static int psensor_pm_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver psensor_pm_driver = {
  .probe    = psensor_pm_probe,
  .remove   = psensor_pm_remove,
  .suspend  = psensor_pm_suspend,
  .resume   = psensor_pm_resume,
  .driver   = {
    .name = PSENSOR_PM_NAME,
    .owner  = THIS_MODULE,
  },
};

#endif
#endif

static int __init psensor_init(void)
{
	int ret;
	struct psensor_t *data = &psensor_data;

	printk("BootLog, +%s+\n", __func__);

	psensor_power_counter = 0;

	ret = misc_register(&misc_psensor_device);
	if (ret)
	{
		printk("BootLog, -%s-, misc_register error, ret=%d\n", __func__, ret);
		return ret;
	}

	psensor_config_gpio();

	INIT_WORK(&psensor_irqwork, psensor_work_func);
	psensor_irqsetup(PSENSOR_INT_GPIO);

	gpio_set_value(PSENSOR_ENABLE_GPIO,1);
	DBGPRINTK(KERN_INFO "psensor_init(), pull high PS_SD to disable P-sensor\n");

	data->input_dev = input_allocate_device();
        if (!data->input_dev) {
                ret = -ENOMEM;
                printk ("%s:: Failed to allocate input device\n", __func__);
                goto exit_input_dev_alloc_failed;
        }

        set_bit(EV_ABS, data->input_dev->evbit);
        input_set_abs_params(data->input_dev, ABS_DISTANCE, 0, 1, 0, 0);
        data->input_dev->name = PSENSOR_NAME;

        ret = input_register_device(data->input_dev);
        if (ret) {
                printk("%s: Unable to register input device: %s\n", __func__, data->input_dev->name);
                goto exit_input_register_device_failed;
        }
	
#if PSENSOR_TEMP_SOLUTION
    wake_lock_init(&psensor_wlock, WAKE_LOCK_SUSPEND, "psensor_active");
#endif

#if PSENSOR_RESUME_SOLUTION
#ifdef CONFIG_PM
    ret=platform_driver_register(&psensor_pm_driver);
    if (ret)
    	printk(KERN_ERR "platform_driver_register fail\n");
    else
    	printk(KERN_ERR "platform_driver_register pass\n");

	psensor_pm_device = platform_device_alloc("psensor_pm", -1);
	if (!psensor_pm_device) {
		ret = -ENOMEM;
		goto exit_input_register_device_failed;
	}
	else
		printk(KERN_ERR "platform_driver_register pass2\n");

	ret= platform_device_add(psensor_pm_device);
	if (ret)
		goto exit_input_register_device_failed;
	else
		printk(KERN_ERR "platform_driver_register pass3\n");
    	
#endif
#endif

	printk("BootLog, -%s-, ret=%d\n", __func__, ret);
	return ret;

exit_input_register_device_failed:
        input_free_device(data->input_dev);
exit_input_dev_alloc_failed:
        misc_deregister(&misc_psensor_device);
        printk("BootLog, -%s-, ret=%d\n", __func__, ret);
        return ret;

}

static void __exit psensor_exit(void)
{
	struct psensor_t *data = &psensor_data;
	gpio_set_value(PSENSOR_ENABLE_GPIO,1);
	psensor_release_gpio(PSENSOR_ENABLE_GPIO);

	input_unregister_device(data->input_dev);
        misc_deregister(&misc_psensor_device);

#if PSENSOR_TEMP_SOLUTION
    wake_lock_destroy(&psensor_wlock);
#endif
	DBGPRINTK(KERN_DEBUG "psensor_exit() \n");
}

module_init(psensor_init);
module_exit(psensor_exit);
