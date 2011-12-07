
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/jiffies.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/i2c.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/earlysuspend.h>
#include <linux/time.h>
#include <mach/gpio.h>
#include <linux/mutex.h>
#include <mach/vreg.h>
#include <mach/mutekey.h>
#include <mach/pm_log.h>
#include <mach/austin_hwid.h>
#include <linux/wakelock.h>
#include <linux/slab.h>

static int DEBUG_LEVEL=0;
module_param_named( 
	DEBUG_LEVEL, DEBUG_LEVEL,
 	int, S_IRUGO | S_IWUSR | S_IWGRP
)
#define INFO_LEVEL  1
#define ERR_LEVEL   2
#define TEST_INFO_LEVEL 1
#define MY_INFO_PRINTK(level, fmt, args...) if(level <= DEBUG_LEVEL) printk( fmt, ##args);
#define PRINT_IN MY_INFO_PRINTK(4,"+++++%s++++ %d\n",__func__,__LINE__);
#define PRINT_OUT MY_INFO_PRINTK(4,"----%s---- %d\n",__func__,__LINE__);

struct mutekey_t {
	struct input_dev         *keyarray_input;
    int                      open_count;
	int						 gpio_num;
	int                      irq;
	int				         mutekey_suspended;
	int                      misc_open_count;
	struct delayed_work      key_work;
    struct workqueue_struct  *key_wqueue;
	wait_queue_head_t        wait;
	int                      state;
	struct early_suspend     key_early_suspend;
	struct  mutex	mutex;
	struct wake_lock mutekey_wakelock;
	int    check_state_flag;
};


static struct mutekey_t         *g_mk;
static int __ref mutekey_probe(struct platform_device *pdev);

static int mutekey_keyarray_event(struct input_dev *dev, unsigned int type,
             unsigned int code, int value)
{
  return 0;
}

static int mutekey_keyarray_open(struct input_dev *dev)
{
	int rc = 0;
    
    PRINT_IN
    mutex_lock(&g_mk->mutex);
    if( g_mk->open_count == 0 )
    {
        g_mk->open_count++;
        MY_INFO_PRINTK( 4, "INFO_LEVEL¡G" "mutekey open count : %d\n",g_mk->open_count );          
    }	
    else
    { 
		rc = -EFAULT;
		MY_INFO_PRINTK( 2, "ERROR_LEVEL¡G" "failed to open count : %d\n",g_mk->open_count );  
    }
    mutex_unlock(&g_mk->mutex);
    PRINT_OUT
    return rc;
}

static void mutekey_keyarray_close(struct input_dev *dev)
{
	PRINT_IN
    mutex_lock(&g_mk->mutex);
    if( g_mk->open_count )
    {
        g_mk->open_count--;
        MY_INFO_PRINTK( 4, "INFO_LEVEL:" "input device still opened %d times\n",g_mk->open_count );        
    }
    mutex_unlock(&g_mk->mutex);
    PRINT_OUT
}

static int mutekey_release_gpio( struct mutekey_t *g_mk )
{
    PRINT_IN
    MY_INFO_PRINTK( 4,"TEST_INFO_LEVEL:""releasing mutekey gpio num%d\n", g_mk->gpio_num );
    gpio_free( g_mk->gpio_num );
    PRINT_OUT 
    return 0;
}

static int mutekey_config_gpio( struct mutekey_t *g_mk )
{
	int rc=0;
	
	PRINT_IN
	#if 0
	rc = gpio_clear_detect_status( g_mk->gpio_num );
    if ( rc )
    {
        MY_INFO_PRINTK( 2,"ERROR_LEVEL:""clear detect status failed (rc=%d)\n", rc);
        PRINT_OUT
        return rc;
    }            
    #endif          
	PRINT_OUT
    return rc;
}

static int mutekey_setup_gpio( struct mutekey_t *g_mk )
{
    int rc;

    PRINT_IN
    MY_INFO_PRINTK( 4,"TEST_INFO_LEVEL:""setup mutekey gpio num %d\n", g_mk->gpio_num );
    rc = gpio_tlmm_config( GPIO_CFG(g_mk->gpio_num, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA),GPIO_CFG_ENABLE );
    if( rc )
    {
        MY_INFO_PRINTK( 2,"ERROR_LEVEL:""remote config gpio_num %d failed (rc=%d)\n", g_mk->gpio_num , rc );
		mutekey_release_gpio( g_mk );
		PRINT_OUT
		return rc;
    }

    rc = gpio_request( g_mk->gpio_num, "mXT224_touchpad_irq" );
    if ( rc )
    {
    	MY_INFO_PRINTK( 2,"ERROR_LEVEL:""request gpionum %d failed (rc=%d)\n", g_mk->gpio_num, rc );
		mutekey_release_gpio( g_mk );
		PRINT_OUT
		return rc;
    }

    rc = gpio_direction_input( g_mk->gpio_num );
    if ( rc )
    {
        MY_INFO_PRINTK( 2,"ERROR_LEVEL:""set gpionum %d mode failed (rc=%d)\n", g_mk->gpio_num, rc );
        mutekey_release_gpio( g_mk );
        PRINT_OUT
        return rc;
    }
	PRINT_OUT
    return rc;
}

#if 0
static void mutekey_suspend(struct early_suspend *h)
{
    int ret = 0;
    struct mutekey_t *g_mk = container_of( h,struct mutekey_t,key_early_suspend);
	
	PRINT_IN
    MY_INFO_PRINTK( 1, "INFO_LEVEL¡G" "mutekey_suspend : E\n" );
	mutex_lock(&g_mk->mutex);
	if( g_mk->mutekey_suspended )
	{
		mutex_unlock(&g_mk->mutex);
		PRINT_OUT
		return;
	}
	
	disable_irq( g_mk->irq );
	MY_INFO_PRINTK( 4, "INFO_LEVEL¡G" "disable irq %d\n",g_mk->irq );
	
    g_mk->mutekey_suspended = 1;
	ret = cancel_work_sync(&g_mk->key_work.work);
    if (ret)
    {
		enable_irq(g_mk->irq);
	}
	mutex_unlock(&g_mk->mutex);
    PRINT_OUT
	return;
}

static void mutekey_resume(struct early_suspend *h)
{
	int pinvalue;
	struct mutekey_t *g_mk = container_of( h,struct mutekey_t,key_early_suspend);
	
    PRINT_IN
	mutex_lock(&g_mk->mutex);
    if( 0 == g_mk->mutekey_suspended )
	{
		mutex_unlock(&g_mk->mutex);
		PRINT_OUT
		return;
	} 
    
	pinvalue = gpio_get_value(g_mk->gpio_num);
	g_mk->state = pinvalue;
	set_irq_type(g_mk->irq, pinvalue ? IRQF_TRIGGER_LOW:IRQF_TRIGGER_HIGH);

	enable_irq( g_mk->irq );
	if ( pinvalue == 0 ) 
	{	
		MY_INFO_PRINTK( 1,"TEST_INFO_LEVEL:"" MUTEKEY %d is pressed\n",KEY_RINGER_MUTE);
		input_report_key(g_mk->keyarray_input, KEY_RINGER_MUTE, 1);
		input_sync(g_mk->keyarray_input);
	}
	else
	{
		MY_INFO_PRINTK( 1,"TEST_INFO_LEVEL:"" %d is released\n",KEY_RINGER_MUTE );
		input_report_key( g_mk->keyarray_input, KEY_RINGER_MUTE, 0 );
		input_sync( g_mk->keyarray_input );
	}
	
	g_mk->mutekey_suspended = 0;
    mutex_unlock(&g_mk->mutex);
	PRINT_OUT
	return;
}
#endif

static int keyarray_register_input( struct input_dev **input,
                              struct platform_device *pdev )
{
	int rc = 0;
	struct input_dev *input_dev;
  
	input_dev = input_allocate_device();
	if ( !input_dev )
	{
		rc = -ENOMEM;
		return rc;
	}

	input_dev->name = MUTEKEY_DRIVER_NAME;
	input_dev->phys = "keypad_key/event0";
	input_dev->id.vendor = 0x0001;
	input_dev->id.product = 0x0002;
	input_dev->id.version = 0x0100;

	input_dev->open = mutekey_keyarray_open;
	input_dev->close = mutekey_keyarray_close;
	input_dev->event = mutekey_keyarray_event;

	input_dev->evbit[0] = BIT_MASK(EV_KEY);
  
	set_bit(KEY_RINGER_MUTE, input_dev->keybit);
 
	rc = input_register_device( input_dev );
	if ( rc )
	{
		MY_INFO_PRINTK( 2, "ERROR_LEVEL:" "failed to register keyarray input device\\n");
		input_free_device( input_dev );
	}
	else
	{
		*input = input_dev;
	}
	return rc;
}

static irqreturn_t mutekey_irqHandler( int irq , void *dev_id )
{
    struct mutekey_t *g_mk = dev_id;
	
	PRINT_IN
	MY_INFO_PRINTK( 4,"TEST_INFO_LEVEL:" "before disable_irq_nosync()\n" );
	disable_irq_nosync( irq );
	wake_lock(&g_mk->mutekey_wakelock);
    queue_delayed_work(g_mk->key_wqueue, &g_mk->key_work, 0);
	PRINT_OUT
	return IRQ_HANDLED;
}

static void mutekey_irqWorkHandler( struct work_struct *work )
{

	int pinvalue;
	
	PRINT_IN

	mutex_lock(&g_mk->mutex);
	msleep(10);
	
	for(;;)
	{
		pinvalue = gpio_get_value(g_mk->gpio_num);
		if(pinvalue != g_mk->state)
		{
			if ( pinvalue == 0 )
			{	
				MY_INFO_PRINTK( 1,"TEST_INFO_LEVEL:"" MUTE keycode[%d] p[%d]\n",KEY_RINGER_MUTE,pinvalue);
				MY_INFO_PRINTK( 1,"TEST_INFO_LEVEL:"" MUTE key up!!!\n");
				MY_INFO_PRINTK( 4,"TEST_INFO_LEVEL:"" MUTEKEY state[%d]\n",g_mk->state);
				input_report_key(g_mk->keyarray_input, KEY_RINGER_MUTE, 1);
				input_sync(g_mk->keyarray_input);				
			}
			else
			{
				MY_INFO_PRINTK( 1,"TEST_INFO_LEVEL:"" MUTE keycode[%d] p[%d]\n",KEY_RINGER_MUTE,pinvalue);
				MY_INFO_PRINTK( 1,"TEST_INFO_LEVEL:"" MUTE key down!!!\n");
				MY_INFO_PRINTK( 4,"TEST_INFO_LEVEL:"" MUTEKEY state[%d]\n",g_mk->state);
				input_report_key( g_mk->keyarray_input, KEY_RINGER_MUTE, 0 );
				input_sync( g_mk->keyarray_input );
			}
			set_irq_type(g_mk->irq, pinvalue ? IRQF_TRIGGER_LOW:IRQF_TRIGGER_HIGH);
			g_mk->state = pinvalue;
			MY_INFO_PRINTK( 4,"TEST_INFO_LEVEL:"" MUTEKEY state %d\n",g_mk->state);
			MY_INFO_PRINTK( 4,"TEST_INFO_LEVEL:"" MUTEKEY pinvalue %d\n",pinvalue);	
		}
		else
		{
			break;
		}
	}
	MY_INFO_PRINTK( 4,"TEST_INFO_LEVEL:" "before enable_irq()\n");
	enable_irq( g_mk->irq ); 
	mutex_unlock(&g_mk->mutex);
	wake_unlock(&g_mk->mutekey_wakelock);
    PRINT_OUT
    return;
}

static ssize_t mk_misc_read( struct file *fp,
                              char __user *buffer,
                              size_t count,
                              loff_t *ppos )
{
 	int value = 0;
	
	PRINT_IN
	mutex_lock(&g_mk->mutex);
	if(g_mk->check_state_flag == 1)
	{
		g_mk->check_state_flag= 0;
		mutex_unlock(&g_mk->mutex);
		PRINT_OUT
		return 0;
	}
	if ( count < sizeof(value) )
	{
		MY_INFO_PRINTK( 2, "ERROR_LEVEL:" "ts_misc_read: invalid count %d\n", count );
		mutex_unlock(&g_mk->mutex);
		PRINT_OUT
		return -EINVAL;
	}
	MY_INFO_PRINTK( 4, "INFO_LEVEL:" "count %d\n", count );
	
	g_mk->state = gpio_get_value(g_mk->gpio_num);
    if (g_mk->state == 0)
	{
		value = 48;
	}
	else if (g_mk->state == 1)
	{
		value = 49;
	}
	
	MY_INFO_PRINTK( 1,"TEST_INFO_LEVEL:"" MUTEKEY state %d\n",g_mk->state);
	if ( copy_to_user(buffer,&value,sizeof(value)) )
	{
		MY_INFO_PRINTK( 2, "ERROR_LEVEL:" "copy state to user failed\n");
		mutex_unlock(&g_mk->mutex);
		PRINT_OUT
		return -EINVAL;
	}
	g_mk->check_state_flag = 1;
	mutex_unlock(&g_mk->mutex);
	PRINT_OUT
	return sizeof(value);
}
static int mk_misc_release(struct inode *inode, struct file *fp)
{
    int result = 0;
    
    PRINT_IN
    mutex_lock(&g_mk->mutex);
    if( g_mk->misc_open_count )
    {
        g_mk->misc_open_count--;      
    }
	mutex_unlock(&g_mk->mutex);
    PRINT_OUT;
    return result;
}
static int mk_misc_open(struct inode *inode, struct file *fp)
{
    int result = 0;
    
    PRINT_IN
    mutex_lock(&g_mk->mutex);
	if( g_mk->misc_open_count ==0 )
    {
        g_mk->misc_open_count++;
		g_mk->check_state_flag = 0;
        MY_INFO_PRINTK( 4, "INFO_LEVEL¡G" "misc open count : %d\n",g_mk->misc_open_count );          
    }	
    else
    { 
		result = -EFAULT;
		MY_INFO_PRINTK( 2, "ERROR_LEVEL¡G" "failed to open misc count : %d\n",g_mk->misc_open_count );  
    }
    mutex_unlock(&g_mk->mutex);
    PRINT_OUT
    return result;
}
static struct file_operations mk_misc_fops = {
	.owner 	= THIS_MODULE,
	.open 	= mk_misc_open,
	.release = mk_misc_release,
	.read = mk_misc_read,
};
static struct miscdevice mk_misc_device = {
	.minor 	= MISC_DYNAMIC_MINOR,
	.name 	= "misc_mutekey",
	.fops 	= &mk_misc_fops,
};
static struct platform_driver plat_mutekey_driver = {
	.driver	 = {
		.name   = MUTEKEY_DRIVER_NAME,
		.owner	= THIS_MODULE,
	},
    .probe	  = mutekey_probe,
};

static int __ref mutekey_probe(struct platform_device *pdev)
{
	int    result;
	struct mutekey_t *mk;
    struct mutekey_platform_data_t *pdata;
	
    PRINT_IN
    mk = kzalloc( sizeof(struct mutekey_t), GFP_KERNEL );
    if( !mk )
    {
        result = -ENOMEM;
        return result;
    }
	
    memset( mk, 0, sizeof(struct mutekey_t));
	g_mk = mk;
	pdata = pdev->dev.platform_data;
	g_mk->gpio_num = pdata->gpio_mutekey;
	MY_INFO_PRINTK( 4,"TEST_INFO_LEVEL:" "get mutekey GPIO num %d\n",g_mk->gpio_num );
	g_mk->irq = MSM_GPIO_TO_INT( g_mk->gpio_num );
	platform_set_drvdata(pdev, g_mk);
	mutex_init(&g_mk->mutex);
	
    result = mutekey_setup_gpio( g_mk );
    if( result )
	{
        MY_INFO_PRINTK( 2, "ERROR_LEVEL:" "failed to setup gpio_mutekey\n" );
        mutekey_release_gpio( g_mk );
		kfree(g_mk);
        PRINT_OUT
        return result;
    }
	
    result = mutekey_config_gpio( g_mk );
    if( result )
    {
        MY_INFO_PRINTK( 2, "ERROR_LEVEL:" "failed to config gpio\n" );
        mutekey_release_gpio( g_mk );
        kfree(g_mk);
        PRINT_OUT
        return result;
    }
	
	result = keyarray_register_input( &g_mk->keyarray_input, NULL );
	if( result )
    {
		MY_INFO_PRINTK( 2, "ERROR_LEVEL:" "failed to register keyarray input\n" ); 
    	mutekey_release_gpio( g_mk );
        kfree(g_mk);
        PRINT_OUT
        return result;
    }
	input_set_drvdata(g_mk->keyarray_input, g_mk);
	
	result = misc_register( &mk_misc_device );
    if( result )
    {
       	MY_INFO_PRINTK( 2,"ERROR_LEVEL¡G" "failed register mute key misc driver\n" );
        result = -EFAULT;
		kfree(g_mk);	
    	PRINT_OUT
		return result;       
    }
    INIT_DELAYED_WORK( &g_mk->key_work, mutekey_irqWorkHandler );
    g_mk->key_wqueue = create_singlethread_workqueue("Mutekey_Wqueue");
    if (!g_mk->key_wqueue)
    {
		MY_INFO_PRINTK( 2, "ERROR_LEVEL:" "failed to create singlethread workqueue\n" );
    	result = -ESRCH;
    	input_unregister_device( g_mk->keyarray_input );
		input_free_device( g_mk->keyarray_input );
    	mutekey_release_gpio( g_mk );
		kfree(g_mk);
    	PRINT_OUT
    	return result;
    }
	
	wake_lock_init(&g_mk->mutekey_wakelock, WAKE_LOCK_SUSPEND, "mutekey");
	g_mk->state = gpio_get_value(g_mk->gpio_num);
    result = request_irq( g_mk->irq, mutekey_irqHandler, g_mk->state ? IRQF_TRIGGER_LOW:IRQF_TRIGGER_HIGH,"Mutekey_IRQ", g_mk );                      
    MY_INFO_PRINTK( 4, "TEST_INFO_LEVEL:" "---- request_irq ----\n" );
    if ( result )
    {
		MY_INFO_PRINTK( 2,"ERROR_LEVEL:""request interrupt: irq %d requested failed\n", g_mk->irq);
    	result = -EFAULT;
    	input_unregister_device( g_mk->keyarray_input );
		input_free_device( g_mk->keyarray_input );	
    	mutekey_release_gpio( g_mk );
		kfree(g_mk);
    	PRINT_OUT
    	return result;
    }
	
	result = set_irq_wake(g_mk->irq, 1);
	if(result < 0)
	{
		MY_INFO_PRINTK( 2,"ERROR_LEVEL:""set_irq_wake: irq %d failed\n", g_mk->irq);
    	result = -EFAULT;
    	input_unregister_device( g_mk->keyarray_input );
		input_free_device( g_mk->keyarray_input );
    	free_irq(g_mk->irq, 0);
		mutekey_release_gpio( g_mk );
		kfree(g_mk);
    	PRINT_OUT
		return result;
	}
	if ( g_mk->state == 0 ) 
	{	
		MY_INFO_PRINTK( 4,"TEST_INFO_LEVEL:"" MUTE keycode[%d] state[%d]\n",KEY_RINGER_MUTE,g_mk->state);
		MY_INFO_PRINTK( 4,"TEST_INFO_LEVEL:"" MUTE key up!!!\n");
		input_report_key(g_mk->keyarray_input, KEY_RINGER_MUTE, 1);
		input_sync(g_mk->keyarray_input);
	}
	else
	{
		MY_INFO_PRINTK( 4,"TEST_INFO_LEVEL:"" MUTE keycode[%d] state[%d]\n",KEY_RINGER_MUTE,g_mk->state);
		MY_INFO_PRINTK( 4,"TEST_INFO_LEVEL:"" MUTE key down!!!\n");
		input_report_key( g_mk->keyarray_input, KEY_RINGER_MUTE, 0 );
		input_sync( g_mk->keyarray_input );
	}
	
	PRINT_OUT
	return 0;
}	

static int __ref mutekey_init(void)
{
	int rc = 0;
        
   	PRINT_IN            
	MY_INFO_PRINTK( 1,"INFO_LEVEL¡G""system_rev=0x%x\n",system_rev);
	rc = platform_driver_register(&plat_mutekey_driver);
    PRINT_OUT
    return rc;
}
module_init(mutekey_init);

static void __exit mutekey_exit(void)
{  
	PRINT_IN
	platform_driver_unregister(&plat_mutekey_driver);
	#if 0
	i2c_del_driver( &i2c_mutekey_driver );
    input_unregister_device( g_tp->input );
	input_free_device( g_tp->input );
    destroy_workqueue( g_tp->touchpad_wqueue );
    touchpad_release_gpio( g_tp->gpio_num, g_tp->gpio_rst );
    kfree(g_tp);
	#endif
    PRINT_OUT
}
module_exit(mutekey_exit);

MODULE_DESCRIPTION("mutekey driver");

