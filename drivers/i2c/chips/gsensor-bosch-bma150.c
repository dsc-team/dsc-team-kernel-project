#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <mach/gpio.h>
#include "gsensor-bosch-bma150.h"

#include <mach/pm_log.h>

#define DBG_MONITOR 0

#define GSENSOR_LOG_ENABLE 1
#define GSENSOR_LOG_DBG 0
#define GSENSOR_LOG_INFO 0
#define GSENSOR_LOG_WARNING 1
#define GSENSOR_LOG_ERR 1

#if GSENSOR_LOG_ENABLE
	#if GSENSOR_LOG_DBG
		#define GSENSOR_LOGD printk
#else
		#define GSENSOR_LOGD(a...) {}
	#endif

	#if GSENSOR_LOG_INFO
		#define GSENSOR_LOGI printk
	#else
		#define GSENSOR_LOGI(a...) {}
	#endif

	#if GSENSOR_LOG_WARNING
		#define GSENSOR_LOGW printk
	#else
		#define GSENSOR_LOGW(a...) {}
	#endif

	#if GSENSOR_LOG_ERR
		#define GSENSOR_LOGE printk
	#else
		#define GSENSOR_LOGE(a...) {}
	#endif
#else
	#define GSENSOR_LOGD(a...) {}
	#define GSENSOR_LOGI(a...) {}
	#define GSENSOR_LOGW(a...) {}
	#define GSENSOR_LOGE(a...) {}
#endif

#define GSENSOR_NAME "gsensor_bma150"
#define GSENSOR_PHYSLEN 128

#define BMA150_VENDORID                  0x0001
#define BMA150_CALIBRATION_MAX_TRIES 10
#define BMA150_CALIBRATION_MAX_VERIFY_FAIL_TIMES 20

struct orientation_accel_t
{
	short	x;
	short	y;
	short	z;
};

struct gsensor_accel_data_t
{
	int              accel_x;
	int              accel_y;
	int              accel_z;
	int              temp;
};

struct gsensor_driver_data_t
{
	struct i2c_client *i2c_gsensor_client;
	struct work_struct        work_data;
	struct gsensor_accel_data_t curr_acc_data;
	struct gsensor_ioctl_calibrate_t calibrate_data;
	atomic_t	open_count;
	short		offset_lsb;
	#if DBG_MONITOR
	struct timer_list dbg_monitor_timer;
	struct work_struct dbg_monitor_work_data;
	#endif
	uint8_t chip_id;
};

static struct gsensor_driver_data_t gsensor_driver_data;
static struct gsensor_ioctl_setting_t gsensor_ioctl_setting;
#ifdef CONFIG_HW_AUSTIN
static short calibrate_table[6][4] = {
	{BMA150_ORIENTATION_PORTRAIT, 0, 1, 0},
	{BMA150_ORIENTATION_PORTRAITUPSIDEDOWN, 0, -1, 0},
	{BMA150_ORIENTATION_LANDSCAPELEFT, -1, 0, 0},
	{BMA150_ORIENTATION_LANDSCAPERIGHT, 1, 0, 0},
	{BMA150_ORIENTATION_FACEUP, 0, 0, -1},
	{BMA150_ORIENTATION_FACEDOWN, 0, 0, 1}
};
#else
static short calibrate_table[6][4] = {
	{BMA150_ORIENTATION_PORTRAIT, -1, 0, 0},
	{BMA150_ORIENTATION_PORTRAITUPSIDEDOWN, 1, 0, 0},
	{BMA150_ORIENTATION_LANDSCAPELEFT, 0, 1, 0},
	{BMA150_ORIENTATION_LANDSCAPERIGHT, 0, -1, 0},
	{BMA150_ORIENTATION_FACEUP, 0, 0, 1},
	{BMA150_ORIENTATION_FACEDOWN, 0, 0, -1}
};
#endif

DECLARE_MUTEX(sem_open);
DECLARE_MUTEX(sem_data);

static int i2c_gsensor_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int i2c_gsensor_remove(struct i2c_client *client);
static int i2c_gsensor_suspend(struct i2c_client *client, pm_message_t state);
static int i2c_gsensor_resume(struct i2c_client *client);
static int i2c_gsensor_command(struct i2c_client *client, unsigned int cmd, void *arg);

static int misc_gsensor_open(struct inode *inode_p, struct file *fp);
static int misc_gsensor_release(struct inode *inode_p, struct file *fp);
static long misc_gsensor_ioctl(struct file *fp, unsigned int cmd, unsigned long arg);
static ssize_t misc_gsensor_read(struct file *fp, char __user *buf, size_t count, loff_t *f_pos);
static ssize_t misc_gsensor_write(struct file *fp, const char __user *buf, size_t count, loff_t *f_pos);

static int i2c_gsensor_read(struct i2c_client *client, uint8_t regaddr, uint8_t *buf, uint32_t len)
{
	int ret = 0;
	uint8_t ldat = regaddr;
	struct i2c_msg msgs[] = {
		[0] = {
			.addr	= client->addr,
			.flags	= 0,
			.buf	= (void *)&ldat,
			.len	= 1
		},
		[1] = {
			.addr	= client->addr,
			.flags	= I2C_M_RD,
			.buf	= (void *)buf,
			.len	= len
		}
	};

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s+, regaddr:0x%x, len:%d\n", __func__, regaddr, len);

	ret = i2c_transfer(client->adapter, msgs, 2);

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s-ret:%d\n",__func__, ret);
	return ret;
}

static int i2c_gsensor_write(struct i2c_client *client, uint8_t regaddr, uint8_t *buf, uint32_t len)
{
	int ret = 0;
	uint8_t write_buf[32];
	struct i2c_msg msgs[] = {
		[0] = {
			.addr	= client->addr,
			.flags	= 0,
			.buf	= (void *)write_buf,
			.len	= len + 1,
			},
	};

	if (len > 31)
	{
		GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::len > 31!!\n", __func__);
		return -ENOMEM;
	}

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s+regaddr:0x%x, len:%d\n", __func__, regaddr, len);

	write_buf[0] = regaddr;
	memcpy((void *)(write_buf + 1), buf, len);
	ret = i2c_transfer(client->adapter, msgs, 1);

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] i2c_gsensor_write-ret:%d\n", ret);
	return ret;
}

#if DBG_MONITOR
static void debug_monitor(unsigned long data)
{
	struct gsensor_driver_data_t *driver_data = (struct gsensor_driver_data_t *) data;
	schedule_work(&driver_data->dbg_monitor_work_data);
	return;
}

static void debug_monitor_work_func(struct work_struct *work)
{
	int i;
	uint8_t rx_buf[20];
	struct gsensor_driver_data_t *driver_data = container_of(work, struct gsensor_driver_data_t, dbg_monitor_work_data);

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s+\n", __func__);

	memset(rx_buf, 0, sizeof(rx_buf));
	i2c_gsensor_read(driver_data->i2c_gsensor_client, BMA150_REG_ACC_X_LSB, rx_buf, sizeof(rx_buf));

	for (i = 0; i < sizeof(rx_buf); i++)
	{
		GSENSOR_LOGI(KERN_INFO "[GSENSOR] [0x%2x] %2x\n", (sizeof(rx_buf) - i + 1), rx_buf[sizeof(rx_buf) - i - 1]);
	}

	mod_timer(&driver_data->dbg_monitor_timer, jiffies + HZ);

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s-\n", __func__);
	return;
}
#endif

static int gsensor_power_down(struct gsensor_driver_data_t *gsensor_driver_data)
{
	int ret = 0;
	uint8_t data = BMA150_SLEEP_MASK;

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s+\n", __func__);

	ret = i2c_gsensor_write(gsensor_driver_data->i2c_gsensor_client, BMA150_REG_SLEEP, &data, sizeof(data));
	if (ret > 0)
		ret = 0;
	else
	{
		GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_write fail!!-ret:%d\n", __func__, ret);
		return ret;
	}

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s-ret:%d\n", __func__, ret);
	return ret;
}

static int gsensor_power_up(struct gsensor_driver_data_t *gsensor_driver_data)
{
	int ret = 0;
	uint8_t data = 0;

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s+\n", __func__);

	ret = i2c_gsensor_write(gsensor_driver_data->i2c_gsensor_client, BMA150_REG_SLEEP, &data, sizeof(data));
	if (ret > 0)
		ret = 0;
	else
	{
		GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_write fail!!-ret:%d\n", __func__, ret);
		return ret;
	}

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s-ret:%d\n", __func__, ret);
	return ret;
}

static int gsensor_set_ctrl_0b(struct gsensor_driver_data_t *driver_data, struct gsensor_ioctl_setting_t *ioctl_setting)
{
	int ret = 0;
	uint8_t tx_data;
	uint8_t rx_data;
	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s+\n", __func__);

	do
	{
		tx_data = BMA150_RESET_INT_MASK;
		ret = i2c_gsensor_write(driver_data->i2c_gsensor_client, BMA150_REG_RESET_INT, &tx_data, sizeof(tx_data));
		if (ret < 0)
		{
			GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_write fail!!-ret:%d\n", __func__, ret);
			return ret;
		}

		tx_data = BMA150_VALUE_ENABLE_LG(ioctl_setting->enable_LG) | BMA150_VALUE_ENABLE_HG(ioctl_setting->enable_HG)
				| BMA150_VALUE_COUNTER_LG(ioctl_setting->counter_LG) | BMA150_VALUE_COUNTER_HG(ioctl_setting->counter_HG)
				| BMA150_VALUE_ANY_MOTION(ioctl_setting->enable_any_montion) | BMA150_VALUE_ALERT(ioctl_setting->enable_alert);
		ret = i2c_gsensor_write(driver_data->i2c_gsensor_client, BMA150_REG_CTRL_0B, &tx_data, sizeof(tx_data));
		if (ret < 0)
		{
			GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_write fail!!-ret:%d\n", __func__, ret);
			return ret;
		}

		ret = i2c_gsensor_read(driver_data->i2c_gsensor_client, BMA150_REG_CTRL_0B, &rx_data, sizeof(rx_data));
		if (ret < 0)
		{
			GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_read fail!!-ret:%d\n", __func__, ret);
			return ret;
		}

		if (tx_data == rx_data)
			break;
		msleep(20);
		GSENSOR_LOGW(KERN_WARNING "[GSENSOR] %s::tx_data!=rx_data\n", __func__);
	} while(1);

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s-\n", __func__);
	return 0;
}

static int gsensor_set_settings_0c(struct gsensor_driver_data_t *driver_data, struct gsensor_ioctl_setting_t *ioctl_setting)
{
	int ret = 0;
	uint8_t tx_data;
	uint8_t rx_data;
	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s+\n", __func__);

	do
	{
		tx_data = BMA150_RESET_INT_MASK;
		ret = i2c_gsensor_write(driver_data->i2c_gsensor_client, BMA150_REG_RESET_INT, &tx_data, sizeof(tx_data));
		if (ret < 0)
		{
			GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_write fail!!-ret:%d\n", __func__, ret);
			return ret;
		}

		tx_data = ioctl_setting->threshold_LG;
		ret = i2c_gsensor_write(driver_data->i2c_gsensor_client, BMA150_REG_LG_THRES, &tx_data, sizeof(tx_data));
		if (ret < 0)
		{
			GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_write fail!!-ret:%d\n", __func__, ret);
			return ret;
		}

		ret = i2c_gsensor_read(driver_data->i2c_gsensor_client, BMA150_REG_LG_THRES, &rx_data, sizeof(rx_data));
		if (ret < 0)
		{
			GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_read fail!!-ret:%d\n", __func__, ret);
			return ret;
		}

		if (tx_data == rx_data)
			break;
		msleep(20);
		GSENSOR_LOGW(KERN_WARNING "[GSENSOR] %s::tx_data!=rx_data\n", __func__);
	} while(1);

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s-\n", __func__);
	return 0;
}

static int gsensor_set_settings_0d(struct gsensor_driver_data_t *driver_data, struct gsensor_ioctl_setting_t *ioctl_setting)
{
	int ret = 0;
	uint8_t tx_data;
	uint8_t rx_data;
	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s+\n", __func__);

	do
	{
		tx_data = BMA150_RESET_INT_MASK;
		ret = i2c_gsensor_write(driver_data->i2c_gsensor_client, BMA150_REG_RESET_INT, &tx_data, sizeof(tx_data));
		if (ret < 0)
		{
			GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_write fail!!-ret:%d\n", __func__, ret);
			return ret;
		}

		tx_data = ioctl_setting->duration_LG;
		ret = i2c_gsensor_write(driver_data->i2c_gsensor_client, BMA150_REG_LG_DUR, &tx_data, sizeof(tx_data));
		if (ret < 0)
		{
			GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_write fail!!-ret:%d\n", __func__, ret);
			return ret;
		}

		ret = i2c_gsensor_read(driver_data->i2c_gsensor_client, BMA150_REG_LG_DUR, &rx_data, sizeof(rx_data));
		if (ret < 0)
		{
			GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_read fail!!-ret:%d\n", __func__, ret);
			return ret;
		}

		if (tx_data == rx_data)
			break;
		msleep(20);
		GSENSOR_LOGW(KERN_WARNING "[GSENSOR] %s::tx_data!=rx_data\n", __func__);
	} while(1);

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s-\n", __func__);
	return 0;
}

static int gsensor_set_settings_0e(struct gsensor_driver_data_t *driver_data, struct gsensor_ioctl_setting_t *ioctl_setting)
{
	int ret = 0;
	uint8_t tx_data;
	uint8_t rx_data;
	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s+\n", __func__);

	do
	{
		tx_data = BMA150_RESET_INT_MASK;
		ret = i2c_gsensor_write(driver_data->i2c_gsensor_client, BMA150_REG_RESET_INT, &tx_data, sizeof(tx_data));
		if (ret < 0)
		{
			GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_write fail!!-ret:%d\n", __func__, ret);
			return ret;
		}

		tx_data = ioctl_setting->threshold_HG;
		ret = i2c_gsensor_write(driver_data->i2c_gsensor_client, BMA150_REG_HG_THRES, &tx_data, sizeof(tx_data));
		if (ret < 0)
		{
			GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_write fail!!-ret:%d\n", __func__, ret);
			return ret;
		}

		ret = i2c_gsensor_read(driver_data->i2c_gsensor_client, BMA150_REG_HG_THRES, &rx_data, sizeof(rx_data));
		if (ret < 0)
		{
			GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_read fail!!-ret:%d\n", __func__, ret);
			return ret;
		}

		if (tx_data == rx_data)
			break;
		msleep(20);
		GSENSOR_LOGW(KERN_WARNING "[GSENSOR] %s::tx_data!=rx_data\n", __func__);
	} while(1);
	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s-\n", __func__);
	return 0;
}

static int gsensor_set_settings_0f(struct gsensor_driver_data_t *driver_data, struct gsensor_ioctl_setting_t *ioctl_setting)
{
	int ret = 0;
	uint8_t tx_data;
	uint8_t rx_data;
	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s+\n", __func__);

	do
	{
		tx_data = BMA150_RESET_INT_MASK;
		ret = i2c_gsensor_write(driver_data->i2c_gsensor_client, BMA150_REG_RESET_INT, &tx_data, sizeof(tx_data));
		if (ret < 0)
		{
			GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_write fail!!-ret:%d\n", __func__, ret);
			return ret;
		}

		tx_data = ioctl_setting->duration_HG;
		ret = i2c_gsensor_write(driver_data->i2c_gsensor_client, BMA150_REG_HG_DUR, &tx_data, sizeof(tx_data));
		if (ret < 0)
		{
			GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_write fail!!-ret:%d\n", __func__, ret);
			return ret;
		}

		ret = i2c_gsensor_read(driver_data->i2c_gsensor_client, BMA150_REG_HG_DUR, &rx_data, sizeof(rx_data));
		if (ret < 0)
		{
			GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_read fail!!-ret:%d\n", __func__, ret);
			return ret;
		}

		if (tx_data == rx_data)
			break;
		msleep(20);
		GSENSOR_LOGW(KERN_WARNING "[GSENSOR] %s::tx_data!=rx_data\n", __func__);
	} while(1);

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s-\n", __func__);
	return 0;
}

static int gsensor_set_settings_10(struct gsensor_driver_data_t *driver_data, struct gsensor_ioctl_setting_t *ioctl_setting)
{
	int ret = 0;
	uint8_t tx_data;
	uint8_t rx_data;
	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s+\n", __func__);

	do
	{
		tx_data = BMA150_RESET_INT_MASK;
		ret = i2c_gsensor_write(driver_data->i2c_gsensor_client, BMA150_REG_RESET_INT, &tx_data, sizeof(tx_data));
		if (ret < 0)
		{
			GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_write fail!!-ret:%d\n", __func__, ret);
			return ret;
		}

		tx_data = ioctl_setting->threshold_any_montion;
		ret = i2c_gsensor_write(driver_data->i2c_gsensor_client, BMA150_REG_ANY_MOTION_THRES, &tx_data, sizeof(tx_data));
		if (ret < 0)
		{
			GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_write fail!!-ret:%d\n", __func__, ret);
			return ret;
		}

		ret = i2c_gsensor_read(driver_data->i2c_gsensor_client, BMA150_REG_ANY_MOTION_THRES, &rx_data, sizeof(rx_data));
		if (ret < 0)
		{
			GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_read fail!!-ret:%d\n", __func__, ret);
			return ret;
		}

		if (tx_data == rx_data)
			break;
		msleep(20);
		GSENSOR_LOGW(KERN_WARNING "[GSENSOR] %s::tx_data!=rx_data\n", __func__);
	} while(1);

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s-\n", __func__);
	return 0;
}

static int gsensor_set_settings_11(struct gsensor_driver_data_t *driver_data, struct gsensor_ioctl_setting_t *ioctl_setting)
{
	int ret = 0;
	uint8_t tx_data;
	uint8_t rx_data;
	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s+\n", __func__);

	do
	{
		tx_data = BMA150_RESET_INT_MASK;
		ret = i2c_gsensor_write(driver_data->i2c_gsensor_client, BMA150_REG_RESET_INT, &tx_data, sizeof(tx_data));
		if (ret < 0)
		{
			GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_write fail!!-ret:%d\n", __func__, ret);
			return ret;
		}

		tx_data = BMA150_VALUE_LG_HYST(ioctl_setting->hysteresis_LG) | BMA150_VALUE_HG_HYST(ioctl_setting->hysteresis_HG)
				| BMA150_VALUE_ANY_MOTION_DUR(ioctl_setting->duration_any_montion);
		ret = i2c_gsensor_write(driver_data->i2c_gsensor_client, BMA150_REG_ANY_MOTION_DUR, &tx_data, sizeof(tx_data));
		if (ret < 0)
		{
			GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_write fail!!-ret:%d\n", __func__, ret);
			return ret;
		}

		ret = i2c_gsensor_read(driver_data->i2c_gsensor_client, BMA150_REG_ANY_MOTION_DUR, &rx_data, sizeof(rx_data));
		if (ret < 0)
		{
			GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_read fail!!-ret:%d\n", __func__, ret);
			return ret;
		}

		if (tx_data == rx_data)
			break;
		msleep(20);
		GSENSOR_LOGW(KERN_WARNING "[GSENSOR] %s::tx_data!=rx_data\n", __func__);
	} while(1);

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s-\n", __func__);
	return 0;
}

static int gsensor_set_ctrl_14(struct gsensor_driver_data_t *driver_data, struct gsensor_ioctl_setting_t *ioctl_setting)
{
	int ret = 0;
	uint8_t tx_data;
	uint8_t rx_data;
	uint8_t tx_temp;
	uint8_t rx_temp;
	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s+\n", __func__);

	ret = i2c_gsensor_read(driver_data->i2c_gsensor_client, BMA150_REG_BANDWIDTH, &rx_data, sizeof(rx_data));
	if (ret < 0)
	{
		GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_read fail!!-ret:%d\n", __func__, ret);
		return ret;
	}

	do
	{
		tx_data = BMA150_RESET_INT_MASK;
		ret = i2c_gsensor_write(driver_data->i2c_gsensor_client, BMA150_REG_RESET_INT, &tx_data, sizeof(tx_data));
		if (ret < 0)
		{
			GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_write fail!!-ret:%d\n", __func__, ret);
			return ret;
		}

		tx_data = (rx_data & ~(BMA150_BANDWIDTH_MASK |BMA150_RANGE_MASK)) | BMA150_VALUE_BANDWIDTH(ioctl_setting->bandwidth)
				| BMA150_VALUE_RANGE(ioctl_setting->range);
		ret = i2c_gsensor_write(driver_data->i2c_gsensor_client, BMA150_REG_BANDWIDTH, &tx_data, sizeof(tx_data));
		if (ret < 0)
		{
			GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_write fail!!-ret:%d\n", __func__, ret);
			return ret;
		}

		ret = i2c_gsensor_read(driver_data->i2c_gsensor_client, BMA150_REG_BANDWIDTH, &rx_data, sizeof(rx_data));
		if (ret < 0)
		{
			GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_read fail!!-ret:%d\n", __func__, ret);
			return ret;
		}

		rx_temp = rx_data & (BMA150_BANDWIDTH_MASK |BMA150_RANGE_MASK);
		tx_temp = tx_data & (BMA150_BANDWIDTH_MASK |BMA150_RANGE_MASK);

		if (tx_temp == rx_temp)
			break;
		msleep(20);
		GSENSOR_LOGW(KERN_WARNING "[GSENSOR] %s::tx_data!=rx_data\n", __func__);
	} while(1);

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s-\n", __func__);
	return 0;
}

static int gsensor_set_ctrl_15(struct gsensor_driver_data_t *driver_data, struct gsensor_ioctl_setting_t *ioctl_setting)
{
	int ret = 0;
	uint8_t tx_data;
	uint8_t rx_data;
	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s+\n", __func__);

	do
	{
		tx_data = BMA150_RESET_INT_MASK;
		ret = i2c_gsensor_write(driver_data->i2c_gsensor_client, BMA150_REG_RESET_INT, &tx_data, sizeof(tx_data));
		if (ret < 0)
		{
			GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_write fail!!-ret:%d\n", __func__, ret);
			return ret;
		}

		tx_data = BMA150_VALUE_ENABLE_ADV_INT(ioctl_setting->enable_advanced);
		ret = i2c_gsensor_write(driver_data->i2c_gsensor_client, BMA150_REG_ENABLE_ADV_INT, &tx_data, sizeof(tx_data));
		if (ret < 0)
		{
			GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_write fail!!-ret:%d\n", __func__, ret);
			return ret;
		}

		ret = i2c_gsensor_read(driver_data->i2c_gsensor_client, BMA150_REG_ENABLE_ADV_INT, &rx_data, sizeof(rx_data));
		if (ret < 0)
		{
			GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_read fail!!-ret:%d\n", __func__, ret);
			return ret;
		}

		if (tx_data == rx_data)
			break;
		msleep(20);
		GSENSOR_LOGW(KERN_WARNING "[GSENSOR] %s::tx_data!=rx_data\n", __func__);
	} while(1);

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s-\n", __func__);
	return 0;
}

static int gsensor_set_eeprom_write(struct gsensor_driver_data_t *driver_data, int enable_eeprom_write)
{
	int ret = 0;
	uint8_t tx_data;
	uint8_t rx_data;
	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s+\n", __func__);

	do
	{
		tx_data = BMA150_RESET_INT_MASK;
		ret = i2c_gsensor_write(driver_data->i2c_gsensor_client, BMA150_REG_RESET_INT, &tx_data, sizeof(tx_data));
		if (ret < 0)
		{
			GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_write fail!!-ret:%d\n", __func__, ret);
			return ret;
		}

		if (enable_eeprom_write)
			tx_data = 0x10;
		else
			tx_data = 0x0;
		ret = i2c_gsensor_write(driver_data->i2c_gsensor_client, BMA150_REG_EE_W, &tx_data, sizeof(tx_data));
		if (ret < 0)
		{
			GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_write fail!!-ret:%d\n", __func__, ret);
			return ret;
		}

		ret = i2c_gsensor_read(driver_data->i2c_gsensor_client, BMA150_REG_EE_W, &rx_data, sizeof(rx_data));
		if (ret < 0)
		{
			GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_read fail!!-ret:%d\n", __func__, ret);
			return ret;
		}

		if ((tx_data & BMA150_EE_W_MASK) == (rx_data & BMA150_EE_W_MASK))
			break;
		msleep(20);
		GSENSOR_LOGW(KERN_WARNING "[GSENSOR] %s::tx_data!=rx_data\n", __func__);
	} while(1);

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s-\n", __func__);
	return 0;
}
static int gsensor_default_config(struct gsensor_driver_data_t *gsensor_driver_data)
{
	int ret = 0;
	int i;
	uint8_t chipid[2], bandwidth[2];
	uint8_t buf[8];
	uint8_t reg[4] = {BMA150_REG_ANY_MOTION_THRES, BMA150_REG_BANDWIDTH, BMA150_REG_CTRL_15, BMA150_REG_CTRL_0B};
	uint8_t reg_data[4];
	struct i2c_msg msgs[4];

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s+\n", __func__);

	ret = gsensor_power_up(gsensor_driver_data);
	if (ret < 0)
	{
		GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::gsensor_power_up fail!!-ret:%d\n", __func__, ret);
		return ret;
	}

	ret = i2c_gsensor_read(gsensor_driver_data->i2c_gsensor_client, BMA150_REG_CHIP_ID, chipid, sizeof(chipid));
	if (ret < 0)
	{
		GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c read chip id fail!!-ret:%d\n", __func__, ret);
		return ret;
	}

	if ((chipid[0] == 0x00) || (chipid[1] == 0x00)) {
		GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::bma150 accelerometer not detected\n", __func__);
		return -ENODEV;
	}
	GSENSOR_LOGI(KERN_INFO "[GSENSOR] bma150::detected chip id: %d, version: %d\n", chipid[0] & BMA150_CHIP_ID_MASK, chipid[1]);
	gsensor_driver_data->chip_id = chipid[0];

	ret = down_interruptible(&sem_data);
	if (ret != 0)
	{
		GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s:: The sleep is interrupted by a signal\n", __func__);
		return ret;
	}
	if ((chipid[1] & 0x0F) == 0x01)
	{
		gsensor_driver_data->offset_lsb = 8;
	}
	else
	{
		gsensor_driver_data->offset_lsb = 14;
	}
	up(&sem_data);

	ret = i2c_gsensor_read(gsensor_driver_data->i2c_gsensor_client, BMA150_REG_BANDWIDTH, bandwidth, sizeof(bandwidth));
	if (ret < 0)
	{
		GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c read bandwidth fail!!-ret:%d\n", __func__, ret);
		return ret;
	}
	GSENSOR_LOGI(KERN_INFO "[GSENSOR] bma150::bandwidth[0]:0x%x, bandwidth[1]:0x%x\n", bandwidth[0], bandwidth[1]);

	reg_data[0] =	BMA150_ANY_MOTION_THRES_INIT;
	reg_data[1] =	(bandwidth[0] & ~BMA150_BANDWIDTH_MASK) | BMA150_BANDWIDTH_INIT;
	reg_data[1] = (reg_data[1] & ~BMA150_RANGE_MASK) | BMA150_RANGE_INIT;
	reg_data[2] = (BMA150_LATCH_INT_MASK | bandwidth[1]) & ~BMA150_ENABLE_ADV_INT_MASK;
	reg_data[3] = 0;

	for (i = 0; i < 4; i++)
	{
		msgs[i].addr	= gsensor_driver_data->i2c_gsensor_client->addr,
		msgs[i].flags	= 0,
		msgs[i].buf	= (void *)(buf + (i << 1)),
		msgs[i].len	= 2,
		buf[(i << 1)] = reg[i];
		buf[(i << 1) + 1] = reg_data[i];
	}

	ret = i2c_transfer(gsensor_driver_data->i2c_gsensor_client->adapter, msgs, sizeof(msgs) / sizeof(msgs[0]));
	if (ret < 0)
	{
		GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_transfer fail!!-ret:%d\n", __func__, ret);
		return ret;
	}

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s-ret:%d\n", __func__, ret);
	return 0;
}

static int gsensor_calibrate_config(struct gsensor_driver_data_t *driver_data)
{
	int ret = 0;
	uint8_t rx_data;
	uint8_t tx_data;

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s+\n", __func__);

	tx_data = BMA150_RESET_INT_MASK;
	ret = i2c_gsensor_write(driver_data->i2c_gsensor_client, BMA150_REG_RESET_INT, &tx_data, sizeof(tx_data));
	if (ret < 0)
	{
		GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_write fail!!-ret:%d\n", __func__, ret);
		return ret;
	}
	else ret = 0;

	ret = i2c_gsensor_read(driver_data->i2c_gsensor_client, BMA150_REG_CTRL_15, &rx_data, sizeof(rx_data));
	if (ret < 0)
	{
		GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_read fail!!-ret:%d\n", __func__, ret);
		return ret;
	}
	else ret = 0;

	rx_data &= ~(BMA150_ENABLE_ADV_INT_MASK | BMA150_NEW_DATA_INT_MASK);
	tx_data = rx_data;
	ret = i2c_gsensor_write(driver_data->i2c_gsensor_client, BMA150_REG_CTRL_15, &tx_data, sizeof(tx_data));
	if (ret < 0)
	{
		GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_write fail!!-ret:%d\n", __func__, ret);
		return ret;
	}
	else ret = 0;

	ret = i2c_gsensor_read(driver_data->i2c_gsensor_client, BMA150_REG_CTRL_0B, &rx_data, sizeof(rx_data));
	if (ret < 0)
	{
		GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_read fail!!-ret:%d\n", __func__, ret);
		return ret;
	}
	else ret = 0;

	rx_data &= ~(BMA150_ALERT_MASK | BMA150_ANY_MOTION_MASK | BMA150_ENABLE_HG_MASK | BMA150_ENABLE_LG_MASK);
	tx_data = rx_data;
	ret = i2c_gsensor_write(driver_data->i2c_gsensor_client, BMA150_REG_CTRL_0B, &tx_data, sizeof(tx_data));
	if (ret < 0)
	{
		GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_write fail!!-ret:%d\n", __func__, ret);
		return ret;
	}
	else ret = 0;

	tx_data = BMA150_RESET_INT_MASK;
	ret = i2c_gsensor_write(driver_data->i2c_gsensor_client, BMA150_REG_RESET_INT, &tx_data, sizeof(tx_data));
	if (ret < 0)
	{
		GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_write fail!!-ret:%d\n", __func__, ret);
		return ret;
	}
	else ret = 0;

	ret = i2c_gsensor_read(driver_data->i2c_gsensor_client, BMA150_REG_CTRL_14, &rx_data, sizeof(rx_data));
	if (ret < 0)
	{
		GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_read fail!!-ret:%d\n", __func__, ret);
		return ret;
	}
	else ret = 0;

	rx_data &= ~(BMA150_BANDWIDTH_MASK | BMA150_RANGE_MASK);
	rx_data |= ((BMA150_RANGE_2G << BMA150_RANGE_SHIFT) | BMA150_BANDWIDTH_25HZ);
	tx_data = rx_data;
	ret = i2c_gsensor_write(driver_data->i2c_gsensor_client, BMA150_REG_CTRL_14, &tx_data, sizeof(tx_data));
	if (ret < 0)
	{
		GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_write fail!!-ret:%d\n", __func__, ret);
		return ret;
	}
	else ret = 0;

	do
	{
		ret = i2c_gsensor_read(driver_data->i2c_gsensor_client, BMA150_REG_EE_W, &rx_data, sizeof(rx_data));
		if (ret < 0)
		{
			GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_read fail!!-ret:%d\n", __func__, ret);
			return ret;
		}
		else ret = 0;

		if (rx_data & BMA150_EE_W_MASK)
		{
			break;
		}

		rx_data |= BMA150_EE_W_MASK;
		tx_data = rx_data;
		ret = i2c_gsensor_write(driver_data->i2c_gsensor_client, BMA150_REG_EE_W, &tx_data, sizeof(tx_data));
		if (ret < 0)
		{
			GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_write fail!!-ret:%d\n", __func__, ret);
			return ret;
		}
		else ret = 0;

		msleep(20);
	}while(1);

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s-ret:%d\n", __func__, ret);
	return ret;
}

static void gsensor_get_offset(struct gsensor_driver_data_t *driver_data, unsigned short *offset)
{
	int ret = 0;
	uint8_t rx_buf[7];

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s+\n", __func__);

	memset(rx_buf, 0, sizeof(rx_buf));
	ret = i2c_gsensor_read(driver_data->i2c_gsensor_client, BMA150_REG_OFFSET_X_LSB, rx_buf, sizeof(rx_buf));
	if (ret < 0)
	{
		GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_read fail!!-ret:%d\n", __func__, ret);
		return;
	}
	offset[0] = ((rx_buf[0] & 0xC0) >> 6) | (rx_buf[4] << 2);
	offset[1] = ((rx_buf[1] & 0xC0) >> 6) | (rx_buf[5] << 2);
	offset[2] = ((rx_buf[2] & 0xC0) >> 6) | (rx_buf[6] << 2);

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s-offset[0]:0x%x, offset[1]:0x%x, offset[2]:0x%x\n", __func__, offset[0], offset[1], offset[2]);
	return;
}

static void gsensor_set_offset(struct gsensor_driver_data_t *driver_data, unsigned short *offset)
{
	int ret = 0;
	uint8_t rx_buf[3];
	uint8_t tx_data;
	int i;

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s+offset_x:0x%x, offset_y:0x%x, offset_z:0x%x\n", __func__, offset[0], offset[1], offset[2]);

	memset(rx_buf, 0, sizeof(rx_buf));
	ret = i2c_gsensor_read(driver_data->i2c_gsensor_client, BMA150_REG_OFFSET_X_LSB, rx_buf, sizeof(rx_buf));
	if (ret < 0)
	{
		GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_read fail!!-ret:%d\n", __func__, ret);
		return;
	}

	for (i = 0; i < 3; i++)
	{
		tx_data = (rx_buf[i] & ~BMA150_OFFSET_X_LSB_MASK) | ((offset[i] & 0x3) << BMA150_ACC_X_LSB_SHIFT);

		ret = i2c_gsensor_write(driver_data->i2c_gsensor_client, BMA150_REG_OFFSET_X_LSB + i, &tx_data, sizeof(tx_data));
		if (ret < 0)
		{
			GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_write fail!!-ret:%d\n", __func__, ret);
			return;
		}
		msleep(20);
	}

	for (i = 0; i < 3; i++)
	{
		tx_data = (offset[i] >> 2) & 0xFF;

		ret = i2c_gsensor_write(driver_data->i2c_gsensor_client, BMA150_REG_OFFSET_X_MSB + i, &tx_data, sizeof(tx_data));
		if (ret < 0)
		{
			GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_write fail!!-ret:%d\n", __func__, ret);
			return;
		}
		msleep(20);
	}

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s-\n", __func__);
	return;
}

static void gsensor_set_offset_eeprom(struct gsensor_driver_data_t *driver_data, unsigned short *offset)
{
	int ret = 0;
	uint8_t rx_buf[3];
	uint8_t tx_data;
	int i;

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s+offset_x:0x%x, offset_y:0x%x, offset_z:0x%x\n", __func__, offset[0], offset[1], offset[2]);

	memset(rx_buf, 0, sizeof(rx_buf));
	ret = i2c_gsensor_read(driver_data->i2c_gsensor_client, BMA150_REG_OFFSET_X_LSB, rx_buf, sizeof(rx_buf));
	if (ret < 0)
	{
		GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_read fail!!-ret:%d\n", __func__, ret);
		return;
	}

	for (i = 0; i < 3; i++)
	{
		tx_data = (rx_buf[i] & ~BMA150_OFFSET_X_LSB_MASK) | ((offset[i] & 0x3) << BMA150_ACC_X_LSB_SHIFT);

		ret = i2c_gsensor_write(driver_data->i2c_gsensor_client, BMA150_REG_EEP_OFFSET_X_LSB + i, &tx_data, sizeof(tx_data));
		if (ret < 0)
		{
			GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_write fail!!-ret:%d\n", __func__, ret);
			return;
		}
		msleep(20);
	}

	for (i = 0; i < 3; i++)
	{
		tx_data = (offset[i] >> 2) & 0xFF;

		ret = i2c_gsensor_write(driver_data->i2c_gsensor_client, BMA150_REG_EEP_OFFSET_X_MSB + i, &tx_data, sizeof(tx_data));
		if (ret < 0)
		{
			GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_write fail!!-ret:%d\n", __func__, ret);
			return;
		}
		msleep(20);
	}

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s-\n", __func__);
	return;
}

static int gsensor_calc_new_offset(struct orientation_accel_t orientation, struct orientation_accel_t accel, short offset_lsb,
                                             unsigned short *offset_x, unsigned short *offset_y, unsigned short *offset_z)
{
	short old_offset_x, old_offset_y, old_offset_z;
	short new_offset_x, new_offset_y, new_offset_z;
	unsigned char  calibrated =0;

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s+offset_x:%d, offset_y:%d, offset_z:%d\n", __func__, *offset_x, *offset_y, *offset_z);

	old_offset_x = *offset_x;
	old_offset_y = *offset_y;
	old_offset_z = *offset_z;

	accel.x = accel.x - (orientation.x * 256);
	accel.y = accel.y - (orientation.y * 256);
	accel.z = accel.z - (orientation.z * 256);

	if ((accel.x > (offset_lsb >> 1)) | (accel.x < -(offset_lsb >> 1))) {

		if ((accel.x < offset_lsb) && accel.x > 0)
			new_offset_x= old_offset_x -1;
		else if ((accel.x > -offset_lsb) && (accel.x < 0))
			new_offset_x= old_offset_x +1;
		else
			new_offset_x = old_offset_x - (accel.x/offset_lsb);
		if (new_offset_x <0)
			new_offset_x =0;
		else if (new_offset_x>1023)
			new_offset_x=1023;
		*offset_x = new_offset_x;
		calibrated = 1;
	}

	if ((accel.y > (offset_lsb >> 1)) | (accel.y < -(offset_lsb >> 1))) {
		if ((accel.y < offset_lsb) && accel.y > 0)
			new_offset_y= old_offset_y -1;
		else if ((accel.y > -offset_lsb) && accel.y < 0)
			new_offset_y= old_offset_y +1;
		else
			new_offset_y = old_offset_y - accel.y/offset_lsb;

		if (new_offset_y <0)
			new_offset_y =0;
		else if (new_offset_y>1023)
			new_offset_y=1023;

		*offset_y = new_offset_y;
		calibrated = 1;
	}

	if ((accel.z > (offset_lsb >> 1)) | (accel.z < -(offset_lsb >> 1))) {
		if ((accel.z < offset_lsb) && accel.z > 0)
			new_offset_z= old_offset_z -1;
		else if ((accel.z > -offset_lsb) && accel.z < 0)
			new_offset_z= old_offset_z +1;
		else
			new_offset_z = old_offset_z - (accel.z/offset_lsb);

		if (new_offset_z <0)
			new_offset_z =0;
		else if (new_offset_z>1023)
			new_offset_z=1023;

		*offset_z = new_offset_z;
		calibrated = 1;
	}

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s-ret:%d, offset_x:%d, offset_y:%d, offset_z:%d\n", __func__, calibrated, *offset_x, *offset_y, *offset_z);
	return calibrated;
}

static int gsensor_read_accel_xyz(struct gsensor_driver_data_t *driver_data, struct orientation_accel_t *accel)
{
	int ret = 0;
	uint8_t rx_buf[6];

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s+\n", __func__);

	ret = i2c_gsensor_read(driver_data->i2c_gsensor_client, BMA150_REG_ACC_X_LSB, rx_buf, sizeof(rx_buf));
	if (ret < 0)
	{
		GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_read fail!!-ret:%d\n", __func__, ret);
		return 0;
	}
	accel->x = ((rx_buf[0] & 0xC0) >> 6) | (rx_buf[1] << 2);
	accel->y = ((rx_buf[2] & 0xC0) >> 6) | (rx_buf[3] << 2);
	accel->z = ((rx_buf[4] & 0xC0) >> 6) | (rx_buf[5] << 2);

	if (accel->x & 0x200)
		accel->x = accel->x - 0x400;
	if (accel->y & 0x200)
		accel->y = accel->y - 0x400;
	if (accel->z & 0x200)
		accel->z = accel->z - 0x400;

	GSENSOR_LOGI(KERN_INFO "[GSENSOR] %s::x:%d, y:%d, z:%d\n", __func__, accel->x, accel->y, accel->z);
	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s-\n", __func__);
	return 1;
}

static int gsensor_read_accel_avg(struct gsensor_driver_data_t *driver_data, int num_avg, struct orientation_accel_t *min, struct orientation_accel_t *max, struct orientation_accel_t *avg )
{
	long x_avg=0, y_avg=0, z_avg=0;
	int comres=0;
	int i;
	struct orientation_accel_t accel;

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s+\n", __func__);

	x_avg = 0; y_avg=0; z_avg=0;
	max->x = -512; max->y =-512; max->z = -512;
	min->x = 512;  min->y = 512; min->z = 512;

	memset(&accel, 0, sizeof(struct orientation_accel_t));
	for (i=0; i<num_avg; i++) {
		comres += gsensor_read_accel_xyz(driver_data, &accel);
		if (accel.x>max->x)
			max->x = accel.x;
		if (accel.x<min->x)
			min->x=accel.x;

		if (accel.y>max->y)
			max->y = accel.y;
		if (accel.y<min->y)
			min->y=accel.y;

		if (accel.z>max->z)
			max->z = accel.z;
		if (accel.z<min->z)
			min->z=accel.z;

		x_avg+= accel.x;
		y_avg+= accel.y;
		z_avg+= accel.z;

		msleep(10);
	}
	avg->x = x_avg /= num_avg;
	avg->y = y_avg /= num_avg;
	avg->z = z_avg /= num_avg;

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s-ret:%d\n", __func__, comres);
	return comres;
}

static int gsensor_verify_min_max(struct orientation_accel_t min, struct orientation_accel_t max, struct orientation_accel_t avg)
{
	short dx, dy, dz;
	int ver_ok=1;

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s+\n", __func__);

	dx =  max.x - min.x;
	dy =  max.y - min.y;
	dz =  max.z - min.z;

	if (dx> 10 || dx<-10)
		ver_ok = 0;
	if (dy> 10 || dy<-10)
		ver_ok = 0;
	if (dz> 10 || dz<-10)
		ver_ok = 0;

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s-ret:%d\n", __func__, ver_ok);
	return ver_ok;
}

static int gsensor_calibrate(struct gsensor_driver_data_t *driver_data, struct orientation_accel_t orientation, int *tries)
{
	short offset_lsb;
	unsigned short offset[3];
	unsigned short old_offset_x, old_offset_y, old_offset_z;
	int need_calibration=0, min_max_ok=0;
	int ltries, verify_fail_times = 0;
	int ret;
	struct orientation_accel_t min,max,avg;

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s+\n", __func__);

	gsensor_calibrate_config(driver_data);

	gsensor_get_offset(driver_data, offset);

	old_offset_x = offset[0];
	old_offset_y = offset[1];
	old_offset_z = offset[2];
	ltries = *tries;

	ret = down_interruptible(&sem_data);
	if (ret != 0)
	{
		GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s:: The sleep is interrupted by a signal\n", __func__);
		return 0;
	}
	offset_lsb = driver_data->offset_lsb;
	up(&sem_data);

	do {
		gsensor_read_accel_avg(driver_data, 10, &min, &max, &avg);

		min_max_ok = gsensor_verify_min_max(min, max, avg);

		GSENSOR_LOGI(KERN_INFO "[GSENSOR] %s::min-->x:%d, y:%d, z:%d\n", __func__, min.x, min.y, min.z);
		GSENSOR_LOGI(KERN_INFO "[GSENSOR] %s::avg-->x:%d, y:%d, z:%d\n", __func__, avg.x, avg.y, avg.z);
		GSENSOR_LOGI(KERN_INFO "[GSENSOR] %s::max-->x:%d, y:%d, z:%d\n", __func__, max.x, max.y, max.z);

		if (min_max_ok)
			need_calibration = gsensor_calc_new_offset(orientation, avg, offset_lsb, &offset[0], &offset[1], &offset[2]);
		else
		{
			verify_fail_times++;
			if (verify_fail_times >= BMA150_CALIBRATION_MAX_VERIFY_FAIL_TIMES)
			{
				need_calibration = 1;
				break;
			}
		}

		if (*tries==0)
			break;

		if (need_calibration) {
			(*tries)--;
			gsensor_set_offset(driver_data, offset);
			msleep(20);
		}
	} while (need_calibration || !min_max_ok);

	*tries = ltries - *tries;

	if (*tries == ltries || verify_fail_times >= BMA150_CALIBRATION_MAX_VERIFY_FAIL_TIMES)
	{
		driver_data->calibrate_data.offset_x = 0;
		driver_data->calibrate_data.offset_y = 0;
		driver_data->calibrate_data.offset_z = 0;
		GSENSOR_LOGW(KERN_WARNING "[GSENSOR] %s fail::tries=%d, verify_fail_times=%d\n", __func__, *tries, verify_fail_times);
	}
	else
	{
		driver_data->calibrate_data.offset_x = offset[0];
		driver_data->calibrate_data.offset_y = offset[1];
		driver_data->calibrate_data.offset_z = offset[2];
		GSENSOR_LOGI(KERN_INFO "[GSENSOR] %s success\n", __func__);
	}

	gsensor_default_config(driver_data);

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s-ret:%d\n", __func__, !need_calibration);
	return !need_calibration;
}

static int gsensor_engineer_mode_calibrate(struct gsensor_driver_data_t *driver_data, int calibrate)
{
	int ret = 0;
	int times = BMA150_CALIBRATION_MAX_TRIES;
	struct orientation_accel_t orientation;
	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s+\n", __func__);

	if (calibrate >=6 || calibrate < 0)
	{
		GSENSOR_LOGW(KERN_WARNING "[GSENSOR] %s calibrate value invalid-calibrate:%d\n", __func__, calibrate);
		return -EFAULT;
	}

	orientation.x = calibrate_table[calibrate][1];
	orientation.y = calibrate_table[calibrate][2];
	orientation.z = calibrate_table[calibrate][3];

	if (!gsensor_calibrate(driver_data, orientation, &times))
	{
		GSENSOR_LOGW(KERN_WARNING "[GSENSOR] %s::gsensor_calibrate first fail\n", __func__);
		return -EFAULT;
	}

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s-\n", __func__);
	return ret;
}

static int gsensor_engineer_mode_set_parameter(struct gsensor_driver_data_t *driver_data, struct gsensor_ioctl_setting_t *ioctl_setting)
{
	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s+\n", __func__);

	gsensor_set_ctrl_0b(driver_data, ioctl_setting);
	gsensor_set_settings_0c(driver_data, ioctl_setting);
	gsensor_set_settings_0d(driver_data, ioctl_setting);
	gsensor_set_settings_0e(driver_data, ioctl_setting);
	gsensor_set_settings_0f(driver_data, ioctl_setting);
	gsensor_set_settings_10(driver_data, ioctl_setting);
	gsensor_set_settings_11(driver_data, ioctl_setting);
	gsensor_set_ctrl_14(driver_data, ioctl_setting);
	gsensor_set_ctrl_15(driver_data, ioctl_setting);

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s-\n", __func__);
	return 0;
}

static int gsensor_engineer_mode_get_accel(struct gsensor_driver_data_t *driver_data, struct gsensor_ioctl_accel_t *ioctl_accel)
{
	int ret = 0;
	uint8_t rx_buf[6];

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s+\n", __func__);

	ioctl_accel->interrupt = gpio_get_value(23);

	ret = i2c_gsensor_read(driver_data->i2c_gsensor_client, BMA150_REG_ACC_X_LSB, rx_buf, sizeof(rx_buf));
	if (ret < 0)
	{
		GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_read fail!!-ret:%d\n", __func__, ret);
		return ret;
	}
	ioctl_accel->accel_x = ((rx_buf[0] & 0xC0) >> 6) | (rx_buf[1] << 2);
	ioctl_accel->accel_y = ((rx_buf[2] & 0xC0) >> 6) | (rx_buf[3] << 2);
	ioctl_accel->accel_z = ((rx_buf[4] & 0xC0) >> 6) | (rx_buf[5] << 2);

	if (ioctl_accel->accel_x & 0x200)
		ioctl_accel->accel_x = ioctl_accel->accel_x - 0x400;
	if (ioctl_accel->accel_y & 0x200)
		ioctl_accel->accel_y = ioctl_accel->accel_y - 0x400;
	if (ioctl_accel->accel_z & 0x200)
		ioctl_accel->accel_z = ioctl_accel->accel_z - 0x400;

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s-\n", __func__);
	return 1;
}

static void convert_regdata_to_accel_data(u8 *buf, struct gsensor_accel_data_t *a)
{
	a->accel_x = ((buf[0] & 0xC0) >> 6) | (buf[1] << 2);
	a->accel_y = ((buf[2] & 0xC0) >> 6) | (buf[3] << 2);
	a->accel_z = ((buf[4] & 0xC0) >> 6) | (buf[5] << 2);
	if (a->accel_x & 0x200)
		a->accel_x = a->accel_x - 0x400;
	if (a->accel_y & 0x200)
		a->accel_y = a->accel_y - 0x400;
	if (a->accel_z & 0x200)
		a->accel_z = a->accel_z - 0x400;
	a->temp = buf[6];
}

static void gsensor_work_func(struct work_struct *work)
{
	uint8_t rx_buf[7];
	uint8_t tx_data;
	int ret = 0;
	struct gsensor_driver_data_t         *driver_data =	container_of(work, struct gsensor_driver_data_t, work_data);
	struct gsensor_accel_data_t    acc_data;

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s+\n", __func__);

	memset(rx_buf, 0, sizeof(rx_buf));
	ret = i2c_gsensor_read(driver_data->i2c_gsensor_client, BMA150_REG_ACC_X_LSB, rx_buf, sizeof(rx_buf));
	if (ret < 0)
	{
		GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_read fail!!-ret:%d\n", __func__, ret);
		return;
	}

	convert_regdata_to_accel_data(rx_buf, &acc_data);

	ret = down_interruptible(&sem_data);
	if (ret != 0)
	{
		GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s:: The sleep is interrupted by a signal\n", __func__);
		return;
	}
	driver_data->curr_acc_data.accel_x = acc_data.accel_x;
	driver_data->curr_acc_data.accel_y = acc_data.accel_y;
	driver_data->curr_acc_data.accel_z = acc_data.accel_z;
	driver_data->curr_acc_data.temp = acc_data.temp;
	up(&sem_data);

	GSENSOR_LOGI(KERN_INFO "[GSENSOR] x:%d, y:%d, z:%d, temp:%d\n", acc_data.accel_x, acc_data.accel_y, acc_data.accel_z, acc_data.temp);

	tx_data = BMA150_RESET_INT_MASK;
	ret = i2c_gsensor_write(driver_data->i2c_gsensor_client, BMA150_REG_RESET_INT, &tx_data, sizeof(tx_data));
	if (ret < 0)
	{
		GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_write fail!!-ret:%d\n", __func__, ret);
		return;
	}

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s-\n", __func__);
}

static irqreturn_t gsensor_irq_handler(int irq, void *dev_id)
{
	struct gsensor_driver_data_t *driver_data = dev_get_drvdata((struct device *) dev_id);

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s+\n", __func__);

	schedule_work(&driver_data->work_data);

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s-\n", __func__);
	return IRQ_HANDLED;
}

static int misc_gsensor_open(struct inode *inode_p, struct file *fp)
{
	int ret = 0;
	uint8_t tx_data;
	struct gsensor_driver_data_t *driver_data = &gsensor_driver_data;

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s+\n", __func__);

	ret = down_interruptible(&sem_open);
	if (ret != 0)
	{
		GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s:: The sleep is interrupted by a signal\n", __func__);
		return ret;
	}

	if (0 == atomic_read(&driver_data->open_count))
	{
		if (!driver_data->i2c_gsensor_client->irq)
		{
			GSENSOR_LOGE(KERN_ERR "[GSENSOR] misc_gsensor_open::IRQ number is NULL\n");
			up(&sem_open);
			return -1;
		}
		ret = request_irq(driver_data->i2c_gsensor_client->irq,
				 gsensor_irq_handler,
				 IRQF_TRIGGER_RISING,
				 GSENSOR_NAME,
				 &driver_data->i2c_gsensor_client->dev);
		if (ret < 0)
		{
			GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::request_irq fail!!\n", __func__);
			up(&sem_open);
			return ret;
		}

		GSENSOR_LOGI(KERN_INFO "[GSENSOR] %s::driver_data->i2c_gsensor_client->irq:%d\n", __func__, driver_data->i2c_gsensor_client->irq);

		tx_data = BMA150_RESET_INT_MASK;
		ret = i2c_gsensor_write(driver_data->i2c_gsensor_client, BMA150_REG_RESET_INT, &tx_data, sizeof(tx_data));
		if (ret < 0)
		{
			GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::i2c_gsensor_write fail!!-ret:%d\n", __func__, ret);
			up(&sem_open);
			return ret;
		}
	}
	atomic_inc(&driver_data->open_count);
	up(&sem_open);

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s-\n", __func__);
	return 0;
}

static int misc_gsensor_release(struct inode *inode_p, struct file *fp)
{
	int ret = 0;
	struct gsensor_driver_data_t *driver_data = &gsensor_driver_data;

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s+\n", __func__);

	if (down_interruptible(&sem_open) != 0)
	{
		GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s:: The sleep is interrupted by a signal\n", __func__);
		return -EFAULT;
	}

	if (1 == atomic_read(&driver_data->open_count))
	{
		free_irq(driver_data->i2c_gsensor_client->irq, &driver_data->i2c_gsensor_client->dev);
	}
	atomic_dec(&driver_data->open_count);
	if (0 > atomic_read(&driver_data->open_count))
	{
		GSENSOR_LOGW(KERN_WARNING "[GSENSOR] %s:: open_count=%d\n", __func__, atomic_read(&driver_data->open_count));
		atomic_set(&driver_data->open_count, 0);
	}
	up(&sem_open);

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s-\n", __func__);
	return ret;
}

static long misc_gsensor_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	int calibrate;
	struct gsensor_ioctl_t local_gsensor_ioctl;
	struct gsensor_ioctl_accel_t local_gsensor_ioctl_accel;
	struct gsensor_ioctl_calibrate_t calibrate_data;
	unsigned short offset[3];

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s+\n", __func__);

	if (_IOC_TYPE(cmd) != GSENSOR_IOC_MAGIC)
	{
		GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::Not GSENSOR_IOC_MAGIC\n", __func__);
		return -ENOTTY;
	}

	if (_IOC_DIR(cmd) & _IOC_READ)
	{
		ret = !access_ok(VERIFY_WRITE, (void __user*)arg, _IOC_SIZE(cmd));
		if (ret)
		{
			GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::access_ok check err\n", __func__);
			return -EFAULT;
		}
	}

	if (_IOC_DIR(cmd) & _IOC_WRITE)
	{
		ret = !access_ok(VERIFY_READ, (void __user*)arg, _IOC_SIZE(cmd));
		if (ret)
		{
			GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::access_ok check err\n", __func__);
			return -EFAULT;
		}
	}

	switch (cmd)
	{
		case GSENSOR_IOC_READ:
			GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] GSENSOR_IOC_READ\n");
			memset(&local_gsensor_ioctl, 0, sizeof(struct gsensor_ioctl_t));
			if (down_interruptible(&sem_data) != 0)
			{
				GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s:: The sleep is interrupted by a signal\n", __func__);
				ret = -EFAULT;
				break;
			}
			local_gsensor_ioctl.accel_x = gsensor_driver_data.curr_acc_data.accel_x;
			local_gsensor_ioctl.accel_y = gsensor_driver_data.curr_acc_data.accel_y;
			local_gsensor_ioctl.accel_z = gsensor_driver_data.curr_acc_data.accel_z;
			up(&sem_data);
			if (copy_to_user((void __user*) arg, &local_gsensor_ioctl, _IOC_SIZE(cmd)))
			{
				GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::GSENSOR_IOC_READ:copy_to_user fail-\n", __func__);
				ret = -EFAULT;
			}
			break;

		case GSENSOR_IOC_READ_DIRECT:
			GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] GSENSOR_IOC_READ_DIRECT\n");
			memset(&local_gsensor_ioctl, 0, sizeof(struct gsensor_ioctl_t));
			memset(&local_gsensor_ioctl_accel, 0, sizeof(struct gsensor_ioctl_accel_t));
			if (gsensor_engineer_mode_get_accel(&gsensor_driver_data, &local_gsensor_ioctl_accel) < 0)
			{
				GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::GSENSOR_IOC_READ_DIRECT:gsensor_engineer_mode_get_accel fail-\n", __func__);
				ret = -EFAULT;
				break;
			}
			local_gsensor_ioctl.accel_x = local_gsensor_ioctl_accel.accel_x;
			local_gsensor_ioctl.accel_y = local_gsensor_ioctl_accel.accel_y;
			local_gsensor_ioctl.accel_z = local_gsensor_ioctl_accel.accel_z;

			ret = down_interruptible(&sem_data);
			if (ret != 0)
			{
				GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s:: The sleep is interrupted by a signal\n", __func__);
				break;
			}
			gsensor_driver_data.curr_acc_data.accel_x = local_gsensor_ioctl_accel.accel_x;
			gsensor_driver_data.curr_acc_data.accel_y = local_gsensor_ioctl_accel.accel_y;
			gsensor_driver_data.curr_acc_data.accel_z = local_gsensor_ioctl_accel.accel_z;
			up(&sem_data);

			if (copy_to_user((void __user*) arg, &local_gsensor_ioctl, _IOC_SIZE(cmd)))
			{
				GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::GSENSOR_IOC_READ_DIRECT:copy_to_user fail-\n", __func__);
				ret = -EFAULT;
			}
			break;

		case GSENSOR_IOC_EM_SET_PARAMETER:
			GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] GSENSOR_IOC_EM_SET_PARAMETER\n");
			if (copy_from_user(&gsensor_ioctl_setting, (void __user*) arg, _IOC_SIZE(cmd)))
			{
				GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::GSENSOR_IOC_EM_SET_PARAMETER:copy_from_user fail-\n", __func__);
				ret = -EFAULT;
				break;
			}

			GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] enable_LG:%d\n", gsensor_ioctl_setting.enable_LG);
			GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] enable_HG:%d\n", gsensor_ioctl_setting.enable_HG);
			GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] enable_advanced:%d\n", gsensor_ioctl_setting.enable_advanced);
			GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] enable_alert:%d\n", gsensor_ioctl_setting.enable_alert);
			GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] enable_any_montion:%d\n", gsensor_ioctl_setting.enable_any_montion);
			GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] range:%d\n", gsensor_ioctl_setting.range);
			GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] bandwidth:%d\n", gsensor_ioctl_setting.bandwidth);
			GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] threshold_LG:%d\n", gsensor_ioctl_setting.threshold_LG);
			GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] duration_LG:%d\n", gsensor_ioctl_setting.duration_LG);
			GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] hysteresis_LG:%d\n", gsensor_ioctl_setting.hysteresis_LG);
			GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] counter_LG:%d\n", gsensor_ioctl_setting.counter_LG);
			GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] threshold_HG:%d\n", gsensor_ioctl_setting.threshold_HG);
			GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] duration_HG:%d\n", gsensor_ioctl_setting.duration_HG);
			GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] hysteresis_HG:%d\n", gsensor_ioctl_setting.hysteresis_HG);
			GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] counter_HG:%d\n", gsensor_ioctl_setting.counter_HG);
			GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] threshold_any_montion:%d\n", gsensor_ioctl_setting.threshold_any_montion);
			GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] duration_any_montion:%d\n", gsensor_ioctl_setting.duration_any_montion);

			if (gsensor_engineer_mode_set_parameter(&gsensor_driver_data, &gsensor_ioctl_setting) < 0)
			{
				GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::GSENSOR_IOC_EM_SET_PARAMETER:gsensor_engineer_mode_set_parameter fail-\n", __func__);
				ret = -EFAULT;
			}
			break;

		case GSENSOR_IOC_EM_GET_ACCEL:
			GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] GSENSOR_IOC_EM_GET_ACCEL\n");
			memset(&local_gsensor_ioctl_accel, 0, sizeof(struct gsensor_ioctl_accel_t));
			if (gsensor_engineer_mode_get_accel(&gsensor_driver_data, &local_gsensor_ioctl_accel) < 0)
			{
				GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::GSENSOR_IOC_EM_GET_ACCEL:gsensor_engineer_mode_get_accel fail-\n", __func__);
				ret = -EFAULT;
				break;
			}

			ret = down_interruptible(&sem_data);
			if (ret != 0)
			{
				GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s:: The sleep is interrupted by a signal\n", __func__);
				break;
			}
			gsensor_driver_data.curr_acc_data.accel_x = local_gsensor_ioctl_accel.accel_x;
			gsensor_driver_data.curr_acc_data.accel_y = local_gsensor_ioctl_accel.accel_y;
			gsensor_driver_data.curr_acc_data.accel_z = local_gsensor_ioctl_accel.accel_z;
			up(&sem_data);

			if (copy_to_user((void __user*) arg, &local_gsensor_ioctl_accel, _IOC_SIZE(cmd)))
			{
				GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::GSENSOR_IOC_EM_GET_ACCEL:copy_to_user fail-\n", __func__);
				ret = -EFAULT;
			}
			break;

		case GSENSOR_IOC_EM_CALIBRATE:
			GSENSOR_LOGW(KERN_DEBUG "[GSENSOR] GSENSOR_IOC_EM_CALIBRATE\n");
			if(copy_from_user(&calibrate, (void __user*) arg, _IOC_SIZE(cmd)))
			{
				GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::GSENSOR_IOC_EM_CALIBRATE:copy_from_user fail-\n", __func__);
				ret = -EFAULT;
				break;
			}

			GSENSOR_LOGI(KERN_INFO "[GSENSOR] %s::calibrate:%d\n", __func__, calibrate);

			if (gsensor_engineer_mode_calibrate(&gsensor_driver_data, calibrate) < 0)
			{
				GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::GSENSOR_IOC_EM_CALIBRATE:gsensor_engineer_mode_calibrate fail-\n", __func__);
				ret = -EFAULT;
			}
			break;

		case GSENSOR_IOC_SET_CALIB_PARM:
			GSENSOR_LOGW(KERN_DEBUG "[GSENSOR] GSENSOR_IOC_SET_CALIB_PARM\n");
			if (copy_from_user(&calibrate_data, (void __user*) arg, _IOC_SIZE(cmd)))
			{
				GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::GSENSOR_IOC_SET_CALIB_PARM:copy_from_user fail-\n", __func__);
				ret = -EFAULT;
				break;
			}
			GSENSOR_LOGW(KERN_DEBUG "[GSENSOR] %s::GSENSOR_IOC_SET_CALIB_PARM: offset@ x=%d, y=%d, z=%d\n", __func__, calibrate_data.offset_x, calibrate_data.offset_y, calibrate_data.offset_z);

			if (calibrate_data.offset_x == 0 && calibrate_data.offset_y == 0 && calibrate_data.offset_z == 0)
			{
				GSENSOR_LOGE(KERN_DEBUG "[GSENSOR] %s::GSENSOR_IOC_SET_CALIB_PARM: offset_x,y,z=0 err-\n", __func__);
				ret = -EFAULT;
				break;
			}

			if (calibrate_data.offset_x > 1023 || calibrate_data.offset_y > 1023 || calibrate_data.offset_z > 1023)
			{
				GSENSOR_LOGE(KERN_DEBUG "[GSENSOR] %s::GSENSOR_IOC_SET_CALIB_PARM: offset_x,y,z > 1023 err-\n", __func__);
				ret = -EFAULT;
				break;
			}

			gsensor_driver_data.calibrate_data.offset_x = calibrate_data.offset_x;
			gsensor_driver_data.calibrate_data.offset_y = calibrate_data.offset_y;
			gsensor_driver_data.calibrate_data.offset_z = calibrate_data.offset_z;
			offset[0] = calibrate_data.offset_x;
			offset[1] = calibrate_data.offset_y;
			offset[2] = calibrate_data.offset_z;
			gsensor_calibrate_config(&gsensor_driver_data);
			gsensor_set_offset(&gsensor_driver_data, offset);
			gsensor_default_config(&gsensor_driver_data);
			break;

		case GSENSOR_IOC_SET_CALIB_PARM_EEPROM:
			GSENSOR_LOGW(KERN_DEBUG "[GSENSOR] GSENSOR_IOC_SET_CALIB_PARM_EEPROM\n");
			if (copy_from_user(&calibrate_data, (void __user*) arg, _IOC_SIZE(cmd)))
			{
				GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::GSENSOR_IOC_SET_CALIB_PARM_EEPROM:copy_from_user fail-\n", __func__);
				ret = -EFAULT;
				break;
			}
			GSENSOR_LOGW(KERN_DEBUG "[GSENSOR] %s::GSENSOR_IOC_SET_CALIB_PARM_EEPROM: offset@ x=%d, y=%d, z=%d\n", __func__, calibrate_data.offset_x, calibrate_data.offset_y, calibrate_data.offset_z);

			if (calibrate_data.offset_x == 0 && calibrate_data.offset_y == 0 && calibrate_data.offset_z == 0)
			{
				GSENSOR_LOGE(KERN_DEBUG "[GSENSOR] %s::GSENSOR_IOC_SET_CALIB_PARM_EEPROM: offset_x,y,z=0 err-\n", __func__);
				ret = -EFAULT;
				break;
			}

			if (calibrate_data.offset_x > 1023 || calibrate_data.offset_y > 1023 || calibrate_data.offset_z > 1023)
			{
				GSENSOR_LOGE(KERN_DEBUG "[GSENSOR] %s::GSENSOR_IOC_SET_CALIB_PARM_EEPROM: offset_x,y,z > 1023 err-\n", __func__);
				ret = -EFAULT;
				break;
			}

			gsensor_driver_data.calibrate_data.offset_x = calibrate_data.offset_x;
			gsensor_driver_data.calibrate_data.offset_y = calibrate_data.offset_y;
			gsensor_driver_data.calibrate_data.offset_z = calibrate_data.offset_z;
			offset[0] = calibrate_data.offset_x;
			offset[1] = calibrate_data.offset_y;
			offset[2] = calibrate_data.offset_z;
			gsensor_calibrate_config(&gsensor_driver_data);
			gsensor_set_eeprom_write(&gsensor_driver_data, 1);
			gsensor_set_offset_eeprom(&gsensor_driver_data, offset);
			gsensor_set_eeprom_write(&gsensor_driver_data, 0);
			gsensor_default_config(&gsensor_driver_data);
			break;

		case GSENSOR_IOC_GET_CALIB_PARM:
			GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] GSENSOR_IOC_GET_CALIB_PARM\n");
			calibrate_data.offset_x = gsensor_driver_data.calibrate_data.offset_x;
			calibrate_data.offset_y = gsensor_driver_data.calibrate_data.offset_y;
			calibrate_data.offset_z = gsensor_driver_data.calibrate_data.offset_z;
			if (copy_to_user((void __user*) arg, &calibrate_data, _IOC_SIZE(cmd)))
			{
				GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::GSENSOR_IOC_GET_CALIB_PARM:copy_to_user fail-\n", __func__);
				ret = -EFAULT;
			}
			break;

		case GSENSOR_IOC_EM_READ_ID:
			GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] GSENSOR_IOC_EM_READ_ID\n");
			if (copy_to_user((void __user*) arg, &gsensor_driver_data.chip_id, _IOC_SIZE(cmd)))
			{
				GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::GSENSOR_IOC_EM_READ_ID:copy_to_user fail-\n", __func__);
				ret = -EFAULT;
			}
			break;

		default:
			GSENSOR_LOGW(KERN_WARNING "[GSENSOR] %s::deafult\n", __func__);
			break;
	}

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s-\n", __func__);
	return ret;
}

static ssize_t misc_gsensor_read(struct file *fp, char __user *buf, size_t count, loff_t *f_pos)
{
	int ret = 0;
	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s+\n", __func__);
	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s-\n", __func__);
	return ret;
}

static ssize_t misc_gsensor_write(struct file *fp, const char __user *buf, size_t count, loff_t *f_pos)
{
	int ret = 0;
	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s+\n", __func__);
	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s-\n", __func__);
	return ret;
}

static const struct i2c_device_id i2c_gsensor_id[] = {
	{ GSENSOR_NAME, 0 },
	{}
};

static struct i2c_driver i2c_gsensor_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = GSENSOR_NAME,
	},
	.probe	 = i2c_gsensor_probe,
	.remove	 = i2c_gsensor_remove,
	.suspend = i2c_gsensor_suspend,
	.resume  = i2c_gsensor_resume,
	.command = i2c_gsensor_command,
	.id_table = i2c_gsensor_id,
};

static struct file_operations misc_gsensor_fops = {
	.owner 	= THIS_MODULE,
	.open 	= misc_gsensor_open,
	.release = misc_gsensor_release,
	.unlocked_ioctl = misc_gsensor_ioctl,
	.read = misc_gsensor_read,
	.write = misc_gsensor_write,
};

static struct miscdevice misc_gsensor_device = {
	.minor 	= MISC_DYNAMIC_MINOR,
	.name 	= GSENSOR_NAME,
	.fops 	= &misc_gsensor_fops,
};

static int i2c_gsensor_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	struct gsensor_driver_data_t *gsensor_driver_data_p = &gsensor_driver_data;

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s+id->name:%s\n", __func__, id->name);

	client->driver = &i2c_gsensor_driver;
	i2c_set_clientdata(client, &gsensor_driver_data);
	gsensor_driver_data_p->i2c_gsensor_client = client;

	memset(&gsensor_ioctl_setting, 0, sizeof(struct gsensor_ioctl_setting_t));

	ret = gsensor_default_config(&gsensor_driver_data);
	if (ret < 0)
	{
		GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::gsensor_config fail-ret:%d\n", __func__, ret);
		return ret;
	}

	ret = misc_register(&misc_gsensor_device);
	if (ret)
	{
		GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s:: misc_register error-ret:%d\n", __func__,ret );
		return ret;
	}

	INIT_WORK(&gsensor_driver_data_p->work_data, gsensor_work_func);

	#if	DBG_MONITOR
	INIT_WORK(&gsensor_driver_data_p->dbg_monitor_work_data, debug_monitor_work_func);
	init_timer(&gsensor_driver_data_p->dbg_monitor_timer);
	gsensor_driver_data_p->dbg_monitor_timer.data = (unsigned long) gsensor_driver_data_p;
	gsensor_driver_data_p->dbg_monitor_timer.function = debug_monitor;
	gsensor_driver_data_p->dbg_monitor_timer.expires = jiffies + HZ;
	add_timer(&gsensor_driver_data_p->dbg_monitor_timer);
	#endif

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s-ret:%d\n", __func__, ret);
	return ret;
}

static int i2c_gsensor_remove(struct i2c_client *client)
{
	int ret = 0;
	struct gsensor_driver_data_t *driver_data;

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s+\n", __func__);

	misc_deregister(&misc_gsensor_device);

	driver_data = i2c_get_clientdata(client);

	ret = gsensor_power_down(driver_data);
	if (ret < 0)
		GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::gsensor_power_down fail\n", __func__);

	i2c_set_clientdata(client, NULL);

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s-ret:%d\n", __func__, ret);
	return 0;
}

static int i2c_gsensor_suspend(struct i2c_client *client, pm_message_t state)
{
	int ret = 0;
	struct gsensor_driver_data_t *driver_data;

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s+\n", __func__);

	driver_data = i2c_get_clientdata(client);

	ret = gsensor_power_down(driver_data);
	if (ret < 0)
		GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::gsensor_power_down fail\n", __func__);

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s-ret:%d\n", __func__, ret);
	return ret;
}

static int i2c_gsensor_resume(struct i2c_client *client)
{
	int ret = 0;
	struct gsensor_driver_data_t *driver_data;

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s+\n", __func__);

	driver_data = i2c_get_clientdata(client);

	ret = gsensor_power_up(driver_data);
	if (ret < 0)
		GSENSOR_LOGE(KERN_ERR "[GSENSOR] %s::gsensor_power_up fail\n", __func__);

	mdelay(2);

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s-ret:%d\n", __func__, ret);
	return ret;
}

static int i2c_gsensor_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
	int ret = 0;

	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s+\n", __func__);
	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s-ret:%d\n", __func__, ret);
	return ret;
}

static int __init gsensor_init(void)
{
	int ret = 0;

	printk("BootLog, +%s+\n", __func__);

	memset(&gsensor_driver_data, 0, sizeof(struct gsensor_driver_data_t));
	ret = i2c_add_driver(&i2c_gsensor_driver);

	printk("BootLog, -%s-, ret=%d\n", __func__, ret);
	return ret;
}

static void __exit gsensor_exit(void)
{
	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s+\n", __func__);
	i2c_del_driver(&i2c_gsensor_driver);
	GSENSOR_LOGD(KERN_DEBUG "[GSENSOR] %s-\n", __func__);
}

module_init(gsensor_init);
module_exit(gsensor_exit);

MODULE_DESCRIPTION("BOSCH BMA 150 G-Sensor Driver");
MODULE_LICENSE("GPL");
