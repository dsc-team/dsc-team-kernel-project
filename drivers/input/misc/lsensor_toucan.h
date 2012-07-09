#ifndef _LSENSOR_H_
#define _LSENSOR_H_
#include <asm/ioctl.h>
#include <linux/input.h>

#define MYBIT(b)        (1<<b)
#define TST_BIT(x,b)    ((x & (1<<b)) ? 1 : 0)
#define CLR_BIT(x,b)    (x &= (~(1<<b)))
#define SET_BIT(x,b)    (x |= (1<<b))

#define LSENSOR_ADDR      (0x72 >> 1)

#define LSENSOR_GPIO_INT        159
#define LSENSOR_GPIO_INT_EVT2   28

#define LSENSOR_EN_AP     0x01
#define LSENSOR_EN_LCD    0x02
#define LSENSOR_EN_ENG    0x04
#define PSENSOR_EN_AP     0x10

#define TAOS_MAX_LUX      65535000
#define LSENSOR_CALIBRATION_LOOP 20

struct lsensor_info_data {
  unsigned int t_ms;
  unsigned int a_ms;
  unsigned int p_ms;
  unsigned int w_ms;

  unsigned short cdata;
  unsigned short irdata;
  unsigned short pdata;

  unsigned int  m_irf;
  unsigned int  m_lux;

  unsigned int  status;
};

struct lux_bkl_table {
  short level;
  short now;
  short high;
  short low;
};
#define LSENSOR_BKL_TABLE_SIZE 23
struct lsensor_drv_data {
  int inited;
  int enable;
  int opened;
  int eng_mode;
  int irq_enabled;
  int in_early_suspend;
  int in_suspend;
  int intr_gpio;

  struct i2c_client *client;
  struct wake_lock wlock;
  struct input_dev *input_als;
  struct input_dev *input_prox;

  unsigned int m_ga;

  struct completion info_comp;
  int info_waiting;

  unsigned int lux_history[3];
  unsigned int bkl_idx;
  struct lux_bkl_table bkl_table[LSENSOR_BKL_TABLE_SIZE];

  unsigned int prox_history;
  unsigned int prox_threshold;

  unsigned int millilux;
  unsigned int distance;
  unsigned int distance_old;
  unsigned int distance_ap;

  unsigned int als_nv;
  unsigned int prox_nv;

  unsigned long jiff_update_bkl_wait_time;
};

struct lsensor_eng_data {
  unsigned char pon;

  unsigned char aen;
  unsigned char again;
  unsigned char atime;
  unsigned int  m_ga;

  unsigned char pen;
  unsigned char ppcount;
  unsigned char pdrive;
  unsigned char ptime;

  unsigned char wen;
  unsigned char wtime;
  unsigned char wlong;
};

struct lsensor_reg_data {
  struct r00{
     unsigned char pon:1;  
     unsigned char aen:1;  
     unsigned char pen:1;  
     unsigned char wen:1;  
     unsigned char aien:1; 
     unsigned char pien:1; 
  } r00;
  struct r01_0F {
    
    unsigned char atime;    
    unsigned char ptime;    
    unsigned char wtime;    
    
    unsigned char ailtl;    
    unsigned char ailth;    
    unsigned char aihtl;    
    unsigned char aihth;    
    
    unsigned char piltl;    
    unsigned char pilth;    
    unsigned char pihtl;    
    unsigned char pihth;    
    
    struct r0C{
       unsigned char apers:4; 
       unsigned char ppers:4; 
    } r0C;
    struct r0D{
       unsigned char rev:1;   
       unsigned char wlong:1; 
    } r0D;
    unsigned char ppcount;
    struct r0F{
       unsigned char again:2; 
       unsigned char rev:2;   
       unsigned char pdiode:2;
       unsigned char pdrive:2;
    } r0F;
  }r01_0F;
};

struct lsensor_cal_data {
  unsigned int als;
  struct {
    unsigned short prox_far;
    unsigned short prox_near;
  } prox;
  unsigned int status;
};
typedef enum
{
  QISDA_DIAGPKT_REQ_RSP_IDLE = 0,
  QISDA_DIAGPKT_REQ_RSP_ON_GOING,
  QISDA_DIAGPKT_REQ_RSP_DONE
}diagpkt_req_rsp_process_status_type;
#define GCC_PACKED __attribute__((packed))
typedef struct GCC_PACKED {
  short           iRequestSize;
  short           piResponseSize;
  unsigned char   piRequestBytes[1024];
  unsigned char   piResponseBytes[1024];
  diagpkt_req_rsp_process_status_type process_status;
} qisda_diagpkt_req_rsp_type;
typedef struct GCC_PACKED {
  unsigned char   nv_operation;
  unsigned short  nv_item_index;
  unsigned char   item_data[128];
  unsigned short  nv_operation_status;
} nv_write_command;
typedef struct GCC_PACKED {
  unsigned char   command_code;
  unsigned char   subsys_id;
  unsigned short  subsys_cmd_code;
  unsigned int    status;
  unsigned short  delayed_rsp_id;
  unsigned short  rsp_cnt;
} diagpkt_subsys_hdr_type_v2;

#define ALS_NV_ITEM   0x7542
#define PROX_NV_ITEM  0x754B

#define LSENSOR_IOC_MAGIC       'l' 
#define LSENSOR_IOC_ENABLE      _IO(LSENSOR_IOC_MAGIC, 1)
#define LSENSOR_IOC_DISABLE     _IO(LSENSOR_IOC_MAGIC, 2)
#define LSENSOR_IOC_GET_STATUS  _IOR(LSENSOR_IOC_MAGIC, 3, unsigned int)
#define LSENSOR_IOC_CALIBRATION _IOR(LSENSOR_IOC_MAGIC, 4, struct lsensor_cal_data)
#define LSENSOR_IOC_CAL_ALS     _IOR(LSENSOR_IOC_MAGIC, 5, struct lsensor_cal_data)
#define LSENSOR_IOC_CAL_PROX_FAR  _IOR(LSENSOR_IOC_MAGIC, 6, struct lsensor_cal_data)
#define LSENSOR_IOC_CAL_PROX_NEAR _IOR(LSENSOR_IOC_MAGIC, 7, struct lsensor_cal_data)
#define LSENSOR_IOC_CAL_WRITE   _IOW(LSENSOR_IOC_MAGIC, 8, struct lsensor_eng_data)
#define PSENSOR_IOC_ENABLE      _IO(LSENSOR_IOC_MAGIC, 11)
#define PSENSOR_IOC_DISABLE     _IO(LSENSOR_IOC_MAGIC, 12)
#define PSENSOR_IOC_GET_STATUS  _IOR(LSENSOR_IOC_MAGIC, 13, unsigned int)
#define LSENSOR_IOC_RESET       _IO(LSENSOR_IOC_MAGIC, 21)
#define LSENSOR_IOC_ENG_ENABLE  _IO(LSENSOR_IOC_MAGIC, 22)
#define LSENSOR_IOC_ENG_DISABLE _IO(LSENSOR_IOC_MAGIC, 23)
#define LSENSOR_IOC_ENG_CTL_R   _IOR(LSENSOR_IOC_MAGIC, 24, struct lsensor_eng_data)
#define LSENSOR_IOC_ENG_CTL_W   _IOW(LSENSOR_IOC_MAGIC, 25, struct lsensor_eng_data)
#define LSENSOR_IOC_ENG_INFO    _IOR(LSENSOR_IOC_MAGIC, 26, struct lsensor_info_data)


static inline void qsd_lsensor_enable_onOff(unsigned int mode, unsigned int onOff)  { }
#endif

