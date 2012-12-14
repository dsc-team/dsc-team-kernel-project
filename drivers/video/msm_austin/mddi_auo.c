/* drivers/video/msm/src/panel/mddi/mddi_auo.c
 *
 * Copyright (c) 2008 QUALCOMM USA, INC.
 *
 * All source code in this file is licensed under the following license
 * except where indicated.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can find it at http://www.fsf.org
 */

#include "msm_fb.h"
#include "mddihost.h"
#include "mddihosti.h"
#include <mach/gpio.h>
#include <mach/vreg.h>
#include "hdmi.h"
#include <mach/pm_log.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>

#define RTC_WORKAROUD_BY_DISPLAYDRIVER	1

#define LONGCHARGER48HR_WAKEUP_HANG_ATLCD_WORKAROUND	1
#if	LONGCHARGER48HR_WAKEUP_HANG_ATLCD_WORKAROUND
#include <linux/wakelock.h>
static struct wake_lock lcd_suspendlock;
static struct wake_lock lcd_idlelock;
#endif

#include <linux/leds.h>
#include <mach/lsensor.h>
#include <mach/bkl_cmd.h>


#define MSG(format, arg...)

#define MSG2(format, arg...) printk(KERN_INFO "[MDDI]" format "\n", ## arg)



#include <mach/austin_hwid.h>
#define AUO_WVGA_PRIM 0

#define DEEP_SLEEP_MODE 1
#define	LCD_PWR_OFF	1
#define MDDI_FB_PORTRAIT 1

//#undef MDDI_FB_PORTRAIT

#define ESD_SOLUTION 1


#define	BACKLIGHT_EARLY_SUSPEND

static int t2_register=0x1BC;
//static int vsync=1;
module_param(t2_register,int,0644);
//module_param(vsync,int,0644);

static int auo_lcd_on(struct platform_device *pdev);
static int auo_lcd_off(struct platform_device *pdev);
static void mddi_auo_prim_lcd_init(void);
void mddi_lcd_reset(void);
void mddi_lcd_disp_powerup(void);
u32 panel_id = 0;
extern int mddi_is_in_suspend;

static bool mddipanel_inited = 0;
static bool mddipanel_registed = 0;

#if 0
static struct workqueue_struct *qsd_mddi_wq;
static struct delayed_work qsd_mddi_delayed_work;
static int mddi_test = 0;
#endif

#if ESD_SOLUTION
static struct workqueue_struct *qsd_mddi_esd_wq;
static struct delayed_work g_mddi_esd_work;
static unsigned int esdval=0;
#endif
static int mddi_param=0;
static unsigned int manual_esdval=0;

static bool dynamic_log_ebable = 0;



static struct led_classdev auo_lcd_bkl;
static char auo_lcd_bkl_mode = LCD_BKL_MODE_NORMAL;
static atomic_t auo_lcd_enable = ATOMIC_INIT(0);
static int  bkl_labc_stage = 0xFFFFFFFF;
module_param(bkl_labc_stage,int,0644);

static unsigned int als_enabled=0;
static unsigned int als_lux_value=500;
static unsigned int reg5A00=0, reg5B00=0, regTemp=0;

static const unsigned int auo_lcd_bkl_init_array[] = {  
  
  0x6A01008A,0x6A02000C,
  
  0x6A12000E,
  
  0x53000024,0x53010004,0x53020001,0x53050005,
  
  0x5100001E,

  0x6A1600ff,
  0x6A150081,
  0x5E030014,
  0x5E040000,

  0x55000000,

  0x50000028,0x5001004D,0x500200BF,0x500300D5,0x500400FF,

  0xFFFFFFFF
  };

static const unsigned int bkl_labc_array[6][21] =  
{
#if (0)
  { 0x57000000,0x5701001E,0x57040000,0x57050090,
  0x57080000,0x570900E0,0x570C0001,0x570D0070,
  0x57100003,0x571100FF,0x57020000,0x57030010,
  0x57060000,0x57070083,0x570A0000,0x570B00D6,
  0x570E0001,0x570F0030,0x57120003,0x571300FF,
  0xFFFFFFFF  },

  { 0x57000000,0x5701001E,0x57040000,0x5705009A,
  0x57080000,0x570900FC,0x570C0001,0x570D009A,
  0x57100003,0x571100FF,0x57020000,0x57030010,
  0x57060000,0x5707008A,0x570A0000,0x570B00E3,
  0x570E0001,0x570F0055,0x57120003,0x571300FF,
  0xFFFFFFFF  },

  { 0x57000000,0x5701001E,0x57040000,0x570500AB,
  0x57080001,0x57090020,0x570C0001,0x570D00D5,
  0x57100003,0x571100FF,0x57020000,0x57030010,
  0x57060000,0x57070095,0x570A0000,0x570B00F8,
  0x570E0001,0x570F0080,0x57120003,0x571300FF,
  0xFFFFFFFF  },

  { 0x57000000,0x5701001E,0x57040000,0x570500BC,
  0x57080001,0x57090042,0x570C0002,0x570D000A,
  0x57100003,0x571100FF,0x57020000,0x57030045,
  0x57060000,0x570700A0,0x570A0001,0x570B0020,
  0x570E0001,0x570F0092,0x57120003,0x571300FF,
  0xFFFFFFFF  },

  { 0x57000000,0x5701001E,0x57040000,0x570500D2,
  0x57080001,0x57090058,0x570C0002,0x570D0045,
  0x57100003,0x571100FF,0x57020000,0x57030010,
  0x57060000,0x570700A9,0x570A0001,0x570B002C,
  0x570E0001,0x570F00C3,0x57120003,0x571300FF,
  0xFFFFFFFF  },

  { 0x57000000,0x5701001E,0x57040000,0x570500E2,
  0x57080001,0x57090075,0x570C0002,0x570D004A,
  0x57100003,0x571100FF,0x57020000,0x57030010,
  0x57060000,0x570700B6,0x570A0001,0x570B004B,
  0x570E0001,0x570F00E6,0x57120003,0x571300FF,
  0xFFFFFFFF  },
};

#else

  { 0x57000000,0x57010034,0x57040000,0x57050090,
  0x57080000,0x570900E0,0x570C0001,0x570D0070,
  0x57100003,0x571100FF,0x57020000,0x5703002A,
  0x57060000,0x57070083,0x570A0000,0x570B00D6,
  0x570E0001,0x570F0030,0x57120003,0x571300FF,
  0xFFFFFFFF  },

  { 0x57000000,0x5701004A,0x57040000,0x5705009A,
  0x57080000,0x570900FC,0x570C0001,0x570D009A,
  0x57100003,0x571100FF,0x57020000,0x5703003A,
  0x57060000,0x5707008A,0x570A0000,0x570B00E3,
  0x570E0001,0x570F0055,0x57120003,0x571300FF,
  0xFFFFFFFF  },

  { 0x57000000,0x57010060,0x57040000,0x570500AB,
  0x57080001,0x57090020,0x570C0001,0x570D00D5,
  0x57100003,0x571100FF,0x57020000,0x57030042,
  0x57060000,0x57070095,0x570A0000,0x570B00F8,
  0x570E0001,0x570F0080,0x57120003,0x571300FF,
  0xFFFFFFFF  },
  
  { 0x57000000,0x57010060,0x57040000,0x570500BC,
  0x57080001,0x57090042,0x570C0002,0x570D000A,
  0x57100003,0x571100FF,0x57020000,0x57030045,
  0x57060000,0x570700A0,0x570A0001,0x570B0020,
  0x570E0001,0x570F0092,0x57120003,0x571300FF,
  0xFFFFFFFF  },
  
  { 0x57000000,0x5701006A,0x57040000,0x570500D2,
  0x57080001,0x57090058,0x570C0002,0x570D0045,
  0x57100003,0x571100FF,0x57020000,0x57030045,
  0x57060000,0x570700A9,0x570A0001,0x570B002C,
  0x570E0001,0x570F00C3,0x57120003,0x571300FF,
  0xFFFFFFFF  },
  
  { 0x57000000,0x5701006A,0x57040000,0x570500E2,
  0x57080001,0x57090075,0x570C0002,0x570D004A,
  0x57100003,0x571100FF,0x57020000,0x57030045,
  0x57060000,0x570700B6,0x570A0001,0x570B004B,
  0x570E0001,0x570F00E6,0x57120003,0x571300FF,
  0xFFFFFFFF  }
};
#endif

static unsigned int my_atoh(unsigned char *in, unsigned int len)
{
	unsigned int sum=0, i;
	unsigned char c, h;
  for(i=0; i<len; i++)
	{
		c = *in++;
		if ((c >= '0') && (c <= '9'))       h = c - '0';
		else if ((c >= 'A') && (c <= 'F'))  h = c - 'A' + 10;
		else if ((c >= 'a') && (c <= 'f'))  h = c - 'a' + 10;
    else                                h = 0;
    sum = (sum << 4) + h;
	}
	return sum;
}
static void write_multi_mddi_reg(const unsigned int *buf)
{
  unsigned int i, reg, val;
  for(i=0; i<255; i++)
  {
    if(buf[i] == 0xFFFFFFFF)
      break;
    reg = (buf[i] >> 16) & 0xFFFF;
    val = buf[i] & 0xFFFF;
    mddi_queue_register_write(reg, val, FALSE, 0);
  }
}
static void auo_lcd_bkl_onOff(unsigned int onOff)
{
  static unsigned int inited = 0;

  MSG("%s = %d", __func__,onOff);
  if(!inited)
  {
    gpio_tlmm_config(GPIO_CFG(57, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
    inited = 1;
    MSG2("%s, inited", __func__);
  }
  if(onOff)
  {
    gpio_set_value(57,1); 
    
    PM_LOG_EVENT(PM_LOG_ON, PM_LOG_BL_LCD);
  }
  else
  {
    
    gpio_set_value(57,0); 
    PM_LOG_EVENT(PM_LOG_OFF, PM_LOG_BL_LCD);
  }
}




static void auo_lcd_bkl_set(struct led_classdev *led_cdev, enum led_brightness value)
{
	if(mddi_is_in_suspend)
	{
		MSG("%s MDDI in suspend mode.. brightness = %d",__func__,(unsigned int)value);
		return;
	}
	if(!atomic_read(&auo_lcd_enable))
	{
		MSG("%s LCD in disabled mode.. brightness = %d",__func__,(unsigned int)value);
		return;
	}
	
	
	mddi_queue_register_write(0x5100, value, FALSE, 0);
	MSG("%s = %d", __func__,value);
}



static ssize_t auo_lcd_bkl_ctrl_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
  static unsigned int reg, val;
  static unsigned int reg5300, reg5500;
  static unsigned int reg5400, reg5600;
  static int mode, mode2 = 0;
  int ret1, ret2, ret3;
  char bufLocal[64];
  int ret;

	if(mddi_is_in_suspend)
	{
		MSG("%s MDDI in suspend mode..",__func__);
		goto exit;
	}

  printk(KERN_INFO "\n");
  if(count > sizeof(bufLocal))
  {
    MSG("%s input invalid, count = %d", __func__, count);
    goto exit;
  }

  memcpy(bufLocal,buf,count);
  bufLocal[63] = '\0';

  switch(bufLocal[0])
  {
    case 'r':
      if(count<5) break;
      reg = my_atoh(&bufLocal[1], 4);
      ret = mddi_queue_register_read(reg, &val, TRUE, 0);
      printk(KERN_INFO "\n");
      MSG2("Read  0x%04X = 0x%04X (ret=%d)", reg,val,ret);
      break;
    case 'w':
      if(count<9) break;
      reg = my_atoh(&bufLocal[1], 4);
      val = my_atoh(&bufLocal[5], 4);
      ret = mddi_queue_register_write(reg, val, FALSE, 0);
      printk(KERN_INFO "\n");
      MSG2("Write 0x%04X = 0x%04X (ret=%d)", reg,val,ret);
      break;
    case 'a': 
      {
        mode = 0xFF;
        reg5400 = 0xFFFF; reg5600 = 0xFFFF;
        
        ret = mddi_queue_register_read(0x5400,&reg5400, TRUE, 0);
        if(ret < 0)   MSG2("reg5400 read fail = %d", ret);
        ret = mddi_queue_register_read(0x5600,&reg5600, TRUE, 0);
        if(ret < 0)   MSG2("reg5600 read fail = %d", ret);
         if     (reg5400 == 0x24)  mode = 1;
        else if(reg5400 == 0x34)  mode = 2;
        else if(reg5400 == 0x2c)  
        {
          if     (reg5600 == 0x00)mode = 3;
          else if(reg5600 == 0x01)mode = 4;
          else if(reg5600 == 0x02)mode = 5;
          else if(reg5600 == 0x03)mode = 6;
        }
        else if(reg5400 == 0x3c)  
        {
          if     (reg5600 == 0x00)mode = 0;
          else if(reg5600 == 0x01)mode = 7;
          else if(reg5600 == 0x02)mode = 8;
          else if(reg5600 == 0x03)mode = 9;
        }
        MSG2("(5400=%02X 5600=%02X) Backlight Mode = %s",reg5400,reg5600,
          mode==1? "NORMAL":
          mode==2? "LABC":
          mode==4? "CABC_UI":
          mode==5? "CABC_STILL":
          mode==6? "CABC_MOVING":
          mode==7? "LABC_CABC_UI":
          mode==8? "LABC_CABC_STILL":
          mode==9? "LABC_CABC_MOVING":"INVALID"
          );
      }
      break;
    case 'b': 
      {
        reg5400 = 0xFFFF; reg5600 = 0xFFFF;
        if(mode2 == 0)       
        {
          mode2 = 1; reg5300 = 0x0024; reg5500 = 0x0000;
        }
        else if(mode2 == 1)  
        {
          mode2 = 2; reg5300 = 0x003C; reg5500 = 0x0003;
        }
        else                
        {
          mode2 = 0; reg5300 = 0x002C; reg5500 = 0x0002;
        }
        ret1 = mddi_queue_register_write(0x5300, reg5300, TRUE, 0);
        ret2 = mddi_queue_register_write(0x5500, reg5500, TRUE, 0);
        msleep(50);
        ret3 = mddi_queue_register_read(0x5400, &reg5400, TRUE, 0);
        printk("\n[MDDI]5300=%02X, 5500=%02X, 5400=%02X %c%c%c%c %s\n\n", reg5300, reg5500, reg5400,
          !ret1?'P':'F', !ret2?'P':'F', !ret3?'P':'F', (reg5300==reg5400)?'P':'F',
          mode2==1?"NORMAL": mode2==2?"LABC_CABC_MOVING":"CABC_STILL");
      }
      break;
    case 'c': 
      {
        reg5400 = 0xFFFF;
        ret3 = mddi_queue_register_read(0x5400, &reg5400, TRUE, 0);
        printk("\n[MDDI]5400=%02X %c\n\n", reg5400, !ret3?'P':'F');
      }
      break;
    case 'f': 
      {
        reg5400 = 0xFFFF;
        ret3 = mddi_queue_register_read(0x0A00, &reg5400, TRUE, 0);
        printk("\n[MDDI]0A00=%02X %c\n\n", reg5400, !ret3?'P':'F');
      }
      break;
    case 'd': 
      MSG2("Reg5300 %02X; Reg5500 %02X", reg5300,reg5500);
      MSG2("Reg5400 %02X; Reg5600 %02X", reg5400,reg5600);
      break;
    case 'e': 
      {
        reg5400 = 0xFFFF; reg5600 = 0xFFFF;
        if(mode2 == 0)       
        {
          mode2 = 1; reg5300 = 0x0024; reg5500 = 0x0000;  MSG2("Backlight Mode = NORMAL");
        }
        else if(mode2 == 1)  
        {
          mode2 = 2; reg5300 = 0x003C; reg5500 = 0x0003;  MSG2("Backlight Mode = LABC_CABC_MOVING");
        }
        else                
        {
          mode2 = 0; reg5300 = 0x002C; reg5500 = 0x0002;  MSG2("Backlight Mode = CABC_STILL");
        }
        ret1 = mddi_queue_register_write(0x5300, reg5300, TRUE, 0);
        ret2 = mddi_queue_register_write(0x5500, reg5500, TRUE, 0);
        printk("\n[MDDI]5300=%02X, 5500=%02X %c%c\n\n", reg5300, reg5500,
          !ret1?'P':'F', !ret2?'P':'F');
      }
      break;

    #if 0
    case 'm': 
      if(count<2) break;
      if(bufLocal[1] == '0')
      {
        MSG2("mddi test mode = OFF");
        mddi_test = 0;
      	cancel_delayed_work(&qsd_mddi_delayed_work);
      	flush_workqueue(qsd_mddi_wq);
      }
      else if(bufLocal[1] == '1')
      {
        MSG2("mddi test mode = ON");
        mddi_test = 1;
        queue_delayed_work(qsd_mddi_wq,&qsd_mddi_delayed_work,HZ);
      }
      break;
    #endif


  }

exit:
	return count;
}
#include <linux/miscdevice.h>
static DEFINE_MUTEX(bkl_mode_mutex);
static void auo_lcd_bkl_mode_switch(int mode, int repeat)
{
  unsigned int reg5300, reg5500, i, light_on;

  mutex_lock(&bkl_mode_mutex);
  MSG("%s+",__func__);

  if(repeat != 0xFF)  
  {
    MSG2("Backlight Mode = %s (ALS:%s)",
      mode==LCD_BKL_MODE_NORMAL?          "NORMAL":
      mode==LCD_BKL_MODE_LABC?            "LABC":
      mode==LCD_BKL_MODE_CABC_UI?         "CABC_UI":
      mode==LCD_BKL_MODE_CABC_STILL?      "CABC_STILL":
      mode==LCD_BKL_MODE_CABC_MOVING?     "CABC_MOVING":
      mode==LCD_BKL_MODE_LABC_CABC_UI?    "LABC_CABC_UI":
      mode==LCD_BKL_MODE_LABC_CABC_STILL? "LABC_CABC_STILL":
      mode==LCD_BKL_MODE_LABC_CABC_MOVING?"LABC_CABC_MOVING":"INVALID",
      als_enabled? "ON":"OFF"
      );
  }
  
  if(mode >= LCD_BKL_MODE_MAX)
    mode = LCD_BKL_MODE_NORMAL; 
  auo_lcd_bkl_mode = mode;      

  if(mode == LCD_BKL_MODE_NORMAL)
    { reg5300 = 0x0024; reg5500 = 0x0000; light_on = 0; }
  else if(mode == LCD_BKL_MODE_LABC)
    { reg5300 = 0x0034; reg5500 = 0x0000; light_on = 1; }
  else if(mode == LCD_BKL_MODE_CABC_UI)
    { reg5300 = 0x002C; reg5500 = 0x0001; light_on = 0; }
  else if(mode == LCD_BKL_MODE_CABC_STILL)
    { reg5300 = 0x002C; reg5500 = 0x0002; light_on = 0; }
  else if(mode == LCD_BKL_MODE_CABC_MOVING)
    { reg5300 = 0x002C; reg5500 = 0x0003; light_on = 0; }
  else if(mode == LCD_BKL_MODE_LABC_CABC_UI)
    { reg5300 = 0x003C; reg5500 = 0x0001; light_on = 1; }
  else if(mode == LCD_BKL_MODE_LABC_CABC_STILL)
    { reg5300 = 0x003C; reg5500 = 0x0002; light_on = 1; }
  else 
    { reg5300 = 0x003C; reg5500 = 0x0003; light_on = 1; }

  if(repeat == 0xFF)
  {
    if(light_on)
    {
      qsd_lsensor_enable_onOff(LSENSOR_ENABLE_LCD,1);
      mddi_queue_register_write(0x5E03, 0x15, FALSE, 0);
    }
    else
    {
      if(als_enabled)
        mddi_queue_register_write(0x5E03, 0x15, FALSE, 0);
      else
        mddi_queue_register_write(0x5E03, 0x14, FALSE, 0);

      mddi_queue_register_write(0x5100, auo_lcd_bkl.brightness, FALSE, 0);
    }
    if(mode == LCD_BKL_MODE_NORMAL)
      mddi_queue_register_write(0x5305, 0x04, FALSE, 0);  
    mddi_queue_register_write(0x5500, reg5500, FALSE, 0); 
    mddi_queue_register_write(0x5300, reg5300, FALSE, 0); 
    goto exit_normal;
  }


  if(!atomic_read(&auo_lcd_enable))   goto exit_cancel;

  if(light_on)
  {
    qsd_lsensor_enable_onOff(LSENSOR_ENABLE_LCD,1);     
    mddi_queue_register_write(0x5E03, 0x15, FALSE, 0);  
    mddi_queue_register_write(0x5305, 0x05, FALSE, 0);  
  }
  else
  {
    mddi_queue_register_write(0x5100, auo_lcd_bkl.brightness, FALSE, 0);
    if(als_enabled)
      mddi_queue_register_write(0x5E03, 0x15, FALSE, 0);
    else
      mddi_queue_register_write(0x5E03, 0x14, FALSE, 0);
    mddi_queue_register_write(0x5305, 0x04, FALSE, 0);
  }

  for(i=0; i<repeat; i++)
  {
    if(!atomic_read(&auo_lcd_enable))   goto exit_cancel;
    mddi_queue_register_write(0x5500, reg5500, FALSE, 0);


    if(!atomic_read(&auo_lcd_enable))   goto exit_cancel;
    mddi_queue_register_write(0x5300, reg5300, FALSE, 0);
  }

  if(!atomic_read(&auo_lcd_enable))   goto exit_cancel;
  if(light_on)
  {
    mddi_queue_register_write(0x5E03, 0x15, FALSE, 0);
    mddi_queue_register_write(0x5305, 0x05, FALSE, 0);
  }
  else
  {
    mddi_queue_register_write(0x5100, auo_lcd_bkl.brightness, FALSE, 0);
    if(als_enabled)
      mddi_queue_register_write(0x5E03, 0x15, FALSE, 0);
    else
      mddi_queue_register_write(0x5E03, 0x14, FALSE, 0);
    qsd_lsensor_enable_onOff(LSENSOR_ENABLE_LCD,0);
    mddi_queue_register_write(0x5305, 0x04, FALSE, 0);  
  }

exit_normal:
  MSG("%s-",__func__);
  mutex_unlock(&bkl_mode_mutex);
  return;

exit_cancel:
  MSG("%s-, Cancelled!",__func__);
  mutex_unlock(&bkl_mode_mutex);
  return;
}
static ssize_t auo_auo_lcd_bkl_mode_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
  int mode;

  MSG("%s: mode = %c, count = %d", __func__,buf[0],count);
  if(count < 1)
  {
    MSG("%s input invalid, count = %d", __func__, count);
    goto exit;
  }
  switch(buf[0])
  {
    case 'm':
      if(count<2) break;
      mode = buf[1] - '0';
      auo_lcd_bkl_mode_switch(mode, 3);
      break;

    case 'b': 
      if(buf[1] == '0')
        auo_lcd_bkl_onOff(0);
      else if(buf[1] == '1')
        auo_lcd_bkl_onOff(1);
      break;
  }

exit:
	return count;
}




static ssize_t bkl_eng_mode2_show(struct device *dev, struct device_attribute *attr, char *buf)
{
  unsigned char bufLocal[64];
	ssize_t size;

  
  size = bkl_eng_mode2_read(bufLocal, sizeof(bufLocal));
  memcpy(buf,bufLocal,size);
  
	return size;
}
static ssize_t bkl_eng_mode2_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
  unsigned char bufLocal[128];
  ssize_t size;
  static unsigned char inited = 0;

  
  if(!inited) 
  {
    qsd_lsensor_enable_onOff(LSENSOR_ENABLE_LCD_ENG,1);
    inited = 1;
  }
  if(count > sizeof(bufLocal))
  {
    MSG("%s: invalid count = %d",__func__,count);
    return count;
  }
  memcpy(bufLocal,buf,count);
  size = bkl_eng_mode2_write(bufLocal, count);
  
	return size;
}

static struct device_attribute auo_lcd_bkl_ctrl_attrs[] = {
  __ATTR(ctrl,  0664, NULL, auo_lcd_bkl_ctrl_store),
  __ATTR(mode,  0664, NULL, auo_auo_lcd_bkl_mode_store),
  __ATTR(ctrl2, 0664, bkl_eng_mode2_show, bkl_eng_mode2_store),
};
static struct led_classdev auo_lcd_bkl = {
  .name     = "lcd-backlight",
  .brightness = LED_HALF,
  .brightness_set   = auo_lcd_bkl_set,
};

static const unsigned short als_adc_array[6][8] =
{
  {0x2a, 0x34, 0x83, 0x90, 0xd6,  0xe0,  0x130, 0x170},
  {0x3a, 0x4a, 0x8a, 0x9a, 0xe3,  0xfc,  0x155, 0x19a},
  {0x42, 0x60, 0x95, 0xab, 0xf8,  0x120, 0x180, 0x1d5},
  {0x45, 0x60, 0xa0, 0xbc, 0x120, 0x142, 0x192, 0x20a},
  {0x45, 0x6a, 0xa9, 0xd2, 0x12c, 0x158, 0x1c3, 0x245},
  {0x45, 0x6a, 0xb6, 0xe2, 0x14b, 0x175, 0x1e6, 0x24a} 
};
unsigned int auo_lcd_als_mapping_to_lux(unsigned int als)
{
  static const unsigned short lux[8] = {100, 150, 220, 300, 370, 450, 550, 700};
  int s = bkl_labc_stage;
  int i, aa=0, bb=0, cc=0, dd=0;

  for(i=0; i<8; i++)
  {
    if(als_adc_array[s][i] > als)
    {
      if(i)
      {
        aa = lux[i] - lux[i-1];
        bb = als - als_adc_array[s][i-1];
        cc = als_adc_array[s][i] - als_adc_array[s][i-1];
        dd = lux[i-1];
      }
      else
      {
        aa = lux[i];
        bb = als;
        cc = als_adc_array[s][i];
        dd = 0;
      }
      break;
    }
  }
  if(i==8)
  {
// Jagan+	
    aa = lux[i-1] - lux[i-2];
    bb = als - als_adc_array[s][i-1];
    cc = als_adc_array[s][i-1] - als_adc_array[s][i-2];
// Jagan-	
    dd = lux[i-1];
  }
  return ((aa * bb / cc) + dd);
}
void auo_lcd_als_update_value(void)
{
  int i, als;
  if (atomic_read(&auo_lcd_enable))
  {
    mddi_queue_register_read(0x5A00, &regTemp, TRUE, 0);
    for(i=0; i<5; i++)
    {
      mddi_queue_register_read(0x5B00, &reg5B00, TRUE, 0);
      mddi_queue_register_read(0x5A00, &reg5A00, TRUE, 0);
      if(reg5A00 == regTemp)
        break;
      else
        regTemp = reg5A00;
    }
    als = (reg5A00 << 8) + reg5B00;
    als_lux_value = auo_lcd_als_mapping_to_lux(als);
    MSG("ALS = %d (%d)\n", als_lux_value, als);
  }
  else
  {
    printk(KERN_ERR "ALS return value\n");
  }
}
void auo_lcd_als_enable(unsigned int onOff)
{
  if(onOff)
  {
    MSG2("%s ON", __func__);
    als_enabled = 1;
    auo_lcd_bkl_mode_switch(auo_lcd_bkl_mode, 1);
    auo_lcd_als_update_value();
  }
  else
  {
    MSG2("%s OFF", __func__);
    als_enabled = 0;
    auo_lcd_bkl_mode_switch(auo_lcd_bkl_mode, 1);
    auo_lcd_als_update_value();
  }
}
EXPORT_SYMBOL(auo_lcd_als_enable);
unsigned int auo_lcd_als_get_lux(void)
{
  return als_lux_value;
}
EXPORT_SYMBOL(auo_lcd_als_get_lux);

static int bklmode_misc_open(struct inode *inode_p, struct file *fp)
{
  return 0;
}
static int bklmode_misc_release(struct inode *inode_p, struct file *fp)
{
  return 0;
}
static int bklmode_misc_ioctl(struct inode *inode_p, struct file *fp, unsigned int cmd, unsigned long arg)
{
  int ret = 0;
  if(_IOC_TYPE(cmd) != BKLMODE_IOC_MAGIC)
  {
    MSG2("%s: Not BKLMODE_IOC_MAGIC", __func__);
    return -ENOTTY;
  }
  if(_IOC_DIR(cmd) & _IOC_READ)
  {
    ret = !access_ok(VERIFY_WRITE, (void __user*)arg, _IOC_SIZE(cmd));
    if(ret)
    {
      MSG2("%s: access_ok check write err", __func__);
      return -EFAULT;
    }
  }
  if(_IOC_DIR(cmd) & _IOC_WRITE)
  {
    ret = !access_ok(VERIFY_READ, (void __user*)arg, _IOC_SIZE(cmd));
    if(ret)
    {
      MSG2("%s: access_ok check read err", __func__);
      return -EFAULT;
    }
  }

  switch (cmd)
  {
    case BKLMODE_IOC_NORMAL:
      auo_lcd_bkl_mode_switch(LCD_BKL_MODE_NORMAL, 3);
      break;
    case BKLMODE_IOC_LABC:
      auo_lcd_bkl_mode_switch(LCD_BKL_MODE_LABC, 3);
      break;
    case BKLMODE_IOC_CABC_UI:
      auo_lcd_bkl_mode_switch(LCD_BKL_MODE_CABC_UI, 3);
      break;
    case BKLMODE_IOC_CABC_STILL:
      auo_lcd_bkl_mode_switch(LCD_BKL_MODE_CABC_STILL, 3);
      break;
    case BKLMODE_IOC_CABC_MOVING:
      auo_lcd_bkl_mode_switch(LCD_BKL_MODE_CABC_MOVING, 3);
      break;
    case BKLMODE_IOC_LABC_CABC_UI:
      auo_lcd_bkl_mode_switch(LCD_BKL_MODE_LABC_CABC_UI, 3);
      break;
    case BKLMODE_IOC_LABC_CABC_STILL:
      auo_lcd_bkl_mode_switch(LCD_BKL_MODE_LABC_CABC_STILL, 3);
      break;
    case BKLMODE_IOC_LABC_CABC_MOVING:
      auo_lcd_bkl_mode_switch(LCD_BKL_MODE_LABC_CABC_MOVING, 3);
      break;
    default:
      MSG("%s: unknown ioctl = 0x%X", __func__,cmd);
      break;
  }
  return ret;
}

static struct file_operations bklmode_misc_fops = {
  .owner    = THIS_MODULE,
  .open     = bklmode_misc_open,
  .release  = bklmode_misc_release,
  .ioctl    = bklmode_misc_ioctl,
};
static struct miscdevice bklmode_misc_device = {
  .minor  = MISC_DYNAMIC_MINOR,
  .name   = "bklmode",
  .fops   = &bklmode_misc_fops,
};


int mddi_os_engineer_mode_test(struct fb_info *info, void __user *p);


typedef enum {
	COLOR_PATTERN = 0,
	POWER_MODE,
	TEARING,
	GAMMA_CURVE,
	GAMMA_LEVEL,
	MAX_CMD,
} OSENGRMODE_CATEGORY;

struct lcd_os_eng_mode {
	uint32_t cmd[MAX_CMD];
	uint32_t value[MAX_CMD];
};


int mddi_os_engineer_mode_test(struct fb_info *info, void __user *p)
{
	struct lcd_os_eng_mode vinfo;
	int ret;
	int i;

	ret = copy_from_user(&vinfo, p, sizeof(vinfo));

	printk(KERN_ERR "[Engr mode] start MAX_CMD=%d\n", MAX_CMD);

	for(i=0 ; i< MAX_CMD ; i++)
	{
		if(vinfo.cmd[i] == POWER_MODE)
		{
			if(vinfo.value[i]==0)
			{
				mddi_queue_register_write(0x1300, 0x0, FALSE, 0);
				mddi_queue_register_write(0x1100, 0x0, FALSE, 0);
				mdelay(100);
				mddi_queue_register_write(0x2900, 0x0, FALSE, 0);

				auo_lcd_bkl_onOff(1);

				printk(KERN_ERR "enter [normal] mode\n");
			}

			if(vinfo.value[i]==1)
			{
				
				auo_lcd_bkl_onOff(0);
				
				mddi_queue_register_write(0x2800, 0x00, FALSE, 0);
				printk(KERN_ERR "enter [Display Off] mode\n");
			}
			
			if(vinfo.value[i]==2)
			{
				
				auo_lcd_bkl_onOff(0);
				
				mddi_queue_register_write(0x1000, 0x00, FALSE, 0);
				printk(KERN_ERR "enter [Sleep] mode\n");
			}
			
			if(vinfo.value[i]==3)
			{
				
				auo_lcd_bkl_onOff(0);
				
				mddi_queue_register_write(0x2800, 0x00, FALSE, 0);
				mddi_queue_register_write(0x1000, 0x00, FALSE, 0);
				mddi_queue_register_write(0x4F00, 0x01, FALSE, 0);
				printk(KERN_ERR "enter [Deep Sleep] mode\n");
			}
			
			if(vinfo.value[i]==4)
			{
				
				gpio_set_value(32,0);
				mdelay(20);
				gpio_set_value(32,1);
				mdelay(20);
				printk(KERN_ERR "HW reset to exit Deep Sleep mode\n");

				mddi_queue_register_write(0x1300, 0x0, FALSE, 0);
				
				mddi_queue_register_write(0xC000, 0x8A, FALSE, 0);
				mddi_queue_register_write(0xC002, 0x8A, FALSE, 0);
				mddi_queue_register_write(0xC200, 0x02, FALSE, 0);
				mddi_queue_register_write(0xC202, 0x32, FALSE, 0);
				mddi_queue_register_write(0xC100, 0x40, FALSE, 0);
				mddi_queue_register_write(0xC700, 0x8B, FALSE, 0);

				mddi_queue_register_write(0x1100, 0, FALSE, 0);
				mdelay(100);
				mddi_queue_register_write(0x2900, 0, FALSE, 0);
#if !defined(MDDI_FB_PORTRAIT)
				mddi_queue_register_write(0x3600, 0x63, FALSE, 0);
#endif
#ifdef CONFIG_DSC_ROTATE_MATRIX
//hPa -miui
				mddi_queue_register_write(0x3600, 0x03, FALSE, 0);
//
#endif
				mddi_queue_register_write(0x2C00, 0, FALSE, 0);

				
				auo_lcd_bkl_onOff(1);
				
				printk(KERN_ERR "Exit [Deep Sleep] mode\n");

				
				mddi_queue_register_write(0x1300, 0x0, FALSE, 0);
				mddi_queue_register_write(0x1100, 0x0, FALSE, 0);
				mdelay(100);
				mddi_queue_register_write(0x2900, 0x0, FALSE, 0);
			}
		}

		
		if((vinfo.cmd[i] == TEARING) && (vinfo.value[POWER_MODE]==0))
		{
			if(vinfo.value[i]==0)
			{
				mddi_queue_register_write(0x3500, 0x00, FALSE, 0);

				printk(KERN_ERR "[cmd] Tearing Off\n");
			}
			if(vinfo.value[i]==1)
			{
				mddi_queue_register_write(0x3500, 0x02, FALSE, 0);

				printk(KERN_ERR "[cmd] Tearing On\n");
			}
		}

		if((vinfo.cmd[i] == GAMMA_CURVE) && (vinfo.value[POWER_MODE]==0))
		{
			if(vinfo.value[i]==0)
			{
				mddi_queue_register_write(0x2600, 0x01, FALSE, 0);
				printk(KERN_ERR "Gamma curve =>0x%x\n",vinfo.value[i]);
			}
			if(vinfo.value[i]==1)
			{
				mddi_queue_register_write(0x2600, 0x02, FALSE, 0);
				printk(KERN_ERR "Gamma curve =>0x%x\n",vinfo.value[i]);
			}
			if(vinfo.value[i]==2)
			{
				mddi_queue_register_write(0x2600, 0x04, FALSE, 0);
				printk(KERN_ERR "Gamma curve =>0x%x\n",vinfo.value[i]);
			}
			if(vinfo.value[i]==3)
			{
				mddi_queue_register_write(0x2600, 0x08, FALSE, 0);
				printk(KERN_ERR "Gamma curve =>0x%x\n",vinfo.value[i]);
			}
		}

		if(vinfo.cmd[i] == MAX_CMD)
		{
			printk(KERN_ERR "ERR: MAX_CMD\n");
		}
	}

	return ret;
}

static void mddi_auo_prim_lcd_init(void)
{
  printk(KERN_ERR "mddi_auo_prim_lcd_init()+\n");

  mddi_queue_register_write(0xC000, 0x8A, FALSE, 0);
  mddi_queue_register_write(0xC002, 0x8A, FALSE, 0);
  mddi_queue_register_write(0xC200, 0x02, FALSE, 0);
  mddi_queue_register_write(0xC202, 0x32, FALSE, 0);
  mddi_queue_register_write(0xC100, 0x40, FALSE, 0);
  mddi_queue_register_write(0xC700, 0x8B, FALSE, 0);
  mddi_queue_register_write(0xB102, t2_register & 0xFF, FALSE, 0);

  mddi_queue_register_write(0x1100, 0, FALSE, 0);
  msleep(100);
#if !defined(MDDI_FB_PORTRAIT)
  mddi_queue_register_write(0x3600, 0x63, FALSE, 0);
#endif
#ifdef CONFIG_DSC_ROTATE_MATRIX
//hPa - miui  
  mddi_queue_register_write(0x3600, 0x03, FALSE, 0);
//
#endif
  mddi_queue_register_write(0x3500, 0x0002, FALSE, 0);
  mddi_queue_register_write(0x4400, 0x0000, FALSE, 0);
  mddi_queue_register_write(0x4401, 0x0000, FALSE, 0);
  mddi_queue_register_write(0x2900, 0, FALSE, 0);

  write_multi_mddi_reg(auo_lcd_bkl_init_array);
  if(bkl_labc_stage == 0xFFFFFFFF)
  {
    bkl_labc_stage = qsd_lsensor_read_cal_data();
    MSG2("labc stage = %d", bkl_labc_stage);
  }
  if(bkl_labc_stage >= 6)
  {
    bkl_labc_stage = 4;
    MSG2("labc stage Fail, assgin = 4");
  }
  write_multi_mddi_reg(&bkl_labc_array[bkl_labc_stage][0]);
  auo_lcd_bkl_mode_switch(auo_lcd_bkl_mode, 0xFF);
  #ifndef BACKLIGHT_EARLY_SUSPEND
    auo_lcd_bkl_onOff(1);
  #endif
  atomic_set(&auo_lcd_enable,1);

  mddipanel_inited=1;

  printk(KERN_ERR "mddi_auo_prim_lcd_init()-\n");
}

static int auo_lcd_on(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;

	printk(KERN_ERR "auo_lcd_on()+\n");

	mfd = platform_get_drvdata(pdev);

	if (!mfd)
		return -ENODEV;

	if (mfd->key != MFD_KEY)
		return -EINVAL;

	if (mfd->panel.id == AUO_WVGA_PRIM) {
		#if LCD_PWR_OFF
		mddi_lcd_disp_powerup();
		#endif

		if(unlikely(mddipanel_registed==0))
		{
			#if !defined(MDDI_FB_PORTRAIT)
			  mddi_queue_register_write(0x3600, 0x63, FALSE, 0);
			#else
#ifdef CONFIG_DSC_ROTATE_MATRIX
//hPa - miui
                          mddi_queue_register_write(0x3600, 0x03, FALSE, 0);
//
#else
			  mddi_queue_register_write(0x3600, 0x00, FALSE, 0);
#endif
			#endif
			  mddi_queue_register_write(0x3500, 0x0002, FALSE, 0);
			  mddi_queue_register_write(0x4400, 0x0000, FALSE, 0);
			  mddi_queue_register_write(0x4401, 0x0000, FALSE, 0);
			  mddi_queue_register_write(0xB102, t2_register & 0xFF, FALSE, 0);

			write_multi_mddi_reg(auo_lcd_bkl_init_array);
			if(
bkl_labc_stage == 0xFFFFFFFF)
			{
			  bkl_labc_stage = qsd_lsensor_read_cal_data();
			  MSG2("labc stage = %d", bkl_labc_stage);
			}
			if(bkl_labc_stage >= 6)
			{
			  bkl_labc_stage = 4; 
			  MSG2("labc stage Fail, assgin = 4");
			}
			write_multi_mddi_reg(&bkl_labc_array[bkl_labc_stage][0]);
			auo_lcd_bkl_mode_switch(auo_lcd_bkl_mode, 0xFF);
			#ifndef BACKLIGHT_EARLY_SUSPEND
			  auo_lcd_bkl_onOff(1);
			#endif
			atomic_set(&auo_lcd_enable,1);

			  mddipanel_inited=1;

			mddipanel_registed=1;
		}
		else
		{
#if	LONGCHARGER48HR_WAKEUP_HANG_ATLCD_WORKAROUND
			wake_lock(&lcd_idlelock);
			wake_lock(&lcd_suspendlock);
#endif	
			mddi_lcd_reset();
			mddi_auo_prim_lcd_init();
		}
	}

	mddi_host_write_pix_attr_reg(0x00C3);

	PM_LOG_EVENT(PM_LOG_ON, PM_LOG_LCD);

  #if 0
  if(mddi_test)
  {
    queue_delayed_work(qsd_mddi_wq,&qsd_mddi_delayed_work,HZ);
  }
  #endif

#if ESD_SOLUTION
	queue_delayed_work(qsd_mddi_esd_wq, &g_mddi_esd_work, (5000 * HZ / 1000));
#endif

	printk(KERN_ERR "auo_lcd_on()-\n");
	return 0;
}

static int auo_lcd_off(struct platform_device *pdev)
{
#if LCD_PWR_OFF
	 struct vreg     *vreg_mddi_lcd = vreg_get(0, "gp1");
#endif
	printk(KERN_ERR "auo_lcd_off()+\n");

  #if 0
  if(mddi_test)
  {
  	cancel_delayed_work(&qsd_mddi_delayed_work);
  	flush_workqueue(qsd_mddi_wq);
  }
  #endif

#if ESD_SOLUTION
  	cancel_delayed_work(&g_mddi_esd_work);
  	flush_workqueue(qsd_mddi_esd_wq);
#endif

  qsd_lsensor_enable_onOff(LSENSOR_ENABLE_LCD,0);

  atomic_set(&auo_lcd_enable,0);
  #ifndef BACKLIGHT_EARLY_SUSPEND
    auo_lcd_bkl_onOff(0);
  #endif

  mddi_queue_register_write(0x5100, 0x00, FALSE, 0);
  mddi_queue_register_write(0x5300, 0x00, FALSE, 0);
  mddi_queue_register_write(0x5301, 0x00, FALSE, 0);

  mddi_queue_register_write(0x3400, 0x00, FALSE, 0);

	mddi_queue_register_write(0x2800, 0x00, FALSE, 0);
        mddi_queue_register_write(0x1000, 0x00, FALSE, 0);
	#if (DEEP_SLEEP_MODE)
	mddi_queue_register_write(0x4F00, 0x01, FALSE, 0);
	#endif

	mdelay(20);

	#if LCD_PWR_OFF
	vreg_disable(vreg_mddi_lcd);
	#endif

	PM_LOG_EVENT(PM_LOG_OFF, PM_LOG_LCD);

#if	LONGCHARGER48HR_WAKEUP_HANG_ATLCD_WORKAROUND
	wake_unlock(&lcd_idlelock);
	wake_unlock(&lcd_suspendlock);
#endif

	printk(KERN_ERR "auo_lcd_off()-\n");
	return 0;
}

static int __ref auo_probe(struct platform_device *pdev)
{
  int ret, i;

	msm_fb_add_device(pdev);

#if	LONGCHARGER48HR_WAKEUP_HANG_ATLCD_WORKAROUND
	wake_lock_init(&lcd_idlelock, WAKE_LOCK_IDLE, "lcd_idleactive");
	wake_lock_init(&lcd_suspendlock, WAKE_LOCK_SUSPEND, "lcd_suspendactive");
#endif

  ret = led_classdev_register(&pdev->dev, &auo_lcd_bkl);
  if(ret)
  {
    MSG2("lcd-backlight create FAIL, ret=%d",ret);
  }
  else
  {
    for(i=0; i<ARRAY_SIZE(auo_lcd_bkl_ctrl_attrs); i++)
    {
      ret = device_create_file(auo_lcd_bkl.dev, &auo_lcd_bkl_ctrl_attrs[i]);
      if(ret) MSG2("%s: create FAIL, ret=%d",auo_lcd_bkl_ctrl_attrs[i].attr.name,ret);
    }
  }
  ret = misc_register(&bklmode_misc_device);
  if(ret)
  {
    MSG2("bklmode_misc_device create Fail, ret=%d",ret);
  }

	return 0;
}

static struct platform_driver this_driver = {
	.probe  = auo_probe,
	.driver = {
		.name   = "mddi_auo_wvga",
	},
};

static struct msm_fb_panel_data auo_panel_data = {
	.on = auo_lcd_on,
	.off = auo_lcd_off,
};

static struct platform_device this_device = {
	.name   = "mddi_auo_wvga",
	.id	= 0,
	.dev	= {
		.platform_data = &auo_panel_data,
	}
};

void mddi_lcd_disp_powerup(void)
{
	struct vreg	*vreg_mddi_lcd = vreg_get(0, "gp1");

	vreg_enable(vreg_mddi_lcd);
	vreg_set_level(vreg_mddi_lcd, 3000);
	printk(KERN_ERR "Jackie: turn on vreg 3.0V for LCD panel\n");
}
void mddi_lcd_reset(void)
{
	mddipanel_inited=0;

	gpio_set_value(32,0);
	mdelay(10);
	gpio_set_value(32,1);
	mdelay(20);
	printk(KERN_ERR "auo_init, HW reset\n");
}

#if 0
static void qsd_mddi_delayed_work_func( struct work_struct *work )
{
  static unsigned int reg5400;
  int ret;

  if(!mddi_test)
    return;
  ret = mddi_queue_register_read(0x5400, &reg5400, TRUE, 0);
  MSG2("5400=%02X %s", reg5400, !ret?"PASS":"FAIL");
  if(mddi_test)
    queue_delayed_work(qsd_mddi_wq,&qsd_mddi_delayed_work,HZ);
}
#endif

#if ESD_SOLUTION
static void qsd_mddi_esd_timer_handler( struct work_struct *work )
{
	int i=0;

	if(dynamic_log_ebable)
		printk(KERN_ERR "qsd_mddi_esd_timer_handler()+\n" );

	if(mddipanel_inited)
	{
		if(!mddi_is_in_suspend)
		{
      if(als_enabled)
        auo_lcd_als_update_value();

			for(i=0;i<20;i++)
			{
				mddi_queue_register_read(0x0A00, &esdval, TRUE, 0);
				if(esdval == 0xBEEFBEEF)
					msleep(10);
				else
					i=20;
			}

		  	if(dynamic_log_ebable)
		  		printk(KERN_ERR "MDDI GET_POWER_MODE=0x%x \n", esdval);

			if(esdval!=0x9C)
			{
					printk(KERN_CRIT "[Jackie] ### esdval=%d ###\n", esdval);

				mddi_lcd_reset();
				mddi_auo_prim_lcd_init();

					printk(KERN_CRIT "[Jackie] ### mddi_esd_timer()-01, call reset LCD ###\n");
			}
		}
	}
	else
	  printk(KERN_ERR "## MDDI panel is reseting (not initialled yet) ##\n" );

	queue_delayed_work(qsd_mddi_esd_wq, &g_mddi_esd_work, (5000 * HZ / 1000));

	if(dynamic_log_ebable)
		printk(KERN_ERR "qsd_mddi_esd_timer_handler()-\n" );
}
#endif


static int mddi_param_set(const char *val, struct kernel_param *kp)
{
	int ret;

	printk(KERN_ERR	"%s +\n", __func__);

	ret = param_set_int(val, kp);

	if(mddi_param==1)
	{
		printk(KERN_ERR "[Jackie] Manual reset MDDI panel...\n");
		mddi_lcd_reset();
		mddi_auo_prim_lcd_init();

	}
	else if(mddi_param==2)
	{
		printk(KERN_ERR "[Jackie] Manual reset MDDI panel...02\n");
		mddi_queue_register_write(0x2800, 0x00, FALSE, 0);
		mddi_queue_register_write(0x1000, 0x00, FALSE, 0);
		mdelay(10);
		mddi_lcd_reset();
		mddi_auo_prim_lcd_init();

	}
	else if(mddi_param==11)
	{
		manual_esdval=100;
		printk(KERN_ERR "[Jackie] Manual read MDDI[0x0A00] ...11\n");
		for(;;)
		{
			mddi_queue_register_read(0x0A00, &manual_esdval, TRUE, 0);
			if(manual_esdval == 0xBEEFBEEF)
				msleep(10);
			else break;
		}
		printk(KERN_ERR "[Jackie] manual_esdval=0x%x\n", manual_esdval);
	}

	else if(mddi_param==90)
	{
		printk(KERN_ERR "### turn off Dynamic Debug log ###\n");
		dynamic_log_ebable = 0;
	}

	else if(mddi_param==91)
	{
		printk(KERN_ERR "### turn on Dynamic Debug log ###\n");
		dynamic_log_ebable = 1;
	}


	printk(KERN_ERR	"%s -\n", __func__);
	return ret;
}

module_param_call(mddi, mddi_param_set, param_get_long,
		  &mddi_param, S_IWUSR | S_IRUGO);


#ifdef BACKLIGHT_EARLY_SUSPEND
static void backlight_early_suspend(struct early_suspend *h)
{
  auo_lcd_bkl_onOff(0);
}
static void backlight_late_resume(struct early_suspend *h)
{
  auo_lcd_bkl_onOff(1);
}
static struct early_suspend backlight_early_suspend_desc = {
  .level = EARLY_SUSPEND_LEVEL_STOP_DRAWING - 1,
  .suspend = backlight_early_suspend,
  .resume = backlight_late_resume,
};
#endif


static int __ref auo_init(void)
{
	int ret;
	struct msm_panel_info *pinfo;

	printk("BootLog, +%s\n", __func__);

#ifdef CONFIG_FB_MSM_MDDI_AUTO_DETECT

	ret = msm_fb_detect_client("mddi_auo_wvga");
	if (ret == -ENODEV)
	{
		printk(KERN_ERR "Jackie: msm_fb_detect_client(), -ENODEV\n");

	}

		panel_id = mddi_get_client_id();
	
		if ((panel_id >> 16) != 0xb9f6)
		{
			printk(KERN_ERR "Jackie: mddi_get_client_id(), panel_id=0x%x\n", panel_id);
		}

	printk(KERN_ERR "Jackie: auo_init()\n");
#endif

	if (system_rev == EVT2P2_Band125 || system_rev == EVT2P2_Band18) {
		gpio_set_value(21,0);
	}

	#if !LCD_PWR_OFF
	mddi_lcd_disp_powerup();
	#endif

#if RTC_WORKAROUD_BY_DISPLAYDRIVER
	msleep(150);
#endif


	ret = platform_driver_register(&this_driver);
	if (!ret) {
		pinfo = &auo_panel_data.panel_info;
#if	defined(MDDI_FB_PORTRAIT)
		pinfo->xres = 480;
		pinfo->yres = 800;
#else
		pinfo->xres = 800;
		pinfo->yres = 480;
#endif
		pinfo->type = MDDI_PANEL;
		pinfo->pdest = DISPLAY_2;
		pinfo->mddi.vdopkt = MDDI_DEFAULT_PRIM_PIX_ATTR;
		pinfo->wait_cycle = 0;
		//pinfo->bpp = 18;
		pinfo->bpp = 16;
		pinfo->fb_num = 2;
		if (panel_id ==0)
		{
			pinfo->clk_rate = 61440000;
			pinfo->clk_min =  60000000;
			pinfo->clk_max =  70000000;
		}
		else
		{
			pinfo->clk_rate = 192000000;
			pinfo->clk_min = 190000000;
			pinfo->clk_max = 200000000;
		}
		//if (vsync) pinfo->lcd.vsync_enable = TRUE; else pinfo->lcd.vsync_enable = FALSE;
		pinfo->lcd.vsync_enable = TRUE;
		pinfo->lcd.refx100 = 6000;
		pinfo->lcd.v_back_porch = 0;
		pinfo->lcd.v_front_porch = 0;
		pinfo->lcd.v_pulse_width = 0;
		//if (vsync) pinfo->lcd.hw_vsync_mode = TRUE; else pinfo->lcd.hw_vsync_mode = FALSE;
		pinfo->lcd.hw_vsync_mode = TRUE;
		pinfo->lcd.vsync_notifier_period = 0;

		printk(KERN_ERR "pinfo->clk_rate=%d\n", pinfo->clk_rate *2);

	#if 0
	qsd_mddi_wq = create_singlethread_workqueue("qsd_mddi_wq");
	INIT_DELAYED_WORK(&qsd_mddi_delayed_work, qsd_mddi_delayed_work_func);
	#endif

#if ESD_SOLUTION
	qsd_mddi_esd_wq = create_singlethread_workqueue("qsd_mddi_esd_wq");

	INIT_DELAYED_WORK( &g_mddi_esd_work, qsd_mddi_esd_timer_handler );
	    printk(KERN_ERR "init MDDI ESD DELAYED_WORK\n");
#endif

		ret = platform_device_register(&this_device);
		if (ret)
			platform_driver_unregister(&this_driver);


    #ifdef BACKLIGHT_EARLY_SUSPEND
      register_early_suspend(&backlight_early_suspend_desc);
    #endif

	}

	printk("BootLog, -%s, ret=%d\n", __func__,ret);
	return ret;
}

module_init(auo_init);
