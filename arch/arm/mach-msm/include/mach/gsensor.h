#ifndef _GSENSOR_H_
#define _GSENSOR_H_
#include <asm/ioctl.h>

#define BMA150_ORIENTATION_PORTRAIT					0
#define BMA150_ORIENTATION_PORTRAITUPSIDEDOWN		1
#define BMA150_ORIENTATION_LANDSCAPELEFT			2
#define BMA150_ORIENTATION_LANDSCAPERIGHT			3
#define BMA150_ORIENTATION_FACEUP					4
#define BMA150_ORIENTATION_FACEDOWN					5

#define BMA150_WAKE_UP_PAUSE_20MS		0
#define BMA150_WAKE_UP_PAUSE_80MS		1
#define BMA150_WAKE_UP_PAUSE_320MS		2
#define BMA150_WAKE_UP_PAUSE_2560MS		3

#define BMA150_RANGE_2G					0
#define BMA150_RANGE_4G					1
#define BMA150_RANGE_8G					2

#define BMA150_BANDWIDTH_25HZ			0
#define BMA150_BANDWIDTH_50HZ			1
#define BMA150_BANDWIDTH_100HZ			2
#define BMA150_BANDWIDTH_190HZ			3
#define BMA150_BANDWIDTH_375HZ			4
#define BMA150_BANDWIDTH_750HZ			5
#define BMA150_BANDWIDTH_1500HZ			6

#define BMA150_COUNTER_RESET				0
#define BMA150_COUNTER_1_LSB				1
#define BMA150_COUNTER_2_LSB				2
#define BMA150_COUNTER_3_LSB				3

#define BMA150_SAMPLE_1					0
#define BMA150_SAMPLE_3					1
#define BMA150_SAMPLE_5					2
#define BMA150_SAMPLE_7					3

struct gsensor_ioctl_accel_t {
	int accel_x;
	int accel_y;
	int accel_z;
	int interrupt;
};


struct gsensor_ioctl_setting_t {
	int enable_LG;
	int enable_HG;
	int enable_advanced;
	int enable_alert;
	int enable_any_montion;
	int range;
	int bandwidth;
	int threshold_LG;
	int duration_LG;
	int hysteresis_LG;
	int counter_LG;
	int threshold_HG;
	int duration_HG;
	int hysteresis_HG;
	int counter_HG;
	int threshold_any_montion;
	int duration_any_montion;
};

struct gsensor_ioctl_t {
	signed short accel_x;
	signed short accel_y;
	signed short accel_z;
};

struct gsensor_ioctl_calibrate_t {
	unsigned short offset_x;
	unsigned short offset_y;
	unsigned short offset_z;
};

#define GSENSOR_IOC_MAGIC 	'z'
#define GSENSOR_IOC_RESET						_IO(GSENSOR_IOC_MAGIC, 0)
#define GSENSOR_IOC_WRITE						_IOW(GSENSOR_IOC_MAGIC, 1, unsigned int)
#define GSENSOR_IOC_READ						_IOR(GSENSOR_IOC_MAGIC, 2, struct gsensor_ioctl_t)
#define GSENSOR_IOC_REG_INT					_IOW(GSENSOR_IOC_MAGIC, 3, unsigned int)
#define GSENSOR_IOC_EM_SET_PARAMETER		_IOW(GSENSOR_IOC_MAGIC, 4, struct gsensor_ioctl_setting_t)
#define GSENSOR_IOC_EM_GET_ACCEL				_IOR(GSENSOR_IOC_MAGIC, 5, struct gsensor_ioctl_accel_t)
#define GSENSOR_IOC_EM_CALIBRATE				_IOW(GSENSOR_IOC_MAGIC, 6, unsigned int)
#define GSENSOR_IOC_SET_CALIB_PARM			_IOW(GSENSOR_IOC_MAGIC, 7, struct gsensor_ioctl_calibrate_t)
#define GSENSOR_IOC_GET_CALIB_PARM			_IOR(GSENSOR_IOC_MAGIC, 8, struct gsensor_ioctl_calibrate_t)
#define GSENSOR_IOC_READ_DIRECT				_IOR(GSENSOR_IOC_MAGIC, 9, struct gsensor_ioctl_t)
#define GSENSOR_IOC_EM_READ_ID				_IOR(GSENSOR_IOC_MAGIC, 10, unsigned char)
#define GSENSOR_IOC_SET_CALIB_PARM_EEPROM	_IOW(GSENSOR_IOC_MAGIC, 11, struct gsensor_ioctl_calibrate_t)
#endif

