/* drivers/i2c/busses/i2c-msm.c
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2009, Code Aurora Forum. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/* #define DEBUG */

#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <mach/board.h>
#include <linux/mutex.h>
#include <linux/timer.h>
#include <linux/remote_spinlock.h>
#include <linux/pm_qos_params.h>
#include <mach/gpio.h>

#define MSG(format, arg...) printk("[I2C]" format "\n", ## arg)
#define MSG2(format, arg...) printk(KERN_CRIT "[I2C]" format "\n", ## arg)

enum {
  I2C_WRITE_DATA          = 0x00,
  I2C_CLK_CTL             = 0x04,
  I2C_STATUS              = 0x08,
  I2C_READ_DATA           = 0x0c,
  I2C_INTERFACE_SELECT    = 0x10,

  I2C_WRITE_DATA_DATA_BYTE            = 0xff,
  I2C_WRITE_DATA_ADDR_BYTE            = 1U << 8,
  I2C_WRITE_DATA_LAST_BYTE            = 1U << 9,

  I2C_CLK_CTL_FS_DIVIDER_VALUE        = 0xff,
  I2C_CLK_CTL_HS_DIVIDER_VALUE        = 7U << 8,

  I2C_STATUS_WR_BUFFER_FULL           = 1U << 0,
  I2C_STATUS_RD_BUFFER_FULL           = 1U << 1,
  I2C_STATUS_BUS_ERROR                = 1U << 2,
  I2C_STATUS_PACKET_NACKED            = 1U << 3,
  I2C_STATUS_ARB_LOST                 = 1U << 4,
  I2C_STATUS_INVALID_WRITE            = 1U << 5,
  I2C_STATUS_FAILED                   = 3U << 6,
  I2C_STATUS_BUS_ACTIVE               = 1U << 8,
  I2C_STATUS_BUS_MASTER               = 1U << 9,
  I2C_STATUS_ERROR_MASK               = 0xfc,

  I2C_INTERFACE_SELECT_INTF_SELECT    = 1U << 0,
  I2C_INTERFACE_SELECT_SCL            = 1U << 8,
  I2C_INTERFACE_SELECT_SDA            = 1U << 9,
  I2C_STATUS_RX_DATA_STATE            = 3U << 11,
  I2C_STATUS_LOW_CLK_STATE            = 3U << 13,
};

struct msm_i2c_dev {
  struct device                *dev;
  void __iomem                 *base; /* virtual */
  int                          irq;
  struct clk                   *clk;
  struct i2c_adapter           adap_pri;
  struct i2c_adapter           adap_aux;

  spinlock_t                   lock;

  struct i2c_msg               *msg;
  int                          rem;
  int                          pos;
  int                          cnt;
  int                          err;
  int                          flush_cnt;
  int                          rd_acked;
  int                          one_bit_t;
  remote_mutex_t               r_lock;
  remote_spinlock_t            s_lock;
  int                          suspended;
  struct mutex                 mlock;
  struct msm_i2c_platform_data *pdata;
  struct timer_list            pwr_timer;
  int                          clk_state;
  void                         *complete;

  struct pm_qos_request_list *pm_qos_req;
};


#if 1
#ifdef CONFIG_HW_AUSTIN
  #define I2C_DEVICE_NUM  15
  struct i2c_error_table {
    unsigned int  count;
    unsigned char id;
    unsigned char name[7];
  } i2c_err_table[I2C_DEVICE_NUM] = {
    { .id = 0x3C, .name = "Cam5M "},
    { .id = 0x21, .name = "CamVGA"},
    { .id = 0x5C, .name = "Touch "},
    { .id = 0x40, .name = "Charge"},
    { .id = 0x55, .name = "Gauge "},
    { .id = 0x48, .name = "TPS650"},
    { .id = 0x38, .name = "G-Sens"},
    { .id = 0x22, .name = "BT-FM "},
    { .id = 0x1C, .name = "E-Comp"},
    { .id = 0x2C, .name = "Capkey"},
    { .id = 0x39, .name = "HdmiP1"},
    { .id = 0x28, .name = "HdmiP2"},
    { .id = 0x3F, .name = "HdmiEd"},
    { .id = 0x2A, .name = "HdmiCe"},
    { .id = 0xFF, .name = "Other "}
  };
#endif
#ifdef CONFIG_HW_TOUCAN
  #define I2C_DEVICE_NUM  10
  struct i2c_error_table {
    unsigned int  count;
    unsigned char id;
    unsigned char name[7];
  } i2c_err_table[I2C_DEVICE_NUM] = {
    { .id = 0x1C, .name = "E-Comp"},
    { .id = 0x36, .name = "Camera"},
    { .id = 0x38, .name = "G-Sens"},
    { .id = 0x39, .name = "L-Sens"},
    { .id = 0x40, .name = "Charge"},
    { .id = 0x48, .name = "TPS650"},
    { .id = 0x4A, .name = "Touch1"},
    { .id = 0x4B, .name = "Touch2"},
    { .id = 0x55, .name = "Gauge "},
    { .id = 0xFF, .name = "Other "}
  };
#endif
static __inline void LOG_ERR_PUSH(uint8_t addr)
{
  int i;
  for(i=0; i<ARRAY_SIZE(i2c_err_table)-1; i++)
  {
    if(i2c_err_table[i].id == addr)
    {
      i2c_err_table[i].count ++;
      return;
    }
  }
  i2c_err_table[ARRAY_SIZE(i2c_err_table)-1].count ++;
}
static __inline void LOG_ERR_OUTPUT(void)
{
  int i;
  printk("[I2C]msm_i2c_resume_early ## Error ## ");
  for(i=0; i<ARRAY_SIZE(i2c_err_table); i++)
    if(i2c_err_table[i].count)
      printk("(0x%02X)%s=%d  ",i2c_err_table[i].id,i2c_err_table[i].name,i2c_err_table[i].count);
  printk("\n");
}
static __inline void LOG_ERR_CLEAR(void)
{
  int i;
  for(i=0; i<ARRAY_SIZE(i2c_err_table); i++)
    i2c_err_table[i].count = 0;
}

enum I2C_LOG{
  I2C_LOG_MSG_RD  = (1U<<15),
  I2C_LOG_MSG_WR  = (1U<<14),
  I2C_LOG_REG_RD  = (1U<<13),
  I2C_LOG_REG_WR  = (1U<<12),
  I2C_LOG_ERR_01  = (0x00010000),
  I2C_LOG_ERR_02  = (0x00020000),
  I2C_LOG_ERR_03  = (0x00030000),
  I2C_LOG_ERR_04  = (0x00040000),
  I2C_LOG_ERR_05  = (0x00050000),
  I2C_LOG_ERR_06  = (0x00060000),
  I2C_LOG_ERR_07  = (0x00070000),
  I2C_LOG_ERR_08  = (0x00080000),
  I2C_LOG_ERR_09  = (0x00090000),
  I2C_LOG_ERR_10  = (0x000A0000),
  I2C_LOG_ERR_11  = (0x000B0000),
  I2C_LOG_ERR_12  = (0x000C0000),
  I2C_LOG_ERR_13  = (0x000D0000),
  I2C_LOG_ERR_14  = (0x000E0000),
  I2C_LOG_ERR_15  = (0x000F0000),
  I2C_LOG_ERR_MSK = (0xFFFF0000),
};
#define I2C_LOG_MAX_NUM 500
uint32_t i2c_buf[I2C_LOG_MAX_NUM+4];
uint32_t i2c_buf2[I2C_LOG_MAX_NUM];
uint16_t i2c_buf_idx;

#ifdef I2C_AUTO_LOG
  uint32_t i2c_err_flag = 0;
  uint32_t i2c_err_count = 0;
#endif
static __inline void LOG_PUSH(uint32_t data)
{
  #if 0
  static int bypass = 0;
  if((data & (I2C_LOG_MSG_WR|I2C_LOG_REG_WR|I2C_WRITE_DATA_ADDR_BYTE)) == (I2C_LOG_MSG_WR|I2C_LOG_REG_WR|I2C_WRITE_DATA_ADDR_BYTE))
  {
    if((data & 0xFF) == 0x70)
      bypass = 1;
    else
      bypass = 0;
  }
  if((data&I2C_LOG_ERR_MSK)==0 && bypass==1)
    return;
  #endif
  if(data)
    i2c_buf[i2c_buf_idx++] = data;
  if(i2c_buf_idx >= I2C_LOG_MAX_NUM)
    i2c_buf_idx = 0;
}
static void LOG_OUTPUT(void)
{
  int i, idx, tmp;
  uint32_t data;

  MSG("i2c_buf+");

  idx = i2c_buf_idx;
  memcpy(i2c_buf2,i2c_buf,sizeof(i2c_buf2));

  for(i=0; i<I2C_LOG_MAX_NUM; i++)
  {
    data = i2c_buf2[idx++];
    if(idx >= I2C_LOG_MAX_NUM)
      idx = 0;
    if((data & (I2C_LOG_MSG_WR | I2C_LOG_REG_WR)) ==
      (I2C_LOG_MSG_WR | I2C_LOG_REG_WR))
    {
      if(data & I2C_WRITE_DATA_ADDR_BYTE)
      {
        printk("\n (W)");
      }
      printk("%02X-",data&0xFF);
      if(data & I2C_WRITE_DATA_LAST_BYTE)
      {
        printk("(P)");
      }
    }
    else
    {
      if((data & (I2C_LOG_MSG_RD | I2C_LOG_REG_WR | I2C_WRITE_DATA_ADDR_BYTE)) ==
        (I2C_LOG_MSG_RD | I2C_LOG_REG_WR | I2C_WRITE_DATA_ADDR_BYTE))
      {
        printk(" (R)%02X-",data&0xFF);
      }
      else if((data & (I2C_LOG_MSG_RD | I2C_LOG_REG_WR | I2C_WRITE_DATA_LAST_BYTE)) ==
        (I2C_LOG_MSG_RD | I2C_LOG_REG_WR | I2C_WRITE_DATA_LAST_BYTE))
      {
        printk("(p)-");
      }
      else
      {
        if((data & I2C_LOG_ERR_MSK) != I2C_LOG_ERR_09)
          printk("%02X-",data&0xFF);
      }
    }
    tmp = (data & I2C_LOG_ERR_MSK) >> 16;
    if(tmp)
    {
      printk(" <%s> ",
        tmp==0 ? "" :
        tmp==1 ? "suspend" :
        tmp==2 ? "resume" :
        tmp==3 ? "xfer: in suspend" :
        tmp==4 ? "xfer: in suspend + clk fail" :
        tmp==5 ? "xfer: bus busy" :
        tmp==6 ? "xfer: bus busy + recover fail" :
        tmp==7 ? "xfer: write busy" :
        tmp==8 ? "xfer: write busy + recover fail" :
        tmp==9 ? "IRQ:  nothing to do" :
        tmp==10? "IRQ:  NAK" :
        tmp==11? "IRQ:  misc error" :
        tmp==12? "IRQ:  read not stop" :
        tmp==13? "IRQ:  write buffer full" :
        tmp==14? "xfer: time out" :
        tmp==15? "xfer: error" : "no define"
        );
    }
  }
  printk("\n");
  MSG("i2c_buf-");
}
static ssize_t msm_i2c_ctrl_show(struct device *dev, struct device_attribute *attr, char *buf)
{
  LOG_OUTPUT();
  return 0;
}
static ssize_t msm_i2c_ctrl_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
  char bufLocal[64];
  printk("\n");
  if(count >= sizeof(bufLocal))
  {
    MSG("%s input invalid, count = %d", __func__, count);
    return count;
  }
  memcpy(bufLocal,buf,count);
  switch(bufLocal[0])
  {
    case 'a':
      if(bufLocal[1]=='0')
      {
        LOG_OUTPUT();
      }
      else if(bufLocal[1]=='1')
      {
        LOG_ERR_OUTPUT();
      }
      else if(bufLocal[1]=='2')
      {
        MSG("Reset i2c error count");
        LOG_ERR_CLEAR();
      }
      break;
    default:
      MSG("a0:log,    a1:err count,    a2:err count reset");
      break;
  }
  return count;
}
static struct device_attribute i2c_ctrl_attrs[] = {
  __ATTR(ctrl, 0664, msm_i2c_ctrl_show, msm_i2c_ctrl_store),
};
#endif

static void
msm_i2c_pwr_mgmt(struct msm_i2c_dev *dev, unsigned int state)
{
  dev->clk_state = state;
  if (state != 0)
    clk_enable(dev->clk);
  else
    clk_disable(dev->clk);
}

static void
msm_i2c_pwr_timer(unsigned long data)
{
  struct msm_i2c_dev *dev = (struct msm_i2c_dev *) data;
  dev_dbg(dev->dev, "I2C_Power: Inactivity based power management\n");
  if (dev->clk_state == 1)
    msm_i2c_pwr_mgmt(dev, 0);
}

#ifdef DEBUG
static void
dump_status(uint32_t status)
{
  printk("STATUS (0x%.8x): ", status);
  if (status & I2C_STATUS_BUS_MASTER)
    printk("MST ");
  if (status & I2C_STATUS_BUS_ACTIVE)
    printk("ACT ");
  if (status & I2C_STATUS_INVALID_WRITE)
    printk("INV_WR ");
  if (status & I2C_STATUS_ARB_LOST)
    printk("ARB_LST ");
  if (status & I2C_STATUS_PACKET_NACKED)
    printk("NAK ");
  if (status & I2C_STATUS_BUS_ERROR)
    printk("BUS_ERR ");
  if (status & I2C_STATUS_RD_BUFFER_FULL)
    printk("RD_FULL ");
  if (status & I2C_STATUS_WR_BUFFER_FULL)
    printk("WR_FULL ");
  if (status & I2C_STATUS_FAILED)
    printk("FAIL 0x%x", (status & I2C_STATUS_FAILED));
  printk("\n");
}
#endif

static irqreturn_t
msm_i2c_interrupt(int irq, void *devid)
{
  struct msm_i2c_dev *dev = devid;
  uint32_t status = readl(dev->base + I2C_STATUS);
  int err = 0;

#ifdef DEBUG
  dump_status(status);
#endif

  spin_lock(&dev->lock);
  if (!dev->msg) {
    MSG("IRQ:  Nothing to do (%04X)", status);
    LOG_PUSH(I2C_LOG_ERR_09);
    spin_unlock(&dev->lock);
    return IRQ_HANDLED;
  }

  if (status & I2C_STATUS_ERROR_MASK) {
    if((status & I2C_STATUS_PACKET_NACKED) && !(status & (I2C_STATUS_BUS_ERROR|I2C_STATUS_ARB_LOST)) )
    {
      MSG2("IRQ:  %02X %c%d NAK (%04X)", (dev->msg->addr<<1),((dev->msg->flags&I2C_M_RD)?'R':'W'),dev->msg->len,status);
      LOG_PUSH(I2C_LOG_ERR_10 |
        ((dev->msg->flags&I2C_M_RD)?I2C_LOG_MSG_RD:I2C_LOG_MSG_WR));
    }
    else
    {
      MSG2("IRQ:  %02X %c%d Error (%04X)", (dev->msg->addr<<1),((dev->msg->flags&I2C_M_RD)?'R':'W'),dev->msg->len,status);
      LOG_PUSH(I2C_LOG_ERR_11 |
        ((dev->msg->flags&I2C_M_RD)?I2C_LOG_MSG_RD:I2C_LOG_MSG_WR));
    }
    err = -EIO;
    goto out_err;
  }

  if (dev->msg->flags & I2C_M_RD) {
    if (status & I2C_STATUS_RD_BUFFER_FULL) {

      /*
       * Theres something in the FIFO.
       * Are we expecting data or flush crap?
       */
      if (dev->cnt) { /* DATA */
        uint8_t *data = &dev->msg->buf[dev->pos];

        /* This is in spin-lock. So there will be no
         * scheduling between reading the second-last
         * byte and writing LAST_BYTE to the controller.
         * So extra read-cycle-clock won't be generated
         * Per I2C MSM HW Specs: Write LAST_BYTE befure
         * reading 2nd last byte
         */
        if (dev->cnt == 2)
        {
          writel(I2C_WRITE_DATA_LAST_BYTE,
            dev->base + I2C_WRITE_DATA);
          LOG_PUSH(I2C_LOG_MSG_RD | I2C_LOG_REG_WR | I2C_WRITE_DATA_LAST_BYTE);
        }
        *data = readl(dev->base + I2C_READ_DATA);
        LOG_PUSH(I2C_LOG_MSG_RD | I2C_LOG_REG_RD | *data);
        dev->cnt--;
        dev->pos++;
        if (dev->msg->len == 1)
          dev->rd_acked = 0;
        if (dev->cnt == 0)
          goto out_complete;

      } else {
        /* Now that extra read-cycle-clocks aren't
         * generated, this becomes error condition
         */
        MSG2("IRQ:  Read not stop (%04X)",status);
        LOG_PUSH(I2C_LOG_ERR_12);
        err = -EIO;
        goto out_err;
      }
    } else if (dev->msg->len == 1 && dev->rd_acked == 0 &&
        ((status & I2C_STATUS_RX_DATA_STATE) ==
         I2C_STATUS_RX_DATA_STATE))
    {
      writel(I2C_WRITE_DATA_LAST_BYTE,
        dev->base + I2C_WRITE_DATA);
      LOG_PUSH(I2C_LOG_MSG_RD | I2C_LOG_REG_WR | I2C_WRITE_DATA_LAST_BYTE);
    }
  } else {
    uint16_t data;

    if (status & I2C_STATUS_WR_BUFFER_FULL) {
      MSG2("IRQ:  Write buffer full (%04X)",status);
      LOG_PUSH(I2C_LOG_ERR_13);
      err = -EIO;
      goto out_err;
    }

    if (dev->cnt) {
      /* Ready to take a byte */
      data = dev->msg->buf[dev->pos];
      if (dev->cnt == 1 && dev->rem == 1)
        data |= I2C_WRITE_DATA_LAST_BYTE;

      if (dev->cnt == 1 && dev->rem == 2)
      {
        #ifdef CONFIG_HW_AUSTIN
          if(dev->msg->addr==0x21 ||
            dev->msg->addr==0x3C ||
            dev->msg->addr==0x38 ||
            dev->msg->addr==0x2C
            )
            data |= I2C_WRITE_DATA_LAST_BYTE;
        #endif
        #ifdef CONFIG_HW_TOUCAN
          if(dev->msg->addr==0x36 ||
            dev->msg->addr==0x38 ||
            dev->msg->addr==0x4A ||
            dev->msg->addr==0x4B
            )
            data |= I2C_WRITE_DATA_LAST_BYTE;
        #endif
      }

      status = readl(dev->base + I2C_STATUS);
      /*
       * Due to a hardware timing issue, data line setup time
       * may be reduced to less than recommended 250 ns.
       * This happens when next byte is written in a
       * particular window of clock line being low and master
       * not stretching the clock line. Due to setup time
       * violation, some slaves may miss first-bit of data, or
       * misinterprete data as start condition.
       * We introduce delay of just over 1/2 clock cycle to
       * ensure master stretches the clock line thereby
       * avoiding setup time violation. Delay is introduced
       * only if I2C clock FSM is LOW. The delay is not needed
       * if I2C clock FSM is HIGH or FORCED_LOW.
       */
      if ((status & I2C_STATUS_LOW_CLK_STATE) ==
          I2C_STATUS_LOW_CLK_STATE)
        udelay((dev->one_bit_t >> 1) + 1);
      writel(data, dev->base + I2C_WRITE_DATA);
      LOG_PUSH(I2C_LOG_MSG_WR | I2C_LOG_REG_WR | data);
      dev->pos++;
      dev->cnt--;
    } else
      goto out_complete;
  }

  spin_unlock(&dev->lock);
  return IRQ_HANDLED;

 out_err:
  dev->err = err;
 out_complete:
  complete(dev->complete);
  spin_unlock(&dev->lock);
  return IRQ_HANDLED;
}

static int
msm_i2c_poll_writeready(struct msm_i2c_dev *dev)
{
  uint32_t retries = 0;

  while (retries != 2000) {
    uint32_t status = readl(dev->base + I2C_STATUS);

    if (!(status & I2C_STATUS_WR_BUFFER_FULL))
      return 0;
    if (retries++ > 1000)
      udelay(100);
  }
  return -ETIMEDOUT;
}

static int
msm_i2c_poll_notbusy(struct msm_i2c_dev *dev)
{
  uint32_t retries = 0;

  while (retries != 2000) {
    uint32_t status = readl(dev->base + I2C_STATUS);

    if (!(status & I2C_STATUS_BUS_ACTIVE))
      return 0;
    if (retries++ > 1000)
      udelay(100);
  }
  return -ETIMEDOUT;
}

static int
msm_i2c_recover_bus_busy(struct msm_i2c_dev *dev, struct i2c_adapter *adap)
{
  int i;
  int gpio_clk;
  int gpio_dat;
  uint32_t status = readl(dev->base + I2C_STATUS);
  bool gpio_clk_status = false;

  if (!(status & (I2C_STATUS_BUS_ACTIVE | I2C_STATUS_WR_BUFFER_FULL)))
    return 0;

  dev->pdata->msm_i2c_config_gpio(adap->nr, 0);
  /* Even adapter is primary and Odd adapter is AUX */
  if (adap->nr % 2) {
    gpio_clk = dev->pdata->aux_clk;
    gpio_dat = dev->pdata->aux_dat;
  } else {
    gpio_clk = dev->pdata->pri_clk;
    gpio_dat = dev->pdata->pri_dat;
  }

  disable_irq(dev->irq);
  if (status & I2C_STATUS_RD_BUFFER_FULL) {
    MSG("Recover (%04X) intf %02X: Read buffer full",status, readl(dev->base + I2C_INTERFACE_SELECT));
    writel(I2C_WRITE_DATA_LAST_BYTE, dev->base + I2C_WRITE_DATA);
    readl(dev->base + I2C_READ_DATA);
  } else if (status & I2C_STATUS_BUS_MASTER) {
    MSG("Recover (%04X) intf %02x: Still the bus master",status, readl(dev->base + I2C_INTERFACE_SELECT));
    writel(I2C_WRITE_DATA_LAST_BYTE | 0xff,
           dev->base + I2C_WRITE_DATA);
  }

  for (i = 0; i < 9; i++) {
    if (gpio_get_value(gpio_dat) && gpio_clk_status)
      break;
    gpio_direction_output(gpio_clk, 0);
    udelay(5);
    gpio_direction_output(gpio_dat, 0);
    udelay(5);
    gpio_direction_input(gpio_clk);
    udelay(5);
    if (!gpio_get_value(gpio_clk))
      udelay(20);
    if (!gpio_get_value(gpio_clk))
      msleep(10);
    gpio_clk_status = gpio_get_value(gpio_clk);
    gpio_direction_input(gpio_dat);
    udelay(5);
  }
  dev->pdata->msm_i2c_config_gpio(adap->nr, 1);
  udelay(10);

  status = readl(dev->base + I2C_STATUS);
  if (!(status & I2C_STATUS_BUS_ACTIVE)) {
    MSG("Recover (%04X) intf %02x: Bus busy cleared after %d clock cycles", status, readl(dev->base + I2C_INTERFACE_SELECT),i);
    enable_irq(dev->irq);
    return 0;
  }

  MSG("Recover (%04X) intf %02x: Bus still busy",status, readl(dev->base + I2C_INTERFACE_SELECT));
  enable_irq(dev->irq);
  return -EBUSY;
}

static void
msm_i2c_rspin_lock(struct msm_i2c_dev *dev)
{
  int gotlock = 0;
  unsigned long flags;
  uint32_t *smem_ptr = (uint32_t *)dev->pdata->rmutex;
  do {
    remote_spin_lock_irqsave(&dev->s_lock, flags);
    if (*smem_ptr == 0) {
      *smem_ptr = 1;
      gotlock = 1;
    }
    remote_spin_unlock_irqrestore(&dev->s_lock, flags);
  } while (!gotlock);
}

static void
msm_i2c_rspin_unlock(struct msm_i2c_dev *dev)
{
  unsigned long flags;
  uint32_t *smem_ptr = (uint32_t *)dev->pdata->rmutex;
  remote_spin_lock_irqsave(&dev->s_lock, flags);
  *smem_ptr = 0;
  remote_spin_unlock_irqrestore(&dev->s_lock, flags);
}

static int
msm_i2c_xfer(struct i2c_adapter *adap, struct i2c_msg msgs[], int num)
{
  DECLARE_COMPLETION_ONSTACK(complete);
  struct msm_i2c_dev *dev = i2c_get_adapdata(adap);
  int ret;
  int rem = num;
  uint16_t addr;
  long timeout;
  unsigned long flags;
  int check_busy = 1;

  del_timer_sync(&dev->pwr_timer);
  mutex_lock(&dev->mlock);
  if (dev->suspended) {
    mutex_unlock(&dev->mlock);
    return -EIO;
  }

  if (dev->clk_state == 0) {
    dev_dbg(dev->dev, "I2C_Power: Enable I2C clock(s)\n");
    msm_i2c_pwr_mgmt(dev, 1);
  }

  /* Don't allow power collapse until we release remote spinlock */
  pm_qos_update_request(dev->pm_qos_req,  dev->pdata->pm_lat);
  if (dev->pdata->rmutex) {
    /*
     * Older modem side uses remote_mutex lock only to update
     * shared variable, and newer modem side uses remote mutex
     * to protect the whole transaction
     */
    if (dev->pdata->rsl_id[0] == 'S')
      msm_i2c_rspin_lock(dev);
    else
      remote_mutex_lock(&dev->r_lock);
    /* If other processor did some transactions, we may have
     * interrupt pending. Clear it
     */
    get_irq_chip(dev->irq)->ack(dev->irq);
  }

  if (adap == &dev->adap_pri)
    writel(0, dev->base + I2C_INTERFACE_SELECT);
  else
    writel(I2C_INTERFACE_SELECT_INTF_SELECT,
        dev->base + I2C_INTERFACE_SELECT);
  enable_irq(dev->irq);
  while (rem) {
    addr = msgs->addr << 1;
    if (msgs->flags & I2C_M_RD)
      addr |= 1;

    spin_lock_irqsave(&dev->lock, flags);
    dev->msg = msgs;
    dev->rem = rem;
    dev->pos = 0;
    dev->err = 0;
    dev->flush_cnt = 0;
    dev->cnt = msgs->len;
    dev->complete = &complete;
    spin_unlock_irqrestore(&dev->lock, flags);

    if (check_busy) {
      ret = msm_i2c_poll_notbusy(dev);
      if (ret)
      {
        #ifdef I2C_AUTO_LOG
          i2c_err_flag ++;
        #endif
        LOG_PUSH(I2C_LOG_ERR_05 |
          (msgs->flags&I2C_M_RD)?I2C_LOG_MSG_RD:I2C_LOG_MSG_WR | addr);
        ret = msm_i2c_recover_bus_busy(dev, adap);
        if (ret) {
          MSG2("xfer: Waiting for notbusy");
          LOG_PUSH(I2C_LOG_ERR_06 |
            (msgs->flags&I2C_M_RD)?I2C_LOG_MSG_RD:I2C_LOG_MSG_WR | addr);
          dev->err = -EBUSY;
          goto out_err;
        }
      }
      check_busy = 0;
    }

    if (rem == 1 && msgs->len == 0)
      addr |= I2C_WRITE_DATA_LAST_BYTE;

    /* Wait for WR buffer not full */
    ret = msm_i2c_poll_writeready(dev);
    if (ret) {
      #ifdef I2C_AUTO_LOG
        i2c_err_flag ++;
      #endif
      LOG_PUSH(I2C_LOG_ERR_07 |
        (msgs->flags&I2C_M_RD)?I2C_LOG_MSG_RD:I2C_LOG_MSG_WR | addr);
      ret = msm_i2c_recover_bus_busy(dev, adap);
      if (ret) {
        MSG2("xfer: Waiting for write ready before addr");
        LOG_PUSH(I2C_LOG_ERR_08 |
          (msgs->flags&I2C_M_RD)?I2C_LOG_MSG_RD:I2C_LOG_MSG_WR | addr);
        dev->err = -EBUSY;
        goto out_err;
      }
    }

    /* special case for doing 1 byte read.
     * There should be no scheduling between I2C controller becoming
     * ready to read and writing LAST-BYTE to I2C controller
     * This will avoid potential of I2C controller starting to latch
     * another extra byte.
     */
    if ((msgs->len == 1) && (msgs->flags & I2C_M_RD)) {
      uint32_t retries = 0;
      spin_lock_irqsave(&dev->lock, flags);

      writel(I2C_WRITE_DATA_ADDR_BYTE | addr,
        dev->base + I2C_WRITE_DATA);
      LOG_PUSH(I2C_LOG_MSG_RD | I2C_LOG_REG_WR | I2C_WRITE_DATA_ADDR_BYTE | addr);

      /* Poll for I2C controller going into RX_DATA mode to
       * ensure controller goes into receive mode.
       * Just checking write_buffer_full may not work since
       * there is delay between the write-buffer becoming
       * empty and the slave sending ACK to ensure I2C
       * controller goes in receive mode to receive data.
       */
      while (retries != 2000) {
        uint32_t status = readl(dev->base + I2C_STATUS);

          if ((status & I2C_STATUS_RX_DATA_STATE)
            == I2C_STATUS_RX_DATA_STATE)
            break;
        retries++;
      }
      if (retries >= 2000) {
        dev->rd_acked = 0;
        spin_unlock_irqrestore(&dev->lock, flags);
        /* 1-byte-reads from slow devices in interrupt
         * context
         */
        goto wait_for_int;
      }

      dev->rd_acked = 1;
      writel(I2C_WRITE_DATA_LAST_BYTE,
          dev->base + I2C_WRITE_DATA);
      LOG_PUSH(I2C_LOG_MSG_RD | I2C_LOG_REG_WR | I2C_WRITE_DATA_LAST_BYTE);
      spin_unlock_irqrestore(&dev->lock, flags);
    } else {
      writel(I2C_WRITE_DATA_ADDR_BYTE | addr,
           dev->base + I2C_WRITE_DATA);
      LOG_PUSH(((dev->msg->flags&I2C_M_RD)?I2C_LOG_MSG_RD:I2C_LOG_MSG_WR) | I2C_LOG_REG_WR | I2C_WRITE_DATA_ADDR_BYTE | addr);
    }
    /* Polling and waiting for write_buffer_empty is not necessary.
     * Even worse, if we do, it can result in invalid status and
     * error if interrupt(s) occur while polling.
     */

    /*
     * Now that we've setup the xfer, the ISR will transfer the data
     * and wake us up with dev->err set if there was an error
     */
wait_for_int:

    timeout = wait_for_completion_timeout(&complete, HZ);
    if (!timeout) {
      MSG2("xfer: Transaction timed out");
      writel(I2C_WRITE_DATA_LAST_BYTE,
        dev->base + I2C_WRITE_DATA);
      LOG_PUSH(I2C_LOG_ERR_14 |
        ((dev->msg->flags&I2C_M_RD)?I2C_LOG_MSG_RD:I2C_LOG_MSG_WR) | I2C_LOG_REG_WR | I2C_WRITE_DATA_LAST_BYTE );
      msleep(100);
      /* FLUSH */
      readl(dev->base + I2C_READ_DATA);
      readl(dev->base + I2C_STATUS);
      ret = -ETIMEDOUT;
      goto out_err;
    }
    if (dev->err) {
      ret = dev->err;
      goto out_err;
    }

    if (msgs->flags & I2C_M_RD)
      check_busy = 1;

    msgs++;
    rem--;
  }

  ret = num;
 out_err:
  if(dev->err)
    LOG_ERR_PUSH(msgs->addr);
  spin_lock_irqsave(&dev->lock, flags);
  dev->complete = NULL;
  dev->msg = NULL;
  dev->rem = 0;
  dev->pos = 0;
  dev->err = 0;
  dev->flush_cnt = 0;
  dev->cnt = 0;
  spin_unlock_irqrestore(&dev->lock, flags);
  disable_irq(dev->irq);
  if (dev->pdata->rmutex) {
    if (dev->pdata->rsl_id[0] == 'S')
      msm_i2c_rspin_unlock(dev);
    else
      remote_mutex_unlock(&dev->r_lock);
  }
  pm_qos_update_request(dev->pm_qos_req,
			      PM_QOS_DEFAULT_VALUE);
  mod_timer(&dev->pwr_timer, (jiffies + 3*HZ));
  mutex_unlock(&dev->mlock);

  #ifdef I2C_AUTO_LOG
    if(i2c_err_flag)
      i2c_err_count ++;
    if(i2c_err_count >= 30)
    {
      i2c_err_flag = 0;
      i2c_err_count = 0;
      LOG_OUTPUT();
    }
  #endif

  return ret;
}

static u32
msm_i2c_func(struct i2c_adapter *adap)
{
  return I2C_FUNC_I2C | (I2C_FUNC_SMBUS_EMUL & ~I2C_FUNC_SMBUS_QUICK);
}

static const struct i2c_algorithm msm_i2c_algo = {
  .master_xfer  = msm_i2c_xfer,
  .functionality  = msm_i2c_func,
};

static int
msm_i2c_probe(struct platform_device *pdev)
{
  struct msm_i2c_dev  *dev;
  struct resource   *mem, *irq, *ioarea;
  int ret;
  int fs_div;
  int hs_div;
  int i2c_clk;
  int clk_ctl;
  struct clk *clk;
  struct msm_i2c_platform_data *pdata;

  printk(KERN_INFO "msm_i2c_probe\n");
  printk(KERN_INFO "[I2C]5035+Log\n");

  {
    int i;
    for(i=0; i<I2C_LOG_MAX_NUM; i++)
      i2c_buf[i] = 0;
    i2c_buf_idx = 0;

    ret = device_create_file(&pdev->dev, &i2c_ctrl_attrs[0]);
    if(ret) MSG("i2c_ctrl_attrs create FAIL, ret=%d" ,ret);
    else    MSG("Get i2c log by cat /sys/devices/platform/msm_i2c.0/ctrl");
  }

  /* NOTE: driver uses the static register mapping */
  mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
  if (!mem) {
    dev_err(&pdev->dev, "no mem resource?\n");
    return -ENODEV;
  }
  irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
  if (!irq) {
    dev_err(&pdev->dev, "no irq resource?\n");
    return -ENODEV;
  }

  ioarea = request_mem_region(mem->start, (mem->end - mem->start) + 1,
      pdev->name);
  if (!ioarea) {
    dev_err(&pdev->dev, "I2C region already claimed\n");
    return -EBUSY;
  }
  clk = clk_get(&pdev->dev, "i2c_clk");
  if (IS_ERR(clk)) {
    dev_err(&pdev->dev, "Could not get clock\n");
    ret = PTR_ERR(clk);
    goto err_clk_get_failed;
  }

  pdata = pdev->dev.platform_data;
  if (!pdata) {
    dev_err(&pdev->dev, "platform data not initialized\n");
    ret = -ENOSYS;
    goto err_clk_get_failed;
  }
  if (!pdata->msm_i2c_config_gpio) {
    dev_err(&pdev->dev, "config_gpio function not initialized\n");
    ret = -ENOSYS;
    goto err_clk_get_failed;
  }
  /* We support frequencies upto FAST Mode(400KHz) */
  if (pdata->clk_freq <= 0 || pdata->clk_freq > 400000) {
    dev_err(&pdev->dev, "clock frequency not supported\n");
    ret = -EIO;
    goto err_clk_get_failed;
  }

  dev = kzalloc(sizeof(struct msm_i2c_dev), GFP_KERNEL);
  if (!dev) {
    ret = -ENOMEM;
    goto err_alloc_dev_failed;
  }

  dev->dev = &pdev->dev;
  dev->irq = irq->start;
  dev->clk = clk;
  dev->pdata = pdata;
  dev->base = ioremap(mem->start, (mem->end - mem->start) + 1);
  if (!dev->base) {
    ret = -ENOMEM;
    goto err_ioremap_failed;
  }

  dev->one_bit_t = USEC_PER_SEC/pdata->clk_freq;
  spin_lock_init(&dev->lock);
  platform_set_drvdata(pdev, dev);

  clk_enable(clk);

  if (pdata->rmutex && pdata->rsl_id[0] == 'S') {
    remote_spinlock_id_t rmid;
    rmid = pdata->rsl_id;
    if (remote_spin_lock_init(&dev->s_lock, rmid) != 0)
      pdata->rmutex = 0;
  } else if (pdata->rmutex) {
    struct remote_mutex_id rmid;
    rmid.r_spinlock_id = pdata->rsl_id;
    rmid.delay_us = 10000000/pdata->clk_freq;
    if (remote_mutex_init(&dev->r_lock, &rmid) != 0)
      pdata->rmutex = 0;
  }
  /* I2C_HS_CLK = I2C_CLK/(3*(HS_DIVIDER_VALUE+1) */
  /* I2C_FS_CLK = I2C_CLK/(2*(FS_DIVIDER_VALUE+3) */
  /* FS_DIVIDER_VALUE = ((I2C_CLK / I2C_FS_CLK) / 2) - 3 */
  i2c_clk = 19200000; /* input clock */
  fs_div = ((i2c_clk / pdata->clk_freq) / 2) - 3;
  hs_div = 3;
  clk_ctl = ((hs_div & 0x7) << 8) | (fs_div & 0xff);
  writel(clk_ctl, dev->base + I2C_CLK_CTL);
  printk(KERN_INFO "msm_i2c_probe: clk_ctl %x, %d Hz\n",
         clk_ctl, i2c_clk / (2 * ((clk_ctl & 0xff) + 3)));

  i2c_set_adapdata(&dev->adap_pri, dev);
  dev->adap_pri.algo = &msm_i2c_algo;
  strlcpy(dev->adap_pri.name,
    "MSM I2C adapter-PRI",
    sizeof(dev->adap_pri.name));

  dev->adap_pri.nr = pdev->id;
  ret = i2c_add_numbered_adapter(&dev->adap_pri);
  if (ret) {
    dev_err(&pdev->dev, "Primary i2c_add_adapter failed\n");
    goto err_i2c_add_adapter_failed;
  }

  i2c_set_adapdata(&dev->adap_aux, dev);
  dev->adap_aux.algo = &msm_i2c_algo;
  strlcpy(dev->adap_aux.name,
    "MSM I2C adapter-AUX",
    sizeof(dev->adap_aux.name));

  dev->adap_aux.nr = pdev->id + 1;
  ret = i2c_add_numbered_adapter(&dev->adap_aux);
  if (ret) {
    dev_err(&pdev->dev, "auxiliary i2c_add_adapter failed\n");
    i2c_del_adapter(&dev->adap_pri);
    goto err_i2c_add_adapter_failed;
  }
  ret = request_irq(dev->irq, msm_i2c_interrupt,
      IRQF_TRIGGER_RISING, pdev->name, dev);
  if (ret) {
    dev_err(&pdev->dev, "request_irq failed\n");
    goto err_request_irq_failed;
  }
  dev->pm_qos_req = pm_qos_add_request(PM_QOS_CPU_DMA_LATENCY,
					     PM_QOS_DEFAULT_VALUE);
	if (!dev->pm_qos_req) {
		dev_err(&pdev->dev, "pm_qos_add_request failed\n");
		goto err_pm_qos_add_request_failed;
	}
  
  disable_irq(dev->irq);
  dev->suspended = 0;
  mutex_init(&dev->mlock);
  dev->clk_state = 0;
  /* Config GPIOs for primary and secondary lines */
  pdata->msm_i2c_config_gpio(dev->adap_pri.nr, 1);
  pdata->msm_i2c_config_gpio(dev->adap_aux.nr, 1);
  clk_disable(dev->clk);
  setup_timer(&dev->pwr_timer, msm_i2c_pwr_timer, (unsigned long) dev);

	return 0;

err_pm_qos_add_request_failed:
	free_irq(dev->irq, dev); 
err_request_irq_failed:
  i2c_del_adapter(&dev->adap_pri);
  i2c_del_adapter(&dev->adap_aux);
err_i2c_add_adapter_failed:
  clk_disable(clk);
  iounmap(dev->base);
err_ioremap_failed:
  kfree(dev);
err_alloc_dev_failed:
  clk_put(clk);
err_clk_get_failed:
  release_mem_region(mem->start, (mem->end - mem->start) + 1);
  return ret;
}

static int
msm_i2c_remove(struct platform_device *pdev)
{
  struct msm_i2c_dev  *dev = platform_get_drvdata(pdev);
  struct resource   *mem;

	/* Grab mutex to ensure ongoing transaction is over */
	mutex_lock(&dev->mlock);
	dev->suspended = 1;
	mutex_unlock(&dev->mlock);
	mutex_destroy(&dev->mlock);
	del_timer_sync(&dev->pwr_timer);
	if (dev->clk_state != 0)
		msm_i2c_pwr_mgmt(dev, 0);
	platform_set_drvdata(pdev, NULL);
	pm_qos_remove_request(dev->pm_qos_req);
	free_irq(dev->irq, dev);
	i2c_del_adapter(&dev->adap_pri);
	i2c_del_adapter(&dev->adap_aux);
	clk_put(dev->clk);
	iounmap(dev->base);
	kfree(dev);
	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (mem)
		release_mem_region(mem->start, (mem->end - mem->start) + 1);
	return 0;
}

static int msm_i2c_suspend(struct platform_device *pdev, pm_message_t state)
{
  struct msm_i2c_dev *dev = platform_get_drvdata(pdev);
  /* Wait until current transaction finishes
   * Make sure remote lock is released before we suspend
   */
  MSG("%s",__func__);
  if (dev) {
    /* Grab mutex to ensure ongoing transaction is over */
    mutex_lock(&dev->mlock);
    dev->suspended = 1;
    mutex_unlock(&dev->mlock);
    del_timer_sync(&dev->pwr_timer);
    if (dev->clk_state != 0)
      msm_i2c_pwr_mgmt(dev, 0);
  }
  LOG_PUSH(I2C_LOG_ERR_01);

  return 0;
}

static int msm_i2c_resume(struct platform_device *pdev)
{
  struct msm_i2c_dev *dev = platform_get_drvdata(pdev);
  LOG_PUSH(I2C_LOG_ERR_02);
  dev->suspended = 0;
  LOG_ERR_OUTPUT();
  return 0;
}

static struct platform_driver msm_i2c_driver = {
  .probe    = msm_i2c_probe,
  .remove   = msm_i2c_remove,
  .suspend  = msm_i2c_suspend,
  .resume   = msm_i2c_resume,
  .driver   = {
    .name = "msm_i2c",
    .owner  = THIS_MODULE,
  },
};

/* I2C may be needed to bring up other drivers */
static int __init
msm_i2c_init_driver(void)
{
  return platform_driver_register(&msm_i2c_driver);
}
subsys_initcall(msm_i2c_init_driver);

static void __exit msm_i2c_exit_driver(void)
{
  platform_driver_unregister(&msm_i2c_driver);
}
module_exit(msm_i2c_exit_driver);

