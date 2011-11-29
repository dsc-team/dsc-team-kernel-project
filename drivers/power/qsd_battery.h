
#ifndef __QSD_BATTERY_H__
#define __QSD_BATTERY_H__

#include <linux/kernel.h>
#include <linux/power_supply.h>
#include <linux/mutex.h>
#include <linux/clocksource.h>
#include <linux/wakelock.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
  #include <linux/earlysuspend.h>
#endif

#define MYBIT(b)        (1<<b)
#define TST_BIT(x,b)    ((x & (1<<b)) ? 1 : 0)
#define CLR_BIT(x,b)    (x &= (~(1<<b)))
#define SET_BIT(x,b)    (x |= (1<<b))

#define USB_STATUS_USB_NONE                 MYBIT(0)
#define USB_STATUS_USB_IN                   MYBIT(1)
#define USB_STATUS_USB_WALL_IN              MYBIT(2)
#define USB_STATUS_USB_0                    MYBIT(3)
#define USB_STATUS_USB_100                  MYBIT(4)
#define USB_STATUS_USB_500                  MYBIT(5)
#define USB_STATUS_USB_1500                 MYBIT(6)

#define AC_STATUS_AC_NONE                   MYBIT(16)
#define AC_STATUS_AC_IN                     MYBIT(17)

#define CHG_EVENT_GAGIC_BATT_GOOD           MYBIT(24)
#define CHG_EVENT_PMIC_BATT_LOW             MYBIT(25)

void qsd_bat_update_usb_status(int flag);

#define GAUGE_ADDR    (0xAA >> 1)
#define CHARGE_ADDR   (0x80 >> 1)

#define BAT_GPIO_BAT_LOW    40
#define BAT_GPIO_CHG_INT    153

enum {BAT_TEMP_NORMAL=0, BAT_TEMP_HOT, BAT_TEMP_COLD};
enum {BAT_CHG_VOLT_4200=0, BAT_CHG_VOLT_4100};
enum {AICL_NONE=0, AICL_UPDATE, AICL_UPDATE_WAIT, AICL_VALID, AICL_INVALID};

struct qsd_bat_data
{
  struct power_supply psy_ac;
  struct power_supply psy_usb;
  struct power_supply psy_bat;

  #ifdef CONFIG_HAS_EARLYSUSPEND
    struct early_suspend drv_early_suspend;
  #endif

  struct wake_lock wlock;
  struct wake_lock wlock2;


  unsigned long jiff_property_valid_time;

  unsigned long jiff_property_valid_interval;

  unsigned long jiff_polling_interval;

  unsigned long jiff_charging_timeout;

  unsigned long jiff_ac_online_debounce_time;

  int gpio_bat_low;
  int gpio_chg_int;
  int irq_bat_low;
  int irq_chg_int;

  int bat_status;
  int bat_health;
  int bat_present;
  int bat_capacity;
  int bat_vol;
  int bat_temp;
  int bat_technology;

  int amss_batt_low;
  int bat_low_count;
  unsigned long jiff_bat_low_count_wait_time;

  int bat_health_err_count;

  int bat_temp_state;
  int bat_chg_volt_state;

  short gag_ctrl;       
  short gag_flag;       
  short gag_rm;         
  short gag_fcc;        
  short gag_ai;         

  struct {
     unsigned int enable:1;       
     unsigned int overtemp:1;     
     unsigned int bat_temp_high:1;
     unsigned int bat_temp_low:1; 
     unsigned int trick_chg:1;    
     unsigned int pre_chg:1;      
     unsigned int fast_chg:1;     
     unsigned int taper_chg:1;    
     unsigned int wdt_timeout:1;  
     unsigned int input_ovlo:1;   
     unsigned int input_uvlo:1;   
  } chg_stat;

  char  inited;
  char  suspend_flag;
  char  early_suspend_flag;
  char  wake_flag;


  char  ac_online;
  char  ac_online_tmp;
  char  usb_online;
  char  usb_aicl_state;

  char  flight_mode;
  char  charger_changed;
  char  low_bat_power_off;  
  int   usb_current;    
  int   read_again;     

  char  gagic_err;
  char  chgic_err;
};

struct qsd_bat_eng_data
{
  int cap;  
  int volt; 
  int curr; 
  int temp; 
  struct {
     unsigned int enable:1;       
     unsigned int overtemp:1;     
     unsigned int bat_temp_high:1;
     unsigned int bat_temp_low:1; 
     unsigned int trick_chg:1;    
     unsigned int pre_chg:1;      
     unsigned int fast_chg:1;     
     unsigned int taper_chg:1;    
     unsigned int wdt_timeout:1;  
     unsigned int input_ovlo:1;   
     unsigned int input_uvlo:1;   
  } chg_stat; 

  int rm;   
  int fcc;  
  int flags;
  struct {
    unsigned int snooze: 1;       
    unsigned int ocv_gd: 1;       
    unsigned int wait_id: 1;      
    unsigned int bat_det: 1;      
    unsigned int soc1: 1;         
    unsigned int socf: 1;         

    unsigned int ac: 1;           
    unsigned int usb: 1;          
    unsigned int usb_ma: 3;       

    unsigned int vok: 1;          
    unsigned int qen: 1;          
  } gag_stat;
};

#endif



