/* drivers/input/touchscreen/auo_touch.c
 *
 * Copyright (c) 2008 QUALCOMM Incorporated.
 * Copyright (c) 2008 QUALCOMM USA, INC.
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/jiffies.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/i2c.h>
#include <linux/semaphore.h>
#include <linux/delay.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <mach/gpio.h>
#include <mach/vreg.h>
#include <mach/auo_touch.h>
#include <mach/pm_log.h>
#include <mach/austin_hwid.h>
#include <mach/../../proc_comm.h>
#include <mach/smem_pc_oem_cmd.h>

//n0p
#include <linux/cpufreq.h>
#include <../../../arch/arm/mach-msm/acpuclock.h>
#include <../../../arch/arm/mach-msm/clock.h>
extern int tps65023_set_dcdc1_level(int vdd);

#define SPEED_DISABLETOUCH_WHEN_PSENSORACTIVE 0

#if SPEED_DISABLETOUCH_WHEN_PSENSORACTIVE
extern atomic_t psensor_approached;
#endif

#define GETTOUCHLOG_AFTER_SCREENOFF 0
#if GETTOUCHLOG_AFTER_SCREENOFF
atomic_t GetTouchLog1AfterLaterResume = ATOMIC_INIT(0);
atomic_t GetTouchLog2AfterLaterResume = ATOMIC_INIT(0);
#endif


#define TS_DRIVER_NAME "auo_touchscreen"
//#define TCH_DBG(fmt, args...) printk(KERN_INFO "AUO_TOUCH: " fmt, ##args)
#define TCH_DBG(fmt, args...) 

static uint printCoord = 0;
module_param(printCoord, uint, 0644);

static uint printDebug = 0;
module_param(printDebug, uint, 0644);

static uint printReport = 0;
module_param(printReport, uint, 0644);

static uint deepfreeze = 1;
module_param(deepfreeze, uint, 0644);

struct ts_t {
    struct i2c_client        *client_4_i2c;
    struct input_dev         *input;
    int                      irq;
    int                      gpio_num;
    int                      gpio_rst;
    int                      open_count;
    struct delayed_work      touch_work;
    struct workqueue_struct  *touch_wqueue;
    struct auo_reg_map_t     reg_map;
    struct auo_mt_t          mtMsg;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend     touch_early_suspend;
#endif
};

#ifdef USE_PIXCIR
struct ts_power_t {
    char     *id;
    unsigned mv;
};
struct ts_power_t const auo_power = {"gp2",  3000};
#endif

static struct ts_t         *g_ts;


#if TOUCH_INDI_INT_MODE
static uint8_t             g_touch_ind_mode = 1;
#else
static uint8_t             g_touch_ind_mode = 0;
#endif
DECLARE_MUTEX( ts_sem );

static uint8_t             g_touch_suspended = 0;
DECLARE_MUTEX( ts_suspend_sem );

static uint8_t             g_pixcir_detected = 0;
static uint8_t             g_pixcir_freeze = 0;


static int __devinit ts_probe(struct i2c_client *,  const struct i2c_device_id *);

static int ts_suspend_ic(struct ts_t *, struct i2c_client *);
static int ts_resume_ic(struct ts_t *, struct i2c_client *);
static ssize_t auo_touch_dump_property(struct device *dev, struct device_attribute *attr, char *buf);

#define TOUCH_RETRY_COUNT 5
static int ts_write_i2c( struct i2c_client *client,
                         uint8_t           regBuf,
                         uint8_t           *dataBuf,
                         uint8_t           dataLen )
{
    int     result = 0;
    uint8_t *buf = NULL;
    int     retryCnt = TOUCH_RETRY_COUNT;

    struct  i2c_msg msgs[] = {
        [0] = {
            .addr   = client->addr,
            .flags  = 0,
            .buf    = (void *)buf,
            .len    = 0
        }
    };

    buf = kzalloc( dataLen+1, GFP_KERNEL );
    if( NULL == buf )
    {
        TCH_DBG("ts_write_i2c: alloc memory failed\n");
        return -EFAULT;
    }

    buf[0] = regBuf;
    memcpy( &buf[1], dataBuf, dataLen );
    msgs[0].buf = buf;
    msgs[0].len = dataLen+1;

    while( retryCnt )
    {
        result = i2c_transfer( client->adapter, msgs, 1 );
        if( result != ARRAY_SIZE(msgs) )
        {
            TCH_DBG("ts_write_i2c: write %Xh %d bytes return failure, %d\n", buf[0], dataLen, result);
            if( -ETIMEDOUT == result ) msleep(10);
            retryCnt--;
        }else {
            result = 0;
            break;
        }
    }

    if( (result == 0) && (retryCnt < TOUCH_RETRY_COUNT) )
        TCH_DBG("ts_write_i2c: write %Xh %d bytes retry at %d\n", buf[0], dataLen, TOUCH_RETRY_COUNT-retryCnt);

    kfree( buf );
    return result;
}

static int ts_read_i2c( struct i2c_client *client,
                        uint8_t           regBuf,
                        uint8_t           *dataBuf,
                        uint8_t           dataLen )
{
    int     result = 0;
    int     retryCnt = TOUCH_RETRY_COUNT;

    struct  i2c_msg msgs[] = {
        [0] = {
            .addr   = client->addr,
            .flags  = 0,
            .buf    = (void *)&regBuf,
            .len    = 1
        },
        [1] = {
            .addr   = client->addr,
            .flags  = I2C_M_RD,
            .buf    = (void *)dataBuf,
            .len    = dataLen
        }
    };

    while( retryCnt )
    {
        result = i2c_transfer( client->adapter, msgs, 2 );
        if( result != ARRAY_SIZE(msgs) )
        {
            TCH_DBG("ts_read_i2c: read %Xh %d bytes return failure, %d\n", regBuf, dataLen, result );
            if( -ETIMEDOUT == result ) msleep(10);
            retryCnt--;
        }else {
            result = 0;
            break;
        }
    }

    if( (result == 0) && (retryCnt < TOUCH_RETRY_COUNT) )
        TCH_DBG("ts_read_i2c: read %Xh %d bytes retry at %d\n", regBuf, dataLen, TOUCH_RETRY_COUNT-retryCnt);

    return result;
}

static void ts_report_coord_via_mt_protocol(struct ts_t *pTS, struct auo_point_info_t *pInfo)
{
	int i;

	for(i=0;i<AUO_REPORT_POINTS;i++)
	{
		if((pInfo+i)->state == RELEASE)
		{
			input_report_abs(pTS->input, ABS_MT_TOUCH_MAJOR, 0);
			input_report_abs(pTS->input, ABS_MT_WIDTH_MAJOR, 0);
			input_report_abs(pTS->input, ABS_MT_POSITION_X, 0);
			input_report_abs(pTS->input, ABS_MT_POSITION_Y, 0);
            if( printReport )
			    TCH_DBG("%s: point[%d] release\n", __func__, i);
		}
		else
		{
			input_report_abs(pTS->input, ABS_MT_TOUCH_MAJOR, 10);
			input_report_abs(pTS->input, ABS_MT_WIDTH_MAJOR, 10);
			input_report_abs(pTS->input, ABS_MT_POSITION_X, (pInfo+i)->coord.x);
			input_report_abs(pTS->input, ABS_MT_POSITION_Y, (pInfo+i)->coord.y);
            if( printReport )
			    TCH_DBG("%s: point[%d] down at (%d,%d)\n",
                           __func__, i, (pInfo+i)->coord.x, (pInfo+i)->coord.y);
		}
		input_mt_sync(pTS->input);
	}
	input_sync(pTS->input);
}

static void ts_fill_mt_info(struct auo_point_info_t *pInfo, struct auo_point_t *pPoint)
{
    int i, x, y;

    for(i=0; i<AUO_REPORT_POINTS; i++)
    {
        x = (pPoint+i)->x;
        y = (pPoint+i)->y;
        if( x > 0 && y > 0 )
        {
            (pInfo+i)->coord.x = x;
            (pInfo+i)->coord.y = y;
            (pInfo+i)->state = PRESS;
        }else {
            (pInfo+i)->coord.x = 0;
            (pInfo+i)->coord.y = 0;
            (pInfo+i)->state = RELEASE;
        }
    }
}

static void ts_get_touch_state( enum auo_mt_state *touch,
                                struct auo_point_t *point )
{
    if( point[0].x == 0 || point[0].y == 0 )
    {
        *touch = NOT_TOUCH_STATE;
        memset( point, 0, sizeof(struct auo_point_t)*AUO_REPORT_POINTS );
    } else if( point[1].x == 0 || point[1].y == 0 ) {
        *touch = ONE_TOUCH_STATE;
        memset( &point[1], 0, sizeof(struct auo_point_t) );
    } else {
        *touch = TWO_TOUCH_STATE;
    }

    return;
}

static void ts_report_coord( struct ts_t *pTS,
                             struct auo_point_t *point )
{
    struct auo_mt_t          *pMsg = &pTS->mtMsg;
    struct auo_point_t       *pre_point;
    enum auo_mt_state        mt_state;
    struct auo_point_info_t  mt_points[AUO_REPORT_POINTS];


    ts_get_touch_state(&mt_state, point);
    ts_fill_mt_info(mt_points, point);

    ts_report_coord_via_mt_protocol(pTS, mt_points);

    pre_point = &pMsg->points[0].coord;
    if( 0 == memcmp( pre_point, &mt_points[0].coord, sizeof(struct auo_point_t)) )
        goto exit_report_coord;

    if( PRESS == mt_points[0].state )
    {
        input_report_abs(pTS->input, ABS_X, mt_points[0].coord.x);
        input_report_abs(pTS->input, ABS_Y, mt_points[0].coord.y);
        input_report_key(pTS->input, BTN_TOUCH, 1);
    } else {
        input_report_key(pTS->input, BTN_TOUCH, 0);
    }
    input_sync(pTS->input);

exit_report_coord:
    memcpy( pMsg->points, mt_points, sizeof(mt_points) );
    pMsg->state = mt_state;

    return;
}

static void ts_irqWorkHandler( struct work_struct *work )
{
    struct ts_t *pTS = container_of(work,
                                    struct ts_t,
                                    touch_work.work);
    struct i2c_client           *client = pTS->client_4_i2c;
    struct auo_point_t          point[AUO_REPORT_POINTS];
    uint8_t                     rawCoord[8];
    uint8_t                     i;
    uint8_t                     is_reported;
    uint8_t is_scheduled = 0;
    
    down( &ts_sem );
    if( g_touch_ind_mode )
    {
        is_scheduled = 1;
        if( 0 == gpio_get_value( pTS->gpio_num ) )
        {
            if( printDebug )
                TCH_DBG("%s: finger up\n", __func__);
            memset( point, 0, sizeof(point) );
            is_reported = 1;
            is_scheduled = 0;
            up( &ts_sem );
            goto report_value;
        }
    }
    up( &ts_sem );

    is_reported = 0;

    if( !ts_read_i2c( client, pTS->reg_map.x1_lsb, rawCoord, 8 ) )
    {
        for( i=0; i<ARRAY_SIZE(point); i++ )
        {
#if USE_AUO_5INCH
            if( 1 == g_pixcir_detected )
            {
#if defined(TOUCH_FB_PORTRAIT)
                point[i].x = rawCoord[4*i+1]<<8 | rawCoord[4*i];
                point[i].y = rawCoord[4*i+3]<<8 | rawCoord[4*i+2];
//hPa
//cm7 streak
                if( 0 != point[i].x ){
                    point[i].x = AUO_X_MAX + 1 - point[i].x;
               }
                if( 0 != point[i].y ){
                    point[i].y = AUO_Y_MAX + 1 - point[i].y;
               }
#else
                point[i].y = rawCoord[4*i+1]<<8 | rawCoord[4*i];
                point[i].x = rawCoord[4*i+3]<<8 | rawCoord[4*i+2];

                if( 0 != point[i].x ){
                    point[i].x = AUO_X_MAX + 1 - point[i].x;
                }
#endif
            }
            else
            {
#if defined(TOUCH_FB_PORTRAIT)
                point[i].x = (rawCoord[2*i+4]<<8) | rawCoord[2*i];
                point[i].y = (rawCoord[2*i+5]<<8) | rawCoord[2*i+1];
                if( point[i].y != 0 )
                    point[i].y = AUO_Y_MAX + 1 - point[i].y;

#else
                point[i].y = (rawCoord[2*i+4]<<8) | rawCoord[2*i];
                point[i].x = (rawCoord[2*i+5]<<8) | rawCoord[2*i+1];
#endif
            }
#else
            point[i].x = (rawCoord[2*i+4]<<8) | rawCoord[2*i];
            point[i].y = (rawCoord[2*i+5]<<8) | rawCoord[2*i+1];
#endif
            if( point[i].x > AUO_X_MAX || point[i].y > AUO_Y_MAX )
            {
                TCH_DBG("ts_irqWorkHandler: (%d,%d) is invalid thus set to zero\n",
                        point[i].x, point[i].y);
                point[i].x = point[i].y = 0;
            }

        }

        if( printCoord )
            TCH_DBG("ts_irqWorkHandler: P1(%d,%d),P2(%d,%d)\n",
                    point[0].x, point[0].y, point[1].x, point[1].y );

#if GETTOUCHLOG_AFTER_SCREENOFF
		if (atomic_read(&GetTouchLog1AfterLaterResume) >0 )
		{
            TCH_DBG("ts_irqWorkHandler: P1(%d,%d),P2(%d,%d)\n",point[0].x, point[0].y, point[1].x, point[1].y );
			atomic_dec(&GetTouchLog1AfterLaterResume);
		}
#endif
#if SPEED_DISABLETOUCH_WHEN_PSENSORACTIVE
        if(unlikely(atomic_read(&psensor_approached)))
        	;
        else
#endif
        is_reported = 1;
    } else {
        TCH_DBG("ts_irqWorkHandler: failed read coordinate\n");
    }

report_value:
    if( is_reported )
    {
        ts_report_coord( pTS, point );
    }

    down( &ts_sem );
    if( is_scheduled && g_touch_ind_mode ) {
        queue_delayed_work( pTS->touch_wqueue, &pTS->touch_work, (TS_PENUP_TIMEOUT_MS * HZ / 1000) );
    } else {
        is_scheduled = 0;
        if( printDebug )
            TCH_DBG("%s: before enable_irq()\n", __func__);
        enable_irq( pTS->irq );
    }
    up( &ts_sem );

    return;
}

static irqreturn_t ts_irqHandler(int irq, void *dev_id)
{
    struct ts_t *ts = dev_id;

    if( printDebug )
        TCH_DBG("%s: before disable_irq()\n", __func__);

#if GETTOUCHLOG_AFTER_SCREENOFF
		if (atomic_read(&GetTouchLog2AfterLaterResume) >0 )
		{
            TCH_DBG("%s: before disable_irq()\n", __func__);
			atomic_dec(&GetTouchLog2AfterLaterResume);
		}
#endif

    disable_irq_nosync( irq );

    queue_delayed_work(ts->touch_wqueue, &ts->touch_work, 0);

    return IRQ_HANDLED;

}

static int ts_release_gpio( int gpio_num , int gpio_rst)
{
    TCH_DBG( "ts_release_gpio: releasing gpio %d\n", gpio_num );
    gpio_free( gpio_num );

    if(system_rev >= EVT2_Band125)
    {
        TCH_DBG( "ts_release_gpio: releasing gpio %d\n", gpio_rst );
        gpio_free( gpio_rst );
    }

    return 0;
}

static int ts_setup_gpio( int gpio_num, int gpio_rst )
{
    int rc;

    TCH_DBG("ts_setup_gpio: setup gpio %d\n", gpio_num);
    rc = gpio_tlmm_config( GPIO_CFG(gpio_num, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),GPIO_CFG_ENABLE );
    if( rc )
    {
        TCH_DBG("ts_setup_gpio: remote config gpio %d failed (rc=%d)\n", gpio_num, rc);
	goto exit_setup_gpio;
    }

    rc = gpio_request( gpio_num, "auo_touchscreen_irq" );
    if ( rc )
    {
        TCH_DBG("ts_setup_gpio: request gpio %d failed (rc=%d)\n", gpio_num, rc);
	goto exit_setup_gpio;
    }

    rc = gpio_direction_input( gpio_num );
    if ( rc )
    {
        TCH_DBG("ts_setup_gpio: set gpio %d mode failed (rc=%d)\n", gpio_num, rc);
        goto exit_setup_gpio;
    }

    if(system_rev >= EVT2_Band125)
    {
        TCH_DBG("ts_setup_gpio: setup gpio %d\n", gpio_rst);
        rc = gpio_tlmm_config( GPIO_CFG(gpio_rst, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA),
                                        GPIO_CFG_ENABLE );
        if( rc )
        {
            TCH_DBG("ts_setup_gpio: remote config gpio %d failed (rc=%d)\n", gpio_rst, rc);
	    goto exit_setup_gpio;
        }

        rc = gpio_request( gpio_rst, "auo_touchscreen_rst" );
        if ( rc )
        {
            TCH_DBG("ts_setup_gpio: request gpio %d failed (rc=%d)\n", gpio_rst, rc);
	    goto exit_setup_gpio;
        }

        rc = gpio_direction_output( gpio_rst, 0 );
        if ( rc )
        {
            TCH_DBG("ts_setup_gpio: set gpio %d mode failed (rc=%d)\n", gpio_rst, rc);
            goto exit_setup_gpio;
        }
    }

    return rc;

exit_setup_gpio:
    ts_release_gpio( gpio_num, gpio_rst );
    return rc;
}

static int ts_config_gpio( int gpio_num, int gpio_rst )
{
    int rc=0;

    if(system_rev >= EVT2_Band125)
    {
        gpio_set_value(gpio_rst, 1);
        TCH_DBG("ts_config_gpio: sleep 60 ms\n");
        msleep(60);
    }

    return rc;
}

static int ts_config_panel(struct ts_t *pTS)
{
#if CONFIG_PANEL
    int result;
    struct i2c_client *client = pTS->client_4_i2c;
    uint8_t value;

#if 0
    if( (system_rev == EVT2P2_Band125) || (system_rev == EVT2P2_Band18) )
    {
        TCH_DBG("ts_config_panel: not config EVT2-2 panel\n");
        return 0;
    }
#endif


    result = ts_read_i2c(client, pTS->reg_map.int_setting, &value, 1);
    if( result )
    {
        TCH_DBG("ts_config_panel: unable to read reg %Xh, return %d\n",
                 pTS->reg_map.int_setting, result);
        return result;
    }
    TCH_DBG("ts_config_panel: read %Xh = 0x%X\n", pTS->reg_map.int_setting, value);

    if( (value & AUO_INT_SETTING_MASK) == AUO_INT_SETTING_VAL )
    {
        goto config_sens;
    }

    value &= (~AUO_INT_SETTING_MASK);
    value |= AUO_INT_SETTING_VAL;
    TCH_DBG("ts_config_panel: write %Xh = 0x%X\n", pTS->reg_map.int_setting, value);

    result = ts_write_i2c( client, pTS->reg_map.int_setting, &value, 1 );
    if( result )
    {
        TCH_DBG("ts_config_panel: unable to write reg %Xh, return %d\n",
                 pTS->reg_map.int_setting, result);
        return result;
    }

config_sens:

    down( &ts_sem );
    if( (AUO_INT_SETTING_VAL & AUO_INT_MODE_MASK) == (AUO_TOUCH_IND_ENA & AUO_INT_MODE_MASK) )
        g_touch_ind_mode = 1;
    else
        g_touch_ind_mode = 0;
    up( &ts_sem );

#endif
    return 0;
}

static void ts_reset_panel( struct ts_t *pTS)
{
#ifdef USE_PIXCIR
    TCH_DBG("ts_reset_panel()\n");

    gpio_set_value(pTS->gpio_rst, 0);
    msleep(1);
    gpio_set_value(pTS->gpio_rst, 1);
	msleep(60);
#endif
    return;
}

static int ts_detect_pixcir(struct ts_t *pTS)
{
#ifdef USE_PIXCIR
    int result;
    uint8_t value;

    value = 0;
    result = ts_read_i2c(pTS->client_4_i2c, 0x70, &value, 1);
	if( result )
	   return result;
	else
	   g_pixcir_detected = (!!value)? 1: 0;

    value = 0;
	result = ts_read_i2c(pTS->client_4_i2c, 0x77, &value, 1);
	if( result )
        return result;
	else {
        TCH_DBG("Firmware Version is 0x%X\n", value);
        g_pixcir_freeze = (value > 0x97)? 1: 0;
	}
#endif
    return 0;
}

static int ts_setup_reg_map(struct ts_t *pTS)
{
    TCH_DBG("ts_setup_reg_map->\n");
    if( NULL == pTS )
    {
        return -1;
    }

    if( 0 == g_pixcir_detected )
    {
        pTS->reg_map.x1_lsb = 0x40;
        pTS->reg_map.x_sensitivity = 0x65;
        pTS->reg_map.y_sensitivity = 0x64;
        pTS->reg_map.int_setting = 0x66;
        pTS->reg_map.int_width = 0x67;
        pTS->reg_map.power_mode = 0x69;
    }
    else
    {
        pTS->reg_map.x1_lsb = AUO_X1_LSB;
        pTS->reg_map.x_sensitivity = AUO_X_SENSITIVITY;
        pTS->reg_map.y_sensitivity = AUO_Y_SENSITIVITY;
        pTS->reg_map.int_setting = AUO_INT_SETTING;
        pTS->reg_map.int_width = AUO_INT_WIDTH;
        pTS->reg_map.power_mode = AUO_POWER_MODE;
    }

    TCH_DBG("ts_setup_reg_map: map is 0x%X 0x%X 0x%X 0x%X 0x%X 0x%X\n",
            pTS->reg_map.x1_lsb, pTS->reg_map.x_sensitivity, pTS->reg_map.y_sensitivity,
            pTS->reg_map.int_setting, pTS->reg_map.int_width, pTS->reg_map.power_mode);

    TCH_DBG("ts_setup_reg_map<-\n");

    return 0;
}

#ifdef USE_PIXCIR
static int ts_read_eeprom_calib(char *buf_x, char *buf_y)
{
    int rc = 0;
    char addr[4] = { 0x78, 0x01, 0x00, 0x00 };
    struct i2c_client *client = g_ts->client_4_i2c;

    struct i2c_msg msgs[] = {
    	[0] = {
                .addr   = client->addr,
    		.flags	= 0,
    		.buf	= (void *)addr,
    		.len	= 4
    	},
        [1] = {
                .addr   = client->addr,
    		.flags	= I2C_M_RD,
    	}
    };

    TCH_DBG("ts_read_eeprom_calib->\n");
    addr[2] = AUO_EEPROM_CALIB_X;
    msgs[1].buf = (void *)buf_x;
    msgs[1].len = AUO_EEPROM_CALIB_X_LEN;
    rc = i2c_transfer( client->adapter, msgs, 2 );
    if( rc != 2)
    {
        TCH_DBG("ts_read_eeprom_calib: unable to read reg %Xh %Xh %Xh %Xh, return %d\n",
                 msgs[0].buf[0], msgs[0].buf[1], msgs[0].buf[2], msgs[0].buf[3], rc);
        return rc;
    }

    addr[2] = AUO_EEPROM_CALIB_Y;
    msgs[1].buf = (void *)buf_y;
    msgs[1].len = AUO_EEPROM_CALIB_Y_LEN;
    rc = i2c_transfer( client->adapter, msgs, 2 );
    if( rc != 2)
    {
        TCH_DBG("ts_read_eeprom_calib: unable to read reg %Xh %Xh %Xh %Xh, return %d\n",
                 msgs[0].buf[0], msgs[0].buf[1], msgs[0].buf[2], msgs[0].buf[3], rc);
        return rc;
    }

    TCH_DBG("ts_read_eeprom_calib<-\n");

    return 0;
}

static int ts_power_switch(int on)
{
    struct  vreg *vreg_auo;
    int     rc = 0;

    TCH_DBG("ts_power_switch(%d)->\n", on);

    vreg_auo = vreg_get( 0, auo_power.id );

    if(on)
    {
        rc = vreg_set_level( vreg_auo, auo_power.mv );
        if( rc < 0 )
        {
            TCH_DBG("ts_power_switch_on: failed to set %s to level %dmv\n",
                  auo_power.id, auo_power.mv);
            goto exit_power_switch;
        }
        rc = vreg_enable( vreg_auo );
        if( rc < 0 )
        {
            TCH_DBG("ts_power_switch_on: failed to enable %s to %dmv\n",
                  auo_power.id, auo_power.mv);
            goto exit_power_switch;
        }

        PM_LOG_EVENT(PM_LOG_ON, PM_LOG_TOUCH);

        msleep(1);
    }else {
        if( vreg_disable( vreg_auo ) < 0 )
        {
            TCH_DBG("ts_power_switch_off: failed to disable %s\n",
                  auo_power.id);
            rc = -1;
            goto exit_power_switch;
        }
        PM_LOG_EVENT(PM_LOG_OFF, PM_LOG_TOUCH);
    }

exit_power_switch:

    TCH_DBG("ts_power_switch<-\n");

    return rc;
}

static int ts_get_eeprom_calib(struct auo_eeprom_calib_t *pCalib)
{
    int rc = 0;
    char *buf_x, *buf_y;

    TCH_DBG("ts_get_eeprom_calib->\n");

    if( NULL == pCalib->raw_buf_x || NULL == pCalib->raw_buf_y )
    {
        TCH_DBG("ts_get_eeprom_calib: invalid args\n");
        return -1;
    }
    if( pCalib->raw_len_x < 0 || pCalib->raw_len_x > AUO_EEPROM_CALIB_X_LEN ||
        pCalib->raw_len_y < 0 || pCalib->raw_len_y > AUO_EEPROM_CALIB_Y_LEN )
    {
        TCH_DBG("ts_get_eeprom_calib: invalid length %d %d\n",
                 pCalib->raw_len_x, pCalib->raw_len_y);
        return -1;
    }

    buf_x = buf_y = NULL;
    buf_x = kzalloc( pCalib->raw_len_x, GFP_KERNEL );
    buf_y = kzalloc( pCalib->raw_len_y, GFP_KERNEL );
    if( NULL == buf_x || NULL == buf_y )
    {
        TCH_DBG("ts_get_eeprom_calib: alloc failed\n");
        rc = -1;
        goto exit_get_calib;
    }

    rc = ts_read_eeprom_calib(buf_x, buf_y);

    if( rc )
        goto exit_get_calib;

    if ( copy_to_user((void *)pCalib->raw_buf_x,
    	              (void *)buf_x,
    	              pCalib->raw_len_x) )
    {
        TCH_DBG("ts_get_eeprom_calib: copy to user failed!\n");
        rc = -EFAULT;
    }

    if ( copy_to_user((void *)pCalib->raw_buf_y,
    	              (void *)buf_y,
    	              pCalib->raw_len_y) )
    {
        TCH_DBG("ts_get_eeprom_calib: copy to user failed!\n");
        rc = -EFAULT;
    }

exit_get_calib:
    if( buf_x )
        kfree(buf_x);
    if( buf_y )
        kfree(buf_y);

    TCH_DBG("ts_get_eeprom_calib<-\n");

    return rc;
}

static int ts_get_raw_data( struct auo_raw_data_t *pRawData )
{
    int rc;
    struct i2c_client *client = g_ts->client_4_i2c;
    uint8_t *rawBuf = NULL;
    int     *buf = NULL;
    int     i;

    if( NULL == pRawData->raw_buf_x || NULL == pRawData->raw_buf_y )
    {
        TCH_DBG("ts_get_raw_data: invalid arg\n");
        rc = -1;
        goto exit_raw_data;
    }

    if( pRawData->raw_len_x < AUO_RAW_DATA_X_LEN ||
        pRawData->raw_len_y < AUO_RAW_DATA_Y_LEN )
    {
        TCH_DBG("ts_get_raw_data: invalid buffer length %d %d\n",
                 pRawData->raw_len_x, pRawData->raw_len_y);
        rc = -1;
        goto exit_raw_data;
    }

    rawBuf = kzalloc( (AUO_RAW_DATA_X_LEN+AUO_RAW_DATA_Y_LEN)*2, GFP_KERNEL );
    buf = kzalloc( (AUO_RAW_DATA_X_LEN+AUO_RAW_DATA_Y_LEN)*sizeof(int), GFP_KERNEL );
    if( NULL == rawBuf || NULL == buf )
    {
        TCH_DBG("ts_get_raw_data: alloc failed\n");
        rc = -1;
        goto exit_raw_data;
    }

    if( pRawData->mode != KEEP_READ )
    {
        uint8_t enableAddr, enableData;

        enableAddr = AUO_TOUCH_STRENGTH_ENA;
        if( pRawData->mode == ENABLE_READ )
            enableData = 1;
        else
            enableData = 0;

        rc = ts_write_i2c( client, enableAddr, &enableData, 1 );
        if( rc )
            goto exit_raw_data;
    }

    rc = ts_read_i2c( client, AUO_RAW_DATA_X, rawBuf,
                        (AUO_RAW_DATA_X_LEN+AUO_RAW_DATA_Y_LEN)*2);
    if( rc )
        goto exit_raw_data;

    for(i=0; i<(AUO_RAW_DATA_X_LEN+AUO_RAW_DATA_Y_LEN); i++)
    {
        *(buf+i) = *(rawBuf+2*i+1);
        *(buf+i) = (*(buf+i)) << 8;
        *(buf+i) |= *(rawBuf+2*i);
    }

    if ( copy_to_user((void *)pRawData->raw_buf_x,
    	              (void *)buf,
    	              AUO_RAW_DATA_X_LEN * sizeof(int)) )
    {
        TCH_DBG("ts_get_raw_data: copy to user failed!\n");
        rc = -EFAULT;
    }

    if ( copy_to_user((void *)pRawData->raw_buf_y,
    	              (void *)(buf+AUO_RAW_DATA_X_LEN),
    	              AUO_RAW_DATA_Y_LEN * sizeof(int)) )
    {
        TCH_DBG("ts_get_raw_data: copy to user failed!\n");
        rc = -EFAULT;
    }

exit_raw_data:
    if( rawBuf )
        kfree(rawBuf);
    if( buf )
        kfree(buf);

    return rc;
}

#endif

static int ts_open(struct input_dev *dev)
{
    int rc = 0;
    struct ts_t *pTS = input_get_drvdata(dev);

    TCH_DBG("ts_open()\n");

    if( 0 == pTS->open_count )
    {
        rc = request_irq( pTS->irq, ts_irqHandler, IRQF_TRIGGER_RISING,
                          "touchscreen", pTS );

        if ( rc )
            TCH_DBG("ts_open: irq %d requested failed\n", pTS->irq);
        else
            TCH_DBG("ts_open: irq %d requested successfully\n", pTS->irq);
    }else {
        TCH_DBG("ts_open: opened %d times previously\n", pTS->open_count);
    }

    if( !rc )
        pTS->open_count++;

    return rc;
}

static void ts_close(struct input_dev *dev)
{
    struct ts_t *pTS = input_get_drvdata(dev);

    if( pTS->open_count > 0 )
    {
        pTS->open_count--;
        TCH_DBG( "ts_close: still opened %d times\n", pTS->open_count );
        if( 0 == pTS->open_count ) {
            TCH_DBG( "ts_close: irq %d will be freed\n", pTS->irq );
            free_irq( pTS->irq, pTS );
        }
    }

    return;
}

static int ts_event(struct input_dev *dev, unsigned int type, unsigned int code, int value)
{
    TCH_DBG("ts_event()\n");

    return 0;
}

static int ts_register_input( struct input_dev **input,
                              struct auo_touchscreen_platform_data *pdata,
                              struct i2c_client *client )
{
    int rc;
    struct input_dev *input_dev;
    unsigned int x_max, y_max;

    input_dev = input_allocate_device();
    if ( !input_dev ) {
        rc = -ENOMEM;
        return rc;
    }


    input_dev->name = TS_DRIVER_NAME;
    input_dev->phys = "auo_touchscreen/input0";
    input_dev->id.bustype = BUS_I2C;
    input_dev->id.vendor = 0x0001;
    input_dev->id.product = 0x0002;
    input_dev->id.version = 0x0100;
    input_dev->dev.parent = &client->dev;

    input_dev->open = ts_open;
    input_dev->close = ts_close;
    input_dev->event = ts_event;

    input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
    if (pdata) {
        x_max = pdata->x_max ? : AUO_X_MAX;
        y_max = pdata->y_max ? : AUO_Y_MAX;
    } else {
        x_max = AUO_X_MAX;
        y_max = AUO_Y_MAX;
    }

	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, AUO_X_MAX, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, AUO_Y_MAX, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR,  0, AUO_X_MAX, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_WIDTH_MAJOR,  0, AUO_X_MAX, 0, 0);

    rc = input_register_device( input_dev );

    if ( rc )
    {
        TCH_DBG("ts_probe: failed to register input device\n");
        input_free_device( input_dev );
    }else {
        *input = input_dev;
    }

    return rc;
}

#if USE_AUO_5INCH
static long ts_misc_ioctl ( struct file *fp,
                            unsigned int cmd,
                            unsigned long arg)
{
    struct i2c_client *client = g_ts->client_4_i2c;
    struct auo_sensitivity_t sensitivity;
    struct auo_interrupt_t   interrupt;
    struct auo_power_mode_t  power_mode;
#ifdef USE_PIXCIR
    struct auo_touch_area_t touch_area;
    struct auo_touch_strength_t touch_strength;
#endif
    uint8_t value = 0;
    uint8_t addr = 0;
    uint8_t *pData = NULL;
    uint    length = 0;
    int     rc = 0;

    TCH_DBG("ts_misc_ioctl()+, cmd number=%d\n", _IOC_NR(cmd) );

    switch(cmd)
    {
        case AUO_TOUCH_IOCTL_GET_X_SENSITIVITY:
            addr = g_ts->reg_map.x_sensitivity;
            pData = (void *)&sensitivity;
            length = sizeof(sensitivity);
            rc = ts_read_i2c( client, addr, &value, 1 );
            sensitivity.value = value;
            break;
        case AUO_TOUCH_IOCTL_SET_X_SENSITIVITY:
            addr = g_ts->reg_map.x_sensitivity;
            pData = (void *)&sensitivity;
            length = sizeof(sensitivity);
            if( copy_from_user( (void *)pData,
                                (void *)arg,
                                length) )
            {
                TCH_DBG("ts_misc_ioctl: copy from user failed!\n");
                rc = -EFAULT;
            }else {
                value = sensitivity.value;
            }
            break;
        case AUO_TOUCH_IOCTL_SET_Y_SENSITIVITY:
            addr = g_ts->reg_map.y_sensitivity;
            pData = (void *)&sensitivity;
            length = sizeof(sensitivity);
            if( copy_from_user( (void *)pData,
                                (void *)arg,
                                length) )
            {
                TCH_DBG("ts_misc_ioctl: copy from user failed!\n");
                rc = -EFAULT;
            }else {
                value = sensitivity.value;
            }
            break;
        case AUO_TOUCH_IOCTL_GET_Y_SENSITIVITY:
            addr = g_ts->reg_map.y_sensitivity;
            pData = (void *)&sensitivity;
            length = sizeof(sensitivity);
            rc = ts_read_i2c( client, addr, &value, 1 );
            sensitivity.value = value;
            break;
        case AUO_TOUCH_IOCTL_SET_INT_SETTING:
            addr = g_ts->reg_map.int_setting;
            pData = (void *)&interrupt;
            length = sizeof(interrupt);
            if( copy_from_user( (void *)pData,
                                (void *)arg,
                                length) )
            {
                TCH_DBG("ts_misc_ioctl: copy from user failed!\n");
                rc = -EFAULT;
            }else {
                value = interrupt.int_setting;
            }
            break;
        case AUO_TOUCH_IOCTL_GET_INT_SETTING:
            addr = g_ts->reg_map.int_setting;
            pData = (void *)&interrupt;
            length = sizeof(interrupt);
            rc = ts_read_i2c( client, addr, &value, 1 );
            interrupt.int_setting = value;
            break;
        case AUO_TOUCH_IOCTL_SET_INT_WIDTH:
            addr = g_ts->reg_map.int_width;
            pData = (void *)&interrupt;
            length = sizeof(interrupt);
            if( copy_from_user( (void *)pData,
                                (void *)arg,
                                length) )
            {
                TCH_DBG("ts_misc_ioctl: copy from user failed!\n");
                rc = -EFAULT;
            }else {
                value = interrupt.int_width;
            }
            break;
        case AUO_TOUCH_IOCTL_GET_INT_WIDTH:
            addr = g_ts->reg_map.int_width;
            pData = (void *)&interrupt;
            length = sizeof(interrupt);
            rc = ts_read_i2c( client, addr, &value, 1 );
            interrupt.int_width = value;
            break;
        case AUO_TOUCH_IOCTL_SET_POWER_MODE:
            addr = g_ts->reg_map.power_mode;
            pData = (void *)&power_mode;
            length = sizeof(power_mode);
            if( copy_from_user( (void *)pData,
                                (void *)arg,
                                length) )
            {
                TCH_DBG("ts_misc_ioctl: copy from user failed!\n");
                rc = -EFAULT;
                break;
            }
            if( power_mode.mode <= UNKNOW_POWER_MODE ||
                power_mode.mode >= MAX_POWER_MODE )
            {
                TCH_DBG("ts_misc_ioctl: unknown powr mode %d!\n", power_mode.mode);
                rc = -EFAULT;
                break;
            }
            rc = ts_read_i2c( client, addr, &value, 1 );
            if( !rc )
            {
                power_mode.mode -= ACTIVE_POWER_MODE;
                power_mode.mode &= AUO_POWER_MODE_MASK;
                value &= ~AUO_POWER_MODE_MASK;
                value |= power_mode.mode;
            }
            break;
        case AUO_TOUCH_IOCTL_GET_POWER_MODE:
            addr = g_ts->reg_map.power_mode;
            pData = (void *)&power_mode;
            length = sizeof(power_mode);
            rc = ts_read_i2c( client, addr, &value, 1 );
            value &= AUO_POWER_MODE_MASK;
            value += ACTIVE_POWER_MODE;
            power_mode.mode = (enum auo_touch_power_mode)value;
            break;
#ifdef USE_PIXCIR
        case AUO_TOUCH_IOCTL_START_CALIB:
        {
            addr = 0x78;
            value = 0x03;
            break;
        }
        case AUO_TOUCH_IOCTL_GET_EEPROM_CALIB:
        {
            struct auo_eeprom_calib_t eeprom_calib;
            if( copy_from_user( (void *)&eeprom_calib,
                                (void *)arg,
                                sizeof(eeprom_calib) ) )
            {
                rc = -EFAULT;
                TCH_DBG("ts_misc_ioctl: copy from user failed!\n");
                break;
            }
            rc = ts_get_eeprom_calib(&eeprom_calib);
            break;
        }
        case AUO_TOUCH_IOCTL_POWER_SWITCH:
        {
            struct auo_power_switch_t power;
            if( copy_from_user( (void *)&power,
                                (void *)arg,
                                sizeof(power) ) )
            {
                rc = -EFAULT;
                TCH_DBG("ts_misc_ioctl: copy from user failed!\n");
                goto exit_ioctl;
            }

            if( power.on ) {
                printk("DSC: Voodoo TS re-resume in misc ioctl.\n");
		rc = ts_resume_ic(g_ts, client);
		usleep(100);
                rc = ts_suspend_ic(g_ts, client);
		usleep(100);
                rc = ts_resume_ic(g_ts, client);

/*
#ifdef CONFIG_DSC_KICKCPU
                printk ("DSC: kick CPU on touchscreen resume in misc ioctl.\n");
		tps65023_set_dcdc1_level(1300);
		//n0p - 300 nanoseconds for TPS chip 
		usleep(300);
                acpuclk_set_rate(0, 998400, SETRATE_CPUFREQ);
		usleep(300);
		cpufreq_update_policy(0);
#endif
*/

            } else
                rc = ts_suspend_ic(g_ts, client);

            break;
        }
        case AUO_TOUCH_IOCTL_GET_RAW_DATA:
        {
            struct auo_raw_data_t raw_data;
            if( copy_from_user( (void *)&raw_data,
                                (void *)arg,
                                sizeof(raw_data)) )
            {
                rc = -EFAULT;
                TCH_DBG("ts_misc_ioctl: copy from user failed!\n");
                goto exit_ioctl;
            }
            rc = ts_get_raw_data( &raw_data );
            break;
        }
        case AUO_TOUCH_IOCTL_RESET_PANEL:
        {
            if(system_rev >= EVT2_Band125)
            {
                ts_reset_panel(g_ts);
                rc = ts_config_panel(g_ts);
            }
            break;
        }
        case AUO_TOUCH_IOCTL_GET_TOUCH_AREA:
        {
            uint8_t area[2] = {0};

            addr = AUO_TOUCH_AREA_X;
            pData = (void *)&touch_area;
            length = sizeof(touch_area);
            rc = ts_read_i2c( client, addr, &area[0], 2 );
            touch_area.area_x = area[0];
            touch_area.area_y = area[1];
            break;
        }
        case AUO_TOUCH_IOCTL_GET_TOUCH_STRENGTH:
        {
            uint8_t enableAddr, enableData;
            uint8_t strength[4] = {0};

            if( copy_from_user( (void *)&touch_strength,
                                (void *)arg,
                                sizeof(touch_strength) ) )
            {
                rc = -EFAULT;
                TCH_DBG("ts_misc_ioctl: copy from user failed!\n");
                goto exit_ioctl;
            }

            if( touch_strength.mode == ENABLE_READ )
            {
                enableAddr = AUO_TOUCH_STRENGTH_ENA;
                enableData = 1;
                rc = ts_write_i2c( client, enableAddr, &enableData, 1);
                if( rc )
                    break;
            }

            addr = AUO_TOUCH_STRENGTH_X;
            pData = (void *)&touch_strength;
            length = sizeof(touch_strength);
            rc = ts_read_i2c( client, addr, strength, 4 );
            if( rc )
                break;

            touch_strength.strength_x = touch_strength.strength_y = 0;
            touch_strength.strength_y = (strength[1] << 8) | strength[0];
            touch_strength.strength_x = (strength[3] << 8) | strength[2];
            if( strength[1] & 0x80 )
                touch_strength.strength_y |= (~0xFFFF);
            if( strength[3] & 0x80 )
                touch_strength.strength_x |= (~0xFFFF);

            TCH_DBG("ts_misc_ioctl: strength_x=%d strength_y=%d\n",
                           touch_strength.strength_x, touch_strength.strength_y);

            if(  touch_strength.mode == DISABLE_READ )
            {
                enableAddr = AUO_TOUCH_STRENGTH_ENA;
                enableData = 0;
                rc = ts_write_i2c( client, enableAddr, &enableData, 1);
            }
            break;
        }
#endif
        default:
            TCH_DBG("ts_misc_ioctl: unsupported ioctl cmd 0x%X\n", cmd);
            rc = -EFAULT;
            break;
    }

    if( rc )
        goto exit_ioctl;

#ifdef USE_PIXCIR
    if( AUO_TOUCH_IOCTL_POWER_SWITCH == cmd ||
        AUO_TOUCH_IOCTL_GET_EEPROM_CALIB == cmd ||
        AUO_TOUCH_IOCTL_GET_RAW_DATA == cmd ||
        AUO_TOUCH_IOCTL_RESET_PANEL == cmd )
        goto exit_ioctl;
#endif

    if( _IOC_READ == _IOC_DIR(cmd))
    {
        if( !rc )
            if ( copy_to_user((void *)arg,
            	              (void *)pData,
            	              length) )
            {
                TCH_DBG("ts_misc_ioctl: copy to user failed!\n");
                rc = -EFAULT;
            }
    }else {
        if( g_ts->reg_map.int_setting == addr )
            down( &ts_sem );

        rc = ts_write_i2c( client, addr, &value, 1);
        if( !rc )
        {
            if( g_ts->reg_map.int_setting == addr )
            {
                if( (value & AUO_TOUCH_IND_ENA) == AUO_TOUCH_IND_ENA )
                    g_touch_ind_mode = 1;
                else
                    g_touch_ind_mode = 0;
                TCH_DBG("%s: touch indicate mode? %s\n", __func__, (g_touch_ind_mode? "YES": "NO"));
            }
        }

        if( g_ts->reg_map.int_setting == addr )
            up( &ts_sem );
    }

exit_ioctl:
    TCH_DBG("ts_misc_ioctl()-, rc=%d\n", rc );
    return rc;
}
#endif


static int ts_misc_release(struct inode *inode, struct file *fp)
{
    TCH_DBG("ts_misc_release()\n");

    return 0;
}

static int ts_misc_open(struct inode *inode, struct file *fp)
{
    TCH_DBG("ts_misc_open()\n");

    return 0;
}

static struct file_operations ts_misc_fops = {
	.owner 	= THIS_MODULE,
	.open 	= ts_misc_open,
	.release = ts_misc_release,
#if USE_AUO_5INCH
        .unlocked_ioctl = ts_misc_ioctl,
#endif
};

static struct miscdevice ts_misc_device = {
	.minor 	= MISC_DYNAMIC_MINOR,
	.name 	= "auo_misc_touch",
	.fops 	= &ts_misc_fops,
};


static int ts_remove(struct i2c_client *client)
{
    struct ts_t *ts = (struct ts_t *)i2c_get_clientdata( client );

    TCH_DBG("ts_remove()\n");

    cancel_rearming_delayed_workqueue(ts->touch_wqueue, &ts->touch_work);
    destroy_workqueue(ts->touch_wqueue);
    misc_deregister( &ts_misc_device );

    input_unregister_device(ts->input);
    input_free_device(ts->input);
    ts->input = NULL;

    ts_release_gpio( ts->gpio_num, ts->gpio_rst );
    i2c_set_clientdata( client, NULL );

    g_ts = NULL;
    kfree( ts );

    return 0;
}

static int ts_suspend_ic(struct ts_t *ts, struct i2c_client *client)
{
    int rc = 0;
    int ret;

    down( &ts_suspend_sem );

    if( g_touch_suspended )
        goto exit_suspend;

    disable_irq( ts->irq );
    TCH_DBG( "%s: disable irq %d\n", __func__, ts->irq);
    g_touch_suspended = 1;

    ret = cancel_work_sync(&ts->touch_work.work);
    if (ret)
        enable_irq(ts->irq);

if (deepfreeze) {
    if(system_rev >= EVT2P2_Band125)
    {
        uint8_t addr, data;
        addr = ts->reg_map.power_mode;
		rc = ts_read_i2c( client, addr, &data, 1);
		if( rc ) goto exit_suspend;

		data &= (~AUO_POWER_MODE_MASK);
        if( g_pixcir_freeze )
		{
            data |= (REAL_SLEEP_POWER_MODE - ACTIVE_POWER_MODE);
            TCH_DBG("%s: enter freeze mode, write %xh=%x\n", __func__, addr, data);
		}else {
            data |= (DEEP_SLEEP_POWER_MODE - ACTIVE_POWER_MODE);
            TCH_DBG("%s: enter deep sleep mode, write %xh=%x\n", __func__, addr, data);
		}
        rc = ts_write_i2c( client, addr, &data, 1);
    }
}

exit_suspend:
    if( rc ) TCH_DBG( "%s: touch suspend failed\n", __func__);
    up( &ts_suspend_sem );

    return rc;
}

static int ts_resume_ic(struct ts_t *ts, struct i2c_client *client)
{
    int rc = 0;

    down( &ts_suspend_sem );

    if( 0 == g_touch_suspended )
        goto exit_resume;

    printk("DSC: TS resume - start.\n");
//    ts_reset_panel(ts);
//n0p
if (deepfreeze) {
    if(system_rev >= EVT2P2_Band125)
    {
	    if( g_pixcir_freeze )
		{
            ts_reset_panel(ts);
		}else {
            uint8_t addr, data;
		    addr = ts->reg_map.power_mode;
            rc = ts_read_i2c( client, addr, &data, 1);
		    if( rc ) goto exit_resume;

		    data &= (~AUO_POWER_MODE_MASK);
		    data |= (ACTIVE_POWER_MODE - ACTIVE_POWER_MODE);
                    TCH_DBG("ts_resume_ic: leave deep sleep mode, write %xh=%x\n", addr, data);
		    rc = ts_write_i2c( client, addr, &data, 1);

   	            usleep(100);

		    printk("DSC: Voodoo - re-resume after deepfreeze.\n");
		    //printk("DSC: Voodoo - 1 - sleep");
		    data &= (~AUO_POWER_MODE_MASK);
                    data |= (DEEP_SLEEP_POWER_MODE - ACTIVE_POWER_MODE);

		    usleep(100);

                    //printk("DSC: Voodoo - 2 - resume");
		    data &= (~AUO_POWER_MODE_MASK);
                    data |= (ACTIVE_POWER_MODE - ACTIVE_POWER_MODE);
                    rc = ts_write_i2c( client, addr, &data, 1);
/*
#ifdef CONFIG_DSC_KICKCPU
                printk("DSC: kick CPU on touchscreen resume after deepfreeze.\n");
                tps65023_set_dcdc1_level(1300);
                //n0p - 300 nanoseconds for TPS chip
                usleep(300);
                acpuclk_set_rate(0, 998400, SETRATE_CPUFREQ);
                usleep(300);
                cpufreq_update_policy(0);
#endif
*/
		    if( rc ) goto exit_resume;
		}

        rc = ts_config_panel(ts);
        if( rc ) goto exit_resume;
    }
}

    if( !rc )
    {
        TCH_DBG( "ts_resume_ic: enable irq %d\n", ts->irq);
        enable_irq( ts->irq );
        g_touch_suspended = 0;
    }

exit_resume:
	if( rc ) TCH_DBG( "ts_resume_ic: touch resume failed\n");
    up( &ts_suspend_sem );

    return rc;
}


#ifdef CONFIG_HAS_EARLYSUSPEND
static void ts_suspend(struct early_suspend *h)
#else
static int ts_suspend(struct i2c_client *client, pm_message_t mseg)
#endif
{
    int rc = 0;
#ifdef CONFIG_HAS_EARLYSUSPEND
    struct ts_t *ts = container_of( h,
                                     struct ts_t,
                                     touch_early_suspend);
    struct i2c_client *client = ts->client_4_i2c;
#else
    struct ts_t *ts = (struct ts_t *)i2c_get_clientdata( client );
#endif
    unsigned id = SMEM_PC_OEM_PM_GP2_LPM_ON;

    TCH_DBG( "ts_suspend: E\n");

    rc = ts_suspend_ic(ts, client);
    if( !rc )
        msm_proc_comm(PCOM_CUSTOMER_CMD1, &id, NULL);

    TCH_DBG( "ts_suspend: X\n");

#if GETTOUCHLOG_AFTER_SCREENOFF
  atomic_set(&GetTouchLog1AfterLaterResume,0);
  atomic_set(&GetTouchLog2AfterLaterResume,0);
#endif

#ifndef CONFIG_HAS_EARLYSUSPEND
    return rc;
#endif
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void ts_resume(struct early_suspend *h)
#else
static int ts_resume(struct i2c_client *client)
#endif
{
    int rc = 0;
#ifdef CONFIG_HAS_EARLYSUSPEND
    struct ts_t *ts = container_of( h,
                                    struct ts_t,
                                    touch_early_suspend);
    struct i2c_client *client = ts->client_4_i2c;
#else
    struct ts_t *ts = (struct ts_t *)i2c_get_clientdata( client );
#endif
    unsigned id = SMEM_PC_OEM_PM_GP2_LPM_OFF;

    TCH_DBG("ts_resume: E\n");

    msm_proc_comm(PCOM_CUSTOMER_CMD1, &id, NULL);

#if GETTOUCHLOG_AFTER_SCREENOFF
  atomic_set(&GetTouchLog1AfterLaterResume,5);
  atomic_set(&GetTouchLog2AfterLaterResume,5);
#endif

    rc = ts_resume_ic(ts, client);

    TCH_DBG("ts_resume: X\n");

#ifndef CONFIG_HAS_EARLYSUSPEND
    return rc;
#endif
}

static struct device_attribute auo_touch_ctrl_attrs[] = {
  __ATTR(dump_reg, 0444, auo_touch_dump_property, NULL),
  __ATTR(dump_var, 0444, auo_touch_dump_property, NULL),
  __ATTR(dump_touch, 0444, auo_touch_dump_property, NULL),
};

static ssize_t auo_touch_dump_property(struct device *dev, struct device_attribute *attr, char *buf)
{
    const ptrdiff_t offset = attr - auo_touch_ctrl_attrs;
    struct i2c_client *client = container_of( dev,
                                              struct i2c_client,
                                              dev);
	uint8_t data[5];
	int strLen = 0;
	
	strLen = sprintf(buf+strLen,"----- DUMP START -----\n");

    switch(offset)
	{
	    case 0:
        {   
            if( !ts_read_i2c( client, 0x77, data, 1 ) )
	            strLen += sprintf(buf+strLen, "VERSION(0x%02X):\n", data[0]);

            if( !ts_read_i2c( client, 0x6F, data, 5 ) )
			{
	            strLen += sprintf(buf+strLen, "X_SENSITIVITY=0x%02X\n", data[0]);
	            strLen += sprintf(buf+strLen, "Y_SENSITIVITY=0x%02X\n", data[1]);
	            strLen += sprintf(buf+strLen, "INT_SETTING=0x%02X\n", data[2]);
	            strLen += sprintf(buf+strLen, "INT_WIDTH=0x%02X\n", data[3]);
	            strLen += sprintf(buf+strLen, "POWER_MODE=0x%02X\n", data[4]);
		    }
	        break;
	    }
		case 1: 
		{
	        down(&ts_sem);
	        strLen += sprintf(buf+strLen, "GLOBAL VARS:\n");
	        //strLen += sprintf(buf+strLen, "psensor approached=%d\n", atomic_read(&psensor_approached));
	        strLen += sprintf(buf+strLen, "touch indicate int=%d\n", g_touch_ind_mode);
	        strLen += sprintf(buf+strLen, "pixcir detected=%d\n", g_pixcir_detected);
	        strLen += sprintf(buf+strLen, "pixcir freeze=%d\n", g_pixcir_freeze);
            up(&ts_sem);
		    break;
        }
		case 2: 
		{
		    struct auo_mt_t *pMsg = &g_ts->mtMsg;
			struct auo_point_info_t *pPoint;
			int i;

	        strLen += sprintf(buf+strLen, "TOUCH INFO:\n");
	        strLen += sprintf(buf+strLen, "mt state=%d\n", pMsg->state);
			for(i=0; i<AUO_REPORT_POINTS; i++)
			{
			    pPoint = &pMsg->points[i];
	            strLen += sprintf(buf+strLen, "point[%d] state =%d\n", i, pPoint->state);
	            strLen += sprintf(buf+strLen, "point[%d] coord =(%d,%d)\n", i, pPoint->coord.x, pPoint->coord.y);
			}
		    break;
		}
	}

	strLen += sprintf(buf+strLen, "----- DUMP END   -----\n");

	return strLen;
}

static const struct i2c_device_id i2cAuoTouch_idtable[] = {
       { TS_DRIVER_NAME, 0 },
       { }
};

MODULE_DEVICE_TABLE(i2c, i2cAuoTouch_idtable);

static struct i2c_driver i2c_ts_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name  = TS_DRIVER_NAME,
	},
	.probe	 = ts_probe,
	.remove	 = ts_remove,
#ifndef CONFIG_HAS_EARLYSUSPEND
    .suspend = ts_suspend,
    .resume  = ts_resume,
#endif
	.id_table = i2cAuoTouch_idtable,
};

static int __devinit ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int    result;
    struct ts_t *ts;
    struct auo_touchscreen_platform_data *pdata;
	int i;

    TCH_DBG("ts_probe()\n");

    ts = kzalloc( sizeof(struct ts_t), GFP_KERNEL );
    if( !ts )
    {
        result = -ENOMEM;
        return result;
    }

    memset( ts, 0, sizeof(struct ts_t));
    pdata = client->dev.platform_data;
    ts->gpio_num = pdata->gpioirq;
    ts->gpio_rst = pdata->gpiorst;
    ts->irq = MSM_GPIO_TO_INT( ts->gpio_num );
    ts->client_4_i2c = client;

    result = misc_register( &ts_misc_device );
    if( result )
    {
        TCH_DBG("ts_probe: failed register misc driver\n");
        goto fail_alloc_mem;
    }

    result = ts_register_input( &ts->input, pdata, client );
    if( result )
        goto fail_misc_register;
    input_set_drvdata(ts->input, ts);

    result = ts_setup_gpio( ts->gpio_num, ts->gpio_rst);
    if( result )
        goto fail_config_gpio;

#ifdef USE_PIXCIR
    result = ts_power_switch(1);
    if( result )
    {
        TCH_DBG("ts_probe: failed to power on\n");
        goto fail_setup_gpio;
    }
#endif

    result = ts_config_gpio( ts->gpio_num, ts->gpio_rst );
    if( result )
    {
        TCH_DBG("ts_probe: failed to config gpio\n");
        goto fail_power_on;
    }

    result = ts_detect_pixcir(ts);
    if( result )
    {
        TCH_DBG("ts_probe: failed to detect\n");
        goto fail_power_on;
    }

    result = ts_setup_reg_map(ts);
    if( result )
    {
        TCH_DBG("ts_probe: failed to setup reg map\n");
        goto fail_power_on;
    }

    result = ts_config_panel(ts);
    if( result )
    {
        TCH_DBG("ts_probe: failed to config panel\n");
    }

    INIT_DELAYED_WORK( &ts->touch_work, ts_irqWorkHandler );
    ts->touch_wqueue = create_singlethread_workqueue(TS_DRIVER_NAME);
    if (!ts->touch_wqueue) {
        result = -ESRCH;
        goto fail_power_on;
    }
    client->driver = &i2c_ts_driver;
    i2c_set_clientdata( client, ts );
    g_ts = ts;

#ifdef CONFIG_HAS_EARLYSUSPEND
    ts->touch_early_suspend.level = 150;
    ts->touch_early_suspend.suspend = ts_suspend;
    ts->touch_early_suspend.resume = ts_resume;
    register_early_suspend(&ts->touch_early_suspend);
#endif

	for(i=0; i<ARRAY_SIZE(auo_touch_ctrl_attrs); i++)
	{
	    result = device_create_file(&client->dev, &auo_touch_ctrl_attrs[i]);
	    if( result ) TCH_DBG("%s: create file err, %d\n", __func__, result);
    }

    return 0;

fail_power_on:
#ifdef USE_PIXCIR
    ts_power_switch(0);
fail_setup_gpio:
#endif
    ts_release_gpio( ts->gpio_num, ts->gpio_rst );
fail_config_gpio:
    input_unregister_device( ts->input );
    input_free_device( ts->input );
    ts->input = NULL;

fail_misc_register:
    misc_deregister( &ts_misc_device );

fail_alloc_mem:
    kfree(ts);
    g_ts = NULL;
    return result;
}


static int __init ts_init(void)
{
    int rc;

    printk("BootLog, +%s\n", __func__);

    i2c_ts_driver.driver.name = TS_DRIVER_NAME;
    rc = i2c_add_driver( &i2c_ts_driver );

    printk("BootLog, -%s, ret=%d\n", __func__, rc);

    return rc;
}
module_init(ts_init);


static void __exit ts_exit(void)
{
    TCH_DBG("ts_exit()\n");

    i2c_del_driver( &i2c_ts_driver );
}
module_exit(ts_exit);


MODULE_DESCRIPTION("AUO touchscreen driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("MENG-HAN CHENG");
MODULE_ALIAS("platform:auo_touch");
