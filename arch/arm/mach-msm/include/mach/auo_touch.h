
#ifndef _AUO_TOUCH_H_
#define _AUO_TOUCH_H_

#ifdef CONFIG_AUO_5INCH_TOUCHSCREEN
#define USE_AUO_5INCH       1

/* Touchscreen Co-ordinates Settings
	#define TOUCH_FB_PORTRAIT   1						
*/ /* Jagan */

#if defined (CONFIG_MACH_EVB) || defined(CONFIG_MACH_EVT0)
#define CONFIG_PANEL        0
#define PERIODICAL_INT_MODE 1
#define TOUCH_INDI_INT_MODE 0
#define COORD_COMP_INT_MODE 0
#elif defined (CONFIG_MACH_EVT0_1)
#define CONFIG_PANEL        1
#define PERIODICAL_INT_MODE 0
#define TOUCH_INDI_INT_MODE 0
#define COORD_COMP_INT_MODE 1
#else
#define CONFIG_PANEL        1
#define PERIODICAL_INT_MODE 0
#define TOUCH_INDI_INT_MODE 0
#define COORD_COMP_INT_MODE 1
#define USE_PIXCIR
#endif
#else
#define USE_AUO_5INCH       0
#define CONFIG_PANEL        1
#define PERIODICAL_INT_MODE 0
#define TOUCH_INDI_INT_MODE 1
#define COORD_COMP_INT_MODE 0
#endif



#define AUO_REPORT_POINTS     2
#define AUO_INT_ENABLE        0x08
#define AUO_INT_POL_HIGH      0x04
#define AUO_INT_POL_LOW       0x00
#define AUO_INT_MODE_MASK     0x03
#define AUO_INT_SETTING_MASK  0x0F
#define AUO_POWER_MODE_MASK   0x03

#if USE_AUO_5INCH

#if defined(TOUCH_FB_PORTRAIT)
	#define AUO_X_MAX	     480
	#define AUO_Y_MAX	     800
#else
	#define AUO_X_MAX	     800
	#define AUO_Y_MAX	     480
#endif
#ifdef USE_PIXCIR
#define AUO_X1_LSB           0x00
#define AUO_X_SENSITIVITY    0x70
#define AUO_Y_SENSITIVITY    0x6F
#define AUO_INT_SETTING      0x71
#define AUO_INT_WIDTH        0x72
#define AUO_POWER_MODE       0x73
#define AUO_EEPROM_CALIB_X   0x42
#define AUO_EEPROM_CALIB_X_LEN 62
#define AUO_EEPROM_CALIB_Y   0xAD
#define AUO_EEPROM_CALIB_Y_LEN 36
#define AUO_RAW_DATA_Y       0x4F
#define AUO_RAW_DATA_Y_LEN   0x0B
#define AUO_RAW_DATA_X       0x2B
#define AUO_RAW_DATA_X_LEN   0x12
#define AUO_TOUCH_AREA_X     0x1E
#define AUO_TOUCH_AREA_Y     0x1F
#define AUO_TOUCH_STRENGTH_ENA 0x0D
#define AUO_TOUCH_STRENGTH_X 0x0E
#else
#define AUO_X1_LSB           0x40
#define AUO_X_SENSITIVITY    0x65
#define AUO_Y_SENSITIVITY    0x64
#define AUO_INT_SETTING      0x66
#define AUO_INT_WIDTH        0x67
#define AUO_POWER_MODE       0x69
#endif

#define AUO_PERIODICAL_ENA   (0x00 | AUO_INT_POL_HIGH | AUO_INT_ENABLE)
#define AUO_COMP_COORD_ENA   (0x01 | AUO_INT_POL_HIGH | AUO_INT_ENABLE)
#define AUO_TOUCH_IND_ENA    (0x02 | AUO_INT_POL_HIGH | AUO_INT_ENABLE)
#define AUO_ACTIVE_MODE      0x00
#define AUO_SLEEP_MODE       0x01
#define AUO_DEEP_SLEEP_MODE  0x02

#if PERIODICAL_INT_MODE
#define AUO_INT_SETTING_VAL  AUO_PERIODICAL_ENA
#endif
#if COORD_COMP_INT_MODE
#define AUO_INT_SETTING_VAL  AUO_COMP_COORD_ENA
#endif
#if TOUCH_INDI_INT_MODE
#define AUO_INT_SETTING_VAL  AUO_TOUCH_IND_ENA
#endif

#else

#define AUO_X_MAX	     480
#define AUO_Y_MAX	     272
#define AUO_X1_LSB           0x40
#define AUO_X_SENSITIVITY    0x67
#define AUO_Y_SENSITIVITY    0x68
#define AUO_INT_SETTING      0x6E
#define AUO_INT_WIDTH        0x6F
#define AUO_POWER_MODE       0x70

#define AUO_PERIODICAL_ENA   (0x00 | AUO_INT_POL_HIGH | AUO_INT_ENABLE)
#define AUO_TOUCH_IND_ENA    (0x01 | AUO_INT_POL_HIGH | AUO_INT_ENABLE)
#if PERIODICAL_INT_MODE
#define AUO_INT_SETTING_VAL  AUO_PERIODICAL_ENA
#endif
#if TOUCH_INDI_INT_MODE
#define AUO_INT_SETTING_VAL  AUO_TOUCH_IND_ENA
#endif

#endif

#define TS_PENUP_TIMEOUT_MS 10

enum auo_touch_state {
    RELEASE = 0,
    PRESS,
};

enum auo_mt_state {
    NOT_TOUCH_STATE = 0,
    ONE_TOUCH_STATE,
    TWO_TOUCH_STATE,
    MAX_TOUCH_STATE
};

enum auo_touch_power_mode {
    UNKNOW_POWER_MODE = 0,
    ACTIVE_POWER_MODE,
    SLEEP_POWER_MODE,
    DEEP_SLEEP_POWER_MODE,
    REAL_SLEEP_POWER_MODE,
    MAX_POWER_MODE,
};

enum auo_touch_strength_mode {
    KEEP_READ = 0,
    ENABLE_READ,
    DISABLE_READ,
};

struct auo_touchscreen_platform_data {
    unsigned int x_max;
    unsigned int y_max;
    unsigned int gpioirq;
    unsigned int gpiorst;
};

struct auo_reg_map_t {
    unsigned char x1_lsb;
    unsigned char x_sensitivity;
    unsigned char y_sensitivity;
    unsigned char int_setting;
    unsigned char int_width;
    unsigned char power_mode;
};

struct auo_point_t {
    uint   x;
    uint   y;
};

struct auo_point_info_t {
    struct auo_point_t   coord;
    enum auo_touch_state state;
};

struct auo_mt_t {
    struct auo_point_info_t points[AUO_REPORT_POINTS];
    enum auo_mt_state       state;
};

#if USE_AUO_5INCH
struct auo_sensitivity_t {
    uint   value;
};

struct auo_interrupt_t {
    uint   int_setting;
    uint   int_width;
};

struct auo_power_mode_t {
    enum auo_touch_power_mode   mode;
};

struct auo_power_switch_t {
    uint   on;
};

struct auo_eeprom_calib_t {
    char *raw_buf_x;
    char *raw_buf_y;
    uint raw_len_x;
    uint raw_len_y;
};

struct auo_raw_data_t {
    int  *raw_buf_x;
    int  *raw_buf_y;
    uint raw_len_x;
    uint raw_len_y;
    enum auo_touch_strength_mode mode;
};

struct auo_touch_area_t {
    uint area_x;
    uint area_y;
};

struct auo_touch_strength_t {
    int strength_x;
    int strength_y;
    enum auo_touch_strength_mode mode;
};

#define AUO_TOUCH_IOCTL_MAGIC 0xA5
#define AUO_TOUCH_IOCTL_GET_X_SENSITIVITY \
	_IOR(AUO_TOUCH_IOCTL_MAGIC, 1, struct auo_sensitivity_t )
#define AUO_TOUCH_IOCTL_GET_Y_SENSITIVITY \
	_IOR(AUO_TOUCH_IOCTL_MAGIC, 2, struct auo_sensitivity_t )
#define AUO_TOUCH_IOCTL_SET_X_SENSITIVITY \
	_IOW(AUO_TOUCH_IOCTL_MAGIC, 3, struct auo_sensitivity_t )
#define AUO_TOUCH_IOCTL_SET_Y_SENSITIVITY \
	_IOW(AUO_TOUCH_IOCTL_MAGIC, 4, struct auo_sensitivity_t )
#define AUO_TOUCH_IOCTL_SET_INT_SETTING \
	_IOW(AUO_TOUCH_IOCTL_MAGIC, 5, struct auo_interrupt_t )
#define AUO_TOUCH_IOCTL_GET_INT_SETTING \
	_IOR(AUO_TOUCH_IOCTL_MAGIC, 6, struct auo_interrupt_t )
#define AUO_TOUCH_IOCTL_SET_INT_WIDTH \
	_IOW(AUO_TOUCH_IOCTL_MAGIC, 7, struct auo_interrupt_t )
#define AUO_TOUCH_IOCTL_GET_INT_WIDTH \
	_IOR(AUO_TOUCH_IOCTL_MAGIC, 8, struct auo_interrupt_t )
#define AUO_TOUCH_IOCTL_SET_POWER_MODE \
        _IOW(AUO_TOUCH_IOCTL_MAGIC, 9, struct auo_power_mode_t )
#define AUO_TOUCH_IOCTL_GET_POWER_MODE \
        _IOR(AUO_TOUCH_IOCTL_MAGIC, 10, struct auo_power_mode_t )
#define AUO_TOUCH_IOCTL_POWER_SWITCH \
        _IOW(AUO_TOUCH_IOCTL_MAGIC, 11, struct auo_power_switch_t )
#define AUO_TOUCH_IOCTL_GET_EEPROM_CALIB \
        _IOR(AUO_TOUCH_IOCTL_MAGIC, 12, struct auo_eeprom_calib_t )
#define AUO_TOUCH_IOCTL_START_CALIB \
        _IOW(AUO_TOUCH_IOCTL_MAGIC, 13, unsigned short )
#define AUO_TOUCH_IOCTL_GET_RAW_DATA \
        _IOR(AUO_TOUCH_IOCTL_MAGIC, 14, struct auo_raw_data_t )
#define AUO_TOUCH_IOCTL_RESET_PANEL \
        _IOW(AUO_TOUCH_IOCTL_MAGIC, 15, unsigned short )
#define AUO_TOUCH_IOCTL_GET_TOUCH_AREA \
        _IOR(AUO_TOUCH_IOCTL_MAGIC, 16, struct auo_touch_area_t )
#define AUO_TOUCH_IOCTL_GET_TOUCH_STRENGTH \
        _IOR(AUO_TOUCH_IOCTL_MAGIC, 17, struct auo_touch_strength_t )
#endif

#endif
