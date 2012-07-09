#ifndef _B4325_FM_REGS_H
#define _B4325_FM_REGS_H

#include <linux/ioctl.h>


#define 	FM_WAKEUP					77
#define 	LOCAL_I2C_BUFFER_SIZE		4
#define 	RDS_WATER_LEVEL			50

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE 
#define FALSE 0
#endif

#ifdef DEBUG
#define BCOMFM_INFO(fmt, arg...) printk(KERN_INFO "bcom_fm: " fmt "\n" , ## arg)
#define BCOMFM_DBG(fmt, arg...)  printk(KERN_INFO "%s: " fmt "\n" , __FUNCTION__ , ## arg)
#define BCOMFM_ERR(fmt, arg...)  printk(KERN_ERR  "%s: " fmt "\n" , __FUNCTION__ , ## arg)
#else
#define BCOMFM_INFO(fmt, arg...) 
#define BCOMFM_DBG(fmt, arg...)  
#define BCOMFM_ERR(fmt, arg...)  
#endif

#define BCOMFM_MAX_TUNING_TRIALS	10

#define I2C_FM_RDS_SYSTEM      		0x00
#define I2C_FM_CTRL            		0x01
#define I2C_RDS_CTRL0	      		0x02
#define I2C_RDS_CTRL1      		0x03
#define I2C_FM_AUDIO_PAUSE      	0x04
#define I2C_FM_AUDIO_CTRL0      	0x05
#define I2C_FM_AUDIO_CTRL1	      	0x06
#define I2C_FM_SEARCH_CTRL0      	0x07
#define I2C_FM_SEARCH_CTRL1      	0x08
#define I2C_FM_SEARCH_TUNE_MODE      	0x09
#define I2C_FM_FREQ0      		0x0a
#define I2C_FM_FREQ1      		0x0b
#define I2C_FM_AF_FREQ0      		0x0c
#define I2C_FM_AF_FREQ1      		0x0d
#define I2C_FM_CARRIER      		0x0e
#define I2C_FM_RSSI      		0x0f
#define I2C_FM_RDS_MASK0      		0x10
#define I2C_FM_RDS_MASK1      		0x11
#define I2C_FM_RDS_FLAG0      		0x12
#define I2C_FM_RDS_FLAG1      		0x13
#define I2C_RDS_WLINE      		0x14
#define I2C_RDS_BLKB_MATCH0      	0x16
#define I2C_RDS_BLKB_MATCH1      	0x17
#define I2C_RDS_BLKB_MASK0      	0x18
#define I2C_RDS_BLKB_MASK1      	0x19
#define I2C_RDS_PI_MATCH0      		0x1a
#define I2C_RDS_PI_MATCH1      		0x1b
#define I2C_RDS_PI_MASK0      		0x1c
#define I2C_RDS_PI_MASK1      		0x1d
#define I2C_FM_RDS_BOOT      		0x1e
#define I2C_FM_RDS_TEST      		0x1f
#define I2C_SPARE0      		0x20
#define I2C_SPARE1      		0x21
#define I2C_FM_RDS_REV_ID      		0x28
#define I2C_SLAVE_CONFIGURATION      	0x29
#define I2C_FM_PCM_ROUTE      		0x4d
#define I2C_RDS_DATA      		0x80
#define I2C_FM_BEST_TUNE      		0x90
#define I2C_FM_SEARCH_METHOD      	0xfc
#define I2C_FM_SEARCH_STEPS      	0xfd
#define I2C_FM_MAX_PRESET      		0xfe
#define I2C_FM_PRESET_STATION      	0xff

enum  {
    BCOMFM_RDS_MODE,
    BCOMFM_RBDS_MODE
};

enum
{
    BCOMFM_BAND_EUUS,
    BCOMFM_BAND_JP
};


enum
{
    BCOMFM_MUTE_DISABLED,
    BCOMFM_MUTE_ENABLED
};


enum
{
    BCOMFM_SCAN_DIR_UP,
    BCOMFM_SCAN_DIR_DOWN
};


typedef struct  {
	uint16_t  rds_data_block[RDS_WATER_LEVEL];
	uint8_t   rds_block_id[RDS_WATER_LEVEL];
	uint8_t   rds_block_errors[RDS_WATER_LEVEL];
}bcom_fm_rds_parameters;


struct bcom_fm_platform_data {
	uint16_t 	gpio_fm_wakeup;
};


#define BCOMFM_POWER_PWF         (1 << 0)
#define BCOMFM_POWER_PWR         (1 << 1)

#define BCOMFM_AUTO_SELECT_STEREO_MONO		(1<<1)
#define BCOMFM_MANUAL_SELECT__STEREO_MONO	(0<<1)
#define BCOMFM_MANUAL_SET_STEREO				(1<<2)

#define BCOMFM_STEREO_MONO_BLEND			(0<<3)
#define BCOMFM_STEREO_MONO_SWITCH			(1<<3)

#define BCOMFM_AUDIO_PAUSE_RSSI_21DB		(0<<0)
#define BCOMFM_AUDIO_PAUSE_RSSI_18DB		(1<<0)
#define BCOMFM_AUDIO_PAUSE_RSSI_15DB		(2<<0)
#define BCOMFM_AUDIO_PAUSE_RSSI_12DB		(3<<0)

#define BCOMFM_AUDIO_PAUSE_RSSI_20MS		(0<<4)
#define BCOMFM_AUDIO_PAUSE_RSSI_26MS		(1<<4)
#define BCOMFM_AUDIO_PAUSE_RSSI_33MS		(2<<4)
#define BCOMFM_AUDIO_PAUSE_RSSI_40MS		(3<<4)

#define BCOMFM_ENABLE_RF_MUTE		(1<<0)
#define BCOMFM_MANUAL_MUTE_ON		(1<<1)
#define BCOMFM_DAC_OUTPUT_LEFT		(1<<2)
#define BCOMFM_DAC_OUTPUT_RIGHT		(1<<3)
#define BCOMFM_AUDIO_ROUTE_DAC		(1<<4)
#define BCOMFM_AUDIO_ROUTE_I2S		(1<<5)
#define BCOMFM_DE_EMPHASIS_75		(1<<6)


#define BCOMFM_SERACH_UP				(1<<7)

#define BCOMFM_STOP_TUNE				(0<<0)
#define BCOMFM_PRE_SET					(1<<0)
#define BCOMFM_AUTO_SERCH				(2<<0)
#define BCOMFM_AF_JUMP					(3<<0)

#define BCOMFM_FLAG_SEARCH_TUNE_DONE	(1<<0)
#define BCOMFM_FLAG_SEARCH_FAIL			(1<<1)
#define BCOMFM_FLAG_RSSI_LOW				(1<<2)
#define BCOMFM_FLAG_CARRIER_ERROR_HIGH	(1<<3)
#define BCOMFM_FLAG_AUDIO_PAUSE_IND		(1<<4)
#define BCOMFM_FLAG_STEREO_DETECTED		(1<<5)
#define BCOMFM_FLAG_STEREO_ACTIVE		(1<<6)

#define BCOMFM_FLAG_RDS_FIFO_WLINE		(1<<1)
#define BCOMFM_FLAG_RDS_B_BLOCK_MATCH	(1<<3)
#define BCOMFM_FLAG_RDS_SYNC_LOST		(1<<4)
#define BCOMFM_FLAG_RDS_PI_MATCH			(1<<5)


#define FM_HCI_WRITE_COMPELTE_EVENT_SIZE 	9

#define BCOM_FM_IOCTL_MAGIC 'k'


#define BCOMFM_POWER_ON_IO \
	_IO(BCOM_FM_IOCTL_MAGIC, 1)

#define BCOMFM_POWER_OFF_IO \
	_IO(BCOM_FM_IOCTL_MAGIC, 2)


#define BCOMFM_SPECIFIED_FREQ_SET \
	_IOW(BCOM_FM_IOCTL_MAGIC, 3, int)

#define BCOMFM_FRE_SCAN \
	_IOW(BCOM_FM_IOCTL_MAGIC, 4, char)


#define BCOMFM_GET_FREQ \
	_IOR(BCOM_FM_IOCTL_MAGIC, 5, int)

#define BCOMFM_GET_RSSI \
	_IOR(BCOM_FM_IOCTL_MAGIC, 6, char)

#define BCOMFM_RDS_POWER_ON_IO \
	_IO(BCOM_FM_IOCTL_MAGIC, 7)

#define BCOMFM_RDS_POWER_OFF_IO \
	_IO(BCOM_FM_IOCTL_MAGIC, 8)


#define BCOMFM_RDS_SET_THRED_PID \
	_IOW(BCOM_FM_IOCTL_MAGIC, 9, int)


#define BCOMFM_READ_FLAG \
	_IOR(BCOM_FM_IOCTL_MAGIC, 10, int)

#define BCOMFM_READ_RDS_DATA \
	_IOR(BCOM_FM_IOCTL_MAGIC, 11, bcom_fm_rds_parameters)

#endif

