/* drivers/input/touchscreen/atmel_mXT224_touch.c
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
#ifdef   CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/time.h>
#include <mach/gpio.h>
#include <linux/mutex.h>
#include <mach/vreg.h>
#include <mach/atmel_mXT224_touch.h>
#include "atmel_touch_obj.h"
#include "atmel_touch_TPK_obj.h"
#include "atmel_touch_TPK_EVT12_obj.h"
#include "atmel_touch_TPK_EVT12_obj_V20.h"
#include "atmel_touch_cando_obj.h"
#include "atmel_touch_TPK_obj_V20.h" 
#include "atmel_firmware_20.h"
#include <mach/pm_log.h>
#include <mach/austin_hwid.h>
#include <linux/slab.h>

#define ECHOSTR_SIZE   30
#define SPEED_DISABLETOUCH_WHEN_PSENSORACTIVE 1

#ifdef SPEED_DISABLETOUCH_WHEN_PSENSORACTIVE
extern atomic_t psensor_approached;
#endif
static int DLL=0;
module_param_named( 
	DLL, DLL,
 	int, S_IRUGO | S_IWUSR | S_IWGRP
)
#define INFO_LEVEL  1
#define ERR_LEVEL   2
#define TEST_INFO_LEVEL 4
#define MY_INFO_PRINTK(level, fmt, args...) if(level <= DLL) printk( fmt, ##args);
#define PRINT_IN MY_INFO_PRINTK(4,"+++++%s++++ %d\n",__func__,__LINE__);
#define PRINT_OUT MY_INFO_PRINTK(4,"----%s---- %d\n",__func__,__LINE__);

struct touchpad_t {
    struct i2c_client        *client_4_i2c;
    struct input_dev         *input;
	struct input_dev         *keyarray_input;
    int                      irq; 
    int                      gpio_num; 
    int                      gpio_rst; 
    int                      open_count; 
	int						 touch_suspended; 
    int                      misc_open_count; 
    struct delayed_work      touchpad_work;
    struct workqueue_struct  *touchpad_wqueue;
    uint32_t                 info_block_checksum;
    uint32_t                 T6_config_checksum;
    struct  id_info_t        id_info; 
    struct  obj_t            *init_obj_element[MAX_OBJ_ELEMENT_SIZE];
    struct  tp_multi_t       *multi;
    struct  touch_point_status_t msg[ATMEL_REPORT_POINTS];  
    struct  mutex            mutex;
	struct early_suspend     touch_early_suspend; 
	uint16_t                 atmel_x_max,atmel_y_max;
	int                      fvs_mode_flag;  
	uint8_t                  i2c_addr;
	int check_home_key;
	int chk_count;
	struct workqueue_struct *chk_wqueue;
	struct delayed_work      chk_work;
	int selftest_flag;
	struct atmel_selftest_t selftest;	
};

static struct touchpad_t         *g_tp;
static int __devinit touchpad_probe(struct i2c_client *client,  const struct i2c_device_id *);
static int calculate_infoblock_crc(struct touchpad_t *g_tp);
static int keyarray_register_input( struct input_dev **input,struct i2c_client *client );
static void tp_resume(struct early_suspend *h);
static void tp_suspend(struct early_suspend *h);

#define TOUCH_RETRY_COUNT 5
static int touchpad_write_i2c( struct i2c_client *client,
							   uint16_t			regBuf,
                               uint8_t          *dataBuf,
                               uint8_t          dataLen )
{
    int     result = 0;
	int     retryCnt = TOUCH_RETRY_COUNT;
    uint8_t *buf = NULL;
    
    struct  i2c_msg msgs[] = { 
        [0] = {
            .addr   = g_tp->i2c_addr,
            .flags  = 0,
            .buf    = (void *)buf,
            .len    = 0
        }
    };
    
    PRINT_IN
    buf = kzalloc( dataLen+sizeof(regBuf), GFP_KERNEL );
    if( NULL == buf )
    {
        MY_INFO_PRINTK( 0,"ERROR_LEVEL:""touchpad_write_i2c: alloc memory failed\n" );
        return -EFAULT;
    }
    
    buf[0] = (uint8_t)(regBuf & 0xFF);
    buf[1] = (uint8_t)(regBuf >> 8);
    memcpy( &buf[2], dataBuf, dataLen );
    msgs[0].buf = buf;
    msgs[0].len = dataLen+sizeof(regBuf);

    while( retryCnt )
    {
		result = i2c_transfer( client->adapter, msgs, 1 );
		if( result != ARRAY_SIZE(msgs) )
		{
			MY_INFO_PRINTK( 0,"ERROR_LEVEL:""touchpad_write_i2c: write %Xh %Xh %d bytes return failure, %d\n", buf[0], buf[1], dataLen ,result);
			if( -ETIMEDOUT == result ) msleep(10);
            retryCnt--;
        }else {
            result = 0;
            break;
		}
	}
	
	
    if( (result == 0) && (retryCnt < TOUCH_RETRY_COUNT) )
        MY_INFO_PRINTK( 1,"INFO_LEVEL:""touchpad_write_i2c: write %Xh %d bytes retry at %d\n", buf[0], dataLen, TOUCH_RETRY_COUNT-retryCnt);
		
    kfree(buf);
    PRINT_OUT
	return result;
}

static int touchpad_read_i2c( struct i2c_client *client,
                            uint16_t           regBuf,
                            uint8_t           *dataBuf,
                            uint8_t           dataLen )
{
    int     result = 0;
	int     retryCnt = TOUCH_RETRY_COUNT;
	
    struct  i2c_msg msgs[] = { 
        [0] = {
            .addr   = g_tp->i2c_addr,
            .flags  = 0,      
            .buf    = (void *)&regBuf,
            .len    = sizeof(regBuf)
        },
        [1] = {                     
            .addr   = g_tp->i2c_addr,
            .flags  = I2C_M_RD,
            .buf    = (void *)dataBuf,
            .len    = dataLen
        }
    };
    
	PRINT_IN
	while( retryCnt )
    {
		result = i2c_transfer( client->adapter, msgs, ARRAY_SIZE(msgs) );
		if( result != ARRAY_SIZE(msgs) )
		{
			MY_INFO_PRINTK( 0,"ERROR_LEVEL:""read %Xh %d bytes return failure, %d\n", regBuf, dataLen,result );
		if( -ETIMEDOUT == result ) msleep(10);
            retryCnt--;
        }else {
            result = 0;
            break;
        }
	}
	
	if( (result == 0) && (retryCnt < TOUCH_RETRY_COUNT) )
        MY_INFO_PRINTK( 1,"INFO_LEVEL:""touchpad_read_i2c: read %Xh %d bytes retry at %d\n", regBuf, dataLen, TOUCH_RETRY_COUNT-retryCnt);
		
	PRINT_OUT
	return result;
}

static int tp_read_T5_via_i2c( struct i2c_client *client,
                            uint8_t           *dataBuf,
                            uint8_t           dataLen )
{
	int result = 0;
	uint16_t addr = maxTouchCfg_T5_obj.obj_addr;
	uint8_t size = maxTouchCfg_T5_obj.size;
	
	PRINT_IN
	if(addr == 0 || size == 0)
	{
		MY_INFO_PRINTK( 2,"ERROR_LEVEL:""maxTouchCfg_T5_obj is NOT initialized\n");
		result = -ENOMEM;
		PRINT_OUT
		return result;		
	}
	if(client == NULL || dataBuf == NULL || dataLen != size)
	{
		MY_INFO_PRINTK( 0,"ERROR_LEVEL:""no mem for reading message via i2c\n");
		result = -ENOMEM;
		PRINT_OUT
		return result;
	}
	result = touchpad_read_i2c(client,addr,dataBuf,dataLen);
	if(result)
	{
		MY_INFO_PRINTK( 0,"ERROR_LEVEL:""%s failed, result=%d\n", __func__,result);
	}
	PRINT_OUT
	return result;
}

static uint32_t CRC_24(uint32_t crc, uint8_t byte1, uint8_t byte2)
{
	static const uint32_t crcpoly = 0x80001B;
   	uint32_t result;
   	uint16_t data_word;
   
   	PRINT_IN
   	data_word = (uint16_t) ((uint16_t) (byte2 << 8u) | byte1);
   	result = ((crc << 1u) ^ (uint32_t) data_word);
   
   	if (result & 0x1000000)
  	{
		result ^= crcpoly;
   	}
   	PRINT_OUT
   	return(result);
}

static int calculate_config_crc(struct touchpad_t *g_tp)
{
	int result = 0;
   	uint32_t crc = 0;
   	uint16_t crc_area_size = 0;
   	uint16_t remainder;
   	uint8_t  *value = NULL; 
    uint8_t  i,j,index;
   
   	PRINT_IN
   	for ( i = 0 ; i < g_tp->id_info.num_obj_element ; i++ )
    {
    	if( g_tp->init_obj_element[i]->config_crc == 1 )
    	{
    		crc_area_size += ( g_tp->init_obj_element[i]->size );   			
    		MY_INFO_PRINTK( 4, "INFO_LEVEL:" "g_tp->init_obj_element[i]->size = %x\n",g_tp->init_obj_element[i]->size );
   			MY_INFO_PRINTK( 4, "INFO_LEVEL:" "crc_area_size = %x\n",crc_area_size );
    	}
    }
    	
    value = kmalloc( crc_area_size, GFP_KERNEL );
   	if( NULL == value ) 
   	{
        result = -ENOMEM;
        PRINT_OUT
		kfree(value);
        return result;
   	}
        
        index = 0;
   	for ( i = 0 ; i < g_tp->id_info.num_obj_element ; i++ )
    {
    	if( g_tp->init_obj_element[i]->config_crc == 1 )
    	{
    		memcpy(&value[index],g_tp->init_obj_element[i]->value_array,g_tp->init_obj_element[i]->size);
    		index += g_tp->init_obj_element[i]->size;
    	}
    }     
        
    for ( j = 0 ; j < crc_area_size ; j++ )
    {
        MY_INFO_PRINTK( 4,"TEST_INFO_LEVEL:""j: %d:\n",j );
        MY_INFO_PRINTK( 4,"TEST_INFO_LEVEL:""value_array: 0x%x:\n",value[j] );
    }
    j = 0;
    remainder = crc_area_size%2;
    if ( remainder==0 ) 
    {  	
   		while ( j < (crc_area_size-1) )
   		{
   			crc = CRC_24( crc, ( value[j]),(value[j+1]) );
      		j += 2;
      	}
    }	  
    else 
    { 	
   		while ( j < (crc_area_size-1) )
   		{
   			crc = CRC_24( crc, ( value[j]),(value[j+1]) );
   			MY_INFO_PRINTK( 4,"TEST_INFO_LEVEL:""calculate config CRC: 0x%x:\n", crc );
      		j += 2;      			
      	}
      	crc = CRC_24(crc, ( value[j]), 0);
        MY_INFO_PRINTK( 4,"TEST_INFO_LEVEL:""calculate config CRC: 0x%x:\n", crc );      		        
    }
   	     
   	crc = (crc & 0x00FFFFFF);
   	MY_INFO_PRINTK( 4,"TEST_INFO_LEVEL:""calculate config CRC: 0x%x:\n", crc );
    MY_INFO_PRINTK( 4, "TEST_INFO_LEVEL:" "g_tp->T6_config_checksum : 0x%x\n",g_tp->T6_config_checksum );
        
   	if ( crc != g_tp->T6_config_checksum )
   	{
   		MY_INFO_PRINTK( 0,"ERROR_LEVEL:""failed to calculate config crc: 0x%x:\n", crc );
   		result = -EFAULT;
   		PRINT_OUT
		kfree(value);
        return result;
   	}
   	PRINT_OUT
	kfree(value);
	return result;
}

static int calculate_infoblock_crc(struct touchpad_t *g_tp)
{
	int result = 0;
   	int INFO_BLOCK_SIZE = ( NUM_OF_ID_INFO_BLOCK+OBJECT_TABLE_ELEMENT_SIZE*g_tp->id_info.num_obj_element )+INFO_BLOCK_CHECKSUM_SIZE;
   	uint32_t crc = 0;
   	uint16_t crc_area_size,regbuf;
   	uint8_t  *value = NULL; 
    uint8_t  i;
   	int	j = 0;
   
   	PRINT_IN
   	value = kmalloc( INFO_BLOCK_SIZE, GFP_KERNEL );
   	if( NULL == value ) 
   	{
        result = -ENOMEM;
        PRINT_OUT
        return result;
   	}
     
   	crc_area_size = NUM_OF_ID_INFO_BLOCK + OBJECT_TABLE_ELEMENT_SIZE*g_tp->id_info.num_obj_element;   
   
   	result = touchpad_read_i2c( g_tp->client_4_i2c,crc_area_size, &value[0], INFO_BLOCK_CHECKSUM_SIZE);
   	for ( regbuf = crc_area_size; regbuf < crc_area_size+INFO_BLOCK_CHECKSUM_SIZE; regbuf++ )
   	{
    	MY_INFO_PRINTK( 4, "TEST_INFO_LEVEL:" "read 0x%xh = 0x%x\n",regbuf, value[j] );
    	j++;
   	}
   	g_tp->info_block_checksum = value[0] | value[1]<< 8 | value[2]<<16;
   	MY_INFO_PRINTK( 4, "INFO_LEVEL:" "info block checksum : 0x%x\n",g_tp->info_block_checksum );
   
   	result = touchpad_read_i2c( g_tp->client_4_i2c,ADD_ID_INFO_BLOCK, &value[0], crc_area_size);
  	MY_INFO_PRINTK( 4,"TEST_INFO_LEVEL:""read info block checksum: 0x%x\n", value[0] );
   	if (result)
   	{
      	MY_INFO_PRINTK( 0, "ERROR_LEVEL:" "unable to read reg 0x%x and value failed 0x%x, return %d\n",crc_area_size,value[0],result );
      	kfree(value);
      	PRINT_OUT
      	return result;
   	}
  
   	i = 0;
   	while (i < (crc_area_size - 1))
   	{
      	crc = CRC_24( crc, (value[i]),(value[i+1]) );
      	i += 2;
   	}
   
   	crc = CRC_24(crc, (value[i]), 0);
   	MY_INFO_PRINTK( 4,"TEST_INFO_LEVEL:""read info block checksum: 0x%x:\n", crc );
      
   	crc = (crc & 0x00FFFFFF);
   	MY_INFO_PRINTK( 4,"INFO_LEVEL:""read info block CRC: 0x%x:\n", crc );
   
   	if ( crc != g_tp->info_block_checksum )
   	{   	
   		MY_INFO_PRINTK( 0,"ERROR_LEVEL:""failed to calculate infoblock crc: 0x%x:\n", crc );
   		kfree(value);
   		PRINT_OUT
        return result;
   	}
   	PRINT_OUT
   	return result;
}

#if 0
static void tp_save_multi_coord(void)
{
	struct atmel_multipoints_t    *buf = g_tp->multi->buf;
    	uint8_t index_r = g_tp->multi->index_r;
    	uint8_t index_w = g_tp->multi->index_w;
        int i; 
    	PRINT_IN
    	for( i = 0;i<ATMEL_REPORT_POINTS;i++)
	{
		buf[index_w].points[i].x = g_tp->msg[i].coord.x;
		buf[index_w].points[i].y = g_tp->msg[i].coord.y;
		if(g_tp->msg[i].state == RELEASE)
		{
			buf[index_w].points[i].x = 0;
			buf[index_w].points[i].y = 0;
		}
	}

	index_w++;
    index_w %= ATMEL_MULTITOUCH_BUF_LEN;
    if( index_r == index_w )
    {
        index_r++;
        index_r %= ATMEL_MULTITOUCH_BUF_LEN;
    }
    g_tp->multi->index_r = index_r;
    g_tp->multi->index_w = index_w;
    wake_up_interruptible( &g_tp->multi->wait );
    return;
}
#endif

static void save_multi_touch_struct(  uint16_t x_coord,uint16_t y_coord,uint8_t touch_status, uint8_t report_id, uint8_t w, uint8_t z )
{
	PRINT_IN
	g_tp->msg[report_id - maxTouchCfg_T9_obj.reportid_ub].coord.x = (uint)x_coord;
	g_tp->msg[report_id - maxTouchCfg_T9_obj.reportid_ub].coord.y = (uint)y_coord;
	if (( touch_status & TOUCH_PRESS_RELEASE_MASK) == TOUCH_PRESS_RELEASE_MASK )
    {
		g_tp->msg[report_id - maxTouchCfg_T9_obj.reportid_ub].state = RELEASE;
		g_tp->msg[report_id - maxTouchCfg_T9_obj.reportid_ub].w = w;
		g_tp->msg[report_id - maxTouchCfg_T9_obj.reportid_ub].z = 0;
	}
	else if	( ( touch_status & TOUCH_RELEASE_MASK) == TOUCH_RELEASE_MASK  )
	{
		g_tp->msg[report_id - maxTouchCfg_T9_obj.reportid_ub].state = RELEASE;
		g_tp->msg[report_id - maxTouchCfg_T9_obj.reportid_ub].w = w;
		g_tp->msg[report_id - maxTouchCfg_T9_obj.reportid_ub].z = 0;
	}
	else if( (touch_status & TOUCH_PRESS_MASK) == TOUCH_PRESS_MASK )
    { 	
		g_tp->msg[report_id - maxTouchCfg_T9_obj.reportid_ub].state = PRESS;
		g_tp->msg[report_id - maxTouchCfg_T9_obj.reportid_ub].w = w;
		g_tp->msg[report_id - maxTouchCfg_T9_obj.reportid_ub].z = z;
	}
	else if ( (touch_status & TOUCH_MOVE_MASK) == TOUCH_MOVE_MASK )
    {
		g_tp->msg[report_id - maxTouchCfg_T9_obj.reportid_ub].state = MOVE;
		g_tp->msg[report_id - maxTouchCfg_T9_obj.reportid_ub].w = w;
		g_tp->msg[report_id - maxTouchCfg_T9_obj.reportid_ub].z = z;
	}
	else if ( (touch_status & TOUCH_DETECT_MASK ) != 0x0 )
	{
		//g_tp->msg[report_id - maxTouchCfg_T9_obj.reportid_ub].state = RELEASE;
	}
	else
    {
    	MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "error touch_state = %x\n",touch_status );
    }			
	PRINT_OUT
	return;
}	

static void tp_report_coord_via_mt_protocol(void)
{
	int i;
	PRINT_IN
	for(i=0;i<ATMEL_REPORT_POINTS;i++)
	{
		if (g_tp->msg[i].z == -1)
			continue;
		
		input_report_abs(g_tp->input, ABS_MT_TOUCH_MAJOR, g_tp->msg[i].z); 
		input_report_abs(g_tp->input, ABS_MT_WIDTH_MAJOR, g_tp->msg[i].w);
		input_report_abs(g_tp->input, ABS_MT_POSITION_X, g_tp->msg[i].coord.x); 
		input_report_abs(g_tp->input, ABS_MT_POSITION_Y, g_tp->msg[i].coord.y);	
		input_report_abs(g_tp->input, ABS_MT_TRACKING_ID, i);
		MY_INFO_PRINTK( 1,"INFO_LEVEL:" "i %d : x %d y %d z %d w %d\n",i,g_tp->msg[i].coord.x,g_tp->msg[i].coord.y,g_tp->msg[i].z,g_tp->msg[i].w ); 

		input_mt_sync(g_tp->input); 
		
		if (g_tp->msg[i].z == 0)
			g_tp->msg[i].z = -1;
	}
	input_sync(g_tp->input);
	PRINT_OUT
}

#if 0
static void tp_report_single_coord( uint16_t x_coord,uint16_t y_coord,uint8_t touch_status )
{
	PRINT_IN
	if (( touch_status & TOUCH_PRESS_RELEASE_MASK) == TOUCH_PRESS_RELEASE_MASK )
	{
		input_report_abs(g_tp->input, ABS_X, x_coord);
		input_report_abs(g_tp->input, ABS_Y, y_coord);
		MY_INFO_PRINTK( 4,"INFO_LEVEL:""report x,y coordinate:(%d,%d)\n", (int)x_coord,(int)y_coord);
		input_report_key(g_tp->input, BTN_TOUCH, 1);
		MY_INFO_PRINTK( 4,"INFO_LEVEL:"" press one touch\n");
		input_sync(g_tp->input);       		
		input_report_abs(g_tp->input, ABS_X, x_coord);
		input_report_abs(g_tp->input, ABS_Y, y_coord);
		MY_INFO_PRINTK( 4,"INFO_LEVEL:""report x,y coordinate:(%d,%d)\n", (int)x_coord,(int)y_coord);
		input_report_key(g_tp->input, BTN_TOUCH, 0);
		MY_INFO_PRINTK( 4,"INFO_LEVEL:"" release one touch\n");
		input_sync(g_tp->input);
	}
	else if ( ( touch_status & TOUCH_RELEASE_MASK) == TOUCH_RELEASE_MASK  )
	{
		input_report_abs(g_tp->input, ABS_X, x_coord);
		input_report_abs(g_tp->input, ABS_Y, y_coord);
		MY_INFO_PRINTK( 4,"INFO_LEVEL:""report x,y coordinate:(%d,%d)\n", (int)x_coord,(int)y_coord);
		input_report_key(g_tp->input, BTN_TOUCH, 0);
		MY_INFO_PRINTK( 4,"INFO_LEVEL:"" release one touch\n");
		input_sync(g_tp->input);
	}
	else if( (touch_status & TOUCH_PRESS_MASK) == TOUCH_PRESS_MASK )
	{
		input_report_abs(g_tp->input, ABS_X, x_coord);
		input_report_abs(g_tp->input, ABS_Y, y_coord);
		MY_INFO_PRINTK( 4,"INFO_LEVEL:""report x,y coordinate:(%d,%d)\n", (int)x_coord,(int)y_coord);
		input_report_key(g_tp->input, BTN_TOUCH, 1);
		MY_INFO_PRINTK( 4,"INFO_LEVEL:"" press one touch\n");
		input_sync(g_tp->input);
	}
	else if ( (touch_status & TOUCH_MOVE_MASK) == TOUCH_MOVE_MASK )
	{
		input_report_abs(g_tp->input, ABS_X, x_coord);
		input_report_abs(g_tp->input, ABS_Y, y_coord);
		MY_INFO_PRINTK( 4,"INFO_LEVEL:""report x,y coordinate:(%d,%d)\n", (int)x_coord,(int)y_coord);
		input_report_key(g_tp->input, BTN_TOUCH, 1);
		MY_INFO_PRINTK( 4,"INFO_LEVEL:"" move one touch\n");
		input_sync(g_tp->input);    	
	}
	else if ( (touch_status & TOUCH_DETECT_MASK ) != 0x0 )
	{
     	
	}
	else
	{
		MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "error touch_state = %x\n",touch_status );
	}
	PRINT_OUT
	return;
}
#endif

static void touchpad_report_coord( uint16_t x_coord,uint16_t y_coord,uint8_t touch_status, uint8_t report_id,uint8_t w,uint8_t z )
{                 
	PRINT_IN 
#ifdef SPEED_DISABLETOUCH_WHEN_PSENSORACTIVE
        if(unlikely(atomic_read(&psensor_approached)))
		{
			save_multi_touch_struct( x_coord,y_coord,touch_status,report_id,w,z );
			MY_INFO_PRINTK( 4,"INFO_LEVEL:"" Ignore touch\n" );
		}	
		else
		{
			save_multi_touch_struct( x_coord,y_coord,touch_status,report_id,w,z );
			tp_report_coord_via_mt_protocol();
			#if 0
			if( g_tp->misc_open_count )
			{
				tp_save_multi_coord();
			}
			#endif
			if ( report_id == maxTouchCfg_T9_obj.reportid_ub )
			{
				//tp_report_single_coord( x_coord,y_coord,touch_status );
			}
		}	
#else
	save_multi_touch_struct( x_coord,y_coord,touch_status,report_id,w,z );
	tp_report_coord_via_mt_protocol();
	#if 0
	if( g_tp->misc_open_count )
    {
        tp_save_multi_coord();
    }
	#endif
	if ( report_id == maxTouchCfg_T9_obj.reportid_ub )
	{
		//tp_report_single_coord( x_coord,y_coord,touch_status );
	}
	
#endif
	PRINT_OUT
	return;   
}

int T20_msg_handler ( uint8_t *value )
{
	int i = 0;
	
	PRINT_IN
	for (i = 0 ; i < ATMEL_REPORT_POINTS ; i++ )
	{
		if (g_tp->msg[i].z == -1)
			continue;
		g_tp->msg[i].z = 0;
		g_tp->msg[i].state = RELEASE;
	}
	tp_report_coord_via_mt_protocol();
	
    PRINT_OUT
    return 0;
}

int T15_msg_handler ( uint8_t *value  )
{	
	PRINT_IN
	MY_INFO_PRINTK( 4,"TEST_INFO_LEVEL:"" multiscreen touch T15 report id is  : %d\n",value[0] );
	printk( " Keyarray T15 value[0] : %d value[1] : %d value[2] : %d \n",value[0],value[1],value[2] );
	if((value[2] & 0x04) && g_tp->check_home_key)
	{
		g_tp->chk_count = 0;
		queue_delayed_work(g_tp->chk_wqueue, &g_tp->chk_work, 0);
		g_tp->check_home_key = 0;
		printk("queue chk work\n");
	}
	if ((system_rev <= TOUCAN_EVT1_Band148) || system_rev == HWID_UNKNOWN)
    {
		if ( g_tp->fvs_mode_flag == 1 )
		{
			if(value[2] & 0x01)
			{
				MY_INFO_PRINTK( 1,"TEST_INFO_LEVEL:"" KEY_F2 is pressed\n");
				input_report_key(g_tp->keyarray_input, KEY_F2, 1);
			}
			else
			{
				MY_INFO_PRINTK( 1,"TEST_INFO_LEVEL:"" KEY_F2 is released\n");
				input_report_key(g_tp->keyarray_input, KEY_F2, 0);
			}
			if(value[2] & 0x02)
			{
				MY_INFO_PRINTK( 1,"TEST_INFO_LEVEL:"" KEY_F5 is pressed\n");
				input_report_key(g_tp->keyarray_input, KEY_F5, 1);
			}
			else
			{
				MY_INFO_PRINTK( 1,"TEST_INFO_LEVEL:"" KEY_F5 is released\n");
				input_report_key(g_tp->keyarray_input, KEY_F5, 0);
			}
			if(value[2] & 0x04)
			{
				MY_INFO_PRINTK( 1,"TEST_INFO_LEVEL:"" KEY_F6 is pressed\n");
				input_report_key(g_tp->keyarray_input, KEY_F6, 1);
			}
			else
			{
				MY_INFO_PRINTK( 1,"TEST_INFO_LEVEL:"" KEY_F6 is released\n");
				input_report_key(g_tp->keyarray_input, KEY_F6, 0);
			}
		}
		else
		{
			if(value[2] & 0x01)
			{
				MY_INFO_PRINTK( 1,"TEST_INFO_LEVEL:"" HOME_BACK is pressed\n");
				input_report_key(g_tp->keyarray_input, KEY_BACK, 1);
			}
			else
			{
				MY_INFO_PRINTK( 1,"TEST_INFO_LEVEL:"" KEY_BACK is released\n");
				input_report_key(g_tp->keyarray_input, KEY_BACK, 0);
			}
			if(value[2] & 0x02)
			{
				MY_INFO_PRINTK( 1,"TEST_INFO_LEVEL:"" KEY_HOME is pressed\n");
				input_report_key(g_tp->keyarray_input, KEY_HOME, 1);
			}
			else
			{
				MY_INFO_PRINTK( 1,"TEST_INFO_LEVEL:"" KEY_HOME is released\n");
				input_report_key(g_tp->keyarray_input, KEY_HOME, 0);
			}
			if(value[2] & 0x04)
			{
				MY_INFO_PRINTK( 1,"TEST_INFO_LEVEL:"" KEY_MENU is pressed\n");
				input_report_key(g_tp->keyarray_input, KEY_MENU, 1);
			}
			else
			{
				MY_INFO_PRINTK( 1,"TEST_INFO_LEVEL:"" KEY_MENU is released\n");
				input_report_key(g_tp->keyarray_input, KEY_MENU, 0);
			}
		}
	}
	else
	{
		if ( g_tp->fvs_mode_flag == 1 )
		{
			if(value[2] & 0x01)
			{
				MY_INFO_PRINTK( 1,"TEST_INFO_LEVEL:"" KEY_F2 is pressed\n");
				input_report_key(g_tp->keyarray_input, KEY_F2, 1);
			}
			else
			{
				MY_INFO_PRINTK( 1,"TEST_INFO_LEVEL:"" KEY_F2 is released\n");
				input_report_key(g_tp->keyarray_input, KEY_F2, 0);
			}
			if(value[2] & 0x02)
			{
				MY_INFO_PRINTK( 1,"TEST_INFO_LEVEL:"" KEY_F6 is pressed\n");
				input_report_key(g_tp->keyarray_input, KEY_F6, 1);
			}
			else
			{
				MY_INFO_PRINTK( 1,"TEST_INFO_LEVEL:"" KEY_F6 is released\n");
				input_report_key(g_tp->keyarray_input, KEY_F6, 0);
			}
			if(value[2] & 0x04)
			{
				MY_INFO_PRINTK( 1,"TEST_INFO_LEVEL:"" KEY_F5 is pressed\n");
				input_report_key(g_tp->keyarray_input, KEY_F5, 1);
			}
			else
			{
				MY_INFO_PRINTK( 1,"TEST_INFO_LEVEL:"" KEY_F5 is released\n");
				input_report_key(g_tp->keyarray_input, KEY_F5, 0);
			}
		}
		else
		{
			if(value[2] & 0x01)
			{
				MY_INFO_PRINTK( 1,"TEST_INFO_LEVEL:"" HOME_BACK is pressed\n");
				input_report_key(g_tp->keyarray_input, KEY_BACK, 1);
			}
			else
			{
				MY_INFO_PRINTK( 1,"TEST_INFO_LEVEL:"" KEY_BACK is released\n");
				input_report_key(g_tp->keyarray_input, KEY_BACK, 0);
			}
			if(value[2] & 0x02)
			{
				MY_INFO_PRINTK( 1,"TEST_INFO_LEVEL:"" KEY_MENU is pressed\n");
				input_report_key(g_tp->keyarray_input, KEY_MENU, 1);
			}
			else
			{
				MY_INFO_PRINTK( 1,"TEST_INFO_LEVEL:"" KEY_MENU is released\n");
				input_report_key(g_tp->keyarray_input, KEY_MENU, 0);
			}
			if(value[2] & 0x04)
			{
				MY_INFO_PRINTK( 1,"TEST_INFO_LEVEL:"" KEY_HOME is pressed\n");
				input_report_key(g_tp->keyarray_input, KEY_HOME, 1);
			}
			else
			{
				MY_INFO_PRINTK( 1,"TEST_INFO_LEVEL:"" KEY_HOME is released\n");
				input_report_key(g_tp->keyarray_input, KEY_HOME, 0);
			}
		}
	}	
    PRINT_OUT
    return 0;
}

int T6_msg_handler ( uint8_t *value )
{
	PRINT_IN
	MY_INFO_PRINTK( 4,"INFO_LEVEL:" " command T6 report id is  : %d\n",value[0] );
	MY_INFO_PRINTK( 4,"INFO_LEVEL:" " command T6 status  : %d\n",value[1] );
	MY_INFO_PRINTK( 0,"ERROR_LEVEL:" " It isn't normal to read T6 message \n " );
    PRINT_OUT
    return 0;
}

int T9_msg_handler ( uint8_t *value  )
{
	uint16_t x_coord,y_coord;
	
	PRINT_IN
	MY_INFO_PRINTK( 4,"TEST_INFO_LEVEL:"" multiscreen touch T9 report id is  : %d\n",value[0] );

    x_coord = value[2] << 2 | (value[4] & 0xc0) >> 6;
    y_coord = value[3] << 2 | (value[4] & 0x0c) >> 2;
	MY_INFO_PRINTK( 4,"TEST_INFO_LEVEL:"" x coordinate  : %d\n",(int)x_coord );
    MY_INFO_PRINTK( 4,"TEST_INFO_LEVEL:"" y coordinate  : %d\n",(int)y_coord );
	x_coord = ( ( x_coord - ATMEL_X_ORIGIN ) * ATMEL_TOUCAN_PANEL_X ) / ( g_tp->atmel_x_max - ATMEL_X_ORIGIN );
	y_coord = ( ( y_coord - ATMEL_Y_ORIGIN ) * ATMEL_TOUCAN_PANEL_Y ) / ( g_tp->atmel_y_max - ATMEL_Y_ORIGIN );
	MY_INFO_PRINTK( 4,"TEST_INFO_LEVEL:"" x coordinate  : %x\n",x_coord );
    MY_INFO_PRINTK( 4,"TEST_INFO_LEVEL:"" y coordinate  : %x\n",y_coord );
	MY_INFO_PRINTK( 4,"TEST_INFO_LEVEL:"" x coordinate  : %d\n",(int)x_coord );
    MY_INFO_PRINTK( 4,"TEST_INFO_LEVEL:"" y coordinate  : %d\n",(int)y_coord );
	MY_INFO_PRINTK( 4,"TEST_INFO_LEVEL:"" g_tp->atmel_x_max coordinate  : %d\n",(int)g_tp->atmel_x_max );
	MY_INFO_PRINTK( 4,"TEST_INFO_LEVEL:"" g_tp->atmel_y_max coordinate  : %d\n",(int)g_tp->atmel_y_max );
    touchpad_report_coord( x_coord,y_coord,value[1], value[0],value[5],value[6] );
    PRINT_OUT
    return 0;
}

int T25_msg_handler ( uint8_t *value )
{
	
	PRINT_IN

	printk( "T25_msg_handler :self test T25 result code is  : %d\n",value[0] );
	printk( "T25_msg_handler :" "Result code=0x%x \n",value[1] );
	g_tp->selftest.value = value[1];
		
	if (value[1] == 0xFE)
	{
		printk( "T25_msg_handler :" "All tests passed.\n" );
	}
	else if (value[1] == 0xFD)
	{
		printk( "T25_msg_handler :" "Invalide test code.\n" );
	}
	else if (value[1] == 0x01)
	{
		printk( "T25_msg_handler :" "AVdd is not present.\n" );
	}
	else if (value[1] == 0x11)
	{
		printk( "T25_msg_handler :" "Pin fault.\n" );
	}
	else if (value[1] == 0x17)
	{
		printk( "T25_msg_handler :" "Signal limit fault.\n" );
	}
	else if (value[1] == 0x20)
	{
		printk( "T25_msg_handler :" "Gain error.\n" );
	}
	
    PRINT_OUT
    return 0;
}

static ssize_t tp_misc_read( struct file *fp,
                             char __user *buffer,
                             size_t count,
                             loff_t *ppos )
{
	ssize_t  rc = 0;
    int      ret = 0;
    uint32_t reserved_num;
    uint32_t copied_num;
    uint32_t available_num;
    int i;
    	
	PRINT_IN
	if( count < sizeof(struct atmel_multipoints_t) )
    {
        MY_INFO_PRINTK( 0, "ERROR_LEVEL:" "invalid count %d\n",count );
        return -EINVAL;
    }

	mutex_lock(&g_tp->mutex);
    if( g_tp->multi->index_r == g_tp->multi->index_w )
    {
        mutex_unlock(&g_tp->mutex);
        ret = wait_event_interruptible( g_tp->multi->wait, (g_tp->multi->index_r != g_tp->multi->index_w) );
        if( ret )
        {
            MY_INFO_PRINTK( 0, "ERROR_LEVEL:" "signaled\n" );
            return ret;
        }
        mutex_lock(&g_tp->mutex);
    }

	reserved_num = count/sizeof(struct atmel_multipoints_t);
    available_num = (g_tp->multi->index_w > g_tp->multi->index_r)?
					(g_tp->multi->index_w - g_tp->multi->index_r):
                    (ATMEL_MULTITOUCH_BUF_LEN + g_tp->multi->index_w - g_tp->multi->index_r);
    for ( i = 0 ; i < ATMEL_REPORT_POINTS; i++ )
    {
        MY_INFO_PRINTK( 4,"INFO_LEVEL:" "x_coord = %x\n",g_tp->multi->buf[g_tp->multi->index_r].points[i].x );
		MY_INFO_PRINTK( 4,"INFO_LEVEL:" "y_coord = %x\n",g_tp->multi->buf[g_tp->multi->index_r].points[i].y );
	}
	
	if( reserved_num < available_num )
		copied_num = reserved_num;
    else
        copied_num = available_num;

    ret = rc = 0;
    if( g_tp->multi->index_r + copied_num > ATMEL_MULTITOUCH_BUF_LEN )
    {
        uint32_t copied_bytes;
        copied_bytes = (ATMEL_MULTITOUCH_BUF_LEN - g_tp->multi->index_r) * sizeof(struct atmel_multipoints_t);
        ret = copy_to_user ( buffer,
                            &g_tp->multi->buf[g_tp->multi->index_r],
                            copied_bytes ); 
        for ( i = 0 ; i < ATMEL_REPORT_POINTS; i++ )
        {
            MY_INFO_PRINTK( 4,"INFO_LEVEL:" "x_coord = %x\n",g_tp->multi->buf[g_tp->multi->index_r].points[i].x );
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "y_coord = %x\n",g_tp->multi->buf[g_tp->multi->index_r].points[i].y );
		}
			
        if( ret )
        {
			MY_INFO_PRINTK( 0, "ERROR_LEVEL:" "failed copy %u bytes to user, ret=%d\n",
           		            copied_bytes,ret );
			rc = -EFAULT;
			goto exit_misc_read;
        }
        copied_num -= (ATMEL_MULTITOUCH_BUF_LEN - g_tp->multi->index_r);
        g_tp->multi->index_r = 0;
        buffer += copied_bytes;
        rc += copied_bytes; 
    	}

	if( copied_num > 0 )
    {
        uint32_t copied_bytes;
        copied_bytes = copied_num * sizeof(struct atmel_multipoints_t);
        ret = copy_to_user ( buffer,
                            &g_tp->multi->buf[g_tp->multi->index_r],
                            copied_bytes );
        for ( i = 0 ; i < ATMEL_REPORT_POINTS; i++ )
       	{
            MY_INFO_PRINTK( 4,"INFO_LEVEL:" "x_coord = %x\n",g_tp->multi->buf[g_tp->multi->index_r].points[i].x );
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "y_coord = %x\n",g_tp->multi->buf[g_tp->multi->index_r].points[i].y );
		}		
        if( ret )
        {
            MY_INFO_PRINTK( 0, "ERROR_LEVEL:" "tp_misc_read:failed copy %u bytes to user, ret=%d\n",
           		            copied_bytes, ret );
			rc = -EFAULT;
           	goto exit_misc_read;
        }
        g_tp->multi->index_r += copied_num;
        rc += copied_bytes;            
    }
exit_misc_read:
    if( rc < 0 )
    {
        MY_INFO_PRINTK( 0, "ERROR_LEVEL:" "reserved_num=%d, available_num=%d, copied_num=%d\n",
        reserved_num, available_num, copied_num);
        MY_INFO_PRINTK( 0, "ERROR_LEVEL:" "index_r=%d, index_w=%d\n",g_tp->multi->index_r,g_tp->multi->index_w);
    }
    g_tp->multi->index_r %= ATMEL_MULTITOUCH_BUF_LEN;
	mutex_unlock(&g_tp->mutex);
	PRINT_OUT
	return rc;	 	
}

static void touchpad_irqWorkHandler( struct work_struct *work )
{
	struct i2c_client           *client = g_tp->client_4_i2c;
    uint8_t value[maxTouchCfg_T5_obj.size];
    uint8_t i;
    
    PRINT_IN         

	mutex_lock(&g_tp->mutex);
    do
	{
    	tp_read_T5_via_i2c( client , &value[0], maxTouchCfg_T5_obj.size );    		
		MY_INFO_PRINTK( 1, "TIL:" "rid=%x msg=%x %x %x %x %x %x %x checksum=%x\n",value[0],value[1],value[2],value[3],value[4],value[5],value[6],value[7],value[8]);
		if (value [0] == 0xf)
		{
			printk("rid=%x msg=%x %x %x %x %x %x %x checksum=%x\n",value[0],value[1],value[2],value[3],value[4],value[5],value[6],value[7],value[8]);
		}
    	for ( i = 0 ; i < g_tp->id_info.num_obj_element ; i++ )
    	{
			if (value[0] >= g_tp->init_obj_element[i]->reportid_ub && value[0] <=  g_tp->init_obj_element[i]->reportid_lb)
			{       	 			
				if(g_tp->init_obj_element[i]->msg_handler)
				{
					g_tp->init_obj_element[i]->msg_handler(value);
				}	
				break;
			}
		}
    }while(gpio_get_value(g_tp->gpio_num) == 0);
   	enable_irq( g_tp->irq ); 
	mutex_unlock(&g_tp->mutex);
	PRINT_OUT
    return;
}

static irqreturn_t touchpad_irqHandler(int irq, void *dev_id)
{
	struct touchpad_t *g_tp = dev_id;
    PRINT_IN
    disable_irq_nosync( irq );
    queue_delayed_work(g_tp->touchpad_wqueue, &g_tp->touchpad_work, 0);
    PRINT_OUT    
    return IRQ_HANDLED;
}

static uint16_t get_ref_value(int x,int y,uint8_t data_buffer[][SIZE_OF_REF_MODE_PAGE])
{
	int page_idx,element_idx;
	
	page_idx = (x*y_channel+y)*2 / (SIZE_OF_REF_MODE_PAGE-2);
	element_idx = ((x*y_channel+y)*2 % (SIZE_OF_REF_MODE_PAGE-2))+2;
	if(page_idx >= NUM_OF_REF_MODE_PAGE || element_idx-1 >= SIZE_OF_REF_MODE_PAGE)
	{
		MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "page_idx[%d] or elemenet_idx[%d] is not correct\n",page_idx,element_idx );
		return 0;
	}
	else
	{
		return (uint16_t)(data_buffer[page_idx][element_idx] | data_buffer[page_idx][element_idx+1] << 8);
	}
}

static int read_T37(struct touchpad_t *g_tp,uint8_t data_buffer[][SIZE_OF_REF_MODE_PAGE],uint8_t command)
{
	struct i2c_client *client = g_tp->client_4_i2c;
	int 		result = 0;
	uint8_t 	try_ctr;
	uint8_t 	value;
	uint16_t	addr_T6;
	uint16_t 	addr_T37;
	int i;
	
	PRINT_IN
	value = command;
	addr_T6 = maxTouchCfg_T6_obj.obj_addr+5;
	result = touchpad_write_i2c( client, addr_T6, &value, 1 );
	if (result)
	{
		MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to write References Mode!\n" );
        result = -EFAULT;
		return result;
	}

	for(i=0;i<NUM_OF_REF_MODE_PAGE;i++)
	{
		memset( data_buffer[i], 0xFF, SIZE_OF_REF_MODE_PAGE );
	}
	
	for(i=0;i<NUM_OF_REF_MODE_PAGE;i++)
	{
		addr_T37 = maxTouchCfg_T37_obj.obj_addr;
		try_ctr = 0;
		while(!((data_buffer[i][0] == command) && (data_buffer[i][1] == i)))
		{
			if(try_ctr > 100)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to read T37!\n" );
				result = -EFAULT;
				return result;
			}
			msleep(5);
			try_ctr++;
			result = touchpad_read_i2c( client, addr_T37, &data_buffer[i][0], 2 );
			if (result)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to read T37!\n" );
				result = -EFAULT;
				return result;
			}
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "Mode = 0x%x\n",data_buffer[i][0] );
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "Page = 0x%x\n",data_buffer[i][1] );
		}
		
		result = touchpad_read_i2c( client, addr_T37, &data_buffer[i][0], SIZE_OF_REF_MODE_PAGE);
		if (result)
		{
			MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to read T37!\n" );
			result = -EFAULT;
			return result;
		}
				
		if(i != NUM_OF_REF_MODE_PAGE -1)
		{
			value = 0x01;
			result = touchpad_write_i2c( client, addr_T6, &value, 1 );
			if (result)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to write page up\n" );
				result = -EFAULT;
				return result;
			}
		}
	}
	
	PRINT_OUT
    return result;
}

#define NUM_OF_X_CH 18
#define NUM_OF_Y_CH 12
#define MAX_NUM_CHK 5
static int should_send_k(uint16_t *data)
{
	int      	x_line_atch;
	int i,j;
	uint16_t 	check_mask;
	
		x_line_atch = NUM_OF_X_CH+2;
		for( i = 0 ; i < NUM_OF_X_CH; i++ )
		{
			for( j = 0; j < NUM_OF_Y_CH-1; j++ )
			{
				check_mask = 1 << j;
				/* check touch detect flags */
				if( (data[2+i] & check_mask))
						return 0;				
			}
		}
		if(data[x_line_atch+NUM_OF_X_CH-1] & (1 << (NUM_OF_Y_CH-1)))
		{
			return 1;
		}
		else
			return 0;
}
/* This function will check if the last calibration was good */
static int check_mXT224_touch_calibration(struct touchpad_t *g_tp, int * send_k)
{
	struct i2c_client *client = g_tp->client_4_i2c;
	uint8_t 	data_buffer[2+NUM_OF_X_CH*2*2];
	uint8_t 	try_ctr;
	uint8_t 	value;
	uint8_t 	num_of_tch;
	uint8_t 	num_of_atch;
	uint8_t 	CAL_THR;
	uint16_t 	check_mask;
	uint16_t	addr;
	uint16_t 	addr_T37;
	uint16_t 	data[2+NUM_OF_X_CH*2];
	int         i,j;
	int      	x_line_tch;
	int      	x_line_atch;
	int 		read_bytes;
	int 		result;
	char	row_string[NUM_OF_Y_CH*2+20];
	
	PRINT_IN
	*send_k = 0;
	try_ctr = 0;
	num_of_tch = 0;
	num_of_atch = 0;
	CAL_THR = 10;
	if(g_tp->touch_suspended)
		return 0;
	printk("==============================\n");
	/* touch/antitouch mode */
	value = 0xF3;
	addr = maxTouchCfg_T6_obj.obj_addr+5;
	result = touchpad_write_i2c( client, addr, &value, 1 );
	if (result)
	{
		MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to write touch/antitouch mode!\n" );
        	result = -EFAULT;
		return result;
	}
		
	/* read touch flags from the diagnostic object - clear buffer so the while loop can run first time */
	memset( data_buffer, 0xFF, sizeof(data_buffer) );
	
	/* wait for diagnostic object to update */
	addr_T37 = maxTouchCfg_T37_obj.obj_addr;
	while(!((data_buffer[0] == 0xF3) && (data_buffer[1] == 0x00)))
	{
		/* wait for data to be valid */
		if(try_ctr > 100)
		{
			/* Failed! */
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "Diagnostic Data didn't update!!\n" );
			break;	
		}
	    	msleep(5);
		try_ctr++;
		result = touchpad_read_i2c( client, addr_T37, &data_buffer[0], 2 );
		if (result)
		{
			MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to read T37!\n" );
			result = -EFAULT;
			return result;
		}
		MY_INFO_PRINTK( 4,"INFO_LEVEL:" "Mode = 0x%x\n",data_buffer[0] );
		MY_INFO_PRINTK( 4,"INFO_LEVEL:" "Page = 0x%x\n",data_buffer[1] );
	}
	
	/* data is ready - read the detection flags */
	read_bytes = (NUM_OF_X_CH*2)*2+2;
	result = touchpad_read_i2c( client, addr_T37, &data_buffer[0], read_bytes );
	if (result)
	{
		MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to read T37 data!\n" );
        	result = -EFAULT;
		return result;
	}
	j = 2;
	x_line_tch = (NUM_OF_X_CH*2)+2;
	x_line_atch = NUM_OF_X_CH+2;
	data[0] = data_buffer[0];
	data[1] = data_buffer[1];
	MY_INFO_PRINTK( 4,"INFO_LEVEL:" "Mode[0] : 0x%x\n",data[0] );
	MY_INFO_PRINTK( 1,"INFO_LEVEL:" "Page[1] : 0x%x\n",data[1] );
	for( i = 2; i < read_bytes; i+=2 )
	{
		if ( j < x_line_tch )
		{
			data[j] = data_buffer[i] | data_buffer[i+1] << 8;
			MY_INFO_PRINTK( 1,"INFO_LEVEL:" "data[%d] : 0x%x\n",j,data[j] );
			j++;
		}	
	}
	
	/* count up the channels/bits if we recived the data properly */
	if (( data[0] == 0xF3) && (data[1] == 0x00 ))
	{	
		MY_INFO_PRINTK( 4,"INFO_LEVEL:" "T9_YSIZE : %d\n",(int)maxTouchCfg_T9_obj.value_array[4]);
		/* count the channels and print the flags to the log */
		for( i = 0 ; i < NUM_OF_X_CH; i++ )
		{
			snprintf(row_string,sizeof(row_string),"T[%2d]: ",i);
			for( j = 0; j < NUM_OF_Y_CH; j++ )
			{
				check_mask = 1 << j;
				MY_INFO_PRINTK( 4,"INFO_LEVEL:" "check_mask : %x\n",check_mask );
				
				/* check touch detect flags */
				if( (data[2+i] & check_mask) && (data[x_line_atch+i] & check_mask) )
				{
					num_of_tch++;
					num_of_atch++;
					strcat(row_string,"@ ");
				}
				else if(data[x_line_atch+i] & check_mask)
				{
					num_of_atch++;
					strcat(row_string,"A ");					
				}
				else if(data[2+i] & check_mask)
				{
					num_of_tch++;
					strcat(row_string,"T ");
				}
				else
				{
					strcat(row_string,"_ ");
				}				
			}
			strcat(row_string,"\n");
			printk("%s",row_string);
		}
		MY_INFO_PRINTK( 1,"INFO_LEVEL:" "num_0f_tch : %x\n",num_of_tch );
		MY_INFO_PRINTK( 1,"INFO_LEVEL:" "num_0f_atch : %x\n",num_of_atch );
		if(should_send_k(data))
		{
			*send_k = 1;
			value = 0x01;
			result = touchpad_write_i2c( client, maxTouchCfg_T6_obj.obj_addr+2, &value, 1 );
			if (result)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to write calibrate_mXT224_touch!\n" );
        			result = -EFAULT;
				return result;
			}
			printk("send k cmd\n");
		}
		/* send page up command so we can detect when data updates next time */
		value = 0x01;
		result = touchpad_write_i2c( client, addr, &value, 1 );
		if (result)
		{
			MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to write page up \n" );
			result = -EFAULT;
			return result;
		}
		printk("==============================\n");
	}
	PRINT_OUT
	return num_of_atch;
}

static void read_tch_atch( struct work_struct *work )
{
		int send_k = 0;
			
		mutex_lock(&g_tp->mutex);
		if(check_mXT224_touch_calibration(g_tp,&send_k) > 0 && send_k > 0)
		{
			mutex_unlock(&g_tp->mutex);
			return;
		}
		if(g_tp->chk_count < MAX_NUM_CHK)
		{
			g_tp->chk_count++;
			queue_delayed_work(g_tp->chk_wqueue, &g_tp->chk_work, 0);	
		}
		mutex_unlock(&g_tp->mutex);
		msleep(250);
}

static int touchpad_release_gpio( int gpio_num, int gpio_rst )
{
    PRINT_IN
    MY_INFO_PRINTK( 4,"TEST_INFO_LEVEL:""touchpad_release_gpio: releasing gpio %d\n", gpio_num );
    gpio_free( gpio_num );
    gpio_free( gpio_rst );
    PRINT_OUT 
    return 0;
}

#if 1
struct tp_power_t {
    char     *id;
    unsigned mv;
};
struct tp_power_t const atmel_power = {"gp2",  2800};

static int touchpad_poweron_device(int on)
{
    struct  vreg *vreg_atmel;
    int     rc = 0;

    PRINT_IN
	vreg_atmel = vreg_get( 0, atmel_power.id );

	if(on)
    {
        rc = vreg_set_level( vreg_atmel, atmel_power.mv );
        if( rc < 0 )
        {
            MY_INFO_PRINTK( 0,"ERROR_LEVEL:""failed to set %s to level %dmv\n",
							atmel_power.id, atmel_power.mv);
            goto exit_power_switch;
        }
        rc = vreg_enable( vreg_atmel );
        if( rc < 0 )
        {
            MY_INFO_PRINTK(0,"ERROR_LEVEL:""failed to enable %s to %dmv\n",
							atmel_power.id, atmel_power.mv);
            goto exit_power_switch;
        }
        PM_LOG_EVENT(PM_LOG_ON, PM_LOG_TOUCH);
        msleep(1);
    }
	else
	{
		if( vreg_disable( vreg_atmel ) < 0 )
        {
            MY_INFO_PRINTK( 0,"ERROR_LEVEL:""failed to disable %s\n",atmel_power.id);
            rc = -1;
            goto exit_power_switch;
        }
        PM_LOG_EVENT(PM_LOG_OFF, PM_LOG_TOUCH);
	}

exit_power_switch:
	PRINT_OUT
    return rc;
}
#endif

static int touchpad_setup_gpio( int gpio_num, int gpio_rst )
{
    int rc;

    PRINT_IN
    MY_INFO_PRINTK( 4,"TEST_INFO_LEVEL:""touchpad_setup_gpio: setup gpio %d\n", gpio_num );
    rc = gpio_tlmm_config( GPIO_CFG(gpio_num, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA),GPIO_CFG_ENABLE );
    if( rc )
    {
        MY_INFO_PRINTK( 0,"ERROR_LEVEL:""touchpad_setup_gpio_num: remote config gpio_num %d failed (rc=%d)\n", gpio_num , rc );
		touchpad_release_gpio( gpio_num, gpio_rst );
		PRINT_OUT
		return rc;
    }

    rc = gpio_request( gpio_num, "mXT224_touchpad_irq" );
    if ( rc )
    {
    	MY_INFO_PRINTK( 0,"ERROR_LEVEL:""touchpad_setup_gpio: request gpionum %d failed (rc=%d)\n", gpio_num, rc );
		touchpad_release_gpio( gpio_num, gpio_rst );
		PRINT_OUT
		return rc;
    }

    rc = gpio_direction_input( gpio_num );
    if ( rc )
    {
        MY_INFO_PRINTK( 0,"ERROR_LEVEL:""touchpad_setup_gpio: set gpionum %d mode failed (rc=%d)\n", gpio_num, rc );
        touchpad_release_gpio( gpio_num, gpio_rst );
        PRINT_OUT
        return rc;
    }   
    
    MY_INFO_PRINTK( 4,"TEST_INFO_LEVEL:""touchpad_setup_gpio: setup gpio_rst %d\n", gpio_rst );
    rc = gpio_tlmm_config( GPIO_CFG(gpio_rst, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA),
                                        GPIO_CFG_ENABLE );
    if( rc )
    {
        MY_INFO_PRINTK( 0,"ERROR_LEVEL:""touchpad_setup_gpio: remote config gpio_rst %d failed (rc=%d)\n", gpio_rst, rc);
		touchpad_release_gpio( gpio_num, gpio_rst );
        PRINT_OUT
        return rc;
    }

    rc = gpio_request( gpio_rst, "mXT224_touchpad_rst" );
	if ( rc )
    {
        MY_INFO_PRINTK( 0,"ERROR_LEVEL:""touchpad_setup_gpio: request gpio_rst %d failed (rc=%d)\n", gpio_rst, rc);
		touchpad_release_gpio( gpio_num, gpio_rst );
        PRINT_OUT
        return rc;
    }

    rc = gpio_direction_output( gpio_rst, 0 );
    if ( rc )
    {
        MY_INFO_PRINTK( 0,"ERROR_LEVEL:""touchpad_setup_gpio: set gpio_rst %d mode failed (rc=%d)\n", gpio_rst, rc);
        touchpad_release_gpio( gpio_num, gpio_rst );
        PRINT_OUT
        return rc;
    }   
	PRINT_OUT
    return rc;
}

static int touchpad_config_gpio( int gpio_num, int gpio_rst )
{
	int rc = 0;
	
	#if 0
	rc = gpio_clear_detect_status( gpio_num );
    if ( rc )
    {
        MY_INFO_PRINTK( 0,"ERROR_LEVEL:""touchpad_config_gpio_num: clear detect status failed (rc=%d)\n", rc);
        PRINT_OUT
        return rc;
    }
	#endif
	
    gpio_set_value(gpio_rst, 0);
    MY_INFO_PRINTK( 4,"TEST_INFO_LEVEL:""touchpad_config_gpio reset get low : delay 1 ms\n");
    msleep(1);
    gpio_set_value(gpio_rst, 1);
    MY_INFO_PRINTK( 4,"TEST_INFO_LEVEL:""touchpad_config_gpio reset get high : delay 40 ms\n");
	msleep(100); 
	PRINT_OUT
    return rc;
}

static int fillin_object_table(struct obj_t *obj, uint16_t addr,uint8_t *value )
{		
	PRINT_IN
	obj->obj_addr = value[1] | value[2] << 8;
	obj->size = value[3] + 1;
	obj->instance = value[4] + 1;
	obj->num_reportid = value[5];
	MY_INFO_PRINTK( 4, "TEST_INFO_LEVEL:" "read 0x%x = 0x%x\n", addr,obj->obj_addr );
	MY_INFO_PRINTK( 4, "TEST_INFO_LEVEL:" "read 0x%x = 0x%d\n",addr,obj->size );
	MY_INFO_PRINTK( 4, "TEST_INFO_LEVEL:" "read 0x%x = 0x%d\n",addr,obj->instance );
	MY_INFO_PRINTK( 4, "TEST_INFO_LEVEL:" "read 0x%x = 0x%d\n",addr,obj->num_reportid );       
	PRINT_OUT
	return 0;
}

static int fillin_init_obj_table( uint8_t match_type_id,struct obj_t *ptr_obj_struct,uint8_t *obj_value,unsigned char *value_ub,uint16_t obj_addr,uint8_t array_size,int *index )
{
	int j;
	uint8_t read_type_id = obj_value[0];
	uint8_t num_of_reportid = obj_value[5];
	uint8_t num_of_instance = obj_value[4]+1;
	
	PRINT_IN
	#if 0
	MY_INFO_PRINTK( 4, "INFO_LEVEL:" "match_type_id 0x%x : \n",match_type_id );
	MY_INFO_PRINTK( 4, "INFO_LEVEL:" "ptr_obj_struct %p : \n",ptr_obj_struct );
	MY_INFO_PRINTK( 4, "INFO_LEVEL:" "obj_value %p : \n",obj_value );
	MY_INFO_PRINTK( 4, "INFO_LEVEL:" "value_ub %p : \n",value_ub );
	MY_INFO_PRINTK( 4, "INFO_LEVEL:" "obj_addr 0x%x : \n",obj_addr );
	MY_INFO_PRINTK( 4, "INFO_LEVEL:" "array_size 0x%x : \n",array_size );
	MY_INFO_PRINTK( 4, "INFO_LEVEL:" "index  %p : \n",index  );
	#endif	
	
	if ( read_type_id == match_type_id )
    {      		
    	for ( j = 0; j< OBJECT_TABLE_ELEMENT_SIZE; j++ )
		{
    		MY_INFO_PRINTK( 4, "TEST_INFO_LEVEL:" "read 0x%xh = 0x%x\n",obj_addr, obj_value[j] );
    	}
    	fillin_object_table( ptr_obj_struct,obj_addr,obj_value );
    	ptr_obj_struct->reportid_ub = *value_ub;
    	ptr_obj_struct->reportid_lb = *value_ub+(num_of_reportid*num_of_instance)-1;
    	*value_ub += (num_of_reportid*num_of_instance);
    	MY_INFO_PRINTK( 4, "TEST_INFO_LEVEL:" "T38_obj_reportid_ub  %d\n",ptr_obj_struct->reportid_ub );
		MY_INFO_PRINTK( 4, "TEST_INFO_LEVEL:" "T38_obj_reportid_lb %d\n",ptr_obj_struct->reportid_lb );	   		
    		
    	if ( (uint8_t)array_size == 0 || ptr_obj_struct->size == (uint8_t)array_size )
    	{
       		g_tp->init_obj_element[*index] = ptr_obj_struct;
            *index += 1;
       	}
       	else
       	{
       		MY_INFO_PRINTK( 0, "ERROR_LEVEL:" "i2c read object value isn't equal actual object table size\n" );
    		PRINT_OUT
    		return -EFAULT;
       	}
    }
    PRINT_OUT
    return 0;
}

static int touchpad_config_mXT224(struct touchpad_t *g_tp)
{
	int result;
	struct i2c_client *client = g_tp->client_4_i2c;
	uint8_t value;
	int i;
	uint16_t regbuf;

    PRINT_IN
    value = T6_BACKUPNV_VALUE;
    for ( i = 0 ; i < g_tp->id_info.num_obj_element ; i++ )
    {
    	if( g_tp->init_obj_element[i]->value_array != NULL )
    	{
			result = touchpad_write_i2c(client,g_tp->init_obj_element[i]->obj_addr,g_tp->init_obj_element[i]->value_array,g_tp->init_obj_element[i]->size);
			MY_INFO_PRINTK( 4, "TEST_INFO_LEVEL:" "write reg_check 0x%x = 0x%x\n",g_tp->init_obj_element[i]->obj_addr,g_tp->init_obj_element[i]->value_array[0] );
    		if( result )
    		{
    	  	 	MY_INFO_PRINTK( 0, "ERROR_LEVEL:" "unable to read reg_check 0x%x = 0x%x\n",g_tp->init_obj_element[i]->obj_addr, g_tp->init_obj_element[i]->value_array[0] );
    	   		PRINT_OUT
    	   		return result;
    		}
    	}
    }
    
    regbuf = maxTouchCfg_T6_obj.obj_addr+1;
    result = touchpad_write_i2c(client,regbuf,&value,WRITE_T6_SIZE);
    MY_INFO_PRINTK( 4, "TEST_INFO_LEVEL:" "software backup 0x%x = 0x%x\n",regbuf,value );
    if( result )
    {
		MY_INFO_PRINTK( 0, "ERROR_LEVEL:" "unable to write T6 backup byte 0x%x = 0x%x\n",regbuf,value );
    	PRINT_OUT
    	return result;
    }

    value = T6_RESET_VALUE;
    result = touchpad_write_i2c(client,maxTouchCfg_T6_obj.obj_addr,&value ,WRITE_T6_SIZE);
    MY_INFO_PRINTK( 4, "TEST_INFO_LEVEL:" "software reset 0x%x = 0x%x\n",maxTouchCfg_T6_obj.obj_addr,value );
    if( result )
    {
    	MY_INFO_PRINTK( 0, "ERROR_LEVEL:" "unable to software reset T6 object 0x%x = 0x%x\n",maxTouchCfg_T6_obj.obj_addr,value );
    	PRINT_OUT
    	return result;
    }    	
	msleep(100);
    PRINT_OUT    	
    return result;
}

#if 0
int print_mXT224_config(struct touchpad_t *g_tp)
{
	
	struct i2c_client *client = g_tp->client_4_i2c;
	int i,j;
	int result;
	uint8_t value;
	
	for ( i = 0 ; i < g_tp->id_info.num_obj_element ; i++ )
    {
    	if( g_tp->init_obj_element[i]->value_array != NULL )
    	{
			for (j = 0 ; j < g_tp->init_obj_element[i]->size; j++ )
			{
				result = touchpad_read_i2c(client,g_tp->init_obj_element[i]->obj_addr+j,&value,1);
				printk("read reg_check 0x%x = 0x%x\n",g_tp->init_obj_element[i]->obj_addr+j,value );
				MY_INFO_PRINTK( 4, "INFO_LEVEL:" "read reg_check 0x%x = 0x%x\n",g_tp->init_obj_element[i]->obj_addr+j,value );
				if( result )
				{
					MY_INFO_PRINTK( 0, "ERROR_LEVEL:" "unable to read reg_check 0x%x = 0x%x\n",g_tp->init_obj_element[i]->obj_addr+j,value );
					PRINT_OUT
					return result;
				}
			}	
    	}
    }
	return result;
}
#endif

static int copy_cando_config_to_obj_h(struct touchpad_t *g_tp)
{
	int result = 0;

    PRINT_IN
    memcpy(&maxTouchCfg_T38,&maxTouchCfg_T38_cando,(uint8_t)ARRAY_SIZE( maxTouchCfg_T38_cando ));
	memcpy(&maxTouchCfg_T7,&maxTouchCfg_T7_cando,(uint8_t)ARRAY_SIZE( maxTouchCfg_T7_cando ));
    memcpy(&maxTouchCfg_T8,&maxTouchCfg_T8_cando,(uint8_t)ARRAY_SIZE( maxTouchCfg_T8_cando ));
	memcpy(&maxTouchCfg_T9,&maxTouchCfg_T9_cando,(uint8_t)ARRAY_SIZE( maxTouchCfg_T9_cando ));
	memcpy(&maxTouchCfg_T15,&maxTouchCfg_T15_cando,(uint8_t)ARRAY_SIZE( maxTouchCfg_T15_cando ));
	memcpy(&maxTouchCfg_T18,&maxTouchCfg_T18_cando,(uint8_t)ARRAY_SIZE( maxTouchCfg_T18_cando ));
    memcpy(&maxTouchCfg_T19,&maxTouchCfg_T19_cando,(uint8_t)ARRAY_SIZE( maxTouchCfg_T19_cando ));
	memcpy(&maxTouchCfg_T20,&maxTouchCfg_T20_cando,(uint8_t)ARRAY_SIZE( maxTouchCfg_T20_cando ));
	memcpy(&maxTouchCfg_T22,&maxTouchCfg_T22_cando,(uint8_t)ARRAY_SIZE( maxTouchCfg_T22_cando ));
	memcpy(&maxTouchCfg_T23,&maxTouchCfg_T23_cando,(uint8_t)ARRAY_SIZE( maxTouchCfg_T23_cando ));
	memcpy(&maxTouchCfg_T24,&maxTouchCfg_T24_cando,(uint8_t)ARRAY_SIZE( maxTouchCfg_T24_cando ));
	memcpy(&maxTouchCfg_T25,&maxTouchCfg_T25_cando,(uint8_t)ARRAY_SIZE( maxTouchCfg_T25_cando )); 
	memcpy(&maxTouchCfg_T27,&maxTouchCfg_T27_cando,(uint8_t)ARRAY_SIZE( maxTouchCfg_T27_cando ));
	memcpy(&maxTouchCfg_T28,&maxTouchCfg_T28_cando,(uint8_t)ARRAY_SIZE( maxTouchCfg_T28_cando ));
	PRINT_OUT
    return result;
}

static int copy_tpk_config_to_obj_h(struct touchpad_t *g_tp)
{
	int result = 0;

    PRINT_IN
    memcpy(&maxTouchCfg_T38,&maxTouchCfg_T38_tpk,(uint8_t)ARRAY_SIZE( maxTouchCfg_T38_tpk ));
	memcpy(&maxTouchCfg_T7,&maxTouchCfg_T7_tpk,(uint8_t)ARRAY_SIZE( maxTouchCfg_T7_tpk ));
    memcpy(&maxTouchCfg_T8,&maxTouchCfg_T8_tpk,(uint8_t)ARRAY_SIZE( maxTouchCfg_T8_tpk ));
	memcpy(&maxTouchCfg_T9,&maxTouchCfg_T9_tpk,(uint8_t)ARRAY_SIZE( maxTouchCfg_T9_tpk ));
	memcpy(&maxTouchCfg_T15,&maxTouchCfg_T15_tpk,(uint8_t)ARRAY_SIZE( maxTouchCfg_T15_tpk ));
	memcpy(&maxTouchCfg_T18,&maxTouchCfg_T18_tpk,(uint8_t)ARRAY_SIZE( maxTouchCfg_T18_tpk ));
    memcpy(&maxTouchCfg_T19,&maxTouchCfg_T19_tpk,(uint8_t)ARRAY_SIZE( maxTouchCfg_T19_tpk ));
	memcpy(&maxTouchCfg_T20,&maxTouchCfg_T20_tpk,(uint8_t)ARRAY_SIZE( maxTouchCfg_T20_tpk ));
	memcpy(&maxTouchCfg_T22,&maxTouchCfg_T22_tpk,(uint8_t)ARRAY_SIZE( maxTouchCfg_T22_tpk ));
	memcpy(&maxTouchCfg_T23,&maxTouchCfg_T23_tpk,(uint8_t)ARRAY_SIZE( maxTouchCfg_T23_tpk ));
	memcpy(&maxTouchCfg_T24,&maxTouchCfg_T24_tpk,(uint8_t)ARRAY_SIZE( maxTouchCfg_T24_tpk ));
	memcpy(&maxTouchCfg_T25,&maxTouchCfg_T25_tpk,(uint8_t)ARRAY_SIZE( maxTouchCfg_T25_tpk )); 
	memcpy(&maxTouchCfg_T27,&maxTouchCfg_T27_tpk,(uint8_t)ARRAY_SIZE( maxTouchCfg_T27_tpk ));
	memcpy(&maxTouchCfg_T28,&maxTouchCfg_T28_tpk,(uint8_t)ARRAY_SIZE( maxTouchCfg_T28_tpk ));
	PRINT_OUT
    return result;
}

static int copy_tpk_EVT12_config_to_obj_h(struct touchpad_t *g_tp)
{
	int result = 0;

    PRINT_IN
    memcpy(&maxTouchCfg_T38,&maxTouchCfg_T38_tpk_EVT12,(uint8_t)ARRAY_SIZE( maxTouchCfg_T38_tpk_EVT12 ));
	memcpy(&maxTouchCfg_T7,&maxTouchCfg_T7_tpk_EVT12,(uint8_t)ARRAY_SIZE( maxTouchCfg_T7_tpk_EVT12 ));
    memcpy(&maxTouchCfg_T8,&maxTouchCfg_T8_tpk_EVT12,(uint8_t)ARRAY_SIZE( maxTouchCfg_T8_tpk_EVT12 ));
	memcpy(&maxTouchCfg_T9,&maxTouchCfg_T9_tpk_EVT12,(uint8_t)ARRAY_SIZE( maxTouchCfg_T9_tpk_EVT12 ));
	memcpy(&maxTouchCfg_T15,&maxTouchCfg_T15_tpk_EVT12,(uint8_t)ARRAY_SIZE( maxTouchCfg_T15_tpk_EVT12 ));
	memcpy(&maxTouchCfg_T18,&maxTouchCfg_T18_tpk_EVT12,(uint8_t)ARRAY_SIZE( maxTouchCfg_T18_tpk_EVT12 ));
    memcpy(&maxTouchCfg_T19,&maxTouchCfg_T19_tpk_EVT12,(uint8_t)ARRAY_SIZE( maxTouchCfg_T19_tpk_EVT12 ));
	memcpy(&maxTouchCfg_T20,&maxTouchCfg_T20_tpk_EVT12,(uint8_t)ARRAY_SIZE( maxTouchCfg_T20_tpk_EVT12 ));
	memcpy(&maxTouchCfg_T22,&maxTouchCfg_T22_tpk_EVT12,(uint8_t)ARRAY_SIZE( maxTouchCfg_T22_tpk_EVT12 ));
	memcpy(&maxTouchCfg_T23,&maxTouchCfg_T23_tpk_EVT12,(uint8_t)ARRAY_SIZE( maxTouchCfg_T23_tpk_EVT12 ));
	memcpy(&maxTouchCfg_T24,&maxTouchCfg_T24_tpk_EVT12,(uint8_t)ARRAY_SIZE( maxTouchCfg_T24_tpk_EVT12 ));
	memcpy(&maxTouchCfg_T25,&maxTouchCfg_T25_tpk_EVT12,(uint8_t)ARRAY_SIZE( maxTouchCfg_T25_tpk_EVT12 )); 
	memcpy(&maxTouchCfg_T27,&maxTouchCfg_T27_tpk_EVT12,(uint8_t)ARRAY_SIZE( maxTouchCfg_T27_tpk_EVT12 ));
	memcpy(&maxTouchCfg_T28,&maxTouchCfg_T28_tpk_EVT12,(uint8_t)ARRAY_SIZE( maxTouchCfg_T28_tpk_EVT12 ));
	PRINT_OUT
    return result;
}

static int copy_tpk_V20_config_to_obj_h(struct touchpad_t *g_tp)
{
	int result = 0;

    PRINT_IN
    memcpy(&maxTouchCfg_T38,&maxTouchCfg_T38_tpk_V20,(uint8_t)ARRAY_SIZE( maxTouchCfg_T38_tpk_V20 ));
	memcpy(&maxTouchCfg_T7,&maxTouchCfg_T7_tpk_V20,(uint8_t)ARRAY_SIZE( maxTouchCfg_T7_tpk_V20 ));
    memcpy(&maxTouchCfg_T8,&maxTouchCfg_T8_tpk_V20,(uint8_t)ARRAY_SIZE( maxTouchCfg_T8_tpk_V20 ));
	memcpy(&maxTouchCfg_T9,&maxTouchCfg_T9_tpk_V20,(uint8_t)ARRAY_SIZE( maxTouchCfg_T9_tpk_V20 ));
	memcpy(&maxTouchCfg_T15,&maxTouchCfg_T15_tpk_V20,(uint8_t)ARRAY_SIZE( maxTouchCfg_T15_tpk_V20 ));
	memcpy(&maxTouchCfg_T18,&maxTouchCfg_T18_tpk_V20,(uint8_t)ARRAY_SIZE( maxTouchCfg_T18_tpk_V20 ));
    memcpy(&maxTouchCfg_T19,&maxTouchCfg_T19_tpk_V20,(uint8_t)ARRAY_SIZE( maxTouchCfg_T19_tpk_V20 ));
	memcpy(&maxTouchCfg_T20,&maxTouchCfg_T20_tpk_V20,(uint8_t)ARRAY_SIZE( maxTouchCfg_T20_tpk_V20 ));
	memcpy(&maxTouchCfg_T22,&maxTouchCfg_T22_tpk_V20,(uint8_t)ARRAY_SIZE( maxTouchCfg_T22_tpk_V20 ));
	memcpy(&maxTouchCfg_T23,&maxTouchCfg_T23_tpk_V20,(uint8_t)ARRAY_SIZE( maxTouchCfg_T23_tpk_V20 ));
	memcpy(&maxTouchCfg_T24,&maxTouchCfg_T24_tpk_V20,(uint8_t)ARRAY_SIZE( maxTouchCfg_T24_tpk_V20 ));
	memcpy(&maxTouchCfg_T25,&maxTouchCfg_T25_tpk_V20,(uint8_t)ARRAY_SIZE( maxTouchCfg_T25_tpk_V20 )); 
	memcpy(&maxTouchCfg_T28,&maxTouchCfg_T28_tpk_V20,(uint8_t)ARRAY_SIZE( maxTouchCfg_T28_tpk_V20 ));
	PRINT_OUT
    return result;
}
static int copy_tpk_EVT12_V20_config_to_obj_h(struct touchpad_t *g_tp)
{
	int result = 0;

    PRINT_IN
    memcpy(&maxTouchCfg_T38,&maxTouchCfg_T38_tpk_EVT12_V20,(uint8_t)ARRAY_SIZE( maxTouchCfg_T38_tpk_EVT12_V20 ));
	memcpy(&maxTouchCfg_T7,&maxTouchCfg_T7_tpk_EVT12_V20,(uint8_t)ARRAY_SIZE( maxTouchCfg_T7_tpk_EVT12_V20 ));
    memcpy(&maxTouchCfg_T8,&maxTouchCfg_T8_tpk_EVT12_V20,(uint8_t)ARRAY_SIZE( maxTouchCfg_T8_tpk_EVT12_V20 ));
	memcpy(&maxTouchCfg_T9,&maxTouchCfg_T9_tpk_EVT12_V20,(uint8_t)ARRAY_SIZE( maxTouchCfg_T9_tpk_EVT12_V20 ));
	memcpy(&maxTouchCfg_T15,&maxTouchCfg_T15_tpk_EVT12_V20,(uint8_t)ARRAY_SIZE( maxTouchCfg_T15_tpk_EVT12_V20 ));
	memcpy(&maxTouchCfg_T18,&maxTouchCfg_T18_tpk_EVT12_V20,(uint8_t)ARRAY_SIZE( maxTouchCfg_T18_tpk_EVT12_V20 ));
    memcpy(&maxTouchCfg_T19,&maxTouchCfg_T19_tpk_EVT12_V20,(uint8_t)ARRAY_SIZE( maxTouchCfg_T19_tpk_EVT12_V20 ));
	memcpy(&maxTouchCfg_T20,&maxTouchCfg_T20_tpk_EVT12_V20,(uint8_t)ARRAY_SIZE( maxTouchCfg_T20_tpk_EVT12_V20 ));
	memcpy(&maxTouchCfg_T22,&maxTouchCfg_T22_tpk_EVT12_V20,(uint8_t)ARRAY_SIZE( maxTouchCfg_T22_tpk_EVT12_V20 ));
	memcpy(&maxTouchCfg_T23,&maxTouchCfg_T23_tpk_EVT12_V20,(uint8_t)ARRAY_SIZE( maxTouchCfg_T23_tpk_EVT12_V20 ));
	memcpy(&maxTouchCfg_T24,&maxTouchCfg_T24_tpk_EVT12_V20,(uint8_t)ARRAY_SIZE( maxTouchCfg_T24_tpk_EVT12_V20 ));
	memcpy(&maxTouchCfg_T25,&maxTouchCfg_T25_tpk_EVT12_V20,(uint8_t)ARRAY_SIZE( maxTouchCfg_T25_tpk_EVT12_V20 )); 
	memcpy(&maxTouchCfg_T28,&maxTouchCfg_T28_tpk_EVT12_V20,(uint8_t)ARRAY_SIZE( maxTouchCfg_T28_tpk_EVT12_V20 ));
	PRINT_OUT
    return result;
}

static int check_module_vendor(struct touchpad_t *g_tp)
{
	int result;
	uint8_t value;
	
	PRINT_IN
	g_tp->i2c_addr = 0x4A;
	result = touchpad_read_i2c( g_tp->client_4_i2c, ADD_ID_INFO_BLOCK, &value,1 );
	if(result == 0) 
	{
		printk( "I2C addr is 0X%x !!!\n",g_tp->i2c_addr );
		printk( "It's a tpk panle !!!\n" );
		if ((system_rev <= TOUCAN_EVT2_Band148) || system_rev == HWID_UNKNOWN)
		{
			copy_tpk_EVT12_config_to_obj_h(g_tp);
			PRINT_OUT
			return 0;
		}
		else
		{
			MY_INFO_PRINTK( 1, "INFO_LEVEL:" "HW ID is used in EVT3 !!!\n" );
			copy_tpk_config_to_obj_h(g_tp);
			PRINT_OUT
			return 0;
		}
	}	
	else 
	{
		g_tp->i2c_addr = 0x4B;
		result = touchpad_read_i2c( g_tp->client_4_i2c, ADD_ID_INFO_BLOCK, &value,1 );
		if (result == 0)
		{
			printk( "I2C addr is 0X%x !!!\n",g_tp->i2c_addr );
			printk( "It's a cando panle !!!\n" );
			copy_cando_config_to_obj_h(g_tp);
			PRINT_OUT
			return 0;
		}
		else
		{
			MY_INFO_PRINTK( 0, "ERROR_LEVEL:" "Neither tpk panel nor cando panel is !!!\n" );
			PRINT_OUT
			return result;
		}
	}
	PRINT_OUT
    return 0;
}

#define READ_MEM_OK                 1u
#define READ_MEM_FAILED             2u
#define WRITE_MEM_OK                1u
#define WRITE_MEM_FAILED            2u
#define QT_WAITING_BOOTLOAD_COMMAND 0xC0
#define QT_WAITING_FRAME_DATA       0x80
#define QT_FRAME_CRC_CHECK          0x02
#define QT_FRAME_CRC_PASS           0x04
#define QT_FRAME_CRC_FAIL           0x03
#define QT_APP_CRC_FAIL 			0x40

uint8_t boot_read_mem(uint16_t start, uint8_t size, uint8_t *mem)
{
    int     result;
    struct  i2c_msg msgs[] = { 
        [0] = {                     
            .addr   = 0x24,
            .flags  = I2C_M_RD,
            .buf    = (void *)mem,
            .len    = size
        }
    };
    
	PRINT_IN
    result = i2c_transfer( g_tp->client_4_i2c->adapter, msgs, ARRAY_SIZE(msgs) );
    if( result != ARRAY_SIZE(msgs) )
    {
        MY_INFO_PRINTK( 0,"ERROR_LEVEL:""%s fail\n", __func__);
        PRINT_OUT
        return READ_MEM_FAILED;
    }
	PRINT_OUT
    return READ_MEM_OK;
}

int boot_qt602240_i2c_write(u16 reg, u8 *read_val, unsigned int len)
{
	struct i2c_msg wmsg;
	unsigned char data[300];
	int ret,i;

	if(len+2 > 300)
	{
		printk("[TSP][ERROR] %s() data length error\n", __FUNCTION__);
		return -ENODEV;
	}

	wmsg.addr = 0x24;
	wmsg.flags = 0;
	wmsg.len = len;
	wmsg.buf = data;


	for (i = 0; i < len; i++)
	{
		data[i] = *(read_val+i);
	}

	ret = i2c_transfer(g_tp->client_4_i2c->adapter, &wmsg, 1);
	return ret;
}

static uint8_t boot_write_mem(uint16_t start, uint16_t size, uint8_t *mem)
{
	int ret;

	ret = boot_qt602240_i2c_write(start,mem,size);
	if(ret < 0){
		printk("boot write mem fail: %d \n",ret);
		return(WRITE_MEM_FAILED);
	}
	else
		return(WRITE_MEM_OK);
}
#if 0
static int reset_and_test(void)
{	
	uint8_t value,boot_status;
	int result;
	int i;
	
	value = 0xA5;
    result = touchpad_write_i2c(g_tp->client_4_i2c,0xFB,&value ,1);
    printk("software reset 0x%x\n",value );
    if( result )
    {
    	MY_INFO_PRINTK( 0, "ERROR_LEVEL:" "unable to software reset T6 object  0x%x\n",value );
    	return result;
    } 
	
	msleep(100);
	
	for(i=0;i<10;i++)
	{
		result = boot_read_mem(0,1,&boot_status);
		if(result==READ_MEM_FAILED)
		{
			printk("fail to read boot_status\n");
			msleep(100);
		}
		else
		{
			printk("boot_status=%x\n",boot_status);
			break;
		}
	}
	
	for(i=0;i<10;i++)
	{
		result = boot_write_mem(0,1,&boot_status);
		if(result == WRITE_MEM_FAILED)
		{
			printk("fail to set back to normal mode\n");
			msleep(100);
		}
		else
		{
			printk("set back to normal mode\n");
			break;
		}
	}
	msleep(100);
	
	return 0;
}
#endif
static uint8_t write_mem(uint16_t start, uint8_t size, uint8_t *mem)
{
	int ret;

	ret = touchpad_write_i2c(g_tp->client_4_i2c,start,mem,size);
	if(ret) 
		return(WRITE_MEM_FAILED);
	else
		return(WRITE_MEM_OK);
}

static uint8_t boot_unlock(void)
{

	int ret;
	unsigned char data[2];

	data[0] = 0xDC;
	data[1] = 0xAA;
	
	ret = boot_write_mem(0,2,(uint8_t*)data);
	if(ret == WRITE_MEM_FAILED) {
		printk("%s : i2c write failed\n",__func__);
		return(WRITE_MEM_FAILED);
	} 

	return(WRITE_MEM_OK);

}

static int is_bl_mode(void)
{
	struct i2c_client *client = g_tp->client_4_i2c;
	uint8_t boot_status;
	
	g_tp->i2c_addr = 0x24;
	if(touchpad_read_i2c( client, 0, &boot_status,1 ) == 0)
	{
		printk("boot_status is 0x%x\n",boot_status);
		printk("touch ic is in bl mode\n");
		g_tp->i2c_addr = 0x4A;
		return 1;
	}
	else
	{
		printk("touch ic is NOT in bl mode\n");
		g_tp->i2c_addr = 0x4A;
		return 0;
	}
}

uint8_t QT_Boot(void)
{
		unsigned char boot_status;
		unsigned char boot_ver;
		unsigned char retry_cnt;
		unsigned long int character_position;
		unsigned int frame_size;
		unsigned int next_frame;
		unsigned int crc_error_count;
		unsigned int size1,size2;
		unsigned int j,read_status_flag;
		uint8_t data = 0xA5;
		unsigned int last_frame = 0;
		uint8_t reset_result = 0;
	
		unsigned char  *firmware_data ;
	
		firmware_data = QT602240_firmware;
	
		crc_error_count = 0;
		character_position = 0;
		next_frame = 0;
	
		reset_result = write_mem(0xFB, 1, &data);
	
	
		if(reset_result != WRITE_MEM_OK)
		{
			for(retry_cnt =0; retry_cnt < 3; retry_cnt++)
			{
				msleep(100);
				reset_result = write_mem(0xFB, 1, &data);
				if(reset_result == WRITE_MEM_OK)
				{
					break;
				}
			}
	
		}
		if (reset_result == WRITE_MEM_OK)
			printk("Boot reset OK\r\n");

		msleep(100);
	
		for(retry_cnt = 0; retry_cnt < 30; retry_cnt++)
		{
			
			if( (boot_read_mem(0,1,&boot_status) == READ_MEM_OK) && (boot_status & 0xC0) == 0xC0) 
			{
				boot_ver = boot_status & 0x3F;
				crc_error_count = 0;
				character_position = 0;
				next_frame = 0;

				while(1)
				{ 
					for(j = 0; j<5; j++)
					{
						msleep(60);
						if(/*is_chg_low() &&*/ boot_read_mem(0,1,&boot_status) == READ_MEM_OK)
						{
							read_status_flag = 1;
							break;
						}
						else
						{
							printk("reading boot status\n");
							read_status_flag = 0;
						}
					}
					
					if(read_status_flag==1)	
					{
						retry_cnt  = 0;
						printk("TSP boot status is %x		stage 2 \n", boot_status);
						if((boot_status & QT_WAITING_BOOTLOAD_COMMAND) == QT_WAITING_BOOTLOAD_COMMAND)
						{
							if(boot_unlock() == WRITE_MEM_OK)
							{
								msleep(10);
			
								printk("Unlock OK\n");
			
							}
							else
							{
								printk("Unlock fail\n");
							}
						}
						else if((boot_status & 0xC0) == QT_WAITING_FRAME_DATA)
						{
								/* Add 2 to frame size, as the CRC bytes are not included */
								size1 =  *(firmware_data+character_position);
								size2 =  *(firmware_data+character_position+1)+2;
								frame_size = (size1<<8) + size2;
				
								printk("Frame size:%d\n", frame_size);
								printk("Firmware pos:%ld\n", character_position);
								/* Exit if frame data size is zero */
								if( 0 == frame_size )
								{
									printk("0 == frame_size\n");
									return 1;
								}
								printk("len[0]=%x len[1]=%x\n",*(firmware_data +character_position),*(firmware_data +character_position+1));
								next_frame = 1;
								boot_write_mem(0,frame_size, (firmware_data +character_position));
								msleep(10);
								printk(".");
								if(frame_size+character_position >= sizeof(QT602240_firmware))
								{
									printk("frame_size+character_position=%ld sizeof(QT602240_firmware)=%d\n",frame_size+character_position,sizeof(QT602240_firmware));
									last_frame = 1;
								}
						}
						else if(boot_status == QT_FRAME_CRC_CHECK)
						{
							printk("CRC Check\n");
						}
						else if(boot_status == QT_FRAME_CRC_PASS)
						{
							if( next_frame == 1)
							{
								printk("CRC Ok\n");
								character_position += frame_size;
								next_frame = 0;
							}
							else {
								printk("next_frame != 1\n");
							}
							if(last_frame) 
							{
								msleep(500);
								printk("quit QT_BOOT\n");
								return 0;
							}
						}
						else if(boot_status  == QT_FRAME_CRC_FAIL)
						{
							printk("CRC Fail\n");
							crc_error_count++;
						}
						if(crc_error_count > 10)
						{
							return QT_FRAME_CRC_FAIL;
						}
			
					}
					else
					{
						return (0);
					}
				}
			}
			else
			{
				printk("[TSP] read_boot_state() or (boot_status & 0xC0) == 0xC0) is fail!!!\n");
			}
		}
		
		printk("QT_Boot end \n");
		return (0);
}


static int touchpad_detect_mXT224(struct touchpad_t *g_tp)
{
	int result;
    uint8_t value[NUM_OF_ID_INFO_BLOCK];
    uint16_t regbuf,obj_addr;
    int i = 0;
    int t = 0;
    uint8_t reportid_ub = 1;
    	
    PRINT_IN
	result = check_module_vendor(g_tp);
	if(/*ftm_mode != 1 &&*/ is_bl_mode())
	{
		QT_Boot();
	}
detect_again:
    result = touchpad_read_i2c( g_tp->client_4_i2c, ADD_ID_INFO_BLOCK, &value[0],NUM_OF_ID_INFO_BLOCK );
    if(result || !(value[0] == 0x80 && (value[2] == 0x20 || value[2] == 0x16 || value[2] == 0xF6) && (value[3] == 0xAA || value[3] == 0xAB)))
    {
        MY_INFO_PRINTK( 0, "ERROR_LEVEL:" "unable to read reg 0xh and value failed 0x%x, return %d\n",value[i],result );
        PRINT_OUT
        return result;
    }
    g_tp->id_info.family_id = value[0];
    g_tp->id_info.variant_id = value[1];
    g_tp->id_info.version = value[2];
    g_tp->id_info.build = value[3];
    g_tp->id_info.matrix_x_size = value[4];
    g_tp->id_info.matrix_y_size = value[5];
    g_tp->id_info.num_obj_element = value[6];
	printk("ATMEL touch fimrware version : 0x%x\n",g_tp->id_info.version);

	if(g_tp->id_info.version == 0x16)
	{
		QT_Boot();
		goto detect_again;
	}
	if(g_tp->id_info.version == 0x20 && g_tp->i2c_addr == 0x4A)
	{
		if(system_rev <= TOUCAN_EVT2_Band148)
		{
			copy_tpk_EVT12_V20_config_to_obj_h(g_tp);
		}
		else
		{
			copy_tpk_V20_config_to_obj_h(g_tp);
		}
	}
    for ( regbuf = ADD_ID_INFO_BLOCK; regbuf< NUM_OF_ID_INFO_BLOCK; regbuf++ )
    {
    	MY_INFO_PRINTK( 4, "TEST_INFO_LEVEL:" "read 0x%xh = 0x%x\n",regbuf, value[i] );
    	i++;
    }
   
    MY_INFO_PRINTK( 4, "TEST_INFO_LEVEL:" "read num_obj_element = %d\n",g_tp->id_info.num_obj_element );
    
    for ( obj_addr = OBJECT_TABLE_ELEMENT_1 ; obj_addr < OBJECT_TABLE_ELEMENT_1+OBJECT_TABLE_ELEMENT_SIZE*g_tp->id_info.num_obj_element ; obj_addr += OBJECT_TABLE_ELEMENT_SIZE )
    {
    	result = touchpad_read_i2c( g_tp->client_4_i2c, obj_addr, &value[0], OBJECT_TABLE_ELEMENT_SIZE );
    	MY_INFO_PRINTK( 4, "TEST_INFO_LEVEL:" "read 0x%x = 0x%x\n",obj_addr, value[0] );
    	if( result )
    	{
        	MY_INFO_PRINTK( 0, "ERROR_LEVEL:" "unable to read reg 0x%x and value failed 0x%x, return %d\n",obj_addr, value[0],result );
        	PRINT_OUT
        	return result;
    	}
        
    	if ( t < MAX_OBJ_ELEMENT_SIZE )	
    	{	
        	fillin_init_obj_table( GEN_MESSAGEPROCESOR_T5, &maxTouchCfg_T5_obj, value, &reportid_ub,
        	                       	obj_addr, 0, &t );
        	fillin_init_obj_table( GEN_COMMANDPROCESSOR_T6, &maxTouchCfg_T6_obj, value, &reportid_ub,
        	                        obj_addr, 0, &t );
        	fillin_init_obj_table( SPT_USERDATA_T38, &maxTouchCfg_T38_obj, value, &reportid_ub,
        		                    obj_addr, (uint8_t)ARRAY_SIZE( maxTouchCfg_T38 ), &t );        
        	fillin_init_obj_table( GEN_POWERCONFIG_T7, &maxTouchCfg_T7_obj, value, &reportid_ub,
        		                    obj_addr, (uint8_t)ARRAY_SIZE( maxTouchCfg_T7 ), &t );
			if(g_tp->id_info.version == 0x20)
			{
				fillin_init_obj_table( GEN_ACQUISITIONCONFIG_T8, &maxTouchCfg_T8_obj, value, &reportid_ub,
										obj_addr, (uint8_t)ARRAY_SIZE( maxTouchCfg_T8 ), &t );
				fillin_init_obj_table( TOUCH_MULTITOUCHSCREEN_T9, &maxTouchCfg_T9_obj, value, &reportid_ub,
											obj_addr, (uint8_t)ARRAY_SIZE( maxTouchCfg_T9 ), &t );
			}
			else
			{
				fillin_init_obj_table( GEN_ACQUISITIONCONFIG_T8, &maxTouchCfg_T8_obj, value, &reportid_ub,
										obj_addr, (uint8_t)ARRAY_SIZE( maxTouchCfg_T8_tpk ), &t );
				fillin_init_obj_table( TOUCH_MULTITOUCHSCREEN_T9, &maxTouchCfg_T9_obj, value, &reportid_ub,
										obj_addr, (uint8_t)ARRAY_SIZE( maxTouchCfg_T9_tpk ), &t );			
			}
        	fillin_init_obj_table( TOUCH_KEYARRAY_T15, &maxTouchCfg_T15_obj, value, &reportid_ub,
        		                    obj_addr, (uint8_t)ARRAY_SIZE( maxTouchCfg_T15 ), &t );
        	fillin_init_obj_table( SPT_COMMSCONFIG_T18, &maxTouchCfg_T18_obj, value,  &reportid_ub,
        		                    obj_addr, (uint8_t)ARRAY_SIZE( maxTouchCfg_T18 ), &t );
        	fillin_init_obj_table( SPT_GPIOPWM_T19, &maxTouchCfg_T19_obj, value, &reportid_ub,
        		                    obj_addr, (uint8_t)ARRAY_SIZE( maxTouchCfg_T19 ), &t );
        	fillin_init_obj_table( PROCI_GRIPFACESUPPRESSION_T20, &maxTouchCfg_T20_obj, value, &reportid_ub,
        		                    obj_addr, (uint8_t)ARRAY_SIZE( maxTouchCfg_T20 ), &t );
        	fillin_init_obj_table( PROCG_NOISESUPPRESSION_T22, &maxTouchCfg_T22_obj, value, &reportid_ub,
        		                    obj_addr, (uint8_t)ARRAY_SIZE( maxTouchCfg_T22 ), &t );
        	fillin_init_obj_table( TOUCH_PROXIMITY_T23, &maxTouchCfg_T23_obj, value, &reportid_ub,
        		                    obj_addr, (uint8_t)ARRAY_SIZE( maxTouchCfg_T23 ), &t );
        	fillin_init_obj_table( PROCI_ONETOUCHGESTUREPROCESSOR_T24, &maxTouchCfg_T24_obj, value, &reportid_ub,
        		                    obj_addr, (uint8_t)ARRAY_SIZE( maxTouchCfg_T24 ), &t );
        	fillin_init_obj_table( SPT_SELFTEST_T25, &maxTouchCfg_T25_obj, value, &reportid_ub,
        		                    obj_addr, (uint8_t)ARRAY_SIZE( maxTouchCfg_T25 ), &t );
        	fillin_init_obj_table( PROCI_TWOTOUCHGESTUREPROCESSOR_T27, &maxTouchCfg_T27_obj, value, &reportid_ub,
        		                    obj_addr, (uint8_t)ARRAY_SIZE( maxTouchCfg_T27 ), &t );
        	fillin_init_obj_table( SPT_CTECONFIG_T28, &maxTouchCfg_T28_obj, value, &reportid_ub,
        		                    obj_addr, (uint8_t)ARRAY_SIZE( maxTouchCfg_T28 ), &t );
        	fillin_init_obj_table( DEBUG_DIAGNOSTIC_T37, &maxTouchCfg_T37_obj, value, &reportid_ub,
        	                        obj_addr, 0, &t );	
			fillin_init_obj_table( SPT_MESSAGECOUNT_T44, &maxTouchCfg_T44_obj, value, &reportid_ub,
        	                        obj_addr, 0, &t );	
        } 
        else
        {
			MY_INFO_PRINTK( 0, "ERROR_LEVEL:" "touchpad_detect_mXT224: failed to fille in  obj table , it's overflow\n" );
        }
	}
	PRINT_OUT
    return 0;
}

static int power_on_flow_another_way(struct touchpad_t *g_tp)
{
	int result;
	int obj_size = (int)( maxTouchCfg_T5_obj.size );
	uint8_t value[obj_size];
	uint16_t regbuf;
	int i;
	int j = 0;
	
	PRINT_IN
	MY_INFO_PRINTK( 4, "TEST_INFO_LEVEL:" "read obj size :%d\n", obj_size )
	
	result = touchpad_config_mXT224(g_tp);
	if( result )
    {
        MY_INFO_PRINTK( 0, "ERROR_LEVEL:" "touchpad_probe: failed to config mXT224 (result=%d)\n",result );
        PRINT_OUT
        return result;
    }
		
    result = gpio_get_value(g_tp->gpio_num);
    if( result )
    {
        MY_INFO_PRINTK( 0, "ERROR_LEVEL:" " the CHG pin is not low.)\n" );
		MY_INFO_PRINTK( 0, "ERROR_LEVEL:" "touchpad_probe: failed to get gpio_num value %d (result=%d)\n",g_tp->gpio_num,result );
        PRINT_OUT
        return result;
    }
    	
    for (;;)
	{
		touchpad_read_i2c( g_tp->client_4_i2c, maxTouchCfg_T5_obj.obj_addr, &value[0],obj_size );
    	i = 0;
    	for ( regbuf = maxTouchCfg_T5_obj.obj_addr; regbuf < maxTouchCfg_T5_obj.obj_addr+maxTouchCfg_T5_obj.size; regbuf++ )
    	{
    		MY_INFO_PRINTK( 4, "INFO_LEVEL:" "read message & cmd obj 0x%xh = 0x%x\n",regbuf, value[i] );
    		i++;
    	}
    	if( value[0] >= maxTouchCfg_T6_obj.reportid_ub && value[0] <= maxTouchCfg_T6_obj.reportid_lb )
    	{
    		if( (value[1] & CFGERR_MASK) != 0x0 )
    		{
    			MY_INFO_PRINTK( 0, "ERROR_LEVEL:" "failed to  CFGERR bit : 0x%x  \n", value[1] );
    			PRINT_OUT
    			return -EFAULT;
        	}
        	else
        	{
        		MY_INFO_PRINTK( 4, "INFO_LEVEL:" "check CFGERR  0x%x\n", value[1] );
    			g_tp->T6_config_checksum = value[2] | value[3]<< 8 | value[4]<<16;
    			MY_INFO_PRINTK( 4, "INFO_LEVEL:" "T6 config checksum : 0x%x\n",g_tp->T6_config_checksum );
    			result = calculate_config_crc(g_tp);
    			if ( result )
    			{
    				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to calculate config crc : %d\n", result );
    				PRINT_OUT
    				return -EFAULT;
    			}
    			break;
        	}
    	}
    	else
    	{
    		if ( j > 10 )
    		{
    			MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to read T6 message\n" );
    			PRINT_OUT
    			return -EFAULT;
    		}
    		j++;	
    	}	
	}
    	PRINT_OUT
    	return 0;
}

static int power_on_flow_one_way(struct touchpad_t *g_tp)
{
	int result;
	int obj_size = (int)( maxTouchCfg_T5_obj.size );
	uint8_t value[obj_size];
	uint16_t regbuf;
	int i;
	int j = 0;
	
	PRINT_IN
	MY_INFO_PRINTK( 4, "TEST_INFO_LEVEL:" "read obj size :%d\n", obj_size )
	for (;;)
	{
		touchpad_read_i2c( g_tp->client_4_i2c, maxTouchCfg_T5_obj.obj_addr, &value[0],obj_size ); 
    	i = 0;
    	
    	for ( regbuf = maxTouchCfg_T5_obj.obj_addr; regbuf < maxTouchCfg_T5_obj.obj_addr+maxTouchCfg_T5_obj.size; regbuf++ )
    	{
    		MY_INFO_PRINTK( 4, "INFO_LEVEL:" "read message & cmd obj 0x%xh = 0x%x\n",regbuf, value[i] );
    		i++;
    	}
    	if( value[0] >= maxTouchCfg_T6_obj.reportid_ub && value[0] <= maxTouchCfg_T6_obj.reportid_lb )
    	{
    		if( (value[1] & CFGERR_MASK) != 0x0 )
    		{
    			MY_INFO_PRINTK( 0, "ERROR_LEVEL:" "failed to  CFGERR bit : 0x%x  \n", value[1] );
        		result = power_on_flow_another_way(g_tp);
        		if ( result )
    			{
    				MY_INFO_PRINTK( 0,"ERROR_LEVEL:""failed to power on flow another way : 0x%x  \n", result );
    				PRINT_OUT
    				return result;
    			}	
				PRINT_OUT
    			return result;
        	}
        	else
        	{
        		MY_INFO_PRINTK( 4, "INFO_LEVEL:" "check CFGERR  0x%x\n", value[1] );
    			g_tp->T6_config_checksum = value[2] | value[3]<< 8 | value[4]<<16;
    			MY_INFO_PRINTK( 4, "INFO_LEVEL:" "T6 config checksum : 0x%x\n",g_tp->T6_config_checksum );				
    			result = calculate_config_crc(g_tp);
    			if ( result )
    			{
    				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to calculate config crc : 0x%x\n", result );
    				result = power_on_flow_another_way(g_tp);
    				    if ( result )
    				    {
    				    	MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to power on flow another way : 0x%x\n", result );
    				    	PRINT_OUT
    				    	return result;
    				    }
    				    PRINT_OUT
    				    return result;
    			}
    			break;
        	}
    	}
    	else
    	{
    		if ( j > 10 )
    		{
    			MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to read T6 message\n" );
    			PRINT_OUT
    			return -EFAULT;
    		}
    		j++;
    	}
	}
    	PRINT_OUT
    	return 0;
}

static int tp_selftest_fvs(struct touchpad_t *g_tp)
{
	int  result = 0;
	struct i2c_client *client = g_tp->client_4_i2c;
	uint8_t value;
	
	PRINT_IN
	if (g_tp->selftest_flag == 1)
	{
		value = 0xFE;
		result = touchpad_write_i2c( client, maxTouchCfg_T25_obj.obj_addr+1, &value, 1 );
		if (result)
		{
			MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to write References Mode!\n" );
			result = -EFAULT;
			return result;
		}
		msleep(2000);
	}
	else
	{
		value = maxTouchCfg_T25_obj.value_array[1];
		result = touchpad_write_i2c( client, maxTouchCfg_T25_obj.obj_addr+1, &value, 1 );
		if (result)
		{
			MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to write References Mode!\n" );
			result = -EFAULT;
			return result;
		}
	}
	
	PRINT_OUT
    return 0;
    
}

static ssize_t tp_misc_write( struct file *fp,
                              const char __user *buffer,
                              size_t count,
                              loff_t *ppos )
{
	char echostr[ECHOSTR_SIZE];
    uint8_t value_rek_thrld;
	int result;
	
	PRINT_IN
	//printk("%s +++ \n",__func__);
	if ( count > ECHOSTR_SIZE )
    {
        MY_INFO_PRINTK( 0, "ERROR_LEVEL:" "ts_misc_write: invalid count %d\n", count );
        return -EINVAL;
    }
    
	if ( copy_from_user(echostr,buffer,count) )
	{
		MY_INFO_PRINTK( 0, "ERROR_LEVEL:" "copy Echo String from user failed: %s\n",echostr);
		return -EINVAL;
	}
	mutex_lock(&g_tp->mutex);
	echostr[count-1]='\0';
	if (g_tp->touch_suspended == 0)
	{
		if ( strcmp(echostr,"touch re k" ) == 0 )
		{
			MY_INFO_PRINTK( 1, "INFO_LEVEL:" "User Input Echo String : %s\n",echostr);
			value_rek_thrld = 0xF;
			result = touchpad_write_i2c( g_tp->client_4_i2c, maxTouchCfg_T8_obj.obj_addr+8, &value_rek_thrld, 1 );
			if (result)
			{
				MY_INFO_PRINTK( 0, "ERROR_LEVEL:" "cannot write a calibrate thrld filed in T8 object!!!\n" );
				mutex_unlock(&g_tp->mutex);
				return -EINVAL;
			}
			printk("%s addr : %x value_rek_thrld : %x\n",__func__,maxTouchCfg_T8_obj.obj_addr+8,value_rek_thrld);
		}
	}	
	else
	{
		MY_INFO_PRINTK(0, "ERROR_LEVEL:" "device is not screen on.\n");
	}
	//printk("%s --- \n",__func__);
	mutex_unlock(&g_tp->mutex);
	PRINT_OUT
    return count;
}
static long tp_misc_ioctl( struct file *fp,
                           unsigned int cmd,
                           unsigned long arg )
{	
	struct i2c_client *client = g_tp->client_4_i2c;
    struct atmel_sensitivity_t sensitivity;
	struct atmel_power_mode_t power_mode;
	struct atmel_linerity_t linerity;
	struct atmel_merge_t merge;
	struct atmel_noise_suppression_t noise_suppression;
	struct atmel_references_mode_t references_mode;
	struct atmel_deltas_mode_t deltas_mode;
	struct atmel_gain_t gain;
	struct atmel_cte_mode_t cte_mode;
	struct atmel_acquisition_t acquisition;
	struct atmel_keyarray_t keyarray;
	struct atmel_multitouchscreen_t multitouchscreen;
	struct atmel_selftest_t local_selftest;
	struct atmel_grip_face_suppression_t grip_face_suppression;
	
	int  result = 0;
	uint8_t value[26];
	uint8_t value_cte[18];
	uint8_t value_backup[4];
    uint16_t addr = 0;
	uint16_t addr_key = 0;
	uint16_t addr_T37 = 0;
	uint16_t addr_cte = 0;
	uint16_t addr_cmd = 0;
	uint16_t addr_diagnostic = 0;
	uint16_t addr_T6 = 0;
    uint8_t *pData = NULL;
    uint    length = 0;
	int     i = 0;
	
	PRINT_IN
	MY_INFO_PRINTK( 1,"INFO_LEVEL:" "cmd number=%d\n", _IOC_NR(cmd) );
	switch(cmd)
    {
		case ATMEL_TOUCH_IOCTL_SET_SENSITIVITY:
            addr = maxTouchCfg_T9_obj.obj_addr+7;
			addr_key = maxTouchCfg_T15_obj.obj_addr+7;
            pData = (void *)&sensitivity;
            length = sizeof(sensitivity);
            if( copy_from_user( (void *)pData,
                                (void *)arg,
                                length) )
            {
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "copy SET_SENSITIVITY from user failed!\n" );
                result = -EFAULT;
				return result;
            }
			else
			{
                value[0] = sensitivity.touch_threshold;
				value[1] = sensitivity.key_threshold;
				result = touchpad_write_i2c( client, addr, &value[0], 1 );
				if (result)
				{
					MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to write SET_TOUCH_SENSITIVITY!\n" );
					result = -EFAULT;
					return result;
				}
				MY_INFO_PRINTK( 4,"INFO_LEVEL:" "SET_TOUCH_SENSITIVITY: %x\n", value[0]);
				result = touchpad_write_i2c( client, addr_key, &value[1], 1 );
				if (result)
				{
					MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to write SET_KEY_SENSITIVITY!\n" );
					result = -EFAULT;
					return result;
				}
				MY_INFO_PRINTK( 4,"INFO_LEVEL:" "SET_KEY_SENSITIVITY: %x\n", value[1]);
            }
            break;
		case ATMEL_TOUCH_IOCTL_GET_SENSITIVITY:
			addr = maxTouchCfg_T9_obj.obj_addr+7;
			addr_key = maxTouchCfg_T15_obj.obj_addr+7;
            pData = (void *)&sensitivity;
            length = sizeof(sensitivity);
            result = touchpad_read_i2c( client, addr, &value[0], 1 );
			if (result)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to read GET_TOUCH_SENSITIVITY!\n" );
                result = -EFAULT;
				return result;
			}
			result = touchpad_read_i2c( client, addr_key, &value[1], 1 );
			if (result)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to read GET_KEY_SENSITIVITY!\n" );
                result = -EFAULT;
				return result;
			}
			result = copy_to_user( (void *)arg,&value[0],2 );
			if (result)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "copy GET_TOUCH_SENSITIVITY to user failed!\n" );
                result = -EFAULT;
				return result;
			}
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "GET_TOUCH_SENSITIVITY: %x\n",value[0] );
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "GET_KEY_SENSITIVITY: %x\n",value[1] );
            break;
		case ATMEL_TOUCH_IOCTL_SET_POWER_MODE:
            addr = maxTouchCfg_T7_obj.obj_addr;
            pData = (void *)&power_mode;
            length = sizeof(power_mode);
            if( copy_from_user( (void *)pData,
                                (void *)arg,
                                length) )
            {
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "copy power_mode from user failed!\n" );
                result = -EFAULT;
				return result;
                break;
            }
            else
			{
				value[0] = power_mode.idleacqint;
				value[1] = power_mode.actvacqint;
				value[2] = power_mode.actv2idleto;
				result = touchpad_write_i2c( client, addr, &value[0], 3 );
				if (result)
				{
					MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to write SET_POWER_MODE!\n" );
					result = -EFAULT;
					return result;
				}
				MY_INFO_PRINTK( 4,"INFO_LEVEL:" "SET_POWER_MODE: idleacqint %x\n", value[0]);
				MY_INFO_PRINTK( 4,"INFO_LEVEL:" "SET_POWER_MODE: actvacqint %x\n", value[1]);
				MY_INFO_PRINTK( 4,"INFO_LEVEL:" "SET_POWER_MODE: actv2idleto %x\n", value[2]);
			}
            break;
        case ATMEL_TOUCH_IOCTL_GET_POWER_MODE:
            addr = maxTouchCfg_T7_obj.obj_addr;
            pData = (void *)&power_mode;
            length = sizeof(power_mode);
            result = touchpad_read_i2c( client, addr, &value[0], 3 );
			if (result)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to read GET_POWER_MODE!\n" );
                result = -EFAULT;
				return result;
			}
			result = copy_to_user( (void *)arg,&value[0],3 );
			if (result)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "copy GET_POWER_MODE to user failed!\n" );
                result = -EFAULT;
				return result;
			}
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "value[1]: %x\n",value[0] );
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "value[2]: %x\n",value[1] );
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "value[3]: %x\n",value[2] );
            break;
		case ATMEL_TOUCH_IOCTL_SET_LINERITY:
            addr = maxTouchCfg_T9_obj.obj_addr+11;
            pData = (void *)&linerity;
            length = sizeof(linerity);
            if( copy_from_user( (void *)pData,
                                (void *)arg,
                                length) )
            {
                MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "copy linerity from user failed!\n" );
                result = -EFAULT;
				return result;
            }
			else
			{
                value[0] = linerity.movhysti;
				value[1] = linerity.movhystn;
				result = touchpad_write_i2c( client, addr, &value[0], 2 );
				if (result)
				{
					MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to write SET_LINERITY!\n" );
					result = -EFAULT;
					return result;
				}
            }
            break;
        case ATMEL_TOUCH_IOCTL_GET_LINERITY:
            addr = maxTouchCfg_T9_obj.obj_addr+11;
            pData = (void *)&linerity;
            length = sizeof(linerity);
            result = touchpad_read_i2c( client, addr, &value[0], 2 );
			if (result)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to read GET_LINERITY!\n" );
                result = -EFAULT;
				return result;
			}
			result = copy_to_user( (void *)arg,&value[0],2 );
			if (result)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "copy GET_LINERITY to user failed!\n" );
                result = -EFAULT;
				return result;
			}
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "value[4]: %x\n",value[0] );
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "value[5]: %x\n",value[1] );
            break;
		case ATMEL_TOUCH_IOCTL_SET_MERGE:
            addr = maxTouchCfg_T9_obj.obj_addr+15;
            pData = (void *)&merge;
            length = sizeof(merge);
            if( copy_from_user( (void *)pData,
                                (void *)arg,
                                length) )
            {
                MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "copy merge from user failed!\n" );
                result = -EFAULT;
				return result;
            }
			else
			{
                value[0] = merge.mrghyst;
				value[1] = merge.mrgthr;
				result = touchpad_write_i2c( client, addr, &value[0], 2 );
				if (result)
				{
					MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to write SET_MERGE!\n" );
					result = -EFAULT;
					return result;
				}
				MY_INFO_PRINTK( 4,"INFO_LEVEL:" "SET_MERGE: mrghyst %x\n", value[0]);
				MY_INFO_PRINTK( 4,"INFO_LEVEL:" "SET_MERGE: mrgthr %x\n", value[1]);
            }
            break;
        case ATMEL_TOUCH_IOCTL_GET_MERGE:
            addr = maxTouchCfg_T9_obj.obj_addr+15;
            pData = (void *)&merge;
            length = sizeof(merge);
            result = touchpad_read_i2c( client, addr, &value[0], 2 );
			if (result)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to read GET_MERGE!\n" );
                result = -EFAULT;
				return result;
			}
			result = copy_to_user( (void *)arg,&value[0],2 );
			if (result)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "copy GET_MERGE to user failed!\n" );
                result = -EFAULT;
				return result;
			}
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "value[6]: %x\n",value[0] );
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "value[7]: %x\n",value[1] );
            break;
		case ATMEL_TOUCH_IOCTL_SET_NOISE_SUPPRESSION:
            addr = maxTouchCfg_T22_obj.obj_addr;
            pData = (void *)&noise_suppression;
            length = sizeof(noise_suppression);
            if( copy_from_user( (void *)pData,
                                (void *)arg,
                                length) )
            {
                MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "copy noise_suppression from user failed!\n" );
                result = -EFAULT;
				return result;
            }
			else
			{
                value[0] = noise_suppression.ctrl;
				result = touchpad_write_i2c( client, addr, &value[0], 1 );
				if (result)
				{
					MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to write SET_NOISE_SUPPRESSION!\n" );
					result = -EFAULT;
					return result;
				}
				MY_INFO_PRINTK( 4,"INFO_LEVEL:" "SET_NOISE_SUPPRESSION: ctrl %x\n", value[0]);
				
				value[1]  = noise_suppression.noisethr;
				addr = maxTouchCfg_T22_obj.obj_addr+8;
				result = touchpad_write_i2c( client, addr, &value[1], 1 );
				if (result)
				{
					MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to write SET_NOISE_SUPPRESSION!\n" );
					result = -EFAULT;
					return result;
				}
				MY_INFO_PRINTK( 4,"INFO_LEVEL:" "SET_NOISE_SUPPRESSION: noisethr %x\n", value[1]);
				value[2] = noise_suppression.freqhopscale;
				value[3] = noise_suppression.freq_0;
				value[4] = noise_suppression.freq_1;
				value[5] = noise_suppression.freq_2;
				value[6] = noise_suppression.freq_3;
				value[7] = noise_suppression.freq_4;
				addr = maxTouchCfg_T22_obj.obj_addr+10;
				result = touchpad_write_i2c( client, addr, &value[2], 6 );
				if (result)
				{
					MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to write SET_NOISE_SUPPRESSION!\n" );
					result = -EFAULT;
					return result;
				}
				MY_INFO_PRINTK( 4,"INFO_LEVEL:" "SET_NOISE_SUPPRESSION: freqhopscale %x\n", value[2]);
				MY_INFO_PRINTK( 4,"INFO_LEVEL:" "SET_NOISE_SUPPRESSION: freq_0 %x\n", value[3]);
				MY_INFO_PRINTK( 4,"INFO_LEVEL:" "SET_NOISE_SUPPRESSION: freq_1 %x\n", value[4]);
				MY_INFO_PRINTK( 4,"INFO_LEVEL:" "SET_NOISE_SUPPRESSION: freq_2 %x\n", value[5]);
				MY_INFO_PRINTK( 4,"INFO_LEVEL:" "SET_NOISE_SUPPRESSION: freq_3 %x\n", value[6]);
				MY_INFO_PRINTK( 4,"INFO_LEVEL:" "SET_NOISE_SUPPRESSION: freq_4 %x\n", value[7]);
            }
            break;
        case ATMEL_TOUCH_IOCTL_GET_NOISE_SUPPRESSION:
            addr = maxTouchCfg_T22_obj.obj_addr;
            pData = (void *)&noise_suppression;
            length = sizeof(noise_suppression);
            result = touchpad_read_i2c( client, addr, &value[0], 1 );
			if (result)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to read GET_NOISE_SUPPRESSION!\n" );
                result = -EFAULT;
				return result;
			}
			addr = maxTouchCfg_T22_obj.obj_addr+8;
            result = touchpad_read_i2c( client, addr, &value[1], 1 );
			if (result)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to read GET_NOISE_SUPPRESSION!\n" );
                result = -EFAULT;
				return result;
			}
			addr = maxTouchCfg_T22_obj.obj_addr+10;
            result = touchpad_read_i2c( client, addr, &value[2], 6 );
			if (result)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to read GET_NOISE_SUPPRESSION!\n" );
                result = -EFAULT;
				return result;
			}
			result = copy_to_user( (void *)arg,&value[0],8 );
			if (result)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "copy GET_NOISE_SUPPRESSION to user failed!\n" );
                result = -EFAULT;
				return result;
			}
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "value[8]: %x\n",value[0] );
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "value[9]: %x\n",value[1] );
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "value[10]: %x\n",value[2] );
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "value[11]: %x\n",value[3] );
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "value[12]: %x\n",value[4] );
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "value[13]: %x\n",value[5] );
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "value[14]: %x\n",value[6] );
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "value[15]: %x\n",value[7] );
            break;	
		case ATMEL_TOUCH_IOCTL_SET_POWER_SWITCH:
        {
			struct atmel_power_switch_t power;	
            if( copy_from_user( (void *)&power,
                                (void *)arg,
                                sizeof(power) ) )
            {
                MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "copy POWER_SWITCH from user failed!\n");
                result = -EFAULT;
				return result;
            }
            if( power.on )
                tp_resume(&g_tp->touch_early_suspend);
            else
                tp_suspend(&g_tp->touch_early_suspend);
            break;
        }
		case ATMEL_TOUCH_IOCTL_GET_REFERENCES_MODE:
		{
			int  i,j;
			uint8_t data_buffer[NUM_OF_REF_MODE_PAGE][SIZE_OF_REF_MODE_PAGE];
			
            pData = (void *)&references_mode;
            length = sizeof(references_mode);
			
        	mutex_lock(&g_tp->mutex);
            read_T37(g_tp,data_buffer,0x11);
			mutex_unlock(&g_tp->mutex);
			
			for(i=0;i<x_channel;i++)
			{
				for(j=0;j<y_channel;j++)
				{
					references_mode.data[i][j] = (int16_t)get_ref_value(i,j,data_buffer);
					MY_INFO_PRINTK( 4,"INFO_LEVEL:""x[%d]y[%d] = %d\n",i,j,references_mode.data[i][j]);
				}
			}
			
			result = copy_to_user( (void *)arg,pData, length );
			if (result)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "copy ATMEL_TOUCH_IOCTL_GET_REFERENCES_MODE to user failed!\n" );
				result = -EFAULT;
				return result;
			}
            break;
		}
		case ATMEL_TOUCH_IOCTL_GET_DELTAS_MODE:
		{
			int  i,j;
			uint8_t data_buffer[NUM_OF_REF_MODE_PAGE][SIZE_OF_REF_MODE_PAGE];
			
            pData = (void *)&deltas_mode;
            length = sizeof(deltas_mode);
			
        	mutex_lock(&g_tp->mutex);
            read_T37(g_tp,data_buffer,0x10);
			mutex_unlock(&g_tp->mutex);
			
			for(i=0;i<x_channel;i++)
			{
				for(j=0;j<y_channel;j++)
				{
					deltas_mode.deltas[i][j] = (int16_t)get_ref_value(i,j,data_buffer);
					MY_INFO_PRINTK( 4,"INFO_LEVEL:""x[%d]y[%d] = %d\n",i,j,deltas_mode.deltas[i][j]);
				}
			}
			
			result = copy_to_user( (void *)arg,pData, length );
			if (result)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "copy ATMEL_TOUCH_IOCTL_GET_DELTAS_MODE to user failed!\n" );
				result = -EFAULT;
				return result;
			}
            break;
		}
		case ATMEL_TOUCH_IOCTL_SET_GAIN:
            addr = maxTouchCfg_T9_obj.obj_addr+6;
			addr_key = maxTouchCfg_T15_obj.obj_addr+6;
            pData = (void *)&gain;
            length = sizeof(gain);
            if( copy_from_user( (void *)pData,
                                (void *)arg,
                                length) )
            {
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "copy SET_GAIN from user failed!\n" );
                result = -EFAULT;
				return result;
            }
			else
			{
                value[0] = gain.touch_blen;
				value[1] = gain.key_blen;
				result = touchpad_write_i2c( client, addr, &value[0], 1 );
				if (result)
				{
					MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to write SET_TOUCH_GAIN!\n" );
					result = -EFAULT;
					return result;
				}
				MY_INFO_PRINTK( 4,"INFO_LEVEL:" "SET_TOUCH_GAIN: %x\n", value[0]);
				result = touchpad_write_i2c( client, addr_key, &value[1], 1 );
				if (result)
				{
					MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to write SET_KEY_GAIN!\n" );
					result = -EFAULT;
					return result;
				}
				MY_INFO_PRINTK( 4,"INFO_LEVEL:" "SET_KEY_GAIN: %x\n", value[1]);
            }
            break;
		case ATMEL_TOUCH_IOCTL_GET_GAIN:
			addr = maxTouchCfg_T9_obj.obj_addr+6;
			addr_key = maxTouchCfg_T15_obj.obj_addr+6;
            pData = (void *)&gain;
            length = sizeof(gain);
            result = touchpad_read_i2c( client, addr, &value[0], 1 );
			if (result)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to read GET_TOUCH_GAIN!\n" );
                result = -EFAULT;
				return result;
			}
			result = touchpad_read_i2c( client, addr_key, &value[1], 1 );
			if (result)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to read GET_KEY_GAIN!\n" );
                result = -EFAULT;
				return result;
			}
			result = copy_to_user( (void *)arg,&value[0],2 );
			if (result)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "copy GET_TOUCH_GAIN to user failed!\n" );
                result = -EFAULT;
				return result;
			}
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "GET_TOUCH_GAIN: %x\n",value[0] );
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "GET_KEY_GAIN: %x\n",value[1] );
            break;
		case ATMEL_TOUCH_IOCTL_GET_CTE_MODE:
		{
			int i;
			addr_T37 = maxTouchCfg_T37_obj.obj_addr;
            addr = maxTouchCfg_T7_obj.obj_addr;
			addr_cte = maxTouchCfg_T28_obj.obj_addr+3;
			addr_cmd = maxTouchCfg_T28_obj.obj_addr+1;
			addr_diagnostic = maxTouchCfg_T6_obj.obj_addr+5;
			addr_T6 = maxTouchCfg_T6_obj.obj_addr+1;
            pData = (void *)&cte_mode;
            length = sizeof(cte_mode);
        	
			mutex_lock(&g_tp->mutex);
			result = touchpad_read_i2c( client, addr, &value_backup[0], 2 );
			if (result)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to read T7!\n" );
				result = -EFAULT;
				mutex_unlock(&g_tp->mutex);
				return result;
			}
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "GET_CTE_MODE read T7 idleacqint: %x\n", value_backup[0]);
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "GET_CTE_MODE readT7 actvacqint: %x\n", value_backup[1]);
			result = touchpad_read_i2c( client, addr_cte, &value_backup[2], 2 );
			if (result)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to read T28!\n" );
				result = -EFAULT;
				mutex_unlock(&g_tp->mutex);
				return result;
			}
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "GET_CTE_MODE read T28 idlegcafdepth: %x\n", value_backup[2]);
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "GET_CTE_MODE read T28 actvgcafdepth: %x\n", value_backup[3]);
			value[0] = 0xA;
			value[1] = 0xA;
			value[2] = 0x8;
			value[3] = 0x8;
			value[4] = 0xA5;
			value[5] = 0x31;
			result = touchpad_write_i2c( client, addr, &value[0], 2 );
			if (result)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to write T7!\n" );
				result = -EFAULT;
				mutex_unlock(&g_tp->mutex);
				return result;
			}
			result = touchpad_write_i2c( client, addr_cte, &value[2], 2 );
			if (result)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to write T28!\n" );
				result = -EFAULT;
				mutex_unlock(&g_tp->mutex);
				return result;
			}
			result = touchpad_write_i2c( client, addr_cmd, &value[4], 1 );
			if (result)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to write T28 CMD!\n" );
				result = -EFAULT;
				mutex_unlock(&g_tp->mutex);
				return result;
			}
			msleep(100);
			result = touchpad_write_i2c( client, addr_diagnostic, &value[5], 1 );
			if (result)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to write T6!\n" );
				result = -EFAULT;
				mutex_unlock(&g_tp->mutex);
				return result;
			}
			msleep(100);
			result = touchpad_read_i2c( client, addr_T37, &value_cte[0], 18 );
			if (result)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to read GET_CTE_MODE!\n" );
				result = -EFAULT;
				mutex_unlock(&g_tp->mutex);
				return result;
			}
			for ( i = 2 ; i < 18 ; i++ )
			{
				cte_mode.cte_data[i-2] = value_cte[i];
			}
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "Current Mode : 0x%x\n",value_cte[0] );
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "Page NO. : 0x%x\n",value_cte[1] );
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "number of Y line : %d\n",(int)cte_mode.cte_data[0] );
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "Reserved : %d\n",(int)cte_mode.cte_data[1] );
			for(i=2;i<16;i++)
			{
				MY_INFO_PRINTK( 4,"INFO_LEVEL:" "Y%d : Gain %d\n",i-2,(int)cte_mode.cte_data[i] );
			}
			msleep(100);
			result = copy_to_user( (void *)arg,pData, length );
			if (result)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "copy ATMEL_TOUCH_IOCTL_GET_CTE_MODE to user failed!\n" );
				result = -EFAULT;
				mutex_unlock(&g_tp->mutex);
				return result;
			}
			result = touchpad_write_i2c( client, addr, &value_backup[0], 2 );
			if (result)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to write T7!\n" );
				result = -EFAULT;
				mutex_unlock(&g_tp->mutex);
				return result;
			}
			result = touchpad_write_i2c( client, addr_cte, &value_backup[2], 2 );
			if (result)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to write T28!\n" );
				result = -EFAULT;
				mutex_unlock(&g_tp->mutex);
				return result;
			}
			#if 0
			value_T6_reset = T6_RESET_VALUE;
			result = touchpad_write_i2c(client,maxTouchCfg_T6_obj.obj_addr,&value_T6_reset ,WRITE_T6_SIZE);
			MY_INFO_PRINTK( 4, "TEST_INFO_LEVEL:" "software reset 0x%x = 0x%x\n",maxTouchCfg_T6_obj.obj_addr,value_T6_reset );
			if( result )
			{
				MY_INFO_PRINTK( 0, "ERROR_LEVEL:" "unable to write software reset T6 object 0x%x = 0x%x\n",maxTouchCfg_T6_obj.obj_addr,value_T6_reset );
				result = -EFAULT;
				mutex_unlock(&g_tp->mutex);
				return result;
			}
			msleep(100);
			#endif
			mutex_unlock(&g_tp->mutex);
            break;
		}
		case ATMEL_TOUCH_IOCTL_SET_KEYARRAY_FVS_MODE:
		{
			struct atmel_fvs_mode_t fvs_mode;	
            if( copy_from_user( (void *)&fvs_mode,
                                (void *)arg,
                                sizeof(fvs_mode) ) )
            {
                MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "copy FVS MODE from user failed!\n");
                result = -EFAULT;
				return result;
            }
            if( fvs_mode.enter )
                g_tp->fvs_mode_flag = 1;
            else
               g_tp->fvs_mode_flag = 0;
            break;
        }
		case ATMEL_TOUCH_IOCTL_SET_ACQUISITION:
            addr = maxTouchCfg_T8_obj.obj_addr;
            pData = (void *)&acquisition;
            length = sizeof(acquisition);
            if( copy_from_user( (void *)pData,
                                (void *)arg,
                                length) )
            {
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "copy SET_ACQUISITION from user failed!\n" );
                result = -EFAULT;
				return result;
            }
			else
			{
                value[0] = acquisition.chrgtime;
				value[1] = acquisition.tchdrift;
				value[2] = acquisition.driftst;
				value[3] = acquisition.tchautocal;
				value[4] = acquisition.sync;
				value[5] = acquisition.atchcalst;
				value[6] = acquisition.atchcalsthr;
				value[7] = acquisition.atchfrccalthr;
				value[8] = acquisition.atchfrccalratio;
				result = touchpad_write_i2c( client, addr, &value[0], 1 );
				if (result)
				{
					MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to write SET_ACQUISITION!\n" );
					result = -EFAULT;
					return result;
				}
				MY_INFO_PRINTK( 4,"INFO_LEVEL:" "write chrgtime : %x\n", value[0]);
				addr = maxTouchCfg_T8_obj.obj_addr+2;
				result = touchpad_write_i2c( client, addr, &value[1], 8 );
				if (result)
				{
					MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to write SET_ACQUISITION!\n" );
					result = -EFAULT;
					return result;
				}
				MY_INFO_PRINTK( 4,"INFO_LEVEL:" "write tchdrift : %x\n", value[1]);
				MY_INFO_PRINTK( 4,"INFO_LEVEL:" "write driftst : %x\n", value[2]);
				MY_INFO_PRINTK( 4,"INFO_LEVEL:" "write tchautocal : %x\n", value[3]);
				MY_INFO_PRINTK( 4,"INFO_LEVEL:" "write sync : %x\n", value[4]);
				MY_INFO_PRINTK( 4,"INFO_LEVEL:" "write atchcalst : %x\n", value[5]);
				MY_INFO_PRINTK( 4,"INFO_LEVEL:" "write atchcalsthr : %x\n", value[6]);
				MY_INFO_PRINTK( 4,"INFO_LEVEL:" "write atchfrccalthr : %x\n", value[7]);
				MY_INFO_PRINTK( 4,"INFO_LEVEL:" "write tchfrccalratio : %x\n", value[8]);
            }
            break;
		case ATMEL_TOUCH_IOCTL_GET_ACQUISITION:
			addr = maxTouchCfg_T8_obj.obj_addr;
            result = touchpad_read_i2c( client, addr, &value[0], 1 );
			if (result)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to read GET_ACQUISITION!\n" );
                result = -EFAULT;
				return result;
			}
			addr = maxTouchCfg_T8_obj.obj_addr+2;
			result = touchpad_read_i2c( client, addr, &value[1], 8 );
			if (result)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to read GET_ACQUISITION!\n" );
                result = -EFAULT;
				return result;
			}
			result = copy_to_user( (void *)arg,&value[0],9 );
			if (result)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "copy GET_ACQUISITION to user failed!\n" );
                result = -EFAULT;
				return result;
			}
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "read chrgtime : %x\n", value[0]);
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "read tchdrift : %x\n", value[1]);
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "read driftst : %x\n", value[2]);
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "read tchautocal : %x\n", value[3]);
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "read sync : %x\n", value[4]);
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "read atchcalst : %x\n", value[5]);
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "read atchcalsthr : %x\n", value[6]);
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "write atchfrccalthr : %x\n", value[7]);
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "write tchfrccalratio : %x\n", value[8]);
            break;
		case ATMEL_TOUCH_IOCTL_SET_KEYARRAY:
            addr = maxTouchCfg_T15_obj.obj_addr;
            pData = (void *)&keyarray;
            length = sizeof(keyarray);
            if( copy_from_user( (void *)pData,
                                (void *)arg,
                                length) )
            {
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "copy SET_KEYARRAY from user failed!\n" );
                result = -EFAULT;
				return result;
            }
			else
			{
                value[0] = keyarray.ctrl;
				value[1] = keyarray.xorigin;
				value[2] = keyarray.yorigin;
				value[3] = keyarray.xsize;
				value[4] = keyarray.ysize;
				value[5] = keyarray.akscfg;
				value[6] = keyarray.blen;
				value[7] = keyarray.chthr;
				value[8] = keyarray.tchdi;
				result = touchpad_write_i2c( client, addr, &value[0], 9 );
				if (result)
				{
					MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to write SET_KEYARRAY!\n" );
					result = -EFAULT;
					return result;
				}
				MY_INFO_PRINTK( 4,"INFO_LEVEL:" "write ctrl : %x\n", value[0]);
				MY_INFO_PRINTK( 4,"INFO_LEVEL:" "write xorigin : %x\n", value[1]);
				MY_INFO_PRINTK( 4,"INFO_LEVEL:" "write yorigin : %x\n", value[2]);
				MY_INFO_PRINTK( 4,"INFO_LEVEL:" "write xsize : %x\n", value[3]);
				MY_INFO_PRINTK( 4,"INFO_LEVEL:" "write ysize : %x\n", value[4]);
				MY_INFO_PRINTK( 4,"INFO_LEVEL:" "write akscfg : %x\n", value[5]);
				MY_INFO_PRINTK( 4,"INFO_LEVEL:" "write blen : %x\n", value[6]);
				MY_INFO_PRINTK( 4,"INFO_LEVEL:" "write chthr : %x\n", value[7]);
				MY_INFO_PRINTK( 4,"INFO_LEVEL:" "write tchdi : %x\n", value[8]);
            }
            break;
		case ATMEL_TOUCH_IOCTL_GET_KEYARRAY:
			addr = maxTouchCfg_T15_obj.obj_addr;
            result = touchpad_read_i2c( client, addr, &value[0], 9 );
			if (result)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to read GET_KEYARRAY!\n" );
                result = -EFAULT;
				return result;
			}
			result = copy_to_user( (void *)arg, &value[0], 9 );
			if (result)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "copy GET_KEYARRAY to user failed!\n" );
                result = -EFAULT;
				return result;
			}
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "read ctrl : %x\n", value[0]);
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "read xorigin : %x\n", value[1]);
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "read yorigin : %x\n", value[2]);
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "read xsize : %x\n", value[3]);
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "read ysize : %x\n", value[4]);
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "read akscfg : %x\n", value[5]);
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "read blen : %x\n", value[6]);
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "read chthr : %x\n", value[7]);
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "read tchdi : %x\n", value[8]);
            break;
		case ATMEL_TOUCH_IOCTL_SET_MULTITOUCHSCREEN:
            addr = maxTouchCfg_T9_obj.obj_addr;
            pData = (void *)&multitouchscreen;
            length = sizeof(multitouchscreen);
            if( copy_from_user( (void *)pData,
                                (void *)arg,
                                length) )
            {
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "copy SET_MULTITOUCHSCREEN from user failed!\n" );
                result = -EFAULT;
				return result;
            }
			else
			{
                value[0] = multitouchscreen.ctrl;
				value[1] = multitouchscreen.xorigin;
				value[2] = multitouchscreen.yorigin;
				value[3] = multitouchscreen.xsize;
				value[4] = multitouchscreen.ysize;
				value[5] = multitouchscreen.akscfg;
				value[6] = multitouchscreen.tchdi;
				value[7] = multitouchscreen.orient;
				value[8] = multitouchscreen.mrgtimeout;
				value[9] = multitouchscreen.movfilter;
				value[10] = multitouchscreen.numtouch;
				value[11] = multitouchscreen.amphyst;
				value[12] = multitouchscreen.xrange0;
				value[13] = multitouchscreen.xrange1;
				value[14] = multitouchscreen.yrange0;
				value[15] = multitouchscreen.yrange1;
				value[16] = multitouchscreen.xloclip;
				value[17] = multitouchscreen.xhiclip;
				value[18] = multitouchscreen.yloclip;
				value[19] = multitouchscreen.yhiclip;
				value[20] = multitouchscreen.xedgectrl;
				value[21] = multitouchscreen.xedgedist;
				value[22] = multitouchscreen.yedgectrl;
				value[23] = multitouchscreen.yedgedist;
				value[24] = multitouchscreen.jumplimit;
				value[25] = multitouchscreen.tchhyst;
				result = touchpad_write_i2c( client, addr, &value[0], 6 );
				if (result)
				{
					MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to write SET_MULTITOUCHSCREEN!\n" );
					result = -EFAULT;
					return result;
				}
				MY_INFO_PRINTK( 1,"INFO_LEVEL:" "write chrgtime : %x\n", value[0]);
				addr = maxTouchCfg_T9_obj.obj_addr+8;
				result = touchpad_write_i2c( client, addr, &value[6], 3 );
				if (result)
				{
					MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to write SET_MULTITOUCHSCREEN!\n" );
					result = -EFAULT;
					return result;
				}
				addr = maxTouchCfg_T9_obj.obj_addr+13;
				result = touchpad_write_i2c( client, addr, &value[9], 2 );
				if (result)
				{
					MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to write SET_MULTITOUCHSCREEN!\n" );
					result = -EFAULT;
					return result;
				}
				addr = maxTouchCfg_T9_obj.obj_addr+17;
				result = touchpad_write_i2c( client, addr, &value[11], 15 );
				if (result)
				{
					MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to write SET_MULTITOUCHSCREEN!\n" );
					result = -EFAULT;
					return result;
				}
				for(i = 0 ; i < 26; i++)
				{
					MY_INFO_PRINTK( 4,"INFO_LEVEL:" "write multitouchscreen reg value: %d\n", value[i]);
				}
            }
            break;
		case ATMEL_TOUCH_IOCTL_GET_MULTITOUCHSCREEN:
			addr = maxTouchCfg_T9_obj.obj_addr;
            result = touchpad_read_i2c( client, addr, &value[0], 6 );
			if (result)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to read GET_MULTITOUCHSCREEN!\n" );
                result = -EFAULT;
				return result;
			}
			addr = maxTouchCfg_T9_obj.obj_addr+8;
			result = touchpad_read_i2c( client, addr, &value[6], 3 );
			if (result)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to read GET_MULTITOUCHSCREEN!\n" );
                result = -EFAULT;
				return result;
			}
			addr = maxTouchCfg_T9_obj.obj_addr+13;
			result = touchpad_read_i2c( client, addr, &value[9], 2 );
			if (result)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to read GET_MULTITOUCHSCREEN!\n" );
                result = -EFAULT;
				return result;
			}
			addr = maxTouchCfg_T9_obj.obj_addr+17;
			result = touchpad_read_i2c( client, addr, &value[11], 15 );
			if (result)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to read GET_MULTITOUCHSCREEN!\n" );
                result = -EFAULT;
				return result;
			}
			result = copy_to_user( (void *)arg,&value[0],26 );
			if (result)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "copy GET_MULTITOUCHSCREEN to user failed!\n" );
                result = -EFAULT;
				return result;
			}
			for(i = 0 ; i < 26; i++)
			{
				MY_INFO_PRINTK( 4,"INFO_LEVEL:" "write multitouchscreen reg value: %d\n", value[i]);
			}
            break;
		case ATMEL_TOUCH_IOCTL_GET_VERSION:
			addr = ADD_ID_INFO_BLOCK+2;
            result = touchpad_read_i2c( client, addr, &value[0], 1 );
			if (result)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to read GET_VERSION!\n" );
                result = -EFAULT;
				return result;
			}
			result = copy_to_user( (void *)arg,&value[0],1 );
			if (result)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "copy GET_VERSION to user failed!\n" );
                result = -EFAULT;
				return result;
			}
			MY_INFO_PRINTK( 4,"INFO_LEVEL:" "value[0]: %x\n",value[0] );
            break;
		case ATMEL_TOUCH_IOCTL_SET_SELFTEST_FVS_MODE:
        {
            if( copy_from_user( (void *)&local_selftest,
                                (void *)arg,
                                sizeof(local_selftest) ) )
            {
                MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "copy SELFTEST from user failed!\n");
                result = -EFAULT;
				return result;
            }
            if (local_selftest.on)
			{	
				g_tp->selftest.value = 0;
                g_tp->selftest_flag = 1;
				tp_selftest_fvs(g_tp);
			}	
            else
			{
                g_tp->selftest_flag = 0;
				tp_selftest_fvs(g_tp);
				result = copy_to_user( (void *)arg,&g_tp->selftest,sizeof(g_tp->selftest) );
				if (result)
				{
					MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "copy Selftest Result to user failed!\n" );
					result = -EFAULT;
					return result;
				}
			}	
            break;
        }
		case ATMEL_TOUCH_IOCTL_SET_GRIP_FACE_SUPPRESSION:
            addr = maxTouchCfg_T20_obj.obj_addr;
            pData = (void *)&grip_face_suppression;
            length = sizeof(grip_face_suppression);
            if( copy_from_user( (void *)pData,
                                (void *)arg,
                                length) )
            {
                MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "copy grip_face_suppression from user failed!\n" );
                result = -EFAULT;
				return result;
            }
			else
			{
                value[0] = grip_face_suppression.ctrl;
				value[1] = grip_face_suppression.xlogrip;
				value[2] = grip_face_suppression.xhigrip;
				value[3] = grip_face_suppression.ylogrip;
				value[4] = grip_face_suppression.yhigrip;
				value[5] = grip_face_suppression.maxtchs;
				result = touchpad_write_i2c( client, addr, &value[0], 6 );
				if (result)
				{
					MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to write SET_GRIP_FACE_SUPPRESSION!\n" );
					result = -EFAULT;
					return result;
				}
				value[6]  = grip_face_suppression.szthr1;
				value[7]  = grip_face_suppression.szthr2;
				value[8]  = grip_face_suppression.shpthr1;
				value[9]  = grip_face_suppression.shpthr2;
				value[10] = grip_face_suppression.supextto;
				addr = maxTouchCfg_T20_obj.obj_addr+7;
				result = touchpad_write_i2c( client, addr, &value[6], 5 );
				if (result)
				{
					MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to write SET_GRIP_FACE_SUPPRESSION!\n" );
					result = -EFAULT;
					return result;
				}
            }
            break;
        case ATMEL_TOUCH_IOCTL_GET_GRIP_FACE_SUPPRESSION:
            addr = maxTouchCfg_T20_obj.obj_addr;
            result = touchpad_read_i2c( client, addr, &value[0], 6 );
			if (result)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to read GET_GRIP_FACE_SUPPRESSION!\n" );
                result = -EFAULT;
				return result;
			}
			addr = maxTouchCfg_T20_obj.obj_addr+7;
            result = touchpad_read_i2c( client, addr, &value[6], 5 );
			if (result)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed to read GET_GRIP_FACE_SUPPRESSION!\n" );
                result = -EFAULT;
				return result;
			}
			result = copy_to_user( (void *)arg,&value[0],11 );
			if (result)
			{
				MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "copy GET_MULTITOUCHSCREEN to user failed!\n" );
                result = -EFAULT;
				return result;
			}
			for(i = 0 ; i < 11; i++)
			{
				MY_INFO_PRINTK( 1,"INFO_LEVEL:" "write grip_face_suppression reg value: %d\n", value[i]);
			}
            break;
	} //end of switch
	PRINT_OUT
    return result;
}

static int tp_misc_release(struct inode *inode, struct file *fp)
{
    int result = 0;
    
    PRINT_IN
    mutex_lock(&g_tp->mutex);
    if( g_tp->misc_open_count )
    {
        g_tp->misc_open_count--;      
    }
    mutex_unlock(&g_tp->mutex);
    PRINT_OUT;
    return result;
}

static int tp_misc_open(struct inode *inode, struct file *fp)
{
    int result = 0;
    
    PRINT_IN
    mutex_lock(&g_tp->mutex);
    if( g_tp->misc_open_count <= 1 )
    {
        g_tp->misc_open_count++;
        MY_INFO_PRINTK( 4, "INFO_LEVEL:" "misc open count : %d\n",g_tp->misc_open_count );          
    }	
    else
    { 
 	result = -EFAULT;
 	MY_INFO_PRINTK( 0, "ERROR_LEVEL:" "failed to open misc count : %d\n",g_tp->misc_open_count );  
    }
    mutex_unlock(&g_tp->mutex);
    PRINT_OUT
    return result;
}

static struct file_operations tp_misc_fops = {
	.owner 	= THIS_MODULE,
	.open 	= tp_misc_open,
	.release = tp_misc_release,
	.write = tp_misc_write,
	.read = tp_misc_read,
    .unlocked_ioctl = tp_misc_ioctl,
};

static struct miscdevice tp_misc_device = {
	.minor 	= MISC_DYNAMIC_MINOR,
	.name 	= "auo_misc_touch",
	.fops 	= &tp_misc_fops,
};

static void tp_suspend(struct early_suspend *h)
{
    int ret = 0;
    struct touchpad_t *tp = container_of( h,struct touchpad_t,touch_early_suspend);
    struct i2c_client *client = tp->client_4_i2c;
    int obj_size = 2;
	uint8_t value[maxTouchCfg_T7_obj.size];
	uint16_t regbuf;
	int i;
	
	PRINT_IN
    MY_INFO_PRINTK( 1, "INFO_LEVEL:" "tp_suspend : E\n" );
	mutex_lock(&g_tp->mutex);
	if( g_tp->touch_suspended )
	{
		mutex_unlock(&g_tp->mutex);
		PRINT_OUT
		return;
	}
    disable_irq( tp->irq );
	MY_INFO_PRINTK( 4, "INFO_LEVEL:" "disable irq %d\n",tp->irq );
    g_tp->touch_suspended = 1;
	mutex_unlock(&g_tp->mutex);
    ret = cancel_work_sync(&tp->touchpad_work.work);
	mutex_lock(&g_tp->mutex);
    if (ret)
    {
		enable_irq(tp->irq);
	}	
	input_report_key(g_tp->keyarray_input, KEY_HOME, 0);
	input_report_key(g_tp->keyarray_input, KEY_MENU, 0);
	input_report_key(g_tp->keyarray_input, KEY_BACK, 0);
    
	i = 0;
	touchpad_read_i2c( client, maxTouchCfg_T7_obj.obj_addr, &value[0], maxTouchCfg_T7_obj.size );
	for ( regbuf = maxTouchCfg_T7_obj.obj_addr; regbuf < maxTouchCfg_T7_obj.obj_addr+maxTouchCfg_T7_obj.size; regbuf++ )
    {
		MY_INFO_PRINTK( 4, "INFO_LEVEL:" "write T7 obj 0x%xh = 0x%x\n",regbuf, value[i] );
    	i++;
    }
   
	value[0]= 0x0;
	value[1] = 0x0;
	ret = touchpad_write_i2c( client, maxTouchCfg_T7_obj.obj_addr, &value[0], obj_size );
	if (ret)
	{
		MY_INFO_PRINTK( 0, "ERROR_LEVEL:" "cannot enter real sleep mode\n" );
	}
	
	i = 0;
	touchpad_read_i2c( client, maxTouchCfg_T7_obj.obj_addr, &value[0], maxTouchCfg_T7_obj.size );
	for ( regbuf = maxTouchCfg_T7_obj.obj_addr; regbuf < maxTouchCfg_T7_obj.obj_addr+maxTouchCfg_T7_obj.size; regbuf++ )
    {
		MY_INFO_PRINTK( 4, "INFO_LEVEL:" "write T7 obj 0x%xh = 0x%x\n",regbuf, value[i] );
    	i++;
    }
	
	touchpad_poweron_device(0);
	MY_INFO_PRINTK( 1, "INFO_LEVEL:" "tp_suspend: X\n" );
	mutex_unlock(&g_tp->mutex);
	cancel_work_sync(&tp->chk_work.work);
	MY_INFO_PRINTK( 4, "INFO_LEVEL:" "----tp_suspend----\n" );
    PRINT_OUT
	return;
}

static void tp_resume(struct early_suspend *h)
{
    int result = 0;
    struct touchpad_t *tp = container_of( h,struct touchpad_t,touch_early_suspend);
	int obj_size = (int)( maxTouchCfg_T5_obj.size );
	uint8_t value[obj_size];
	int i;
	uint8_t value_rek_thrld;
	
    PRINT_IN
    MY_INFO_PRINTK( 1, "INFO_LEVEL:" "tp_resume: E\n" );
	mutex_lock(&g_tp->mutex);
    if( 0 == g_tp->touch_suspended )
	{
		mutex_unlock(&g_tp->mutex);
		PRINT_OUT
		return;
	}
	for (i = 0 ; i < ATMEL_REPORT_POINTS ; i++ )
	{
		if (g_tp->msg[i].z == -1)
			continue;
		g_tp->msg[i].z = 0;
		g_tp->msg[i].state = RELEASE;
	}
	tp_report_coord_via_mt_protocol();
	#if 1
	touchpad_poweron_device(1);
	
    result = touchpad_config_gpio( tp->gpio_num, tp->gpio_rst );
    if( result )
    {
        MY_INFO_PRINTK( 0, "ERROR_LEVEL:" "touchpad_probe: failed to config gpio\n" );
    }    
        
    result = gpio_get_value(tp->gpio_num);
    if( result )
    {
        MY_INFO_PRINTK( 0, "ERROR_LEVEL:" "touchpad_probe: failed to get gpio_num value %d (result=%d)\n",tp->gpio_num,result );
    }
	
	touchpad_read_i2c( tp->client_4_i2c, maxTouchCfg_T5_obj.obj_addr, &value[0],obj_size );
    if( value[0] >= maxTouchCfg_T6_obj.reportid_ub && value[0] <= maxTouchCfg_T6_obj.reportid_lb )
    {
    	if( (value[1] & CFGERR_MASK) != 0x0 )
    	{
    		MY_INFO_PRINTK( 0, "ERROR_LEVEL:" "failed to  CFGERR bit : 0x%x  \n", value[1] );
		}
    }
	else
	{
		MY_INFO_PRINTK( 4, "INFO_LEVEL:" "report id is not 1  0x%x\n", value[0] );
	}
	#endif
	
	value[0]= maxTouchCfg_T7_obj.value_array[0];
	value[1] = maxTouchCfg_T7_obj.value_array[1];
	MY_INFO_PRINTK( 4, "INFO_LEVEL:" "read T7 IDLEACQINT : 0x%x\n", value[0] );
	MY_INFO_PRINTK( 4, "INFO_LEVEL:" "read T7 ACTVACQINT : 0x%x\n", value[1] );

	result = touchpad_write_i2c( tp->client_4_i2c, maxTouchCfg_T7_obj.obj_addr, &value[0], 2 );
	if (result)
	{
		MY_INFO_PRINTK( 0, "ERROR_LEVEL:" "cannot enter normal mode\n" );
	}
	
	#if 0
	value[0] = T6_CALIBRATE_VALUE;
	result = touchpad_write_i2c( tp->client_4_i2c, maxTouchCfg_T6_obj.obj_addr+2, &value[0], WRITE_T6_SIZE );
	if (result)
	{
		MY_INFO_PRINTK( 0, "ERROR_LEVEL:" "cannot write a calibrate filed in T6 object!!!\n" );
	}
	#endif
	value_rek_thrld = 0x2;
	result = touchpad_write_i2c( tp->client_4_i2c, maxTouchCfg_T8_obj.obj_addr+8, &value_rek_thrld, 1 );
	if (result)
	{
		MY_INFO_PRINTK( 0, "ERROR_LEVEL:" "cannot write a calibrate filed in T6 object!!!\n" );
	}
	printk("%s addr : %x value_rek_thrld : %x\n",__func__,maxTouchCfg_T8_obj.obj_addr+8,value_rek_thrld);
	enable_irq( tp->irq );
	g_tp->check_home_key = 1;
	g_tp->touch_suspended = 0;
	MY_INFO_PRINTK( 1, "INFO_LEVEL:" "tp_resume: X\n" );
    mutex_unlock(&g_tp->mutex);
	MY_INFO_PRINTK( 4, "INFO_LEVEL:" "----tp_resume----\n" );
	PRINT_OUT
	return;
}

static const struct i2c_device_id i2cAtmelTouch_idtable[] = {
       { ATMEL_TOUCHSCREEN_DRIVER_NAME, 0 },
       { }
};

MODULE_DEVICE_TABLE(i2c, i2cAtmelTouch_idtable);

static struct i2c_driver i2c_touchpad_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name  = ATMEL_TOUCHSCREEN_DRIVER_NAME,
	},
	.probe	 = touchpad_probe,

	.id_table = i2cAtmelTouch_idtable,
};

static int touchpad_keyarray_event(struct input_dev *dev, unsigned int type,
             unsigned int code, int value)
{
  return 0;
}

static int touchpad_keyarray_open(struct input_dev *dev)
{
	int rc = 0;
    
    PRINT_IN
	#if 0
    mutex_lock(&g_tp->mutex);
    if( g_tp->keyarray_open_count == 0 )
    {
        g_tp->keyarray_open_count++;
        MY_INFO_PRINTK( 0, "ERROR_LEVEL:" "keyarray open count : %d\n",g_tp->keyarray_open_count );      
    }	
    else
    {	
		rc = -EFAULT;
    }
    mutex_unlock(&g_tp->mutex);
	#endif
    PRINT_OUT
    return rc;
}

static void touchpad_keyarray_close(struct input_dev *dev)
{
	PRINT_IN
    #if 0
	mutex_lock(&g_tp->mutex);
    if( g_tp->keyarray_open_count )
    {
        g_tp->keyarray_open_count--;
        MY_INFO_PRINTK( 4, "INFO_LEVEL:" " keyarray still opened %d times\n",g_tp->keyarray_open_count );        
    }
    mutex_unlock(&g_tp->mutex);
	#endif
    PRINT_OUT
}

static int keyarray_register_input( struct input_dev **input,
                              struct i2c_client *client )
{
  int rc = 0;
  struct input_dev *input_dev;
  
  PRINT_IN
  input_dev = input_allocate_device();
  if ( !input_dev )
  {
    rc = -ENOMEM;
    return rc;
  }

  input_dev->name = ATMEL_TOUCHSCREEN_KEYARRAY_DRIVER_NAME;
  input_dev->phys = "atmel_touchscreen_keyarray/event0";
  input_dev->id.bustype = BUS_I2C;
  input_dev->id.vendor = 0x0001;
  input_dev->id.product = 0x0002;
  input_dev->id.version = 0x0100;
  input_dev->dev.parent = &client->dev;

  input_dev->open = touchpad_keyarray_open;
  input_dev->close = touchpad_keyarray_close;
  input_dev->event = touchpad_keyarray_event;

  input_dev->evbit[0] = BIT_MASK(EV_KEY);
  
  set_bit(KEY_MENU, input_dev->keybit);
  set_bit(KEY_HOME, input_dev->keybit);
  set_bit(KEY_BACK, input_dev->keybit);
  set_bit(KEY_F2, input_dev->keybit);
  set_bit(KEY_F5, input_dev->keybit);
  set_bit(KEY_F6, input_dev->keybit);

  rc = input_register_device( input_dev );
  if ( rc )
  {
    MY_INFO_PRINTK( 0,"ERROR_LEVEL:""failed to register keyarray input device\\n");
    input_free_device( input_dev );
  }
  else
  {
    *input = input_dev;
  }
  PRINT_OUT
  return rc;
}

static int touchpad_open(struct input_dev *dev)
{
    int rc = 0;
    
    PRINT_IN
    mutex_lock(&g_tp->mutex);
    if( g_tp->open_count == 0 )
    {
        g_tp->open_count++;
        MY_INFO_PRINTK( 4, "INFO_LEVEL:" "open count : %d\n",g_tp->open_count );      
    }	
    else
    {	
 	rc = -EFAULT;
    }
    mutex_unlock(&g_tp->mutex);
    PRINT_OUT
    return rc;
}

static void touchpad_close(struct input_dev *dev)
{
    PRINT_IN
    mutex_lock(&g_tp->mutex);
    if( g_tp->open_count )
    {
        g_tp->open_count--;
        MY_INFO_PRINTK( 4, "INFO_LEVEL:" "input device still opened %d times\n",g_tp->open_count );        
    }
    mutex_unlock(&g_tp->mutex);
    PRINT_OUT
}

static int touchpad_register_input( struct input_dev **input,
                                    struct atmel_touchscreen_platform_data_t *pdata,
                                    struct i2c_client *client )
{
    int rc = 0;
    struct input_dev *input_dev;
    int i;
    
    PRINT_IN
    i = 0;
    input_dev = input_allocate_device();
    if ( !input_dev ) {
        rc = -ENOMEM;
        return rc;
    }
    input_dev->name = ATMEL_TOUCHSCREEN_DRIVER_NAME;
    input_dev->phys = "atmel_touchscreen/input0";
    input_dev->id.bustype = BUS_I2C;
    input_dev->id.vendor = 0x0001;
    input_dev->id.product = 0x0002;
    input_dev->id.version = 0x0100;
    input_dev->dev.parent = &client->dev;
    input_dev->open = touchpad_open;
    input_dev->close = touchpad_close;
    
    input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
    #if 0
    input_dev->absbit[BIT_WORD(ABS_HAT0X)] |= BIT(ABS_HAT0X) | BIT(ABS_HAT0Y);
    input_dev->keybit[BIT_WORD(BTN_BASE2)] |= BIT_MASK(BTN_BASE2);
    #endif
    
    input_dev->keybit[BIT_WORD(BTN_TOUCH)] |= BIT_MASK(BTN_TOUCH);
    
	g_tp->atmel_x_max = maxTouchCfg_T9_obj.value_array[18] | maxTouchCfg_T9_obj.value_array[19] << 8;
	g_tp->atmel_y_max = maxTouchCfg_T9_obj.value_array[20] | maxTouchCfg_T9_obj.value_array[21] << 8;
	if(g_tp->atmel_y_max == 0) 
	    g_tp->atmel_y_max = ATMEL_Y_MAX;
	if(g_tp->atmel_x_max == 0) 
		g_tp->atmel_x_max = ATMEL_X_MAX;
    MY_INFO_PRINTK( 4, "INFO_LEVEL:" " atmel_x_max : %d\n",(int)g_tp->atmel_x_max );
    MY_INFO_PRINTK( 4, "INFO_LEVEL:" " atmel_y_max : %d\n",(int)g_tp->atmel_y_max );
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, ATMEL_TOUCAN_PANEL_X, 0, 0); 
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, ATMEL_TOUCAN_PANEL_Y, 0, 0); 
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR,  0, ATMEL_TOUCAN_PANEL_X, 0, 0); 
	input_set_abs_params(input_dev, ABS_MT_WIDTH_MAJOR,  0, ATMEL_TOUCAN_PANEL_X, 0, 0); 
	input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0,1, 0, 0);

    rc = input_register_device( input_dev );

    if ( rc )
    {
        MY_INFO_PRINTK( 0,"ERROR_LEVEL:""failed to register input device\\n");
        input_free_device( input_dev );
    }else {
        *input = input_dev;
    }
    PRINT_OUT
    return rc;
}

static int __devinit touchpad_probe(struct i2c_client *client, const struct i2c_device_id *id )
{
	int      result = 0;
	struct   atmel_touchscreen_platform_data_t *pdata;
    int i;
    	
	PRINT_IN
	g_tp = kzalloc( sizeof(struct touchpad_t), GFP_KERNEL );
    if( !g_tp )
    {
        result = -ENOMEM;
        PRINT_OUT
        return result;
    }
    g_tp->multi = kzalloc( sizeof(struct tp_multi_t), GFP_KERNEL );
    if( !g_tp->multi )
    {
        result = -ENOMEM;
		PRINT_OUT
		kfree(g_tp);
        return result;
    }
        
    for (i = 0 ; i < ATMEL_REPORT_POINTS ; i++ )
	{
		g_tp->msg[i].coord.x = 0;
		g_tp->msg[i].coord.y = 0;
		g_tp->msg[i].state = RELEASE;
	}
    pdata = client->dev.platform_data;
    g_tp->gpio_num = pdata->gpioirq;
    g_tp->gpio_rst = pdata->gpiorst;
    g_tp->irq = MSM_GPIO_TO_INT( g_tp->gpio_num );
    g_tp->client_4_i2c = client;
    mutex_init(&g_tp->mutex);
    
    result = touchpad_poweron_device(1);
    if(result)
	{
    	MY_INFO_PRINTK( 0, "ERROR_LEVEL:" "touchpad_probe: failed to power on device\n" );
		touchpad_poweron_device(0);
        kfree(g_tp->multi);
		kfree(g_tp);
        PRINT_OUT
        return result;
    }
	
    result = touchpad_setup_gpio( g_tp->gpio_num,g_tp->gpio_rst );
    if( result )
	{
        MY_INFO_PRINTK( 0, "ERROR_LEVEL:" "touchpad_probe: failed to setup gpio\n" );
		touchpad_poweron_device(0);
        touchpad_release_gpio( g_tp->gpio_num,g_tp->gpio_rst );
		kfree(g_tp->multi);
		kfree(g_tp);	
        PRINT_OUT
        return result;
    }
        
    result = touchpad_config_gpio( g_tp->gpio_num, g_tp->gpio_rst );
    if( result )
    {
        MY_INFO_PRINTK( 0, "ERROR_LEVEL:" "touchpad_probe: failed to config gpio\n" );
		touchpad_poweron_device(0);
        touchpad_release_gpio( g_tp->gpio_num,g_tp->gpio_rst );
        kfree(g_tp->multi);
		kfree(g_tp);
        PRINT_OUT
        return result;
    }    
        
    result = gpio_get_value(g_tp->gpio_num);
    if( result )
    {
        MY_INFO_PRINTK( 0, "ERROR_LEVEL:" "touchpad_probe: failed to get gpio_num value %d (result=%d)\n",g_tp->gpio_num,result );
		touchpad_poweron_device(0);
        touchpad_release_gpio( g_tp->gpio_num,g_tp->gpio_rst );
        kfree(g_tp->multi);
		kfree(g_tp);
        PRINT_OUT
        return result;
    }
    MY_INFO_PRINTK( 4, "TEST_INFO_LEVEL:" "touchpad_probe: get gpio_num value %d (result=%d)\n",g_tp->gpio_num, result );
    
    result = touchpad_detect_mXT224(g_tp);
    if( result )
    {
        MY_INFO_PRINTK( 0, "ERROR_LEVEL:" "touchpad_probe: failed to detect\n" );
		touchpad_poweron_device(0);
        touchpad_release_gpio( g_tp->gpio_num,g_tp->gpio_rst );
        kfree(g_tp->multi);
		kfree(g_tp);
        PRINT_OUT
        return result;
    }
   	client->driver = &i2c_touchpad_driver;
    i2c_set_clientdata( client, g_tp );
    	     
    result = calculate_infoblock_crc(g_tp);
    if( result )
    {
    	MY_INFO_PRINTK( 0, "ERROR_LEVEL:" "touchpad_probe: failed to calculate infoblock crc\n" );
		touchpad_poweron_device(0);
    	touchpad_release_gpio( g_tp->gpio_num,g_tp->gpio_rst );
    	kfree(g_tp->multi);
		kfree(g_tp);
        PRINT_OUT
        return result;
    }
    	
    result = power_on_flow_one_way(g_tp);
    if( result )
    {
    	MY_INFO_PRINTK( 0, "ERROR_LEVEL:" "touchpad_probe: failed to power_on_flow_one_way\n" );
		touchpad_poweron_device(0);
    	touchpad_release_gpio( g_tp->gpio_num,g_tp->gpio_rst );
    	kfree(g_tp->multi);
		kfree(g_tp);
        PRINT_OUT
        return result;
    }
    	
    result = touchpad_register_input( &g_tp->input, pdata, client );
    if( result )
    {
    	MY_INFO_PRINTK( 0, "ERROR_LEVEL:" "touchpad_probe: failed to register input\n" );
		touchpad_poweron_device(0);
    	touchpad_release_gpio( g_tp->gpio_num, g_tp->gpio_rst );
    	kfree(g_tp->multi);
		kfree(g_tp);
    	PRINT_OUT
		return result;
    }
    input_set_drvdata(g_tp->input, g_tp);    

    result = keyarray_register_input( &g_tp->keyarray_input, client );
    if( result )
    {
		MY_INFO_PRINTK( 0, "ERROR_LEVEL:" "touchpad_probe: failed to register keyarray input\n" );
		touchpad_poweron_device(0);
		input_unregister_device( g_tp->input );
		input_free_device( g_tp->input ); 
    	touchpad_release_gpio( g_tp->gpio_num, g_tp->gpio_rst );
    	kfree(g_tp->multi);
		kfree(g_tp);
		PRINT_OUT
		return result;
    }
    input_set_drvdata(g_tp->keyarray_input, g_tp); 
    	
    INIT_DELAYED_WORK( &g_tp->touchpad_work, touchpad_irqWorkHandler );
    g_tp->touchpad_wqueue = create_singlethread_workqueue("ATMEL_Touchpad_Wqueue");
    if (!g_tp->touchpad_wqueue)
    {
		MY_INFO_PRINTK( 0, "ERROR_LEVEL:" "touchpad_probe: failed to create singlethread workqueue\n" );
		touchpad_poweron_device(0);
    	result = -ESRCH;
    	input_unregister_device( g_tp->input );
		input_free_device( g_tp->input );
    	touchpad_release_gpio( g_tp->gpio_num, g_tp->gpio_rst );
    	kfree(g_tp->multi);
		kfree(g_tp);
    	PRINT_OUT
    	return result;
    }
    	
    init_waitqueue_head( &g_tp->multi->wait );
 
    INIT_DELAYED_WORK( &g_tp->chk_work, read_tch_atch );
    g_tp->chk_wqueue = create_singlethread_workqueue("ATMEL_chk_Wqueue");
    	
    result = request_irq( g_tp->irq, touchpad_irqHandler, IRQF_TRIGGER_LOW,"ATMEL_Touchpad_IRQ", g_tp );                      
    MY_INFO_PRINTK( 4, "TEST_INFO_LEVEL:" "---- request_irq ----\n" );
    if ( result )
    {
		MY_INFO_PRINTK( 0,"ERROR_LEVEL:""request interrupt: irq %d requested failed\n", g_tp->irq);
		touchpad_poweron_device(0);
    	result = -EFAULT;
    	input_unregister_device( g_tp->input );
		input_free_device( g_tp->input );
    	destroy_workqueue( g_tp->touchpad_wqueue );
    	touchpad_release_gpio( g_tp->gpio_num, g_tp->gpio_rst );
    	kfree(g_tp->multi);
		kfree(g_tp);
    	PRINT_OUT
    	return result;
    }
    	
    result = misc_register( &tp_misc_device );
    if( result )
    {
       	MY_INFO_PRINTK( 0,"ERROR_LEVEL:" "failed register misc driver\n" );
        result = -EFAULT;
    	input_unregister_device( g_tp->input );
		input_free_device( g_tp->input );
    	destroy_workqueue( g_tp->touchpad_wqueue );
		touchpad_poweron_device(0);
    	touchpad_release_gpio( g_tp->gpio_num, g_tp->gpio_rst );
    	kfree(g_tp->multi);
		kfree(g_tp);
		disable_irq(g_tp->irq); 	
    	PRINT_OUT
		return result;       
    }
	
    g_tp->touch_early_suspend.level = 150;
    g_tp->touch_early_suspend.suspend = tp_suspend;
    g_tp->touch_early_suspend.resume = tp_resume;
    register_early_suspend(&g_tp->touch_early_suspend);
	
	g_tp->check_home_key = 1;
    PRINT_OUT
    return 0;
}

static int __init touchpad_init(void)
{
	int rc = 0;
        
   	PRINT_IN            
	MY_INFO_PRINTK( 1,"INFO_LEVEL:""system_rev=0x%x\n",system_rev);
    i2c_touchpad_driver.driver.name = ATMEL_TOUCHSCREEN_DRIVER_NAME;
    rc = i2c_add_driver( &i2c_touchpad_driver );
    PRINT_OUT
    return rc;
}
module_init(touchpad_init);

static void __exit touchpad_exit(void)
{  
	PRINT_IN
    i2c_del_driver( &i2c_touchpad_driver );
    input_unregister_device( g_tp->input );
	input_free_device( g_tp->input );
    destroy_workqueue( g_tp->touchpad_wqueue );
    touchpad_release_gpio( g_tp->gpio_num, g_tp->gpio_rst );
    kfree(g_tp);
    PRINT_OUT
}
module_exit(touchpad_exit);

MODULE_DESCRIPTION("ATMEL xMT224 touchpad driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Emily Jiang");
MODULE_ALIAS("platform:atmel_xMT224_touch");
