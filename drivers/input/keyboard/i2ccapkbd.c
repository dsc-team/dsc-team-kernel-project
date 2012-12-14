/*
 *  drivers/input/keyboard/i2ccapkybd.c
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

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <mach/gpio.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <mach/pm_log.h>
#include <mach/austin_hwid.h>
#include <mach/smem_pc_oem_cmd.h>
#include <mach/custmproc.h>
#include <linux/delay.h>
#include <linux/leds.h>
#include <linux/time.h>

#define GETSOMELOG_AFTER_SCREENOFF 1

#if GETSOMELOG_AFTER_SCREENOFF
atomic_t GetSomeLogAfterLaterResume = ATOMIC_INIT(0);

#endif

#define CAPKEY_ESD_SOLUTION 1
#define CURRENT_CONTROL_LEDS 0

#undef CAPSENSOR_DEBUG
#ifdef CAPSENSOR_DEBUG
#define DBGPRINTK printk
#else
#define DBGPRINTK(a...)
#endif


#define DRIVER_VERSION "v1.0"

static const char *i2ccapkybd_name  = "synaptics_capsensor";

MODULE_VERSION(DRIVER_VERSION);
MODULE_AUTHOR("Qisda Incorporated");
MODULE_DESCRIPTION("I2C Cap-sensor keyboard driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:synaptics_capsensor");

struct i2ccapkybd_record;

#define QKBD_CAPSENSOR_GPIO 31

#define QKBD_CAP_STRIP_STATUS_POSITION 0x0106
#define QKBD_CAP_BUTTON_STATUS 0x0109
static bool dynamic_log_ebable = 0;

static int  cap_button_light = 0x007F7F7F;
void qi2ccapkybd_update_led(unsigned int light);
void qi2ccapkybd_update_led_forced(unsigned int light);

enum kbd_inevents {
  QKBD_IN_KEYPRESS        = 1,
  QKBD_IN_KEYRELEASE      = 0,
  QKBD_IN_MXKYEVTS        = 16
};


enum kbd_constants {
  QKBD_PHYSLEN    = 128,
  QCVENDOR_ID   = 0x5143
};


struct capsensor_reg_t {
    unsigned short addr;
    unsigned short  data;
};


struct i2ccapkybd_record {
  struct i2c_client *mykeyboard;
  struct input_dev *i2ckbd_idev;
  int irq;
  int     gpio_num;
  char  physinfo[QKBD_PHYSLEN];
  int keystate;
  int lastkeycode;
#ifdef CONFIG_HAS_EARLYSUSPEND
  struct early_suspend capsensor_early_suspend;
#if CAPKEY_ESD_SOLUTION
  struct early_suspend capsensor_very_early_suspend;
#endif
#endif
};

static struct i2ccapkybd_record kbd_data;

static struct work_struct qkybd_irqwork;

#define KBDIRQNO(kbdrec)  MSM_GPIO_TO_INT(kbdrec->mykeyboard->irq)

#ifdef CONFIG_HAS_EARLYSUSPEND
static void capsensor_early_suspend(struct early_suspend *h);
static void capsensor_early_resume(struct early_suspend *h);
#if CAPKEY_ESD_SOLUTION
static void capsensor_very_early_suspend(struct early_suspend *h);
static void capsensor_very_early_resume(struct early_suspend *h);
unsigned long start_jiffies;
unsigned int sleep_ms_time;
#endif
#endif
atomic_t capsensor_is_in_suspend = ATOMIC_INIT(0);
static int capsensor_param=1;

static struct mutex qi2ccapkybd_lock;
atomic_t qi2ccapkybd_inited = ATOMIC_INIT(0);

static int qi2ccapkybd_reset(void);
static int qi2ccapkybd_probe(struct i2c_client *client, const struct i2c_device_id *id);
static void qi2ccapkybd_capsensor_init(struct i2c_client *kbd);

static irqreturn_t qi2ccapkybd_irqhandler(int irq, void *dev_id)
{
  schedule_work(&qkybd_irqwork);
  return IRQ_HANDLED;
}

static int qi2ccapkybd_irqsetup(int kbd_irqpin)
{
  int rc = request_irq(MSM_GPIO_TO_INT(kbd_irqpin), &qi2ccapkybd_irqhandler,
           IRQF_TRIGGER_FALLING, i2ccapkybd_name, NULL);
  if (rc < 0) {
    printk(KERN_ERR
           "Could not register for  %s interrupt "
           "(rc = %d)\n", i2ccapkybd_name, rc);
    rc = -EIO;
  }
  return rc;
}

static int qi2ccapkybd_release_gpio(int kbd_irqpin)
{
  gpio_free(kbd_irqpin);
  return 0;
}

 static int qi2ccapkybd_config_gpio(int kbd_irqpin)
{
  int rc;

  gpio_tlmm_config(GPIO_CFG(kbd_irqpin, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);


  rc = gpio_request(kbd_irqpin, "gpio_keybd_irq");
  if (rc) {
    printk("cap_i2c_write: gpio_request failed on pin %d\n",kbd_irqpin);
    goto err_gpioconfig;
  }
  rc = gpio_direction_input(kbd_irqpin);
  if (rc) {
    printk("cap_i2c_write: gpio_direction_input failed on pin %d\n",kbd_irqpin);
    goto err_gpioconfig;
  }
  return rc;

err_gpioconfig:
  qi2ccapkybd_release_gpio(kbd_irqpin);
  return rc;
}


static int32_t cap_i2c_write(struct i2c_client *kbd, struct capsensor_reg_t *regs, uint32_t size)
{
    unsigned char buf[4];
    struct i2c_msg msg[] =
    {
        [0] = {
      .addr = kbd->addr,
      .flags = 0,
      .buf = buf,
      .len = 4,
        }
    };

        buf[0] = (regs->addr & 0xFF00)>>8;
        buf[1] = (regs->addr & 0x00FF);
        buf[2] = (regs->data & 0xFF00)>>8;
        buf[3] = (regs->data & 0x00FF);

        if ( i2c_transfer( kbd->adapter, msg, 1) < 0 ) {
            printk("cap_i2c_write: write %Xh=0x%X failed\n", regs->addr, regs->data );
//n0p
		udelay(200);
		if ( i2c_transfer( kbd->adapter, msg, 1) < 0 ) {
			printk("NAK cap_i2c_write retry: write %Xh=0x%X failed\n", regs->addr, regs->data );
            		return -EIO;
		}
        }

    return 1;
}

static int32_t cap_i2c_read(struct i2c_client *kbd, struct capsensor_reg_t *regs, uint32_t size)
{
    unsigned char buf[2];
    unsigned char dataBuf[2];
    int ret = 0;

    struct i2c_msg msgs[] =
    {
        [0] = {
            .addr   = kbd->addr,
            .flags  = 0,
            .buf    = (void *)buf,
            .len    = 2
        },
        [1] = {
            .addr   = kbd->addr,
            .flags  = I2C_M_RD,
            .buf    = (void *)buf,
            .len    = size*2
        }
    };


    buf[0] = (regs->addr & 0xFF00)>>8;
    buf[1] = (regs->addr & 0x00FF);
    msgs[1].buf = (void *)dataBuf;

    ret = i2c_transfer(kbd->adapter, msgs, 2);

    if( ret != ARRAY_SIZE(msgs) )
    {
        printk(KERN_ERR "cap_i2c_read: read %d bytes return failure,buf=0x%xh , size=%d\n", ret, (unsigned int)buf, (unsigned int)size );
//n0p
		udelay(200);
                ret = i2c_transfer(kbd->adapter, msgs, 2);
		    if( ret != ARRAY_SIZE(msgs) ) {
			  printk(KERN_ERR "NAK cap_i2c_read retry: read %d bytes return failure,buf=0x%xh , size=%d\n", ret, (unsigned int)buf, (unsigned int)size );
		        return ret;
                    }
    }

    regs->data = (dataBuf[0]<<8) | (dataBuf[1]);

    return 0;

}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void capsensor_early_suspend(struct early_suspend *h)
{
  struct i2c_client *kbd = kbd_data.mykeyboard;
  struct   capsensor_reg_t temp_write;

  printk(KERN_ERR "capsensor_early_suspend()+\n");
  if (!atomic_read(&capsensor_is_in_suspend))
  {
    atomic_set(&capsensor_is_in_suspend,1);
    if(atomic_read(&qi2ccapkybd_inited))
    {
      temp_write.addr = 0x1;
      temp_write.data = 0x00F0;
      if(!cap_i2c_write(kbd, &temp_write, 1)) {
        printk(KERN_ERR "write  failed. temp_write.data=0x%x\n",temp_write.data);
//n0p
        udelay(200);
	if(!cap_i2c_write(kbd, &temp_write, 1)) printk(KERN_ERR "Retry write  failed. temp_write.data=0x%x\n",temp_write.data);
		else 
	PM_LOG_EVENT(PM_LOG_OFF, PM_LOG_SENSOR_CAP);
	}
      else
        PM_LOG_EVENT(PM_LOG_OFF, PM_LOG_SENSOR_CAP);
    }
  }

#if GETSOMELOG_AFTER_SCREENOFF
  atomic_set(&GetSomeLogAfterLaterResume,0);
#endif

  printk(KERN_ERR "capsensor_early_suspend()-\n");
}

static void capsensor_early_resume(struct early_suspend *h)
{
  struct i2c_client *kbd = kbd_data.mykeyboard;
  struct   capsensor_reg_t temp_write;
  struct i2ccapkybd_record *rd = &kbd_data;

  DBGPRINTK(KERN_ERR  "%s +\n", __func__);

  if (atomic_read(&capsensor_is_in_suspend))
  {
	atomic_set(&capsensor_is_in_suspend,0);

	#if CAPKEY_ESD_SOLUTION
        mutex_lock(&qi2ccapkybd_lock);

	sleep_ms_time = jiffies_to_msecs(jiffies - start_jiffies);
	if(sleep_ms_time < 250 )
 	{
		msleep(250 - sleep_ms_time);
        }     
	if(dynamic_log_ebable)
	{
		printk(KERN_ERR "%d ms passed after Capkey IC reset\n", sleep_ms_time);
	}
	qi2ccapkybd_capsensor_init(kbd);
	if(dynamic_log_ebable)
	{
		printk(KERN_ERR "%s [re-init] Cap-Sensor Done!!\n",__func__);
	}
	atomic_set(&qi2ccapkybd_inited,1);

	enable_irq( rd->irq );

        mutex_unlock(&qi2ccapkybd_lock);
	#endif

    if(atomic_read(&qi2ccapkybd_inited))
    {
	#if !CAPKEY_ESD_SOLUTION
        mutex_lock(&qi2ccapkybd_lock);
        qi2ccapkybd_update_led(cap_button_light);
        mutex_unlock(&qi2ccapkybd_lock);
      #endif

      temp_write.addr = 0x1;
      temp_write.data = 0x0030;
      if(!cap_i2c_write(kbd, &temp_write, 1))
        printk(KERN_ERR "write  failed. temp_write.data=0x%x\n",temp_write.data);
      else
        PM_LOG_EVENT(PM_LOG_ON, PM_LOG_SENSOR_CAP);
    }
  }

#if GETSOMELOG_AFTER_SCREENOFF
  atomic_set(&GetSomeLogAfterLaterResume,8);
#endif

  DBGPRINTK(KERN_ERR  "%s -\n", __func__);
}


#if CAPKEY_ESD_SOLUTION
static void capsensor_very_early_suspend(struct early_suspend *h)
{
  DBGPRINTK(KERN_ERR  "%s +\n", __func__);
  DBGPRINTK(KERN_ERR  "%s -\n", __func__);
}
static void capsensor_very_early_resume(struct early_suspend *h)
{
  struct i2c_client *kbd = kbd_data.mykeyboard;
  struct   capsensor_reg_t temp_write;
  struct i2ccapkybd_record *rd = &kbd_data;

  DBGPRINTK(KERN_ERR  "%s +\n", __func__);

  if (atomic_read(&capsensor_is_in_suspend))
  {
    if(atomic_read(&qi2ccapkybd_inited))
    {
        mutex_lock(&qi2ccapkybd_lock);
	atomic_set(&qi2ccapkybd_inited,0);
	temp_write.addr = 0x0300;
	temp_write.data = 0x0001;
	if(!cap_i2c_write(kbd, &temp_write, 1))
	{
		printk(KERN_ERR "[reset Cap. IC] write failed. temp_write.data=0x%x\n",temp_write.data);
}
	start_jiffies = jiffies;
	disable_irq( rd->irq );

	if(dynamic_log_ebable)
		printk(KERN_ERR "[reset Cap. IC] it needs to wait at least 250 ms\n");
	
	mutex_unlock(&qi2ccapkybd_lock);
    }
  }
  DBGPRINTK(KERN_ERR  "%s -\n", __func__);
}
#endif
#endif

#if 1
void qi2ccapkybd_update_led(unsigned int light)
{
  struct i2c_client *kbdcl = kbd_data.mykeyboard;
  struct capsensor_reg_t creg;
  char led0, led1, led2, tmp;
  unsigned short data;

  if(system_rev <= EVT3P2_Band18)
  {
    led0 = (light & 0x0000FF) >> 1;
    led1 = (light & 0x00FF00) >> 9;
    led2 = (light & 0xFF0000) >> 17;

    creg.addr = 0x0023;
    creg.data = led0 + (led1 << 8);
    cap_i2c_write(kbdcl, &creg, 1);

    creg.addr = 0x0024;
    creg.data = led2;
    cap_i2c_write(kbdcl, &creg, 1);

    data = 0;
    if(led0) data =  0x01;
    if(led1) data |= 0x02;
    if(led2) data |= 0x04;

    creg.addr = 0x000E;
    creg.data = ((~data)&0x07) | 0x0700;
    cap_i2c_write(kbdcl, &creg, 1);

    creg.addr = 0x0022;
    creg.data = data;
    cap_i2c_write(kbdcl, &creg, 1);
  }
  else
  {
    led0 = (light & 0xFF);
    tmp  = led0*22/256;
    if(tmp==0 && led0!=0)   led0 = 1;
    else                    led0 = tmp;
    led1 = (light >> 8) & 0xFF;;
    tmp  = led1*22/256;
    if(tmp==0 && led1!=0)   led1 = 1;
    else                    led1 = tmp;
    led2 = (light >> 16) & 0xFF;
    tmp  = led2*22/256;
    if(tmp==0 && led2!=0)   led2 = 1;
    else                    led2 = tmp;

    #if CURRENT_CONTROL_LEDS
      creg.addr = 0x0023;
      creg.data = 0xFF00;
      cap_i2c_write(kbdcl, &creg, 1);

      creg.addr = 0x0024;
      creg.data = 0;
      cap_i2c_write(kbdcl, &creg, 1);

      creg.addr = 0x0025;
      creg.data = (led0 | (led1 << 8));
      cap_i2c_write(kbdcl, &creg, 1);

      creg.addr = 0x0026;
      creg.data = led2;
      cap_i2c_write(kbdcl, &creg, 1);
    #else
      creg.addr = 0x0023;
      creg.data = 0;
      cap_i2c_write(kbdcl, &creg, 1);

      creg.addr = 0x0024;
      creg.data = 0;
      cap_i2c_write(kbdcl, &creg, 1);

      creg.addr = 0x0025;
      creg.data = 0x8080| (led0 | (led1 << 8));
      cap_i2c_write(kbdcl, &creg, 1);

      creg.addr = 0x0026;
      creg.data = 0x80| led2;
      cap_i2c_write(kbdcl, &creg, 1);
    #endif

    creg.addr = 0x0027;
    creg.data = 0x0000;
    cap_i2c_write(kbdcl, &creg, 1);

    creg.addr = 0x000E;
    creg.data = 0x1C1C;
    cap_i2c_write(kbdcl, &creg, 1);

    data = 0;
    if(led0) data =  0x04;
    if(led1) data |= 0x08;
    if(led2) data |= 0x10;

    creg.addr = 0x0022;
    creg.data = data;
    cap_i2c_write(kbdcl, &creg, 1);

    creg.addr = 0x000F;
    creg.data = 0x0100;
    cap_i2c_write(kbdcl, &creg, 1);
  }

  if(light)
    PM_LOG_EVENT(PM_LOG_ON, PM_LOG_BL_KEYPAD);
  else
    PM_LOG_EVENT(PM_LOG_OFF, PM_LOG_BL_KEYPAD);
}
void qi2ccapkybd_set_led(unsigned int light)
{
  mutex_lock(&qi2ccapkybd_lock);

  cap_button_light = light;

  if(atomic_read(&qi2ccapkybd_inited) && !atomic_read(&capsensor_is_in_suspend))
  {
    qi2ccapkybd_update_led(light);
  }
  else
  {
  }

  mutex_unlock(&qi2ccapkybd_lock);
}
EXPORT_SYMBOL(qi2ccapkybd_set_led);

static struct device_attribute qsd_led_button_led_attrs[3];
static char button_led_test_enable = 0;

static ssize_t qsd_led_button_led_show(struct device *dev, struct device_attribute *attr, char *buf)
{
  const ptrdiff_t off = attr - qsd_led_button_led_attrs;
  ssize_t ret = -EINVAL;
  unsigned int light;

  if(off >= ARRAY_SIZE(qsd_led_button_led_attrs))
    goto exit;

  light = cap_button_light;

  if(off == 0)
    light = light & 0xFF;
  else if(off == 1)
    light = (light >> 8) & 0xFF;
  else
    light = (light >> 16) & 0xFF;

  sprintf(buf, "%u\n", light);
  ret = strlen(buf) + 1;

exit:
  return ret;
}

void qi2ccapkybd_update_led_forced(unsigned int light)
{
static int is_in_suspend;
struct i2c_client *kbd = kbd_data.mykeyboard;
struct   capsensor_reg_t temp_write;
atomic_read(&capsensor_is_in_suspend);
if (is_in_suspend)
    {
        temp_write.addr = 0x1;
        temp_write.data = 0x0030;
	cap_i2c_write(kbd, &temp_write, 1);
    }
printk("DSC: Led update called");
//n0p - turn on the lights :)
qi2ccapkybd_update_led(light);

if (is_in_suspend)
    {  
        temp_write.addr = 0x1;
        temp_write.data = 0x0080;
	cap_i2c_write(kbd, &temp_write, 1);
    }
}

static ssize_t qsd_led_button_led_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
  const ptrdiff_t off = attr - qsd_led_button_led_attrs;
  ssize_t ret = -EINVAL;
  char *after;
  unsigned long light = simple_strtoul(buf, &after, 10);
  unsigned int led_old, led_new;

  if(buf[0] == 't')
  {
    if(buf[1] == '1')
      button_led_test_enable = 1;
    else if(buf[1] == '0')
      button_led_test_enable = 0;
    printk("button_led_test_enable = %d", button_led_test_enable);
    ret = count;
    goto exit;
  }

  if((light > LED_FULL) || (off >= ARRAY_SIZE(qsd_led_button_led_attrs)))
    goto exit;

  led_old = cap_button_light;

  if(off == 0)
    led_new = (led_old & 0xFFFF00) | light;
  else if(off == 1)
    led_new = (led_old & 0xFF00FF) | (light<<8);
  else
    led_new = (led_old & 0x00FFFF) | (light<<16);

  qi2ccapkybd_set_led(led_new);

  ret = count;

exit:
  return ret;
}

static struct device_attribute qsd_led_button_led_attrs[] = {
  __ATTR(led0, 0664, qsd_led_button_led_show, qsd_led_button_led_store),
  __ATTR(led1, 0664, qsd_led_button_led_show, qsd_led_button_led_store),
  __ATTR(led2, 0664, qsd_led_button_led_show, qsd_led_button_led_store),
};

static void qsd_led_button_backlight_set(struct led_classdev *led_cdev, enum led_brightness value)
{
  unsigned int light;

  if(button_led_test_enable)
    return;

  light = value + (value<<8) + (value<<16);
  qi2ccapkybd_set_led(light);
}
static struct led_classdev qsd_led_button_backlight = {
  .name     = "button-backlight",
  .brightness = LED_OFF,
  .brightness_set   = qsd_led_button_backlight_set,
};
#endif

static int qi2ccapkybd_reset(void)
{
  struct i2ccapkybd_record *kbdrec = &kbd_data;
  struct i2c_client *kbdcl = kbdrec->mykeyboard;
  struct capsensor_reg_t temp_write;

  atomic_set(&qi2ccapkybd_inited,0);

  temp_write.addr = 0x0300;
  temp_write.data = 0x0001;
  if(!cap_i2c_write(kbdcl, &temp_write, 1))
  {
    printk(KERN_ERR "[reset Cap. IC] write failed. temp_write.data=0x%x\n",temp_write.data);
    return 0;
  }
  msleep(250);
  printk(KERN_ERR "[reset Cap. IC] delay 250ms\n");

  qi2ccapkybd_capsensor_init(kbdcl);
  printk(KERN_ERR "[reset Cap. IC] re-init Cap-Sensor Done!!\n");

  atomic_set(&qi2ccapkybd_inited,1);

  return 1;
}

static void qi2ccapkybd_fetchkeys(struct work_struct *work)
{
  struct i2ccapkybd_record *kbdrec = &kbd_data;
  struct i2c_client *kbdcl = kbdrec->mykeyboard;
  struct input_dev *idev = kbdrec->i2ckbd_idev;
  struct capsensor_reg_t temp_rd;
  unsigned short data_back,data_home,data_menu;

  if(dynamic_log_ebable)
    printk(KERN_ERR "Got Capkey INT\n");

  if(system_rev <= EVT3P2_Band18)
  {
    data_back = 0x40;
    data_home = 0x20;
    data_menu = 0x04;
  }
  else
  {
    data_back = 0x10;
    data_home = 0x08;
    data_menu = 0x01;
  }

  mutex_lock(&qi2ccapkybd_lock);

  temp_rd.addr = QKBD_CAP_BUTTON_STATUS;
  temp_rd.data = 0;

  if(!cap_i2c_read(kbdcl, &temp_rd, 1))
  {
    if(dynamic_log_ebable)
      printk(KERN_ERR "temp_rd.data=0x%x\n", temp_rd.data);


#if GETSOMELOG_AFTER_SCREENOFF
	if (atomic_read(&GetSomeLogAfterLaterResume) >0 )
	{
		printk(KERN_ERR "[capkey]temp_rd.data=0x%x\n", temp_rd.data);
	}
#endif

    if(temp_rd.data & data_back)
    {
      if(kbd_data.keystate == QKBD_IN_KEYPRESS)
      {
        input_report_key(idev, kbd_data.lastkeycode, QKBD_IN_KEYRELEASE);
#if GETSOMELOG_AFTER_SCREENOFF
	  	if (atomic_read(&GetSomeLogAfterLaterResume) >0 )
	  		printk(KERN_ERR "[capkey]report KEY[%d] Release at first\n",kbd_data.lastkeycode);
#endif
      }
      kbd_data.keystate = QKBD_IN_KEYPRESS;
      kbd_data.lastkeycode = KEY_BACK;
      input_report_key(idev, KEY_BACK, QKBD_IN_KEYPRESS);
#if GETSOMELOG_AFTER_SCREENOFF
	  if (atomic_read(&GetSomeLogAfterLaterResume) >0 )
	  	printk(KERN_ERR "[capkey]report KEY_BACK KEYPRESS\n");
#endif
    }
    if(temp_rd.data & data_home)
    {
      if(kbd_data.keystate == QKBD_IN_KEYPRESS)
      {
        input_report_key(idev, kbd_data.lastkeycode, QKBD_IN_KEYRELEASE);
#if GETSOMELOG_AFTER_SCREENOFF
	  	if (atomic_read(&GetSomeLogAfterLaterResume) >0 )
	  		printk(KERN_ERR "[capkey]report KEY[%d] Release at first\n",kbd_data.lastkeycode);
#endif
      }
      kbd_data.keystate = QKBD_IN_KEYPRESS;
      kbd_data.lastkeycode = KEY_HOME;
      input_report_key(idev, KEY_HOME, QKBD_IN_KEYPRESS);
#if GETSOMELOG_AFTER_SCREENOFF
	  if (atomic_read(&GetSomeLogAfterLaterResume) >0 )
	  	printk(KERN_ERR "[capkey]report KEY_HOME KEYPRESS\n");
#endif
    }
    if(temp_rd.data & data_menu)
    {
      if(kbd_data.keystate == QKBD_IN_KEYPRESS)
      {
        input_report_key(idev, kbd_data.lastkeycode, QKBD_IN_KEYRELEASE);
#if GETSOMELOG_AFTER_SCREENOFF
	  	if (atomic_read(&GetSomeLogAfterLaterResume) >0 )
	  		printk(KERN_ERR "[capkey]report KEY[%d] Release at first\n",kbd_data.lastkeycode);
#endif
      }
      kbd_data.keystate = QKBD_IN_KEYPRESS;
      kbd_data.lastkeycode = KEY_MENU;
      input_report_key(idev, KEY_MENU, QKBD_IN_KEYPRESS);
#if GETSOMELOG_AFTER_SCREENOFF
	  if (atomic_read(&GetSomeLogAfterLaterResume) >0 )
	  	printk(KERN_ERR "[capkey]report KEY_MENU KEYPRESS\n");
#endif
    }
    if((temp_rd.data == 0)&&(kbd_data.keystate == QKBD_IN_KEYPRESS))
    {
      input_report_key(idev, kbd_data.lastkeycode, QKBD_IN_KEYRELEASE);
      kbd_data.keystate = QKBD_IN_KEYRELEASE;
      kbd_data.lastkeycode = 0;
#if GETSOMELOG_AFTER_SCREENOFF
	  	if (atomic_read(&GetSomeLogAfterLaterResume) >0 )
	  		printk(KERN_ERR "[capkey]report KEY[%d] Release\n",kbd_data.lastkeycode);
#endif
    }
  }
  else
  {
    printk(KERN_ERR "qi2ccapkybd_fetchkeys() read failed. temp_rd.data=0x%x\n",temp_rd.data);
    if(!cap_i2c_read(kbdcl, &temp_rd, 1))
      printk(KERN_ERR "qi2ccapkybd_fetchkeys() read again Done, temp_rd.data=0x%x\n",temp_rd.data);
    else
    {
      printk(KERN_ERR "qi2ccapkybd_fetchkeys() read failed again, rest Cap-Sensor here.\n");
      qi2ccapkybd_reset();
    }
  }

#if GETSOMELOG_AFTER_SCREENOFF
  atomic_dec(&GetSomeLogAfterLaterResume);
#endif

  mutex_unlock(&qi2ccapkybd_lock);

}

static void qi2ccapkybd_shutdown(struct i2ccapkybd_record *rd)
{
  dev_info(&rd->mykeyboard->dev, "disconnecting keyboard\n");
  free_irq(KBDIRQNO(rd), NULL);
}

static int qi2ccapkybd_eventcb(struct input_dev *dev, unsigned int type,
             unsigned int code, int value)
{
  printk(KERN_ERR "qi2ccapkybd_eventcb()\n");
  return 0;
}

static void qi2ccapkybd_capsensor_init(struct i2c_client *kbd)
{
  struct   capsensor_reg_t temp_write;
  struct   capsensor_reg_t temp_rd;

  temp_write.addr = 0x0;
  temp_write.data = 0x0004;
  if(!cap_i2c_write(kbd, &temp_write, 1))
    printk(KERN_ERR "write failed. temp_write.data=0x%x\n",temp_write.data);

  temp_write.addr = 0x1;
  temp_write.data = 0x0030;
  if(!cap_i2c_write(kbd, &temp_write, 1))
    printk(KERN_ERR "write  failed. temp_write.data=0x%x\n",temp_write.data);

  temp_write.addr = 0x2;
  temp_write.data = 0x0032;
  if(!cap_i2c_write(kbd, &temp_write, 1))
    printk(KERN_ERR "write  failed. temp_write.data=0x%x\n",temp_write.data);

  if(system_rev <= EVT3P2_Band18)
  {
    temp_write.addr = 0x4;
    temp_write.data = 0x0064;
    if(!cap_i2c_write(kbd, &temp_write, 1))
      printk(KERN_ERR "write  failed. temp_write.data=0x%x\n",temp_write.data);

    temp_write.addr = 0x8;
    temp_write.data = 0x0;
    if(!cap_i2c_write(kbd, &temp_write, 1))
      printk(KERN_ERR "write  failed. temp_write.data=0x%x\n",temp_write.data);
  }
  else
  {
    temp_write.addr = 0x4;
    temp_write.data = 0x0019;
    if(!cap_i2c_write(kbd, &temp_write, 1))
      printk(KERN_ERR "write  failed. temp_write.data=0x%x\n",temp_write.data);
  }

  temp_write.addr = 0x10;
  temp_write.data = 0x0;
  if(!cap_i2c_write(kbd, &temp_write, 1))
    printk(KERN_ERR "write  failed. temp_write.data=0x%x\n",temp_write.data);

  if (system_rev <= EVT1_4G_4G)
  {
    if(dynamic_log_ebable)
    printk(KERN_ERR "EVT1 Cap-key value, system_rev=%d\n", system_rev);

    temp_write.addr = 0x11;
    temp_write.data = 0x008C;
    if(!cap_i2c_write(kbd, &temp_write, 1))
      printk(KERN_ERR "write  failed. temp_write.data=0x%x\n",temp_write.data);

    temp_write.addr = 0x12;
    temp_write.data = 0x8C00;
    if(!cap_i2c_write(kbd, &temp_write, 1))
      printk(KERN_ERR "write  failed. temp_write.data=0x%x\n",temp_write.data);

    temp_write.addr = 0x13;
    temp_write.data = 0x008C;
    if(!cap_i2c_write(kbd, &temp_write, 1))
      printk(KERN_ERR "write  failed. temp_write.data=0x%x\n",temp_write.data);
  }
  else if((system_rev == EVT2_Band125)||(system_rev == EVT2_Band18))
  {
    unsigned int arg1=0;
    unsigned int arg2HOME=0, arg2MENU=0, arg2BACK=0;
    int retValue=0;

    arg1 = SMEM_PC_OEM_GET_FA_HOME_KEY_SENSITIVITY;
    retValue=cust_mproc_comm1(&arg1,&arg2HOME);

    arg1 = SMEM_PC_OEM_GET_FA_MENU_KEY_SENSITIVITY;
    retValue=cust_mproc_comm1(&arg1,&arg2MENU);

    arg1 = SMEM_PC_OEM_GET_FA_BACK_KEY_SENSITIVITY;
    retValue=cust_mproc_comm1(&arg1,&arg2BACK);

    if(dynamic_log_ebable)
    printk(KERN_ERR "EVT2-1 Cap-key value, system_rev=%d\n", system_rev);

    temp_write.addr = 0x11;
    temp_write.data = 0x00A0;
    if(!cap_i2c_write(kbd, &temp_write, 1))
      printk(KERN_ERR "write  failed. temp_write.data=0x%x\n",temp_write.data);
    else
      printk(KERN_ERR "[capkey]Menu default assign 0x00A0\n");

    temp_write.addr = 0x12;
    temp_write.data = 0xA300;
    if(!cap_i2c_write(kbd, &temp_write, 1))
      printk(KERN_ERR "write  failed. temp_write.data=0x%x\n",temp_write.data);
    else
      printk(KERN_ERR "[capkey]HOME default assign 0xA300");

    temp_write.addr = 0x13;
    temp_write.data = 0x009F;
    if(!cap_i2c_write(kbd, &temp_write, 1))
      printk(KERN_ERR "write  failed. temp_write.data=0x%x\n",temp_write.data);
    else
      printk(KERN_ERR "[capkey]BACK default assign 0x009F\n");
  }
  else if(system_rev <= EVT3P2_Band18)
  {
    unsigned int arg1=0;
    unsigned int arg2HOME=0, arg2MENU=0, arg2BACK=0;
    int retValue=0;

    arg1 = SMEM_PC_OEM_GET_FA_HOME_KEY_SENSITIVITY;
    retValue=cust_mproc_comm1(&arg1,&arg2HOME);

    arg1 = SMEM_PC_OEM_GET_FA_MENU_KEY_SENSITIVITY;
    retValue=cust_mproc_comm1(&arg1,&arg2MENU);

    arg1 = SMEM_PC_OEM_GET_FA_BACK_KEY_SENSITIVITY;
    retValue=cust_mproc_comm1(&arg1,&arg2BACK);

    if(dynamic_log_ebable)
    	printk(KERN_ERR "EVT2-2~EVT3-2 Cap-key value, system_rev=%d\n", system_rev);

    temp_write.addr = 0x11;
    if ( arg2MENU>0xA0 || arg2MENU<0x94)
    {
      temp_write.data = 0x00A0;
      printk(KERN_ERR "[capkey]FA Menu out of range=0x%x, default assign 0x00A0\n",arg2MENU);
    }
    else
    {
      temp_write.data = (unsigned short)arg2MENU;
      if(dynamic_log_ebable)
      printk(KERN_ERR "[capkey]FA Read Menu=0x%x\n",temp_write.data);
    }
    if(!cap_i2c_write(kbd, &temp_write, 1))
      printk(KERN_ERR "write  failed. temp_write.data=0x%x\n",temp_write.data);

    temp_write.addr = 0x12;
    if ( arg2HOME>0xA3 || arg2HOME<0x96)
    {
      temp_write.data = 0xA300;
      printk(KERN_ERR "[capkey]FA HOME out of range=0x%x, default assign 0xA300\n",arg2HOME);
    }
    else
    {
      temp_write.data = (unsigned short)((arg2HOME)<<8);
      if(dynamic_log_ebable)
      printk(KERN_ERR "[capkey]FA Read Home=0x%x\n",temp_write.data);
    }
    if(!cap_i2c_write(kbd, &temp_write, 1))
      printk(KERN_ERR "write  failed. temp_write.data=0x%x\n",temp_write.data);

    temp_write.addr = 0x13;
    if ( arg2BACK>0x9F || arg2BACK<0x90)
    {
      temp_write.data = 0x009F;
      printk(KERN_ERR "[capkey]FA BACK out of range=0x%x, default assign 0x009F\n",arg2BACK);
    }
    else
    {
      temp_write.data = (unsigned short)arg2BACK;
      if(dynamic_log_ebable)
      printk(KERN_ERR "[capkey]FA Read Back=0x%x\n",temp_write.data);
    }
    if(!cap_i2c_write(kbd, &temp_write, 1))
      printk(KERN_ERR "write  failed. temp_write.data=0x%x\n",temp_write.data);
  }
  else
  {
    unsigned int arg1=0;
    unsigned int arg2HOME=0, arg2MENU=0, arg2BACK=0;
    int retValue=0;

    arg1 = SMEM_PC_OEM_GET_FA_HOME_KEY_SENSITIVITY;
    retValue=cust_mproc_comm1(&arg1,&arg2HOME);

    arg1 = SMEM_PC_OEM_GET_FA_MENU_KEY_SENSITIVITY;
    retValue=cust_mproc_comm1(&arg1,&arg2MENU);

    arg1 = SMEM_PC_OEM_GET_FA_BACK_KEY_SENSITIVITY;
    retValue=cust_mproc_comm1(&arg1,&arg2BACK);

    if(dynamic_log_ebable)
    	printk(KERN_ERR "EVT3-3~... SO380000QFN Cap-key value, system_rev=%d\n", system_rev);

    temp_write.addr = 0x10;
    if ( arg2MENU>0xA0 || arg2MENU<0x94)
    {
      temp_write.data = 0x00A0;
      printk(KERN_ERR "[capkey]FA Menu out of range=0x%x, default assign 0x00A0\n",arg2MENU);
    }
    else
    {
      temp_write.data = (unsigned short)arg2MENU;
      if(dynamic_log_ebable)
      printk(KERN_ERR "[capkey]FA Read Menu=0x%x\n",temp_write.data);
    }
    if(!cap_i2c_write(kbd, &temp_write, 1))
      printk(KERN_ERR "write  failed. temp_write.data=0x%x\n",temp_write.data);

    temp_write.addr = 0x11;
    if ( arg2HOME>0xA3 || arg2HOME<0x96)
    {
      temp_write.data = 0xA300;
      printk(KERN_ERR "[capkey]FA HOME out of range=0x%x, default assign 0xA300\n",arg2HOME);
    }
    else
    {
      temp_write.data = (unsigned short)((arg2HOME)<<8);
      if(dynamic_log_ebable)
      printk(KERN_ERR "[capkey]FA Read Home=0x%x\n",temp_write.data);
    }
    if(!cap_i2c_write(kbd, &temp_write, 1))
      printk(KERN_ERR "write  failed. temp_write.data=0x%x\n",temp_write.data);

    temp_write.addr = 0x12;
    if ( arg2BACK>0x9F || arg2BACK<0x90)
    {
      temp_write.data = 0x009F;
      printk(KERN_ERR "[capkey]FA BACK out of range=0x%x, default assign 0x009F\n",arg2BACK);
    }
    else
    {
      temp_write.data = (unsigned short)arg2BACK;
      if(dynamic_log_ebable)
      printk(KERN_ERR "[capkey]FA Read Back=0x%x\n",temp_write.data);
    }
    if(!cap_i2c_write(kbd, &temp_write, 1))
      printk(KERN_ERR "write  failed. temp_write.data=0x%x\n",temp_write.data);
  }

  temp_write.addr = 0x14;
  temp_write.data = 0x0;
  if(!cap_i2c_write(kbd, &temp_write, 1))
    printk(KERN_ERR "write  failed. temp_write.data=0x%x\n",temp_write.data);

  temp_write.addr = 0x15;
  temp_write.data = 0x0;
  if(!cap_i2c_write(kbd, &temp_write, 1))
    printk(KERN_ERR "write  failed. temp_write.data=0x%x\n",temp_write.data);

  qi2ccapkybd_update_led(cap_button_light);

  temp_write.addr = 0x2B;
  temp_write.data = 0x0;
  if(!cap_i2c_write(kbd, &temp_write, 1))
    printk(KERN_ERR "write  failed. temp_write.data=0x%x\n",temp_write.data);

  temp_write.addr = 0x0;
  temp_write.data = 0x0007;
  if(!cap_i2c_write(kbd, &temp_write, 1))
    printk(KERN_ERR "write failed. temp_write.data=0x%x\n",temp_write.data);

  temp_rd.addr = QKBD_CAP_STRIP_STATUS_POSITION;
  temp_rd.data = 0;

  cap_i2c_read(kbd, &temp_rd, 1);
}

static int qi2ccapkybd_opencb(struct input_dev *dev)
{
  int rc=0;

  printk(KERN_ERR "qi2ccapkybd_eventcb()\n");
  return rc;
}

static void qi2ccapkybd_closecb(struct input_dev *idev)
{
  struct i2ccapkybd_record *kbdrec = input_get_drvdata(idev);
  struct device *dev = &kbdrec->mykeyboard->dev;

  dev_dbg(dev, "ENTRY: close callback\n");
  qi2ccapkybd_shutdown(kbdrec);
}


static int qi2ccapkybd_remove(struct i2c_client *kbd)
{
  int rc = 0;
  struct i2ccapkybd_record *rd = i2c_get_clientdata(kbd);
  dev_info(&kbd->dev, "removing keyboard driver\n");

  {
    int i;
    for(i=0; i<ARRAY_SIZE(qsd_led_button_led_attrs); i++)
      device_remove_file(qsd_led_button_backlight.dev, &qsd_led_button_led_attrs[i]);
    led_classdev_unregister(&qsd_led_button_backlight);
  }

  if (rd->i2ckbd_idev) {
    dev_dbg(&kbd->dev, "deregister from input system\n");
    input_unregister_device(rd->i2ckbd_idev);
    rd->i2ckbd_idev = 0;
  }
  qi2ccapkybd_shutdown(rd);
  qi2ccapkybd_release_gpio(rd->gpio_num);
  return rc;
}


static int capkey_register_input( struct input_dev **input,
                              struct i2c_client *client )
{
  int rc;
  struct input_dev *input_dev;
  struct i2ccapkybd_record *kbdrec = &kbd_data;

  input_dev = input_allocate_device();
  if ( !input_dev ) {
    rc = -ENOMEM;
    return rc;
  }

  snprintf(kbdrec->physinfo, QKBD_PHYSLEN,
         "/event0");

  input_dev->name = i2ccapkybd_name;
  input_dev->phys = kbdrec->physinfo;
  input_dev->id.bustype = BUS_I2C;
  input_dev->id.vendor = 0x0001;
  input_dev->id.product = 0x0002;
  input_dev->id.version = 0x0100;
  input_dev->dev.parent = &client->dev;

  input_dev->open = qi2ccapkybd_opencb;
  input_dev->close = qi2ccapkybd_closecb;
  input_dev->event = qi2ccapkybd_eventcb;

  input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);

  set_bit(KEY_MENU, input_dev->keybit);
  set_bit(KEY_HOME, input_dev->keybit);
  set_bit(KEY_BACK, input_dev->keybit);

  kbd_data.keystate = QKBD_IN_KEYRELEASE;
  kbd_data.lastkeycode = 0;

  input_set_drvdata(input_dev, kbdrec);

  rc = input_register_device( input_dev );
  if ( rc )
  {
    printk(KERN_ERR "failed to register input device\n");
    input_free_device( input_dev );
  }else {
    *input = input_dev;
  }

  return rc;
}


static const struct i2c_device_id qi2ccapkybd_id[] = {
  { "synaptics_capsensor", 0 },
  { }
};
static struct i2c_driver i2ccapkbd_driver = {
  .driver = {
    .owner = THIS_MODULE,
  },
  .probe   = qi2ccapkybd_probe,
  .remove  = qi2ccapkybd_remove,
  .id_table = qi2ccapkybd_id,
};


static int capsensor_param_set(const char *val, struct kernel_param *kp)
{
  int ret;
  struct i2c_client *kbd = kbd_data.mykeyboard;
  struct   capsensor_reg_t temp_write;

  DBGPRINTK(KERN_ERR  "%s +\n", __func__);

  ret = param_set_int(val, kp);

  if(capsensor_param==0)
  {
    printk(KERN_ERR "[capkey] sleep=1, GPI=1\n");
    if (!atomic_read(&capsensor_is_in_suspend))
    {
      atomic_set(&capsensor_is_in_suspend,1);
      if(atomic_read(&qi2ccapkybd_inited))
      {
        temp_write.addr = 0x1;
        temp_write.data = 0x00F0;
        if(!cap_i2c_write(kbd, &temp_write, 1))
          printk(KERN_ERR "write  failed. temp_write.data=0x%x\n",temp_write.data);
      }
    }
  }
  else if(capsensor_param==1)
  {
    printk(KERN_ERR "[capkey] sleep=0, GPI=1\n");
    if (atomic_read(&capsensor_is_in_suspend))
    {
      atomic_set(&capsensor_is_in_suspend,0);
      if(atomic_read(&qi2ccapkybd_inited))
      {
        temp_write.addr = 0x1;
        temp_write.data = 0x0030;
        if(!cap_i2c_write(kbd, &temp_write, 1))
          printk(KERN_ERR "write  failed. temp_write.data=0x%x\n",temp_write.data);
      }
    }
  }
  else if(capsensor_param==2)
  {
    printk(KERN_ERR "[capkey] sleep=1, GPI=0\n");
    if (!atomic_read(&capsensor_is_in_suspend))
    {
      atomic_set(&capsensor_is_in_suspend,1);
      if(atomic_read(&qi2ccapkybd_inited))
      {
        temp_write.addr = 0x1;
        temp_write.data = 0x0080;
        if(!cap_i2c_write(kbd, &temp_write, 1))
          printk(KERN_ERR "write  failed. temp_write.data=0x%x\n",temp_write.data);
      }
    }
  }

  else if(capsensor_param==90)
  {
    printk(KERN_ERR "### turn off Dynamic Debug log ###\n");
    dynamic_log_ebable = 0;
  }
  else if(capsensor_param==91)
  {
    printk(KERN_ERR "### turn on Dynamic Debug log ###\n");
    dynamic_log_ebable = 1;
  }
  else if(capsensor_param==99)
  {
    printk(KERN_ERR "### param_set() call reset...... ###\n");
    mutex_lock(&qi2ccapkybd_lock);
    qi2ccapkybd_reset();
    mutex_unlock(&qi2ccapkybd_lock);
    printk(KERN_ERR "### param_set() rest Cap-Sensor Done ###\n");
  }
  else if( (0x1000 <= capsensor_param) && (capsensor_param < 0x1100))
  {
    printk(KERN_ERR "capsensor_param=0x%x, capkey sensitivity value=0x%x\n", capsensor_param, (capsensor_param -0x1000));

    temp_write.addr = 0x11;
    temp_write.data = capsensor_param -0x1000;
    if(!cap_i2c_write(kbd, &temp_write, 1))
      printk(KERN_ERR "write  failed. temp_write.data=0x%x\n",temp_write.data);

    temp_write.addr = 0x12;
    temp_write.data = (capsensor_param -0x1000) << 8;
    if(!cap_i2c_write(kbd, &temp_write, 1))
      printk(KERN_ERR "write  failed. temp_write.data=0x%x\n",temp_write.data);

    temp_write.addr = 0x13;
    temp_write.data = capsensor_param -0x1000;
    if(!cap_i2c_write(kbd, &temp_write, 1))
      printk(KERN_ERR "write  failed. temp_write.data=0x%x\n",temp_write.data);
  }
  else
  {
    printk(KERN_ERR "unknow command=0x%x\n", capsensor_param);
  }

  DBGPRINTK(KERN_ERR  "%s -\n", __func__);
  return ret;
}

module_param_call(capsensor, capsensor_param_set, param_get_long,
      &capsensor_param, S_IWUSR | S_IRUGO);


static int qi2ccapkybd_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
  int                              rc = 0;
  struct i2ccapkybd_record           *rd = &kbd_data;

  if (rd->mykeyboard) {
    dev_err(&client->dev,
      "only a single i2c keyboard is supported\n");
    return -ENODEV;
  }

  mutex_init(&qi2ccapkybd_lock);

  client->driver = &i2ccapkbd_driver;
  i2c_set_clientdata(client, &kbd_data);
  rd->mykeyboard    = client;
  rd->gpio_num       = QKBD_CAPSENSOR_GPIO;
  rd->irq    = MSM_GPIO_TO_INT( rd->gpio_num );

  rc = qi2ccapkybd_config_gpio(rd->gpio_num);
  if (!rc) {
    rc = capkey_register_input( &rd->i2ckbd_idev, client );
    if (rc)
      qi2ccapkybd_release_gpio(rd->gpio_num);

  INIT_WORK(&qkybd_irqwork, qi2ccapkybd_fetchkeys);
  qi2ccapkybd_irqsetup(QKBD_CAPSENSOR_GPIO);
  qi2ccapkybd_capsensor_init(rd->mykeyboard);

    #ifdef CONFIG_HAS_EARLYSUSPEND
      rd ->capsensor_early_suspend.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING - 1;
      rd ->capsensor_early_suspend.suspend = capsensor_early_suspend;
      rd ->capsensor_early_suspend.resume = capsensor_early_resume;
      register_early_suspend(&rd ->capsensor_early_suspend);

      #if CAPKEY_ESD_SOLUTION
      rd ->capsensor_very_early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB +1;
      rd ->capsensor_very_early_suspend.suspend = capsensor_very_early_suspend;
      rd ->capsensor_very_early_suspend.resume = capsensor_very_early_resume;
      register_early_suspend(&rd ->capsensor_very_early_suspend);
      #endif
    #endif
  }

  #if 1
  {
    int ret, i;

    ret = led_classdev_register(&client->dev, &qsd_led_button_backlight);
    if(ret < 0)
    {
      printk(KERN_ERR "qsd_led_button_backlight: create FAIL, ret=%d\n", ret);
    }
    else
    {
      for(i=0; i<ARRAY_SIZE(qsd_led_button_led_attrs); i++)
      {
        ret = device_create_file(qsd_led_button_backlight.dev, &qsd_led_button_led_attrs[i]);
        if(ret) printk(KERN_ERR "%s: create FAIL, ret=%d",qsd_led_button_led_attrs[i].attr.name,ret);
      }
    }
  }
  #endif

  atomic_set(&qi2ccapkybd_inited,1);
  qi2ccapkybd_update_led(cap_button_light);

  return rc;
}

static int __init qi2ccapkybd_init(void)
{
  bool testflag=false;
  printk(KERN_ERR "+qi2ccapkybd_init\n");
  memset(&kbd_data, 0, sizeof(struct i2ccapkybd_record));
  i2ccapkbd_driver.driver.name = i2ccapkybd_name;

  testflag = i2c_add_driver(&i2ccapkbd_driver);
  printk(KERN_ERR "-qi2ccapkybd_init\n");
  return testflag;
}


static void __exit qi2ccapkybd_exit(void)
{
  i2c_del_driver(&i2ccapkbd_driver);
}

module_init(qi2ccapkybd_init);
module_exit(qi2ccapkybd_exit);

