
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <mach/gpio.h>

#include <mach/ecompass.h>
#include <mach/pm_log.h>

#define DBG_MONITOR 0
#define ENABLE_ECOMPASS_INT 0

#define ECOMPASS_LOG_ENABLE 1
#define ECOMPASS_LOG_DBG 0
#define ECOMPASS_LOG_INFO 0
#define ECOMPASS_LOG_WARNING 1
#define ECOMPASS_LOG_ERR 1

#if ECOMPASS_LOG_ENABLE
	#if ECOMPASS_LOG_DBG
		#define ECOMPASS_LOGD printk
	#else
		#define ECOMPASS_LOGD(a...) {}
	#endif

	#if ECOMPASS_LOG_INFO
		#define ECOMPASS_LOGI printk
	#else
		#define ECOMPASS_LOGI(a...) {}
	#endif

	#if ECOMPASS_LOG_WARNING
		#define ECOMPASS_LOGW printk
#else
		#define ECOMPASS_LOGW(a...) {}
	#endif

	#if ECOMPASS_LOG_ERR
		#define ECOMPASS_LOGE printk
	#else
		#define ECOMPASS_LOGE(a...) {}
	#endif
#else
	#define ECOMPASS_LOGD(a...) {}
	#define ECOMPASS_LOGI(a...) {}
	#define ECOMPASS_LOGW(a...) {}
	#define ECOMPASS_LOGE(a...) {}
#endif

#define ECOMPASS_NAME 		"ecompass_ak8973s"
#define ECOMPASS_PHYSLEN 	128
#define AK8973S_VENDORID	1

#define ST_REG_ADDR		0xC0
#define TMPS_REG_ADDR		0xC1
#define H1X_REG_ADDR		0xC2
#define H1Y_REG_ADDR		0xC3
#define H1Z_REG_ADDR		0xC4
#define MS1_REG_ADDR		0xE0
#define HXDA_REG_ADDR		0xE1
#define HYDA_REG_ADDR		0xE2
#define HZDA_REG_ADDR		0xE3
#define HXGA_REG_ADDR		0xE4
#define HYGA_REG_ADDR		0xE5
#define HZGA_REG_ADDR		0xE6

#define ETS_REG_ADDR		0x62
#define EVIR_REG_ADDR		0x63
#define EIHE_REG_ADDR		0x64

#define EHXGA_REG_ADDR		0x66
#define EHYGA_REG_ADDR		0x67
#define EHZGA_REG_ADDR		0x68

#define SENSOR_DATA_MASK	0xFF

#define ST_INT_MASK			0x01
#define ST_WEN_MASK		0x02

#define MS1_MODE_MASK		0x03
#define MS1_WEN_MASK		0xF8

#define HXGA_MASK			0x0F
#define HYGA_MASK			0x0F
#define HZGA_MASK			0x0F

#define ETS_MASK			0x3F

#define EVIR_IREF_MASK		0x0F
#define EVIR_VREF_MASK		0xF0

#define EIHE_OSC_MASK		0x0F
#define EIHE_HE_MASK		0xF0

#define ST_WEN_SHIFT		1
#define MS1_WEN_SHIFT		3

#define EVIR_VREF_SHIFT		4
#define EIHE_HE_SHIFT		4

#define ST_INT_RST				0
#define ST_INT_INTR				1
#define ST_WEN_READ			(0 << ST_WEN_SHIFT)
#define ST_WEN_WRITE			(1 << ST_WEN_SHIFT)

#define MS1_MODE_MEASURE		0
#define MS1_MODE_EEPROM		2
#define MS1_MODE_PWR_DOWN	3

#define MS1_WEN_WRITE			(0x15 << MS1_WEN_SHIFT)
#define MS1_WEN_READ			(0x00 << MS1_WEN_SHIFT)

struct ecompass_driver_data_t
{
	struct i2c_client *i2c_ecompass_client;
	struct miscdevice *misc_ecompass_dev;
	struct work_struct        work_data;
	unsigned int gpio_reset;
	unsigned int gpio_intr;
	atomic_t	open_count;
	int user_pid;
	#if DBG_MONITOR
	struct timer_list dbg_monitor_timer;
	struct work_struct dbg_monitor_work_data;
	#endif
};

struct ecompass_magnetic_data_t
{
	int              magnetic_x;
	int              magnetic_y;
	int              magnetic_z;
	int              temp;
};

static struct ecompass_driver_data_t ecompass_driver_data = {
	.open_count = ATOMIC_INIT(0),
	.user_pid = 0,
};

static int i2c_ecompass_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int i2c_ecompass_remove(struct i2c_client *client);
static int i2c_ecompass_suspend(struct i2c_client *client, pm_message_t state);
static int i2c_ecompass_resume(struct i2c_client *client);
static int i2c_ecompass_command(struct i2c_client *client, unsigned int cmd, void *arg);

static int misc_ecompass_open(struct inode *inode_p, struct file *fp);
static int misc_ecompass_release(struct inode *inode_p, struct file *fp);
static long misc_ecompass_ioctl(struct file *fp, unsigned int cmd, unsigned long arg);
static ssize_t misc_ecompass_read(struct file *fp, char __user *buf, size_t count, loff_t *f_pos);
static ssize_t misc_ecompass_write(struct file *fp, const char __user *buf, size_t count, loff_t *f_pos);

static const struct i2c_device_id i2c_ecompass_id[] = {
	{ ECOMPASS_NAME, 0 },
	{}
};

static struct i2c_driver i2c_ecompass_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = ECOMPASS_NAME,
	},
	.probe	 = i2c_ecompass_probe,
	.remove	 = i2c_ecompass_remove,
	.suspend = i2c_ecompass_suspend,
	.resume  = i2c_ecompass_resume,
	.command = i2c_ecompass_command,
	.id_table = i2c_ecompass_id,
};

static struct file_operations misc_ecompass_fops = {
	.owner 	= THIS_MODULE,
	.open 	= misc_ecompass_open,
	.release = misc_ecompass_release,
	.unlocked_ioctl = misc_ecompass_ioctl,
	.read = misc_ecompass_read,
	.write = misc_ecompass_write,
};

static struct miscdevice misc_ecompass_device = {
	.minor 	= MISC_DYNAMIC_MINOR,
	.name 	= ECOMPASS_NAME,
	.fops 	= &misc_ecompass_fops,
};

static int i2c_ecompass_read(struct i2c_client *client, uint8_t regaddr, uint8_t *buf, uint32_t len)
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

	ECOMPASS_LOGD(KERN_DEBUG "[ECOMPASS] %s+, regaddr:0x%x, len:%d\n", __func__, regaddr, len);

	ret = i2c_transfer(client->adapter, msgs, 2);
	if (ret < 0)
	{
		ECOMPASS_LOGE(KERN_ERR "[ECOMPASS] %s::i2c_transfer failed-ret:%d\n", __func__, ret);
		return ret;
	}

	ECOMPASS_LOGD(KERN_DEBUG "[ECOMPASS] %s-\n", __func__);
	return 0;
}

static int i2c_ecompass_write(struct i2c_client *client, uint8_t regaddr, uint8_t *buf, uint32_t len)
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
		ECOMPASS_LOGE(KERN_ERR "[ECOMPASS] %s:: len > 31!!\n", __func__);
		return -ENOMEM;
	}

	ECOMPASS_LOGD(KERN_DEBUG "[ECOMPASS] %s+regaddr:0x%x, len:%d\n", __func__, regaddr, len);

	write_buf[0] = regaddr;
	memcpy((void *)(write_buf + 1), buf, len);
	ret = i2c_transfer(client->adapter, msgs, 1);
	if (ret < 0)
	{
		ECOMPASS_LOGE(KERN_ERR "[ECOMPASS] %s::i2c_transfer failed-ret:%d\n", __func__, ret);
		return ret;
	}

	ECOMPASS_LOGD(KERN_DEBUG "[ECOMPASS] %s-\n", __func__);
	return 0;
}

static int ecompass_reset_pin(struct ecompass_driver_data_t *driver_data, int value)
{
	ECOMPASS_LOGD(KERN_DEBUG "[ECOMPASS] %s+\n", __func__);
	gpio_set_value(driver_data->gpio_reset, value);
	ECOMPASS_LOGD(KERN_DEBUG "[ECOMPASS] %s-\n", __func__);
	return 0;
}

#if ENABLE_ECOMPASS_INT
static irqreturn_t ecompass_irq_handler(int irq, void *dev_id)
{
	struct ecompass_driver_data_t *driver_data = dev_get_drvdata((struct device *) dev_id);

	ECOMPASS_LOGD(KERN_DEBUG "[ECOMPASS] %s+\n", __func__);

	schedule_work(&driver_data->work_data);

	ECOMPASS_LOGD(KERN_DEBUG "[ECOMPASS] %s-\n", __func__);
	return IRQ_HANDLED;
}
#endif

#if DBG_MONITOR
static void debug_monitor(unsigned long data)
{
	struct ecompass_driver_data_t *driver_data = (struct ecompass_driver_data_t *) data;
	schedule_work(&driver_data->dbg_monitor_work_data);
	return;
}

static void debug_monitor_work_func(struct work_struct *work)
{
	int i;
	uint8_t rx_buf[7];
	uint8_t tx_buf;
	struct ecompass_driver_data_t *driver_data = container_of(work, struct ecompass_driver_data_t, dbg_monitor_work_data);
	ECOMPASS_LOGD(KERN_DEBUG "[ECOMPASS] %s+\n", __func__);

	memset(rx_buf, 0, sizeof(rx_buf));
	i2c_ecompass_read(driver_data->i2c_ecompass_client, ST_REG_ADDR, rx_buf, 5);

	for (i = 0; i < 5; i++)
	{
		ECOMPASS_LOGI(KERN_INFO "[ECOMPASS] [0x%2x] %2x\n", (i + 0xC0), rx_buf[i]);
	}

	memset(rx_buf, 0, sizeof(rx_buf));
	i2c_ecompass_read(driver_data->i2c_ecompass_client, MS1_REG_ADDR, rx_buf, sizeof(rx_buf));

	for (i = 0; i < sizeof(rx_buf); i++)
	{
		ECOMPASS_LOGI(KERN_INFO "[ECOMPASS] [0x%2x] %2x\n", (i + 0xE0), rx_buf[i]);
	}

	tx_buf = MS1_MODE_EEPROM;
	i2c_ecompass_write(driver_data->i2c_ecompass_client, MS1_REG_ADDR, &tx_buf, sizeof(tx_buf));

	memset(rx_buf, 0, sizeof(rx_buf));
	i2c_ecompass_read(driver_data->i2c_ecompass_client, ETS_REG_ADDR, rx_buf, sizeof(rx_buf));

	for (i = 0; i < sizeof(rx_buf); i++)
	{
		ECOMPASS_LOGI(KERN_INFO "[ECOMPASS] [0x%2x] %2x\n", (i + 0x62), rx_buf[i]);
	}

	tx_buf = MS1_MODE_PWR_DOWN;
	i2c_ecompass_write(driver_data->i2c_ecompass_client, MS1_REG_ADDR, &tx_buf, sizeof(tx_buf));

	mod_timer(&driver_data->dbg_monitor_timer, jiffies + HZ);
	ECOMPASS_LOGD(KERN_DEBUG "[ECOMPASS] %s-\n", __func__);
	return;
}
#endif

static int misc_ecompass_open(struct inode *inode_p, struct file *fp)
{
	ECOMPASS_LOGD(KERN_DEBUG "[ECOMPASS] %s+\n", __func__);
	ECOMPASS_LOGI(KERN_INFO "[ECOMPASS] inode_p->i_rdev: 0x%x\n", inode_p->i_rdev);
	atomic_inc(&ecompass_driver_data.open_count);

	ECOMPASS_LOGD(KERN_DEBUG "[ECOMPASS] %s-\n", __func__);
	return 0;
}

static int misc_ecompass_release(struct inode *inode_p, struct file *fp)
{
	ECOMPASS_LOGD(KERN_DEBUG "[ECOMPASS] %s+\n", __func__);
	atomic_dec(&ecompass_driver_data.open_count);
	ECOMPASS_LOGD(KERN_DEBUG "[ECOMPASS] %s-\n", __func__);
	return 0;
}

static long misc_ecompass_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	void __user *argp = (void __user *)arg;
	struct ecompass_ioctl_t ioctl_data;
	unsigned char *kbuf = NULL;
	unsigned char __user *ubuf = NULL;
	int pid;

	ECOMPASS_LOGD(KERN_DEBUG "[ECOMPASS] %s+cmd_nr:%d\n", __func__, _IOC_NR(cmd));

	if (_IOC_TYPE(cmd) != ECOMPASS_IOC_MAGIC)
	{
		ECOMPASS_LOGE(KERN_ERR "[ECOMPASS] %s::Not ECOMPASS_IOC_MAGIC\n", __func__);
		return -ENOTTY;
	}

	ECOMPASS_LOGI(KERN_INFO "[ECOMPASS] arg:%d\n", arg);

	if (cmd == ECOMPASS_IOC_REG_INT)
	{
		ret = !access_ok(VERIFY_READ, (void __user*)arg, _IOC_SIZE(cmd));
		if (ret)
		{
			ECOMPASS_LOGE(KERN_ERR "[ECOMPASS] %s::access_ok check err\n", __func__);
			return -EFAULT;
		}
	}
	else
	{
		if (cmd != ECOMPASS_IOC_RESET)
		{
			if (copy_from_user(&ioctl_data, (void *)argp, sizeof(struct ecompass_ioctl_t)))
			{
				ECOMPASS_LOGE(KERN_ERR "[ECOMPASS] %s::copy_from_user fail\n", __func__);
				return -EFAULT;
			}

			if (ioctl_data.len <= 0)
			{
				return -EFAULT;
			}

			ubuf = ioctl_data.buf;
		}

		if (_IOC_DIR(cmd) & _IOC_READ)
		{
			ret = !access_ok(VERIFY_WRITE, (void __user*)ubuf, ioctl_data.len);
		}
		if (_IOC_DIR(cmd) & _IOC_WRITE)
		{
			ret = !access_ok(VERIFY_READ, (void __user*)ubuf, ioctl_data.len);
		}
		if (ret)
		{
			ECOMPASS_LOGE(KERN_ERR "[ECOMPASS] %s::access_ok check err\n", __func__);
			return -EFAULT;
		}

		if (cmd == ECOMPASS_IOC_READ || cmd == ECOMPASS_IOC_WRITE)
		{
			kbuf = kmalloc(ioctl_data.len, GFP_KERNEL);
			if (!kbuf)
			{
				ECOMPASS_LOGE(KERN_ERR "[ECOMPASS] %s::kmalloc fail!!\n", __func__);
				return -ENOMEM;
			}
		}
	}

	switch(cmd)
	{
		case ECOMPASS_IOC_RESET:
			ecompass_reset_pin(&ecompass_driver_data, 1);
			ecompass_reset_pin(&ecompass_driver_data, 0);
			ecompass_reset_pin(&ecompass_driver_data, 1);
			break;
		case ECOMPASS_IOC_READ:
			ret = i2c_ecompass_read(ecompass_driver_data.i2c_ecompass_client, ioctl_data.regaddr, kbuf, ioctl_data.len);
			if (ret)
			{
				ECOMPASS_LOGE(KERN_ERR "[ECOMPASS] %s::i2c_ecompass_read fail-ret:%d\n", __func__, ret);
				ret = -EIO;
				goto misc_ecompass_ioctl_err_exit;
			}

			if (copy_to_user(ubuf, kbuf, ioctl_data.len))
			{
				ECOMPASS_LOGE(KERN_ERR "[ECOMPASS] %s::ECOMPASS_IOC_READ:copy_to_user fail-\n", __func__);
				ret = -EFAULT;
				goto misc_ecompass_ioctl_err_exit;
			}
			break;
		case ECOMPASS_IOC_WRITE:
			if (copy_from_user(kbuf, ubuf, ioctl_data.len))
			{
				ECOMPASS_LOGE(KERN_ERR "[ECOMPASS] %s::ECOMPASS_IOC_WRITE:copy_from_user fail-\n", __func__);
				ret = -EFAULT;
				goto misc_ecompass_ioctl_err_exit;
			}

			ret = i2c_ecompass_write(ecompass_driver_data.i2c_ecompass_client, ioctl_data.regaddr, kbuf, ioctl_data.len);
			if (ret)
			{
				ECOMPASS_LOGE(KERN_ERR "[ECOMPASS] %s::i2c_ecompass_write fail-ret:%d\n", __func__, ret);
				ret = -EIO;
				goto misc_ecompass_ioctl_err_exit;
			}
			break;
		case ECOMPASS_IOC_REG_INT:
			if (copy_from_user(&pid, (void *)argp, _IOC_SIZE(cmd)))
			{
				ECOMPASS_LOGE(KERN_ERR "[ECOMPASS] %s::ECOMPASS_IOC_REG_INT:copy_from_user fail-\n", __func__);
				ret = -EFAULT;
				goto misc_ecompass_ioctl_err_exit;
			}
			ecompass_driver_data.user_pid = pid;
			ECOMPASS_LOGI(KERN_INFO "[ECOMPASS] %s::user_pid=%d\n", __func__, ecompass_driver_data.user_pid);
			break;
		default:
			ECOMPASS_LOGE(KERN_ERR "[ECOMPASS] %s::default-\n", __func__);
			ret = -ENOTTY;
			break;
	}

misc_ecompass_ioctl_err_exit:

	if (kbuf)
		kfree(kbuf);

	ECOMPASS_LOGD(KERN_DEBUG "[ECOMPASS] %s-\n", __func__);
	return ret;
}

static ssize_t misc_ecompass_read(struct file *fp, char __user *buf, size_t count, loff_t *f_pos)
{
	int ret = 0;

	ECOMPASS_LOGD(KERN_DEBUG "[ECOMPASS] %s+\n", __func__);

	ECOMPASS_LOGD(KERN_DEBUG "[ECOMPASS] %s-\n", __func__);
	return ret;
}

static ssize_t misc_ecompass_write(struct file *fp, const char __user *buf, size_t count, loff_t *f_pos)
{
	int ret = 0;

	ECOMPASS_LOGD(KERN_DEBUG "[ECOMPASS] %s+fp:0x%x, buf:0x%x, count:%d, f_pos:%d\n", __func__, &fp, &buf, count, *f_pos);

	ECOMPASS_LOGD(KERN_DEBUG "[ECOMPASS] %s-\n", __func__);
	return ret;
}

#if ENABLE_ECOMPASS_INT
static void ecompass_intr_work_func(struct work_struct *work)
{
	ECOMPASS_LOGD(KERN_DEBUG "[ECOMPASS] %s+\n", __func__);

	ECOMPASS_LOGD(KERN_DEBUG "[ECOMPASS] %s-\n", __func__);
}
#endif

static int i2c_ecompass_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	struct ecompass_driver_data_t *ecompass_driver_data_p = &ecompass_driver_data;
	struct ecompass_platform_data_t *plat_data = client->dev.platform_data;
	unsigned char buf[1];

	ECOMPASS_LOGD(KERN_DEBUG "[ECOMPASS] %s+\n", __func__);

	PM_LOG_EVENT(PM_LOG_ON, PM_LOG_SENSOR_ECOMPASS);

	client->driver = &i2c_ecompass_driver;
	i2c_set_clientdata(client, &ecompass_driver_data);
	ecompass_driver_data_p->i2c_ecompass_client = client;

	ecompass_driver_data_p->gpio_intr = plat_data->gpiointr;
	ecompass_driver_data_p->gpio_reset = plat_data->gpioreset;
	ECOMPASS_LOGI(KERN_INFO "[ECOMPASS] %s::gpio_intr:%d, gpio_rst:%d\n", __func__, ecompass_driver_data_p->gpio_intr, ecompass_driver_data_p->gpio_reset);

	#if ENABLE_ECOMPASS_INT
	INIT_WORK(&ecompass_driver_data_p->work_data, ecompass_intr_work_func);

	if (!client->irq)
	{
		ECOMPASS_LOGE(KERN_ERR "[ECOMPASS] %s::IRQ number is NULL\n", __func__);
		ret = -1;
		goto probe_err_irq_number;
	}
	ECOMPASS_LOGI(KERN_INFO "[ECOMPASS] client->irq:%d\n", client->irq);
	#endif

	#if	DBG_MONITOR
	INIT_WORK(&ecompass_driver_data_p->dbg_monitor_work_data, debug_monitor_work_func);
	init_timer(&ecompass_driver_data_p->dbg_monitor_timer);
	ecompass_driver_data_p->dbg_monitor_timer.data = (unsigned long) ecompass_driver_data_p;
	ecompass_driver_data_p->dbg_monitor_timer.function = debug_monitor;
	ecompass_driver_data_p->dbg_monitor_timer.expires = jiffies + HZ;
	add_timer(&ecompass_driver_data_p->dbg_monitor_timer);
	#endif

	#if ENABLE_ECOMPASS_INT
	ret = request_irq(client->irq,
			 ecompass_irq_handler,
			 IRQF_TRIGGER_RISING,
			 ECOMPASS_NAME,
			 &client->dev);
	if (ret)
	{
		ECOMPASS_LOGE(KERN_ERR "[ECOMPASS] %s::request_irq fail!!\n", __func__);
		goto probe_err_req_irq;
	}
	#endif

	ecompass_reset_pin(&ecompass_driver_data, 0);
	ecompass_reset_pin(&ecompass_driver_data, 1);

	ret = i2c_ecompass_read(client, 0xE0, buf, 1);
	if (ret)
	{
 		ECOMPASS_LOGE(KERN_ERR "[ECOMPASS] %s::i2c_ecompass_read fail-ret:%d\n", __func__, ret);
		ret = -1;
		goto probe_err_i2c_ecompass_read;
	}

	if ((buf[0] & 0x03) != 0x03)
      	{
		ECOMPASS_LOGE(KERN_ERR "[ECOMPASS] %s::[E0] = %d != 3 error\n", __func__, buf[0]);
		ret = -1;
		goto probe_err_mode;
	}

	ret = misc_register(&misc_ecompass_device);
	if (ret)
	{
		ECOMPASS_LOGE(KERN_ERR "[ECOMPASS] %s:: misc_register error-ret:%d\n", __func__,ret );
		goto probe_err_misc_register;
	}

	ECOMPASS_LOGI(KERN_INFO "[ECOMPASS] misc_ecompass_device->devt:0x%x\n", misc_ecompass_device.this_device->devt);

	ECOMPASS_LOGD(KERN_DEBUG "[ECOMPASS] %s-\n", __func__);
	return 0;

probe_err_misc_register:
probe_err_mode:
probe_err_i2c_ecompass_read:
#if ENABLE_ECOMPASS_INT
	free_irq(client->irq, &client->dev);
probe_err_req_irq:
#endif
#if ENABLE_ECOMPASS_INT
probe_err_irq_number:
#endif
	i2c_set_clientdata(client, NULL);
	return ret;
}

static int i2c_ecompass_remove(struct i2c_client *client)
{
	struct ecompass_driver_data_t *driver_data;

	ECOMPASS_LOGD(KERN_DEBUG "[ECOMPASS] %s+\n", __func__);

	driver_data = i2c_get_clientdata(client);

    	misc_deregister(&misc_ecompass_device);

	#if ENABLE_ECOMPASS_INT
	free_irq(client->irq, &client->dev);
	#endif

	ecompass_reset_pin(driver_data, 0);

	i2c_set_clientdata(client, NULL);

	ECOMPASS_LOGD(KERN_DEBUG "[ECOMPASS] %s-\n", __func__);
	return 0;
}

static int i2c_ecompass_suspend(struct i2c_client *client, pm_message_t state)
{
	ECOMPASS_LOGD(KERN_DEBUG "[ECOMPASS] %s+\n", __func__);
	ECOMPASS_LOGD(KERN_DEBUG "[ECOMPASS] %s-\n", __func__);
	return 0;
}

static int i2c_ecompass_resume(struct i2c_client *client)
{
	ECOMPASS_LOGD(KERN_DEBUG "[ECOMPASS] %s+\n", __func__);
	ECOMPASS_LOGD(KERN_DEBUG "[ECOMPASS] %s-\n", __func__);
	return 0;
}

static int i2c_ecompass_command(struct i2c_client *client, unsigned int cmd, void *arg)
{
	ECOMPASS_LOGD(KERN_DEBUG "[ECOMPASS] %s+\n", __func__);
	ECOMPASS_LOGD(KERN_DEBUG "[ECOMPASS] %s-\n", __func__);
	return 0;
}

static int __init ecompass_init(void)
{
	int ret = 0;

	printk("BootLog, +%s+\n", __func__);

	memset(&ecompass_driver_data, 0, sizeof(struct ecompass_driver_data_t));

	ret = i2c_add_driver(&i2c_ecompass_driver);
	if (ret)
	{
		printk("BootLog, -%s-, i2c_add_driver failed, ret=%d\n", __func__, ret);
		return ret;
	}

	printk("BootLog, -%s-, ret=0\n", __func__);
	return 0;
}

static void __exit ecompass_exit(void)
{
	ECOMPASS_LOGD(KERN_DEBUG "[ECOMPASS] %s+\n", __func__);
	i2c_del_driver(&i2c_ecompass_driver);
	ECOMPASS_LOGD(KERN_DEBUG "[ECOMPASS] %s-\n", __func__);
}

module_init(ecompass_init);
module_exit(ecompass_exit);

MODULE_DESCRIPTION("AKM AK8973S E-compass Driver");
MODULE_LICENSE("GPL");

