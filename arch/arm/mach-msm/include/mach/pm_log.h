#ifndef __PM_LOG_H_
#define __PM_LOG_H_

enum {
	PM_LOG_ON = 0,
	PM_LOG_OFF
};

enum {
	PM_LOG_TOUCH = 0,
	PM_LOG_SDCARD_INT,
	PM_LOG_SDCARD_EXT,
	PM_LOG_CAMERA,
	PM_LOG_BLUETOOTH,
	PM_LOG_WIFI,
	PM_LOG_AUDIO_SPK,
	PM_LOG_AUDIO_HS,
	PM_LOG_AUTIO_MIC,
	PM_LOG_BL_KEYPAD,
	PM_LOG_BL_LCD,
	PM_LOG_FLASHLIGHT,
	PM_LOG_LCD,
	PM_LOG_HDMI,
	PM_LOG_VIBRATOR,
	PM_LOG_SENSOR_CAP,
	PM_LOG_SENSOR_PROXIMITY,
	PM_LOG_SENSOR_ALS,
	PM_LOG_SENSOR_ECOMPASS,
	PM_LOG_NUM
};

#define	PM_LOG_NAME	\
{	\
	"Touch",	\
	"SD Card (internal)",	\
	"SD Card (external)",	\
	"Camera",		\
	"Bluetooth",		\
	"WiFi",			\
	"Audio Speaker",	\
	"Audio Headset",	\
	"Audio Microphone",	\
	"Backlight (Keypad)",	\
	"Backlight (LCD)",	\
	"Flashlight",		\
	"LCD",			\
	"HDMI",			\
	"Vibrator",		\
	"Sensor (Cap)",		\
	"Sensor (Proximity)",	\
	"Sensor (ALS)",		\
	"Sensor (E-Compass)",	\
}

#ifdef CONFIG_PM_LOG
#define	PM_LOG_EVENT(x,y)	pm_log_event(x,y)
#else
#define	PM_LOG_EVENT(x,y)	do{} while(0)
#endif

void pm_log_event(int mode, unsigned which_src);

#endif
