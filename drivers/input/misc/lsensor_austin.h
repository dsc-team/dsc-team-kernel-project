#ifndef _LSENSOR_H_
#define _LSENSOR_H_
#include <asm/ioctl.h>
#include <linux/input.h>

#define LSENSOR_IOC_MAGIC       'l' 
#define LSENSOR_IOC_RESET       _IO(LSENSOR_IOC_MAGIC, 0)
#define LSENSOR_IOC_ENABLE      _IO(LSENSOR_IOC_MAGIC, 1)
#define LSENSOR_IOC_DISABLE     _IO(LSENSOR_IOC_MAGIC, 2)
#define LSENSOR_IOC_GET_STATUS  _IOR(LSENSOR_IOC_MAGIC, 3, float)

#define ALS_BUF_LENGTH  256

#define ALS_GPIO_SD 106

#define LSENSOR_ENABLE_SENSOR     0x01
#define LSENSOR_ENABLE_LCD        0x02
#define LSENSOR_ENABLE_LCD_ENG    0x04

struct lsensor_data {
  int enable;
  int enable_inited;
  int opened;
  int in_early_suspend;
  int vreg_state;
  struct vreg *vreg_mmc;
  struct input_dev  *input;
};

#ifdef CONFIG_LIGHT_SENSOR_QSD
void qsd_lsensor_enable_onOff(unsigned int mode, unsigned int onOff);
unsigned int qsd_lsensor_read_cal_data(void);
//unsigned int qsd_lsensor_read(void);
#else
static inline void qsd_lsensor_enable_onOff(unsigned int mode, unsigned int onOff)  { }
static inline unsigned int qsd_lsensor_read_cal_data(void)  { return 0; }
//static inline unsigned int qsd_lsensor_read(void)   { return 0; }
#endif

void auo_lcd_als_enable(unsigned int onOff);
unsigned int auo_lcd_als_get_lux(void);

#endif

