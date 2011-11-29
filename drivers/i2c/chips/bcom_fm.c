#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <mach/bcom_fm.h>
#include <linux/delay.h>
#include <mach/gpio.h>
#include <linux/irq.h>

#define DRIVER_VERSION "v1.0"

static const char *bcomfm_name  = "bcom_fm";
static dev_t	bcomfm_device_no;
static struct cdev	bcomfm_cdev;
static struct class	*bcom_class;

void rds_notify_work_func(struct work_struct *work);
DECLARE_DELAYED_WORK(rds_read_work, rds_notify_work_func);

MODULE_VERSION(DRIVER_VERSION);
MODULE_AUTHOR("Terry.Cheng");
MODULE_DESCRIPTION("Bcom 4325 FM driver");
MODULE_LICENSE("GPL v2");

struct bcomfm_data {
	struct i2c_client *bcomfm_client;
	uint16_t rds_signal_pid;
	atomic_t 	b_rds_on;
	atomic_t irq_fm_wakeup_enable;
	uint16_t  gpio_fm_wakeup;
};

static struct bcomfm_data fm_data;

static int bcomfm_read(struct i2c_client *fm, uint8_t regaddr,
		     uint8_t *buf, uint32_t rdlen)
{

	uint8_t ldat = regaddr;
	int rc = 0;
	int retry = 0;

	struct i2c_msg msgs[] = {
		[0] = {
			.addr	= fm->addr,
			.flags	= 0,
			.buf	= (void *)&ldat,
			.len	= 1
		},
		[1] = {
			.addr	= fm->addr,
			.flags	= I2C_M_RD,
			.buf	= (void *)buf,
			.len	= rdlen
		}
	};
	do{
		rc = i2c_transfer(fm->adapter, msgs, 2);
		BCOMFM_DBG("retry = %d, rc = %d", retry, rc);
		retry ++;
		mdelay(50);
	}while( ((2 != rc) && (10 > retry)) );


	return rc;
}

static int bcomfm_write(struct i2c_client *fm, uint8_t regaddr, uint8_t *data, int wlen)
{

	uint8_t buf[LOCAL_I2C_BUFFER_SIZE] = {0};
	int i = 0;
	int rc = 0;
	int retry = 0;

	struct i2c_msg msgs[] = {
		[0] = {
			.addr	= fm->addr,
			.flags	= 0,
			.buf	= (void *)buf,
			.len	= wlen+1
		}
	};

	buf[0] =  regaddr;
	for (i = 0; i < wlen; i++){
		buf[i+1] = data[i];
	}
	do
	{
		rc = i2c_transfer(fm->adapter, msgs, 1);
		BCOMFM_DBG("retry = %d, rc = %d", retry, rc);
		retry++;
		mdelay(50);
	}while( ((1 != rc) && (10 > retry)));
	return rc;
}

void rds_notify_work_func(struct work_struct *work)
{
	BCOMFM_DBG("fm_data.b_rds_on = %d", atomic_read(&fm_data.b_rds_on));
	if ( 1 == atomic_read(&fm_data.b_rds_on))
	{
		struct siginfo info;
		info.si_signo = SIGUSR1;
		info.si_errno = 0;
		info.si_code = SI_USER;
		info.si_pid = current->tgid;
		kill_proc_info(SIGUSR1, &info, fm_data.rds_signal_pid);
	}
}

static irqreturn_t bcomfm_irqhandler(int irq, void *dev_id)
{
	unsigned long irq_flags;
	BCOMFM_DBG("atomic_read(&fm_data.irq_fm_wakeup_enable) = %d",atomic_read(&fm_data.irq_fm_wakeup_enable));
	if (atomic_read(&fm_data.irq_fm_wakeup_enable))
	{
		local_irq_save(irq_flags);
		disable_irq_nosync(fm_data.bcomfm_client->irq);
		local_irq_restore(irq_flags);
		atomic_set(&fm_data.irq_fm_wakeup_enable, 0);
		schedule_delayed_work(&rds_read_work, 0);
		BCOMFM_INFO("Disable FM wake up ");
	}
	return IRQ_HANDLED;
}

static int bcomfm_ioctl(struct inode *inode, struct file *f,
		unsigned int cmd, unsigned long arg)

{
	void __user *argp = (void __user *)arg;
	long   rc = 0;
	unsigned long irq_flags;

	switch (cmd) {
	case BCOMFM_POWER_ON_IO:
	{
		uint8_t fm_power = 0;
		uint8_t data =0;

		fm_power |= BCOMFM_POWER_PWF;
		bcomfm_write(fm_data.bcomfm_client, I2C_FM_RDS_SYSTEM, &fm_power, sizeof(fm_power));
		BCOMFM_INFO("BCOMFM_POWER_ON_IO:BCOMFM_POWER_PWF Done \n");

		mdelay(10);
		data= BCOMFM_BAND_EUUS;
		data |=BCOMFM_AUTO_SELECT_STEREO_MONO;
		BCOMFM_INFO("BCOMFM_POWER_ON_IO:I2C_FM_CTRL data = 0x%x \n",data );
		bcomfm_write(fm_data.bcomfm_client, I2C_FM_CTRL, &data, sizeof(data));

		data= (BCOMFM_DAC_OUTPUT_LEFT|BCOMFM_DAC_OUTPUT_RIGHT|
			  BCOMFM_DE_EMPHASIS_75|BCOMFM_AUDIO_ROUTE_DAC);
		BCOMFM_INFO("BCOMFM_POWER_ON_IO:I2C_FM_AUDIO_CTRL0 data = 0x%x \n",data );
		bcomfm_write(fm_data.bcomfm_client, I2C_FM_AUDIO_CTRL0, &data, sizeof(data));
	}
	break;

	case BCOMFM_POWER_OFF_IO:
	{
		uint8_t data =0;
		data = 0x00;
		bcomfm_write(fm_data.bcomfm_client, I2C_FM_RDS_SYSTEM, &data, sizeof(data));
		atomic_set(&fm_data.b_rds_on, 0);
		BCOMFM_INFO("BCOMFM_POWER_OFF_IO: atomic_read(&fm_data.irq_fm_wakeup_enable) = %d\n",atomic_read(&fm_data.irq_fm_wakeup_enable));
		if (atomic_read(&fm_data.irq_fm_wakeup_enable))
		{
			local_irq_save(irq_flags);
			disable_irq_nosync(fm_data.bcomfm_client->irq);
			local_irq_restore(irq_flags);
			atomic_set(&fm_data.irq_fm_wakeup_enable, 0);
			BCOMFM_INFO("Disable FM wake up ");
		}
	}
	break;

	case BCOMFM_SPECIFIED_FREQ_SET:
	{
		int freq = 0;
		uint8_t freqdata[2] = {0};
		uint8_t data = 0;
		uint8_t flag_status[2]= {0};
		uint16_t count =0;
		rc = copy_from_user(&freq, argp , sizeof(int));
		if (rc > 0) {
			BCOMFM_ERR(" BCOMFM_POWER_ON_IO fail rc = %x\n",(unsigned int)rc);
			return -EFAULT;
		}
		BCOMFM_INFO("BCOMFM_SPECIFIED_FREQ_SET: freq = %x\n", freq);

		freq = (freq * 100) - 64000;
    		freqdata[0] = (uint8_t) ( freq & 0xff );
    		freqdata[1] = (uint8_t) ( freq >> 8 );
		BCOMFM_INFO("BCOMFM_SPECIFIED_FREQ_SET: freq =0x%x 0x%x \n", freqdata[0], freqdata[1]);
		bcomfm_write(fm_data.bcomfm_client, I2C_FM_FREQ0, freqdata, sizeof(freqdata));

		data = BCOMFM_PRE_SET;
		bcomfm_write(fm_data.bcomfm_client, I2C_FM_SEARCH_TUNE_MODE, &data, sizeof(data));
		BCOMFM_INFO("BCOMFM_SPECIFIED_FREQ_SET: set tuning mode as 0x%x\n", data);

		do {
			mdelay(50);
			bcomfm_read(fm_data.bcomfm_client , I2C_FM_RDS_FLAG0,  flag_status,  sizeof(flag_status));
			BCOMFM_INFO("I2C_FM_RDS_FLAG0 :flag_status[0] = %x, flag_status[1] = %x cout = %d\n", flag_status[0], flag_status[1], count);
			count++;
		}while( (0 == (flag_status[0] & 0x01) ) && (count < BCOMFM_MAX_TUNING_TRIALS));

		BCOMFM_INFO("BCOMFM_SPECIFIED_FREQ_SET: RDS power = %d\n", atomic_read(&fm_data.b_rds_on));
		if ( 1 == atomic_read(&fm_data.b_rds_on))
		{
			uint8_t flag_status[2]= {0};
			uint8_t int_mask[2] = {0};
			bcomfm_read(fm_data.bcomfm_client , I2C_FM_RDS_FLAG0,  flag_status,  sizeof(flag_status));
			BCOMFM_DBG("flag_status[0] = %x, flag_status[1] = %x\n", flag_status[0], flag_status[1]);
			int_mask[0]=0x00;
			int_mask[1]=BCOMFM_FLAG_RDS_FIFO_WLINE;
			bcomfm_write(fm_data.bcomfm_client, I2C_FM_RDS_MASK0, int_mask, sizeof(int_mask));
			BCOMFM_DBG("int_mask[0] = %x, int_mask[1] = %x\n", int_mask[0], int_mask[1]);
		}
	}
	break;

	case BCOMFM_FRE_SCAN:
	{
		uint8_t scan_dir = 0;
		uint8_t data = 0;
		uint8_t flag_status[2]= {0};
		uint16_t count = 0;

		rc = copy_from_user(&scan_dir, argp , sizeof(uint8_t));
		BCOMFM_INFO("BCOMFM_FRE_SCAN: scan_dir= %d\n", scan_dir);

		data = 0x5f;
		if( TRUE == scan_dir)
		{
			data |= BCOMFM_SERACH_UP;
		}
		bcomfm_write(fm_data.bcomfm_client, I2C_FM_SEARCH_CTRL0, &data, sizeof(data));

		data = 0x00;
		data = BCOMFM_AUTO_SERCH;
		bcomfm_write(fm_data.bcomfm_client, I2C_FM_SEARCH_TUNE_MODE, &data, sizeof(data));
		BCOMFM_INFO("BCOMFM_FRE_SCAN: set tuning mode as 0x%x\n", data);

		do {
			mdelay(50);
			bcomfm_read(fm_data.bcomfm_client , I2C_FM_RDS_FLAG0,  flag_status,  sizeof(flag_status));
			count++;
			BCOMFM_INFO("I2C_FM_RDS_FLAG0 :flag_status[0] = %x, flag_status[1] = %x count = %d\n", flag_status[0], flag_status[1],count);
		}while( ((0 == (flag_status[0] & 0x01) ) || (1 == (flag_status[0] & 0x02) )) && (count < BCOMFM_MAX_TUNING_TRIALS));

		if((1 == (flag_status[0] & BCOMFM_FLAG_SEARCH_FAIL) ))
		{
			BCOMFM_ERR(" BCOMFM_FRE_SCAN fail  flag_status[0] = %x \n",flag_status[0]);
			return -EFAULT;
		}
		if ( 1 == atomic_read(&fm_data.b_rds_on))
		{
			uint8_t flag_status[2]= {0};
			uint8_t int_mask[2] = {0};
			bcomfm_read(fm_data.bcomfm_client , I2C_FM_RDS_FLAG0,  flag_status,  sizeof(flag_status));
			BCOMFM_DBG("flag_status[0] = %x, flag_status[1] = %x\n", flag_status[0], flag_status[1]);
			int_mask[0]=0x00;
			int_mask[1]=BCOMFM_FLAG_RDS_FIFO_WLINE;
			bcomfm_write(fm_data.bcomfm_client, I2C_FM_RDS_MASK0, int_mask, sizeof(int_mask));
			BCOMFM_DBG("int_mask[0] = %x, int_mask[1] = %x\n", int_mask[0], int_mask[1]);
		}
		BCOMFM_INFO("BCOMFM_FRE_SCAN:\n");
	}
	break;

	case BCOMFM_GET_FREQ:
	{
		uint8_t freqdata[2] = {0};
		uint32_t 		freq = 0;
		bcomfm_read(fm_data.bcomfm_client , I2C_FM_FREQ0,  freqdata,  sizeof(freqdata));
		BCOMFM_INFO("BCOMFM_GET_FREQ :freqdata[0] = %x, freqdata[1] = %x\n", freqdata[0], freqdata[1]);
		freq = (freqdata[1] << 8) | freqdata[0];
		freq = (freq + 64000) /100;
		rc = copy_to_user(argp, &freq , sizeof(int));
		if (rc > 0) {
			BCOMFM_ERR("BCOMFM_GET_FREQ fail rc = %x\n",(unsigned int)rc);
			return -EFAULT;
		}
		BCOMFM_INFO( "BCOMFM_GET_FREQ: freq = %d\n", freq);
	}
	break;

	case BCOMFM_GET_RSSI:
	{

		uint8_t rssi = 0;
		bcomfm_read(fm_data.bcomfm_client , I2C_FM_RSSI,  &rssi,  sizeof(rssi));
		BCOMFM_INFO("BCOMFM_GET_RSSI :  rssi = %x\n",rssi);

		rc = copy_to_user(argp, &rssi , sizeof(rssi));
		if (rc > 0) {
			BCOMFM_ERR(" BCOMFM_GET_RSSI fail rc = %x\n",(unsigned int)rc);
			return -EFAULT;
		}
		BCOMFM_INFO("BCOMFM_GET_RSSI:\n");
	}
	break;

	case BCOMFM_RDS_POWER_ON_IO:
	{

		uint8_t fm_power = 0;
		uint8_t data = 0;
		uint8_t flag_status[2]= {0};
		uint8_t int_mask[2] = {0};

		if (0 == atomic_read(&fm_data.b_rds_on))
		{
			bcomfm_read(fm_data.bcomfm_client , I2C_FM_RDS_SYSTEM,  &fm_power,  sizeof(fm_power));
			BCOMFM_INFO("BCOMFM_RDS_POWER_ON_IO fm_power = 0x%x\n", fm_power);
			if( (fm_power & 0x01) != BCOMFM_POWER_PWF){
					fm_power |= BCOMFM_POWER_PWF;
			}
			fm_power |= BCOMFM_POWER_PWR;
			BCOMFM_INFO("BCOMFM_RDS_POWER_ON_IO fm_power = 0x%x\n",fm_power );
			bcomfm_write(fm_data.bcomfm_client, I2C_FM_RDS_SYSTEM, &fm_power, sizeof(fm_power));
			atomic_set(&fm_data.b_rds_on, 1);
			BCOMFM_INFO("BCOMFM_RDS_POWER_ON_IO\n");

			data = RDS_WATER_LEVEL;
			bcomfm_write(fm_data.bcomfm_client, I2C_RDS_WLINE, &data, sizeof(data));
			BCOMFM_INFO("I2C_RDS_WLINE= %d\n", data);

			bcomfm_read(fm_data.bcomfm_client , I2C_FM_RDS_FLAG0,  flag_status,  sizeof(flag_status));

			int_mask[0]=0x00;
			int_mask[1]=BCOMFM_FLAG_RDS_FIFO_WLINE;
			bcomfm_write(fm_data.bcomfm_client, I2C_FM_RDS_MASK0, int_mask, sizeof(int_mask));
			BCOMFM_INFO("I2C_FM_RDS_MASK0 :int_mask[0] = %x, int_mask[1] = %x, atomic_read(&fm_data.irq_fm_wakeup_enable) = %d\n", int_mask[0], int_mask[1], atomic_read(&fm_data.irq_fm_wakeup_enable));
			if (!atomic_read(&fm_data.irq_fm_wakeup_enable))
			{
				atomic_set(&fm_data.irq_fm_wakeup_enable, 1);
				BCOMFM_INFO("Enable FM wake up, irq = %d",fm_data.bcomfm_client->irq );
				local_irq_save(irq_flags);
				enable_irq(fm_data.bcomfm_client->irq);
				local_irq_restore(irq_flags);
			}
		}
		BCOMFM_INFO("GPIO %d value:%d\n",fm_data.gpio_fm_wakeup,gpio_get_value(fm_data.gpio_fm_wakeup));
	}
	break;

	case BCOMFM_RDS_POWER_OFF_IO:
	{
		uint8_t fm_power = 0;

		bcomfm_read(fm_data.bcomfm_client , I2C_FM_RDS_SYSTEM,  &fm_power,  sizeof(fm_power));
		BCOMFM_INFO("BCOMFM_RDS_POWER_OFF_IO fm_power = 0x%x\n", fm_power);
		fm_power = (fm_power & (~BCOMFM_POWER_PWR) );
		BCOMFM_INFO( "BCOMFM_RDS_POWER_OFF_IO fm_power = 0x%x\n", fm_power);
		bcomfm_write(fm_data.bcomfm_client, I2C_FM_RDS_SYSTEM, &fm_power, sizeof(fm_power));
		atomic_set(&fm_data.b_rds_on, 0);

		BCOMFM_INFO( "BCOMFM_RDS_POWER_OFF_IO, atomic_read(&fm_data.irq_fm_wakeup_enable) = %d\n",atomic_read(&fm_data.irq_fm_wakeup_enable) );
		if (atomic_read(&fm_data.irq_fm_wakeup_enable))
		{
			local_irq_save(irq_flags);
			disable_irq_nosync(fm_data.bcomfm_client->irq);
			local_irq_restore(irq_flags);
			atomic_set(&fm_data.irq_fm_wakeup_enable, 0);
			BCOMFM_INFO("Disable FM wake up ");
		}
	}
	break;

	case BCOMFM_RDS_SET_THRED_PID:
	{
		int data = 0;
		rc = copy_from_user(&data, argp , sizeof(data));
		if (rc > 0) {
			BCOMFM_ERR( "BCOMFM_RDS_SET_THRED_PID fail rc = %x\n",(unsigned int)rc);
			return -EFAULT;
		}
		fm_data.rds_signal_pid = data;
		BCOMFM_INFO("BCOMFM_RDS_SET_THRED_PID  rds_user_thread_id = %d\n", fm_data.rds_signal_pid );
	}
	break;

	case BCOMFM_READ_FLAG:
	{
		uint8_t flag_status[2]= {0};
		uint16_t flag = 0;
		BCOMFM_DBG("BCOMFM_READ_FLAG");
		bcomfm_read(fm_data.bcomfm_client , I2C_FM_RDS_FLAG0,  flag_status,  sizeof(flag_status));
		mdelay(10);
		flag = ((flag_status[1] << 8 ) | flag_status[0]);
		rc = copy_to_user(argp, &flag , sizeof(int));
	}
	break;

	case BCOMFM_READ_RDS_DATA:
	{
		uint8_t rds_data[3] = {0};
		bcom_fm_rds_parameters rds_parsed_data;
		uint16_t i = 0;
		BCOMFM_INFO("BCOMFM_READ_RDS_DATA: atomic_read(&fm_data.irq_fm_wakeup_enable) = %d", atomic_read(&fm_data.irq_fm_wakeup_enable));

		memset(&rds_parsed_data, 0, sizeof(bcom_fm_rds_parameters));
		for (i = 0; i <RDS_WATER_LEVEL ; i++)
		{
			bcomfm_read(fm_data.bcomfm_client , I2C_RDS_DATA,  rds_data,  sizeof(rds_data));
			rds_parsed_data.rds_block_id[i] = ( (rds_data[0] & 0xf0)>>4);
			rds_parsed_data.rds_block_errors[i] = ((rds_data[0] & 0x0c)>>2);
			rds_parsed_data.rds_data_block[i] = ((rds_data[1] & 0xff)<<8) | (rds_data[2]& 0xff);
		}
		rc = copy_to_user(argp, &rds_parsed_data , sizeof(bcom_fm_rds_parameters));
		if ( 1 == atomic_read(&fm_data.b_rds_on))
		{
			uint8_t	flag_status[2] = {0};
			uint8_t int_mask[2] = {0};

			if (!atomic_read(&fm_data.irq_fm_wakeup_enable))
			{
				atomic_set(&fm_data.irq_fm_wakeup_enable, 1);
				BCOMFM_INFO("Enable FM wake up ");
				local_irq_save(irq_flags);
				enable_irq(fm_data.bcomfm_client->irq);
				local_irq_restore(irq_flags);

       		}
			bcomfm_read(fm_data.bcomfm_client , I2C_FM_RDS_FLAG0,  flag_status,  sizeof(flag_status));
			BCOMFM_DBG("flag_status[0] = %x, flag_status[1] = %x\n", flag_status[0], flag_status[1]);
			int_mask[0]=0x00;
			int_mask[1]=BCOMFM_FLAG_RDS_FIFO_WLINE;
			bcomfm_write(fm_data.bcomfm_client, I2C_FM_RDS_MASK0, int_mask, sizeof(int_mask));
			BCOMFM_DBG("int_mask[0] = %x, int_mask[1] = %x\n", int_mask[0], int_mask[1]);
		}
		else
		{
			BCOMFM_ERR("BCOMFM_READ_RDS_DATA: Fake FM interrupt");
		}
	}

	break;

	default:
		BCOMFM_ERR("bcomfm_ioctl: default !!!Not find command \n");
		rc = -EFAULT;
		break;
	}

	return rc;
}


static int bcomfm_open(struct inode *inode, struct file *fp)
{
	int32_t rc = 0;

	BCOMFM_INFO(" + bcomfm_open + \n");
	return rc;
}

static int bcomfm_release(struct inode *ip, struct file *fp)
{
	int rc = 0;
	BCOMFM_INFO(" + bcomfm_release + \n");

	return rc;
}

static struct file_operations bcomfm_fops = {
	.owner 	= THIS_MODULE,
	.open 	= bcomfm_open,
	.release = bcomfm_release,
	.ioctl = bcomfm_ioctl,
};

static int bcomfm_remove(struct i2c_client *fm)
{

	free_irq(fm->irq, 0);
	cdev_del(&bcomfm_cdev);
	device_destroy(bcom_class, bcomfm_device_no);
	class_destroy(bcom_class);
	unregister_chrdev_region(bcomfm_device_no, 1);
	return 0;
}

static int bcomfm_probe(struct i2c_client *client,  const struct i2c_device_id *id)
{
	int rc = 0;
	struct device	*class_dev;
	struct bcom_fm_platform_data *plat_data = client->dev.platform_data;

	BCOMFM_INFO(" + bcomfm_probe + \n");

	BCOMFM_INFO(" + bcomfm_probe addr = 0x%x  + irq = %d \n", client->addr, client->irq);
	fm_data.bcomfm_client = client;
	fm_data.gpio_fm_wakeup = plat_data->gpio_fm_wakeup;

	BCOMFM_INFO(" + bcomfm_probe fm_data.bcomfm_client addr  = 0x%x, fm_data.gpio_fm_wakeup = %d  + \n", fm_data.bcomfm_client->addr, fm_data.gpio_fm_wakeup);
	rc = alloc_chrdev_region(&bcomfm_device_no, 0, 1, "bcomfm");

	BCOMFM_INFO("BCOM FM Device Major number:%d\n", MAJOR(bcomfm_device_no));

	bcom_class = class_create(THIS_MODULE, "bcomfm");
	if (IS_ERR(bcom_class)) {
		BCOMFM_ERR("%s: class_create failed \n", __func__);
		goto err_bcomfm_init_error_unregister_device;
	}
	class_dev = device_create(bcom_class, NULL, bcomfm_device_no, NULL, "bcomfm");
	if (IS_ERR(class_dev)) {
		BCOMFM_ERR("%s: class_device_create failed\n", __func__);
		goto err_bcomfm_init_error_destroy_class;
	}

	cdev_init(&bcomfm_cdev, &bcomfm_fops);
	bcomfm_cdev.owner = THIS_MODULE;
	rc = cdev_add(&bcomfm_cdev, bcomfm_device_no, 1);
	if (rc<0) {
		BCOMFM_ERR("%s: cdev_add failed %d\n", __func__, rc);
		goto err_bcomfm_init_error_destroy_device;
	}

	BCOMFM_INFO("GPIO %d, value:%d, irq = %d\n",fm_data.gpio_fm_wakeup,gpio_get_value(fm_data.gpio_fm_wakeup), fm_data.bcomfm_client->irq);

	atomic_set(&fm_data.irq_fm_wakeup_enable, 0);
	BCOMFM_DBG("Disable FM Wake up");

	set_irq_flags(fm_data.bcomfm_client->irq , IRQF_VALID | IRQF_NOAUTOEN);
	rc = request_irq(fm_data.bcomfm_client->irq, bcomfm_irqhandler, IRQF_TRIGGER_LOW, "bcom_fm", &bcomfm_cdev);
	if (rc < 0){
			BCOMFM_ERR("Fail to register FM_WAKEUP");
			goto err_bcomfm_init_error_cleanup_all;
	}
	return 0;
err_bcomfm_init_error_cleanup_all:
	cdev_del(&bcomfm_cdev);
err_bcomfm_init_error_destroy_device:
	BCOMFM_ERR("bcomfm_probe:bcomfm_init_error_cleanup_all \n");
	device_destroy(bcom_class, bcomfm_device_no);
err_bcomfm_init_error_destroy_class:
	BCOMFM_ERR( "bcomfm_probe:bcomfm_init_error_destroy_class \n");
	class_destroy(bcom_class);
err_bcomfm_init_error_unregister_device:
	BCOMFM_ERR("bcomfm_probe:bcomfm_init_error_unregister_device \n");
	unregister_chrdev_region(bcomfm_device_no, 1);

	return rc;
}

static const struct i2c_device_id bcomfm_id[] = {
	{ "bcom_fm", 0 },
	{ }
};

static struct i2c_driver bcomfm_driver = {
	.driver = {
		.owner = THIS_MODULE,
	},
	.probe	 = bcomfm_probe,
	.remove	 = bcomfm_remove,
	.id_table = bcomfm_id,
};

static int __init bcomfm_init(void)
{
	int ret = 0;
	printk(KERN_INFO "BootLog, +%s\n", __func__);
	memset(&fm_data, 0, sizeof(struct bcomfm_data));
	bcomfm_driver.driver.name = bcomfm_name;
	ret = i2c_add_driver(&bcomfm_driver);
	printk(KERN_INFO "BootLog, -%s, ret=%d\n", __func__, ret);
	return ret;
}


static void __exit bcomfm_exit(void)
{

	BCOMFM_DBG("Poweroff, + bcomfm_exit + \n");

	i2c_del_driver(&bcomfm_driver);
}

module_init(bcomfm_init);
module_exit(bcomfm_exit);

