#ifndef _SENSORS_DAEMON_H_
#define _SENSORS_DAEMON_H_
#include <asm/ioctl.h>

struct sensors_daemon_ecompass_t {
	int magnetic_x;
	int magnetic_y;
	int magnetic_z;
	int temperature;
	int azimuth;
	int pitch;
	int roll;
	int accuracy;
};

struct sensors_daemon_gsensor_t {
	int accel_x;
	int accel_y;
	int accel_z;
};

struct sensors_daemon_status_t {
	unsigned int status;
	unsigned int activesensors;
	int em_inuse;
};

enum SENSORS_DAEMON_STATUS
{
	SENSORS_DAEMON_RUNNING = 0x1,
	SENSORS_DAEMON_NO_ACTIVE = 0x2,
	SENSORS_DAEMON_SUSPEND = 0x4,
	SENSORS_DAEMON_SLEEP = 0x8,
	SENSORS_DAEMON_EXIT = 0x10
};

struct hal_status {
	int hal_status;
	int hal_count;
};

#define SENSORS_DAEMON_IOC_MAGIC 	'X'
#define SENSORS_DAEMON_IOC_SET_ACTIVE_SENSORS		_IOW(SENSORS_DAEMON_IOC_MAGIC, 0, unsigned int)
#define SENSORS_DAEMON_IOC_GET_ACTIVE_SENSORS		_IOR(SENSORS_DAEMON_IOC_MAGIC, 1, unsigned int)
#define SENSORS_DAEMON_IOC_SET_ECOMPASS				_IOW(SENSORS_DAEMON_IOC_MAGIC, 2, struct sensors_daemon_ecompass_t)
#define SENSORS_DAEMON_IOC_GET_ECOMPASS			_IOR(SENSORS_DAEMON_IOC_MAGIC, 3, struct sensors_daemon_ecompass_t)
#define SENSORS_DAEMON_IOC_SET_GSENSOR				_IOW(SENSORS_DAEMON_IOC_MAGIC, 4, struct sensors_daemon_gsensor_t)
#define SENSORS_DAEMON_IOC_GET_GSENSOR				_IOR(SENSORS_DAEMON_IOC_MAGIC, 5, struct sensors_daemon_gsensor_t)
#define SENSORS_DAEMON_IOC_GET_STATUS				_IOR(SENSORS_DAEMON_IOC_MAGIC, 6, struct sensors_daemon_status_t)
#define SENSORS_DAEMON_IOC_HAL_INUSE				_IOW(SENSORS_DAEMON_IOC_MAGIC, 7, unsigned int)
#define SENSORS_DAEMON_IOC_EM_INUSE					_IOW(SENSORS_DAEMON_IOC_MAGIC, 8, unsigned int)
#define SENSORS_DAEMON_IOC_BLOCKING					_IO(SENSORS_DAEMON_IOC_MAGIC, 9)
#define SENSORS_DAEMON_IOC_SET_DELAY_MS				_IOW(SENSORS_DAEMON_IOC_MAGIC, 10, int)
#define SENSORS_DAEMON_IOC_GET_DELAY_MS				_IOR(SENSORS_DAEMON_IOC_MAGIC, 11, int)
#define SENSORS_DAEMON_IOC_SET_WAKEUP				_IOW(SENSORS_DAEMON_IOC_MAGIC, 12, int)
#define SENSORS_DAEMON_IOC_GET_WAKEUP				_IOR(SENSORS_DAEMON_IOC_MAGIC, 13, int)
#define SENSORS_DAEMON_IOC_GET_HW_VER				_IOR(SENSORS_DAEMON_IOC_MAGIC, 14, unsigned int)
#define SENSORS_DAEMON_IOC_SET_HAL_STATUS			_IOW(SENSORS_DAEMON_IOC_MAGIC, 15, struct hal_status)

#endif

