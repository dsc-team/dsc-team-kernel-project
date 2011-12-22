#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/power_supply.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/clocksource.h>
#include <linux/wakelock.h>
#include <linux/rtc.h>
#include <mach/gpio.h>
#include <mach/custmproc.h>
#include <mach/qsd_battery.h>
#include <mach/smem_pc_oem_cmd.h>
#include <mach/austin_hwid.h>


#ifdef CONFIG_HAS_EARLYSUSPEND
  #include <linux/earlysuspend.h>
#endif

#define TEST_MUGEN 0

#if (TEST_MUGEN)
static int is_mugen = 0;
static int a_voltage = 0;
static int c_voltage = 0;
static int e_charger = 0;
#endif

static int bat_log_on  = 0;
static int bat_log_on2 = 0;



#define MSG(format, arg...)


#define MSG2(format, arg...) printk("[BAT]" format "\n", ## arg)



#define MSG3(format, arg...) {if(bat_log_on)  printk(KERN_INFO "[BAT]" format "\n", ## arg);}



#define MSG4(format, arg...) {if(bat_log_on2) printk(KERN_INFO "[BAT]" format "\n", ## arg);}

#define BAT_HEALTH_DEBOUNCE_COUNT 9

#define I2C_RETRY_MAX   5



struct i2c_client *qb_i2c = NULL;
struct qsd_bat_data qb_data;
struct qsd_bat_eng_data qb_eng_data;


static struct delayed_work qsd_bat_work;
static struct workqueue_struct *qsd_bat_wqueue;



#if 1 
  const char *status_text[] = {"Unknown", "Charging", "Discharging", "Not charging", "Full"};
  const char *health_text[] = {"Unknown", "Good", "Overheat", "Dead", "Over voltage", "Unspecified failure"};
  const char *technology_text[] = {"Unknown", "NiMH", "Li-ion", "Li-poly", "LiFe", "NiCd", "LiMn"};
  const char *bat_temp_state_text[] = {"Normal", "Hot", "Cold"};
#endif

#ifdef CONFIG_PM_LOG
  #include "../../arch/arm/mach-msm/smd_private.h"
  static DEFINE_SPINLOCK(smem_bat_lock);
  static unsigned long  *smem_bat_base = NULL;
  struct timeval bat_timeval;
  struct smem_bat_info {
    long  time; 
    long  cap;  
    long  volt; 
    long  ai;   
  } smem_bat_info_data;
#endif

static void qsd_bat_work_func(struct work_struct *work);
void qsd_bat_timer_func(unsigned long temp);
static struct device_attribute qsd_bat_ctrl_attrs[];
static struct i2c_driver qsd_bat_i2c_driver;
#ifdef CONFIG_HAS_EARLYSUSPEND
  static void qsd_bat_early_suspend(struct early_suspend *h);
  static void qsd_bat_late_resume(struct early_suspend *h);
#endif




static int bat_read_i2c(uint8_t addr, uint8_t reg, uint8_t* buf, uint8_t len)
{
  struct i2c_msg msgs[] = {
    [0] = {
      .addr   = addr,
      .flags  = 0,
      .buf    = (void *)&reg,
      .len    = 1
    },
    [1] = {
      .addr   = addr,
      .flags  = I2C_M_RD,
      .buf    = (void *)buf,
      .len    = len
    }
  };
  if(!qb_i2c)
    return -ENODEV;
  return i2c_transfer(qb_i2c->adapter, msgs, 2);
}
static int bat_read_i2c_retry(uint8_t addr, uint8_t reg, uint8_t* buf, uint8_t len)
{
  int i,ret;
  for(i=0; i<I2C_RETRY_MAX; i++)
  {
    ret = bat_read_i2c(addr,reg,buf,len);
    if(ret == 2)
      return ret;
    else
      msleep(10);
  }
  return ret;
}

static int bat_write_i2c(uint8_t addr, uint8_t reg, uint8_t* buf, uint8_t len)
{
  int32_t i;
  uint8_t buf_w[64];
  struct i2c_msg msgs[] = {
    [0] = {
      .addr   = addr,
      .flags  = 0,
      .buf    = (void *)buf_w,
      .len    = len+1
    }
  };
  if(len >= sizeof(buf_w))  
    return -ENOMEM;
  if(!qb_i2c)
    return -ENODEV;
  buf_w[0] = reg;
  for(i=0; i<len; i++)
    buf_w[i+1] = buf[i];
  return i2c_transfer(qb_i2c->adapter, msgs, 1);
}
static int bat_write_i2c_retry(uint8_t addr, uint8_t reg, uint8_t* buf, uint8_t len)
{
  int i,ret;
  for(i=0; i<I2C_RETRY_MAX; i++)
  {
    ret = bat_write_i2c(addr,reg,buf,len);
    if(ret == 1)
      return ret;
    else
      msleep(10);
  }
  return ret;
}

static void bat_coin_cell_charge_onOff(uint8_t onOff)
{
  uint32_t  arg1, arg2=0, ret;
  if(onOff) 
  {
    arg1 = SMEM_PC_OEM_BAT_CTL_COIN_CHG_ON;
    ret = cust_mproc_comm1(&arg1,&arg2);
  }
  else      
  {
    arg1 = SMEM_PC_OEM_BAT_CTL_COIN_CHG_OFF;
    ret = cust_mproc_comm1(&arg1,&arg2);
  }
}
static int bat_get_flight_mode_info(void)
{
  #ifdef CONFIG_HW_AUSTIN
    uint32_t  arg1=0, arg2=0, ret;

    arg1 = SMEM_PC_OEM_AIRPLANE_MODE_INFO;
    ret = cust_mproc_comm1(&arg1,&arg2);  
    
    if(ret != 0)  
      return -EIO;
    if(arg2)  
      return 1;
    else      
      return 0;
  #else
    return 0; 
  #endif
}
static int bat_set_low_bat_flag(uint8_t onOff)
{
  uint32_t  arg1, arg2 = 0, ret, i;

  if(onOff)
    arg1 = SMEM_PC_OEM_ENABLE_LOW_BATTERY_STATUS;
  else
    arg1 = SMEM_PC_OEM_DISABLE_LOW_BATTERY_STATUS;

  ret = cust_mproc_comm1(&arg1,&arg2);
  MSG("%s =%d,%d: ret=%d (write %d)", __func__,arg1,arg2, ret, onOff);  
  if(ret)
    goto exit;

  for(i=0; i<3; i++)
  {
    msleep(10);
    arg1 = SMEM_PC_OEM_QUERY_WRITE_LOW_BATTERY_STATUS;
    ret = cust_mproc_comm1(&arg1,&arg2);
    MSG("%s =%d,%d: ret=%d (query)", __func__,arg1,arg2, ret);  
    if(ret)
      goto exit;
    if(arg2 == 2) 
      goto exit;
    
    
  }
  if(arg2 != 2)   
    ret = -EBUSY;

exit:
  if(ret == 0)
    MSG("%s low_bat_power_off %s PASS", __func__, onOff?"Set":"Clr");  
  else
    MSG2("%s low_bat_power_off %s FAIL", __func__, onOff?"Set":"Clr");  
  return ret;
}
static int bat_get_low_bat_flag(void)
{
  uint32_t  arg1, arg2=0, ret;
  arg1 = SMEM_PC_OEM_LOW_BATTERY_STATUS;
  ret = cust_mproc_comm1(&arg1,&arg2);
  MSG("%s =%d,%d: ret=%d", __func__,arg1,arg2, ret);  
  if(ret == 0)  
  {
    if(arg2)
      ret = 1;  
    else
      ret = 0;  
    MSG("%s Flag = %d", __func__, ret);
  }
  else
  {
    ret = -EIO;
    MSG2("%s FAIL!", __func__);
  }
  return ret; 
}

static int bat_get_vchg_voltage(int *volt)
{
  uint32_t  arg1 = SMEM_PC_OEM_AC_WALL_READ_VOLTAGE, arg2 = 0, ret;
  ret = cust_mproc_comm1(&arg1,&arg2);
  if(ret == 0)
    *volt = arg2;
  else
    *volt = 0;
  return ret;
}
static int bat_get_vbus_voltage(int *volt)
{
  uint32_t  arg1 = SMEM_PC_OEM_USB_WALL_READ_VOLTAGE, arg2 = 0, ret;
  ret = cust_mproc_comm1(&arg1,&arg2);
  if(ret == 0)
    *volt = arg2;
  else
    *volt = 0;
  return ret;
}
static int bat_get_usb_voltage(int *volt)
{
  #ifdef CONFIG_HW_AUSTIN
    return bat_get_vbus_voltage(volt);
  #else
    return bat_get_vchg_voltage(volt);
  #endif
}

static void qsd_bat_gauge_log(void)
{
  uint8_t bat_tvf_06_0b[6];   
  uint8_t bat_ma_14_15[2];    
  uint8_t bat_cap_2c_2d[2];   
  uint16_t  volt,flag,capacity;
  int16_t   temp,ma;
  int32_t ret;

  ret = bat_read_i2c(GAUGE_ADDR,0x06,&bat_tvf_06_0b[0],sizeof(bat_tvf_06_0b));
  ret = bat_read_i2c(GAUGE_ADDR,0x14,&bat_ma_14_15[0],sizeof(bat_ma_14_15));
  ret = bat_read_i2c(GAUGE_ADDR,0x2c,&bat_cap_2c_2d[0],sizeof(bat_cap_2c_2d));
  temp = bat_tvf_06_0b[0] + (bat_tvf_06_0b[1]<<8);
  temp = (temp - 2731)/10; 
  volt = bat_tvf_06_0b[2] + (bat_tvf_06_0b[3]<<8);
  flag = bat_tvf_06_0b[4] + (bat_tvf_06_0b[5]<<8);
  ma = bat_ma_14_15[0] + (bat_ma_14_15[1]<<8);
  capacity = bat_cap_2c_2d[0] + (bat_cap_2c_2d[1]<<8);

  MSG2("Temperature = %d",temp);
  MSG2("Voltage     = %d",volt);
  MSG2("Flags       = 0x%04X",flag);
  MSG2("FC=%d, CHG=%d, SOC1=%d, SOCF=%d, DSG=%d, OCV_GD=%d",
    TST_BIT(flag,9), TST_BIT(flag,8), TST_BIT(flag,2), TST_BIT(flag,1), TST_BIT(flag,0), TST_BIT(flag,5));
  MSG2("Current     = %d",ma);
  MSG2("Capacity    = %04d",capacity);
  MSG2("CHG_INT=%d, BAT_LOW=%d", gpio_get_value(qb_data.gpio_chg_int),gpio_get_value(qb_data.gpio_bat_low));

  MSG2("AC=%d, USB=%d, Current=%d", qb_data.ac_online, qb_data.usb_online,
    qb_data.usb_current==USB_STATUS_USB_0? 0:
    qb_data.usb_current==USB_STATUS_USB_100? 100:
    qb_data.usb_current==USB_STATUS_USB_500? 500:
    qb_data.usb_current==USB_STATUS_USB_1500? 1000: 9999
    );
  MSG2("gagic_err=%d, chgic_err=%d, suspend=%d, early=%d, wake=%d",
    qb_data.gagic_err,qb_data.chgic_err,qb_data.suspend_flag,qb_data.early_suspend_flag,qb_data.wake_flag);
}

static void qsd_bat_charge_log(void)
{
  uint8_t reg00_0b[12];
  uint8_t reg30_3c[13];

  bat_read_i2c(CHARGE_ADDR,0x00,&reg00_0b[0],sizeof(reg00_0b));
  bat_read_i2c(CHARGE_ADDR,0x30,&reg30_3c[0],sizeof(reg30_3c));

  MSG2("(00-0B= %02X %02X %02X %02X %02X  %02X %02X %02X %02X %02X  %02X %02X)",
    reg00_0b[0], reg00_0b[1], reg00_0b[2], reg00_0b[3], reg00_0b[4],
    reg00_0b[5], reg00_0b[6], reg00_0b[7], reg00_0b[8], reg00_0b[9],
    reg00_0b[10], reg00_0b[11]
    );

  MSG2("(31~39= %02X %02X %02X %02X %02X  %02X %02X %02X %02X) (%02X %02X %02X)",
    reg30_3c[1], reg30_3c[2], reg30_3c[3], reg30_3c[4], reg30_3c[5],
    reg30_3c[6], reg30_3c[7], reg30_3c[8], reg30_3c[9],
    reg30_3c[10], reg30_3c[11], reg30_3c[12]
    );
  MSG2("CHG_INT=%d, BAT_LOW=%d", gpio_get_value(qb_data.gpio_chg_int),gpio_get_value(qb_data.gpio_bat_low));

#if (TEST_MUGEN)
    if (reg30_3c[1]==0) is_mugen=1;
    if (is_mugen) MSG2("MUGEN Battery");
    is_mugen=1;
#endif

}


static int qsd_bat_get_ac_property(struct power_supply *psy,
  enum power_supply_property psp,
  union power_supply_propval *val)
{
  int ret = 0;
  if(qb_data.inited && time_after(jiffies, qb_data.jiff_property_valid_time))
  {
    cancel_delayed_work_sync(&qsd_bat_work);
    queue_delayed_work(qsd_bat_wqueue, &qsd_bat_work, 0);
  }
  switch(psp)
  {
    case POWER_SUPPLY_PROP_ONLINE:
      if(qb_data.amss_batt_low)
        val->intval = 0;
      else if(qb_data.usb_online == 2)
        val->intval = 1;
      else
        val->intval = qb_data.ac_online_tmp;
      MSG("ac:  online = %d", qb_data.ac_online);
      break;
    default:
      ret = -EINVAL;
      break;
  }
  return ret;
}

static int qsd_bat_get_usb_property(struct power_supply *psy,
  enum power_supply_property psp,
  union power_supply_propval *val)
{
  int ret = 0;
  if(qb_data.inited && time_after(jiffies, qb_data.jiff_property_valid_time))
  {
    cancel_delayed_work_sync(&qsd_bat_work);
    queue_delayed_work(qsd_bat_wqueue, &qsd_bat_work, 0);
  }
  switch(psp)
  {
    case POWER_SUPPLY_PROP_ONLINE:
      if(qb_data.amss_batt_low)
        val->intval = 0;
      else if(qb_data.usb_online == 2)
        val->intval = 0;      
      else
        val->intval = qb_data.usb_online;
      MSG("usb: online = %d", qb_data.usb_online);
      break;
    default:
      ret = -EINVAL;
      break;
  }
  return ret;
}

static int qsd_bat_get_bat_property(struct power_supply *psy,
  enum power_supply_property psp,
  union power_supply_propval *val)
{
  static int ap_get_cap_0 = 0;
  int ret = 0;
  if(qb_data.inited && time_after(jiffies, qb_data.jiff_property_valid_time))
  {
    cancel_delayed_work_sync(&qsd_bat_work);
    queue_delayed_work(qsd_bat_wqueue, &qsd_bat_work, 0);
  }
  switch(psp)
  {
    case POWER_SUPPLY_PROP_STATUS:
      val->intval = qb_data.bat_status;
      MSG("bat: status = %s", status_text[qb_data.bat_status]);
      break;
    case POWER_SUPPLY_PROP_HEALTH:
      if(qb_data.bat_health_err_count <= 0) 
      {
        val->intval = qb_data.bat_health;
        MSG("bat: health = Good");
      }
      else if(qb_data.bat_health_err_count <= BAT_HEALTH_DEBOUNCE_COUNT)  
      {
        val->intval = POWER_SUPPLY_HEALTH_GOOD;
        MSG("bat: health = Good (%s)", health_text[qb_data.bat_health]);
      }
      else  
      {
        val->intval = qb_data.bat_health;
        MSG("bat: health = %s", health_text[qb_data.bat_health]);
      }
      break;
    case POWER_SUPPLY_PROP_PRESENT:
      val->intval = qb_data.bat_present;
      MSG("bat: present = %d", qb_data.bat_present);
      break;
    case POWER_SUPPLY_PROP_CAPACITY:
      val->intval = qb_data.bat_capacity;
      if(qb_data.amss_batt_low)
      {
        MSG("bat: capacity = 0 (%d) (amss_batt_low)", qb_data.bat_capacity);
        val->intval = 0;
      }
      else
      {
        if(qb_data.bat_capacity == 0)
        {
          MSG("bat: capacity = 1 (0)", qb_data.bat_capacity);
          val->intval = 1;
        }
        else
        {
          MSG("bat: capacity = %d", qb_data.bat_capacity);
        }
      }
      if(val->intval != 0)
      {
        ap_get_cap_0 = 0;
      }
      else if(!ap_get_cap_0)
      {
        ap_get_cap_0 = 1;
        MSG2("## AP get bat_capacity = 0, it will power off!");
      }
      break;
    case POWER_SUPPLY_PROP_TECHNOLOGY:
      val->intval = qb_data.bat_technology;
      break;
    default:
      ret = -EINVAL;
      break;
  }
  return ret;
}


static ssize_t qsd_bat_get_other_property(struct device *dev, struct device_attribute *attr,char *buf)
{
  int val=0;
  const ptrdiff_t off = attr - qsd_bat_ctrl_attrs;  
  if(qb_data.inited && time_after(jiffies, qb_data.jiff_property_valid_time))
  {
    cancel_delayed_work_sync(&qsd_bat_work);
    queue_delayed_work(qsd_bat_wqueue, &qsd_bat_work, 0);
  }
  switch(off)
  {
    case 0: 
      val = qb_data.bat_vol;
      MSG("bat: batt_vol = %d", qb_data.bat_vol);
      break;
    case 1: 
      val = qb_data.bat_temp;
      MSG("bat: batt_temp = %d", qb_data.bat_temp);
      break;
    case 2: 
      if(qb_data.ac_online)
        val = 1;
      else if(qb_data.usb_online == 2)
        val = 2;
      else
        val = 0;
      MSG("bat: batt_type = %d", val);
      break;
  }
  return sprintf(buf, "%d\n", val);
}


void qsd_bat_update_usb_status(int flag)
{
  
  if(flag & USB_STATUS_USB_NONE)
  {
    qb_data.usb_online = 0;     MSG("Set [USB NONE]");
    qb_data.usb_aicl_state = AICL_NONE;
  }
  else if(flag & USB_STATUS_USB_IN)
  {
    qb_data.usb_online = 1;     MSG("Set [USB IN]");
    qb_data.usb_aicl_state = AICL_VALID;
  }
  else if(flag & USB_STATUS_USB_WALL_IN)
  {
    qb_data.usb_online = 2;     MSG("Set [USB WALL IN]");
    qb_data.usb_aicl_state = AICL_UPDATE;
  }

  if(flag & USB_STATUS_USB_0)
  {
    qb_data.usb_current = USB_STATUS_USB_0;    MSG("Set [USB 0]");
  }
  else if(flag & USB_STATUS_USB_100)
  {
    qb_data.usb_current = USB_STATUS_USB_100;  MSG("Set [USB 100]");
  }
  else if(flag & USB_STATUS_USB_500)
  {
    qb_data.usb_current = USB_STATUS_USB_500;  MSG("Set [USB 500]");
  }
  else if(flag & USB_STATUS_USB_1500)
  {
    qb_data.usb_current = USB_STATUS_USB_1500; MSG("Set [USB 1500]");
  }

  
  #ifdef CONFIG_HW_AUSTIN
    if(flag & AC_STATUS_AC_NONE)
    {
      qb_data.ac_online = 0;      MSG("Set [AC NONE]");
    }
    else if(flag & AC_STATUS_AC_IN)
    {
      qb_data.ac_online = 1;      MSG("Set [AC IN]");
    }
  #endif
  if(flag & CHG_EVENT_GAGIC_BATT_GOOD)
  {
    MSG2("## Set [BATT GOOD]");
  }
  else if(flag & CHG_EVENT_PMIC_BATT_LOW)
  {
    MSG2("## Set [BATT LOW]");
    qb_data.amss_batt_low = 1;
    if(!qb_data.wake_flag)
    {
      wake_lock_timeout(&qb_data.wlock, HZ*5);
      wake_lock_timeout(&qb_data.wlock2, HZ*5);
      MSG2("## wake_lock 5 sec (amss_batt_low)");
    }
  }

  qb_data.charger_changed = 1;

  if(qb_data.inited)
  {
    cancel_delayed_work_sync(&qsd_bat_work);
    queue_delayed_work(qsd_bat_wqueue, &qsd_bat_work, 0);
  }
}

EXPORT_SYMBOL(qsd_bat_update_usb_status);

static void qsd_bat_work_func(struct work_struct *work)
{
  
  static char ac_online_old = 0;
  static char usb_online_old = 0;
  static char flight_mode_old = 0;
  static char chg_current_term_old = 0;
  static int usb_current_old = USB_STATUS_USB_0;
  static int bat_status_old = 0;
  static int bat_health_old = 0;
  static int bat_capacity_old = 255;
  static int bat_soc_old = 255;
  static int bat_low_old = 0;
  static int bat_present_old = 1;
  static int bat_temp_state_old = BAT_TEMP_NORMAL;

  int i,tmp,status_changed = 0;

  
  char gag_reg_00_15[22];
  char gag_reg_2c_2d[2];
  enum {GAG_CTRL=0, GAG_TEMP, GAG_VOLT, GAG_FLAG, GAG_RM, GAG_FCC, GAG_AI, GAG_SOC, GAG_MAX_NUM,};
  short gag_data[GAG_MAX_NUM];
  int ret_gag1, ret_gag2;

  
  char chg_reg_02_05[4];
  char chg_reg_30;
  char chg_cmd_31;
  char chg_reg_30_39[10];
  int ret_chg_info, ret_chg;

  
  
  char ac_online_tmp   = qb_data.ac_online;
  char usb_online_tmp  = qb_data.usb_online;
  int  usb_current_tmp = qb_data.usb_current;

  #ifdef CONFIG_PM_LOG
    unsigned long flags;
    struct rtc_time tm;
  #endif

  

  if(!qb_data.inited) 
  {
    MSG2("## Cancel Work, driver not inited! ##");
    return;
  }

  

  
  
  
  memset(gag_data,0,sizeof(gag_data));
  
  {
    ret_gag1 = bat_read_i2c_retry(GAUGE_ADDR,0x00,&gag_reg_00_15[0],sizeof(gag_reg_00_15));
    ret_gag2 = bat_read_i2c_retry(GAUGE_ADDR,0x2c,&gag_reg_2c_2d[0],sizeof(gag_reg_2c_2d));
    if(ret_gag1 == 2 && ret_gag2 == 2)
    {
      qb_data.gagic_err = 0;
      gag_data[GAG_CTRL]  = gag_reg_00_15[0x00] + (gag_reg_00_15[0x01]<<8);
      gag_data[GAG_TEMP]  = gag_reg_00_15[0x06] + (gag_reg_00_15[0x07]<<8); 
      gag_data[GAG_TEMP]  = gag_data[GAG_TEMP]-2731;  
      gag_data[GAG_VOLT]  = gag_reg_00_15[0x08] + (gag_reg_00_15[0x09]<<8);
      gag_data[GAG_FLAG]  = gag_reg_00_15[0x0a] + (gag_reg_00_15[0x0b]<<8);
      gag_data[GAG_RM]    = gag_reg_00_15[0x10] + (gag_reg_00_15[0x11]<<8);
      gag_data[GAG_FCC]   = gag_reg_00_15[0x12] + (gag_reg_00_15[0x13]<<8);
      gag_data[GAG_AI]    = gag_reg_00_15[0x14] + (gag_reg_00_15[0x15]<<8);
      gag_data[GAG_SOC]   = gag_reg_2c_2d[0x00] + (gag_reg_2c_2d[0x01]<<8);

//n0p
#define HIGHEST_VOLT 4130
//#define LOWEST_VOLT 3690
#define LOWEST_VOLT 3550
#define CHARGE_CORRECTION 70
#define VSAMPLES 32

#if (TEST_MUGEN)
      if (is_mugen) {
	if (gag_data[GAG_AI]>0) {
		if (!e_charger) { a_voltage=0; e_charger=1; };
		c_voltage=gag_data[GAG_VOLT]-CHARGE_CORRECTION;
	} else {
		if (e_charger) { a_voltage=0; e_charger=0; };
		c_voltage=gag_data[GAG_VOLT];
	};
	if (a_voltage==0) a_voltage=c_voltage;
        a_voltage = a_voltage*(VSAMPLES-1)+c_voltage;
	a_voltage = a_voltage/VSAMPLES;
	qb_data.bat_capacity =  (a_voltage - LOWEST_VOLT) / ((HIGHEST_VOLT-LOWEST_VOLT)/100) ;
	if (qb_data.bat_capacity > 100) qb_data.bat_capacity = 100;
	if (qb_data.bat_capacity < 0  ) qb_data.bat_capacity = 0;
	MSG2(" average mV: %d corrected mV: %d current real mV: %d mA: %d",a_voltage,c_voltage,gag_data[GAG_VOLT],gag_data[GAG_AI]);
      } else {
#endif

      if(gag_data[GAG_SOC] < 20)   
      {
        if(qb_data.low_bat_power_off)   
          qb_data.bat_capacity = (gag_data[GAG_SOC] < 4)? gag_data[GAG_SOC] : 4;  
        else
          qb_data.bat_capacity = gag_data[GAG_SOC]; 
      }
      else  
      {
        if(gag_data[GAG_SOC] >= 90) 
          qb_data.bat_capacity = gag_data[GAG_SOC] + 4; 
        else if(gag_data[GAG_SOC] <= 89 && gag_data[GAG_SOC] >= 86) 
        {
          int soc_89_5, soc_88_5, soc_87_5, soc_86_5;
          if(gag_data[GAG_SOC] == 89)
          {
            soc_89_5 = gag_data[GAG_FCC]*885/1000;
            if(gag_data[GAG_RM] > soc_89_5)
              qb_data.bat_capacity = 93;
            else
              qb_data.bat_capacity = 92;
          }
          else if(gag_data[GAG_SOC] == 88)
          {
            soc_88_5 = gag_data[GAG_FCC]*875/1000;
            if(gag_data[GAG_RM] > soc_88_5)
              qb_data.bat_capacity = 91;
            else
              qb_data.bat_capacity = 90;
          }
          else if(gag_data[GAG_SOC] == 87)
          {
            soc_87_5 = gag_data[GAG_FCC]*865/1000;
            if(gag_data[GAG_RM] > soc_87_5)
              qb_data.bat_capacity = 89;
            else
              qb_data.bat_capacity = 88;
          }
          else if(gag_data[GAG_SOC] == 86)
          {
            soc_86_5 = gag_data[GAG_FCC]*855/1000;
            if(gag_data[GAG_RM] > soc_86_5)
              qb_data.bat_capacity = 87;
            else
              qb_data.bat_capacity = 86;
          }
          else
          {
            qb_data.bat_capacity = gag_data[GAG_SOC]; 
          }
        }
        else
          qb_data.bat_capacity = gag_data[GAG_SOC];
        if(qb_data.bat_capacity > 100)
          qb_data.bat_capacity = 100;
      }

#if TEST_MUGEN
}
#endif

      qb_data.bat_vol       = gag_data[GAG_VOLT];
      qb_data.bat_temp      = gag_data[GAG_TEMP];
      qb_data.gag_ctrl      = gag_data[GAG_CTRL];
      qb_data.gag_flag      = gag_data[GAG_FLAG];
      qb_data.gag_rm        = gag_data[GAG_RM];
      qb_data.gag_fcc       = gag_data[GAG_FCC];
      qb_data.gag_ai        = gag_data[GAG_AI];
      

      #ifdef CONFIG_PM_LOG
        if(qb_data.read_again <= 1)
        {
        do_gettimeofday(&bat_timeval);
        smem_bat_info_data.time = bat_timeval.tv_sec;
        smem_bat_info_data.cap  = gag_data[GAG_SOC];
        smem_bat_info_data.volt = gag_data[GAG_VOLT];
        smem_bat_info_data.ai   = gag_data[GAG_AI];
        spin_lock_irqsave(&smem_bat_lock, flags);
        memcpy(smem_bat_base,&smem_bat_info_data,sizeof(smem_bat_info_data));
        spin_unlock_irqrestore(&smem_bat_lock, flags);
        if(bat_log_on2) 
        {
          rtc_time_to_tm(smem_bat_info_data.time,&tm);
          MSG4("[%3d %4dmV %4dmA] (%04d-%02d-%02d %02d:%02d:%02d)",
            (int)smem_bat_info_data.cap, (int)smem_bat_info_data.volt, (int)smem_bat_info_data.ai,
            tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday, tm.tm_hour,tm.tm_min,tm.tm_sec);
        }
        }
      #endif
    }
    else
    {
      qb_data.gagic_err ++;
    }
  }

  
  
  
  memset(&chg_reg_30_39,0,sizeof(chg_reg_30_39));
  
  {
    
    ret_chg_info = bat_read_i2c_retry(CHARGE_ADDR,0x30,&chg_reg_30_39[0],sizeof(chg_reg_30_39));
    if(ret_chg_info != 2)   
    {
      if(qb_data.chgic_err < 10)  
        MSG2("## chgic read fail" );
      qb_data.chgic_err ++;
    }
    else if(qb_data.chgic_err)
    {
      MSG2("## chgic read success, chgic_err %d -> 0",qb_data.chgic_err);
      qb_data.chgic_err = 0;
    }

    
    for(i=0; i<8; i++)
    {
      if(gpio_get_value(qb_data.gpio_chg_int))  
        break;

      status_changed ++;  

      
      
      
      bat_read_i2c_retry(CHARGE_ADDR,0x30,&chg_reg_30_39[0],sizeof(chg_reg_30_39));
      chg_reg_30 = 0xFF;
      ret_chg = bat_write_i2c(CHARGE_ADDR,0x30,&chg_reg_30,sizeof(chg_reg_30)); 
      
    }

    
    ret_chg = bat_read_i2c_retry(CHARGE_ADDR,0x02,&chg_reg_02_05[0],sizeof(chg_reg_02_05));
    if(ret_chg == 2 && chg_reg_02_05[3] != 0x07)
    {
      
      if(ac_online_tmp ||
        (usb_online_tmp && usb_current_tmp==USB_STATUS_USB_1500) )
        chg_cmd_31 = 0x9C;    
      else if(usb_online_tmp && usb_current_tmp==USB_STATUS_USB_500)
        chg_cmd_31 = 0x98;    
      else
        chg_cmd_31 = 0x90;    
      bat_write_i2c_retry(CHARGE_ADDR,0x31,&chg_cmd_31,sizeof(chg_cmd_31));
      mdelay(1);
      
      chg_reg_02_05[3] = 0x07;
      bat_write_i2c_retry(CHARGE_ADDR,0x05,&chg_reg_02_05[3],sizeof(chg_reg_02_05[3]));
      
      chg_reg_02_05[0] = 0xCA;
      bat_write_i2c_retry(CHARGE_ADDR,0x02,&chg_reg_02_05[0],sizeof(chg_reg_02_05[0]));
      MSG2("## Charger Mode = [I2C Control]");
    }

    
    memset(&qb_data.chg_stat,0,sizeof(qb_data.chg_stat));
    if(ret_chg_info == 2)
    {
      if(TST_BIT(chg_reg_30_39[3],4))
        qb_data.chg_stat.overtemp = 1;
      if(TST_BIT(chg_reg_30_39[9],5) || (chg_reg_30_39[5]&0x30)==0x20)
        qb_data.chg_stat.bat_temp_high = 1;
      if(TST_BIT(chg_reg_30_39[9],4) || (chg_reg_30_39[5]&0x30)==0x10)
        qb_data.chg_stat.bat_temp_low = 1;
      if(TST_BIT(chg_reg_30_39[7],0))
        qb_data.chg_stat.trick_chg = 1;
      tmp = chg_reg_30_39[6] & 0x06;        
      if(tmp == 0x02)       qb_data.chg_stat.pre_chg = 1;
      else if(tmp == 0x04)  qb_data.chg_stat.fast_chg = 1;
      else if(tmp == 0x06)  qb_data.chg_stat.taper_chg = 1;
      if(TST_BIT(chg_reg_30_39[6],0))
        qb_data.chg_stat.enable = 1;        
      if(TST_BIT(chg_reg_30_39[3],3) || TST_BIT(chg_reg_30_39[3],1))
        qb_data.chg_stat.input_ovlo = 1;
      if((ac_online_tmp && TST_BIT(chg_reg_30_39[3],2)) ||
        (usb_online_tmp && TST_BIT(chg_reg_30_39[3],0)))
        qb_data.chg_stat.input_uvlo = 1;
    }
    
  }

  
  
  
  
  if(TST_BIT(gag_data[GAG_FLAG],3) || qb_data.gagic_err)  
    qb_data.bat_present = 1;  
  else
    qb_data.bat_present = 0;

  
  
  
  if(qb_data.gagic_err || !TST_BIT(gag_data[GAG_FLAG],5)) 
  {
    qb_data.bat_status = POWER_SUPPLY_STATUS_UNKNOWN;
  }
  else if(gag_data[GAG_AI] < 0)
  {
    qb_data.bat_status = POWER_SUPPLY_STATUS_DISCHARGING;
  }
  else if(TST_BIT(gag_data[GAG_FLAG],9) || (qb_data.bat_capacity == 100)) 
  {
    qb_data.bat_status = POWER_SUPPLY_STATUS_FULL;
    if(TST_BIT(chg_reg_30_39[6],6)) 
    {
      qb_data.jiff_charging_timeout = jiffies + 30*24*60*60*HZ;  
    }
  }
  else if(gag_data[GAG_AI] == 0)
  {
    qb_data.bat_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
  }
  else if(gag_data[GAG_AI] > 0)
  {
    if(!qb_data.ac_online_tmp && !usb_online_tmp)
      qb_data.bat_status = POWER_SUPPLY_STATUS_NOT_CHARGING;
    else
      qb_data.bat_status = POWER_SUPPLY_STATUS_CHARGING;
  }
  else
  {
    qb_data.bat_status = POWER_SUPPLY_STATUS_UNKNOWN;
  }

  
  
  
  if(qb_data.gagic_err) 
  {
    qb_data.bat_health = POWER_SUPPLY_HEALTH_UNKNOWN;
  }
  else if(!qb_data.bat_present)
  {
    qb_data.bat_health = POWER_SUPPLY_HEALTH_UNKNOWN;
  }
  else if(gag_data[GAG_VOLT] >= 4250)    
  {
    MSG3("## OverVoltage: gag battery > 4.25V");
    qb_data.bat_health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
  }
  else if(TST_BIT(chg_reg_30_39[9],3))  
  {
    MSG3("## OverVoltage: chg battery > 4.2V");
    qb_data.bat_health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
  }

  
  else if(qb_data.bat_temp_state == BAT_TEMP_NORMAL && gag_data[GAG_TEMP] > 500)
  {
    MSG3("## OverTemp: gag temp > 50'c (normal) (t:%d)",qb_data.bat_temp);
    qb_data.bat_health = POWER_SUPPLY_HEALTH_OVERHEAT;
  }
  else if(qb_data.bat_temp_state == BAT_TEMP_HOT && gag_data[GAG_TEMP] > 450)
  {
    MSG3("## OverTemp: gag temp > 45'c (hot) (t:%d)",qb_data.bat_temp);
    qb_data.bat_health = POWER_SUPPLY_HEALTH_OVERHEAT;
  }
  else if(qb_data.bat_temp_state == BAT_TEMP_NORMAL && gag_data[GAG_TEMP] < 0)
  {
    MSG3("## OverTemp: gag temp < 0'c (normal) (t:%d)",qb_data.bat_temp);
    qb_data.bat_health = POWER_SUPPLY_HEALTH_OVERHEAT;
  }
  else if(qb_data.bat_temp_state == BAT_TEMP_COLD && gag_data[GAG_TEMP] < 50)
  {
    MSG3("## OverTemp: gag temp < 5'c (cold) (t:%d)",qb_data.bat_temp);
    qb_data.bat_health = POWER_SUPPLY_HEALTH_OVERHEAT;
  }
  
  else if(qb_data.chg_stat.bat_temp_high || qb_data.chg_stat.bat_temp_low)
  {
    MSG3("## OverTemp: chg temp high/low (t:%d)",qb_data.bat_temp);
    qb_data.bat_health = POWER_SUPPLY_HEALTH_OVERHEAT;
  }

  else if(qb_data.chg_stat.overtemp)
  {
    MSG3("## Failure: ic overtemp (t:%d)",qb_data.bat_temp);
    qb_data.bat_health = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
  }
  else if(qb_data.chg_stat.input_ovlo || qb_data.chg_stat.input_uvlo)
  {
    MSG3("## Failure: chg ovlo, chg uvlo");
    qb_data.bat_health = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
  }
  else if(qb_data.usb_aicl_state == AICL_INVALID)
  {
    MSG3("## Failure: usb charger invalid");
    qb_data.bat_health = POWER_SUPPLY_HEALTH_UNSPEC_FAILURE;
  }
  else if(ac_online_tmp || (usb_online_tmp && (usb_current_tmp == USB_STATUS_USB_1500)))  
  {
    if(bat_health_old == POWER_SUPPLY_HEALTH_DEAD)
    {
      MSG3("## Dead: charging timeout (keep)");
      qb_data.bat_health = POWER_SUPPLY_HEALTH_DEAD;
    }
    else if(qb_data.early_suspend_flag &&
      time_after(jiffies, qb_data.jiff_charging_timeout) &&
      
      (qb_data.bat_capacity < 100 || !TST_BIT(chg_reg_30_39[6],6))  
      )
    {
      MSG3("## Dead: charging timeout");
      qb_data.bat_health = POWER_SUPPLY_HEALTH_DEAD;
    }
    else
    {
      MSG3("## Good: 1");
      qb_data.bat_health = POWER_SUPPLY_HEALTH_GOOD;
    }
  }
  else
  {
    MSG3("## Good: 2");
    qb_data.bat_health = POWER_SUPPLY_HEALTH_GOOD;
  }
  
  if((qb_data.bat_health == POWER_SUPPLY_HEALTH_GOOD) &&  
    (!qb_data.flight_mode) &&                             
    !qb_data.chgic_err &&                                 
    !qb_data.gagic_err &&                                 
    !TST_BIT(chg_reg_30_39[6],3) &&                       
    ((chg_reg_30_39[6]&0x30) != 0x30))                    
  {
    if(qb_data.ac_online_tmp ||
      (usb_online_tmp && (usb_current_tmp==USB_STATUS_USB_500 || usb_current_tmp==USB_STATUS_USB_1500)))
    {
      if(qb_data.bat_status==POWER_SUPPLY_STATUS_DISCHARGING || qb_data.bat_status==POWER_SUPPLY_STATUS_NOT_CHARGING)
      {
        qb_data.bat_status = POWER_SUPPLY_STATUS_CHARGING;
      }
    }
  }
  
  if(qb_data.bat_health == POWER_SUPPLY_HEALTH_GOOD)
  {
    qb_data.bat_health_err_count = 0; 
  }
  else
  {
    if(bat_health_old == qb_data.bat_health)  
    {
      if(qb_data.bat_health_err_count <= BAT_HEALTH_DEBOUNCE_COUNT)
      {
        switch(qb_data.bat_health)
        {
          case POWER_SUPPLY_HEALTH_OVERHEAT:
          case POWER_SUPPLY_HEALTH_DEAD:
          case POWER_SUPPLY_HEALTH_COLD:
            qb_data.bat_health_err_count += BAT_HEALTH_DEBOUNCE_COUNT;
            break;
          case POWER_SUPPLY_HEALTH_OVERVOLTAGE:
            qb_data.bat_health_err_count += (BAT_HEALTH_DEBOUNCE_COUNT>>1);
            break;
          case POWER_SUPPLY_HEALTH_UNSPEC_FAILURE:
          default:
            qb_data.bat_health_err_count ++;
            break;
        }
      }
    }
    else  
    {
      qb_data.bat_health_err_count = 1;
    }
  }
  
  if(!qb_data.gagic_err)  
  {
    if(!ac_online_tmp && !usb_online_tmp) 
      qb_data.bat_temp_state = BAT_TEMP_NORMAL;
    else if(qb_data.bat_temp_state == BAT_TEMP_NORMAL)
    {
      if(gag_data[GAG_TEMP] > 500)
        qb_data.bat_temp_state = BAT_TEMP_HOT;
      else if(gag_data[GAG_TEMP] < 0)
        qb_data.bat_temp_state = BAT_TEMP_COLD;
    }
    else if(qb_data.bat_temp_state == BAT_TEMP_HOT && gag_data[GAG_TEMP] <= 450)
      qb_data.bat_temp_state = BAT_TEMP_NORMAL;
    else if(qb_data.bat_temp_state == BAT_TEMP_COLD && gag_data[GAG_TEMP] >= 50)
      qb_data.bat_temp_state = BAT_TEMP_NORMAL;
    if(bat_temp_state_old != qb_data.bat_temp_state)
    {
      MSG2("## bat_temp: %s -> %s (t:%d)",bat_temp_state_text[bat_temp_state_old],bat_temp_state_text[qb_data.bat_temp_state],qb_data.bat_temp);
      bat_temp_state_old = qb_data.bat_temp_state;
      status_changed ++;
    }
  }
  
  if(!qb_data.gagic_err && !qb_data.chgic_err)  
  {
    if(!ac_online_tmp && !usb_online_tmp) 
    {
      if(qb_data.bat_chg_volt_state == BAT_CHG_VOLT_4100)
      {
        MSG2("## bat_chg_volt: 4.1V -> 4.2");
        qb_data.bat_chg_volt_state = BAT_CHG_VOLT_4200;
        status_changed ++;
      }
    }
    else if(qb_data.bat_chg_volt_state == BAT_CHG_VOLT_4200 && gag_data[GAG_TEMP] <= 50)
    {
      MSG2("## bat_chg_volt: 4.2V -> 4.1V");
      qb_data.bat_chg_volt_state = BAT_CHG_VOLT_4100;
      status_changed ++;
    }
    else if(qb_data.bat_chg_volt_state == BAT_CHG_VOLT_4100 && gag_data[GAG_TEMP] >= 70)
    {
      MSG2("## bat_chg_volt: 4.1V -> 4.2");
      qb_data.bat_chg_volt_state = BAT_CHG_VOLT_4200;
      status_changed ++;
    }
  }

  
  
  
  if(qb_data.charger_changed)
  {
    status_changed ++;
    qb_data.read_again = 6;
  }
  if(ac_online_old != ac_online_tmp)
  {
    MSG2("## ac_online: %d -> %d",ac_online_old,ac_online_tmp);
    ac_online_old = ac_online_tmp;
    status_changed ++;
    qb_data.read_again += 4;

    
    qb_data.jiff_ac_online_debounce_time = jiffies + HZ/5;  
  }
  
  if(time_after(jiffies, qb_data.jiff_ac_online_debounce_time))
  {
    MSG2("## ac_online: %d -> %d (tmp)",qb_data.ac_online_tmp,ac_online_tmp);
    qb_data.jiff_ac_online_debounce_time = jiffies + 30*24*60*60*HZ;  
    qb_data.ac_online_tmp = ac_online_tmp;
    status_changed ++;
  }
  if(usb_online_old != usb_online_tmp)
  {
    MSG2("## usb_online: %d -> %d",usb_online_old,usb_online_tmp);
    usb_online_old = usb_online_tmp;
    status_changed ++;
    qb_data.read_again += 4;
  }
  if(usb_current_old != usb_current_tmp)
  {
    MSG2("## usb_current: %d -> %d",
      usb_current_old==USB_STATUS_USB_0? 0:
      usb_current_old==USB_STATUS_USB_100? 100:
      usb_current_old==USB_STATUS_USB_500? 500:
      usb_current_old==USB_STATUS_USB_1500? 1000: 9999  ,
      usb_current_tmp==USB_STATUS_USB_0? 0:
      usb_current_tmp==USB_STATUS_USB_100? 100:
      usb_current_tmp==USB_STATUS_USB_500? 500:
      usb_current_tmp==USB_STATUS_USB_1500? 1000: 9999
      );
    usb_current_old = usb_current_tmp;
    status_changed ++;
    qb_data.read_again += 4;
  }
  if(flight_mode_old != qb_data.flight_mode)
  {
    MSG2("## flight_mode: %d -> %d",flight_mode_old,qb_data.flight_mode);
    flight_mode_old = qb_data.flight_mode;
    status_changed ++;
    qb_data.read_again += 4;
  }
  if(bat_status_old != qb_data.bat_status)
  {
    MSG2("## bat_status: %s -> %s",status_text[bat_status_old],status_text[qb_data.bat_status]);
    bat_status_old = qb_data.bat_status;
    status_changed ++;
    qb_data.read_again += 4;
  }
  if(bat_health_old != qb_data.bat_health)
  {
    MSG2("## bat_health: %s -> %s",health_text[bat_health_old],health_text[qb_data.bat_health]);
    bat_health_old = qb_data.bat_health;
    status_changed ++;
    qb_data.read_again += 4;
  }
  
  if(qb_data.amss_batt_low)
  {
    MSG2("## bat_low: vol: %d, ma: %d, fcc: %d, temp: %d",
      gag_data[GAG_VOLT], gag_data[GAG_AI], gag_data[GAG_FCC], qb_data.bat_temp);
    status_changed ++;
  }
  else
  {
    if(!qb_data.gagic_err && (gag_data[GAG_VOLT] < 3200))
    {
      qb_data.read_again += 2;
      if(gag_data[GAG_VOLT] < 3050)
      {
        qb_data.bat_low_count ++;
        if(qb_data.bat_low_count >= 3)
        {
          qb_data.amss_batt_low = 1;
          status_changed ++;
        }
        MSG2("## bat_low: vol: %d, ma: %d, fcc: %d, temp: %d, count: %d",
          gag_data[GAG_VOLT], gag_data[GAG_AI], gag_data[GAG_FCC], qb_data.bat_temp, qb_data.bat_low_count);
      }
      else
      {
        qb_data.bat_low_count = 0;
        if(gag_data[GAG_VOLT] < 3100)
        {
          MSG2("## bat_low: vol: %d, ma: %d, fcc: %d, temp = %d",
            gag_data[GAG_VOLT], gag_data[GAG_AI], gag_data[GAG_FCC], qb_data.bat_temp);
        }
      }
    }
    else if(bat_capacity_old != qb_data.bat_capacity)  
    {
      MSG2("## bat_capacity: %d (%d), vol: %d, ma: %d, fcc: %d, temp: %d",
        qb_data.bat_capacity, gag_data[GAG_SOC], gag_data[GAG_VOLT], gag_data[GAG_AI], gag_data[GAG_FCC], qb_data.bat_temp);
      bat_capacity_old = qb_data.bat_capacity;
      bat_soc_old = gag_data[GAG_SOC];
      status_changed ++;
      qb_data.read_again += 4;
    }
  }

  
  if(qb_data.low_bat_power_off)
  {
    if(qb_data.bat_status == POWER_SUPPLY_STATUS_CHARGING ||  
      qb_data.bat_capacity >= 20 )
    {
      int ret;
      if(bat_set_low_bat_flag(0)==0)
        qb_data.low_bat_power_off = 0;
      ret = bat_get_low_bat_flag();
      MSG2("## low_bat_power_off = %d",ret);
    }
  }
  else
  {
    if(qb_data.amss_batt_low) 
    {
      int ret;
      if(bat_set_low_bat_flag(1)==0)
        qb_data.low_bat_power_off = 1;
      ret = bat_get_low_bat_flag();
      MSG2("## low_bat_power_off = %d",ret);
    }
  }
  
  if(bat_present_old != qb_data.bat_present)
  {
    MSG2("## bat_present: %d -> %d",bat_present_old,qb_data.bat_present);
    bat_present_old = qb_data.bat_present;
    status_changed ++;
    qb_data.read_again += 4;
  }
  if(qb_data.read_again > (BAT_HEALTH_DEBOUNCE_COUNT+3))
    qb_data.read_again = (BAT_HEALTH_DEBOUNCE_COUNT+3);
  qb_data.charger_changed = 0;

  
  
  
  {
    int wake = 0;
    if(ac_online_tmp || usb_online_tmp)                     
      wake |= 1;
    if(wake)
    {
      if(!qb_data.wake_flag)
      {
        qb_data.wake_flag = 1;
        wake_lock(&qb_data.wlock);
        wake_lock(&qb_data.wlock2);
        MSG2("## wake_lock: 0 -> 1, vol: %d, ac: %d, usb: %d, flight: %d, bat_low: %d",
          gag_data[GAG_VOLT], ac_online_tmp, usb_online_tmp, qb_data.flight_mode, gpio_get_value(qb_data.gpio_bat_low));
      }
    }
    else
    {
      if(qb_data.wake_flag)
      {
        wake_lock_timeout(&qb_data.wlock, HZ*5);
        wake_lock_timeout(&qb_data.wlock2, HZ*5);
        qb_data.wake_flag = 0;
        MSG2("## wake_lock: 1 -> 0, vol: %d, ac: %d, usb: %d, flight: %d, bat_low: %d (5 sec)",
          gag_data[GAG_VOLT], ac_online_tmp, usb_online_tmp, qb_data.flight_mode, gpio_get_value(qb_data.gpio_bat_low));
      }
    }
  }
  

  
  if(bat_low_old && !gpio_get_value(qb_data.gpio_bat_low))
  {
    bat_low_old = 0;
    MSG2("## bat_low: 1 -> 0");
  }
  else if(!bat_low_old && gpio_get_value(qb_data.gpio_bat_low))
  {
    bat_low_old = 1;
    MSG2("## bat_low: 0 -> 1");
  }
  
  if(TST_BIT(chg_reg_30_39[6],6) != chg_current_term_old)
  {
    MSG2("## CHG CURRENT TERMINATE %d -> %d", chg_current_term_old, TST_BIT(chg_reg_30_39[6],6));
    chg_current_term_old = TST_BIT(chg_reg_30_39[6],6);
  }

  if(qb_data.usb_aicl_state == AICL_UPDATE_WAIT)
  {
    int ret=0, volt=0;
    ret = bat_get_usb_voltage(&volt);
    if(ret == 0 && volt < 4400)
    {
      MSG2("## [USB5] AICL Volt=%4d, Fail 500mA Re-check, read_again=%d",volt,qb_data.read_again);
      chg_cmd_31 = 0x88;
      bat_write_i2c_retry(CHARGE_ADDR,0x31,&chg_cmd_31,sizeof(chg_cmd_31));
      if(qb_data.usb_aicl_state == AICL_UPDATE_WAIT)
        qb_data.usb_aicl_state = AICL_INVALID;
    }
    else
    {
      if(qb_data.read_again <= 1)
      {
        MSG2("## [USB5] AICL Volt=%4d, Pass 500mA Re-check, End",volt);
        if(qb_data.usb_aicl_state == AICL_UPDATE_WAIT)
          qb_data.usb_aicl_state = AICL_VALID;
      }
      else
      {
        MSG3("## [USB5] AICL Volt=%4d, Pass 500mA Re-check, read_again=%d",volt,qb_data.read_again);
      }
    }
  }

  
  
  if(status_changed)
  {
    
    if(qb_data.chgic_err)
      goto exit_status_changed;

    
    
    
    if(qb_data.bat_chg_volt_state == BAT_CHG_VOLT_4200 && chg_reg_02_05[0] != 0xCA)
    {
      chg_reg_02_05[0] = 0xCA;
      bat_write_i2c_retry(CHARGE_ADDR,0x02,&chg_reg_02_05[0],sizeof(chg_reg_02_05[0]));
      MSG3("## Charge Voltege = 4.2V");
    }
    else if(qb_data.bat_chg_volt_state == BAT_CHG_VOLT_4100 && chg_reg_02_05[0] != 0xC0)
    {
      chg_reg_02_05[0] = 0xC0;
      bat_write_i2c_retry(CHARGE_ADDR,0x02,&chg_reg_02_05[0],sizeof(chg_reg_02_05[0]));
      MSG3("## Charge Voltege = 4.1V");
    }

    
    
    
    if(qb_data.bat_health == POWER_SUPPLY_HEALTH_OVERHEAT || qb_data.bat_health == POWER_SUPPLY_HEALTH_DEAD ||
      qb_data.bat_health == POWER_SUPPLY_HEALTH_OVERVOLTAGE || qb_data.bat_health == POWER_SUPPLY_HEALTH_UNSPEC_FAILURE ||
      qb_data.bat_present == 0 || qb_data.flight_mode)
    {
      MSG3("## Charger Mode = [DISABLE]");
      
      if(!qb_data.chg_stat.enable)
      {
        chg_cmd_31 = 0;
        if(ac_online_tmp)
          goto exit_status_changed;
        
        
        if(usb_online_tmp)
        {
          if(usb_current_tmp == USB_STATUS_USB_1500)      
          {
            if(TST_BIT(chg_reg_30_39[3],5) && chg_reg_30_39[1] == 0x8C)
              goto exit_status_changed;
          }
          else if(usb_current_tmp == USB_STATUS_USB_500)  
          {
            if(TST_BIT(chg_reg_30_39[3],6) && !TST_BIT(chg_reg_30_39[3],5) && chg_reg_30_39[1] == 0x88)
              goto exit_status_changed;
          }
          else  
          {
            if(!TST_BIT(chg_reg_30_39[3],6) && !TST_BIT(chg_reg_30_39[3],5) && chg_reg_30_39[1] == 0x80)
              goto exit_status_changed;
          }
        }
      }
      if(ac_online_tmp)
      {
        chg_cmd_31 = 0x8C;    
      }
      else if(usb_online_tmp)
      {
        if(usb_current_tmp == USB_STATUS_USB_1500)
          chg_cmd_31 = 0x8C;  
        else if(usb_current_tmp == USB_STATUS_USB_500)
          chg_cmd_31 = 0x88;  
      }
      else
        chg_cmd_31 = 0x80;    
      bat_write_i2c_retry(CHARGE_ADDR,0x31,&chg_cmd_31,sizeof(chg_cmd_31));
      MSG3("## Charger Mode = [DISABLE] (cmd31 = %02X)",chg_cmd_31);
    }
    
    
    
    else
    {
      
      if(TST_BIT(chg_reg_30_39[6],3)) 
      {
        
        MSG2("## (31~39= %02X %02X %02X %02X %02X  %02X %02X %02X %02X) Charger Error Detected!",
          chg_reg_30_39[1], chg_reg_30_39[2], chg_reg_30_39[3], chg_reg_30_39[4], chg_reg_30_39[5],
          chg_reg_30_39[6], chg_reg_30_39[7], chg_reg_30_39[8], chg_reg_30_39[9]);
        if((chg_reg_30_39[6] & 0x30) == 0x20)  
        {
          if(ac_online_tmp)
          {
            chg_cmd_31 = 0x8C;    
          }
          else if(usb_online_tmp)
          {
            if(usb_current_tmp == USB_STATUS_USB_1500)
              chg_cmd_31 = 0x8C;  
            else if(usb_current_tmp == USB_STATUS_USB_500)
              chg_cmd_31 = 0x88;  
          }
          else
            chg_cmd_31 = 0x80;    
          bat_write_i2c_retry(CHARGE_ADDR,0x31,&chg_cmd_31,sizeof(chg_cmd_31));
          qb_data.chg_stat.enable = 0;  
          MSG2("## Charge Safty Timer Timeout ==> Disable and Enable charging");
        }
      }

      
      
      
      if(ac_online_tmp)
      {
        MSG3("## Charger Mode = [AC]");
        
        if(qb_data.chg_stat.enable)
          goto exit_status_changed;
        
        chg_cmd_31 = 0x9C;
        bat_write_i2c_retry(CHARGE_ADDR,0x31,&chg_cmd_31,sizeof(chg_cmd_31));
        MSG3("## Charger Mode = [AC] (cmd31 = 9C)");
      }
      
      
      
      else if(usb_online_tmp)
      {
        
        
        
        if(usb_current_tmp == USB_STATUS_USB_1500)
        {
          MSG3("## Charger Mode = [USB HC]");
 
          if(qb_data.usb_aicl_state == AICL_UPDATE)
          {
            int ret=0, volt=0;
            MSG2("## [USB HC] AICL Set 500mA");
            chg_cmd_31 = 0x98;
            bat_write_i2c_retry(CHARGE_ADDR,0x31,&chg_cmd_31,sizeof(chg_cmd_31));

            msleep(95);
            if(qb_data.usb_aicl_state != AICL_UPDATE)
            {
              MSG2("## [USB HC] AICL Skip 500mA");
              goto exit_status_changed;
            }

            ret = bat_get_usb_voltage(&volt);
            if(ret == 0 && volt < 4400)
            {
              MSG2("## [USB HC] AICL Volt=%4d, Fail 500mA",volt);
              chg_cmd_31 = 0x88;
              bat_write_i2c_retry(CHARGE_ADDR,0x31,&chg_cmd_31,sizeof(chg_cmd_31));
              qb_data.usb_current = USB_STATUS_USB_500;
              qb_data.usb_aicl_state = AICL_INVALID;
            }
            else
            {
              MSG2("## [USB HC] AICL Set 1000mA");
              chg_cmd_31 = 0x9C;
              bat_write_i2c_retry(CHARGE_ADDR,0x31,&chg_cmd_31,sizeof(chg_cmd_31));

              msleep(95);
              if(qb_data.usb_aicl_state != AICL_UPDATE)
              {
                MSG2("## [USB HC] AICL Skip 1000mA");
                goto exit_status_changed;
              }

              ret = bat_get_usb_voltage(&volt);
              if(ret == 0 && volt < 4400)
              {
                MSG2("## [USB HC] AICL Volt=%4d, Fail 1000mA, Set 500mA",volt);
                chg_cmd_31 = 0x98;
                bat_write_i2c_retry(CHARGE_ADDR,0x31,&chg_cmd_31,sizeof(chg_cmd_31));
                qb_data.usb_current = USB_STATUS_USB_500;
                qb_data.usb_aicl_state = AICL_UPDATE_WAIT;
              }
              else
              {
                MSG2("## [USB HC] AICL Volt=%4d, Pass 1000mA",volt);
                qb_data.usb_aicl_state = AICL_VALID;
              }
            }
          }
          else
          {         
          if(qb_data.chg_stat.enable && TST_BIT(chg_reg_30_39[3],5))
            goto exit_status_changed;
          
          chg_cmd_31 = 0x9C;
          bat_write_i2c_retry(CHARGE_ADDR,0x31,&chg_cmd_31,sizeof(chg_cmd_31));
          MSG3("## Charger Mode = [USB HC] (cmd31 = 9C)");
}
        }
        
        
        
        else if(usb_current_tmp == USB_STATUS_USB_500)
        {
          MSG3("## Charger Mode = [USB5]");
 
          if(qb_data.usb_aicl_state == AICL_UPDATE)
          {
            int ret=0, volt=0;
            MSG2("## [USB5] AICL Set 500mA");
            chg_cmd_31 = 0x98;
            bat_write_i2c_retry(CHARGE_ADDR,0x31,&chg_cmd_31,sizeof(chg_cmd_31));

            msleep(95);
            if(qb_data.usb_aicl_state != AICL_UPDATE)
            {
              MSG2("## [USB5] AICL Skip 500mA");
              goto exit_status_changed;
            }

            ret = bat_get_usb_voltage(&volt);
            if(ret == 0 && volt < 4400)
            {
              MSG2("## [USB5] AICL Volt=%4d, Fail 500mA",volt);
              chg_cmd_31 = 0x88;
              bat_write_i2c_retry(CHARGE_ADDR,0x31,&chg_cmd_31,sizeof(chg_cmd_31));
              qb_data.usb_aicl_state = AICL_INVALID;
            }
            else
            {
              MSG2("## [USB5] AICL Volt=%4d, Pass 500mA",volt);
              qb_data.usb_aicl_state = AICL_UPDATE_WAIT;
            }
          }
          else
          {         
          if(qb_data.chg_stat.enable && TST_BIT(chg_reg_30_39[3],6) && !TST_BIT(chg_reg_30_39[3],5))
            goto exit_status_changed;
          
          chg_cmd_31 = 0x98;
          bat_write_i2c_retry(CHARGE_ADDR,0x31,&chg_cmd_31,sizeof(chg_cmd_31));
          MSG3("## Charger Mode = [USB5] (cmd31 = 98)");
}
        }
        
        
        
        else  
        {
          MSG3("## Charger Mode = [USB1]");
          
          if(qb_data.chg_stat.enable && !TST_BIT(chg_reg_30_39[3],6) && !TST_BIT(chg_reg_30_39[3],5))
            goto exit_status_changed;
          
          chg_cmd_31 = 0x90;
          bat_write_i2c_retry(CHARGE_ADDR,0x31,&chg_cmd_31,sizeof(chg_cmd_31));
          MSG3("## Charger Mode = [USB1] (cmd31 = 90)");
        }
      }
      
      
      
      else
      {
        MSG3("## Charger Mode = [NONE]");
        
        if(!qb_data.chg_stat.enable)
          goto exit_status_changed;
        
        chg_cmd_31 = 0x80;
        bat_write_i2c_retry(CHARGE_ADDR,0x31,&chg_cmd_31,sizeof(chg_cmd_31));
        MSG3("## Charger Mode = [NONE] (cmd31 = 80)");
      }
    }
  }
exit_status_changed:

  qb_data.jiff_property_valid_time = jiffies + qb_data.jiff_property_valid_interval;

  
  if(ac_online_tmp != qb_data.ac_online ||
    usb_online_tmp != qb_data.usb_online ||
    usb_current_tmp != qb_data.usb_current)
  {
    status_changed ++;
    if(qb_data.read_again <= 0)
      qb_data.read_again = 1;
  }
  
  if(qb_data.read_again==1 || qb_data.read_again==4)
  {
    status_changed ++;  
  }

  if(status_changed)
  {
    
    
    power_supply_changed(&qb_data.psy_bat);
  }

  if(!qb_data.suspend_flag)
  {
    if(qb_data.read_again > 0)
    {
      qb_data.read_again --;
     	queue_delayed_work(qsd_bat_wqueue, &qsd_bat_work, 1*HZ);
    }
    else if(qb_data.ac_online || qb_data.ac_online_tmp || qb_data.usb_online || (gag_data[GAG_VOLT] < 3450))
    {
      queue_delayed_work(qsd_bat_wqueue, &qsd_bat_work, qb_data.jiff_polling_interval);
    }
    else  
    {
      queue_delayed_work(qsd_bat_wqueue, &qsd_bat_work, qb_data.jiff_polling_interval*2);
    }
  }

  
  
}

#if 0 
static irqreturn_t qsd_bat_bat_low_irqhandler(int irq, void *dev_id)
{
  MSG2("%s", __func__);
  if(qb_data.inited)
  {
    if(!qb_data.wake_flag)
    {
      wake_lock_timeout(&qb_data.wlock, HZ*5);
      MSG2("## wake_lock: 5 sec when BAT_LOW");
    }
    
    cancel_delayed_work(&qsd_bat_work);
    queue_delayed_work(qsd_bat_wqueue, &qsd_bat_work, 0);
  }
  return IRQ_HANDLED;
}
#endif

static irqreturn_t qsd_bat_chg_int_irqhandler(int irq, void *dev_id)
{
  MSG3("%s", __func__);
  if(qb_data.inited)
  {
    
    cancel_delayed_work(&qsd_bat_work);
    queue_delayed_work(qsd_bat_wqueue, &qsd_bat_work, 0);
  }
  return IRQ_HANDLED;
}




static int qsd_bat_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
  int rc = 0;
  MSG("%s+", __func__);
  client->driver = &qsd_bat_i2c_driver;
  qb_i2c = client;
  MSG("%s-", __func__);
  return rc;
}

static int qsd_bat_i2c_remove(struct i2c_client *client)
{
  int rc = 0;
  qb_i2c = NULL;
  client->driver = NULL;
  return rc;
}

void qsd_bat_i2c_shutdown(struct i2c_client *client)
{
  int ret;
  char gag_cmd[2];

  
  gag_cmd[0] = 0x14;
  gag_cmd[1] = 0x00;
  ret = bat_write_i2c_retry(GAUGE_ADDR,0x00,gag_cmd,sizeof(gag_cmd));
  MSG2("%s Gauge IC CLR_SLEEP+ %s",__func__, ret==1 ? "Pass":"Fail");
  mdelay(10);
}
static int qsd_bat_i2c_suspend(struct i2c_client *client, pm_message_t state)
{
  
  qb_data.suspend_flag = 1;
  if(qb_data.inited)
  {
    cancel_delayed_work_sync(&qsd_bat_work);
    flush_workqueue(qsd_bat_wqueue);
  }
  return 0;
}
static int qsd_bat_i2c_resume(struct i2c_client *client)
{
  
  qb_data.suspend_flag = 0;
  if(qb_data.inited)
    queue_delayed_work(qsd_bat_wqueue, &qsd_bat_work, 0);
  return 0;
}

static const struct i2c_device_id qsd_bat_i2c_id[] = {
  { "qsd_bat_i2c", 0 },
  { }
};

static struct i2c_driver qsd_bat_i2c_driver = {
  .driver = {
    .owner = THIS_MODULE,
    .name = "qsd_bat_i2c"
  },
  .id_table = qsd_bat_i2c_id,
  .probe    = qsd_bat_i2c_probe,
  .remove   = qsd_bat_i2c_remove,
  .shutdown = qsd_bat_i2c_shutdown,
  .suspend  = qsd_bat_i2c_suspend,
  .resume   = qsd_bat_i2c_resume,
};




static ssize_t qsd_bat_ctrl_show(struct device *dev, struct device_attribute *attr, char *buf)
{
  int flag;

  if(qb_data.inited && time_after(jiffies, qb_data.jiff_property_valid_time))
  {
    cancel_delayed_work_sync(&qsd_bat_work);
    queue_delayed_work(qsd_bat_wqueue, &qsd_bat_work, 0);
  }

  qb_eng_data.cap = qb_data.bat_capacity;
  qb_eng_data.volt = qb_data.bat_vol;
  qb_eng_data.curr = qb_data.gag_ai;
  qb_eng_data.temp = qb_data.bat_temp;
  memcpy(&qb_eng_data.chg_stat, &qb_data.chg_stat, sizeof(qb_data.chg_stat));

  qb_eng_data.rm = qb_data.gag_rm;
  qb_eng_data.fcc = qb_data.gag_fcc;
  qb_eng_data.flags = qb_data.gag_flag;

  
  if(TST_BIT(qb_data.gag_ctrl,5))
    qb_eng_data.gag_stat.snooze = 1;
  else
    qb_eng_data.gag_stat.snooze = 0;
  if(TST_BIT(qb_data.gag_ctrl,1))
    qb_eng_data.gag_stat.vok = 1;
  else
    qb_eng_data.gag_stat.vok = 0;
  if(TST_BIT(qb_data.gag_ctrl,0))
    qb_eng_data.gag_stat.qen = 1;
  else
    qb_eng_data.gag_stat.qen = 0;

  
  flag = qb_eng_data.flags;
  if(TST_BIT(flag,5))
    qb_eng_data.gag_stat.ocv_gd = 1;
  else
    qb_eng_data.gag_stat.ocv_gd = 0;
  if(TST_BIT(flag,4))
    qb_eng_data.gag_stat.wait_id = 1;
  else
    qb_eng_data.gag_stat.wait_id = 0;
  if(TST_BIT(flag,3))
    qb_eng_data.gag_stat.bat_det = 1;
  else
    qb_eng_data.gag_stat.bat_det = 0;
  if(TST_BIT(flag,2))
    qb_eng_data.gag_stat.soc1 = 1;
  else
    qb_eng_data.gag_stat.soc1 = 0;
  if(TST_BIT(flag,1))
    qb_eng_data.gag_stat.socf = 1;
  else
    qb_eng_data.gag_stat.socf = 0;

  
  if(qb_data.ac_online)
    qb_eng_data.gag_stat.ac = 1;
  else
    qb_eng_data.gag_stat.ac = 0;
  if(qb_data.usb_online)
    qb_eng_data.gag_stat.usb = 1;
  else
    qb_eng_data.gag_stat.usb = 0;
  if(qb_data.usb_current == USB_STATUS_USB_0)           qb_eng_data.gag_stat.usb_ma = 0;
  else if(qb_data.usb_current == USB_STATUS_USB_100)    qb_eng_data.gag_stat.usb_ma = 1;
  else if(qb_data.usb_current == USB_STATUS_USB_500)    qb_eng_data.gag_stat.usb_ma = 2;
  else if(qb_data.usb_current == USB_STATUS_USB_1500)   qb_eng_data.gag_stat.usb_ma = 3;
  else                                                  qb_eng_data.gag_stat.usb_ma = 7;  

  memcpy(buf,&qb_eng_data,sizeof(qb_eng_data));
  return sizeof(qb_eng_data);
}

static void a2h(char *in, char *out) 
{
  int i;
  char a, h[2];
  for(i=0; i<2; i++)
  {
    a = *in++;
    if(a <= '9')        h[i] = a - '0';
    else if (a <= 'F')  h[i] = a - 'A' + 10;
    else if (a <= 'f')  h[i] = a - 'a' + 10;
    else                h[i] = 0;
  }
  *out = (h[0]<<4) + h[1];
}
static const char my_ascii[] = "0123456789ABCDEF";
static void h2a(char *in, char *out) 
{
  char c = *in;
  *out++ =  my_ascii[c >> 4];
  *out =    my_ascii[c & 0xF];
}
#define QSD_BAT_BUF_LENGTH  256

static ssize_t qsd_bat_ctrl_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
  char bufLocal[QSD_BAT_BUF_LENGTH];
  uint32_t  ret;

  printk(KERN_INFO "\n");
  if(count >= sizeof(bufLocal))
  {
    MSG2("%s input invalid, count = %d", __func__, count);
    return count;
  }
  memcpy(bufLocal,buf,count);

  switch(bufLocal[0])
  {
    
    
    case 'z':
      if(bufLocal[1]=='0')
      {
        MSG2("Dynamic Log All Off");
        bat_log_on = 0;
        bat_log_on2 = 0;
      }
      else if(bufLocal[1]=='1')
      {
        MSG2("Dynamic Log 1 On");
        bat_log_on = 1;
      }
      else if(bufLocal[1]=='2')
      {
        MSG2("Dynamic Log 2 On");
        bat_log_on2 = 1;
      }
      break;

    
    
    case 'f':
      if(count<2) break;
      MSG2("## Set flightmode = %c", bufLocal[1]);
      if(bufLocal[1]=='0')
      {
        qb_data.flight_mode = 0;
        if(qb_data.inited)
        {
          cancel_delayed_work_sync(&qsd_bat_work);
          queue_delayed_work(qsd_bat_wqueue, &qsd_bat_work, 0);
        }
      }
      else if(bufLocal[1]=='1')
      {
        qb_data.flight_mode = 1;
        if(qb_data.inited)
        {
          cancel_delayed_work_sync(&qsd_bat_work);
          queue_delayed_work(qsd_bat_wqueue, &qsd_bat_work, 0);
        }
      }
      else
      {
        ret = bat_get_flight_mode_info();
        MSG2("Get flight mode from AMSS = %d", ret);
      }
      break;

    case 'v':
      {
        int vchg=0, vbus=0;
        bat_get_vchg_voltage(&vchg);
        bat_get_vbus_voltage(&vbus);
      }
      break;

    case '1':
      qsd_bat_charge_log();
      break;

    
    
    case 'i':
      {
        struct i2c_msg msgs[2];
        int i2c_ret, i, j;
        char id, reg[2], len, dat[QSD_BAT_BUF_LENGTH/4];

        printk(KERN_INFO "\n");
        
        
        
        if(bufLocal[1]=='r' && count>=9)
        {
          a2h(&bufLocal[2], &id);     
          a2h(&bufLocal[4], &reg[0]); 
          a2h(&bufLocal[6], &len);    
          if(len >= sizeof(dat))
          {
            MSG2("R %02X:%02X(%02d) Fail: max length=%d", id,reg[0],len,sizeof(dat));
            break;
          }

          msgs[0].addr = id;
          msgs[0].flags = 0;
          msgs[0].buf = &reg[0];
          msgs[0].len = 1;

          msgs[1].addr = id;
          msgs[1].flags = I2C_M_RD;
          msgs[1].buf = &dat[0];
          msgs[1].len = len;

          i2c_ret = i2c_transfer(qb_i2c->adapter, msgs,2);
          if(i2c_ret != 2)
          {
            MSG2("R %02X:%02X(%02d) Fail: ret=%d", id,reg[0],len,i2c_ret);
            break;
          }

          j = 0;
          for(i=0; i<len; i++)
          {
            h2a(&dat[i], &bufLocal[j]);
            bufLocal[j+2] = ' ';
            j = j + 3;
          }
          bufLocal[j] = '\0';
          MSG2("R %02X:%02X(%02d) = %s", id,reg[0],len,bufLocal);
        }
        
        
        
        else if(bufLocal[1]=='R' && count>=11)
        {
          a2h(&bufLocal[2], &id);     
          a2h(&bufLocal[4], &reg[0]); 
          a2h(&bufLocal[6], &reg[1]); 
          a2h(&bufLocal[8], &len);    
          if(len >= sizeof(dat))
          {
            MSG2("R %02X:%02X%02X(%02d) Fail (max length=%d)", id,reg[0],reg[1],len,sizeof(dat));
            break;
          }

          msgs[0].addr = id;
          msgs[0].flags = 0;
          msgs[0].buf = &reg[0];
          msgs[0].len = 2;

          msgs[1].addr = id;
          msgs[1].flags = I2C_M_RD;
          msgs[1].buf = &dat[0];
          msgs[1].len = len;

          i2c_ret = i2c_transfer(qb_i2c->adapter, msgs,2);
          if(i2c_ret != 2)
          {
            MSG2("R %02X:%02X%02X(%02d) Fail (ret=%d)", id,reg[0],reg[1],len,i2c_ret);
            break;
          }
          j = 0;
          for(i=0; i<len; i++)
          {
            h2a(&dat[i], &bufLocal[j]);
            bufLocal[j+2] = ' ';
            j = j + 3;
          }
          bufLocal[j] = '\0';
          MSG2("R %02X:%02X%02X(%02d) = %s", id,reg[0],reg[1],len,bufLocal);
        }
        
        
        
        else if(bufLocal[1]=='w' && count>=9)
        {
          a2h(&bufLocal[2], &id);     
          len = count - 5;
          if(len & 1)
          {
            MSG2("W %02X Fail (invalid data) len=%d", id,len);
            break;
          }
          len = len/2;
          if(len >= sizeof(dat))
          {
            MSG2("W %02X Fail (too many data)", id);
            break;
          }

          j = 4;
          for(i=0; i<len; i++)
          {
            a2h(&bufLocal[j], &dat[i]);
            j = j + 2;
          }

          msgs[0].addr = id;
          msgs[0].flags = 0;
          msgs[0].buf = &dat[0];
          msgs[0].len = len;

          i2c_ret = i2c_transfer(qb_i2c->adapter, msgs,1);
          
          MSG2("W %02X = %s", id, i2c_ret==1 ? "Pass":"Fail");
        }
        else
        {
          MSG2("rd: r40000B   (addr=40(7bit), reg=00, read count=11");
          MSG2("Rd: R2C010902 (addr=2C(7bit), reg=0109, read count=2");
          MSG2("wr: w40009265CA (addr=40(7bit), reg & data=00,92,65,CA...");
        }
      }
      break;

    default:
      qsd_bat_gauge_log();
      
      break;
  }
  return count;
}

static struct device_attribute qsd_bat_ctrl_attrs[] = {
  __ATTR(batt_vol, 0664, qsd_bat_get_other_property, NULL),
  __ATTR(batt_temp, 0664, qsd_bat_get_other_property, NULL),
  __ATTR(chg_type, 0664, qsd_bat_get_other_property, NULL),
  __ATTR(ctrl, 0664, qsd_bat_ctrl_show, qsd_bat_ctrl_store),
};




static enum power_supply_property qsd_bat_ac_props[] = {
  POWER_SUPPLY_PROP_ONLINE,
};
static enum power_supply_property qsd_bat_usb_props[] = {
  POWER_SUPPLY_PROP_ONLINE,
};
static enum power_supply_property qsd_bat_bat_props[] = {
  POWER_SUPPLY_PROP_STATUS,
  POWER_SUPPLY_PROP_HEALTH,
  POWER_SUPPLY_PROP_PRESENT,
  POWER_SUPPLY_PROP_CAPACITY,
  POWER_SUPPLY_PROP_TECHNOLOGY,
};

struct qsd_bat_data qb_data = {
  .psy_ac = {
    .name   = "ac",
    .type   = POWER_SUPPLY_TYPE_MAINS,
    .properties = qsd_bat_ac_props,
    .num_properties = ARRAY_SIZE(qsd_bat_ac_props),
    .get_property = qsd_bat_get_ac_property,
  },
  .psy_usb = {
    .name   = "usb",
    .type   = POWER_SUPPLY_TYPE_USB,
    .properties = qsd_bat_usb_props,
    .num_properties = ARRAY_SIZE(qsd_bat_usb_props),
    .get_property = qsd_bat_get_usb_property,
  },
  .psy_bat = {
    .name   = "battery",
    .type   = POWER_SUPPLY_TYPE_BATTERY,
    .properties = qsd_bat_bat_props,
    .num_properties = ARRAY_SIZE(qsd_bat_bat_props),
    .get_property = qsd_bat_get_bat_property,
  },
  
  #ifdef CONFIG_HAS_EARLYSUSPEND
    .drv_early_suspend.level = 150,
    .drv_early_suspend.suspend = qsd_bat_early_suspend,
    .drv_early_suspend.resume = qsd_bat_late_resume,
  #endif
  
  .jiff_property_valid_interval = 1*HZ/2,
  .jiff_polling_interval = 10*HZ, 
  
  .gpio_bat_low = BAT_GPIO_BAT_LOW,
  .gpio_chg_int = BAT_GPIO_CHG_INT,
  
  
  
  .bat_status = POWER_SUPPLY_STATUS_UNKNOWN,
  .bat_health = POWER_SUPPLY_HEALTH_UNKNOWN,
  .bat_present = 1,
  .bat_capacity = 50,
  .bat_vol = 3800,
  .bat_temp = 270,
  .bat_technology = POWER_SUPPLY_TECHNOLOGY_LION,
  .amss_batt_low = 0,
  .bat_low_count = 0,
  .bat_health_err_count = 0,
  .bat_temp_state = BAT_TEMP_NORMAL,
  .bat_chg_volt_state = BAT_CHG_VOLT_4200,
  
  .inited = 0,
  .suspend_flag = 0,
  .early_suspend_flag = 0,
  .wake_flag = 0,
  
  .ac_online = 0,
  .usb_online = 0,
  .usb_aicl_state = AICL_NONE,
  .flight_mode = 0,
  .charger_changed = 1,
  .low_bat_power_off = 0,
  .usb_current = USB_STATUS_USB_100,
  .read_again = 0,
  
  .gagic_err = 0,
  .chgic_err = 0,
};


#ifdef CONFIG_HAS_EARLYSUSPEND
  static void qsd_bat_early_suspend(struct early_suspend *h)
  {
    
    if(qb_data.ac_online || (qb_data.usb_online && qb_data.usb_current == USB_STATUS_USB_1500))
      qb_data.jiff_charging_timeout = jiffies + 4*60*60*HZ; 
    else
      qb_data.jiff_charging_timeout = jiffies + 30*24*60*60*HZ;  
    qb_data.early_suspend_flag = 1;
    
  }
  static void qsd_bat_late_resume(struct early_suspend *h)
  {
    
    
    qb_data.jiff_charging_timeout = jiffies + 30*24*60*60*HZ;  
    qb_data.early_suspend_flag = 0;
    if(qb_data.inited)
      queue_delayed_work(qsd_bat_wqueue, &qsd_bat_work, 0);
    
  }
#endif

static int __devinit qsd_bat_probe(struct platform_device *plat_dev)
{
  int ret, fail, i;
  char gag_ctrl[2];
  char gag_cmd[2];
  char chg_reg_3a_3c[3];

  MSG("%s+", __func__);

  

  
  
  
  qb_data.gpio_chg_int = BAT_GPIO_CHG_INT;
  #if 0 
  qb_data.irq_bat_low = MSM_GPIO_TO_INT(qb_data.gpio_bat_low);
  #endif
  qb_data.irq_chg_int = MSM_GPIO_TO_INT(qb_data.gpio_chg_int);
  MSG("%s: CHG_INT gpio=%d",__func__,qb_data.gpio_chg_int);

  #if 0 
  
  ret = gpio_tlmm_config(GPIO_CFG(qb_data.gpio_bat_low,0,GPIO_INPUT,GPIO_PULL_UP,GPIO_2MA),GPIO_ENABLE);
  if(ret < 0) {fail = 0;  goto err_exit;}
  #endif
  
  ret = gpio_tlmm_config(GPIO_CFG(qb_data.gpio_chg_int,0,GPIO_CFG_INPUT,GPIO_CFG_PULL_UP,GPIO_CFG_2MA),GPIO_CFG_ENABLE);
  if(ret < 0) {fail = 1;  goto err_exit;}
  #if 0  
  ret = gpio_request(qb_data.gpio_bat_low, "irq_bat_low");
  if(ret < 0) {fail = 2;  goto err_exit;}
  #endif
  ret = gpio_request(qb_data.gpio_chg_int, "irq_chg_int");
  if(ret < 0) {fail = 3;  goto err_exit;}

  #if 0  
  
  ret = request_irq(qb_data.irq_bat_low, qsd_bat_bat_low_irqhandler, IRQF_TRIGGER_FALLING, "irq_bat_low", NULL);
  if(ret < 0) {fail = 4;  goto err_exit;}
  #endif

  
  ret = request_irq(qb_data.irq_chg_int, qsd_bat_chg_int_irqhandler, IRQF_TRIGGER_FALLING, "irq_chg_int", NULL);
  if(ret < 0) {fail = 5;  goto err_exit;}

  #if 0  
  ret = set_irq_wake(qb_data.irq_bat_low, 1);
  if(ret < 0) {fail = 6;  goto err_exit;}
  #endif
  ret = set_irq_wake(qb_data.irq_chg_int, 1);
  if(ret < 0) {fail = 7;  goto err_exit;}

  
  
  
  ret = i2c_add_driver(&qsd_bat_i2c_driver);
  if(ret < 0) {fail = 8;  goto err_exit;}

  
  
  
  ret = power_supply_register(&(plat_dev->dev), &(qb_data.psy_ac));
  if(ret < 0) {fail = 9;  goto err_exit;}
  ret = power_supply_register(&(plat_dev->dev), &(qb_data.psy_usb));
  if(ret < 0) {fail = 10; goto err_exit;}
  ret = power_supply_register(&(plat_dev->dev), &(qb_data.psy_bat));
  if(ret < 0) {fail = 11; goto err_exit;}

  
  for(i=0; i<ARRAY_SIZE(qsd_bat_ctrl_attrs); i++)
  {
    ret = device_create_file(qb_data.psy_bat.dev, &qsd_bat_ctrl_attrs[i]);
    if(ret) MSG2("%s: create FAIL, ret=%d",qsd_bat_ctrl_attrs[i].attr.name,ret);
  }

  
  
  
  
  
  {
    
    gag_cmd[0] = 0x02;
    gag_cmd[1] = 0x00;
    ret = bat_write_i2c_retry(GAUGE_ADDR,0x00,&gag_cmd[0],sizeof(gag_cmd));
    if(ret != 1)  qb_data.gagic_err ++;
    mdelay(2);
    
    ret = bat_read_i2c_retry(GAUGE_ADDR,0x00,&gag_ctrl[0],sizeof(gag_ctrl));
    if(ret != 2)  qb_data.gagic_err ++;
    MSG2("%s Gauge IC  FW_VERSION  %04X",__func__,gag_ctrl[0]+(gag_ctrl[1]<<8));

    
    gag_cmd[0] = 0x00;
    gag_cmd[1] = 0x00;
    ret = bat_write_i2c_retry(GAUGE_ADDR,0x00,&gag_cmd[0],sizeof(gag_cmd));
    if(ret != 1)  qb_data.gagic_err ++;
    mdelay(2);
    
    ret = bat_read_i2c_retry(GAUGE_ADDR,0x00,&gag_ctrl[0],sizeof(gag_ctrl));
    if(ret != 2)  qb_data.gagic_err ++;
    MSG2("%s Gauge IC  CTRL_STATUS %04X",__func__,gag_ctrl[0]+(gag_ctrl[1]<<8));

    #if 1 
    {
      if(!TST_BIT(gag_ctrl[0],0) && !qb_data.gagic_err) 
      {
        gag_cmd[0] = 0x21;        
        gag_cmd[1] = 0x00;
        ret = bat_write_i2c_retry(GAUGE_ADDR,0x00,&gag_cmd[0],sizeof(gag_cmd));
        if(ret != 1)  qb_data.gagic_err ++;
        MSG2("%s Gauge IC  QMax Enable %s!",__func__,ret==1?"Pass":"Fail");
      }
      else if(TST_BIT(gag_ctrl[0],0))
      {
        MSG2("%s Gauge IC  QMax Enabled!",__func__);
      }
    }
    #endif

    if(!TST_BIT(gag_ctrl[0],5) && !qb_data.gagic_err) 
    {
      gag_cmd[0] = 0x13;        
      gag_cmd[1] = 0x00;
      ret = bat_write_i2c_retry(GAUGE_ADDR,0x00,&gag_cmd[0],sizeof(gag_cmd));
      if(ret != 1)  qb_data.gagic_err ++;
      MSG2("%s Gauge IC  set SLEEP+ %s!",__func__,ret==1?"Pass":"Fail");
    }

    
    gag_cmd[0] = 0x00;
    gag_cmd[1] = 0x00;
    ret = bat_write_i2c_retry(GAUGE_ADDR,0x00,&gag_cmd[0],sizeof(gag_cmd));
    if(ret != 1)  qb_data.gagic_err ++;

    MSG2("%s Gauge IC  (%02X) access %s!",__func__,GAUGE_ADDR<<1,!qb_data.gagic_err?"Pass":"Fail");
  }

  
  
  
  
  
  ret = bat_read_i2c_retry(CHARGE_ADDR,0x3A,&chg_reg_3a_3c[0],sizeof(chg_reg_3a_3c));
  if(ret == 2)
  {
    MSG2("%s Charge IC (%02X) access PASS! (Reg=%02X %02X %02X)",__func__,CHARGE_ADDR<<1,
      chg_reg_3a_3c[0],chg_reg_3a_3c[1],chg_reg_3a_3c[2]);
  }
  else
  {
    MSG2("%s Charge IC (%02X) access Fail!",__func__,CHARGE_ADDR<<1);
    qb_data.chgic_err ++;
  }

  qsd_bat_charge_log();

  
  
  
  bat_coin_cell_charge_onOff(1);
  ret = bat_get_flight_mode_info();
  if(ret < 0)
  {
    MSG2("%s flight mode get Fail!",__func__);
  }
  else
  {
    qb_data.flight_mode = ret;
    MSG2("%s flight mode = %d",__func__,qb_data.flight_mode);
  }
  #ifdef CONFIG_TINY_ANDROID
    
    qb_data.flight_mode = 1;
  #endif
  ret = bat_get_low_bat_flag();
  if(ret < 0)
  {
    MSG2("%s low_bat_power_off get FAIL!",__func__);
  }
  else
  {
    qb_data.low_bat_power_off  = ret;
    MSG2("%s low_bat_power_off = %d",__func__,qb_data.low_bat_power_off);
  }

  
  #ifdef CONFIG_HAS_EARLYSUSPEND
    register_early_suspend(&qb_data.drv_early_suspend);
  #endif

  
  qb_data.jiff_charging_timeout = jiffies + 30*24*60*60*HZ;  

  
  qb_data.jiff_ac_online_debounce_time = jiffies + 30*24*60*60*HZ;  

  qb_data.jiff_bat_low_count_wait_time = jiffies + 30*24*60*60*HZ;  

  
  wake_lock_init(&qb_data.wlock, WAKE_LOCK_SUSPEND, "qsd_bat_active");
  wake_lock_init(&qb_data.wlock2, WAKE_LOCK_IDLE,   "qsd_bat_active2");

  #ifdef CONFIG_PM_LOG
    smem_bat_base = (unsigned long*) smem_alloc(SMEM_SMEM_BATT_PM_LOG, sizeof(smem_bat_info_data));
    if (!smem_bat_base)
      MSG2("%s smem_bat_base buffer allocate FAIL!",__func__);
    else
      MSG2("%s smem_bat_base buffer = 0x%08X",__func__,(unsigned int)smem_bat_base);
  #endif

  
  INIT_DELAYED_WORK(&qsd_bat_work, qsd_bat_work_func);
  qsd_bat_wqueue = create_singlethread_workqueue("qsd_bat_workqueue");
  if(qsd_bat_wqueue) 
  {
    MSG("%s qsd_bat_workqueue created PASS!",__func__);
  }
  else  
  {
    MSG2("%s qsd_bat_workqueue created FAIL!",__func__);
    fail = 10;
    goto err_exit;
  }
  qb_data.inited = 1;
  queue_delayed_work(qsd_bat_wqueue, &qsd_bat_work, 0);

  MSG("%s- (success)", __func__);
  return 0;

err_exit:

if(fail > 10)
    power_supply_unregister(&qb_data.psy_usb);
  if(fail > 9)
    power_supply_unregister(&qb_data.psy_ac);
  if(fail > 8)
    i2c_del_driver(&qsd_bat_i2c_driver);
  if(fail > 5)
    free_irq(qb_data.irq_chg_int, 0);
  #if 0  
  if(fail > 4)
    free_irq(qb_data.irq_bat_low, 0);
  #endif
  if(fail > 3)
    gpio_free(qb_data.gpio_chg_int);
  #if 0  
  if(fail > 2)
    gpio_free(qb_data.gpio_bat_low);
  #endif
  

  MSG("%s- (fail=%d)", __func__,fail);
  return ret;
}

#if 0
static int __devexit qsd_bat_remove(struct platform_device *dev)
{
  int i;
  

  qb_data.inited = 0;

  
  wake_lock_destroy(&qb_data.wlock);

  
  #ifdef CONFIG_HAS_EARLYSUSPEND
    unregister_early_suspend(&qb_data.drv_early_suspend);
  #endif

  del_timer_sync(&qb_timer);

  for(i=0; i<ARRAY_SIZE(qsd_bat_ctrl_attrs); i++)
    device_remove_file(qb_data.psy_bat.dev, &qsd_bat_ctrl_attrs[i]);

  
  power_supply_unregister(&qb_data.psy_bat);
  power_supply_unregister(&qb_data.psy_usb);
  power_supply_unregister(&qb_data.psy_ac);

  i2c_del_driver(&qsd_bat_i2c_driver);
  free_irq(qb_data.irq_chg_int, 0);
  free_irq(qb_data.irq_bat_low, 0);
  gpio_free(qb_data.gpio_chg_int);
  gpio_free(qb_data.gpio_bat_low);
  flush_scheduled_work();
  

  
  return 0;
}
#endif

static struct platform_driver qsd_bat_driver = {
  .driver.name  = "qsd_battery",
  .driver.owner = THIS_MODULE,
  .probe    = qsd_bat_probe,
  
};


static int __init qsd_bat_init(void)
{
  int ret;
  printk("BootLog, +%s\n", __func__);
  ret = platform_driver_register(&qsd_bat_driver);
  printk("BootLog, -%s, ret=%d\n", __func__,ret);
  return ret;
}
static void __exit qsd_bat_exit(void)
{
  platform_driver_unregister(&qsd_bat_driver);
}



module_init(qsd_bat_init);
module_exit(qsd_bat_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Eric Liu, Qisda Incorporated");
MODULE_DESCRIPTION("QSD Battery driver");




