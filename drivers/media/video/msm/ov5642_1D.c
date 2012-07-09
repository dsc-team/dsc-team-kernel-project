
#include "ov5642.h"


struct ov5642_reg_t const ov5642_1D_default_reg[] =
{
    { 0x3103, 0x93 },  
    { 0x3008, 0x82 },  


    { 0x3017, 0x7f },
    { 0x3018, 0xfc },
    { 0x3810, 0xc2 }, 
    { 0x3615, 0xf0 },

    { 0x3001, 0x00 },
    { 0x3002, 0x00 },
    { 0x3003, 0x00 },

    { 0x3030, 0x2b }, 

    { 0x3011, 0x08 },
    { 0x3010, 0x10 },

    { 0x3604, 0x60 },
    { 0x3622, 0x60 }, 
    { 0x3621, 0x09 }, 
    { 0x3709, 0x00 },

    { 0x4000, 0x21 },
    { 0x401d, 0x22 },  

    { 0x3600, 0x54 },
    { 0x3605, 0x04 },
    { 0x3606, 0x3f },
    { 0X3623, 0X01 },
    { 0X3630, 0X24 },
    { 0X3633, 0X00 },
    { 0X3C00, 0X00 },
    { 0X3C01, 0X34 },
    { 0X3C04, 0X28 },
    { 0X3C05, 0X98 },
    { 0X3C06, 0X00 },
    { 0X3C07, 0X07 },
    { 0X3C08, 0X01 },
    { 0X3C09, 0XC2 },
    { 0X300D, 0X02 },
    { 0X3104, 0X01 },
    { 0X3C0A, 0X4E },
    { 0X3C0B, 0X1F },

    { 0x5020, 0x04 },
    { 0x5181, 0x79 }, 
    { 0x5182, 0x00 }, 
    { 0x5185, 0x22 }, 
    { 0x5197, 0x01 }, 
    { 0x5500, 0x0a },
    { 0x5504, 0x00 },
    { 0x5505, 0x7f },
    { 0x5080, 0x08 },
    { 0x300e, 0x18 },
    { 0x4610, 0x00 },
    { 0x471d, 0x05 },
    { 0x4708, 0x06 },

    { 0x370c, 0xa0 },
    { 0x3808, 0x0a },
    { 0x3809, 0x20 },
    { 0x380a, 0x07 },
    { 0x380b, 0x98 },
    { 0x380c, 0x0c },
    { 0x380d, 0x80 },
    { 0x380e, 0x07 },
    { 0x380f, 0xd0 },
    { 0x5687, 0x94 },
    { 0x501f, 0x00 },

    { 0x4300, 0x30 },
    { 0x4300, 0x30 },
    { 0x460b, 0x35 },
    { 0x471d, 0x00 },
    { 0x3002, 0x0c },
    { 0x3002, 0x00 },
    { 0x4713, 0x03 },
    { 0x471c, 0x50 },
    { 0x4721, 0x02 },  
    { 0x4402, 0x90 },  
    { 0x460c, 0x22 },
    { 0x3815, 0x44 },
    { 0x3503, 0x07 },
    { 0x3501, 0x73 },
    { 0x3502, 0x80 },
    { 0x350b, 0x00 },
    { 0x3818, 0xc8 },
    { 0x3801, 0x88 },  
    { 0x3824, 0x11 },  
    { 0x3a00, 0x78 },
    { 0x3a1a, 0x04 },
    { 0x3a13, 0x30 },
    { 0x3a18, 0x00 },
    { 0x3a19, 0x3e }, 
    { 0x3a08, 0x12 },
    { 0x3a09, 0xc0 },
    { 0x3a0a, 0x0f },
    { 0x3a0b, 0xa0 },
    { 0x350c, 0x07 },
    { 0x350d, 0xd0 },
    { 0x3a0d, 0x08 },
    { 0x3a0e, 0x06 },
    { 0x3500, 0x00 },
    { 0x3501, 0x00 },
    { 0x3502, 0x00 },
    { 0x350a, 0x00 },
    { 0x350b, 0x00 },
    { 0x3503, 0x00 },


    { 0x5193, 0x70 },
    { 0x589b, 0x00 },
    { 0x589a, 0xc0 },
    { 0x4001, 0x42 },  
    { 0x401c, 0x06 },  
    { 0x3825, 0xac },  
    { 0x3827, 0x0c },  

    { 0x3710, 0x10 },
    { 0x3632, 0x51 },
    { 0x3702, 0x10 },
    { 0x3703, 0xb2 },
    { 0x3704, 0x18 },
    { 0x370b, 0x40 },
    { 0x370d, 0x03 }, 

    { 0x3631, 0x01 },
    { 0x3632, 0x52 }, 
    { 0x3606, 0x24 },
    { 0x3620, 0x96 }, 
    { 0x5785, 0x07 },
    { 0x3a13, 0x30 },
    { 0x3600, 0x52 },
    { 0x3604, 0x48 },
    { 0x3606, 0x1b },   
    { 0x370d, 0x0b },   
    { 0x370f, 0xc0 },   
    { 0x3709, 0x01 },   
    { 0x3823, 0x00 },   
    
    { 0x5007, 0x00 },
    { 0x5009, 0x00 },
    { 0x5011, 0x00 },
    { 0x5013, 0x00 },
    { 0x5086, 0x00 },
    { 0x5087, 0x00 },
    { 0x5088, 0x00 },
    { 0x5089, 0x00 },
    { 0x302b, 0x00 },

    { 0x4407, 0x06 },
};

char const ov5642_1D_IQ_lensC_reg[] =
{
    0x58,
    0x00,
    0x4a,
    0x2c,
    0x23,
    0x1f,
    0x20,
    0x23,
    0x2f, 
    0x51, 
    0x1d, 
    0x19,
    0x12,
    0xf ,
    0xf ,
    0x13,
    0x18,
    0x20, 
    0x13,
    0xd ,
    0x7 ,
    0x5 ,
    0x5 ,
    0x8 ,
    0xd ,
    0x16,
    0xe ,
    0x8 ,
    0x2 ,
    0x0 ,
    0x0 ,
    0x3 ,
    0x8 ,
    0x10,
    0xd ,
    0x8 ,
    0x2 ,
    0x0 ,
    0x0 ,
    0x3 ,
    0x8 ,
    0x10,
    0x13,
    0xd ,
    0x7 ,
    0x5 ,
    0x5 ,
    0x8 ,
    0xd ,
    0x15,
    0x1d,
    0x18,
    0x12,
    0xf ,
    0xf ,
    0x12,
    0x17,
    0x23,
    0x49,
    0x2e,
    0x23,
    0x1e,
    0x1e,
    0x21,
    0x31,
    0x52,

    
    0x10, 
    0xe , 
    0xc , 
    0xc , 
    0xe , 
    0xb , 
    0xa , 
    0xe , 
    0xe , 
    0xe , 
    0xc , 
    0xd , 
    0x9 , 
    0x10, 
    0x11, 
    0x10, 
    0xe , 
    0xc , 
    0x8 , 
    0x10, 
    0x10, 
    0x10, 
    0xe , 
    0xe , 
    0x8 , 
    0xe , 
    0xe , 
    0xe , 
    0xd , 
    0xd , 
    0xb , 
    0xb , 
    0xa , 
    0xa , 
    0xa , 
    0x10, 
    
    0x14, 
    0x18, 
    0x19, 
    0x19, 
    0x17, 
    0x16, 
    0x1c, 
    0x16, 
    0x15, 
    0x14, 
    0x17, 
    0x19, 
    0x1a, 
    0x12, 
    0x10, 
    0x10, 
    0x13, 
    0x16, 
    0x1a, 
    0x12, 
    0x10, 
    0x10, 
    0x13, 
    0x18, 
    0x1b, 
    0x15, 
    0x14, 
    0x14, 
    0x16, 
    0x17, 
    0x18, 
    0x16, 
    0x1a, 
    0x19, 
    0x17, 
    0x14, 
};

char const ov5642_1D_IQ_awb_reg[] =
{
    
    
    0x51, 
    0x80,
    0xff,
    0x54, 
    0x11, 
    0x14, 
    0x25, 
    0x24, 
    0x12, 
    0x12, 
    0x18, 
    0x60, 
    0x5f, 
    0xa8, 
    0x98, 
    0x37, 
    0x42, 
    0x60, 
    0x57, 
    0xf8, 
    0x4 , 
    0x70, 
    0xf0, 
    0xf0, 
    0x3 , 
    0x1 , 
    0x4 , 
    0xfb, 
    0x4 , 
    0x0 , 
    0x6 , 
    0xc3, 
    0xa0, 
};

struct ov5642_reg_t const ov5642_1D_IQ_reg[] =
{
    { 0x5000, 0xcf },
    { 0x5001, 0x3f },
    { 0x3a14, 0x00 },
    { 0x3a15, 0x7d },
    { 0x3a16, 0x00 },
    { 0x3a08, 0x09 },
    { 0x3a09, 0x60 },
    { 0x3a0a, 0x07 },
    { 0x3a0b, 0xd0 },
    { 0x3a0d, 0x10 },
    { 0x3a0e, 0x0d },

    { 0x530c, 0x10 },
    { 0x530d, 0x38 },
    { 0x5312, 0x08 },

    { 0x530e, 0x20 },
    { 0x530f, 0x80 },
    { 0x5310, 0x20 },
    { 0x5311, 0x80 },

    { 0x3406, 0x00 },

    { 0x4007, 0x10 },

    { 0x5001, 0xbf },
    { 0x5583, 0x38 },
    { 0x5584, 0x38 },
    { 0x5580, 0x06 },
    { 0x5587, 0x20 },
    { 0x5588, 0x20 },
    { 0x5589, 0x00 },

};


struct ov5642_reg_t const ov5642_1D_preview_reg[] =
{
    { 0x3002, 0x5c },
    { 0x3003, 0x2  },
    { 0x3005, 0xff },
    { 0x3006, 0x43 },
    { 0x3007, 0x37 },
    { 0x3011, 0x0a },
    { 0x3012, 0x02 },
    { 0x3602, 0xfc },
    { 0x3612, 0xff },
    { 0x3613, 0x00 },
    { 0x3621, 0x87 },
    { 0x3622, 0x60 },
    { 0x3623, 0x01 },
    { 0x3604, 0x48 },
    { 0x3705, 0xdb },
    { 0x370a, 0x81 },
    { 0x370d, 0x0b },
    { 0x3801, 0x50 },
    { 0x3803, 0x8  },
    { 0x3804, 0x5  },
    { 0x3805, 0x0  },
    { 0x3806, 0x3  },
    { 0x3807, 0xc0 },
    { 0x3808, 0x05 }, 
    { 0x3809, 0x00 }, 
    { 0x380a, 0x03 }, 
    { 0x380b, 0xc0 }, 
    { 0x380c, 0x07 },
    { 0x380d, 0xc0 },
    { 0x380e, 0x3  },
    { 0x380f, 0xf0 },
    { 0x3810, 0x80 }, 
    { 0x3815, 0x2  },
    { 0x3818, 0xc1 },
    { 0x3824, 0x11 },
    { 0x3825, 0xb0 },
    { 0x3826, 0x00 },
    { 0x3827, 0x8  },

    { 0x3a08, 0x12 },
    { 0x3a09, 0xc0 },
    { 0x3a0a, 0x0f },
    { 0x3a0b, 0xa0 },
    { 0x3a0d, 0x04 },
    { 0x3a0e, 0x03 },
    { 0x3a1a, 0x05 },

    { 0x401c, 0x04 },
    
    { 0x460b, 0x37 },
    { 0x471d, 0x5  },
    { 0x4713, 0x3  },
    { 0x471c, 0xd0 },

    { 0x5682, 0x5  },
    { 0x5683, 0x0  },
    { 0x5686, 0x3  },
    { 0x5687, 0xbc },

    { 0x5001, 0xbf }, 
    { 0x589b, 0x4  },
    { 0x589a, 0xc5 },
    
    { 0x589b, 0x0  },
    { 0x589a, 0xc0 },
    { 0x3002, 0x0c },
    { 0x3002, 0x00 },
    { 0x3503, 0x00 },
    { 0x460c, 0x20 },
    { 0x3010, 0x10 },
    { 0x3012, 0x01 },

    { 0x3A08, 0xC  },       
    { 0x3A09, 0x80 },       
    { 0x3A0E, 0x5  },       
    { 0x3A0A, 0xA  },       
    { 0x3A0B, 0x70 },       
    { 0x3A0D, 0x6  },
};


struct ov5642_reg_t const ov5642_1D_snapshot_reg[] =
{
    { 0x3003, 0x00 }, 
    { 0x3005, 0xff }, 
    { 0x3006, 0xff }, 
    { 0x3007, 0x3f }, 
    { 0x3011, 0x08 }, 
    { 0x3012, 0x00 }, 

    { 0x3602, 0xe4 }, 
    { 0x3612, 0xac }, 
    { 0x3613, 0x44 }, 
    { 0x3621, 0x09 }, 
    { 0x3622, 0x60 }, 
    { 0x3623, 0x22 }, 
    { 0x3604, 0x60 }, 
    { 0x3705, 0xd9 }, 
    { 0x370a, 0x80 }, 
    { 0x370d, 0x03 }, 
    { 0x3801, 0x8a }, 
    { 0x3803, 0x0a }, 
    { 0x3804, 0x0a }, 
    { 0x3805, 0x20 },
    { 0x3806, 0x07 }, 
    { 0x3807, 0x98 }, 
    { 0x3808, 0x0a }, 
    { 0x3809, 0x20 }, 
    { 0x380a, 0x07 }, 
    { 0x380b, 0x98 }, 
    { 0x380c, 0x0c }, 
    { 0x380d, 0x80 }, 
    { 0x380e, 0x07 }, 
    { 0x380f, 0xd0 }, 



    { 0x3824, 0x11 }, 
    { 0x3825, 0xac }, 
    { 0x3827, 0x0a }, 
    { 0x3a08, 0x09 }, 
    { 0x3a09, 0x60 }, 
    { 0x3a0a, 0x07 }, 
    { 0x3a0b, 0xd0 }, 
    { 0x3a0d, 0x10 }, 
    { 0x3a0e, 0x0d }, 
    { 0x3a1a, 0x04 }, 
    { 0x460b, 0x35 },
    { 0x471d, 0x00 },
    { 0x4713, 0x03 },

    { 0x5682, 0x0a },  
    { 0x5683, 0x20 },  
    { 0x5686, 0x07 },  
    { 0x5687, 0x98 },  
    { 0x5001, 0xbf },  
    { 0x589b, 0x00 },  
    { 0x589a, 0xc0 },  

    { 0x589b, 0x00 },  
    { 0x589a, 0xc0 },  
    
    { 0x381c, 0x21 }, 
    { 0x381d, 0x50 }, 
    { 0x381e, 0x01 }, 
    { 0x381f, 0x20 }, 
    { 0x3820, 0x00 }, 
    { 0x3821, 0x00 }, 
    
    { 0x3810, 0xc2 },
    { 0x3818, 0xc8 },
    { 0x5001, 0xbf }, 
    { 0x589b, 0x00 }, 
    { 0x589a, 0xc0 }, 
    
    { 0x4300, 0x30 },
    { 0x460b, 0x35 },
    { 0x3002, 0x0c },
    { 0x3002, 0x00 },
    { 0x4713, 0x04 },
    { 0x4600, 0x80 },
    { 0x4721, 0x02 },
    { 0x471c, 0x50 },
    { 0x4408, 0x00 },
    { 0x460c, 0x22 },
    { 0x3815, 0x42 },
    { 0x4402, 0x90 },
    { 0x4602, 0x05 },
    { 0x4603, 0x00 },
    { 0x4604, 0x07 },
    { 0x4605, 0x98 },

    { 0x300F, 0x6  },
    { 0x3010, 0x10 },
    { 0x3011, 0x8  },
    { 0x3012, 0x0  },
    { 0x380C, 0xC  },
    { 0x380D, 0x80 },
    { 0x3815, 0x44 },
    { 0x460C, 0x22 },
    { 0x3615, 0x0  },
    { 0x3A08, 0x9  },
    { 0x3A09, 0x60 },
    { 0x3A0E, 0xD  },
    { 0x3A0A, 0x7  },
    { 0x3A0B, 0xD0 },
    { 0x3A0D, 0x10 },
};

struct ov5642_reg_burst_group_t const ov5642_1D_IQ_lensC_group =
{
    (char *)ov5642_1D_IQ_lensC_reg,
    ARRAY_SIZE(ov5642_1D_IQ_lensC_reg)
};

struct ov5642_reg_burst_group_t const ov5642_1D_IQ_awb_group =
{
    (char *)ov5642_1D_IQ_awb_reg,
    ARRAY_SIZE(ov5642_1D_IQ_awb_reg)
};

struct ov5642_basic_iq_misc_groups const ov5642_1D_IQ_misc_groups =
{
    .lens_c_group = &ov5642_1D_IQ_lensC_group,
    .awb_group = &ov5642_1D_IQ_awb_group,
};

struct ov5642_basic_reg_arrays ov5642_1D_regs = {
	.default_array = &ov5642_1D_default_reg[0],
	.default_length = ARRAY_SIZE(ov5642_1D_default_reg),
	.preview_array = &ov5642_1D_preview_reg[0],
	.preview_length = ARRAY_SIZE(ov5642_1D_preview_reg),
	.snapshot_array = &ov5642_1D_snapshot_reg[0],
	.snapshot_length = ARRAY_SIZE(ov5642_1D_snapshot_reg),
        .IQ_array = &ov5642_1D_IQ_reg[0],
        .IQ_length = ARRAY_SIZE(ov5642_1D_IQ_reg),
    .IQ_misc_groups = &ov5642_1D_IQ_misc_groups,
};


#define EXPOSURE_HIGH    0
#define EXPOSURE_MID     1
#define EXPOSURE_LOW     2
#define GAIN             3
#define VTS_MAXLINE_HIGH 4
#define VTS_MAXLINE_LOW  5
#define AWB              6
#define LIGHT_INDICATOR  AWB
static struct ov5642_reg_t ov5642_1D_transform_reg[] =
{
    { 0x3500, 0x00 },
    { 0x3501, 0x00 },
    { 0x3502, 0x00 },
    { 0x350b, 0x00 },
    { 0x350c, 0x00 },
    { 0x350d, 0x00 },
    { 0x3406, 0x00 },
};

#define AUTO_TRANSFORM   0
#define MANUAL_TRANSFORM 1
static char ov5642_1D_transform_flag = AUTO_TRANSFORM;

static long ov5642_1D_preview_transform(enum ov5642_white_balance_t wb)
{
    long rc;

    if( wb == AUTO_WB )
        ov5642_1D_transform_reg[AWB].data = 0x00;
    else
        ov5642_1D_transform_reg[AWB].data = 0x01;

    CSDBG("ov5642_1D_preview_transform: exposure=0x%02X %02X %02X gain=0x%02X awb=0x%02X vts=0x%02X %02X\n",
          ov5642_1D_transform_reg[EXPOSURE_HIGH].data,
          ov5642_1D_transform_reg[EXPOSURE_MID].data,
          ov5642_1D_transform_reg[EXPOSURE_LOW].data,
          ov5642_1D_transform_reg[GAIN].data,
          ov5642_1D_transform_reg[AWB].data,
          ov5642_1D_transform_reg[VTS_MAXLINE_HIGH].data,
          ov5642_1D_transform_reg[VTS_MAXLINE_LOW].data );

    if( MANUAL_TRANSFORM == ov5642_1D_transform_flag )
    {
        unsigned long exposure = 0;

        exposure = (ov5642_1D_transform_reg[EXPOSURE_HIGH].data << 16) |
                   (ov5642_1D_transform_reg[EXPOSURE_MID].data << 8) |
                   ov5642_1D_transform_reg[EXPOSURE_LOW].data;

        rc = ov5642_i2c_write( (struct ov5642_reg_t *)ov5642_1D_transform_reg,
                               AWB+1 );

        ov5642_1D_transform_flag = AUTO_TRANSFORM;
    }else {
        ov5642_1D_transform_reg[VTS_MAXLINE_HIGH].data = 0x03;
        ov5642_1D_transform_reg[VTS_MAXLINE_LOW].data = 0xf0;
        rc = ov5642_i2c_write( (struct ov5642_reg_t *)&ov5642_1D_transform_reg[VTS_MAXLINE_HIGH],
                               3 );
    }

    return rc;
}

static long ov5642_1D_snapshot_transform(enum ov5642_anti_banding_t banding)
{
    long rc;
    struct ov5642_reg_t const ov5642_stop_ae_awb[] =
    {
        { 0x3503, 0x07 },
        { 0x3406, 0x01 },
    };

    struct ov5642_reg_t ov5642_transform_read[] =
    {
        { 0x3500, 0x00 },
        { 0x3501, 0x00 },
        { 0x3502, 0x00 },
        { 0x350b, 0x00 },
        { 0x350c, 0x00 },
        { 0x350d, 0x00 },
        { 0x3c0c, 0x00 },
    };

#define  CAPTURE_FRAMERATE 750
#define  PREVIEW_FRAMERATE 2000

    unsigned long ulLines_10ms;
    unsigned int Capture_MaxLines, Preview_Maxlines;
    unsigned long ulCapture_Exposure, ulPreviewExposure, ulCapture_Exposure_Gain;
    long iCapture_Gain;
    char Gain;
    unsigned long aec_pk_vts;

    rc = ov5642_i2c_write( (struct ov5642_reg_t *)ov5642_stop_ae_awb,
                           ARRAY_SIZE(ov5642_stop_ae_awb) );
    if( rc )
        goto exit_config_exp;

    rc = ov5642_i2c_read( (struct ov5642_reg_t *)ov5642_transform_read,
                          ARRAY_SIZE(ov5642_transform_read) );
    if( rc )
        goto exit_config_exp;

    ov5642_1D_transform_flag = MANUAL_TRANSFORM;

    CSDBG("ov5642_1D_snapshot_transform: exposure=0x%02X %02X %02X gain=0x%02X vts=0x%02X %02X \n",
          ov5642_transform_read[EXPOSURE_HIGH].data,
          ov5642_transform_read[EXPOSURE_MID].data,
          ov5642_transform_read[EXPOSURE_LOW].data,
          ov5642_transform_read[GAIN].data,
          ov5642_transform_read[VTS_MAXLINE_HIGH].data, ov5642_transform_read[VTS_MAXLINE_LOW].data );

    ov5642_transform_read[EXPOSURE_HIGH].data &= 0x0F;
    ulPreviewExposure  = ((unsigned long)(ov5642_transform_read[EXPOSURE_HIGH].data))<<12 ;
    ulPreviewExposure += ((unsigned long)ov5642_transform_read[EXPOSURE_MID].data)<<4 ;
    ulPreviewExposure += (ov5642_transform_read[EXPOSURE_LOW].data >>4);
    Gain = ov5642_transform_read[GAIN].data;

    Preview_Maxlines = 0x3f0;

    rc = ov5642_i2c_write( (struct ov5642_reg_t *)ov5642_1D_snapshot_reg,
                            ARRAY_SIZE(ov5642_1D_snapshot_reg) );
    if( rc )
        goto exit_config_exp;

    Capture_MaxLines = 0x7d0;

    if( banding == AUTO_BANDING )
    {
        if( ov5642_transform_read[LIGHT_INDICATOR].data & 0x01 )
            banding = MANUAL_50HZ_BANDING;
        else
            banding = MANUAL_60HZ_BANDING;
    }

    if((banding == MANUAL_60HZ_BANDING) )
    {
        ulLines_10ms = CAPTURE_FRAMERATE * Capture_MaxLines / 12000;
    }
    else
    {
        ulLines_10ms = CAPTURE_FRAMERATE * Capture_MaxLines / 10000;
    }

    if(0 == Preview_Maxlines ||0 ==PREVIEW_FRAMERATE || 0== ulLines_10ms)
    {
        CSDBG("ov5642_1D_snapshot_transform: zero parameters\n");
        rc = -1;
        goto exit_config_exp;
    }

    if( ulPreviewExposure > 0x3f0 )
    {
        unsigned long long temp;

        temp =
	       (unsigned long long)(ulPreviewExposure*3*Capture_MaxLines) / (unsigned long long)(4*Preview_Maxlines);
        ulCapture_Exposure = (unsigned long)temp;
    }else {
        unsigned long long temp;
        temp =
	       (unsigned long long)(ulPreviewExposure*15*Capture_MaxLines) / (unsigned long long)(16*Preview_Maxlines);
        ulCapture_Exposure = (unsigned long)temp;
    }
    CDBG("%s: ulCapture_Exposure=%lx, ulPreviewExposure=%lx, Capture_MaxLines=%x, Preview_Maxlines=%x\n",
         __func__, ulCapture_Exposure, ulPreviewExposure, Capture_MaxLines, Preview_Maxlines);


    iCapture_Gain = (Gain & 0x0f) + 16;
    if (Gain & 0x10)
    {
       iCapture_Gain = iCapture_Gain << 1;
    }
    if (Gain & 0x20)
    {
       iCapture_Gain = iCapture_Gain << 1;
    }
    if (Gain & 0x40)
    {
       iCapture_Gain = iCapture_Gain << 1;
    }
    if (Gain & 0x80)
    {
       iCapture_Gain = iCapture_Gain << 1;
    }

    ulCapture_Exposure_Gain = ulCapture_Exposure * iCapture_Gain;
    if(ulCapture_Exposure_Gain < ((unsigned long)(Capture_MaxLines)*16))
    {
       ulCapture_Exposure = ulCapture_Exposure_Gain/16;
       if (ulCapture_Exposure > ulLines_10ms)
       {
        ulCapture_Exposure /= ulLines_10ms;
        ulCapture_Exposure *= ulLines_10ms;
       }
    }
    else
    {
       ulCapture_Exposure = Capture_MaxLines;
    }
    CDBG("%s: ulCapture_Exposure_Gain=%lx, iCapture_Gain=%lx, ulCapture_Exposure=%lx\n",
         __func__, ulCapture_Exposure_Gain, iCapture_Gain, ulCapture_Exposure);

    if(ulCapture_Exposure == 0)
    {
      ulCapture_Exposure = 1;
    }
    iCapture_Gain = ulCapture_Exposure_Gain / ulCapture_Exposure;
    iCapture_Gain += ((ulCapture_Exposure_Gain%ulCapture_Exposure)*2/ulCapture_Exposure + 1)/2;

    ov5642_transform_read[EXPOSURE_LOW].data =  (char)((ulCapture_Exposure << 4) & 0xff);
    ov5642_transform_read[EXPOSURE_MID].data = (char)((ulCapture_Exposure >> 4) & 0xff);
    ov5642_transform_read[EXPOSURE_HIGH].data = (char)((ulCapture_Exposure >> 12) & 0xff);

    Gain = 0;
    if (iCapture_Gain > 31)
    {
        Gain |= 0x10;
        iCapture_Gain = iCapture_Gain >> 1;
    }
    if (iCapture_Gain > 31)
    {
        Gain |= 0x20;
        iCapture_Gain = iCapture_Gain >> 1;
    }
    if (iCapture_Gain > 31)
    {
        Gain |= 0x40;
        iCapture_Gain = iCapture_Gain >> 1;
    }
    if (iCapture_Gain > 31)
    {
        Gain |= 0x80;
        iCapture_Gain = iCapture_Gain >> 1;
    }

    if (iCapture_Gain > 16)
    {
        Gain |= ((iCapture_Gain -16) & 0x0f);
    }
    if(Gain==0x10)
        {Gain=0x11;}

    ov5642_transform_read[GAIN].data = Gain;


    aec_pk_vts = ulPreviewExposure * 2;
    if( (aec_pk_vts & 0xFFFF) < 0x7D0 )
        aec_pk_vts = 0x7D0;
    ov5642_transform_read[VTS_MAXLINE_HIGH].data = (aec_pk_vts >> 8) & 0xFF;
    ov5642_transform_read[VTS_MAXLINE_LOW].data = aec_pk_vts & 0xFF;

    CDBG("ov5642_1D_snapshot_transform: exposure=0x%02X %02X %02X gain=0x%02X vts=0x%02X %02X \n",
          ov5642_transform_read[EXPOSURE_HIGH].data,
          ov5642_transform_read[EXPOSURE_MID].data,
          ov5642_transform_read[EXPOSURE_LOW].data,
          ov5642_transform_read[GAIN].data,
          ov5642_transform_read[VTS_MAXLINE_HIGH].data, ov5642_transform_read[VTS_MAXLINE_LOW].data );

    rc = ov5642_i2c_write( (struct ov5642_reg_t *)ov5642_transform_read,
                            ARRAY_SIZE(ov5642_transform_read)-1 );

exit_config_exp:
    return rc;
}

static long ov5642_1D_prepare_snapshot(void) {
	long rc;
    
	struct ov5642_reg_t exp_gain_vts[] = {
		{ 0x3500, 0x00 },
		{ 0x3501, 0x00 },
		{ 0x3502, 0x00 },
		{ 0x350b, 0x00 },
		{ 0x350c, 0x00 },
		{ 0x350d, 0x00 }
	};

	rc = ov5642_i2c_read( (struct ov5642_reg_t *)exp_gain_vts,
				ARRAY_SIZE(exp_gain_vts) );
	if( rc )
		return rc;

	memcpy(ov5642_1D_transform_reg,
		exp_gain_vts,
		sizeof(exp_gain_vts));
	CSDBG("ov5642_1D_prepare_snapshot: exposure=0x%02X %02X %02X gain=0x%02X vts=0x%02X %02X \n",
		ov5642_1D_transform_reg[EXPOSURE_HIGH].data,
		ov5642_1D_transform_reg[EXPOSURE_MID].data,
		ov5642_1D_transform_reg[EXPOSURE_LOW].data,
		ov5642_1D_transform_reg[GAIN].data,
		ov5642_1D_transform_reg[VTS_MAXLINE_HIGH].data, ov5642_1D_transform_reg[VTS_MAXLINE_LOW].data );

	return rc;
}

struct ov5642_ae_func_array ov5642_1D_ae_func =
{
    .preview_to_snapshot = ov5642_1D_snapshot_transform,
    .snapshot_to_preview = ov5642_1D_preview_transform,
    .prepare_snapshot = ov5642_1D_prepare_snapshot,
};




