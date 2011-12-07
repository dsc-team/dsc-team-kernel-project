#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/earlysuspend.h>
#include <asm/io.h>
#include <mach/gpio.h>
#include <mach/pm_log.h>
#include <linux/i2c.h>
#include <linux/jiffies.h>
#include <mach/austin_hwid.h>
#include <mach/custmproc.h>
#include <mach/smem_pc_oem_cmd.h>
#include <linux/wakelock.h>
#include <linux/spinlock.h>
#include <mach/lsensor.h>

static int lsensor_log_on = 0;
static int lsensor_log3_on = 0;

#define MSG(format, arg...) {if(lsensor_log_on)  printk(KERN_INFO "[ALS]" format "\n", ## arg);}
#define MSG2(format, arg...) printk(KERN_INFO "[ALS]" format "\n", ## arg)
#define MSG3(format, arg...) {if(lsensor_log3_on)  printk(KERN_INFO "[ALS]" format "\n", ## arg);}
#define LSENSOR_DEBUG_ENG_MODE 0

atomic_t psensor_approached = ATOMIC_INIT(0);
extern atomic_t voice_started;

static DEFINE_MUTEX(lsensor_enable_lock);
static DEFINE_SPINLOCK(lsensor_irq_lock);

static struct lsensor_info_data ls_info;
static struct lsensor_drv_data  ls_drv =
{
  .irq_enabled = 1,
  .m_ga = 9216,
  .lux_history = {420,420,420},
  .bkl_idx = (LSENSOR_BKL_TABLE_SIZE>>1),
  .bkl_table = {
    {0 ,  0   , 19  , 0  },
    {1 ,  12  , 28  , 0  },
    {2 ,  25  , 33  , 6  },
    {3 ,  30  , 38  , 18 },
    {4 ,  35  , 43  , 27 },
    {5 ,  40  , 48  , 32 },
    {6 ,  45  , 54  , 37 },
    {7 ,  50  , 62  , 42 },
    {8 ,  58  , 70  , 47 },
    {9 ,  66  , 80  , 54 },
    {10,  75  , 92  , 62 },
    {11,  85  , 120 , 70 },
    {12,  100 , 160 , 80 },
    {13,  140 , 200 , 92 },
    {14,  180 , 270 , 120},
    {15,  220 , 370 , 160},
    {16,  310 , 480 , 200},
    {17,  420 , 610 , 260},
    {18,  540 , 720 , 360},
    {19,  670 , 830 , 480},
    {20,  770 , 940 , 600},
    {21,  880 , 1100, 720},
    {22,  1000, 1100, 820},
    },
  .prox_history = 0,
  .prox_threshold = 500,
  .distance = 1,
  .distance_old = 1,
  .distance_ap  = 1,
};
static struct lsensor_eng_data  ls_eng;
static struct lsensor_reg_data  ls_reg =
{
  .r00.pon = 0,
  .r01_0F = {
    .atime = 183,
    .ptime = 255,
    .wtime = 255,
    .ailtl = 0x00,
    .ailth = 0x80,
    .aihtl = 0x00,
    .aihth = 0x80,
    .piltl = 0x5E,
    .pilth = 0x01,
    .pihtl = 0xFF,
    .pihth = 0xFF,
    .r0C.apers  = 0,
    .r0C.ppers  = 1,
    .r0D.wlong  = 0,
    .ppcount    = 8,
    .r0F.again  = 2,
    .r0F.pdiode = 2,
    .r0F.pdrive = 1,
  }
};

static struct work_struct ls_work;

#define LSENSOR_I2C_RETRY_MAX   5
static int lsensor_read_i2c(uint8_t addr, uint8_t reg, uint8_t* buf, uint8_t len)
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
  if(!ls_drv.client)
    return -ENODEV;
  return i2c_transfer(ls_drv.client->adapter, msgs, 2);
}
static int lsensor_read_i2c_retry(uint8_t addr, uint8_t reg, uint8_t* buf, uint8_t len)
{
  int i,ret;
  for(i=0; i<LSENSOR_I2C_RETRY_MAX; i++)
  {
    ret = lsensor_read_i2c(addr,reg,buf,len);
    if(ret == 2)
      return ret;
    else
      msleep(10);
  }
  return ret;
}
static int lsensor_write_i2c(uint8_t addr, uint8_t reg, uint8_t* buf, uint8_t len)
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
  if(!ls_drv.client)
    return -ENODEV;
  buf_w[0] = reg;
  for(i=0; i<len; i++)
    buf_w[i+1] = buf[i];
  return i2c_transfer(ls_drv.client->adapter, msgs, 1);
}
static int lsensor_write_i2c_retry(uint8_t addr, uint8_t reg, uint8_t* buf, uint8_t len)
{
  int i,ret;
  for(i=0; i<LSENSOR_I2C_RETRY_MAX; i++)
  {
    ret = lsensor_write_i2c(addr,reg,buf,len);
    if(ret == 1)
      return ret;
    else
      msleep(10);
  }
  return ret;
}

void lsensor_info_data_log(void)
{
  #if LSENSOR_DEBUG_ENG_MODE
    MSG("ms=%3d %3d %3d, data=%5d %5d %5d, irf lux=%5d %6d",
      ls_info.a_ms, ls_info.p_ms, ls_info.w_ms,
      ls_info.cdata, ls_info.irdata, ls_info.pdata,
      ls_info.m_irf, ls_info.m_lux);
  #endif
}
void lsensor_eng_data_log(void)
{
  #if LSENSOR_DEBUG_ENG_MODE
    MSG("pon aen again atime m_ga = %d %d %d %d %d",
      ls_eng.pon, ls_eng.aen, ls_eng.again, ls_eng.atime, ls_eng.m_ga);
    MSG("pen ppcount pdrive ptime = %d %d %d %d",
      ls_eng.pen, ls_eng.ppcount, ls_eng.pdrive, ls_eng.ptime);
    MSG("wen wtime wlong = %d %d %d",
      ls_eng.wen, ls_eng.wtime, ls_eng.wlong);
  #endif
}

static int lsensor_clr_irq(void)
{
  int ret;
  uint8_t buf = 0x80|0x60|0x07;
  struct i2c_msg msgs[] = {
    [0] = {
      .addr   = LSENSOR_ADDR,
      .flags  = 0,
      .buf    = &buf,
      .len    = 1
    }
  };
  if(!ls_drv.client)
    return -ENODEV;
  ret = i2c_transfer(ls_drv.client->adapter, msgs, 1);
  return ret;
}

static inline void lsensor_irq_onOff(unsigned int onOff)
{
  unsigned long flags;
  spin_lock_irqsave(&lsensor_irq_lock, flags);
  if(onOff)
  {
    if(! ls_drv.irq_enabled)
    {
      ls_drv.irq_enabled = 1;
      set_irq_wake(MSM_GPIO_TO_INT(ls_drv.intr_gpio), 1);
      enable_irq(MSM_GPIO_TO_INT(ls_drv.intr_gpio));
    }
  }
  else
  {
    if(ls_drv.irq_enabled)
    {
      disable_irq_nosync(MSM_GPIO_TO_INT(ls_drv.intr_gpio));
      set_irq_wake(MSM_GPIO_TO_INT(ls_drv.intr_gpio), 0);
      ls_drv.irq_enabled = 0;
    }
  }
  spin_unlock_irqrestore(&lsensor_irq_lock, flags);
}

static void lsensor_get_lux(void)
{
  const unsigned int gain_100_table[] = {100, 800, 1600, 12000};
  const unsigned int coef[4] = {5200, 9600, 2652, 4523};
  const unsigned int gain_25_table[] = {25, 200, 400, 3000};
  const unsigned int coef_3[3][2] = { {1059, 2395}, {1795, 4238}, {455, 887}};
  unsigned int raw_clear  = ls_info.cdata;
  unsigned int raw_ir     = ls_info.irdata;
  unsigned int saturation;
  int iac, iac1, iac2;
  unsigned int lux, gain_100;
  unsigned int ratio_1024, idx, gain_25;

  saturation = (256 - ls_reg.r01_0F.atime)*1024;
  if(saturation > 65535)
    saturation = 65535;
  if(raw_clear >= saturation)
  {
    ls_info.m_irf = TAOS_MAX_LUX;
    ls_info.m_lux = TAOS_MAX_LUX;
    return;
  }

  if(system_rev <= TOUCAN_EVT2_Band148)
  {
    iac1 = raw_clear * coef[0] - raw_ir * coef[1];
    iac2 = raw_clear * coef[2] - raw_ir * coef[3];
    iac = (iac1 > iac2)? iac1:iac2;
    iac = (iac > 0)? iac:0;
    if(iac == 0)
    {
      lux = 0;
    }
    else
    {
      gain_100 = gain_100_table[ls_reg.r01_0F.r0F.again];
      lux = (iac >> 10) * ls_drv.m_ga / (ls_info.a_ms * gain_100);
    }
    ls_info.m_irf = iac;
    ls_info.m_lux = lux;
    return;
  }

  if(!raw_clear)
  {
    ls_info.m_irf = 0;
    ls_info.m_lux = 0;
    return;
  }
  ratio_1024 = (raw_ir * 1024) / raw_clear;
  if(ratio_1024 <= 122)
    idx= 0;
  else if(ratio_1024 <= 389)
    idx= 1;
  else
    idx= 2;
  gain_25 = gain_25_table[ls_reg.r01_0F.r0F.again];
  lux = coef_3[idx][0] * raw_clear - coef_3[idx][1] * raw_ir;
  ls_info.m_irf = lux;
  lux = (lux  >> 10) * ls_drv.m_ga / (ls_info.a_ms * gain_25);
  ls_info.m_lux = lux;
}

static void lsensor_update_info(void)
{
  unsigned char reg_13_19[7];
  int ret;

  ret = lsensor_read_i2c_retry(0x39,(0xA0|0x13),&reg_13_19[0],sizeof(reg_13_19));

  ls_info.cdata   = reg_13_19[1] + (reg_13_19[2]<<8);
  ls_info.irdata  = reg_13_19[3] + (reg_13_19[4]<<8);
  ls_info.pdata   = reg_13_19[5] + (reg_13_19[6]<<8);

  ls_info.a_ms = (256 - ls_reg.r01_0F.atime)*2785/1024;
  ls_info.p_ms = (256 - ls_reg.r01_0F.ptime)*2785/1024;
  if(ls_reg.r01_0F.r0D.wlong)
    ls_info.w_ms = (256 - ls_reg.r01_0F.wtime)*33423/1024;
  else
    ls_info.w_ms = (256 - ls_reg.r01_0F.wtime)*2785/1024;
  ls_info.t_ms = (!ls_reg.r00.pon) ? 0 :
    (ls_reg.r00.aen ? ls_info.a_ms : 0) +
    (ls_reg.r00.pen ? ls_info.p_ms : 0) +
    (ls_reg.r00.wen ? ls_info.w_ms : 0);

  if(!ls_reg.r00.pon || !ls_reg.r00.aen)
  {
    ls_info.m_irf = 0;
    ls_info.m_lux = 0;
    return;
  }

  if(!TST_BIT(reg_13_19[0],0))
  {
    MSG2("Error!!! ALS data invalid! 0x%02X",reg_13_19[0]);
    return;
  }

  lsensor_get_lux();

  {
    unsigned int saturation;
    saturation = (256 - ls_reg.r01_0F.atime)*1024;
    if(saturation >= 65535)
      saturation = 50000;
    else
      saturation = saturation * 10 / 13;
    if((ls_info.cdata > saturation) && (ls_reg.r01_0F.r0F.again != 0))
    {
      ls_reg.r01_0F.r0F.again = 0;
      lsensor_write_i2c_retry(0x39,(0x80|0x0F),(uint8_t *)&ls_reg.r01_0F.r0F,sizeof(ls_reg.r01_0F.r0F));
    }
    else if((ls_info.cdata < 2000) && (ls_reg.r01_0F.r0F.again != 2))
    {
      ls_reg.r01_0F.r0F.again = 2;
      lsensor_write_i2c_retry(0x39,(0x80|0x0F),(uint8_t *)&ls_reg.r01_0F.r0F,sizeof(ls_reg.r01_0F.r0F));
    }
  }
}

static void lsensor_update_result(void)
{
  static u8 middle[] = {1,0,2,0,0,2,0,1};
  int index;
  unsigned int saturation;

  ls_drv.lux_history[2] = ls_drv.lux_history[1];
  ls_drv.lux_history[1] = ls_drv.lux_history[0];
  ls_drv.lux_history[0] = ls_info.m_lux;
  {
    index = 0;
    if( ls_drv.lux_history[0] > ls_drv.lux_history[1] ) index += 4;
    if( ls_drv.lux_history[1] > ls_drv.lux_history[2] ) index += 2;
    if( ls_drv.lux_history[0] > ls_drv.lux_history[2] ) index++;
    ls_drv.millilux = ls_drv.lux_history[middle[index]];
  }

  ls_drv.prox_history <<= 1;
  ls_drv.prox_history &= 0x07;
  if(ls_info.pdata > ls_drv.prox_threshold)
  {
    saturation = (256 - ls_reg.r01_0F.atime)*768;
    if(saturation > 49152)
      saturation = 49152;
    if(ls_info.cdata < saturation)
      ls_drv.prox_history |= 1;
  }

  if(ls_drv.prox_history == 0x07)
  {
    ls_drv.distance = 0;
    if(!ls_drv.eng_mode)
    {
      if (atomic_read(&voice_started))
        atomic_set(&psensor_approached, 1);
    }
  }
  else
  {
    ls_drv.distance = 1;
    atomic_set(&psensor_approached, 0);
  }

  if(ls_drv.distance_old != ls_drv.distance)
  {
    if(ls_drv.distance_old==0 && ls_drv.distance==1)
    {
        MSG("prox 0->1 (result)");
    }
    else
    {
      MSG("prox 1->0 (result)");
    }
    ls_drv.distance_old = ls_drv.distance;
  }
}

#ifdef CONFIG_FB_MSM_LCDC_S6E63M0
void lcdc_lsensor_set_backlight(int auto_mode, int level);
#else
static void lcdc_lsensor_set_backlight(int auto_mode, int level)   {}
#endif
static void lsenosr_update_bkl(void)
{
  static unsigned int bkl_idx_old = 0xFF;
  int high, low, dd;
  unsigned i, wait;

  if(time_before(jiffies, ls_drv.jiff_update_bkl_wait_time))
    return;

  if(ls_drv.bkl_idx > (LSENSOR_BKL_TABLE_SIZE-1))
  {
    MSG2("ERROR: %s bkl_idx=%d (OVERFLOW!)",__func__,ls_drv.bkl_idx);
    ls_drv.bkl_idx = (LSENSOR_BKL_TABLE_SIZE-1);
  }
  high = ls_drv.bkl_table[ls_drv.bkl_idx].high;
  low  = ls_drv.bkl_table[ls_drv.bkl_idx].low;
  if(ls_drv.millilux <= high && ls_drv.millilux >= low)
  {
  }
  else if(ls_drv.millilux > high)
  {
    if(ls_drv.bkl_idx < (LSENSOR_BKL_TABLE_SIZE-1))
      ls_drv.bkl_idx ++;
  }
  else if(ls_drv.millilux < low)
  {
    if(ls_drv.bkl_idx > 0)
      ls_drv.bkl_idx --;
  }

  ls_info.status = ls_drv.bkl_idx;
  if(bkl_idx_old != ls_drv.bkl_idx)
  {
    for(i=LSENSOR_BKL_TABLE_SIZE-1; i>0; i--)
    {
      if(ls_drv.millilux > ls_drv.bkl_table[i].now)
        break;
    }
    if(i > ls_drv.bkl_idx)
    {
      dd = i - ls_drv.bkl_idx;
      wait = dd > 4? 200  :
             dd > 3? 400  :
             dd > 2? 800  : 1600;
    }
    else
    {
      dd = ls_drv.bkl_idx - i;
      wait = dd > 4? 400  :
             dd > 3? 800  :
             dd > 2? 1600 : 3200;
    }
    MSG3("idx=%02d i=%02d, now=%04d, lux=%04d, wait=%04d", ls_drv.bkl_idx, i, ls_drv.bkl_table[ls_drv.bkl_idx].now, ls_drv.millilux, wait);

    if(ls_drv.enable & LSENSOR_EN_LCD)
      lcdc_lsensor_set_backlight(1, ls_drv.bkl_table[ls_drv.bkl_idx].level);

    ls_drv.jiff_update_bkl_wait_time = jiffies + HZ*wait/1024;
  }
  bkl_idx_old = ls_drv.bkl_idx;
}

static void lsensor_eng_set_reg(void)
{
  ls_reg.r00.aien = 0;
  lsensor_write_i2c_retry(0x39,(0x80|0x00),(uint8_t *)&ls_reg.r00,sizeof(ls_reg.r00));

  ls_reg.r00.pon  = ls_eng.pon ? 1:0;
  ls_reg.r00.pen  = ls_eng.pen ? 1:0;
  ls_reg.r00.wen  = ls_eng.wen ? 1:0;
  ls_reg.r00.aen  = (ls_eng.aen || ls_eng.pen) ? 1:0;
  ls_reg.r00.aien = (ls_eng.pon && (ls_eng.pen || ls_eng.aen)) ? 1:0;

  ls_reg.r01_0F.atime = ls_eng.atime;
  ls_reg.r01_0F.ptime = ls_eng.ptime;
  ls_reg.r01_0F.wtime = ls_eng.wtime;

  ls_reg.r01_0F.r0D.wlong   = ls_eng.wlong ? 1:0;
  ls_reg.r01_0F.ppcount     = ls_eng.ppcount;
  ls_reg.r01_0F.r0F.again   = (ls_eng.again <= 3) ? ls_eng.again:0;
  ls_reg.r01_0F.r0F.pdrive  = (ls_eng.pdrive <= 3) ? ls_eng.pdrive:0;
  ls_reg.r01_0F.r0F.pdiode  = 2;

  ls_drv.m_ga = ls_eng.m_ga;
  lsensor_write_i2c_retry(0x39,(0xA0|0x01),(uint8_t *)&ls_reg.r01_0F,sizeof(ls_reg.r01_0F));
  lsensor_write_i2c_retry(0x39,(0x80|0x00),(uint8_t *)&ls_reg.r00,sizeof(ls_reg.r00));
}

static void lsensor_eng_get_reg(void)
{
  lsensor_read_i2c_retry(0x39,(0x80|0x00),(uint8_t *)&ls_reg.r00,sizeof(ls_reg.r00));
  lsensor_read_i2c_retry(0x39,(0xA0|0x01),(uint8_t *)&ls_reg.r01_0F,sizeof(ls_reg.r01_0F));

  ls_eng.pon = ls_reg.r00.pon;
  ls_eng.pen = ls_reg.r00.pen;
  ls_eng.aen = ls_reg.r00.aen;
  ls_eng.wen = ls_reg.r00.wen;

  ls_eng.atime = ls_reg.r01_0F.atime;
  ls_eng.ptime = ls_reg.r01_0F.ptime;
  ls_eng.wtime = ls_reg.r01_0F.wtime;

  ls_eng.wlong    = ls_reg.r01_0F.r0D.wlong;
  ls_eng.ppcount  = ls_reg.r01_0F.ppcount;
  ls_eng.again    = ls_reg.r01_0F.r0F.again;
  ls_eng.pdrive   = ls_reg.r01_0F.r0F.pdrive;

  ls_eng.m_ga = ls_drv.m_ga;
}

static void lsensor_update_equation_parameter(struct lsensor_cal_data *p_cal_data)
{
  unsigned int als, prox, prox_far;

  if(system_rev <= TOUCAN_EVT2_Band148)
  {
    if(p_cal_data->als < 500)
    {
      als = p_cal_data->als;
    }
    else
    {
      als = 71;
    }
    if(TST_BIT(p_cal_data->status,0))
    {
      ls_drv.m_ga = 500*1024/als;
      MSG2("%s: m_ga = %d",__func__, ls_drv.m_ga);
    }
    if(p_cal_data->prox.prox_far < 600)
    {
      prox  = p_cal_data->prox.prox_far;
    }
    else
    {
      prox = 150;
    }
    if(TST_BIT(p_cal_data->status,1))
    {
      ls_drv.prox_threshold = prox + 400;
      ls_reg.r01_0F.piltl = (prox + 200) & 0xFF;
      ls_reg.r01_0F.pilth = ((prox + 200)>>8) & 0xFF;
      lsensor_write_i2c_retry(0x39,(0x80|0x08),&ls_reg.r01_0F.piltl,sizeof(ls_reg.r01_0F.piltl));
      lsensor_write_i2c_retry(0x39,(0x80|0x09),&ls_reg.r01_0F.pilth,sizeof(ls_reg.r01_0F.pilth));
      MSG2("%s: prox_threshold = %d, pilt = %d",__func__,ls_drv.prox_threshold,
        ((ls_reg.r01_0F.pilth << 8) + ls_reg.r01_0F.piltl) );
    }
  }
  else
  {
    if(p_cal_data->als < 100 && p_cal_data->als > 20)
    {
      als = p_cal_data->als;
    }
    else
    {
      als = 55;
    }
    if(TST_BIT(p_cal_data->status,0))
    {
      ls_drv.m_ga = 500*1024/als;
      MSG2("%s: m_ga = %d",__func__, ls_drv.m_ga);
    }
    if((p_cal_data->prox.prox_far > 1023) ||  
      (p_cal_data->prox.prox_near > 1023) ||  
      (p_cal_data->prox.prox_far >= p_cal_data->prox.prox_near))  
    {
      prox      = 500;   
      prox_far  = 350;   
    }
    else if((p_cal_data->prox.prox_far > 200) ||  
            (p_cal_data->prox.prox_near < 200))   
    {
      prox      = p_cal_data->prox.prox_far + 300;
      prox_far  = p_cal_data->prox.prox_far + 150;
    }
    else if((p_cal_data->prox.prox_far + 100) >= p_cal_data->prox.prox_near)
    {
      prox      = p_cal_data->prox.prox_far + 200;
      prox_far  = p_cal_data->prox.prox_far + 100;
    }
    else  
    {
      prox      = p_cal_data->prox.prox_near;
      prox_far  = (p_cal_data->prox.prox_far + p_cal_data->prox.prox_near) / 2;
    }
    if(prox >= 900) 
    {
      prox      = 1000;
      prox_far  = 850;
    }
    if(TST_BIT(p_cal_data->status,1))
    {
      ls_drv.prox_threshold = prox;  
      ls_reg.r01_0F.piltl = prox_far & 0xFF; 
      ls_reg.r01_0F.pilth = (prox_far>>8) & 0xFF;
      lsensor_write_i2c_retry(0x39,(0x80|0x08),&ls_reg.r01_0F.piltl,sizeof(ls_reg.r01_0F.piltl));
      lsensor_write_i2c_retry(0x39,(0x80|0x09),&ls_reg.r01_0F.pilth,sizeof(ls_reg.r01_0F.pilth));
      MSG2("%s: prox_threshold = %d, pilt = %d",__func__,ls_drv.prox_threshold,
        ((ls_reg.r01_0F.pilth << 8) + ls_reg.r01_0F.piltl) );
    }
  }
}
static void lsensor_read_nv_data(struct lsensor_cal_data* p_cal_data)
{
  unsigned int arg1, arg2;
  int ret;

  arg1 = SMEM_PC_OEM_GET_ALS_CAL_NV_DATA;
  arg2 = 0;
  ret = cust_mproc_comm1(&arg1,&arg2);
  if(!ret)
  {
    MSG2("%s: ALS NV  = 0x%08X",__func__,arg2);
    SET_BIT(p_cal_data->status,0);
    p_cal_data->als = arg2;
  }
  else
  {
    MSG2("%s: ALS NV  read Fail!",__func__);
    CLR_BIT(p_cal_data->status,0);
    p_cal_data->als = 0xFFFFFFFF;
  }
  arg1 = SMEM_PC_OEM_GET_PROXIMITY_SENSOR_NV_DATA;
  arg2 = 0;
  ret = cust_mproc_comm1(&arg1,&arg2);
  if(!ret)
  {
    MSG2("%s: PROX NV = 0x%08X",__func__,arg2);
    SET_BIT(p_cal_data->status,1); 
    p_cal_data->prox.prox_far  = arg2 & 0xFFFF;
    p_cal_data->prox.prox_near = (arg2>>16) & 0xFFFF;
  }
  else
  {
    MSG2("%s: PROX NV read Fail!",__func__);
    CLR_BIT(p_cal_data->status,1); 
    p_cal_data->prox.prox_far  = 0xFFFF;
    p_cal_data->prox.prox_near = 0xFFFF;
  }
}
static void lsensor_write_nv_data(struct lsensor_cal_data* p_cal_data)
{
  unsigned int arg1, arg2;
  int ret, i, ret_val=0;
  static qisda_diagpkt_req_rsp_type *pkt_ptr = NULL;
  nv_write_command *write_command;
  diagpkt_subsys_hdr_type_v2 *backup_command;

  if(!pkt_ptr)
  {
    arg1 = SMEM_PC_OEM_GET_QISDA_DIAGPKT_REQ_RSP_ADDR;
    arg2 = 0;
    ret = cust_mproc_comm1(&arg1,&arg2);
    if(!ret)
    {
    }
    else
    {
      MSG2("%s: (1)diagpkt alloc req, ret = %d (FAIL)",__func__,ret);
      ret_val = ret;
      goto exit_err;
    }
    arg1 = SMEM_PC_OEM_GET_QISDA_DIAGPKT_REQ_RSP_ADDR_STATUS;
    arg2 = 0;
    for(i=0; i<5; i++)
    {
      msleep(10);
      ret = cust_mproc_comm1(&arg1,&arg2);
      if(!ret)
      {
        pkt_ptr = (qisda_diagpkt_req_rsp_type *)
          ioremap((unsigned long)arg2, sizeof(qisda_diagpkt_req_rsp_type));
        MSG2("%s: (1)diagpkt alloc addr = 0x%08X",__func__,(int)pkt_ptr);
        if(!pkt_ptr)
        {
          ret_val = -ENOMEM;
          goto exit_err;
        }
        break;
      }
      else
      {
        MSG2("%s: (1)diagpkt alloc query: ret = %d (FAIL)",__func__,ret);
        ret_val = ret;
        goto exit_err;
      }
    }
  }
  else
  {
    MSG2("%s: (1)diagpkt alloc addr = 0x%08X (alloc alrady)",__func__,(int)pkt_ptr);
  }

  if(TST_BIT(p_cal_data->status, 0))
  {
    memset(pkt_ptr,0,sizeof(qisda_diagpkt_req_rsp_type));
    write_command = (nv_write_command*)pkt_ptr->piRequestBytes;
    write_command->nv_operation   = 0x27;
    write_command->nv_item_index  = ALS_NV_ITEM;
    write_command->item_data[0]   = (p_cal_data->als >>  0) & 0xFF;
    write_command->item_data[1]   = (p_cal_data->als >>  8) & 0xFF;
    write_command->item_data[2]   = (p_cal_data->als >> 16) & 0xFF;
    write_command->item_data[3]   = (p_cal_data->als >> 24) & 0xFF;
    arg1 = SMEM_PC_OEM_QISDA_DIAGPKT_REQ_RSP;
    arg2 = 0;
    ret = cust_mproc_comm1(&arg1,&arg2);
    if(!ret)
    {
      for(i=0; i<100; i++)
      {
        msleep(10);
        if(pkt_ptr->process_status == QISDA_DIAGPKT_REQ_RSP_DONE)
          break;
      }
    }
    else
    {
      ret_val = ret;
      goto exit_err;
    }
    write_command = (nv_write_command*)pkt_ptr->piResponseBytes;
    if(pkt_ptr->process_status != QISDA_DIAGPKT_REQ_RSP_DONE ||
      write_command->nv_operation_status != 0)
    {
      MSG2("%s: (2)w als  nv: process_status = %d, nv_operation_status = %d (FAIL)",
        __func__, pkt_ptr->process_status, write_command->nv_operation_status);
      ret_val = -EIO;
      goto exit_err;
    }
    else
    {
      MSG2("%s: (2)w als  nv: process_status = %d, nv_operation_status = %d (PASS)",
        __func__, pkt_ptr->process_status, write_command->nv_operation_status);
    }
  }

  if(TST_BIT(p_cal_data->status, 1))
  {
    unsigned int prox = p_cal_data->prox.prox_far + (p_cal_data->prox.prox_near<<16);
    memset(pkt_ptr,0,sizeof(qisda_diagpkt_req_rsp_type));
    write_command = (nv_write_command*)pkt_ptr->piRequestBytes;
    write_command->nv_operation   = 0x27;
    write_command->nv_item_index  = PROX_NV_ITEM;
    write_command->item_data[0]   = (prox >>  0) & 0xFF;
    write_command->item_data[1]   = (prox >>  8) & 0xFF;
    write_command->item_data[2]   = (prox >> 16) & 0xFF;
    write_command->item_data[3]   = (prox >> 24) & 0xFF;
    arg1 = SMEM_PC_OEM_QISDA_DIAGPKT_REQ_RSP;
    arg2 = 0;
    ret = cust_mproc_comm1(&arg1,&arg2);
    if(!ret)
    {
      for(i=0; i<100; i++)
      {
        msleep(10);
        if(pkt_ptr->process_status == QISDA_DIAGPKT_REQ_RSP_DONE)
          break;
      }
    }
    else
    {
      ret_val = ret;
      goto exit_err;
    }
    write_command = (nv_write_command*)pkt_ptr->piResponseBytes;
    if(pkt_ptr->process_status != QISDA_DIAGPKT_REQ_RSP_DONE ||
      write_command->nv_operation_status != 0)
    {
      MSG2("%s: (3)w prox nv: process_status = %d, nv_operation_status = %d (FAIL)",
        __func__, pkt_ptr->process_status, write_command->nv_operation_status);
      ret_val = -EIO;
      goto exit_err;
    }
    else
    {
      MSG2("%s: (3)w prox nv: process_status = %d, nv_operation_status = %d (PASS)",
        __func__, pkt_ptr->process_status, write_command->nv_operation_status);
    }
  }

#if 1
  memset(pkt_ptr,0,sizeof(qisda_diagpkt_req_rsp_type));
  backup_command = (diagpkt_subsys_hdr_type_v2*)pkt_ptr->piRequestBytes;
  backup_command->command_code    = 0x80;
  backup_command->subsys_id       = 0x37;
  backup_command->subsys_cmd_code = 0x0001;
  arg1 = SMEM_PC_OEM_QISDA_DIAGPKT_REQ_RSP;
  arg2 = 0;
  ret = cust_mproc_comm1(&arg1,&arg2);
  if(!ret)
  {
    for(i=0; i<200; i++)
    {
      msleep(10);
      if(pkt_ptr->process_status == QISDA_DIAGPKT_REQ_RSP_DONE)
        break;
    }
  }
  else
  {
    ret_val = ret;
    goto exit_err;
  }
  backup_command = (diagpkt_subsys_hdr_type_v2*)pkt_ptr->piResponseBytes;
  if(pkt_ptr->process_status != QISDA_DIAGPKT_REQ_RSP_DONE ||
    backup_command->status != 0)
  {
    MSG2("%s: (4)backup nv: process_status = %d, status = %d (FAIL)",
      __func__, pkt_ptr->process_status, backup_command->status);
    ret_val = -EIO;
    goto exit_err;
  }
  else
  {
    MSG2("%s: (4)backup nv: process_status = %d, status = %d (PASS)",
      __func__, pkt_ptr->process_status, backup_command->status);
  }
#endif
  SET_BIT(p_cal_data->status,2);
  return;

exit_err:
  CLR_BIT(p_cal_data->status,2);
}
static void lsensor_calibration(struct lsensor_cal_data* p_cal_data)
{
  unsigned int m_ga_old, irq_old;
  int i, lux_count, dist_count;
  unsigned int lux_average, dist_average, lux_sum, dist_sum;
  unsigned int lux[LSENSOR_CALIBRATION_LOOP], dist[LSENSOR_CALIBRATION_LOOP];
  unsigned int lux_high, lux_low, dist_high, dist_low;
  struct lsensor_reg_data  ls_reg_old;

  p_cal_data->status = 0;

  irq_old = ls_drv.irq_enabled;
  lsensor_irq_onOff(0);

  m_ga_old = ls_drv.m_ga;
  memcpy(&ls_reg_old, &ls_reg, sizeof(ls_reg));

  ls_reg.r00.aien = 0;
  ls_reg.r00.pien = 0;
  lsensor_write_i2c_retry(0x39,(0x80|0x00),(uint8_t *)&ls_reg.r00,sizeof(ls_reg.r00));

  ls_reg.r00.pon = 1;
  ls_reg.r00.aen = 1;
  ls_reg.r00.pen = 1;
  ls_reg.r00.wen = 1;
  ls_reg.r00.aien = 0;
  ls_reg.r00.pien = 0;
  ls_reg.r01_0F.atime = 219;
  ls_reg.r01_0F.ptime = 255;
  ls_reg.r01_0F.wtime = 255;
  ls_reg.r01_0F.ppcount    = 8;
  ls_reg.r01_0F.r0F.again  = 2;
  ls_reg.r01_0F.r0F.pdiode = 2;
  ls_reg.r01_0F.r0F.pdrive = 1;
  lsensor_write_i2c_retry(0x39,(0xA0|0x01),(uint8_t *)&ls_reg.r01_0F,sizeof(ls_reg.r01_0F));
  lsensor_write_i2c_retry(0x39,(0x80|0x00),(uint8_t *)&ls_reg.r00,sizeof(ls_reg.r00));
  msleep(300);

  ls_drv.m_ga = 1024;

  lux_average = 0;
  dist_average = 0;
  lux_high  = 0;
  lux_low   = 0xFFFFFFFF;
  dist_high = 0;
  dist_low  = 0xFFFFFFFF;
  for(i=0; i<LSENSOR_CALIBRATION_LOOP; i++)
  {
    msleep(120);
    lsensor_update_info();
    lux[i]  = ls_info.m_lux;
    dist[i] = ls_info.pdata;
    lux_average  += ls_info.m_lux;
    dist_average += ls_info.pdata;
    MSG2("%02d = %04d %04d", i, ls_info.m_lux, ls_info.pdata);
    if(ls_info.m_lux >= lux_high)   lux_high = ls_info.m_lux;
    if(ls_info.m_lux <= lux_low)    lux_low  = ls_info.m_lux;
    if(ls_info.pdata >= dist_high)  dist_high = ls_info.pdata;
    if(ls_info.pdata <= dist_low)   dist_low  = ls_info.pdata;
  }
  lux_average  /= LSENSOR_CALIBRATION_LOOP;
  dist_average /= LSENSOR_CALIBRATION_LOOP;
  MSG2("Lux= %d~%d~%d, Pdata = %d~%d~%d (Low~Average~High)",
    lux_low, lux_average, lux_high, dist_low, dist_average, dist_high);

  lux_count   = 0;
  dist_count  = 0;
  lux_sum     = 0;
  dist_sum    = 0;
  for(i=0; i<LSENSOR_CALIBRATION_LOOP; i++)
  {
    if(lux[i] != lux_high && lux[i] != lux_low)
    {
      lux_count ++;
      lux_sum += lux[i];
    }
    if(dist[i] != dist_high && dist[i] != dist_low)
    {
      dist_count ++;
      dist_sum += dist[i];
    }
  }
  if(lux_count)
  {
    lux_sum /= lux_count;
  }
  else
  {
    MSG2("Lux have no middle value");
    lux_sum = lux_average;
  } 
  if(dist_count)
  {
    dist_sum /= dist_count;
  }
  else
  {
    MSG2("Pdata have no middle value");
    dist_sum = dist_average;
  } 
  MSG2("Calibration = %d %d, count = %d %d",
    lux_sum, dist_sum, lux_count, dist_count);

  {
    SET_BIT(p_cal_data->status,0);
    p_cal_data->als = lux_sum;
  }
  {
    SET_BIT(p_cal_data->status,1);
    p_cal_data->prox.prox_far  = dist_sum;
    p_cal_data->prox.prox_near = 0xFFFF;
  }

  ls_drv.m_ga = m_ga_old;
  ls_reg.r00.aien = 0;
  ls_reg.r00.pien = 0;
  lsensor_write_i2c_retry(0x39,(0x80|0x00),(uint8_t *)&ls_reg.r00,sizeof(ls_reg.r00));
  memcpy(&ls_reg, &ls_reg_old, sizeof(ls_reg));
  lsensor_write_i2c_retry(0x39,(0xA0|0x01),(uint8_t *)&ls_reg.r01_0F,sizeof(ls_reg.r01_0F));
  lsensor_write_i2c_retry(0x39,(0x80|0x00),(uint8_t *)&ls_reg.r00,sizeof(ls_reg.r00));
  if(irq_old)
    lsensor_irq_onOff(1);
}

static void lsensor_work_func(struct work_struct *work)
{
  const unsigned char gain[4] = {1,8,16,120};
  int i;

  if(!ls_drv.inited)
    return;

  if(ls_drv.in_suspend)
  {
    for(i=0; i<20; i++)
    {
      MSG("%s: in_suspend %d",__func__,i);
      msleep(20);
      if(!ls_drv.in_suspend)
      {
        MSG("%s: leave suspend",__func__);
        break;
      }
    }
  }

  lsensor_update_info();

  if(ls_drv.enable)
  {
    lsensor_update_result();

    if(!ls_drv.in_early_suspend)
      lsenosr_update_bkl();

    MSG3("(C)%05d (R)%05d [P]%05d [L]%05d [G]%d",
      ls_info.cdata, ls_info.irdata, ls_info.pdata, ls_info.m_lux, gain[ls_reg.r01_0F.r0F.again]);
  }

  if(ls_drv.enable & LSENSOR_EN_AP)
  {
    unsigned int light;
    if(ls_drv.millilux > 4000)
      light = 4000;
    else
      light = ls_drv.millilux;
    input_event(ls_drv.input_als, EV_MSC, MSC_RAW, light);
  	input_sync(ls_drv.input_als);
  }
  if(ls_drv.enable & PSENSOR_EN_AP)
  {
    if(ls_drv.distance_ap != ls_drv.distance)
    {
      input_report_abs(ls_drv.input_prox, ABS_DISTANCE, ls_drv.distance);
      input_sync(ls_drv.input_prox);
      MSG2("prox %d->%d",ls_drv.distance_ap,ls_drv.distance);
      ls_drv.distance_ap = ls_drv.distance;
    }
  }

  if(ls_drv.info_waiting)
  {
    ls_drv.info_waiting = 0;
    complete(&(ls_drv.info_comp));
  }

    lsensor_clr_irq();

  if(ls_drv.enable & PSENSOR_EN_AP)
  {
    lsensor_irq_onOff(1);
  }
  else if(ls_drv.in_early_suspend)
  {
  }
  else if(ls_drv.enable)
  {
    lsensor_irq_onOff(1);
  }

}

static irqreturn_t lsensor_irqhandler(int irq, void *dev_id)
{
  lsensor_irq_onOff(0);
  if(!ls_drv.inited)
  {
    MSG2("%s, lsensor not inited! Disable irq!", __func__);
    disable_irq_nosync(MSM_GPIO_TO_INT(ls_drv.intr_gpio));
  }
  else
    schedule_work(&ls_work);
  return IRQ_HANDLED;
}
void lsensor_enable_onOff(unsigned int mode, unsigned int onOff)
{
  static unsigned int als_pm_on = 0, prox_pm_on = 0;

  MSG("%s+ mode=0x%02X onOff=0x%02X", __func__,mode,onOff);
  mutex_lock(&lsensor_enable_lock);

  if(mode==LSENSOR_EN_AP || mode==LSENSOR_EN_LCD ||
    mode==LSENSOR_EN_ENG || mode==PSENSOR_EN_AP )
  {
    if(onOff) ls_drv.enable |=   mode;
    else      ls_drv.enable &= (~mode);
  }

  if(ls_drv.enable & (LSENSOR_EN_AP | LSENSOR_EN_LCD))
  {
    if(!als_pm_on)
    {
      als_pm_on = 1;
      PM_LOG_EVENT(PM_LOG_ON, PM_LOG_SENSOR_ALS);
    }
  }
  else
  {
    if(als_pm_on)
    {
      als_pm_on = 0;
      PM_LOG_EVENT(PM_LOG_OFF, PM_LOG_SENSOR_ALS);
    }
  }
  if(ls_drv.enable & PSENSOR_EN_AP)
  {
    if(!prox_pm_on)
    {
      prox_pm_on = 1;
      PM_LOG_EVENT(PM_LOG_ON, PM_LOG_SENSOR_PROXIMITY);
    }
  }
  else
  {
    if(prox_pm_on)
    {
      prox_pm_on = 0;
      PM_LOG_EVENT(PM_LOG_OFF, PM_LOG_SENSOR_PROXIMITY);
    }
  }

  if(mode != LSENSOR_EN_ENG)
  {
    lsensor_irq_onOff(0);

    if(! ls_drv.enable)
    {
      ls_reg.r00.pon = 0;     
      ls_reg.r00.aen = 0;     
      ls_reg.r00.pen = 0;     
      ls_reg.r00.aien = 0;    
      ls_reg.r00.pien = 0;    
    }
    else  
    {
      if(ls_drv.in_early_suspend) 
      {
        if(ls_drv.enable & PSENSOR_EN_AP) 
        {
          ls_reg.r00.pon = 1;     
          ls_reg.r00.aen = 1;     
          ls_reg.r00.pen = 1;     
          ls_reg.r00.wen = 1;
          ls_reg.r00.aien = 0;    
          ls_reg.r00.pien = 1;    
          ls_reg.r01_0F.atime = 255;
          ls_reg.r01_0F.ptime = 255;
          ls_reg.r01_0F.wtime = 238;
        }
        else  
        {
          ls_reg.r00.pon = 0;     
          ls_reg.r00.aen = 0;     
          ls_reg.r00.pen = 0;     
          ls_reg.r00.aien = 0;    
          ls_reg.r00.pien = 0;    
        }
      }
      else  
      {
        ls_reg.r00.pon = 1;     
        ls_reg.r00.aen = 1;     
        ls_reg.r00.pen = (ls_drv.enable & PSENSOR_EN_AP) ? 1:0; 
        ls_reg.r00.wen = 1;     
        ls_reg.r00.aien = 1;    
        ls_reg.r00.pien = 0;    
        
        ls_reg.r01_0F.ptime = 255;
        if(ls_drv.enable & (LSENSOR_EN_AP|LSENSOR_EN_LCD))
        {
          if(ls_drv.enable & PSENSOR_EN_AP) 
          {
            ls_reg.r01_0F.atime = 238;
            ls_reg.r01_0F.wtime = 255;
          }
          else  
          {
            ls_reg.r01_0F.atime = 183;
            ls_reg.r01_0F.wtime = 183;
          }        
        }
        else  
        {
          ls_reg.r01_0F.atime = 255;
          ls_reg.r01_0F.wtime = 238;
        }
      }
    }
    lsensor_write_i2c_retry(0x39,(0xA0|0x01),(uint8_t *)&ls_reg.r01_0F,sizeof(ls_reg.r01_0F));
    lsensor_write_i2c_retry(0x39,(0x80|0x00),(uint8_t *)&ls_reg.r00,sizeof(ls_reg.r00));
  }

    lsensor_clr_irq();

  if(ls_drv.enable & PSENSOR_EN_AP)
  {
    lsensor_irq_onOff(1);
  }
  else if(ls_drv.in_early_suspend)
  {
  }
  else if(ls_drv.enable)
  {
    lsensor_irq_onOff(1);
  }

  mutex_unlock(&lsensor_enable_lock);
  MSG("%s-", __func__);
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
void lsensor_i2c_test(unsigned char *bufLocal, size_t count)
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
      return;
    }

    msgs[0].addr = id;
    msgs[0].flags = 0;
    msgs[0].buf = &reg[0];
    msgs[0].len = 1;

    msgs[1].addr = id;
    msgs[1].flags = I2C_M_RD;
    msgs[1].buf = &dat[0];
    msgs[1].len = len;

    i2c_ret = i2c_transfer(ls_drv.client->adapter, msgs,2);
    if(i2c_ret != 2)
    {
      MSG2("R %02X:%02X(%02d) Fail: ret=%d", id,reg[0],len,i2c_ret);
      return;
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
      return;
    }

    msgs[0].addr = id;
    msgs[0].flags = 0;
    msgs[0].buf = &reg[0];
    msgs[0].len = 2;

    msgs[1].addr = id;
    msgs[1].flags = I2C_M_RD;
    msgs[1].buf = &dat[0];
    msgs[1].len = len;

    i2c_ret = i2c_transfer(ls_drv.client->adapter, msgs,2);
    if(i2c_ret != 2)
    {
      MSG2("R %02X:%02X%02X(%02d) Fail (ret=%d)", id,reg[0],reg[1],len,i2c_ret);
      return;
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
      return;
    }
    len = len/2;
    if(len >= sizeof(dat))
    {
      MSG2("W %02X Fail (too many data)", id);
      return;
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

    i2c_ret = i2c_transfer(ls_drv.client->adapter, msgs,1);
    MSG2("W %02X = %s", id, i2c_ret==1 ? "Pass":"Fail");
  }
  else
  {
    MSG2("rd: r40000B   (addr=40(7bit), reg=00, read count=11");
    MSG2("Rd: R2C010902 (addr=2C(7bit), reg=0109, read count=2");
    MSG2("wr: w40009265CA (addr=40(7bit), reg & data=00,92,65,CA...");
  }
}
static ssize_t lsensor_ctrl_show(struct device *dev, struct device_attribute *attr, char *buf)
{
  return 0;
}
static ssize_t lsensor_ctrl_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
  char bufLocal[QSD_BAT_BUF_LENGTH];
  char reg_00_19[26];
  int  ret;
  static struct lsensor_cal_data cal_data;

  if(!ls_drv.inited)
    goto exit;

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
        MSG2("Dynamic Log Off");
        lsensor_log_on = 0;
        lsensor_log3_on = 0;
      }
      else if(bufLocal[1]=='1')
      {
        MSG2("Dynamic Log On");
        lsensor_log_on = 1;
      }
      else if(bufLocal[1]=='3')
      {
        MSG2("Dynamic Log On");
        lsensor_log3_on = 1;
      }
      break;
    case 'm':
      if(bufLocal[1]=='0')
      {
        MSG2("Backlight Mode = [USER]");
        lsensor_enable_onOff(LSENSOR_EN_LCD,0);
        lcdc_lsensor_set_backlight(0, ls_drv.bkl_table[ls_drv.bkl_idx].level);
      }
      else if(bufLocal[1]=='1')
      {
        MSG2("Backlight Mode = [SENSOR]");
        lsensor_enable_onOff(LSENSOR_EN_LCD,1);
        lcdc_lsensor_set_backlight(1, ls_drv.bkl_table[ls_drv.bkl_idx].level);
      }
      break;

    case 'a':
      switch(bufLocal[1])
      {
        case '0':
          MSG2("Calibration");
          lsensor_calibration(&cal_data);
          break;
        case '1':
          MSG2("Write ALS  NV");
          cal_data.status = 1;  
          lsensor_write_nv_data(&cal_data);
          if(TST_BIT(cal_data.status,2))
          {
            ls_drv.als_nv  = cal_data.als;
            MSG2("als_nv=0x%X", ls_drv.als_nv);
          }
          break;
        case '2':
          MSG2("Write PROX FAR NV");
          cal_data.prox.prox_near = (ls_drv.prox_nv>>16) & 0xFFFF;
          cal_data.prox.prox_far  = cal_data.prox.prox_far;
          cal_data.status = 2;  
          lsensor_write_nv_data(&cal_data);
          if(TST_BIT(cal_data.status,2))
          {
            ls_drv.prox_nv = cal_data.prox.prox_far + (cal_data.prox.prox_near<<16);
            MSG2("prox_nv=0x%X", ls_drv.prox_nv);
          }
          break;
        case '3':
          MSG2("Write ALS + PROX FAR NV");
          cal_data.prox.prox_near = (ls_drv.prox_nv>>16) & 0xFFFF;
          cal_data.prox.prox_far  = cal_data.prox.prox_far;
          cal_data.status = 3;  
          lsensor_write_nv_data(&cal_data);
          if(TST_BIT(cal_data.status,2))
          {
            ls_drv.als_nv  = cal_data.als;
            ls_drv.prox_nv = cal_data.prox.prox_far + (cal_data.prox.prox_near<<16);
            MSG2("als_nv=0x%X prox_nv=0x%X", ls_drv.als_nv, ls_drv.prox_nv);
          }
          break;
        case '4':
          MSG2("Clear ALS + PROX NV");
          cal_data.als = 0xFFFFFFFF;
          cal_data.prox.prox_near = 0xFFFF;
          cal_data.prox.prox_far  = 0xFFFF;
          cal_data.status = 3;  
          lsensor_write_nv_data(&cal_data);
          if(TST_BIT(cal_data.status,2))
          {
            ls_drv.als_nv  = 0xFFFFFFFF;
            ls_drv.prox_nv = 0xFFFFFFFF;
            MSG2("als_nv=0xFFFFFFFF prox_nv=0xFFFFFFFF");
          }
          break;
        case '5':
          MSG2("Read NV");
          lsensor_read_nv_data(&cal_data);
          ls_drv.als_nv   = cal_data.als;
          ls_drv.prox_nv  = cal_data.prox.prox_far + (cal_data.prox.prox_near<<16);
          MSG2("%s ALS=%d, PROX_FAR=%d, PROX_NEAR=%d",
            __func__, cal_data.als, cal_data.prox.prox_far, cal_data.prox.prox_near);
          break;
        case '6':
          MSG2("Update m_ga");
          cal_data.status = 1;  
          lsensor_update_equation_parameter(&cal_data);
          break;
        case '7':
          MSG2("Update prox_threshold");
          cal_data.status = 2;  
          lsensor_update_equation_parameter(&cal_data);
          break;
        case '8':
          MSG2("Write PROX NEAR NV");
          cal_data.prox.prox_near = cal_data.prox.prox_far; 
          cal_data.prox.prox_far  = ls_drv.prox_nv & 0xFFFF;
          cal_data.status = 2;  
          lsensor_write_nv_data(&cal_data);
          if(TST_BIT(cal_data.status,2))
          {
            ls_drv.prox_nv = cal_data.prox.prox_far + (cal_data.prox.prox_near<<16);
            MSG2("prox_nv=0x%X", ls_drv.prox_nv);
          }
          break;
        default:
          MSG2("[0]cal, [1]write als, [2]write prox far, [3]write als+prox far, \n[4]clear nv, [5]read nv, [6]update m_ga, [7]update prox_threshold [8]write prox near");
          break;
      }
      break;

    case 'd':
      switch(bufLocal[1])
      {
        case '0':
          MSG2("Disable Irq");
          disable_irq(MSM_GPIO_TO_INT(ls_drv.intr_gpio));
          break;
        case '1':
          MSG2("Enable Irq");
          enable_irq(MSM_GPIO_TO_INT(ls_drv.intr_gpio));
          break;
        case '2':
          MSG2("Disable Wake");
          set_irq_wake(MSM_GPIO_TO_INT(ls_drv.intr_gpio), 0);
          break;
        case '3':
          MSG2("Enable Wake");
          set_irq_wake(MSM_GPIO_TO_INT(ls_drv.intr_gpio), 1);
          break;
      }
      break;    

    case 'e':
      MSG2("lsensor enable = %c", bufLocal[1]);
      switch(bufLocal[1])
      {
        case '0':
          lsensor_enable_onOff(LSENSOR_EN_AP,0);
          break;
        case '1':
          lsensor_enable_onOff(LSENSOR_EN_AP,1);
          break;
        case '2':
          lsensor_enable_onOff(PSENSOR_EN_AP,0);
          break;
        case '3':
          lsensor_enable_onOff(PSENSOR_EN_AP,1);
          break;
      }
      lsensor_update_info();
      MSG2("Time: (T)%5d (A)%5d (P)%5d (W)%5d",
        ls_info.t_ms, ls_info.a_ms, ls_info.p_ms, ls_info.w_ms);
      break;

    case 'b': 
      ret = lsensor_read_i2c_retry(0x39,(0xA0|0x00),&reg_00_19[0],sizeof(reg_00_19));
      MSG2("read sensor regs: %s", ret==2?"PASS":"FAIL");
      MSG2("[00-03]%02X %02X %02X %02X (enable...)",  reg_00_19[0],reg_00_19[1],reg_00_19[2],reg_00_19[3]);
      MSG2("[04-07]%02X %02X %02X %02X (ait)",        reg_00_19[4],reg_00_19[5],reg_00_19[6],reg_00_19[7]);
      MSG2("[08-0B]%02X %02X %02X %02X (pit)",        reg_00_19[8],reg_00_19[9],reg_00_19[10],reg_00_19[11]);
      MSG2("[0C-0F]%02X %02X %02X %02X (intr..)",     reg_00_19[12],reg_00_19[13],reg_00_19[14],reg_00_19[15]);
      MSG2("[10-12]%02X %02X %02X %02X (..rev...)",   reg_00_19[16],reg_00_19[17],reg_00_19[18],reg_00_19[19]);
      MSG2("[14-15]%02X %02X (cdata)",    reg_00_19[20],reg_00_19[21]);
      MSG2("[16-17]%02X %02X (irdata)",   reg_00_19[22],reg_00_19[23]);
      MSG2("[18-19]%02X %02X (pdata)",    reg_00_19[24],reg_00_19[25]);
      break;

    case 'c': 
      MSG2("inited   = %d",     ls_drv.inited);
      MSG2("enable   = 0x%X",   ls_drv.enable);
      MSG2("opened   = %d",     ls_drv.opened);
      MSG2("eng_mode = %d",     ls_drv.eng_mode);
      MSG2("irq_enabled  = %d", ls_drv.irq_enabled);
      MSG2("in_early_suspend = %d", ls_drv.in_early_suspend);
      MSG2("in_suspend = %d",   ls_drv.in_suspend);
      MSG2("m_ga     = %d",     ls_drv.m_ga);
      MSG2("info_waiting = %d", ls_drv.info_waiting);
      MSG2("lux_history  = %d %d %d",
        ls_drv.lux_history[0], ls_drv.lux_history[1], ls_drv.lux_history[2]);
      MSG2("bkl_idx  = %d",     ls_drv.bkl_idx);
      MSG2("prox_history = %d", ls_drv.prox_history);
      MSG2("prox_threshold = %d",ls_drv.prox_threshold);
      MSG2("lux      = %d",     ls_drv.millilux);
      MSG2("distance = %d",     ls_drv.distance);
      MSG2("als_nv   = %d",     ls_drv.als_nv);
      MSG2("prox_nv far  = %d", ls_drv.prox_nv & 0xFFFF);
      MSG2("prox_nv near = %d", (ls_drv.prox_nv>>16) & 0xFFFF);
      MSG2("jiff wait = %ld, now = %ld",
        ls_drv.jiff_update_bkl_wait_time, jiffies);
      MSG2("approached = %d", atomic_read(&psensor_approached));
      break;

    case 'i':
      lsensor_i2c_test(bufLocal, count);
      break;

    default:
      break;
  }
exit:
  return count;
}
static struct device_attribute lsensor_ctrl_attrs[] = {
  __ATTR(ctrl, 0664, lsensor_ctrl_show, lsensor_ctrl_store),
};

static void lsensor_early_suspend_func(struct early_suspend *h)
{
  MSG("%s+", __func__);
  ls_drv.in_early_suspend = 1;
  flush_work(&ls_work);
  lsensor_enable_onOff(0,0);
  if(ls_drv.info_waiting)
  {
    ls_drv.info_waiting = 0;
    complete(&(ls_drv.info_comp));
  }
  MSG("%s-", __func__);
}
static void lsensor_late_resume_func(struct early_suspend *h)
{
  MSG("%s+", __func__);
  if(ls_drv.enable & LSENSOR_EN_LCD)  
    lcdc_lsensor_set_backlight(1, ls_drv.bkl_table[ls_drv.bkl_idx].level);
  ls_drv.in_early_suspend = 0;
  ls_drv.jiff_update_bkl_wait_time = jiffies;
  lsensor_enable_onOff(0,0);
  MSG("%s-", __func__);
}
static struct early_suspend lsensor_early_suspend = {
  .level    = EARLY_SUSPEND_LEVEL_BLANK_SCREEN,
  .suspend  = lsensor_early_suspend_func,
  .resume   = lsensor_late_resume_func,
};

static int lsensor_i2c_suspend(struct i2c_client *client, pm_message_t state)
{
  MSG("%s+", __func__);
    lsensor_clr_irq();
  flush_work(&ls_work);
  ls_drv.in_suspend = 1;
  MSG("%s-", __func__);
  return 0;
}
static int lsensor_i2c_resume(struct i2c_client *client)
{
  MSG("%s+", __func__);
  ls_drv.in_suspend = 0;
  if(ls_drv.enable & PSENSOR_EN_AP)
    lsensor_enable_onOff(0,0);
  MSG("%s-", __func__);
  return 0;
}
static int lsensor_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
  int ret=0;
  struct lsensor_cal_data cal_data;
  MSG("%s+", __func__);
  ls_drv.client = client;

  #if 0
  for(i=0; i<ARRAY_SIZE(lsensor_ctrl_attrs); i++)
  {
    ret = device_create_file(&ls_drv.client->dev, &lsensor_ctrl_attrs[i]);
    if(ret) MSG2("%s: create FAIL, ret=%d",lsensor_ctrl_attrs[i].attr.name,ret);
  }
  #endif

  lsensor_read_nv_data(&cal_data);
  ls_drv.als_nv   = cal_data.als;
  ls_drv.prox_nv  = cal_data.prox.prox_far + (cal_data.prox.prox_near<<16);
  MSG2("%s ALS=%d, PROX_FAR=%d, PROX_NEAR=%d",
    __func__, cal_data.als, cal_data.prox.prox_far, cal_data.prox.prox_near);

  cal_data.status = 3;
  lsensor_update_equation_parameter(&cal_data);

  MSG("Init TAOS register");
  lsensor_write_i2c_retry(0x39,(0x80|0x00),(uint8_t *)&ls_reg.r00,sizeof(ls_reg.r00));
  lsensor_write_i2c_retry(0x39,(0xA0|0x01),(uint8_t *)&ls_reg.r01_0F,sizeof(ls_reg.r01_0F));
  lsensor_clr_irq();
  MSG("%s-", __func__);
  return ret;
}

void lsensor_i2c_shutdown(struct i2c_client *client)
{
  MSG("%s", __func__);
  ls_reg.r00.pon  = 0;
  ls_reg.r00.pen  = 0;
  ls_reg.r00.wen  = 0;
  ls_reg.r00.aen  = 0;
  ls_reg.r00.aien = 0;
  ls_reg.r00.pien = 0;
  lsensor_write_i2c_retry(0x39,(0x80|0x00),(uint8_t *)&ls_reg.r00,sizeof(ls_reg.r00));
  lsensor_clr_irq();
}

static const struct i2c_device_id lsensor_i2c_id[] = {
  { "lsensor_i2c", 0 },
  { }
};
static struct i2c_driver lsensor_i2c_driver = {
  .driver = {
    .owner = THIS_MODULE,
    .name = "lsensor_i2c"
  },
  .id_table = lsensor_i2c_id,
  .probe    = lsensor_i2c_probe,
  .suspend  = lsensor_i2c_suspend,
  .resume   = lsensor_i2c_resume,
  .shutdown = lsensor_i2c_shutdown,
};

static int lsensor_misc_open(struct inode *inode_p, struct file *fp)
{
  ls_drv.opened ++;
  MSG("%s    <== [%d] (%04d)",__func__,ls_drv.opened,current->pid);
  return 0;
}
static int lsensor_misc_release(struct inode *inode_p, struct file *fp)
{
  ls_drv.opened --;
  MSG("%s <== [%d] (%04d)\n",__func__,ls_drv.opened,current->pid);
  return 0;
}
static int lsensor_misc_ioctl(struct inode *inode_p, struct file *fp, unsigned int cmd, unsigned long arg)
{
  unsigned int light;
  unsigned int distance;
  int ret = 0;
  long timeout;
  static unsigned int p_old = 1;
  struct lsensor_cal_data cal_data;

  if(_IOC_TYPE(cmd) != LSENSOR_IOC_MAGIC)
  {
    MSG2("%s: Not LSENSOR_IOC_MAGIC", __func__);
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
    case LSENSOR_IOC_ENABLE:
      if(ls_drv.eng_mode)
      {
        MSG2("%s: LSENSOR_IOC_ENABLE Skip (Eng Mode)", __func__);
        ret = -EFAULT;
      }
      else
      {
        MSG("%s: (%04d) LSENSOR_IOC_ENABLE", __func__,current->pid);
        lsensor_enable_onOff(LSENSOR_EN_AP,1);
      }
      break;

    case LSENSOR_IOC_DISABLE:
      if(ls_drv.eng_mode)
      {
        MSG2("%s: LSENSOR_IOC_DISABLE Skip (Eng Mode)", __func__);
        ret = -EFAULT;
      }
      else
      {
        MSG("%s: (%04d) LSENSOR_IOC_DISABLE", __func__,current->pid);
        lsensor_enable_onOff(LSENSOR_EN_AP,0);
      }
      break;

    case LSENSOR_IOC_GET_STATUS:
      light = ls_drv.millilux;
      MSG("%s: (%04d) LSENSOR_IOC_GET_STATUS = %d", __func__,light,current->pid);
      if(copy_to_user((void __user*) arg, &light, _IOC_SIZE(cmd)))
      {
        MSG2("%s: LSENSOR_IOC_GET_STATUS Fail!", __func__);
        ret = -EFAULT;
      }
      break;

    case LSENSOR_IOC_CALIBRATION:
      lsensor_calibration(&cal_data);
      if(TST_BIT(cal_data.status,0) && TST_BIT(cal_data.status,1))
      {
        cal_data.prox.prox_near = (ls_drv.prox_nv>>16) & 0xFFFF;
        cal_data.prox.prox_far  = cal_data.prox.prox_far;
        cal_data.status = 3;
        lsensor_write_nv_data(&cal_data);
        if(TST_BIT(cal_data.status,2))
        {
          ls_drv.als_nv   = cal_data.als;
          ls_drv.prox_nv  = cal_data.prox.prox_far + (cal_data.prox.prox_near<<16);
        }
      }
      MSG("%s: LSENSOR_IOC_CALIBRATION = %d (%04d)", __func__,light,current->pid);
      if(copy_to_user((void __user*) arg, &cal_data, _IOC_SIZE(cmd)))
      {
        MSG2("%s: LSENSOR_IOC_CALIBRATION Fail!", __func__);
        ret = -EFAULT;
      }
      break;

    case LSENSOR_IOC_CAL_ALS:
      lsensor_calibration(&cal_data);
      if(TST_BIT(cal_data.status,0))
      {
        cal_data.status = 1;
        lsensor_write_nv_data(&cal_data);
        if(TST_BIT(cal_data.status,2))
        {
          ls_drv.als_nv   = cal_data.als;
        }
      }
      MSG("%s: LSENSOR_IOC_CAL_ALS = %d (%04d)", __func__,light,current->pid);
      if(copy_to_user((void __user*) arg, &cal_data, _IOC_SIZE(cmd)))
      {
        MSG2("%s: LSENSOR_IOC_CAL_ALS Fail!", __func__);
        ret = -EFAULT;
      }
      break;

    case LSENSOR_IOC_CAL_PROX_FAR:
      lsensor_calibration(&cal_data);  
      if(TST_BIT(cal_data.status,1))
      {
        cal_data.prox.prox_near = (ls_drv.prox_nv>>16) & 0xFFFF;
        cal_data.prox.prox_far  = cal_data.prox.prox_far;
        cal_data.status = 2;  
        lsensor_write_nv_data(&cal_data);
        if(TST_BIT(cal_data.status,2))  
        {
          ls_drv.prox_nv  = cal_data.prox.prox_far + (cal_data.prox.prox_near<<16);
        }
      }
      MSG("%s: LSENSOR_IOC_CAL_PROX = %d (%04d)", __func__,light,current->pid);
      if(copy_to_user((void __user*) arg, &cal_data, _IOC_SIZE(cmd)))
      {
        MSG2("%s: LSENSOR_IOC_CAL_PROX Fail!", __func__);
        ret = -EFAULT;
      }
      break;

    case LSENSOR_IOC_CAL_PROX_NEAR:
      lsensor_calibration(&cal_data);  
      if(TST_BIT(cal_data.status,1))
      {
        cal_data.prox.prox_near = cal_data.prox.prox_far; 
        cal_data.prox.prox_far  = ls_drv.prox_nv & 0xFFFF;
        cal_data.status = 2;  
        lsensor_write_nv_data(&cal_data);
        if(TST_BIT(cal_data.status,2))  
        {
          ls_drv.prox_nv  = cal_data.prox.prox_far + (cal_data.prox.prox_near<<16);
        }
      }
      MSG("%s: LSENSOR_IOC_CAL_PROX = %d (%04d)", __func__,light,current->pid);
      if(copy_to_user((void __user*) arg, &cal_data, _IOC_SIZE(cmd)))
      {
        MSG2("%s: LSENSOR_IOC_CAL_PROX Fail!", __func__);
        ret = -EFAULT;
      }
      break;

    case LSENSOR_IOC_CAL_WRITE:
      MSG("%s: LSENSOR_IOC_CAL_WRITE", __func__);

      if(copy_from_user(&cal_data, (void __user*) arg, _IOC_SIZE(cmd)))
      {
        MSG2("%s: LSENSOR_IOC_CAL_WRITE Fail", __func__);
        ret = -EFAULT;
      }
      else
      {
        MSG2("%s: LSENSOR_IOC_CAL_WRITE, als=%d, prox_far=0x%04X, prox_near=0x%04X, status=%d",
          __func__, cal_data.als, cal_data.prox.prox_far, cal_data.prox.prox_near, cal_data.status);
        if(TST_BIT(cal_data.status,0))
          ls_drv.als_nv   = cal_data.als;
        if(TST_BIT(cal_data.status,1))
          ls_drv.prox_nv  = cal_data.prox.prox_far + (cal_data.prox.prox_near<<16);
        lsensor_write_nv_data(&cal_data);
        if(!TST_BIT(cal_data.status,2))
          ret = -EFAULT;
      }
      break;

    case PSENSOR_IOC_ENABLE:
      if(ls_drv.eng_mode)
      {
        MSG2("%s: PSENSOR_IOC_ENABLE Skip (Eng Mode)", __func__);
        ret = -EFAULT;
      }
      else
      {
        MSG2("%s: (%04d) PSENSOR_IOC_ENABLE, prox %d", __func__,current->pid,ls_drv.distance);
        lsensor_enable_onOff(PSENSOR_EN_AP,1);
        input_report_abs(ls_drv.input_prox, ABS_DISTANCE, ls_drv.distance);
        input_sync(ls_drv.input_prox);
        ls_drv.distance_ap = ls_drv.distance;
      }
      break;

    case PSENSOR_IOC_DISABLE:
      if(ls_drv.eng_mode)
      {
        MSG2("%s: PSENSOR_IOC_DISABLE Skip (Eng Mode)", __func__);
        ret = -EFAULT;
      }
      else
      {
        MSG2("%s: (%04d) PSENSOR_IOC_DISABLE, prox %d->1", __func__,current->pid,ls_drv.distance_old);
        lsensor_enable_onOff(PSENSOR_EN_AP,0);
        ls_drv.distance = 1;
        ls_drv.distance_old = 1;
        ls_drv.distance_ap  = 1;
        ls_drv.prox_history = 0;
        input_report_abs(ls_drv.input_prox, ABS_DISTANCE, ls_drv.distance);
        input_sync(ls_drv.input_prox);
        atomic_set(&psensor_approached, 0);
      }
      break;

    case PSENSOR_IOC_GET_STATUS:
      distance = ls_drv.distance;
      if(p_old != distance)
      {
        MSG2("%s: (%04d) prox %d->%d (AP Get)",__func__,current->pid,p_old,distance);
        p_old = distance;
      }
      else
      {
        MSG("%s: (%04d) PSENSOR_IOC_GET_STATUS = %d",__func__,distance,current->pid);
      }
      if(copy_to_user((void __user*) arg, &distance, _IOC_SIZE(cmd)))
      {
        MSG2("%s: PSENSOR_IOC_GET_STATUS Fail", __func__);
        ret = -EFAULT;
      }
      break;

    case LSENSOR_IOC_ENG_ENABLE:
      MSG("%s: LSENSOR_IOC_ENG_ENABLE", __func__);
      ls_drv.eng_mode = 1;
      lsensor_enable_onOff(LSENSOR_EN_ENG,1);
      atomic_set(&psensor_approached, 0);
      break;

    case LSENSOR_IOC_ENG_DISABLE:
      MSG("%s: LSENSOR_IOC_ENG_DISABLE", __func__);
      ls_drv.eng_mode = 0;
      lsensor_enable_onOff(LSENSOR_EN_ENG,0);
      break;

    case LSENSOR_IOC_ENG_CTL_R:
      if(!ls_drv.eng_mode)
      {
        MSG2("%s: LSENSOR_IOC_ENG_CTL_R Skip (Not Eng Mode)", __func__);
        ret = -EFAULT;
      }
      else
      {
        MSG("%s: LSENSOR_IOC_ENG_CTL_R", __func__);
        lsensor_eng_get_reg();
        lsensor_eng_data_log();
        if(copy_to_user((void __user*) arg, &ls_eng, _IOC_SIZE(cmd)))
        {
          MSG("%s: LSENSOR_IOC_ENG_CTL_R Fail", __func__);
          ret = -EFAULT;
        }
      }
      break;

    case LSENSOR_IOC_ENG_CTL_W:
      if(!ls_drv.eng_mode)
      {
        MSG2("%s: LSENSOR_IOC_ENG_CTL_W Skip (Not Eng Mode)", __func__);
        ret = -EFAULT;
      }
      else
      {
        MSG("%s: LSENSOR_IOC_ENG_CTL_W", __func__);
        if(copy_from_user(&ls_eng, (void __user*) arg, _IOC_SIZE(cmd)))
        {
          MSG2("%s: LSENSOR_IOC_ENG_CTL_W Fail", __func__);
          ret = -EFAULT;
        }
        else
        {
          lsensor_eng_data_log();
          lsensor_eng_set_reg();
        }
      }
      break;

    case LSENSOR_IOC_ENG_INFO:
      if(!ls_drv.eng_mode)
      {
        MSG("%s: LSENSOR_IOC_ENG_INFO Skip (Not Eng Mode)", __func__);
        ret = -EFAULT;
      }
      else
      {
        MSG("%s: LSENSOR_IOC_ENG_INFO", __func__);
        if(ls_drv.info_waiting)
        {
          MSG2("ERROR!!! info_waiting was TRUE before use");
        }
        ls_drv.info_waiting = 1;
        INIT_COMPLETION(ls_drv.info_comp);
        timeout = wait_for_completion_timeout(&(ls_drv.info_comp), 10*HZ);
        if(!timeout)
        {
          MSG2("ERROR!!! info_comp timeout");
          ls_drv.info_waiting = 0;
        }
        lsensor_info_data_log();
        if(copy_to_user((void __user*) arg, &ls_info, _IOC_SIZE(cmd)))
        {
          MSG2("%s: LSENSOR_IOC_ENG_INFO Fail", __func__);
          ret = -EFAULT;
        }
      }
      break;

    default:
      MSG("%s: unknown ioctl = 0x%X", __func__,cmd);
      break;
  }

  return ret;
}
static struct file_operations lsensor_misc_fops = {
  .owner    = THIS_MODULE,
  .open     = lsensor_misc_open,
  .release  = lsensor_misc_release,
  .ioctl    = lsensor_misc_ioctl,
};
static struct miscdevice lsensor_misc_device = {
  .minor  = MISC_DYNAMIC_MINOR,
  .name   = "lsensor_taos",
  .fops   = &lsensor_misc_fops,
};

static int __init lsensor_init(void)
{
  int ret=0, err=0;

  printk("BootLog, +%s\n", __func__);

  if(system_rev == TOUCAN_EVT2_Band125 || system_rev == TOUCAN_EVT2_Band148)
    ls_drv.intr_gpio = LSENSOR_GPIO_INT_EVT2;
  else
    ls_drv.intr_gpio = LSENSOR_GPIO_INT;
  MSG2("intr_gpio = %d (system_rev = %d)",ls_drv.intr_gpio,system_rev);
  gpio_tlmm_config(GPIO_CFG(ls_drv.intr_gpio, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);

  if(system_rev == TOUCAN_EVT2_Band125 || system_rev == TOUCAN_EVT2_Band148)
  {
  }
  else
  {
    MSG2("HW Test, set gpio 28 = i/p, 2mA pull down");
    gpio_tlmm_config(GPIO_CFG(LSENSOR_GPIO_INT_EVT2, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
  }

  ret = i2c_add_driver(&lsensor_i2c_driver);
  if(ret < 0)
  {
    MSG2("%s: i2c_add_driver Fail, ret=%d", __func__, ret);
    err = 3;  goto exit_error;
  }

  ls_drv.info_waiting = 0;
  init_completion(&(ls_drv.info_comp));

  INIT_WORK(&ls_work, lsensor_work_func);

  ls_drv.jiff_update_bkl_wait_time = jiffies;

  ret = request_irq(MSM_GPIO_TO_INT(ls_drv.intr_gpio), &lsensor_irqhandler, IRQF_TRIGGER_LOW, "lsensor_taos", NULL);
  set_irq_wake(MSM_GPIO_TO_INT(ls_drv.intr_gpio), 1);

  ls_drv.inited = 1;

  ret = misc_register(&lsensor_misc_device);
  if(ret)
  {
    MSG2("%s: lsensor misc_register Fail, ret=%d", __func__, ret);
    err = 4;  goto exit_error;
  }

  #if 1
  ret = device_create_file(lsensor_misc_device.this_device, &lsensor_ctrl_attrs[0]);
  if(ret) MSG2("%s: create FAIL, ret=%d",lsensor_ctrl_attrs[0].attr.name,ret);
  #endif

  ls_drv.input_als = input_allocate_device();
  if(ls_drv.input_als)
  {
    MSG2("als: input_allocate_device: PASS");
  }
  else
  {
    MSG2("als: input_allocate_device: FAIL");   
    ret = -1;
    goto exit_error;    
  }
  input_set_capability(ls_drv.input_als, EV_MSC, MSC_RAW);
  ls_drv.input_als->name = "lsensor_taos";
  ret = input_register_device(ls_drv.input_als);
  if(ret)
  {
    MSG2("als: input_register_device: FAIL=%d",ret);
    goto exit_error;
  }
  else
  {
    MSG2("als: input_register_device: PASS");
  }

  ls_drv.input_prox = input_allocate_device();
  if(ls_drv.input_prox)
  {
    MSG2("prox: input_allocate_device: PASS");
  }
  else
  {
    MSG2("prox: input_allocate_device: FAIL");
    ret = -1;
    goto exit_error;
  }
  set_bit(EV_ABS, ls_drv.input_prox->evbit);
  input_set_abs_params(ls_drv.input_prox, ABS_DISTANCE, 0, 1, 0, 0);
  ls_drv.input_prox->name = "psensor_taos";
  ret = input_register_device(ls_drv.input_prox);
  if(ret)
  {
    MSG2("prox: input_register_device: FAIL=%d",ret);
    goto exit_error;
  }
  else
  {
    MSG2("prox: input_register_device: PASS");
  }

  register_early_suspend(&lsensor_early_suspend);

  printk("BootLog, -%s, ret=%d\n", __func__,ret);
  return ret;

exit_error:

  printk("BootLog, -%s, ret=%d\n", __func__,ret);
  return ret;
}

module_init(lsensor_init);

