#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/input.h>
#include <mach/sensors_daemon.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#define SENSORS_DAEMON_DEBUG_LOG_ENABLE 1
#define SENSORS_DAEMON_PRINTKE_ENABLE 1
#define SENSORS_DAEMON_PRINTKD_ENABLE 0
#define SENSORS_DAEMON_PRINTKW_ENABLE 1
#define SENSORS_DAEMON_PRINTKI_ENABLE 0

#if SENSORS_DAEMON_DEBUG_LOG_ENABLE
	#if SENSORS_DAEMON_PRINTKE_ENABLE
		#define SENSORS_DAEMON_PRINTKE printk
	#else
		#define SENSORS_DAEMON_PRINTKE(a...)
	#endif

	#if SENSORS_DAEMON_PRINTKD_ENABLE
		#define SENSORS_DAEMON_PRINTKD printk
	#else
		#define SENSORS_DAEMON_PRINTKD(a...)
	#endif

	#if SENSORS_DAEMON_PRINTKW_ENABLE
		#define SENSORS_DAEMON_PRINTKW printk
	#else
		#define SENSORS_DAEMON_PRINTKW(a...)
	#endif

	#if SENSORS_DAEMON_PRINTKI_ENABLE
		#define SENSORS_DAEMON_PRINTKI printk
	#else
		#define SENSORS_DAEMON_PRINTKI(a...)
	#endif
#else
	#define SENSORS_DAEMON_PRINTKE(a...)
	#define SENSORS_DAEMON_PRINTKD(a...)
	#define SENSORS_DAEMON_PRINTKW(a...)
	#define SENSORS_DAEMON_PRINTKI(a...)
#endif

#define SENSORS_DAEMON_NAME "sensors_daemon"

struct sensors_daemon_t {
	atomic_t open_count;
	unsigned int active_sensors;
	int magnetic_x;
	int magnetic_y;
	int magnetic_z;
	int azimuth;
	int pitch;
	int roll;
	int temperature;
	int accuracy;
	int accel_x;
	int accel_y;
	int accel_z;
	atomic_t wakeup;
	atomic_t delay_ms;
	atomic_t hal_inuse;
	atomic_t em_inuse;
	atomic_t is_suspend;
	atomic_t is_active;
	struct input_dev *input_dev;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend sensors_daemon_early_suspend;
#endif
};

static struct sensors_daemon_t sensors_daemon_data;

static DECLARE_WAIT_QUEUE_HEAD(active_wq);
DECLARE_MUTEX(sem_active_sensors);
DECLARE_MUTEX(sem_ecompass);
DECLARE_MUTEX(sem_gsensor);
DECLARE_MUTEX(sem_wakeup);

#ifdef CONFIG_HAS_EARLYSUSPEND
static void sensors_daemon_early_suspend(struct early_suspend *h)
{
	struct sensors_daemon_t *data = container_of(h, struct sensors_daemon_t,
							sensors_daemon_early_suspend);

	SENSORS_DAEMON_PRINTKD(KERN_DEBUG "[SENSORS DAEMON] %s+\n", __func__);
	atomic_set(&data->is_suspend, 1);
	wake_up(&active_wq);
	SENSORS_DAEMON_PRINTKD(KERN_DEBUG "[SENSORS DAEMON] %s-\n", __func__);
}

static void sensors_daemon_early_resume(struct early_suspend *h)
{
	struct sensors_daemon_t *data = container_of(h, struct sensors_daemon_t,
							sensors_daemon_early_suspend);

	SENSORS_DAEMON_PRINTKD(KERN_DEBUG "[SENSORS DAEMON] %s+\n", __func__);
	atomic_set(&data->is_suspend, 0);
	wake_up(&active_wq);
	SENSORS_DAEMON_PRINTKD(KERN_DEBUG "[SENSORS DAEMON] %s-\n", __func__);
}
#endif

static int misc_sensors_daemon_open(struct inode *inode_p, struct file *fp)
{
	int ret = 0;
	SENSORS_DAEMON_PRINTKD(KERN_DEBUG "[SENSORS DAEMON] %s+\n", __func__);
	atomic_inc(&sensors_daemon_data.open_count);
	SENSORS_DAEMON_PRINTKD(KERN_DEBUG "[SENSORS DAEMON] %s-ret=%d\n", __func__, ret);
	return ret;
}

static int misc_sensors_daemon_release(struct inode *inode_p, struct file *fp)
{
	int ret = 0;
	SENSORS_DAEMON_PRINTKD(KERN_DEBUG "[SENSORS DAEMON] %s+\n", __func__);
	atomic_dec(&sensors_daemon_data.open_count);
	SENSORS_DAEMON_PRINTKD(KERN_DEBUG "[SENSORS DAEMON] %s-ret=%d\n", __func__, ret);
	return ret;
}

static int report_ecompass(void)
{
	int ret = 0;
	struct sensors_daemon_t *data = &sensors_daemon_data;

	SENSORS_DAEMON_PRINTKD(KERN_DEBUG "[SENSORS DAEMON] %s+\n", __func__);

	input_report_abs(data->input_dev, ABS_RX, data->azimuth);
	input_report_abs(data->input_dev, ABS_RY, data->pitch);
	input_report_abs(data->input_dev, ABS_RZ, data->roll);
	input_report_abs(data->input_dev, ABS_RUDDER, data->accuracy);
	input_report_abs(data->input_dev, ABS_HAT0X, data->magnetic_x);
	input_report_abs(data->input_dev, ABS_HAT0Y, data->magnetic_y);
	input_report_abs(data->input_dev, ABS_BRAKE, data->magnetic_z);
	input_report_abs(data->input_dev, ABS_THROTTLE, data->temperature);

	input_sync(data->input_dev);

	SENSORS_DAEMON_PRINTKD(KERN_DEBUG "[SENSORS DAEMON] %s-ret=%d\n", __func__, ret);
	return ret;
}

static int report_gsensor(void)
{
	int ret = 0;
	struct sensors_daemon_t *data = &sensors_daemon_data;

	SENSORS_DAEMON_PRINTKD(KERN_DEBUG "[SENSORS DAEMON] %s+\n", __func__);

	input_report_abs(data->input_dev, ABS_X, data->accel_x);
	input_report_abs(data->input_dev, ABS_Y, data->accel_y);
	input_report_abs(data->input_dev, ABS_Z, data->accel_z);
	input_report_abs(data->input_dev, ABS_WHEEL, 3);

	input_sync(data->input_dev);

	SENSORS_DAEMON_PRINTKD(KERN_DEBUG "[SENSORS DAEMON] %s-ret=%d\n", __func__, ret);
	return ret;
}

static long misc_sensors_daemon_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	unsigned int temp_uint;
	int temp_int;
	struct sensors_daemon_ecompass_t ecompass_data;
	struct sensors_daemon_gsensor_t gsensor_data;
	struct sensors_daemon_status_t status;

	SENSORS_DAEMON_PRINTKD(KERN_DEBUG "[SENSORS DAEMON] %s+\n", __func__);

	if (_IOC_TYPE(cmd) != SENSORS_DAEMON_IOC_MAGIC)
	{
		SENSORS_DAEMON_PRINTKE(KERN_ERR "[SENSORS DAEMON] %s::Not SENSORS_DAEMON_IOC_MAGIC\n", __func__);
		return -ENOTTY;
	}

	if (_IOC_DIR(cmd) & _IOC_READ)
	{
		ret = !access_ok(VERIFY_WRITE, (void __user*)arg, _IOC_SIZE(cmd));
		if (ret)
		{
			SENSORS_DAEMON_PRINTKE(KERN_ERR "[SENSORS DAEMON] %s::access_ok check err\n", __func__);
			return -EFAULT;
		}
	}

	if (_IOC_DIR(cmd) & _IOC_WRITE)
	{
		ret = !access_ok(VERIFY_READ, (void __user*)arg, _IOC_SIZE(cmd));
		if (ret)
		{
			SENSORS_DAEMON_PRINTKE(KERN_ERR "[SENSORS DAEMON] %s::access_ok check err\n", __func__);
			return -EFAULT;
		}
	}

	switch(cmd)
	{
		case SENSORS_DAEMON_IOC_SET_ACTIVE_SENSORS:
			SENSORS_DAEMON_PRINTKD(KERN_DEBUG "[SENSORS DAEMON] SENSORS_DAEMON_IOC_SET_ACTIVE_SENSORS\n");
			if (down_interruptible(&sem_active_sensors) != 0)
			{
				SENSORS_DAEMON_PRINTKE(KERN_ERR "[SENSORS DAEMON] %s:: SENSORS_DAEMON_IOC_SET_ACTIVE_SENSORS:The sleep is interrupted by a signal\n", __func__);
				ret = -EFAULT;
				break;
			}
			if (copy_from_user(&sensors_daemon_data.active_sensors, (void __user*) arg, _IOC_SIZE(cmd)))
			{
				SENSORS_DAEMON_PRINTKE(KERN_ERR "[SENSORS DAEMON] %s::SENSORS_DAEMON_IOC_SET_ACTIVE_SENSORS:copy_from_user fail-\n", __func__);
				ret = -EFAULT;
			}
			up(&sem_active_sensors);
			break;

		case SENSORS_DAEMON_IOC_GET_ACTIVE_SENSORS:
			SENSORS_DAEMON_PRINTKD(KERN_DEBUG "[SENSORS DAEMON] SENSORS_DAEMON_IOC_GET_ACTIVE_SENSORS\n");
			if (down_interruptible(&sem_active_sensors) != 0)
			{
				SENSORS_DAEMON_PRINTKE(KERN_ERR "[SENSORS DAEMON] %s:: SENSORS_DAEMON_IOC_GET_ACTIVE_SENSORS:The sleep is interrupted by a signal\n", __func__);
				ret = -EFAULT;
				break;
			}
			if (copy_to_user((void __user*) arg, &sensors_daemon_data.active_sensors, _IOC_SIZE(cmd)))
			{
				SENSORS_DAEMON_PRINTKE(KERN_ERR "[SENSORS DAEMON] %s::SENSORS_DAEMON_IOC_GET_ACTIVE_SENSORS:copy_to_user fail-\n", __func__);
				ret = -EFAULT;
			}
			up(&sem_active_sensors);
			break;

		case SENSORS_DAEMON_IOC_SET_ECOMPASS:
			SENSORS_DAEMON_PRINTKD(KERN_DEBUG "[SENSORS DAEMON] SENSORS_DAEMON_IOC_SET_ECOMPASS\n");
			if (copy_from_user(&ecompass_data, (void __user*) arg, _IOC_SIZE(cmd)))
			{
				SENSORS_DAEMON_PRINTKE(KERN_ERR "[SENSORS DAEMON] %s::SENSORS_DAEMON_IOC_SET_ECOMPASS:copy_from_user fail-\n", __func__);
				ret = -EFAULT;
			}
			else
			{
				if (down_interruptible(&sem_ecompass) != 0)
				{
					SENSORS_DAEMON_PRINTKE(KERN_ERR "[SENSORS DAEMON] %s:: SENSORS_DAEMON_IOC_SET_ECOMPASS:The sleep is interrupted by a signal\n", __func__);
					ret = -EFAULT;
					break;
				}
				sensors_daemon_data.accuracy = ecompass_data.accuracy;
				sensors_daemon_data.azimuth = ecompass_data.azimuth;
				sensors_daemon_data.magnetic_x = ecompass_data.magnetic_x;
				sensors_daemon_data.magnetic_y = ecompass_data.magnetic_y;
				sensors_daemon_data.magnetic_z = ecompass_data.magnetic_z;
				sensors_daemon_data.pitch = ecompass_data.pitch;
				sensors_daemon_data.roll = ecompass_data.roll;
				sensors_daemon_data.temperature = ecompass_data.temperature;
				report_ecompass();
				up(&sem_ecompass);
			}
			break;

		case SENSORS_DAEMON_IOC_GET_ECOMPASS:
			SENSORS_DAEMON_PRINTKD(KERN_DEBUG "[SENSORS DAEMON] SENSORS_DAEMON_IOC_GET_ECOMPASS\n");
			if (down_interruptible(&sem_ecompass) != 0)
			{
				SENSORS_DAEMON_PRINTKE(KERN_ERR "[SENSORS DAEMON] %s:: SENSORS_DAEMON_IOC_GET_ECOMPASS:The sleep is interrupted by a signal\n", __func__);
				ret = -EFAULT;
				break;
			}
			ecompass_data.accuracy = sensors_daemon_data.accuracy;
			ecompass_data.azimuth = sensors_daemon_data.azimuth;
			ecompass_data.magnetic_x = sensors_daemon_data.magnetic_x;
			ecompass_data.magnetic_y = sensors_daemon_data.magnetic_y;
			ecompass_data.magnetic_z = sensors_daemon_data.magnetic_z;
			ecompass_data.pitch = sensors_daemon_data.pitch;
			ecompass_data.roll = sensors_daemon_data.roll;
			ecompass_data.temperature = sensors_daemon_data.temperature;
			up(&sem_ecompass);
			if (copy_to_user((void __user*) arg, &ecompass_data, _IOC_SIZE(cmd)))
			{
				SENSORS_DAEMON_PRINTKE(KERN_ERR "[SENSORS DAEMON] %s::SENSORS_DAEMON_IOC_GET_ECOMPASS:copy_to_user fail-\n", __func__);
				ret = -EFAULT;
			}
			break;

		case SENSORS_DAEMON_IOC_SET_GSENSOR:
			SENSORS_DAEMON_PRINTKD(KERN_DEBUG "[SENSORS DAEMON] SENSORS_DAEMON_IOC_SET_GSENSOR\n");
			if (copy_from_user(&gsensor_data, (void __user*) arg, _IOC_SIZE(cmd)))
			{
				SENSORS_DAEMON_PRINTKE(KERN_ERR "[SENSORS DAEMON] %s::SENSORS_DAEMON_IOC_SET_GSENSOR:copy_from_user fail-\n", __func__);
				ret = -EFAULT;
			}
			else
			{
				if (down_interruptible(&sem_gsensor) != 0)
				{
					SENSORS_DAEMON_PRINTKE(KERN_ERR "[SENSORS DAEMON] %s:: SENSORS_DAEMON_IOC_SET_GSENSOR:The sleep is interrupted by a signal\n", __func__);
					ret = -EFAULT;
					break;
				}
				sensors_daemon_data.accel_x = gsensor_data.accel_x;
				sensors_daemon_data.accel_y = gsensor_data.accel_y;
				sensors_daemon_data.accel_z = gsensor_data.accel_z;
				report_gsensor();
				up(&sem_gsensor);
			}
			break;

		case SENSORS_DAEMON_IOC_GET_GSENSOR:
			SENSORS_DAEMON_PRINTKD(KERN_DEBUG "[SENSORS DAEMON] SENSORS_DAEMON_IOC_GET_GSENSOR\n");
			if (down_interruptible(&sem_gsensor) != 0)
			{
				SENSORS_DAEMON_PRINTKE(KERN_ERR "[SENSORS DAEMON] %s:: SENSORS_DAEMON_IOC_GET_GSENSOR:The sleep is interrupted by a signal\n", __func__);
				ret = -EFAULT;
				break;
			}
			gsensor_data.accel_x = sensors_daemon_data.accel_x;
			gsensor_data.accel_y = sensors_daemon_data.accel_y;
			gsensor_data.accel_z = sensors_daemon_data.accel_z;
			up(&sem_gsensor);
			if (copy_to_user((void __user*) arg, &gsensor_data, _IOC_SIZE(cmd)))
			{
				SENSORS_DAEMON_PRINTKE(KERN_ERR "[SENSORS DAEMON] %s::SENSORS_DAEMON_IOC_GET_GSENSOR:copy_to_user fail-\n", __func__);
				ret = -EFAULT;
			}
			break;

		case SENSORS_DAEMON_IOC_GET_STATUS:
			SENSORS_DAEMON_PRINTKD(KERN_DEBUG "[SENSORS DAEMON] SENSORS_DAEMON_IOC_GET_STATUS\n");
			temp_uint = 0;
			if (atomic_read(&sensors_daemon_data.is_active) == 1)
			{
				temp_uint |= SENSORS_DAEMON_RUNNING;
			}
			else
			{
				temp_uint |= SENSORS_DAEMON_NO_ACTIVE;
			}
			if (atomic_read(&sensors_daemon_data.is_suspend) == 1)
			{
				temp_uint |= SENSORS_DAEMON_SUSPEND;
			}

			status.status = temp_uint;
			status.activesensors = sensors_daemon_data.active_sensors;
			status.em_inuse = atomic_read(&sensors_daemon_data.em_inuse);

			if (copy_to_user((void __user*) arg, &status, _IOC_SIZE(cmd)))
			{
				SENSORS_DAEMON_PRINTKE(KERN_ERR "[SENSORS DAEMON] %s::SENSORS_DAEMON_IOC_GET_STATUS:copy_to_user fail-\n", __func__);
				ret = -EFAULT;
			}
			break;

		case SENSORS_DAEMON_IOC_HAL_INUSE:
			SENSORS_DAEMON_PRINTKD(KERN_DEBUG "[SENSORS DAEMON] SENSORS_DAEMON_IOC_HAL_INUSE\n");
			if (copy_from_user(&temp_uint, (void __user*) arg, _IOC_SIZE(cmd)))
			{
				SENSORS_DAEMON_PRINTKE(KERN_ERR "[SENSORS DAEMON] %s::SENSORS_DAEMON_IOC_HAL_INUSE:copy_from_user fail-\n", __func__);
				ret = -EFAULT;
			}
			else
			{
				if (down_interruptible(&sem_wakeup) != 0)
				{
					SENSORS_DAEMON_PRINTKE(KERN_ERR "[SENSORS DAEMON] %s:: SENSORS_DAEMON_IOC_HAL_INUSE:The sleep is interrupted by a signal\n", __func__);
					ret = -EFAULT;
					wake_up(&active_wq);
					break;
				}
				if (temp_uint == 1)
					atomic_inc(&sensors_daemon_data.hal_inuse);
				else if (temp_uint == 0)
					atomic_dec(&sensors_daemon_data.hal_inuse);
				if (atomic_read(&sensors_daemon_data.hal_inuse) < 0)
				{
					SENSORS_DAEMON_PRINTKE(KERN_ERR "[SENSORS DAEMON] SENSORS_DAEMON_IOC_HAL_INUSE::Error!!! hal_inuse=%d\n", atomic_read(&sensors_daemon_data.hal_inuse));
					atomic_set(&sensors_daemon_data.hal_inuse, 0);
				}

				if (atomic_read(&sensors_daemon_data.hal_inuse) > 0 || atomic_read(&sensors_daemon_data.em_inuse) > 0)
					atomic_set(&sensors_daemon_data.is_active, 1);
				else
					atomic_set(&sensors_daemon_data.is_active, 0);

				up(&sem_wakeup);

				wake_up(&active_wq);
			}
			break;

		case SENSORS_DAEMON_IOC_EM_INUSE:
			SENSORS_DAEMON_PRINTKD(KERN_DEBUG "[SENSORS DAEMON] SENSORS_DAEMON_IOC_EM_INUSE\n");
			if (copy_from_user(&temp_uint, (void __user*) arg, _IOC_SIZE(cmd)))
			{
				SENSORS_DAEMON_PRINTKE(KERN_ERR "[SENSORS DAEMON] %s::SENSORS_DAEMON_IOC_EM_INUSE:copy_from_user fail-\n", __func__);
				ret = -EFAULT;
			}
			else
			{
				if (down_interruptible(&sem_wakeup) != 0)
				{
					SENSORS_DAEMON_PRINTKE(KERN_ERR "[SENSORS DAEMON] %s:: SENSORS_DAEMON_IOC_EM_INUSE:The sleep is interrupted by a signal\n", __func__);
					ret = -EFAULT;
					wake_up(&active_wq);
					break;
				}
				if (temp_uint == 1)
					atomic_inc(&sensors_daemon_data.em_inuse);
				else if (temp_uint == 0)
					atomic_dec(&sensors_daemon_data.em_inuse);
				if (atomic_read(&sensors_daemon_data.em_inuse) < 0)
				{
					SENSORS_DAEMON_PRINTKE(KERN_ERR "[SENSORS DAEMON] SENSORS_DAEMON_IOC_EM_INUSE::Error!!! em_inuse=%d\n", atomic_read(&sensors_daemon_data.em_inuse));
					atomic_set(&sensors_daemon_data.em_inuse, 0);
				}

				if (atomic_read(&sensors_daemon_data.hal_inuse) > 0 || atomic_read(&sensors_daemon_data.em_inuse) > 0)
					atomic_set(&sensors_daemon_data.is_active, 1);
				else
					atomic_set(&sensors_daemon_data.is_active, 0);

				up(&sem_wakeup);

				wake_up(&active_wq);
			}
			break;

		case SENSORS_DAEMON_IOC_BLOCKING:
			SENSORS_DAEMON_PRINTKD(KERN_DEBUG "[SENSORS DAEMON] SENSORS_DAEMON_IOC_BLOCKING\n");
			wait_event_interruptible(active_wq, (atomic_read(&sensors_daemon_data.is_active) == 1) && (atomic_read(&sensors_daemon_data.is_suspend) == 0));
			break;

		case SENSORS_DAEMON_IOC_SET_DELAY_MS:
			SENSORS_DAEMON_PRINTKD(KERN_DEBUG "[SENSORS DAEMON] SENSORS_DAEMON_IOC_SET_DELAY_MS\n");
			if (copy_from_user(&temp_int, (void __user*) arg, _IOC_SIZE(cmd)))
			{
				SENSORS_DAEMON_PRINTKE(KERN_ERR "[SENSORS DAEMON] %s::SENSORS_DAEMON_IOC_SET_DELAY_MS:copy_from_user fail-\n", __func__);
				ret = -EFAULT;
			}
			else
			{
				atomic_set(&sensors_daemon_data.delay_ms, temp_int);
			}
			break;

		case SENSORS_DAEMON_IOC_GET_DELAY_MS:
			SENSORS_DAEMON_PRINTKD(KERN_DEBUG "[SENSORS DAEMON] SENSORS_DAEMON_IOC_GET_DELAY_MS\n");
			temp_int = atomic_read(&sensors_daemon_data.delay_ms);
			if (copy_to_user((void __user*) arg, &temp_int, _IOC_SIZE(cmd)))
			{
				SENSORS_DAEMON_PRINTKE(KERN_ERR "[SENSORS DAEMON] %s::SENSORS_DAEMON_IOC_GET_DELAY_MS:copy_to_user fail-\n", __func__);
				ret = -EFAULT;
			}
			break;

		case SENSORS_DAEMON_IOC_SET_WAKEUP:
			SENSORS_DAEMON_PRINTKD(KERN_DEBUG "[SENSORS DAEMON] SENSORS_DAEMON_IOC_SET_WAKEUP\n");
			if (copy_from_user(&temp_int, (void __user*) arg, _IOC_SIZE(cmd)))
			{
				SENSORS_DAEMON_PRINTKE(KERN_ERR "[SENSORS DAEMON] %s::SENSORS_DAEMON_IOC_SET_WAKEUP:copy_from_user fail-\n", __func__);
				ret = -EFAULT;
			}
			else
			{
				atomic_set(&sensors_daemon_data.wakeup, temp_int);
			}
			break;

		case SENSORS_DAEMON_IOC_GET_WAKEUP:
			SENSORS_DAEMON_PRINTKD(KERN_DEBUG "[SENSORS DAEMON] SENSORS_DAEMON_IOC_GET_WAKEUP\n");
			temp_int = atomic_read(&sensors_daemon_data.wakeup);
			if (copy_to_user((void __user*) arg, &temp_int, _IOC_SIZE(cmd)))
			{
				SENSORS_DAEMON_PRINTKE(KERN_ERR "[SENSORS DAEMON] %s::SENSORS_DAEMON_IOC_GET_WAKEUP:copy_to_user fail-\n", __func__);
				ret = -EFAULT;
			}
			break;

		case SENSORS_DAEMON_IOC_GET_HW_VER:
			SENSORS_DAEMON_PRINTKD(KERN_DEBUG "[SENSORS DAEMON] SENSORS_DAEMON_IOC_GET_HW_VER\n");
			if (copy_to_user((void __user*) arg, &system_rev, _IOC_SIZE(cmd)))
			{
				SENSORS_DAEMON_PRINTKE(KERN_ERR "[SENSORS DAEMON] %s::SENSORS_DAEMON_IOC_GET_HW_VER:copy_to_user fail-\n", __func__);
				ret = -EFAULT;
			}
			break;

		default:
			SENSORS_DAEMON_PRINTKI(KERN_INFO "[SENSORS DAEMON] %s::deafult\n", __func__);
			break;
	}

	SENSORS_DAEMON_PRINTKD(KERN_DEBUG "[SENSORS DAEMON] %s-ret=%d\n", __func__, ret);
	return ret;
}

static struct file_operations misc_sensors_daemon_fops = {
	.owner 	= THIS_MODULE,
	.open 	= misc_sensors_daemon_open,
	.release = misc_sensors_daemon_release,
	.unlocked_ioctl = misc_sensors_daemon_ioctl,
};

static struct miscdevice misc_sensors_daemon = {
	.minor 	= MISC_DYNAMIC_MINOR,
	.name 	= SENSORS_DAEMON_NAME,
	.fops 	= &misc_sensors_daemon_fops,
};

static int __init sensors_daemon_init(void)
{
	int ret = 0;
	struct sensors_daemon_t *data = &sensors_daemon_data;

	printk("BootLog, +%s+\n", __func__);

	memset(&sensors_daemon_data, 0, sizeof(struct sensors_daemon_t));
	atomic_set(&sensors_daemon_data.open_count, 0);
	atomic_set(&sensors_daemon_data.hal_inuse, 0);
	atomic_set(&sensors_daemon_data.em_inuse, 0);
	atomic_set(&sensors_daemon_data.is_suspend, 0);
	atomic_set(&sensors_daemon_data.is_active, 0);
	atomic_set(&sensors_daemon_data.wakeup, 0);
	atomic_set(&sensors_daemon_data.delay_ms, 0);

	/* Register a misc device */
	ret = misc_register(&misc_sensors_daemon);
	if (ret)
	{
		printk("BootLog, -%s-, misc_register error, ret=%d\n", __func__, ret);
		return ret;
	}

	data->input_dev = input_allocate_device();
	if (!data->input_dev) {
		ret = -ENOMEM;
		SENSORS_DAEMON_PRINTKE(KERN_ERR "[SENSORS DAEMON] %s:: Failed to allocate input device\n", __func__);
		goto exit_input_dev_alloc_failed;
	}

	set_bit(EV_ABS, data->input_dev->evbit);
	/* yaw */
	input_set_abs_params(data->input_dev, ABS_RX, 0, 360, 0, 0);
	/* pitch */
	input_set_abs_params(data->input_dev, ABS_RY, -180, 180, 0, 0);
	/* roll */
	input_set_abs_params(data->input_dev, ABS_RZ, -90, 90, 0, 0);
	/* x-axis acceleration */
	input_set_abs_params(data->input_dev, ABS_X, -512, 512, 0, 0);
	/* y-axis acceleration */
	input_set_abs_params(data->input_dev, ABS_Y, -512, 512, 0, 0);
	/* z-axis acceleration */
	input_set_abs_params(data->input_dev, ABS_Z, -512, 512, 0, 0);
	/* temparature */
	input_set_abs_params(data->input_dev, ABS_THROTTLE, -50, 110, 0, 0);
	/* status of magnetic sensor */
	input_set_abs_params(data->input_dev, ABS_RUDDER, -32768, 3, 0, 0);
	/* status of acceleration sensor */
	input_set_abs_params(data->input_dev, ABS_WHEEL, -32768, 3, 0, 0);
	/* step count */
	input_set_abs_params(data->input_dev, ABS_GAS, 0, 65535, 0, 0);
	/* x-axis of raw magnetic vector */
	input_set_abs_params(data->input_dev, ABS_HAT0X, -2048, 2032, 0, 0);
	/* y-axis of raw magnetic vector */
	input_set_abs_params(data->input_dev, ABS_HAT0Y, -2048, 2032, 0, 0);
	/* z-axis of raw magnetic vector */
	input_set_abs_params(data->input_dev, ABS_BRAKE, -2048, 2032, 0, 0);

	data->input_dev->name = "sensors_daemon";

	ret = input_register_device(data->input_dev);
	if (ret) {
		printk(KERN_ERR "[SENSORS DAEMON] %s:: Unable to register input device: %s\n", __func__, data->input_dev->name);
		goto exit_input_register_device_failed;
	}

	init_waitqueue_head(&active_wq);

#ifdef CONFIG_HAS_EARLYSUSPEND
	sensors_daemon_data.sensors_daemon_early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	sensors_daemon_data.sensors_daemon_early_suspend.suspend = sensors_daemon_early_suspend;
	sensors_daemon_data.sensors_daemon_early_suspend.resume = sensors_daemon_early_resume;
	register_early_suspend(&sensors_daemon_data.sensors_daemon_early_suspend);
#endif

	printk("BootLog, -%s-, ret=%d\n", __func__, ret);
	return ret;

exit_input_register_device_failed:
	input_free_device(data->input_dev);
exit_input_dev_alloc_failed:
	misc_deregister(&misc_sensors_daemon);
	printk("BootLog, -%s-, ret=%d\n", __func__, ret);
	return ret;
}

static void __exit sensors_daemon_exit(void)
{
	struct sensors_daemon_t *data = &sensors_daemon_data;
	SENSORS_DAEMON_PRINTKD(KERN_DEBUG "[SENSORS DAEMON] %s+\n", __func__);
	input_unregister_device(data->input_dev);
	misc_deregister(&misc_sensors_daemon);
	SENSORS_DAEMON_PRINTKD(KERN_DEBUG "[SENSORS DAEMON] %s-\n", __func__);
}

module_init(sensors_daemon_init);
module_exit(sensors_daemon_exit);

MODULE_DESCRIPTION("Sensors daemon");
MODULE_LICENSE("GPL");

