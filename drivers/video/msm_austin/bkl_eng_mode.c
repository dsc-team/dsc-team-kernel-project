
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/err.h>
#include <mach/gpio.h>
#include <mach/bkl_cmd.h>
#include "mddihost.h"



#ifdef DEBUG_BKL_ENG_MODE
#define MSG(format, arg...) printk(KERN_INFO "[BKL]" format "\n", ## arg)
#else
#define MSG(format, arg...)
#endif

#define BKL_ENG_MODE_LOOP 5




void bkl_write_reg(unsigned int addr, unsigned int data)
{
  MSG("Write 0x%04X = 0x%04X",addr,data);
  mddi_queue_register_write(addr, data, FALSE, 0);
  mdelay(5);  
};
void bkl_read_reg(unsigned int addr, unsigned int *data)
{
  static unsigned int val;  
  
  mddi_queue_register_read(addr, &val, FALSE, 0);
  msleep(10);
  *data = val;
  MSG("Read 0x%04X = 0x%04X",addr,val);
}
int bkl_write_multi_reg(struct bkl_reg * src)
{
  int i;
  unsigned int addr,data;
  for(i=0; i<256; i++)
  {
    addr = src[i].addr;
    data = src[i].data;
    if(addr == 0xFFFF) 
      break;
    bkl_write_reg(addr,data);
  }
  return 1;
}



int bkl_eng_mode2_write(void *buf, unsigned int size)
{
  struct bkl_reg labc_regs[] = {
    {0x5700,0x0000},    {0x5701,0x003C},    {0x5704,0x0001},    {0x5705,0x0008},    {0x5708,0x0001},
    {0x5709,0x0090},    {0x570C,0x0002},    {0x570D,0x0030},    {0x5710,0x0002},    {0x5711,0x00FC},
    {0x5714,0x0003},    {0x5715,0x00FF},    {0x5702,0x0000},    {0x5703,0x0028},    {0x5706,0x0000},
    {0x5707,0x00E0},    {0x570A,0x0001},    {0x570B,0x0040},    {0x570E,0x0002},    {0x570F,0x0008},
    {0x5712,0x0002},    {0x5716,0x0003},    {0x5717,0x00FF},    {0xFFFF,0x0000},  
  };
  struct bkl_cmd2 cmd2;
  static unsigned short reg5300 = 0x0024;  
  unsigned short reg5301,reg5500,reg6A01;
  int i,loop;

  if(size != sizeof(struct bkl_cmd2))
    return 0;

  memcpy(&cmd2, buf, sizeof(struct bkl_cmd2));

  #if 0
    MSG("mode =0x%04X",cmd2.mode);
    MSG("dbv  =0x%04X",cmd2.dbv);
    MSG("pwmdi=0x%04X",cmd2.pwmdi);
    MSG("pwmdu=0x%04X",cmd2.pwmdu);
    MSG("drivi=0x%04X",cmd2.drivi);

    MSG("clock=0x%04X",cmd2.clock);
    MSG("logic=0x%04X",cmd2.logic);
    MSG("pwmin=0x%04X",cmd2.pwmin);
    MSG("blena=0x%04X",cmd2.blena);
    MSG("bldim=0x%04X",cmd2.bldim);

    MSG("wrctr=0x%04X",cmd2.wrctr);
    MSG("ctrle=0x%04X",cmd2.ctrle);
    MSG("labcc=0x%04X",cmd2.labcc);
    for(i=0;i<6;i++)
    MSG("wrpfd=0x%04X",cmd2.wrpfd[i]);

    MSG("cabcm=0x%04X",cmd2.cabcm);
    MSG("minbr=0x%04X",cmd2.minbr);

    MSG("labcd=0x%04X",cmd2.labcd);
    MSG("dmin =0x%04X",cmd2.dmin);
    MSG("dmstp=0x%04X",cmd2.dmstp);
    MSG("dmde =0x%04X",cmd2.dmde);
    MSG("dmfix=0x%04X",cmd2.dmfix);

    MSG("cabcd=0x%04X",cmd2.cabcd);
    MSG("dmsps=0x%04X",cmd2.dmsps);
    MSG("dmspm=0x%04X",cmd2.dmspm);
    MSG("dmstc=0x%04X",cmd2.dmstc);
  #endif

  switch(cmd2.mode)
  {
    case 0: 
      reg5300 = 0x0024;
      for(loop=0; loop<BKL_ENG_MODE_LOOP; loop++)
      {
        bkl_write_reg(0x5300,0x0024);
        bkl_write_reg(0x5301,0x0004);
        bkl_write_reg(0x5100,cmd2.dbv);
        msleep(50);
      }
      break;

    case 1: 
      for(loop=0; loop<BKL_ENG_MODE_LOOP; loop++)
      {
        bkl_write_multi_reg(labc_regs);
        reg5300 = cmd2.wrctr;
        bkl_write_reg(0x5300,cmd2.wrctr);
        bkl_write_reg(0x5301,cmd2.ctrle);
        bkl_write_reg(0x5E03,cmd2.labcc);
        for(i=0; i<6 ;i++)
          bkl_write_reg((0x5000+i),cmd2.wrpfd[i]);
        msleep(50);
      }
      break;

    case 2: 
      for(loop=0; loop<BKL_ENG_MODE_LOOP; loop++)
      {
        
        reg5500 = 0x0000; 
        if(cmd2.cabcm==1 || cmd2.cabcm==4) 
          reg5500 = 0x0001;
        else if(cmd2.cabcm==2 || cmd2.cabcm==5) 
          reg5500 = 0x0002;
        else if(cmd2.cabcm==3 || cmd2.cabcm==6) 
          reg5500 = 0x0003;
        
        if(cmd2.cabcm==1 || cmd2.cabcm==2 || cmd2.cabcm==3) 
          reg5300 = 0x002C;
        else if(cmd2.cabcm==4 || cmd2.cabcm==5 || cmd2.cabcm==6) 
          reg5300 = 0x003C;
        bkl_write_reg(0x5300,reg5300);
        bkl_write_reg(0x5500,reg5500);
        bkl_write_reg(0x5E00,cmd2.minbr);
        msleep(50);
      }
      break;

    case 3: 
      for(loop=0; loop<BKL_ENG_MODE_LOOP; loop++)
      {
        bkl_write_reg(0x6A02,cmd2.pwmdi);   
        reg6A01 = cmd2.pwmdu; 
        if(cmd2.clock)  SET_BIT(reg6A01,7); 
        else            CLR_BIT(reg6A01,7); 
        bkl_write_reg(0x6A01,reg6A01);
        reg5300 = 0x0024;
        reg5301 = 0x0004;
        if(cmd2.drivi)  SET_BIT(reg5301,4); 
        else            CLR_BIT(reg5301,4); 
        if(cmd2.logic)  SET_BIT(reg5301,3); 
        else            CLR_BIT(reg5301,3); 
        if(cmd2.pwmin)  SET_BIT(reg5301,2); 
        else            CLR_BIT(reg5301,2); 
        if(cmd2.bldim)  SET_BIT(reg5300,2); 
        else            CLR_BIT(reg5300,2); 
        bkl_write_reg(0x5300,reg5300);
        bkl_write_reg(0x5301,reg5301);
        if(cmd2.blena)  
          gpio_set_value(57,1); 
        else
          gpio_set_value(57,0); 
        msleep(50);
      }
      break;

    case 4: 
      for(loop=0; loop<BKL_ENG_MODE_LOOP; loop++)
      {
        if(cmd2.labcd)
          bkl_write_reg(0x5302,0x0001); 
        else
          bkl_write_reg(0x5302,0x0000);
        bkl_write_reg(0x5303,cmd2.dmin);
        bkl_write_reg(0x5304,cmd2.dmde);
        bkl_write_reg(0x5305,cmd2.dmstp);
        bkl_write_reg(0x5306,cmd2.dmfix);
        msleep(50);
      }
      break;

    case 5: 
      for(loop=0; loop<BKL_ENG_MODE_LOOP; loop++)
      {
        if(cmd2.cabcd)  SET_BIT(reg5300,3); 
        else            CLR_BIT(reg5300,3); 
        bkl_write_reg(0x5300,reg5300);
        bkl_write_reg(0x5307,cmd2.dmsps);
        bkl_write_reg(0x5308,cmd2.dmspm);
        bkl_write_reg(0x5309,cmd2.dmstc);
        msleep(50);
      }
      break;
  }
  return sizeof(struct bkl_cmd2);
}
int bkl_eng_mode2_read(void *buf, unsigned int sizeMax)
{
  struct bkl_cmd2_read cmd2_read;
  unsigned int  reg5A00, reg5A01, reg5B00, reg5B01, reg5400, reg5600;
  unsigned int  regTemp;
  int i;

  if(sizeMax < sizeof(struct bkl_cmd2_read))
    return 0;

  for(i=0; i<10; i++)
  {
    bkl_read_reg(0x5A00,&reg5A00);  
    bkl_read_reg(0x5B00,&reg5B00);  
    bkl_read_reg(0x5A00,&regTemp);  
    if(regTemp == reg5A00)
      break;
  }
  for(i=0; i<10; i++)
  {
    bkl_read_reg(0x5A01,&reg5A01);  
    bkl_read_reg(0x5B01,&reg5B01);  
    bkl_read_reg(0x5A01,&regTemp);  
    if(regTemp == reg5A01)
      break;
  }
  cmd2_read.fsval = (reg5A00 << 8) + reg5B00;
  cmd2_read.alsva = (reg5A01 << 8) + reg5B01;

  
  bkl_read_reg(0x5400,&reg5400);
  bkl_read_reg(0x5600,&reg5600);

  cmd2_read.mode = 0;
  if     (reg5400 == 0x24)  cmd2_read.mode = 1;
  else if(reg5400 == 0x34)  cmd2_read.mode = 2;
  else if(reg5400 == 0x2c)  
  {
    if     (reg5600 == 0x00)cmd2_read.mode = 3;
    else if(reg5600 == 0x01)cmd2_read.mode = 4;
    else if(reg5600 == 0x02)cmd2_read.mode = 5;
    else if(reg5600 == 0x03)cmd2_read.mode = 6;
  }
  else if(reg5400 == 0x3c)  
  {
    if     (reg5600 == 0x00)cmd2_read.mode = 0;
    else if(reg5600 == 0x01)cmd2_read.mode = 7;
    else if(reg5600 == 0x02)cmd2_read.mode = 8;
    else if(reg5600 == 0x03)cmd2_read.mode = 9;
  }

  memcpy(buf, &cmd2_read, sizeof(struct bkl_cmd2_read));

  MSG("%s: alsva=%4d, fsval=%4d",__func__,cmd2_read.alsva,cmd2_read.fsval);

  return sizeof(struct bkl_cmd2_read);
}
int bkl_eng_mode2_read_als(void)
{
  unsigned int  reg5A00, reg5B00, regTemp, als;
  int i;

  bkl_read_reg(0x5A00,&regTemp);    
  for(i=0; i<5; i++)
  {
    bkl_read_reg(0x5B00,&reg5B00);  
    bkl_read_reg(0x5A00,&reg5A00);  
    if(reg5A00 == regTemp)
      break;
    else
      regTemp = reg5A00;
  }
  als = (reg5A00 << 8) + reg5B00;

  return als; 
}


EXPORT_SYMBOL(bkl_eng_mode2_write);
EXPORT_SYMBOL(bkl_eng_mode2_read);
EXPORT_SYMBOL(bkl_eng_mode2_read_als);


