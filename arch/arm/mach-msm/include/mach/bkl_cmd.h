
#ifndef _BKL_CMD_H_
#define _BKL_CMD_H_

enum {
  LCD_BKL_MODE_NORMAL = 0,
  LCD_BKL_MODE_LABC,
  LCD_BKL_MODE_CABC_UI,
  LCD_BKL_MODE_CABC_STILL,
  LCD_BKL_MODE_CABC_MOVING,
  LCD_BKL_MODE_LABC_CABC_UI,
  LCD_BKL_MODE_LABC_CABC_STILL,
  LCD_BKL_MODE_LABC_CABC_MOVING,
  LCD_BKL_MODE_MAX,
  LCD_BKL_MODE_TEST=100,
  };

#define MYBIT(b)        (1<<b)
#define GET_BIT(x,b)    ((x & (1<<b)) ? 1 : 0)
#define CLR_BIT(x,b)    (x &= (~(1<<b)))
#define SET_BIT(x,b)    (x |= (1<<b))

struct bkl_reg {
  unsigned int addr;
  unsigned int data;
};

struct bkl_cmd2 {

  unsigned short mode;

  unsigned short dbv;

  unsigned short pwmdi;

  unsigned short pwmdu;

  unsigned short drivi;

  unsigned short clock;

  unsigned short logic;

  unsigned short pwmin;

  unsigned short blena;

  unsigned short bldim;

  unsigned short wrctr;

  unsigned short ctrle;

  unsigned short labcc;

  unsigned short wrpfd[6];

  unsigned short cabcm;

  unsigned short minbr;

  unsigned short labcd;

  unsigned short dmin;

  unsigned short dmstp;

  unsigned short dmde;

  unsigned short dmfix;

  unsigned short cabcd;

  unsigned short dmsps;

  unsigned short dmspm;

  unsigned short dmstc;
};

struct bkl_cmd2_read {

  unsigned short alsva;

  unsigned short fsval;

  unsigned short mode;
};

#ifdef CONFIG_FB_MSM
int bkl_eng_mode2_write(void *buf, unsigned int size);
int bkl_eng_mode2_read(void *buf, unsigned int sizeMax);
int bkl_eng_mode2_read_als(void);
#else
static inline int bkl_eng_mode2_write(void *buf, unsigned int size)   { return 0; }
static inline int bkl_eng_mode2_read(void *buf, unsigned int sizeMax) { return 0; }
static inline int bkl_eng_mode2_read_als(void)    { return 0; }
#endif

#define BKLMODE_IOC_MAGIC       'm'
#define BKLMODE_IOC_NORMAL            _IO(BKLMODE_IOC_MAGIC, 1)
#define BKLMODE_IOC_LABC              _IO(BKLMODE_IOC_MAGIC, 2)
#define BKLMODE_IOC_CABC_UI           _IO(BKLMODE_IOC_MAGIC, 3)
#define BKLMODE_IOC_CABC_STILL        _IO(BKLMODE_IOC_MAGIC, 4)
#define BKLMODE_IOC_CABC_MOVING       _IO(BKLMODE_IOC_MAGIC, 5)
#define BKLMODE_IOC_LABC_CABC_UI      _IO(BKLMODE_IOC_MAGIC, 6)
#define BKLMODE_IOC_LABC_CABC_STILL   _IO(BKLMODE_IOC_MAGIC, 7)
#define BKLMODE_IOC_LABC_CABC_MOVING  _IO(BKLMODE_IOC_MAGIC, 8)

#endif

