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


#include "ov7690.h"
#include <linux/slab.h>

struct ov7690_work {
	struct work_struct work;
};

static struct  ov7690_work *ov7690_sensorw;
static struct  i2c_client *ov7690_client;

struct ov7690_ctrl {
    const struct msm_camera_sensor_info *sensordata;
    struct ov7690_basic_reg_arrays *reg_map;
};


static struct ov7690_ctrl *ov7690_ctrl;

static DECLARE_WAIT_QUEUE_HEAD(ov7690_wait_queue);
DECLARE_MUTEX(ov7690_sem);

extern struct ov7690_basic_reg_arrays ov7690_regs;
extern struct ov7690_special_reg_groups ov7690_special_regs;

static inline int isValidClient( void *client )
{
    return ((client != NULL)? 0: -1);
}

#define MAX_RETRY_COUNT 5
static int32_t ov7690_i2c_read( struct ov7690_reg_t *regs, uint32_t size)
{
    uint32_t i;
    int retryCnt;
    int rc;
    unsigned char buf[1];
    struct i2c_msg msg[] =
    {
        [0] = {
            .addr   = OV7690_SLAVE_ADDR,
            .flags  = 0,
            .buf    = (void *)buf,
            .len    = 1
        },
        [1] = {
            .addr   = OV7690_SLAVE_ADDR,
            .flags  = I2C_M_RD,
            .len    = 1
        }
    };

    if( isValidClient(ov7690_client) )
    {
        CSDBG("ov7690_i2c_read: invalid client\n");
        return -EFAULT;
    }

    for( i=0; i<size; i++ )
    {
        buf[0] = regs->addr;
        msg[1].buf = (void *)&regs->data;

#ifndef MAX_RETRY_COUNT
        if ( ARRAY_SIZE(msg) != i2c_transfer( ov7690_client->adapter, msg, 2) ) {
            CSDBG("ov7690_i2c_read: read %Xh failed\n", buf[0] );
            return -EIO;
        }
#else
        for( retryCnt=0; retryCnt<MAX_RETRY_COUNT; retryCnt++)
        {
            rc = i2c_transfer( ov7690_client->adapter, msg, 2);
            if( ARRAY_SIZE(msg) == rc )
            {
                if( retryCnt > 0 )
                    CSDBG("ov7690_i2c_read: read %Xh=0x%X successfull at retryCnt=%d\n",
                           buf[0], regs->data, retryCnt);
                break;
            }
        }

        if( (rc != ARRAY_SIZE(msg)) && (retryCnt == MAX_RETRY_COUNT) )
        {
            CSDBG( "ov7690_i2c_read: read %Xh failed\n", buf[0] );
            return -EIO;
        }
#endif
        regs++;
    }


    return 0;
}

static int32_t ov7690_i2c_write( struct ov7690_reg_t *regs, uint32_t size)
{
    uint32_t i;
    int retryCnt;
    int rc;
    unsigned char buf[2];
    struct i2c_msg msg[] =
    {
        {
    	.addr = OV7690_SLAVE_ADDR,
    	.flags = 0,
    	.len = 2,
    	.buf = buf,
        }
    };

    if( isValidClient(ov7690_client) )
    {
        CSDBG("ov7690_i2c_write: invalid client\n");
        return -EFAULT;
    }

    for( i=0; i<size; i++ )
    {
        if( OV7690_BIT_OPERATION_TAG == regs->addr )
        {
            struct ov7690_reg_t writableReg;
            char mask, value;
            mask = regs->data;
            i++;
            regs++;
            value = regs->data;
            writableReg.addr = regs->addr;
            if( !ov7690_i2c_read( &writableReg, 1 ) )
            {
                writableReg.data &= (~mask);
                writableReg.data |= value;
            }else {
                CSDBG("ov7690_i2c_write: read %Xh before write failed\n", writableReg.addr);
                return -EIO;
            }
            buf[0] = writableReg.addr;
            buf[1] = writableReg.data;
        }else {
            buf[0] = regs->addr;
            buf[1] = regs->data;
        }

#ifndef MAX_RETRY_COUNT
        if ( ARRAY_SIZE(msg) != i2c_transfer( ov7690_client->adapter, msg, 1) ) {
            CSDBG("ov7690_i2c_write: write %Xh=0x%X failed\n", buf[0], buf[1] );
            return -EIO;
        }
#else
        for( retryCnt=0; retryCnt<MAX_RETRY_COUNT; retryCnt++)
        {
            rc = i2c_transfer( ov7690_client->adapter, msg, 1);
            if( ARRAY_SIZE(msg) == rc )
            {
                if( retryCnt > 0 )
                    CSDBG("ov7690_i2c_write: write %Xh=0x%X successfull at retryCnt=%d\n",
                               buf[0], buf[1], retryCnt);
                break;
            }
        }

        if( (rc != ARRAY_SIZE(msg)) && (retryCnt == MAX_RETRY_COUNT) )
        {
            CSDBG( "ov7690_i2c_write: write %Xh =0x%X failed\n", buf[0], buf[1] );
            return -EIO;
        }
#endif
        if( 0x12 == regs->addr && (regs->data & 0x80) )
        {
            CDBG("ov7690_i2c_write: sleep 3 ms after software reset\n");
            msleep(SWRESET_STABLE_TIME_MS);
        }

        regs++;
    }

    return 0;
}

static int32_t ov7690_i2c_burst_read( unsigned short addr, unsigned char *data, uint32_t size)
{
    struct i2c_msg msg[] =
    {
        [0] = {
            .addr   = OV7690_SLAVE_ADDR,
            .flags  = 0,
            .buf    = (void *)&addr,
            .len    = 1
        },
        [1] = {
            .addr   = OV7690_SLAVE_ADDR,
            .flags  = I2C_M_RD,
            .buf    = (void *)data,
            .len    = size
        }
    };

    if( isValidClient(ov7690_client) )
    {
        CSDBG("ov7690_i2c_burst_read: invalid client\n");
        return -EFAULT;
    }

    if ( i2c_transfer(ov7690_client->adapter, msg, ARRAY_SIZE(msg)) != ARRAY_SIZE(msg) ) {
        CSDBG( "ov7690_i2c_burst_read: read %Xh %d bytes failed\n", addr, size );
        return -EIO;
    }

    return 0;
}

static long ov7690_reg_init(void)
{
    int32_t rc;

    CDBG("ov7690_reg_init()+\n");

    rc = ov7690_i2c_write( (struct ov7690_reg_t *)ov7690_ctrl->reg_map->default_array,
                           ov7690_ctrl->reg_map->default_length );

    if( rc )
        return rc;

    rc = ov7690_i2c_write( (struct ov7690_reg_t *)ov7690_ctrl->reg_map->IQ_array,
                           ov7690_ctrl->reg_map->IQ_length );

    CDBG("ov7690_reg_init()-\n");

    return rc;
}


static long ov7690_engineer_mode( int type, void *pCfg )
{
    long rc = 0;

    switch(type)
    {
        case CFG_DUMP_SINGLE_REGISTER:
        {
            struct ov7690_reg_t reg;
            struct os_engineer_reg_cfg *pOrigCfg;

            pOrigCfg = (struct os_engineer_reg_cfg *)pCfg;
            reg.addr = (unsigned char)pOrigCfg->addr;

            rc = ov7690_i2c_read( &reg, 1 );
            if( !rc )
            {
                CSDBG("ov7690_engineer_mode: read %Xh=0x%X\n", reg.addr, reg.data);
                pOrigCfg->data = (uint16_t)reg.data;
            }
            break;
        }
        case CFG_SET_SINGLE_REGISTER:
        {
            struct ov7690_reg_t reg;
            struct os_engineer_reg_cfg *pOrigCfg;

            pOrigCfg = (struct os_engineer_reg_cfg *)pCfg;
            reg.addr = (unsigned char)pOrigCfg->addr;
            reg.data = (unsigned char)pOrigCfg->data;

            rc = ov7690_i2c_write( &reg, 1 );
            break;
        }
        case CFG_DUMP_REGISTERS_TO_FILE:
        {
            struct os_engineer_file_cfg *pOrigCfg;
            char buf[256];

            pOrigCfg = (struct os_engineer_file_cfg *)pCfg;
            if( pOrigCfg->len > 256 || pOrigCfg->addr > 256 || NULL == pOrigCfg->buf )
            {
                rc = -EFAULT;
                CSDBG("ov7690_engineer_mode: invalid args\n");
                break;
            }

            memset(buf, 0, sizeof(buf));
            if( (rc = ov7690_i2c_burst_read( pOrigCfg->addr , buf, pOrigCfg->len )) )
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
                CSDBG("ov7690_engineer_mode: invalid args\n");
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

            for(i=0; i < pOrigCfg->len; i+=2)
            {
                struct ov7690_reg_t reg;

                reg.addr = buf[i];
                reg.data = buf[i+1];

                if( OV7690_BIT_OPERATION_TAG == reg.addr )
                {
                    char mask = buf[i+1];

                    if( i+2 > pOrigCfg->len )
                    {
                        CSDBG("ov7690_engineer_mode: bit operation is broken\n");
                        rc = -1;
                        break;
                    }

                    i += 2;
                    reg.addr = buf[i];
                    rc = ov7690_i2c_read( &reg, 1);
                    if( rc )
                        break;

                    CSDBG("ov7690_engineer_mode: read  %Xh=%X\n", reg.addr, reg.data);

                    reg.data &= ~mask;
                    reg.data |= buf[i+1];
                }

                CSDBG("ov7690_engineer_mode: write %Xh=%X\n", reg.addr, reg.data);
                rc = ov7690_i2c_write( &reg, 1 );
                if( rc )
                    break;
            }

            break;
        }
        default:
            CSDBG("ov7690_engineer_mode: not supported type\n");
            rc = -EFAULT;
            break;
    }

    return rc;
}

static long ov7690_set_sensor_mode(int mode,
                                   int res)
{
    long rc = 0;

    CDBG( "ov7690_set_sensor_mode+, mode=%d, res=%d\n", mode, res);

    switch( mode )
    {
        case SENSOR_PREVIEW_MODE:
            rc = ov7690_i2c_write( (struct ov7690_reg_t *)ov7690_ctrl->reg_map->preview_array,
                                   ov7690_ctrl->reg_map->preview_length );

            break;
        case SENSOR_SNAPSHOT_MODE:
#if 0
            rc = ov7690_i2c_write( (struct ov7690_reg_t *)ov7690_ctrl->reg_map->snapshot_array,
                                   ov7690_ctrl->reg_map->snapshot_length );
#endif
            break;
        default:
            CSDBG( "ov7690_set_sensor_mode: unknown mode %d\n", mode );
            rc = -1;
            break;
    }

    CDBG( "ov7690_set_sensor_mode-\n");

    return rc;
}

static long ov7690_set_wb( int8_t whiteBalance )
{
    long rc = 0;
    enum ov7690_white_balance_t index;
    struct ov7690_reg_group_t const *pSpecRegGroup;

    CDBG( "ov7690_set_wb: wb=%d\n", whiteBalance );

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
            CSDBG("ov7690_set_wb: unsupported wb %d\n", whiteBalance);
            rc = -EFAULT;
        }
        break;
    }

    if( rc )
        return rc;

    pSpecRegGroup = ov7690_special_regs.white_balance_group;
    rc = ov7690_i2c_write( (pSpecRegGroup+index)->start,
                           (pSpecRegGroup+index)->len );

    return rc;
}

static long ov7690_set_brightness( int8_t brightness )
{
    long rc = 0;
    enum ov7690_brightness_t index;
    struct ov7690_reg_group_t const *pSpecRegGroup;

    CDBG( "ov7690_set_brightness: brightness=%d\n", brightness );

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
            CSDBG("ov7690_set_brightness: unsupported brightness %d\n", brightness);
            rc = -EFAULT;
        }
        break;
    }

    if( rc )
        return rc;

    pSpecRegGroup = ov7690_special_regs.brightness_group;
    rc = ov7690_i2c_write( (pSpecRegGroup+index)->start,
                           (pSpecRegGroup+index)->len );

    return rc;
}

static long ov7690_set_contrast( int8_t contrast )
{
    long rc = 0;
    enum ov7690_contrast_t index;
    struct ov7690_reg_group_t const *pSpecRegGroup;

    CDBG( "ov7690_set_contrast: contrast=%d\n", contrast );

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
            CSDBG("ov7690_set_contrast: unsupported contrast %d\n", contrast);
            rc = -EFAULT;
        }
        break;
    }

    if( rc )
        return rc;

    pSpecRegGroup = ov7690_special_regs.contrast_group;
    rc = ov7690_i2c_write( (pSpecRegGroup+index)->start,
                           (pSpecRegGroup+index)->len );

    return rc;
}

static long ov7690_set_antibanding( int8_t banding )
{
    long rc = 0;
    enum ov7690_anti_banding_t index;
    struct ov7690_reg_group_t const *pSpecRegGroup;

    CDBG( "ov7690_set_antibanding: banding=%d\n", banding );

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
            index = MANUAL_60HZ_BANDING;
        }
        break;
        default: {
            CSDBG("ov7690_set_antibanding: unsupported banding %d\n", banding);
            rc = -EFAULT;
        }
        break;
    }

    if( rc )
        return rc;

    pSpecRegGroup = ov7690_special_regs.anti_banding_group;
    rc = ov7690_i2c_write( (pSpecRegGroup+index)->start,
                           (pSpecRegGroup+index)->len );

    return rc;
}

#if 0
static long ov7690_set_standby( bool standby )
{
    int pwd = ov7690_ctrl->sensordata->sensor_pwd;

    if( standby )
    {
        gpio_set_value(pwd, 1);
        msm_camio_clk_disable(CAMIO_VFE_CLK);
    }else {
        msm_camio_clk_enable(CAMIO_VFE_CLK);
        gpio_set_value(pwd, 0);
    }

    return 0;
}
#endif

static long ov7690_set_scene( int8_t scene )
{
    long rc = 0;
    enum ov7690_frame_rate_t frameRate;
    enum ov7690_ae_weighting_t aeWeighting;
    enum ov7690_color_matrix_t colorMatrix;
    enum ov7690_ae_target_t aeTarget;
    struct ov7690_reg_group_t const *pSpecRegGroup;

    CDBG( "ov7690_set_scene: scene=%d\n", scene );

    switch(scene) {
        case CAMERA_BESTSHOT_OFF: {
            frameRate = ONE_OF_TWO_FPS;
            aeWeighting = CENTER_QUARTER_AE;
            colorMatrix = GENERAL_CMX;
            aeTarget = GENERAL_AE_TARGET;
        }
        break;
        case CAMERA_BESTSHOT_NIGHT: {
            frameRate = ONE_OF_EIGHT_FPS;
            aeWeighting = WHOLE_IMAGE_AE;
            colorMatrix = GENERAL_CMX;
            aeTarget = NIGHT_AE_TARGET;
        }
        break;
        case CAMERA_BESTSHOT_PORTRAIT: {
            frameRate = ONE_OF_TWO_FPS;
            aeWeighting = CENTER_QUARTER_AE;
            colorMatrix = PERSON_CMX;
            aeTarget = GENERAL_AE_TARGET;
        }
        break;
        case CAMERA_BESTSHOT_LANDSCAPE: {
            frameRate = ONE_OF_TWO_FPS;
            aeWeighting = WHOLE_IMAGE_AE;
            colorMatrix = LAND_CMX;
            aeTarget = GENERAL_AE_TARGET;
        }
        break;
        case CAMERA_BESTSHOT_SPORTS: {
            frameRate = ONE_OF_ONE_FPS;
            aeWeighting = WHOLE_IMAGE_AE;
            colorMatrix = GENERAL_CMX;
            aeTarget = GENERAL_AE_TARGET;
        }
        break;
        case CAMERA_BESTSHOT_SNOW: {
            frameRate = ONE_OF_ONE_FPS;
            aeWeighting = WHOLE_IMAGE_AE;
            colorMatrix = GENERAL_CMX;
            aeTarget = GENERAL_AE_TARGET;
        }
        break;
        default: {
            CSDBG("ov7690_set_scene: unsupported scene %d\n", scene);
            rc = -EFAULT;
        }
        break;
    }

    if( rc )
        return rc;

    pSpecRegGroup = ov7690_special_regs.frame_rate_group;
    rc = ov7690_i2c_write( (pSpecRegGroup+frameRate)->start,
                           (pSpecRegGroup+frameRate)->len );
    if( rc )
        return rc;

    pSpecRegGroup = ov7690_special_regs.color_matrix_group;
    rc = ov7690_i2c_write( (pSpecRegGroup+colorMatrix)->start,
                           (pSpecRegGroup+colorMatrix)->len );
    if( rc )
        return rc;

    pSpecRegGroup = ov7690_special_regs.ae_weighting_group;
    rc = ov7690_i2c_write( (pSpecRegGroup+aeWeighting)->start,
                           (pSpecRegGroup+aeWeighting)->len );

    return rc;
}


int32_t ov7690_sensor_output_enable(void)
{
    int32_t rc;

    struct ov7690_reg_t ov7690_output_enable[] = { {0x0C, 0x16} };

    rc = ov7690_i2c_write( (struct ov7690_reg_t *)ov7690_output_enable, 1 );

    return rc;
}

int32_t ov7690_sensor_output_disable(void)
{
    int32_t rc;

    struct ov7690_reg_t ov7690_output_disable [] = { {0x0C, 0x10} };

    rc = ov7690_i2c_write( (struct ov7690_reg_t *)ov7690_output_disable, 1 );

    return rc;
}

static void ov7690_sensor_power_up(const struct msm_camera_sensor_info *dev)
{
    if( 0 != gpio_get_value(dev->sensor_pwd) )
	{
        gpio_set_value(dev->sensor_pwd, 0);
        msleep(3);
    }
    return;
}

static void ov7690_sensor_power_down(const struct msm_camera_sensor_info *dev)
{
    if( 1 != gpio_get_value(dev->sensor_pwd) )
	{
        gpio_set_value(dev->sensor_pwd, 1);
        msleep(1);
    }
    return;
}

static void ov7690_gpio_config_on(struct msm_camera_sensor_info *dev)
{
#ifdef CONFIG_MACH_EVT0
        gpio_set_value( 2, 0 );
#else
    gpio_set_value(dev->sensor_pwd, 0);
#endif

    return;
}

static void ov7690_sensor_power_on_sequence(struct msm_camera_sensor_info *data)
{
#if 0
    mclk = 24000000;
    msm_camio_clk_rate_set( mclk );
    msleep(5);
#endif

    msleep(3);

    return;
}

static int32_t ov7690_sensor_detect( void )
{
    int32_t rc = 0;
    struct ov7690_reg_t ov7690_product_id_reg[] =
    {
        { OV7690_PIDH_ADDR, 0 },
        { OV7690_PIDL_ADDR, 0 },
    };

    rc = ov7690_i2c_read( (struct ov7690_reg_t *)ov7690_product_id_reg,
                          (sizeof(ov7690_product_id_reg)/sizeof(ov7690_product_id_reg[0])) );

    if( !rc )
        if( ov7690_product_id_reg[0].data != OV7690_PIDH_DEFAULT_VALUE ||
            ov7690_product_id_reg[1].data != OV7690_PIDL_DEFAULT_VALUE )
        {
            CSDBG( "ov7690_sensor_detect: id 0x%X 0x%X not matched\n",
                  ov7690_product_id_reg[0].data, ov7690_product_id_reg[1].data );
            rc = -1;
        }

    return rc;
}

static int ov7690_sensor_init_probe(const struct msm_camera_sensor_info *data)
{
    int rc;

    CDBG("ov7690_sensor_init_probe()+\n");

    ov7690_sensor_power_up(data);
    rc = ov7690_sensor_output_enable();
    if( rc < 0 )
        goto exit_init_probe;

    rc = ov7690_sensor_detect();
    if( rc < 0 )
    {
        CSDBG("ov7690_sensor_init: sensor detect failed\n");
        goto exit_init_probe;
    }

exit_init_probe:
    CDBG("ov7690_sensor_init_probe()-, rc=%d\n", rc);

    return rc;
}

#ifdef AUSTIN_CAMERA
int ov7690_sensor_preinit(void *info)
{
    int rc = 0;
    struct msm_camera_sensor_info *data = info;

    CDBG("ov7690_sensor_preinit()+\n");

    if( NULL == data )
    {
        rc = -1;
        CSDBG("ov7690_sensor_preinit: invalid arg\n");
        goto exit_preinit;
    }

    ov7690_ctrl = kzalloc(sizeof(struct ov7690_ctrl), GFP_KERNEL);
    if (!ov7690_ctrl) {
        CDBG("ov7690_sensor_preinit failed!\n");
        rc = -ENOMEM;
        goto exit_preinit;
    }
    ov7690_ctrl->sensordata = data;
    ov7690_ctrl->reg_map = &ov7690_regs;

    ov7690_gpio_config_on(data);

    ov7690_sensor_power_on_sequence(data);

    rc = ov7690_reg_init();
    if( rc < 0 ) {
        CSDBG("ov7690_sensor_preinit(): reg init failed\n");
        goto reg_init_fail;
    }

    if( ov7690_client )
    {
        rc = ov7690_sensor_output_disable();
        if( rc < 0 )
            CSDBG("ov7690_sensor_init: output disabled failure\n");
    }

reg_init_fail:

exit_preinit:

    if( ov7690_ctrl )
    {
        kfree(ov7690_ctrl);
        ov7690_ctrl = NULL;
    }

    CDBG("ov7690_sensor_preinit()-, rc=%d\n", rc);
    return rc;
}
#endif

int ov7690_sensor_init(const struct msm_camera_sensor_info *data)
{
    int rc = 0;

    ov7690_ctrl = kzalloc(sizeof(struct ov7690_ctrl), GFP_KERNEL);
    if (!ov7690_ctrl) {
        CDBG("ov7690_init failed!\n");
        rc = -ENOMEM;
        goto init_done;
    }

    if (data)
        ov7690_ctrl->sensordata = data;
    else
        goto init_fail;

    ov7690_ctrl->reg_map = &ov7690_regs;

#if 0
    msm_camio_clk_rate_set(24000000);
    msleep(5);
#endif
    msm_camio_camif_pad_reg_reset();

    rc = ov7690_sensor_init_probe(data);
    if (rc < 0) {
        CDBG("ov7690_sensor_init failed!\n");
        goto init_fail;
    }


init_done:
    CDBG("ov7690_sensor_init()-, rc=%d\n", rc);

    return rc;

init_fail:
    kfree(ov7690_ctrl);
    ov7690_ctrl = NULL;
    CSDBG("ov7690_sensor_init(): init failed\n");

    return rc;
}

static int ov7690_init_client(struct i2c_client *client)
{
	init_waitqueue_head(&ov7690_wait_queue);
	return 0;
}

int ov7690_sensor_config(void __user *argp)
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
            rc = ov7690_set_sensor_mode( cfg_data.mode , cfg_data.rs - SENSOR_VGA_SIZE );
        break;

        case CFG_SET_WB:
            rc = ov7690_set_wb( cfg_data.cfg.wb );
        break;

        case CFG_SET_BRIGHTNESS:
            rc = ov7690_set_brightness( cfg_data.cfg.brightness );
        break;

        case CFG_SET_CONTRAST:
            rc = ov7690_set_contrast( cfg_data.cfg.contrast );
        break;

        case CFG_SET_ANTIBANDING:
            rc = ov7690_set_antibanding( cfg_data.cfg.antibanding );
        break;

        case CFG_PWR_DOWN:
            rc = ov7690_sensor_output_disable();
            ov7690_sensor_power_down(ov7690_ctrl->sensordata);
        break;

        case CFG_PWR_UP:
            ov7690_sensor_power_up(ov7690_ctrl->sensordata);
            rc = ov7690_sensor_output_enable();
        break;

        case CFG_SET_SCENE:
            rc = ov7690_set_scene( cfg_data.cfg.scene );
        break;

        case CFG_DUMP_SINGLE_REGISTER:
        case CFG_DUMP_REGISTERS_TO_FILE:
        case CFG_SET_SINGLE_REGISTER:
        case CFG_SET_REGISTERS_FROM_FILE:
            rc = ov7690_engineer_mode( cfg_data.cfgtype, (void *)(&cfg_data.cfg) );
            if( !rc && CFG_DUMP_SINGLE_REGISTER == cfg_data.cfgtype )
            {
                if ( copy_to_user( (void *)argp,
                                    &cfg_data,
                                    sizeof(struct sensor_cfg_data)) )
                {
                    CSDBG("ov7690_ioctl: copy to user failed\n");
                    rc = -EFAULT;
                }
            }
        break;

        default:
            rc = -EFAULT;
        break;
    }

    if( rc ) CSDBG("ov7690_ioctl(), type=%d, mode=%d, rc=%d\n", cfg_data.cfgtype, cfg_data.mode, rc);

    return rc;
}

int ov7690_sensor_release(void)
{
    int rc = 0;

    CSDBG("ov7690_release()\n");

    if(ov7690_ctrl)
    {
        kfree(ov7690_ctrl);
        ov7690_ctrl = NULL;
    }

    return rc;
}

static int ov7690_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;

    CDBG("ov7690_i2c_probe\n");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		rc = -ENOTSUPP;
		goto probe_failure;
	}

	ov7690_sensorw =
		kzalloc(sizeof(struct ov7690_work), GFP_KERNEL);

	if (!ov7690_sensorw) {
		rc = -ENOMEM;
		goto probe_failure;
	}

	i2c_set_clientdata(client, ov7690_sensorw);
	ov7690_init_client(client);
	ov7690_client = client;

	CDBG("ov7690_i2c_probe successed!\n");

	return 0;

probe_failure:
	kfree(ov7690_sensorw);
	ov7690_sensorw = NULL;
	CDBG("ov7690_i2c_probe failed!\n");
	return rc;
}

static const struct i2c_device_id ov7690_i2c_id[] = {
	{ "ov7690", 0},
	{ },
};

static struct i2c_driver ov7690_i2c_driver = {
	.id_table = ov7690_i2c_id,
	.probe  = ov7690_i2c_probe,
	.remove = __exit_p(ov7690_i2c_remove),
	.driver = {
		.name = "ov7690",
	},
};

static int ov7690_sensor_probe(const struct msm_camera_sensor_info *info,
				struct msm_sensor_ctrl *s)
{
	int rc = 0;

    printk("BootLog, +%s\n", __func__);

	rc = i2c_add_driver(&ov7690_i2c_driver);
	if (rc < 0 || ov7690_client == NULL) {
		rc = -ENOTSUPP;
		goto probe_done;
	}

#if 0
	msm_camio_clk_rate_set(24000000);
    msleep(5);

	rc = ov7690_sensor_init_probe(info);
	if (rc < 0)
		goto probe_done;

    rc = ov7690_sensor_output_disable();
    ov7690_sensor_power_down(info);
	if (rc < 0)
		goto probe_done;
#endif

	s->s_init = ov7690_sensor_init;
	s->s_release = ov7690_sensor_release;
	s->s_config  = ov7690_sensor_config;

probe_done:

	CDBG("%s %s:%d\n", __FILE__, __func__, __LINE__);
    printk("BootLog, -%s, ret=%d\n", __func__, rc);
	return rc;
}

static int __ov7690_probe(struct platform_device *pdev)
{
	return msm_camera_drv_start(pdev, ov7690_sensor_probe);
}

static struct platform_driver msm_camera_driver = {
	.probe = __ov7690_probe,
	.driver = {
		.name = "msm_camera_ov7690",
		.owner = THIS_MODULE,
	},
};

static int __init ov7690_init(void)
{
	return platform_driver_register(&msm_camera_driver);
}

module_init(ov7690_init);
