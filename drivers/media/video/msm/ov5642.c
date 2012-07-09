/*
 * Copyright (c) 2008-2009 QUALCOMM USA, INC.
 *
 * All source code in this file is licensed under the following license
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can find it at http://www.fsf.org
 */

#include "ov5642.h"
#include <linux/slab.h>

struct ov5642_work {
	struct work_struct work;
};

static struct  ov5642_work *ov5642_sensorw;
static struct  i2c_client *ov5642_client = NULL;

struct ov5642_ctrl {
    const struct msm_camera_sensor_info *sensordata;
    struct ov5642_basic_reg_arrays	*reg_map;
    struct ov5642_ae_func_array		*ae_func;
    enum ov5642_anti_banding_t		banding_type;
    enum ov5642_white_balance_t		wb_type;
    enum ov5642_contrast_t		contrast;
    enum ov5642_brightness_t		brightness;
    int scene_value;
    int sensor_mode;
};


static struct ov5642_ctrl *ov5642_ctrl;

static DECLARE_WAIT_QUEUE_HEAD(ov5642_wait_queue);
DECLARE_MUTEX(ov5642_sem);

extern struct ov5642_basic_reg_arrays ov5642_1C_regs;
extern struct ov5642_basic_reg_arrays ov5642_1D_regs;
extern struct ov5642_reg_group_t ov5642_resolution_group[5];
extern struct ov5642_special_reg_groups ov5642_special_regs;

#ifdef USE_AF
extern struct ov5642_af_func_array ov5642_af_func;
#endif
extern struct ov5642_awb_func_array ov5642_awb_func;
extern struct ov5642_night_mode_func_array ov5642_night_mode_func;

extern struct ov5642_ae_func_array ov5642_1C_ae_func;
extern struct ov5642_ae_func_array ov5642_1D_ae_func;
static inline int isValidClient( void *client )
{
    return ((client != NULL)? 0: -1);
}


static void ov5642_reset(struct msm_camera_sensor_info *dev)
{
    gpio_set_value( dev->sensor_pwd, 0 );
    msleep(3);
    gpio_set_value( dev->sensor_reset, 1 );

    return;
}

#define MAX_RETRY_COUNT 5

int32_t ov5642_i2c_read( struct ov5642_reg_t *regs, uint32_t size)
{
    uint32_t i;
    int retryCnt;
    int rc;
    unsigned char buf[2];
    struct i2c_msg msg[] =
    {
        [0] = {
            .addr   = OV5642_SLAVE_ADDR,
            .flags  = 0,
            .buf    = (void *)buf,
            .len    = 2
        },
        [1] = {
            .addr   = OV5642_SLAVE_ADDR,
            .flags  = I2C_M_RD,
            .len    = 1
        }
    };

    if( isValidClient(ov5642_client) )
    {
        CSDBG("ov5642_i2c_read: invalid client\n");
        return -EFAULT;
    }

    for( i=0; i<size; i++ )
    {
        buf[0] = (regs->addr & 0xFF00)>>8;
        buf[1] = (regs->addr & 0x00FF);
        msg[1].buf = (void *)&regs->data;

#ifndef MAX_RETRY_COUNT
        if ( ARRAY_SIZE(msg) != i2c_transfer( ov5642_client->adapter, msg, 2) ) {
            CSDBG("ov5642_i2c_read: read %Xh failed\n", (buf[0]<<8)|buf[1] );
            return -EIO;
        }
#else
        for( retryCnt=0; retryCnt<MAX_RETRY_COUNT; retryCnt++)
        {
            rc = i2c_transfer( ov5642_client->adapter, msg, 2);
            if( ARRAY_SIZE(msg) == rc )
            {
                if( retryCnt > 0 )
                    CSDBG("ov5642_i2c_read: read %Xh=0x%X successfull at retryCnt=%d\n",
                               (buf[0]<<8)|buf[1], regs->data, retryCnt);
                break;
            }
        }

        if( (rc != ARRAY_SIZE(msg)) && (retryCnt == MAX_RETRY_COUNT) )
        {
            CSDBG( "ov5642_i2c_read: read %Xh failed\n", (buf[0]<<8)|buf[1] );
            return -EIO;
        }
#endif
        regs++;
    }


    return 0;
}

int32_t ov5642_i2c_write( struct ov5642_reg_t *regs, uint32_t size)
{
    uint32_t i;
    int retryCnt;
    int rc;
    unsigned char buf[3];
    struct i2c_msg msg[] =
    {
        {
    	.addr = OV5642_SLAVE_ADDR,
    	.flags = 0,
    	.len = 3,
    	.buf = buf,
        }
    };

    if( isValidClient(ov5642_client) )
    {
        CSDBG("ov5642_i2c_write: invalid client\n");
        return -EFAULT;
    }

    for( i=0; i<size; i++ )
    {
        if( OV5642_BIT_OPERATION_TAG == regs->addr )
        {
            struct ov5642_reg_t writableReg;
            char mask, value;
            mask = regs->data;
            i++;
            regs++;
            value = regs->data;
            writableReg.addr = regs->addr;
            if( !ov5642_i2c_read( &writableReg, 1 ) )
            {
                writableReg.data &= (~mask);
                writableReg.data |= value;
            }else {
                CSDBG("ov5642_i2c_write: read %Xh before write failed\n", writableReg.addr);
                return -EIO;
            }

            buf[0] = (writableReg.addr & 0xFF00)>>8;
            buf[1] = (writableReg.addr & 0x00FF);
            buf[2] = writableReg.data;
        }else {
            buf[0] = (regs->addr & 0xFF00)>>8;
            buf[1] = (regs->addr & 0x00FF);
            buf[2] = regs->data;
        }

#ifndef MAX_RETRY_COUNT
        if ( ARRAY_SIZE(msg) != i2c_transfer( ov5642_client->adapter, msg, 1) ) {
            CSDBG("ov5642_i2c_write: write %Xh=0x%X failed\n", (buf[0]<<8)|buf[1], buf[2] );
            return -EIO;
        }
#else
        for( retryCnt=0; retryCnt<MAX_RETRY_COUNT; retryCnt++)
        {
            rc = i2c_transfer( ov5642_client->adapter, msg, 1);
            if( ARRAY_SIZE(msg) == rc )
            {
                if( retryCnt > 0 )
                    CSDBG("ov5642_i2c_write: write %Xh=0x%X successfull at retryCnt=%d\n",
                               (buf[0]<<8)|buf[1], buf[2], retryCnt);
                break;
            }
        }

        if( (rc != ARRAY_SIZE(msg)) && (retryCnt == MAX_RETRY_COUNT) )
        {
            CSDBG( "ov5642_i2c_write: write %Xh =0x%X failed\n", (buf[0]<<8)|buf[1], buf[2] );
            return -EIO;
        }
#endif

        if( 0x3008 == regs->addr && (regs->data & 0x80))
        {
            CDBG("ov5642_i2c_write: sleep 5 ms after software reset\n");
            msleep(5);
        }

        regs++;
    }

    return 0;
}


int32_t ov5642_i2c_burst_read( unsigned short addr, unsigned char *data, uint32_t size)
{
    int retryCnt;
    int rc;
    unsigned char buf[2];
    struct i2c_msg msg[] =
    {
        [0] = {
            .addr   = OV5642_SLAVE_ADDR,
            .flags  = 0,
            .buf    = (void *)buf,
            .len    = 2
        },
        [1] = {
            .addr   = OV5642_SLAVE_ADDR,
            .flags  = I2C_M_RD,
            .buf    = (void *)data,
            .len    = size
        }
    };

    if( isValidClient(ov5642_client) )
    {
        CSDBG("ov5642_i2c_burst_read: invalid client\n");
        return -EFAULT;
    }

    buf[0] = (addr & 0xFF00)>>8;
    buf[1] = (addr & 0x00FF);

#ifndef MAX_RETRY_COUNT
    if ( i2c_transfer(ov5642_client->adapter, msg, ARRAY_SIZE(msg)) != ARRAY_SIZE(msg) ) {
        CSDBG( "ov5642_i2c_burst_read: read %Xh %d bytes failed\n", addr, size );
        return -EIO;
    }
#else
    for( retryCnt=0; retryCnt<MAX_RETRY_COUNT; retryCnt++)
    {
        rc = i2c_transfer( ov5642_client->adapter, msg, ARRAY_SIZE(msg));
        if( ARRAY_SIZE(msg) == rc )
        {
            if( retryCnt > 0 )
                CSDBG("ov5642_i2c_burst_read: read %Xh %d bytes successfully at retryCnt=%d\n",
                           addr, size, retryCnt);
            break;
        }
    }

    if( (rc != ARRAY_SIZE(msg)) && (retryCnt == MAX_RETRY_COUNT) )
    {
        CSDBG( "ov5642_i2c_burst_read: read %Xh %d bytes failed\n", addr, size );
        return -EIO;
    }
#endif

    return 0;
}


int32_t ov5642_i2c_burst_write( char const *data, uint32_t size)
{
    int retryCnt;
    int rc;

    struct i2c_msg msg[] =
    {
        [0] = {
            .addr   = OV5642_SLAVE_ADDR,
            .flags  = 0,
            .buf    = (void *)data,
            .len    = size
        },
    };

    if( isValidClient(ov5642_client) )
    {
        CSDBG("ov5642_i2c_burst_write: invalid client\n");
        return -EFAULT;
    }

#ifndef MAX_RETRY_COUNT
    if ( i2c_transfer(ov5642_client->adapter, msg, ARRAY_SIZE(msg)) != ARRAY_SIZE(msg) ) {
        CSDBG( "ov5642_i2c_burst_write: write %02x%02xh %d bytes failed\n", *data, *(data+1), size );
        return -EIO;
    }
#else
    for( retryCnt=0; retryCnt<MAX_RETRY_COUNT; retryCnt++)
    {
        rc = i2c_transfer( ov5642_client->adapter, msg, ARRAY_SIZE(msg));
        if( ARRAY_SIZE(msg) == rc )
        {
            if( retryCnt > 0 )
                CSDBG("ov5642_i2c_burst_write: write %Xh %d bytes successfully at retryCnt=%d\n",
                           *data, size, retryCnt);
            break;
        }
    }

    if( (rc != ARRAY_SIZE(msg)) && (retryCnt == MAX_RETRY_COUNT) )
    {
        CSDBG( "ov5642_i2c_burst_write: write %Xh %d bytes failed\n", *data, size );
        return -EIO;
    }
#endif

    return 0;
}

static long ov5642_reg_init_iq(void)
{
    long rc = 0;
    struct ov5642_reg_burst_group_t const *pSpecRegBurstGroup;
#ifdef PRESET_IQ
    struct ov5642_reg_group_t const *pSpecRegGroup;

    pSpecRegGroup = ov5642_special_regs.ae_target_group;
    rc |= ov5642_i2c_write( (pSpecRegGroup+GENERAL_AE_TARGET)->start,
                            (pSpecRegGroup+GENERAL_AE_TARGET)->len );

    pSpecRegGroup = ov5642_special_regs.frame_rate_group;
    rc |= ov5642_i2c_write( (pSpecRegGroup+ONE_OF_THREE_FPS)->start,
                            (pSpecRegGroup+ONE_OF_THREE_FPS)->len );

    pSpecRegGroup = ov5642_special_regs.denoise_group;
    rc |= ov5642_i2c_write( (pSpecRegGroup+DENOISE_AUTO_DEFAULT)->start,
                            (pSpecRegGroup+DENOISE_AUTO_DEFAULT)->len );

    pSpecRegBurstGroup = ov5642_special_regs.ae_weighting_group;
    rc |= ov5642_i2c_burst_write( (pSpecRegBurstGroup+GENERAL_AE)->start,
                                  (pSpecRegBurstGroup+GENERAL_AE)->len );
#endif

    pSpecRegBurstGroup = ov5642_ctrl->reg_map->IQ_misc_groups->lens_c_group;
    rc |= ov5642_i2c_burst_write( (char *)pSpecRegBurstGroup->start,
                                  pSpecRegBurstGroup->len );

#ifdef PRESET_IQ
    pSpecRegBurstGroup = ov5642_special_regs.gamma_group;
    rc |= ov5642_i2c_burst_write( (pSpecRegBurstGroup+GENERAL_GAMMA)->start,
                                  (pSpecRegBurstGroup+GENERAL_GAMMA)->len );

    pSpecRegBurstGroup = ov5642_special_regs.color_matrix_group;
    rc |= ov5642_i2c_burst_write( (pSpecRegBurstGroup+GENERAL_CMX)->start,
                                  (pSpecRegBurstGroup+GENERAL_CMX)->len );
#endif

    pSpecRegBurstGroup = ov5642_ctrl->reg_map->IQ_misc_groups->awb_group;
    rc |= ov5642_i2c_burst_write( (char *)pSpecRegBurstGroup->start,
                                  pSpecRegBurstGroup->len );

    return rc;
}

static long ov5642_reg_init(void)
{
    int32_t rc = 0;
#ifdef CHECK_PERF
struct timespec start;
struct timespec temp;
#endif

    CDBG("ov5642_reg_init()+\n");

#ifdef CHECK_PERF
    start = CURRENT_TIME;
#endif

    rc |= ov5642_i2c_write( (struct ov5642_reg_t *)ov5642_ctrl->reg_map->default_array,
                            ov5642_ctrl->reg_map->default_length );

#ifdef CHECK_PERF
            temp = timespec_sub(CURRENT_TIME, start);
            CSDBG("ov5642_reg_init(%d): %ld ms\n",
                     __LINE__, ((long)temp.tv_sec*1000)+((long)temp.tv_nsec/1000000));
#endif

    rc |= ov5642_i2c_write( (struct ov5642_reg_t *)ov5642_ctrl->reg_map->IQ_array,
                             ov5642_ctrl->reg_map->IQ_length );
    if( system_rev >= EVT2_Band125 )
        rc |= ov5642_reg_init_iq();

    rc |= ov5642_i2c_write( (struct ov5642_reg_t *)ov5642_ctrl->reg_map->preview_array,
                           ov5642_ctrl->reg_map->preview_length );


    ov5642_awb_func.init_awb();
    ov5642_night_mode_func.init_night_mode();
#ifdef USE_AF
    ov5642_af_func.init_auto_focus();
#endif

#ifdef CHECK_PERF
            temp = timespec_sub(CURRENT_TIME, start);
            CSDBG("ov5642_reg_init(%d): %ld ms\n",
                     __LINE__, ((long)temp.tv_sec*1000)+((long)temp.tv_nsec/1000000));
#endif

    CDBG("ov5642_reg_init()-, rc=%d\n", rc);

    return rc;
}


static long ov5642_engineer_mode( int type, void *pCfg )
{
    long rc = 0;

    switch(type)
    {
        case CFG_DUMP_SINGLE_REGISTER:
        {
            struct ov5642_reg_t reg;
            struct os_engineer_reg_cfg *pOrigCfg;

            pOrigCfg = (struct os_engineer_reg_cfg *)pCfg;
            reg.addr = (unsigned short)pOrigCfg->addr;

            rc = ov5642_i2c_read( &reg, 1);
            if( !rc )
            {
                CSDBG("ov5642_engineer_mode: read %Xh=0x%X\n", reg.addr, reg.data);
                pOrigCfg->data = (uint16_t)reg.data;
            }
            break;
        }
        case CFG_SET_SINGLE_REGISTER:
        {
            struct ov5642_reg_t reg;
            struct os_engineer_reg_cfg *pOrigCfg;

            pOrigCfg = (struct os_engineer_reg_cfg *)pCfg;
            reg.addr = (unsigned short)pOrigCfg->addr;
            reg.data = (unsigned char)pOrigCfg->data;

            rc = ov5642_i2c_write( &reg, 1 );
            break;
        }
        case CFG_DUMP_REGISTERS_TO_FILE:
        {
            struct os_engineer_file_cfg *pOrigCfg;
            char buf[256];

            pOrigCfg = (struct os_engineer_file_cfg *)pCfg;
            if( pOrigCfg->len > 256 || NULL == pOrigCfg->buf )
            {
                rc = -EFAULT;
                CSDBG("ov5642_engineer_mode: invalid args\n");
                break;
            }

            memset(buf, 0, sizeof(buf));
            if( (rc = ov5642_i2c_burst_read( pOrigCfg->addr , buf, pOrigCfg->len )) )
                break;

            if( copy_to_user( pOrigCfg->buf,
                              buf,
                              pOrigCfg->len) )
            {
                rc = -EFAULT;
                break;
            }

            break;
        }
        case CFG_SET_REGISTERS_FROM_FILE:
        {
            struct os_engineer_file_cfg *pOrigCfg;
            char buf[1024];
            int i;

            pOrigCfg = (struct os_engineer_file_cfg *)pCfg;
            if( pOrigCfg->len > 1024 || NULL == pOrigCfg->buf )
            {
                rc = -EFAULT;
                CSDBG("ov5642_engineer_mode: invalid args\n");
                break;
            }

            memset(buf, 0, sizeof(buf));
            if( copy_from_user( buf,
                                pOrigCfg->buf,
                                pOrigCfg->len) )
            {
                rc = -EFAULT;
                break;
            }

            for(i=0; i < pOrigCfg->len; i+=3)
            {
                struct ov5642_reg_t reg;

                reg.addr = (buf[i] << 8) & 0xFF00;
                reg.addr |= buf[i+1];
                reg.data = buf[i+2];

                if( OV5642_BIT_OPERATION_TAG == reg.addr )
                {
                    char mask = buf[i+2];

                    if( i+3 > pOrigCfg->len )
                    {
                        CSDBG("ov5642_engineer_mode: bit operation is broken\n");
                        rc = -1;
                        break;
                    }

                    i += 3;
                    reg.addr = (buf[i] << 8) & 0xFF00;
                    reg.addr |= buf[i+1];
                    rc = ov5642_i2c_read( &reg, 1);
                    if( rc )
                        break;

                    CSDBG("ov5642_engineer_mode: read  %Xh=%X\n", reg.addr, reg.data);

                    reg.data &= ~mask;
                    reg.data |= buf[i+2];
                }

                CSDBG("ov5642_engineer_mode: write %Xh=%X\n", reg.addr, reg.data);
                rc = ov5642_i2c_write( &reg, 1 );
                if( rc )
                    break;

            }

            break;
        }
        default:
            CSDBG("ov5642_engineer_mode: not supported type\n");
            rc = -EFAULT;
            break;
    }

    return rc;
}

static long ov5642_set_sharpness( int value )
{
    long rc = 0;
    enum ov5642_sharpness_t sharpness = (enum ov5642_sharpness_t)value;
    struct ov5642_reg_group_t const *pSpecRegGroup;

    CDBG( "ov5642_set_sharpness: sharpness=%d\n", sharpness );

    pSpecRegGroup = ov5642_special_regs.sharpness_group;
    rc = ov5642_i2c_write( (pSpecRegGroup+sharpness)->start,
                           (pSpecRegGroup+sharpness)->len );

    return rc;
}

static long ov5642_set_sensor_mode(int mode,
                                   int res)
{
    long rc = 0;
#ifdef CHECK_PERF
struct timespec start;
struct timespec temp;
#endif

    CDBG( "ov5642_set_sensor_mode+, mode=%d, res=%d\n", mode, res);
#ifdef CHECK_PERF
    start = CURRENT_TIME;
#endif

    switch( mode )
    {
        case SENSOR_PREVIEW_MODE:
        {
            ov5642_night_mode_func.finish_night_mode(0);
            ov5642_night_mode_func.disable_night_mode();

            ov5642_awb_func.finish_awb(1);
            ov5642_ctrl->ae_func->snapshot_to_preview(ov5642_ctrl->wb_type);
            rc = ov5642_i2c_write( (struct ov5642_reg_t *)ov5642_ctrl->reg_map->preview_array,
                                   ov5642_ctrl->reg_map->preview_length );
            if( rc )
                break;

            rc = ov5642_set_sharpness(SHARPNESS_AUTO_DEFAULT);

            if (ov5642_ctrl->sensor_mode == SENSOR_RAW_SNAPSHOT_MODE)
                ov5642_night_mode_func.convert_night_mode(ov5642_ctrl->scene_value, 4);
            else
                ov5642_night_mode_func.convert_night_mode(ov5642_ctrl->scene_value, 2);

            ov5642_awb_func.convert_awb();

            if( rc )
                break;

#ifdef CHECK_PERF
            temp = timespec_sub(CURRENT_TIME, start);
            CDBG("ov5642_set_sensor_mode(%d): %ld ms\n",
                     __LINE__, ((long)temp.tv_sec*1000)+((long)temp.tv_nsec/1000000));
#endif

#ifdef USE_AF
            rc = ov5642_af_func.prepare_auto_focus();

            if( rc )
                break;
#endif
            break;
        }
        case SENSOR_SNAPSHOT_MODE:
        {
            ov5642_awb_func.finish_awb(1);
            ov5642_night_mode_func.finish_night_mode(0);
            break;
        }
        case SENSOR_RAW_SNAPSHOT_MODE:
        case SENSOR_ENGINEER_MODE:
        {
            int sharpness;

            ov5642_awb_func.finish_awb(1);
            ov5642_night_mode_func.finish_night_mode(0);
            rc = ov5642_ctrl->ae_func->preview_to_snapshot(ov5642_ctrl->banding_type);
            if( rc )
                break;

            if( CAMERA_BESTSHOT_OFF == ov5642_ctrl->scene_value )
                ov5642_night_mode_func.query_night_mode(&sharpness);
            else if( CAMERA_BESTSHOT_NIGHT == ov5642_ctrl->scene_value )
                sharpness = SHARPNESS_AUTO_DEFAULT;
            else
                sharpness = SHARPNESS_AUTO_P3;
            rc = ov5642_set_sharpness(sharpness);

            if( SENSOR_ENGINEER_MODE == mode )
                break;

            if( system_rev < EVT2_Band125 )
            {
                rc = ov5642_i2c_write( (struct ov5642_reg_t *)ov5642_ctrl->reg_map->snapshot_array,
                                       ov5642_ctrl->reg_map->snapshot_length );
            }

            if( res && !rc )
            {
                rc = ov5642_i2c_write( ov5642_resolution_group[res-1].start,
                                       ov5642_resolution_group[res-1].len );
            }
            break;
        }
        default:
        {
            CSDBG( "ov5642_set_sensor_mode: unknown mode %d\n", mode );
            rc = -1;
            break;
        }
    }

    ov5642_ctrl->sensor_mode = mode;

#ifdef CHECK_PERF
    temp = timespec_sub(CURRENT_TIME, start);
    CDBG("ov5642_set_sensor_mode(%d): %ld ms\n",
             __LINE__, ((long)temp.tv_sec*1000)+((long)temp.tv_nsec/1000000));
#endif

    CDBG( "ov5642_set_sensor_mode-\n");

    return rc;
}


static long ov5642_set_wb( int8_t whiteBalance )
{
    long rc = 0;
    enum ov5642_white_balance_t index;
    struct ov5642_reg_group_t const *pSpecRegGroup;

    CDBG( "ov5642_set_wb: wb=%d\n", whiteBalance );

    switch((enum camera_wb_t)whiteBalance) {
        case CAMERA_WB_AUTO: {
            index = AUTO_WB;
        }
        break;
        case CAMERA_WB_DAYLIGHT: {
            index = SUN_LIGHT_WB;
        }
        break;
        case CAMERA_WB_FLUORESCENT: {
            index = FLUORESCENT_WB;
        }
        break;
        case CAMERA_WB_INCANDESCENT: {
            index = INCANDESCENT_WB;
        }
        break;
        default: {
            CSDBG("ov5642_set_wb: unsupported wb %d\n", whiteBalance);
            rc = -EFAULT;
        }
        break;
    }

    if( rc )
        return rc;

    ov5642_awb_func.finish_awb(1);

    ov5642_ctrl->wb_type = index;
    pSpecRegGroup = ov5642_special_regs.white_balance_group;
    rc = ov5642_i2c_write( (pSpecRegGroup+index)->start,
                           (pSpecRegGroup+index)->len );

    return rc;
}

static long ov5642_set_brightness( int8_t brightness )
{
    long rc = 0;
    enum ov5642_brightness_t index;
    struct ov5642_reg_group_t const *pSpecRegGroup;

    CDBG( "ov5642_set_brightness: brightness=%d\n", brightness );

    if(brightness > 2)
        brightness = 2;
    else if(brightness < -2)
        brightness = -2;

    switch(brightness) {
        case +2: {
            index = BRIGHTNESS_POSITIVE_TWO;
        }
        break;
        case +1: {
            index = BRIGHTNESS_POSITIVE_ONE;
        }
        break;
        case 0: {
            index = BRIGHTNESS_POSITIVE_ZERO;
        }
        break;
        case -1: {
            index = BRIGHTNESS_NEGATIVE_ONE;
        }
        break;
        case -2: {
            index = BRIGHTNESS_NEGATIVE_TWO;
        }
        break;
        default: {
            CSDBG("ov5642_set_brightness: unsupported brightness %d\n", brightness);
            rc = -EFAULT;
        }
        break;
    }

    if( rc )
        return rc;

    if (ov5642_ctrl->brightness == index)
        return rc;

    CSDBG("ov5642_set_brightness: %d\n", index);
    pSpecRegGroup = ov5642_special_regs.brightness_group;
    rc = ov5642_i2c_write( (pSpecRegGroup+index)->start,
                           (pSpecRegGroup+index)->len );
    if (!rc)
        ov5642_ctrl->brightness = index;

    return rc;
}

static long ov5642_set_contrast( int8_t contrast )
{
    long rc = 0;
    enum ov5642_contrast_t index;
    struct ov5642_reg_group_t const *pSpecRegGroup;

    CDBG( "ov5642_set_contrast: contrast=%d\n", contrast );

    if(contrast > 2)
        contrast = 2;
    else if(contrast < -2)
        contrast = -2;

    switch(contrast) {
        case +2: {
            index = CONTRAST_POSITIVE_TWO;
        }
        break;
        case +1: {
            index = CONTRAST_POSITIVE_ONE;
        }
        break;
        case 0: {
            index = CONTRAST_POSITIVE_ZERO;
        }
        break;
        case -1: {
            index = CONTRAST_NEGATIVE_ONE;
        }
        break;
        case -2: {
            index = CONTRAST_NEGATIVE_TWO;
        }
        break;
        default: {
            CSDBG("ov5642_set_contrast: unsupported contrast %d\n", contrast);
            rc = -EFAULT;
        }
        break;
    }

    if( rc )
        return rc;

    if (ov5642_ctrl->contrast == index)
        return rc;

    CSDBG("ov5642_set_contrast: %d\n", index);
    pSpecRegGroup = ov5642_special_regs.contrast_group;
    rc = ov5642_i2c_write( (pSpecRegGroup+index)->start,
                           (pSpecRegGroup+index)->len );
    if (!rc)
        ov5642_ctrl->contrast = index;

    return rc;
}

static long ov5642_set_antibanding( int8_t banding )
{
    long rc = 0;
    enum ov5642_anti_banding_t index;
    struct ov5642_reg_group_t const *pSpecRegGroup;

    CDBG( "ov5642_set_antibanding: banding=%d\n", banding );

    switch((enum camera_antibanding_t)banding) {
        case CAMERA_ANTIBANDING_60HZ: {
            index = MANUAL_60HZ_BANDING;
        }
        break;
        case CAMERA_ANTIBANDING_50HZ: {
            index = MANUAL_50HZ_BANDING;
        }
        break;
        case CAMERA_ANTIBANDING_AUTO: {
            index = AUTO_BANDING;
        }
        break;
        default: {
            CSDBG("ov5642_set_antibanding: unsupported banding %d\n", banding);
            rc = -EFAULT;
        }
        break;
    }

    if( rc )
        return rc;

    pSpecRegGroup = ov5642_special_regs.anti_banding_group;
    rc = ov5642_i2c_write( (pSpecRegGroup+index)->start,
                           (pSpecRegGroup+index)->len );
    if (!rc)
        ov5642_ctrl->banding_type = index;

    return rc;
}

static long ov5642_set_scene( int8_t scene )
{
    long rc = 0;
    enum ov5642_frame_rate_t frameRate;
    enum ov5642_ae_weighting_t aeWeighting;
    enum ov5642_color_matrix_t colorMatrix;
    enum ov5642_ae_target_t aeTarget;
    enum ov5642_lens_c_t lensCorrection;
    enum ov5642_gamma_t gamma;
    enum ov5642_denoise_t denoise;
    enum ov5642_sharpness_t sharpness;
    struct ov5642_reg_group_t const *pSpecRegGroup;
    struct ov5642_reg_burst_group_t const *pSpecRegBurstGroup;

    CDBG( "ov5642_set_scene: scene=%d\n", scene );

    if (ov5642_ctrl->scene_value == scene)
        return rc;

    switch(scene) {
        case CAMERA_BESTSHOT_OFF: {
            frameRate = ONE_OF_THREE_FPS;
            aeWeighting = GENERAL_AE;
            colorMatrix = GENERAL_CMX;
            aeTarget = GENERAL_AE_TARGET;
            lensCorrection = GENERAL_LENS_C;
            gamma = GENERAL_GAMMA;
            denoise = DENOISE_AUTO_DEFAULT;
            sharpness = SHARPNESS_AUTO_DEFAULT;
        }
        break;
        case CAMERA_BESTSHOT_NIGHT: {
            frameRate = ONE_OF_FIVE_FPS;
            aeWeighting = LANDSCAPE_AE;
            colorMatrix = GENERAL_CMX;
            aeTarget = GENERAL_AE_TARGET;
            lensCorrection = NIGHT_LENS_C;
            gamma = NIGHT_GAMMA;
            denoise = DENOISE_AUTO_P1;
            sharpness = SHARPNESS_AUTO_DEFAULT;
        }
        break;
        case CAMERA_BESTSHOT_PORTRAIT: {
            frameRate = ONE_OF_THREE_FPS;
            aeWeighting = PORTRAIT_AE;
            colorMatrix = PERSON_CMX;
            aeTarget = GENERAL_AE_TARGET;
            lensCorrection = GENERAL_LENS_C;
            gamma = GENERAL_GAMMA;
            denoise = DENOISE_AUTO_DEFAULT;
            sharpness = SHARPNESS_AUTO_DEFAULT;
        }
        break;
        case CAMERA_BESTSHOT_LANDSCAPE: {
            frameRate = ONE_OF_THREE_FPS;
            aeWeighting = LANDSCAPE_AE;
            colorMatrix = LAND_CMX;
            aeTarget = GENERAL_AE_TARGET;
            lensCorrection = GENERAL_LENS_C;
            gamma = GENERAL_GAMMA;
            denoise = DENOISE_AUTO_DEFAULT;
            sharpness = SHARPNESS_AUTO_DEFAULT;
        }
        break;
        case CAMERA_BESTSHOT_SPORTS: {
            frameRate = ONE_OF_ONE_FPS;
            aeWeighting = GENERAL_AE;
            colorMatrix = GENERAL_CMX;
            aeTarget = GENERAL_AE_TARGET;
            lensCorrection = GENERAL_LENS_C;
            gamma = GENERAL_GAMMA;
            denoise = DENOISE_AUTO_DEFAULT;
            sharpness = SHARPNESS_AUTO_DEFAULT;
        }
        break;
        case CAMERA_BESTSHOT_SNOW: {
            frameRate = ONE_OF_ONE_FPS;
            aeWeighting = GENERAL_AE;
            colorMatrix = SNOW_CMX;
            aeTarget = GENERAL_AE_TARGET;
            lensCorrection = GENERAL_LENS_C;
            gamma = GENERAL_GAMMA;
            denoise = DENOISE_AUTO_DEFAULT;
            sharpness = SHARPNESS_AUTO_DEFAULT;
        }
        break;
        default: {
            CSDBG("ov5642_set_scene: unsupported scene %d\n", scene);
            rc = -EFAULT;
        }
        break;
    }

    if( rc )
        return rc;

    CSDBG("ov5642_set_scene: %d\n", scene);
    ov5642_night_mode_func.finish_night_mode(0);
    pSpecRegGroup = ov5642_special_regs.frame_rate_group;
    rc |= ov5642_i2c_write( (pSpecRegGroup+frameRate)->start,
                            (pSpecRegGroup+frameRate)->len );

    pSpecRegBurstGroup = ov5642_special_regs.ae_weighting_group;
    rc |= ov5642_i2c_burst_write( (pSpecRegBurstGroup+aeWeighting)->start,
                                  (pSpecRegBurstGroup+aeWeighting)->len );

    pSpecRegBurstGroup = ov5642_special_regs.color_matrix_group;
    rc |= ov5642_i2c_burst_write( (pSpecRegBurstGroup+colorMatrix)->start,
                                  (pSpecRegBurstGroup+colorMatrix)->len );

    pSpecRegGroup = ov5642_special_regs.ae_target_group;
    rc |= ov5642_i2c_write( (pSpecRegGroup+aeTarget)->start,
                            (pSpecRegGroup+aeTarget)->len );

    pSpecRegBurstGroup = ov5642_special_regs.gamma_group;
    rc |= ov5642_i2c_burst_write( (pSpecRegBurstGroup+gamma)->start,
                                  (pSpecRegBurstGroup+gamma)->len );

    pSpecRegBurstGroup = ov5642_special_regs.lens_c_group;
    rc |= ov5642_i2c_burst_write( (pSpecRegBurstGroup+lensCorrection)->start,
                                  (pSpecRegBurstGroup+lensCorrection)->len );

    pSpecRegGroup = ov5642_special_regs.denoise_group;
    rc |= ov5642_i2c_write( (pSpecRegGroup+denoise)->start,
                            (pSpecRegGroup+denoise)->len );

    rc |= ov5642_set_sharpness(sharpness);

    if (!rc) {
        ov5642_ctrl->scene_value = scene;

        if( (CAMERA_BESTSHOT_OFF == scene) && (SENSOR_PREVIEW_MODE == ov5642_ctrl->sensor_mode) )
            ov5642_night_mode_func.convert_night_mode(scene, 2);
    }
    return rc;
}

static int32_t ov5642_sensor_output_disable(void)
{
    int32_t rc;

    struct ov5642_reg_t ov5642_output_disable [] = { {0x3017, 0x00},
                                                     {0x3018, 0x00} };

    rc = ov5642_i2c_write( (struct ov5642_reg_t *)ov5642_output_disable,
                           ARRAY_SIZE(ov5642_output_disable) );


    return rc;
}

static int32_t ov5642_sensor_output_enable(void)
{
    int32_t rc;

    struct ov5642_reg_t ov5642_output_enable [] = { {0x3017, 0x7F},
                                                    {0x3018, 0xFC} };

    rc = ov5642_i2c_write( (struct ov5642_reg_t *)ov5642_output_enable,
                           ARRAY_SIZE(ov5642_output_enable) );

    return rc;
}

static void ov5642_sensor_power_up(const struct msm_camera_sensor_info *dev)
{
    if( 0 != gpio_get_value(dev->sensor_pwd) )
    {
        gpio_set_value(dev->sensor_pwd, 0);
        msleep(100);
    }
    return;
}

static void ov5642_sensor_power_down(const struct msm_camera_sensor_info *dev)
{
    if( 1 != gpio_get_value(dev->sensor_pwd) )
	{
        gpio_set_value(dev->sensor_pwd, 1);
        msleep(1);
    }
    return;
}

static void ov5642_gpio_config_on(struct msm_camera_sensor_info *dev)
{
    gpio_set_value( dev->sensor_reset, 0 );

    gpio_set_value( dev->sensor_pwd, 1 );

    return;
}

static void ov5642_sensor_power_on_sequence(struct msm_camera_sensor_info *data)
{


#if 0
    ov5642_camclk_po_hz = 24000000;
    msm_camio_clk_rate_set( ov5642_camclk_po_hz );
    msleep(5);
#endif

    ov5642_reset(data);

    msleep(100);

    return;
}

static int32_t ov5642_sensor_detect( void )
{
    int32_t rc = 0;
    struct ov5642_reg_t ov5642_product_id_reg[] =
    {
        { OV5642_PIDH_ADDR, 0 },
        { OV5642_PIDL_ADDR, 0 },
    };

    rc = ov5642_i2c_read( (struct ov5642_reg_t *)ov5642_product_id_reg,
                          (sizeof(ov5642_product_id_reg)/sizeof(ov5642_product_id_reg[0])) );

    if( !rc )
        if( ov5642_product_id_reg[0].data != OV5642_PIDH_DEFAULT_VALUE ||
            ov5642_product_id_reg[1].data != OV5642_PIDL_DEFAULT_VALUE )
        {
            CSDBG( "ov5642_sensor_detect: id 0x%X 0x%X not matched\n",
                  ov5642_product_id_reg[0].data, ov5642_product_id_reg[1].data );
            rc = -1;
        }

    return rc;
}

static int ov5642_sensor_init_probe(const struct msm_camera_sensor_info *data)
{
    int rc;

    CDBG("ov5642_sensor_init_probe()+\n");

    ov5642_sensor_power_up(data);
    rc = ov5642_sensor_output_enable();
    if( rc < 0 )
        goto exit_init_probe;

    rc = ov5642_sensor_detect();
    if( rc < 0 )
    {
        CSDBG("ov5642_sensor_init: sensor detect failed\n");
        goto exit_init_probe;
    }

exit_init_probe:
    CDBG("ov5642_sensor_init_probe()-, rc=%d\n", rc);

    return rc;
}

#ifdef AUSTIN_CAMERA
int ov5642_sensor_preinit(void *info)
{
    int rc = 0;
    struct msm_camera_sensor_info *data = info;

    CDBG("ov5642_sensor_preinit()+\n");

    if( NULL == data )
    {
        rc = -1;
        CSDBG("ov5642_sensor_preinit: invalid arg\n");
        goto exit_preinit;
    }

    ov5642_ctrl = kzalloc(sizeof(struct ov5642_ctrl), GFP_KERNEL);
    if (!ov5642_ctrl) {
        CSDBG("ov5642_sensor_preinit failed!\n");
        rc = -ENOMEM;
        goto exit_preinit;
    }
    ov5642_ctrl->sensordata = data;

    if( system_rev < EVT2_Band125 )
    {
        ov5642_ctrl->reg_map = &ov5642_1C_regs;
        ov5642_ctrl->ae_func = &ov5642_1C_ae_func;
        CDBG("ov5642_sensor_preinit: 1C sensor\n");
    } else {
        ov5642_ctrl->reg_map = &ov5642_1D_regs;
        ov5642_ctrl->ae_func = &ov5642_1D_ae_func;
        CDBG("ov5642_sensor_preinit: 1D sensor\n");
    }

    ov5642_gpio_config_on(data);

    ov5642_sensor_power_on_sequence(data);

    rc = ov5642_reg_init();
    if( rc < 0 ) {
        CSDBG("ov5642_sensor_preinit(): reg init failed\n");
        goto reg_init_fail;
    }

    if( ov5642_client )
    {
        rc = ov5642_sensor_output_disable();
        if( rc < 0 )
            CSDBG("ov5642_sensor_init: output disabled failure\n");
    }

reg_init_fail:

exit_preinit:

    if( ov5642_ctrl )
    {
        kfree(ov5642_ctrl);
        ov5642_ctrl = NULL;
    }

    CDBG("ov5642_sensor_preinit()-, rc=%d\n", rc);
    return rc;
}
#endif

int ov5642_sensor_init(const struct msm_camera_sensor_info *data)
{
    int rc = 0;

    ov5642_ctrl = kzalloc(sizeof(struct ov5642_ctrl), GFP_KERNEL);
    if (!ov5642_ctrl) {
        CSDBG("ov5642_init failed!\n");
        rc = -ENOMEM;
        goto init_done;
    }

    if (data)
        ov5642_ctrl->sensordata = data;
    else
        goto init_fail;

    if( system_rev < EVT2_Band125 )
    {
        ov5642_ctrl->reg_map = &ov5642_1C_regs;
        ov5642_ctrl->ae_func = &ov5642_1C_ae_func;
        CSDBG("ov5642_sensor_init: 1C sensor\n");
    } else {
        ov5642_ctrl->reg_map = &ov5642_1D_regs;
        ov5642_ctrl->ae_func = &ov5642_1D_ae_func;
        CSDBG("ov5642_sensor_init: 1D sensor\n");
    }
    ov5642_ctrl->banding_type = AUTO_BANDING;
    ov5642_ctrl->wb_type = AUTO_WB;
    ov5642_ctrl->scene_value = -1;
    ov5642_ctrl->sensor_mode = -1;
    ov5642_ctrl->contrast = -1;
    ov5642_ctrl->brightness = -1;

#if 0
    msm_camio_clk_rate_set(24000000);
    msleep(5);
#endif	
    msm_camio_camif_pad_reg_reset();

    rc = ov5642_sensor_init_probe(data);
    if (rc < 0) {
        CSDBG("ov5642_sensor_init failed!\n");
        goto init_fail;
    }


init_done:
    CDBG("ov5642_sensor_init()-, rc=%d\n", rc);

    return rc;

init_fail:
    kfree(ov5642_ctrl);
    ov5642_ctrl = NULL;
    CSDBG("ov5642_sensor_init(): init failed\n");

    return rc;
}

static int ov5642_init_client(struct i2c_client *client)
{
	init_waitqueue_head(&ov5642_wait_queue);
	return 0;
}

static long ov5642_set_jpeg_image_quality(int8_t value)
{
    long rc = 0;
    enum ov5642_jpeg_quality_t index;
    struct ov5642_reg_group_t const *pSpecRegGroup;

    CDBG("ov5642_set_jpeg_image_quality, value=%d\n", value);

    if( value <= 60 )
        index = QUALITY_60;
    else if( value <= 70 )
        index = QUALITY_70;
    else
        index = QUALITY_80;

    CDBG("ov5642_set_jpeg_image_quality, quality index=%d\n", index);

    pSpecRegGroup = ov5642_special_regs.jpeg_quality_group;
    rc = ov5642_i2c_write( (pSpecRegGroup+index)->start,
                           (pSpecRegGroup+index)->len );

    CDBG("ov5642_set_jpeg_image_quality, rc=%ld\n", rc);

    return rc;
}

int ov5642_sensor_config(void __user *argp)
{
	struct sensor_cfg_data cfg_data;
	int   rc = 0;

	if (copy_from_user(
				&cfg_data,
				(void *)argp,
				sizeof(struct sensor_cfg_data)))
		return -EFAULT;

    switch (cfg_data.cfgtype) {
        case CFG_SET_MODE:
            rc = ov5642_set_sensor_mode( cfg_data.mode , cfg_data.rs - SENSOR_5M_SIZE );
        break;
        case CFG_SET_WB:
            rc = ov5642_set_wb( cfg_data.cfg.wb );
        break;

        case CFG_SET_BRIGHTNESS:
            rc = ov5642_set_brightness( cfg_data.cfg.brightness );
        break;

        case CFG_SET_CONTRAST:
            rc = ov5642_set_contrast( cfg_data.cfg.contrast );
        break;

        case CFG_SET_ANTIBANDING:
            rc = ov5642_set_antibanding( cfg_data.cfg.antibanding );
        break;

        case CFG_PWR_DOWN:
            rc = ov5642_sensor_output_disable();
            ov5642_sensor_power_down(ov5642_ctrl->sensordata);
        break;

        case CFG_PWR_UP:
            ov5642_sensor_power_up(ov5642_ctrl->sensordata);
            rc = ov5642_sensor_output_enable();
        break;

        case CFG_SET_SCENE:
            rc = ov5642_set_scene( cfg_data.cfg.scene );
        break;
#ifdef USE_AF
        case CFG_SET_AUTO_FOCUS:
            rc = ov5642_af_func.detect_auto_focus();
            if( rc )
                break;

            ov5642_night_mode_func.finish_night_mode(0);
            rc = ov5642_af_func.set_auto_focus( &cfg_data.cfg.af_status );
            if( !rc )
            {
                if ( copy_to_user( (void *)argp,
                                    &cfg_data,
                                    sizeof(struct sensor_cfg_data)) )
                {
                    CSDBG("ov5642_ioctl: copy to user failed\n");
                    rc = -EFAULT;
                }
            }
        break;
        case CFG_SET_DEFAULT_FOCUS:
            ov5642_night_mode_func.convert_night_mode(ov5642_ctrl->scene_value, 2);
            rc = ov5642_af_func.set_default_focus();
        break;
#endif
        case CFG_SET_IMAGE_QUALITY:
            rc = ov5642_set_jpeg_image_quality( cfg_data.cfg.quality );
        break;

        case CFG_DUMP_SINGLE_REGISTER:
        case CFG_DUMP_REGISTERS_TO_FILE:
        case CFG_SET_SINGLE_REGISTER:
        case CFG_SET_REGISTERS_FROM_FILE:
            rc = ov5642_engineer_mode( cfg_data.cfgtype, (void *)(&cfg_data.cfg) );
            if( !rc && CFG_DUMP_SINGLE_REGISTER == cfg_data.cfgtype )
            {
                if ( copy_to_user( (void *)argp,
                                    &cfg_data,
                                    sizeof(struct sensor_cfg_data)) )
                {
                    CSDBG("ov5642_ioctl: copy to user failed\n");
                    rc = -EFAULT;
                }
            }
        break;

        case CFG_PREPARE_SNAPSHOT:
            rc = ov5642_ctrl->ae_func->prepare_snapshot();
        break;

        default:
            rc = -EFAULT;
        break;
    }

    if( rc ) CSDBG("ov5642_ioctl(), type=%d, mode=%d, rc=%d\n", cfg_data.cfgtype, cfg_data.mode, rc);

    return rc;
}

int ov5642_sensor_release(void)
{
    int rc = 0;

    CSDBG("ov5642_release()\n");

    ov5642_awb_func.finish_awb(0);
    ov5642_night_mode_func.finish_night_mode(0);
#ifdef USE_AF
    ov5642_af_func.deinit_auto_focus();
#endif

    if(ov5642_ctrl)
    {
        kfree(ov5642_ctrl);
        ov5642_ctrl = NULL;
    }

    return rc;
}

static int ov5642_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;

    CDBG("ov5642_i2c_probe\n");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		rc = -ENOTSUPP;
		goto probe_failure;
	}

	ov5642_sensorw =
		kzalloc(sizeof(struct ov5642_work), GFP_KERNEL);

	if (!ov5642_sensorw) {
		rc = -ENOMEM;
		goto probe_failure;
	}

	i2c_set_clientdata(client, ov5642_sensorw);
	ov5642_init_client(client);
	ov5642_client = client;

	CDBG("ov5642_i2c_probe successed!\n");

	return 0;

probe_failure:
	kfree(ov5642_sensorw);
	ov5642_sensorw = NULL;
	CDBG("ov5642_i2c_probe failed!\n");
	return rc;
}

static const struct i2c_device_id ov5642_i2c_id[] = {
	{ "ov5642", 0},
	{ },
};

static struct i2c_driver ov5642_i2c_driver = {
	.id_table = ov5642_i2c_id,
	.probe  = ov5642_i2c_probe,
	.remove = __exit_p(ov5642_i2c_remove),
	.driver = {
		.name = "ov5642",
	},
};

static int ov5642_sensor_probe(const struct msm_camera_sensor_info *info,
				struct msm_sensor_ctrl *s)
{
	int rc = 0;

    printk("BootLog, +%s\n", __func__);

	rc = i2c_add_driver(&ov5642_i2c_driver);
	if (rc < 0 || ov5642_client == NULL) {
		rc = -ENOTSUPP;
		goto probe_done;
	}
#if 0
    msm_camio_clk_rate_set(24000000);
    msleep(5);

    rc = ov5642_sensor_init_probe(info);
    if (rc < 0)
        goto probe_done;

    rc = ov5642_sensor_output_disable();
    ov5642_sensor_power_down(info);
    if (rc < 0)
        goto probe_done;
#endif
	s->s_init = ov5642_sensor_init;
	s->s_release = ov5642_sensor_release;
	s->s_config  = ov5642_sensor_config;

probe_done:

	CDBG("%s %s:%d\n", __FILE__, __func__, __LINE__);
    printk("BootLog, -%s, ret=%d\n", __func__, rc);
	return rc;
}

static int __ov5642_probe(struct platform_device *pdev)
{
	return msm_camera_drv_start(pdev, ov5642_sensor_probe);
}

static struct platform_driver msm_camera_driver = {
	.probe = __ov5642_probe,
	.driver = {
		.name = "msm_camera_ov5642",
		.owner = THIS_MODULE,
	},
};

static int __init ov5642_init(void)
{
	return platform_driver_register(&msm_camera_driver);
}

module_init(ov5642_init);
