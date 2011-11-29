#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/switch.h>
#include <linux/input.h>
#include <linux/wakelock.h>
#include <mach/gpio.h>

#include <mach/pmic.h>
#include <mach/pm_log.h>
#include <mach/austin_hwid.h>

#define DRIVER_VERSION "v1.1"
#define RECOVER_CHK_HEADSET_TYPE_TIMES	10

#ifdef CONFIG_HW_AUSTIN
#define AUDIO_HEADSET_MIC_SHTDWN_N	17
#else
#define AUDIO_HEADSET_MIC_SHTDWN_N	34
#endif	//CONFIG_HW_AUSTIN

#ifdef DEBUG
#define HEADSET_INFO(fmt, arg...) printk(KERN_INFO "headset detection %s: " fmt "\n" , __FUNCTION__ , ## arg)
#define HEADSET_DBG(fmt, arg...)  printk(KERN_DEBUG "headset detection %s: " fmt "\n" , __FUNCTION__ , ## arg)
#define HEADSET_ERR(fmt, arg...)  printk(KERN_ERR  "headset detection %s: " fmt "\n" , __FUNCTION__ , ## arg)


int g_debounceDelay1 		= 300;
int g_debounceDelay2 		= 600;
int g_debounceDelayHookkey 	= 65;

module_param(g_debounceDelay1, int, 0644);
module_param(g_debounceDelay2, int, 0644);
module_param(g_debounceDelayHookkey, int, 0644);

#else
#define HEADSET_INFO(fmt, arg...)  printk(KERN_INFO "headset detection %s: " fmt "\n" , __FUNCTION__ , ## arg)
#define HEADSET_DBG(fmt, arg...)
#define HEADSET_ERR(fmt, arg...)
#endif

static struct workqueue_struct *g_detection_work_queue;

void cable_ins_det_work_func(struct work_struct *work);

void check_hook_key_state_work_func(struct work_struct *work);

DECLARE_DELAYED_WORK(cable_ins_det_work, cable_ins_det_work_func);

DECLARE_DELAYED_WORK(check_hook_key_state_work, check_hook_key_state_work_func);

typedef enum  {
	HSD_IRQ_DISABLE,
	HSD_IRQ_ENABLE
}irq_status;

struct headset_info{
	unsigned int	jack_gpio;
	unsigned int	hook_gpio;
	unsigned int	jack_irq;
	unsigned int    hook_irq;
	unsigned int	jack_status;
    	unsigned int 	debounceDelay1;
   	unsigned int 	debounceDelay2;
	unsigned int 	debounceDelayHookKey;
	irq_status 	hook_irq_status;
	irq_status 	jack_irq_status;
	struct switch_dev sdev;
	struct input_dev *input;
	atomic_t 		is_button_press;
	unsigned	int	recover_chk_times;

	struct wake_lock headset_det_wakelock;
} ;

typedef enum  {
	HSD_NONE = 0,
    	HSD_HEADSET = 1,
    	HSD_EAR_PHONE = 2
}headset_type;



static struct headset_info	headset_data;

headset_type g_headset_type;

MODULE_VERSION(DRIVER_VERSION);
MODULE_AUTHOR("Terry.Cheng");
MODULE_DESCRIPTION("Headset detection");
MODULE_LICENSE("GPL v2");

static void button_pressed(void)
{
	HEADSET_INFO("");
	input_report_key(headset_data.input, KEY_MEDIA, 1);
	input_sync(headset_data.input);
}

static void button_released(void)
{

	HEADSET_INFO("");
	input_report_key(headset_data.input, KEY_MEDIA, 0);
	input_sync(headset_data.input);
}

static void remove_headset(void)
{
	int rc = 0;
	unsigned long irq_flags;

	if (HSD_HEADSET == g_headset_type)
	{
		gpio_set_value(AUDIO_HEADSET_MIC_SHTDWN_N, 1);
		HEADSET_INFO("Turn off mic en");

		PM_LOG_EVENT(PM_LOG_OFF, PM_LOG_AUTIO_MIC);

		if ( 1 == atomic_read(&headset_data.is_button_press))
		{
			button_released();
			atomic_set(&headset_data.is_button_press, 0);
		}
		if ( HSD_IRQ_ENABLE == headset_data.hook_irq_status)
		{
			local_irq_save(irq_flags);
			disable_irq_nosync(headset_data.hook_irq);
			headset_data.hook_irq_status = HSD_IRQ_DISABLE;
			set_irq_wake(headset_data.hook_irq, 0);
			local_irq_restore(irq_flags);
		}
	}
	g_headset_type = HSD_NONE;

	rc = set_irq_type(headset_data.jack_irq, IRQF_TRIGGER_LOW);
	if (rc)
		HEADSET_ERR("change IRQ detection type as low fail!!");

	headset_data.recover_chk_times = RECOVER_CHK_HEADSET_TYPE_TIMES;

	HEADSET_INFO("Headset remove, jack_irq active LOW");


}

static void insert_headset(void)
{
	int hook_key_status = 0;
	int rc = 0;
	unsigned long irq_flags;
	int need_update_path = 0;

	gpio_set_value(AUDIO_HEADSET_MIC_SHTDWN_N, 0);
	HEADSET_INFO("Turn on mic en");

	PM_LOG_EVENT(PM_LOG_ON, PM_LOG_AUTIO_MIC);

	msleep(headset_data.debounceDelay2);

	if( 1 == gpio_get_value(headset_data.jack_gpio))
	{
		HEADSET_INFO("Headset removed while detection\n");
		return;
	}

	hook_key_status = gpio_get_value(headset_data.hook_gpio);
	if ( 1 == hook_key_status )
	{
		g_headset_type = HSD_EAR_PHONE;
		gpio_set_value(AUDIO_HEADSET_MIC_SHTDWN_N, 1);
		HEADSET_INFO("Turn off mic en");

		PM_LOG_EVENT(PM_LOG_OFF, PM_LOG_AUTIO_MIC);

		if (headset_data.recover_chk_times > 0)
		{
			headset_data.recover_chk_times--;
			HEADSET_INFO("Start recover timer, recover_chk_times = %d", headset_data.recover_chk_times);
			queue_delayed_work(g_detection_work_queue,&cable_ins_det_work, (HZ/10));
		}
	}
	else
	{
		msleep(headset_data.debounceDelay1);

		if( 1 == gpio_get_value(headset_data.jack_gpio))
		{
			HEADSET_INFO("Headset removed while detection\n");
			return;
		}
		if ( g_headset_type == HSD_EAR_PHONE)
		{
			need_update_path = 1;
		}
		g_headset_type = HSD_HEADSET;
		if(1 == need_update_path)
		{
			HEADSET_INFO("need_update_path = 1, switch audio path to CAD_HW_DEVICE_ID_HEADSET_MIC ");
		}
		rc = set_irq_type(headset_data.hook_irq, IRQF_TRIGGER_HIGH);
		if (rc)
			HEADSET_ERR("Set hook key interrupt detection type as rising Fail!!!");
		if(HSD_IRQ_DISABLE == headset_data.hook_irq_status)
		{
			local_irq_save(irq_flags);

			enable_irq(headset_data.hook_irq);
			headset_data.hook_irq_status = HSD_IRQ_ENABLE;
			set_irq_wake(headset_data.hook_irq, 1);
			local_irq_restore(irq_flags);
		}
	}

	HEADSET_INFO("g_headset_type= %d", g_headset_type);

	rc = set_irq_type(headset_data.jack_irq, IRQF_TRIGGER_HIGH);
	if (rc)
		HEADSET_ERR(" change IRQ detection type as high active fail!!");

}

void cable_ins_det_work_func(struct work_struct *work)
	{
	unsigned long irq_flags;

	headset_data.jack_status  = gpio_get_value(headset_data.jack_gpio);

	if( 0 == headset_data.jack_status)
	{
		HEADSET_INFO("lock headset wakelock when doing headset insert detection");

		wake_lock_timeout(&headset_data.headset_det_wakelock, (2*HZ) );
		insert_headset();
	}
	else
	{
		HEADSET_INFO("lock headset wakelock when doing headset remove detection");
		wake_lock_timeout(&headset_data.headset_det_wakelock, HZ );
		remove_headset();
	}

	if ( HSD_IRQ_DISABLE == headset_data.jack_irq_status)
	{
		local_irq_save(irq_flags);

		enable_irq(headset_data.jack_irq);
		headset_data.jack_irq_status = HSD_IRQ_ENABLE;
		local_irq_restore(irq_flags);
	}
	switch_set_state(&headset_data.sdev, g_headset_type);
}


void check_hook_key_state_work_func(struct work_struct *work)
{
	int hook_key_status = 0;
	int rc = 0;
	unsigned long irq_flags;

	HEADSET_DBG("");
	if ( HSD_HEADSET != g_headset_type )
	{
		HEADSET_INFO("Headset remove!! or may ear phone noise !!");
		return;
	}
	hook_key_status = gpio_get_value(headset_data.hook_gpio);
	if ( 1 == hook_key_status )
	{
		button_pressed();
		atomic_set(&headset_data.is_button_press, 1);
		rc = set_irq_type(headset_data.hook_irq, IRQF_TRIGGER_LOW);
		if (rc)
			HEADSET_ERR( "change hook key detection type as low fail!!");

	}
	else
	{
		if ( 1 == atomic_read(&headset_data.is_button_press))
		{
			button_released();
			atomic_set(&headset_data.is_button_press, 0);
			rc = set_irq_type(headset_data.hook_irq, IRQF_TRIGGER_HIGH);
			HEADSET_DBG("Hook Key release change hook key detection type as high");
			if (rc)
				HEADSET_ERR("change hook key detection type as high fail!!");
		}
	}
	if ( HSD_IRQ_DISABLE == headset_data.hook_irq_status)
	{
		local_irq_save(irq_flags);

		enable_irq(headset_data.hook_irq);
		headset_data.hook_irq_status = HSD_IRQ_ENABLE;
		set_irq_wake(headset_data.hook_irq, 1);
		local_irq_restore(irq_flags);

	}
}

static irqreturn_t hook_irqhandler(int irq, void *dev_id)
{
	unsigned long irq_flags;

	local_irq_save(irq_flags);
	disable_irq_nosync(headset_data.hook_irq);
	headset_data.hook_irq_status = HSD_IRQ_DISABLE;
	set_irq_wake(headset_data.hook_irq, 0);
	local_irq_restore(irq_flags);

	queue_delayed_work(g_detection_work_queue,&check_hook_key_state_work, msecs_to_jiffies(headset_data.debounceDelayHookKey));

	return IRQ_HANDLED;
}


static irqreturn_t jack_irqhandler(int irq, void *dev_id)
{
	unsigned long irq_flags;

	local_irq_save(irq_flags);
	disable_irq_nosync(headset_data.jack_irq);
	headset_data.jack_irq_status = HSD_IRQ_DISABLE;
	local_irq_restore(irq_flags);

	queue_delayed_work(g_detection_work_queue,&cable_ins_det_work, 0);
	return IRQ_HANDLED;
}

static ssize_t switch_hds_print_state(struct switch_dev *sdev, char *buf)
{

	switch (switch_get_state(&headset_data.sdev)) {
	case HSD_NONE:
		return sprintf(buf, "No Device\n");
	case HSD_HEADSET:
		return sprintf(buf, "Headset\n");
	case HSD_EAR_PHONE:
		return sprintf(buf, "Headset\n");
	}

	return -EINVAL;
}


static int __init_or_module headset_probe(struct platform_device *pdev)
{

	int ret;
	struct resource *res;
	unsigned int	irq;

	memset(&headset_data, 0, sizeof( headset_data));

	headset_data.sdev.name = pdev->name;
	headset_data.sdev.print_name = switch_hds_print_state;

    	ret = switch_dev_register(&headset_data.sdev);
	if (ret < 0)
	{
		goto err_switch_dev_register;
	}

	headset_data.debounceDelay1 = 300;
	headset_data.debounceDelay2 = 600;
	headset_data.debounceDelayHookKey = 65;
	headset_data.jack_status = 0;
	headset_data.recover_chk_times = RECOVER_CHK_HEADSET_TYPE_TIMES;

	g_headset_type = HSD_NONE;
	atomic_set(&headset_data.is_button_press, 0);

	printk(KERN_INFO "Init headset wake lock\n");
	wake_lock_init(&headset_data.headset_det_wakelock, WAKE_LOCK_SUSPEND, "headset_det");

	g_detection_work_queue = create_workqueue("headset_detection");
	if (g_detection_work_queue == NULL) {
		ret = -ENOMEM;
		goto err_create_work_queue;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
				"hook_int");
	if (!res) {
		HEADSET_ERR("couldn't find hook int\n");
		ret = -ENODEV;
		goto err_get_hook_gpio_resource;
	}
	headset_data.hook_gpio = res->start;

	ret = gpio_request(headset_data.hook_gpio, "headset_hook");
	if (ret < 0)
		goto err_request_hook_gpio;

	ret = gpio_direction_input(headset_data.hook_gpio);
	if (ret < 0)
		goto err_set_hook_gpio;

	irq = MSM_GPIO_TO_INT(headset_data.hook_gpio);
	if (irq <0)
	{
		ret = irq;
		goto err_get_hook_detect_irq_num_failed;
	}
	headset_data.hook_irq = irq;

	set_irq_flags(headset_data.hook_irq, IRQF_VALID | IRQF_NOAUTOEN);
	ret = request_irq(headset_data.hook_irq, hook_irqhandler, IRQF_TRIGGER_NONE, "headset_hook", &headset_data);
	if( ret < 0)
	{
		HEADSET_ERR("Fail to request hook irq");
		goto err_request_austin_headset_hook_irq;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
				"jack_int");
	if (!res) {
		HEADSET_ERR("couldn't find jack int\n");
		ret = -ENODEV;
		goto err_get_jack_int_resource;
	}
	headset_data.jack_gpio = res->start;

	ret = gpio_request(headset_data.jack_gpio, "headset_jack");
	if (ret < 0)
		goto err_request_jack_gpio;

	ret = gpio_direction_input(headset_data.jack_gpio);
	if (ret < 0)
		goto err_set_jack_gpio;

	irq = MSM_GPIO_TO_INT(headset_data.jack_gpio);
	if (irq <0)
	{
		ret = irq;
		goto err_get_jack_detect_irq_num_failed;
	}
	headset_data.jack_irq = irq;

	headset_data.jack_irq_status = HSD_IRQ_ENABLE;
	ret = request_irq(headset_data.jack_irq, jack_irqhandler, IRQF_TRIGGER_LOW, "headset_jack", &headset_data);
	if (ret < 0)
	{
		HEADSET_ERR("Fail to request jack irq");
		goto err_request_austin_headset_jack_irq;
	}

	ret = set_irq_wake(headset_data.jack_irq, 1);
	if (ret < 0)
		goto err_request_input_dev;


	headset_data.input = input_allocate_device();
	if (!headset_data.input) {
		ret = -ENOMEM;
		goto err_request_input_dev;
	}
	headset_data.input->name = "Austin headset";
	headset_data.input->evbit[0] = BIT_MASK(EV_KEY);
	set_bit(KEY_MEDIA, headset_data.input->keybit);
	ret = input_register_device(headset_data.input);
	if (ret < 0)
	{
		HEADSET_ERR("Fail to register input device");
		goto err_register_input_dev;
	}

	gpio_set_value(AUDIO_HEADSET_MIC_SHTDWN_N, 1);

	PM_LOG_EVENT(PM_LOG_OFF, PM_LOG_AUTIO_MIC);

	return 0;
err_register_input_dev:
	input_free_device(headset_data.input);
err_request_input_dev:
	free_irq(headset_data.jack_irq, 0);
err_request_austin_headset_jack_irq:
err_get_jack_detect_irq_num_failed:
err_set_jack_gpio:
	gpio_free(headset_data.jack_gpio);
err_request_jack_gpio:
err_get_jack_int_resource:
	free_irq(headset_data.hook_irq, 0);
err_request_austin_headset_hook_irq:
err_get_hook_detect_irq_num_failed:
err_set_hook_gpio:
	 gpio_free(headset_data.hook_gpio);
err_request_hook_gpio:
err_get_hook_gpio_resource:
	destroy_workqueue(g_detection_work_queue);
err_create_work_queue:
	switch_dev_unregister(&headset_data.sdev);
err_switch_dev_register:
	HEADSET_ERR(" Failed to register driver");

	return ret;

}

static int headset_remove(struct platform_device *pdev)
{


	input_unregister_device(headset_data.input);
	free_irq(headset_data.hook_irq, 0);
	free_irq(headset_data.jack_irq, 0);
	destroy_workqueue(g_detection_work_queue);
	switch_dev_unregister(&headset_data.sdev);
	return 0;

}

static struct platform_driver headset_driver = {
	.probe = headset_probe,
	.remove = headset_remove,
	.driver = {
		.name = "headset",
		.owner = THIS_MODULE,
	},
};


static int __init headset_init(void)
{
	int ret;

	printk(KERN_INFO "BootLog, +%s\n", __func__);
	ret = platform_driver_register(&headset_driver);


	printk(KERN_INFO "BootLog, -%s, ret=%d\n", __func__, ret);
	return ret;
}


static void __exit headset_exit(void)
{

	HEADSET_DBG("Poweroff, +");
	platform_driver_unregister(&headset_driver);

}

module_init(headset_init);
module_exit(headset_exit);

