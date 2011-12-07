
#include "ov5642.h"

#ifdef USE_AF
#include "ov5642_af.h"
#endif

struct ov5642_reg_t const ov5642_320x240_reg[] =
{
    { 0x3800, 0x01 },
    { 0x3801, 0x8A },
    { 0x3802, 0x00 },
    { 0x3803, 0x0A },
    { 0x3804, 0x0A },
    { 0x3805, 0x20 },
    { 0x3806, 0x07 },
    { 0x3807, 0x98 },
    { 0x3808, 0x01 },
    { 0x3809, 0x40 },
    { 0x380A, 0x00 },
    { 0x380B, 0xF0 },
    { 0x380C, 0x0C },
    { 0x380D, 0x80 },
    { 0x380E, 0x07 },
    { 0x380F, 0xD0 },
    { OV5642_BIT_OPERATION_TAG, 0x3F },
    { 0x5001, 0x3f },
    { 0x5680, 0x00 },
    { 0x5681, 0x00 },
    { 0x5682, 0x0A },
    { 0x5683, 0x20 },
    { 0x5684, 0x00 },
    { 0x5685, 0x00 },
    { 0x5686, 0x07 },
    { 0x5687, 0x98 },
};

struct ov5642_reg_t const ov5642_640x480_reg[] =
{
    { 0x3800, 0x01 },
    { 0x3801, 0x8A },
    { 0x3802, 0x00 },
    { 0x3803, 0x0A },
    { 0x3804, 0x0A },
    { 0x3805, 0x20 },
    { 0x3806, 0x07 },
    { 0x3807, 0x98 },
    { 0x3808, 0x02 },
    { 0x3809, 0x80 },
    { 0x380A, 0x01 },
    { 0x380B, 0xE0 },
    { 0x380C, 0x0C },
    { 0x380D, 0x80 },
    { 0x380E, 0x07 },
    { 0x380F, 0xD0 },
    { OV5642_BIT_OPERATION_TAG, 0x3F },
    { 0x5001, 0x3f },
    { 0x5680, 0x00 },
    { 0x5681, 0x00 },
    { 0x5682, 0x0A },
    { 0x5683, 0x20 },
    { 0x5684, 0x00 },
    { 0x5685, 0x00 },
    { 0x5686, 0x07 },
    { 0x5687, 0x98 },
};

struct ov5642_reg_t const ov5642_1280x960_reg[] =
{
    { 0x3800, 0x01 }, 
    { 0x3801, 0x8A },
    { 0x3802, 0x00 }, 
    { 0x3803, 0x0A },
    { 0x3804, 0x0A }, 
    { 0x3805, 0x20 },
    { 0x3806, 0x07 }, 
    { 0x3807, 0x98 },
    { 0x3808, 0x05 },
    { 0x3809, 0x00 },
    { 0x380A, 0x03 },
    { 0x380B, 0xC0 },
    { 0x380C, 0x0C }, 
    { 0x380D, 0x80 },
    { 0x380E, 0x07 }, 
    { 0x380F, 0xD0 },
    { OV5642_BIT_OPERATION_TAG, 0x3F },
    { 0x5001, 0x3f },
    { 0x5680, 0x00 },
    { 0x5681, 0x00 },
    { 0x5682, 0x0A },
    { 0x5683, 0x20 },
    { 0x5684, 0x00 },
    { 0x5685, 0x00 },
    { 0x5686, 0x07 },
    { 0x5687, 0x98 },
};

struct ov5642_reg_t const ov5642_1600x1200_reg[] =
{
    { 0x3800, 0x01 }, 
    { 0x3801, 0x8A },
    { 0x3802, 0x00 }, 
    { 0x3803, 0x0A },
    { 0x3804, 0x0A }, 
    { 0x3805, 0x20 },
    { 0x3806, 0x07 }, 
    { 0x3807, 0x98 },
    { 0x3808, 0x06 },
    { 0x3809, 0x40 },
    { 0x380A, 0x04 },
    { 0x380B, 0xB0 },
    { 0x380C, 0x0C }, 
    { 0x380D, 0x80 },
    { 0x380E, 0x07 }, 
    { 0x380F, 0xD0 },
    { OV5642_BIT_OPERATION_TAG, 0x3F },
    { 0x5001, 0x3f },
    { 0x5680, 0x00 },
    { 0x5681, 0x00 },
    { 0x5682, 0x0A },
    { 0x5683, 0x20 },
    { 0x5684, 0x00 },
    { 0x5685, 0x00 },
    { 0x5686, 0x07 },
    { 0x5687, 0x98 },
};

struct ov5642_reg_t const ov5642_2048x1536_reg[] =
{
    { 0x3800, 0x01 }, 
    { 0x3801, 0x8A },
    { 0x3802, 0x00 }, 
    { 0x3803, 0x0A },
    { 0x3804, 0x0A }, 
    { 0x3805, 0x20 },
    { 0x3806, 0x07 }, 
    { 0x3807, 0x98 },
    { 0x3808, 0x08 },
    { 0x3809, 0x00 },
    { 0x380A, 0x06 },
    { 0x380B, 0x00 },
    { 0x380C, 0x0C }, 
    { 0x380D, 0x80 },
    { 0x380E, 0x07 }, 
    { 0x380F, 0xD0 },
    { OV5642_BIT_OPERATION_TAG, 0x3F },
    { 0x5001, 0x3f },
    { 0x5680, 0x00 },
    { 0x5681, 0x00 },
    { 0x5682, 0x0A },
    { 0x5683, 0x20 },
    { 0x5684, 0x00 },
    { 0x5685, 0x00 },
    { 0x5686, 0x07 },
    { 0x5687, 0x98 },
};

struct ov5642_reg_t const ov5642_2592x1040_reg[] =
{
	{ 0x3824, 0x11 },
	{ 0x3825, 0xac },
	{ 0x3826, 0x01 },
	{ 0x3827, 0xc4 },
    { 0x3803, 0x0A },
    { 0x3804, 0x0a }, 
    { 0x3805, 0x20 },
    { 0x3806, 0x04 }, 
    { 0x3807, 0x10 },
    { 0x3808, 0x0a }, 
    { 0x3809, 0x20 },
    { 0x380A, 0x04 }, 
    { 0x380B, 0x10 },
    { 0x380C, 0x0C },
    { 0x380D, 0x80 },
    { 0x380E, 0x07 },
	{ 0x380f, 0xd0 },
};

struct ov5642_reg_group_t const ov5642_resolution_group[] =
{
    { (struct ov5642_reg_t *)ov5642_2592x1040_reg, ARRAY_SIZE(ov5642_2592x1040_reg) },
    { (struct ov5642_reg_t *)ov5642_2048x1536_reg, ARRAY_SIZE(ov5642_2048x1536_reg) },
    { (struct ov5642_reg_t *)ov5642_1600x1200_reg, ARRAY_SIZE(ov5642_1600x1200_reg) },
    { (struct ov5642_reg_t *)ov5642_1280x960_reg,  ARRAY_SIZE(ov5642_1280x960_reg) },
    { (struct ov5642_reg_t *)ov5642_640x480_reg,   ARRAY_SIZE(ov5642_640x480_reg) },
    { (struct ov5642_reg_t *)ov5642_320x240_reg,   ARRAY_SIZE(ov5642_320x240_reg) },
};

struct ov5642_reg_t const ov5642_1_of_1_fps_reg[] =
{
    { OV5642_BIT_OPERATION_TAG, 0x04 },
    { 0x3a00, 0x00 },
};

struct ov5642_reg_t const ov5642_1_of_3_fps_reg[] =
{
    { OV5642_BIT_OPERATION_TAG, 0x04 },
    { 0x3a00, 0x04 },
    { 0x3a02, 0x00 },
    { 0x3a03, 0xbb },
    { 0x3a04, 0x00 },
};

struct ov5642_reg_t const ov5642_1_of_5_fps_reg[] =
{
    { OV5642_BIT_OPERATION_TAG, 0x04 },
    { 0x3a00, 0x04 },
    { 0x3a02, 0x01 },
    { 0x3a03, 0x38 },
    { 0x3a04, 0x00 },
};

struct ov5642_reg_group_t const ov5642_frame_rate_group[] =
{
    { (struct ov5642_reg_t *)ov5642_1_of_1_fps_reg, ARRAY_SIZE(ov5642_1_of_1_fps_reg) },
    { (struct ov5642_reg_t *)ov5642_1_of_3_fps_reg, ARRAY_SIZE(ov5642_1_of_3_fps_reg) },
    { (struct ov5642_reg_t *)ov5642_1_of_5_fps_reg, ARRAY_SIZE(ov5642_1_of_5_fps_reg) },
};

char const ov5642_general_ae_reg[] =
{
    0x56,
    0x88,
    0x21,
    0x12,
    0x64,
    0x46,
    0xd6,
    0x6d,
    0x26,
    0x62,
};

char const ov5642_portrait_ae_reg[] =
{
    0x56, 
    0x88,
    0x00,
    0x00, 
    0xa0, 
    0x0a, 
    0xa0, 
    0x0a, 
    0x00, 
    0x00, 
};

char const ov5642_landscape_ae_reg[] =
{
    0x56, 
    0x88,
    0x11,
    0x11, 
    0x11, 
    0x11, 
    0x11, 
    0x11, 
    0x11, 
    0x11, 
};

struct ov5642_reg_burst_group_t const ov5642_ae_weighting_group[] =
{
    { (char *)ov5642_general_ae_reg, ARRAY_SIZE(ov5642_general_ae_reg) },
    { (char *)ov5642_portrait_ae_reg, ARRAY_SIZE(ov5642_portrait_ae_reg) },
    { (char *)ov5642_landscape_ae_reg, ARRAY_SIZE(ov5642_landscape_ae_reg) },
};

char const ov5642_general_cmx_reg[] =
{
    0x53, 
    0x80,
    0x1 ,
    0x0 , 
    0x0 , 
    0x4e, 
    0x0 , 
    0xf , 
    0x0 , 
    0x0 , 
    0x1 , 
    0x6d, 
    0x0 , 
    0x5b, 
    0x0 , 
    0x0 , 
    0x0 , 
    0xc , 
    0x1 , 
    0x28, 
    0x0 , 
    0xa2, 
    0x8 , 
};

char const ov5642_person_cmx_reg[] =
{
    0x53, 
    0x80,
    0x1 ,
    0x0 , 
    0x0 , 
    0x4e, 
    0x0 , 
    0xf , 
    0x0 , 
    0x0 , 
    0x1 , 
    0x2d, 
    0x0 , 
    0x7b, 
    0x0 , 
    0x0 , 
    0x0 , 
    0x6c, 
    0x1 , 
    0x8 , 
    0x0 , 
    0xa2, 
    0x8 , 
};

char const ov5642_land_cmx_reg[] =
{
    0x53, 
    0x80,
    0x1 ,
    0x0 , 
    0x0 , 
    0x4e, 
    0x0 , 
    0xf , 
    0x0 , 
    0x0 , 
    0x2 , 
    0xd , 
    0x0 , 
    0x2b, 
    0x0 , 
    0x0 , 
    0x0 , 
    0xc , 
    0x1 , 
    0x20, 
    0x0 , 
    0xa2, 
    0x8 , 
};

char const ov5642_snow_cmx_reg[] =
{
    0x53, 
    0x80,
    0x1 ,
    0xa0, 
    0x0 , 
    0x4e, 
    0x0 , 
    0xf , 
    0x0 , 
    0x0 , 
    0x1 , 
    0x6d, 
    0x0 , 
    0x2b, 
    0x0 , 
    0x0 , 
    0x0 , 
    0xc , 
    0x1 , 
    0x28, 
    0x0 , 
    0xa2, 
    0x8 , 
};

struct ov5642_reg_burst_group_t const ov5642_color_matrix_group[] =
{
    { (char *)ov5642_general_cmx_reg, ARRAY_SIZE(ov5642_general_cmx_reg) },
    { (char *)ov5642_person_cmx_reg, ARRAY_SIZE(ov5642_person_cmx_reg) },
    { (char *)ov5642_land_cmx_reg, ARRAY_SIZE(ov5642_land_cmx_reg) },
    { (char *)ov5642_snow_cmx_reg, ARRAY_SIZE(ov5642_snow_cmx_reg) },
};

char const ov5642_general_lens_c_reg[] =
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
};

char const ov5642_night_lens_c_reg[] =
{
    0x58, 
    0x00,
    0x00,
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
    0x00, 
};

struct ov5642_reg_burst_group_t const ov5642_lens_c_group[] =
{
    { (char *)ov5642_general_lens_c_reg, ARRAY_SIZE(ov5642_general_lens_c_reg) },
    { (char *)ov5642_night_lens_c_reg, ARRAY_SIZE(ov5642_night_lens_c_reg) },
};

struct ov5642_reg_t const ov5642_general_lens_c_reg_latch[] =
{
    {0x3200, 0x40}, 
    {0x3201, 0x78}, 
    {0x3202, 0xb0}, 
    {0x3203, 0xf0}, 
    {0x3212, 0x02}, 
    {0x5800, 0x4a},
    {0x5801, 0x2c},
    {0x5802, 0x23},
    {0x5803, 0x1f},
    {0x5804, 0x20},
    {0x5805, 0x23},
    {0x5806, 0x2f},
    {0x5807, 0x51},
    {0x5808, 0x1d},
    {0x5809, 0x19},
    {0x580a, 0x12},
    {0x580b, 0xf },
    {0x580c, 0xf },
    {0x580d, 0x13},
    {0x580e, 0x18},
    {0x580f, 0x20},
    {0x5810, 0x13},
    {0x5811, 0xd },
    {0x5812, 0x7 },
    {0x5813, 0x5 },
    {0x5814, 0x5 },
    {0x5815, 0x8 },
    {0x5816, 0xd },
    {0x5817, 0x16},
    {0x5818, 0xe },
    {0x5819, 0x8 },
    {0x581a, 0x2 },
    {0x581b, 0x0 },
    {0x581c, 0x0 },
    {0x581d, 0x3 },
    {0x581e, 0x8 },
    {0x581f, 0x10},
    {0x5820, 0xd },
    {0x5821, 0x8 },
    {0x5822, 0x2 },
    {0x5823, 0x0 },
    {0x5824, 0x0 },
    {0x5825, 0x3 },
    {0x5826, 0x8 },
    {0x5827, 0x10},
    {0x5828, 0x13},
    {0x5829, 0xd },
    {0x582a, 0x7 },
    {0x582b, 0x5 },
    {0x582c, 0x5 },
    {0x582d, 0x8 },
    {0x582e, 0xd },
    {0x582f, 0x15},
    {0x5830, 0x1d},
    {0x5831, 0x18},
    {0x5832, 0x12},
    {0x5833, 0xf },
    {0x5834, 0xf },
    {0x5835, 0x12},
    {0x5836, 0x17},
    {0x5837, 0x23},
    {0x5838, 0x49},
    {0x5839, 0x2e},
    {0x583a, 0x23},
    {0x583b, 0x1e},
    {0x583c, 0x1e},
    {0x583d, 0x21},
    {0x583e, 0x31},
    {0x583f, 0x52},
    {0x3212, 0x12}, 
    {0x3212, 0xa2}, 
};

struct ov5642_reg_t const ov5642_night_lens_c_reg_latch[] =
{
    {0x3200, 0x40},
    {0x3201, 0x78},
    {0x3202, 0xb0},
    {0x3203, 0xf0},
    {0x3212, 0x03},
    {0x5800, 0x0},
    {0x5801, 0x0},
    {0x5802, 0x0},
    {0x5803, 0x0},
    {0x5804, 0x0},
    {0x5805, 0x0},
    {0x5806, 0x0},
    {0x5807, 0x0},
    {0x5808, 0x0},
    {0x5809, 0x0},
    {0x580a, 0x0},
    {0x580b, 0x0},
    {0x580c, 0x0},
    {0x580d, 0x0},
    {0x580e, 0x0},
    {0x580f, 0x0},
    {0x5810, 0x0},
    {0x5811, 0x0},
    {0x5812, 0x0},
    {0x5813, 0x0},
    {0x5814, 0x0},
    {0x5815, 0x0},
    {0x5816, 0x0},
    {0x5817, 0x0},
    {0x5818, 0x0},
    {0x5819, 0x0},
    {0x581a, 0x0},
    {0x581b, 0x0},
    {0x581c, 0x0},
    {0x581d, 0x0},
    {0x581e, 0x0},
    {0x581f, 0x0},
    {0x5820, 0x0},
    {0x5821, 0x0},
    {0x5822, 0x0},
    {0x5823, 0x0},
    {0x5824, 0x0},
    {0x5825, 0x0},
    {0x5826, 0x0},
    {0x5827, 0x0},
    {0x5828, 0x0},
    {0x5829, 0x0},
    {0x582a, 0x0},
    {0x582b, 0x0},
    {0x582c, 0x0},
    {0x582d, 0x0},
    {0x582e, 0x0},
    {0x582f, 0x0},
    {0x5830, 0x0},
    {0x5831, 0x0},
    {0x5832, 0x0},
    {0x5833, 0x0},
    {0x5834, 0x0},
    {0x5835, 0x0},
    {0x5836, 0x0},
    {0x5837, 0x0},
    {0x5838, 0x0},
    {0x5839, 0x0},
    {0x583a, 0x0},
    {0x583b, 0x0},
    {0x583c, 0x0},
    {0x583d, 0x0},
    {0x583e, 0x0},
    {0x583f, 0x0},
    {0x3212, 0x13},
    {0x3212, 0xa3},
};

struct ov5642_reg_group_t const ov5642_lens_c_group_latch[] =
{
    { (struct ov5642_reg_t *)ov5642_general_lens_c_reg_latch, ARRAY_SIZE(ov5642_general_lens_c_reg_latch) },
    { (struct ov5642_reg_t *)ov5642_night_lens_c_reg_latch, ARRAY_SIZE(ov5642_night_lens_c_reg_latch) },
};
struct ov5642_reg_t const ov5642_auto_banding_reg[] =
{
    { OV5642_BIT_OPERATION_TAG, 0x80 },
    { 0x3C01, 0x00 },
    { OV5642_BIT_OPERATION_TAG, 0x04 },
    { 0x3C00, 0x00 },
};

struct ov5642_reg_t const ov5642_50Hz_banding_reg[] =
{
    { OV5642_BIT_OPERATION_TAG, 0x80 },
    { 0x3C01, 0x80 },
    { OV5642_BIT_OPERATION_TAG, 0x04 },
    { 0x3C00, 0x04 },
};

struct ov5642_reg_t const ov5642_60Hz_banding_reg[] =
{
    { OV5642_BIT_OPERATION_TAG, 0x80 },
    { 0x3C01, 0x80 },
    { OV5642_BIT_OPERATION_TAG, 0x04 },
    { 0x3C00, 0x00 },
};

struct ov5642_reg_group_t const ov5642_anti_banding_group[] =
{
    { (struct ov5642_reg_t *)ov5642_auto_banding_reg, ARRAY_SIZE(ov5642_auto_banding_reg) },
    { (struct ov5642_reg_t *)ov5642_50Hz_banding_reg, ARRAY_SIZE(ov5642_50Hz_banding_reg) },
    { (struct ov5642_reg_t *)ov5642_60Hz_banding_reg, ARRAY_SIZE(ov5642_60Hz_banding_reg) },
};

struct ov5642_reg_t const ov5642_brightness_P2_reg[] =
{
    { OV5642_BIT_OPERATION_TAG, 0x80 },
    { 0x5001, 0x80 },
    { 0x5589, 0x20 },
    { OV5642_BIT_OPERATION_TAG, 0x04 },
    { 0x5580, 0x04 },
    { OV5642_BIT_OPERATION_TAG, 0x08 },
    { 0x558A, 0x00 },
};

struct ov5642_reg_t const ov5642_brightness_P1_reg[] =
{
    { OV5642_BIT_OPERATION_TAG, 0x80 },
    { 0x5001, 0x80 },
    { 0x5589, 0x10 },
    { OV5642_BIT_OPERATION_TAG, 0x04 },
    { 0x5580, 0x04 },
    { OV5642_BIT_OPERATION_TAG, 0x08 },
    { 0x558A, 0x00 },
};

struct ov5642_reg_t const ov5642_brightness_P0_reg[] =
{
    { OV5642_BIT_OPERATION_TAG, 0x80 },
    { 0x5001, 0x80 },
    { 0x5589, 0x00 },
    { OV5642_BIT_OPERATION_TAG, 0x04 },
    { 0x5580, 0x04 },
    { OV5642_BIT_OPERATION_TAG, 0x08 },
    { 0x558A, 0x00 },
};

struct ov5642_reg_t const ov5642_brightness_N1_reg[] =
{
    { OV5642_BIT_OPERATION_TAG, 0x80 },
    { 0x5001, 0x80 },
    { 0x5589, 0x10 },
    { OV5642_BIT_OPERATION_TAG, 0x04 },
    { 0x5580, 0x04 },
    { OV5642_BIT_OPERATION_TAG, 0x08 },
    { 0x558A, 0x08 },
};

struct ov5642_reg_t const ov5642_brightness_N2_reg[] =
{
    { OV5642_BIT_OPERATION_TAG, 0x80 },
    { 0x5001, 0x80 },
    { 0x5589, 0x20 },
    { OV5642_BIT_OPERATION_TAG, 0x04 },
    { 0x5580, 0x04 },
    { OV5642_BIT_OPERATION_TAG, 0x08 },
    { 0x558A, 0x08 },
};

struct ov5642_reg_group_t const ov5642_brightness_group[] =
{
    { (struct ov5642_reg_t *)ov5642_brightness_P2_reg, ARRAY_SIZE(ov5642_brightness_P2_reg) },
    { (struct ov5642_reg_t *)ov5642_brightness_P1_reg, ARRAY_SIZE(ov5642_brightness_P1_reg) },
    { (struct ov5642_reg_t *)ov5642_brightness_P0_reg, ARRAY_SIZE(ov5642_brightness_P0_reg) },
    { (struct ov5642_reg_t *)ov5642_brightness_N1_reg, ARRAY_SIZE(ov5642_brightness_N1_reg) },
    { (struct ov5642_reg_t *)ov5642_brightness_N2_reg, ARRAY_SIZE(ov5642_brightness_N2_reg) },
};

struct ov5642_reg_t const ov5642_contrast_P2_reg[] =
{
    { OV5642_BIT_OPERATION_TAG, 0x80 },
    { 0x5001, 0x80 },
    { OV5642_BIT_OPERATION_TAG, 0x04 },
    { 0x5580, 0x04 },
    { 0x5587, 0x28 },
    { 0x5588, 0x28 },
    { OV5642_BIT_OPERATION_TAG, 0x04 },
    { 0x558A, 0x00 },
};

struct ov5642_reg_t const ov5642_contrast_P1_reg[] =
{
    { OV5642_BIT_OPERATION_TAG, 0x80 },
    { 0x5001, 0x80 },
    { OV5642_BIT_OPERATION_TAG, 0x04 },
    { 0x5580, 0x04 },
    { 0x5587, 0x24 },
    { 0x5588, 0x24 },
    { OV5642_BIT_OPERATION_TAG, 0x04 },
    { 0x558A, 0x00 },
};

struct ov5642_reg_t const ov5642_contrast_P0_reg[] =
{
    { OV5642_BIT_OPERATION_TAG, 0x80 },
    { 0x5001, 0x80 },
    { OV5642_BIT_OPERATION_TAG, 0x04 },
    { 0x5580, 0x04 },
    { 0x5587, 0x20 },
    { 0x5588, 0x20 },
    { OV5642_BIT_OPERATION_TAG, 0x04 },
    { 0x558A, 0x00 },
};

struct ov5642_reg_t const ov5642_contrast_N1_reg[] =
{
    { OV5642_BIT_OPERATION_TAG, 0x80 },
    { 0x5001, 0x80 },
    { OV5642_BIT_OPERATION_TAG, 0x04 },
    { 0x5580, 0x04 },
    { 0x5587, 0x1C },
    { 0x5588, 0x1C },
    { OV5642_BIT_OPERATION_TAG, 0x04 },
    { 0x558A, 0x00 },
};

struct ov5642_reg_t const ov5642_contrast_N2_reg[] =
{
    { OV5642_BIT_OPERATION_TAG, 0x80 },
    { 0x5001, 0x80 },
    { OV5642_BIT_OPERATION_TAG, 0x04 },
    { 0x5580, 0x04 },
    { 0x5587, 0x18 },
    { 0x5588, 0x18 },
    { OV5642_BIT_OPERATION_TAG, 0x04 },
    { 0x558A, 0x00 },
};

struct ov5642_reg_group_t const ov5642_contrast_group[] =
{
    { (struct ov5642_reg_t *)ov5642_contrast_P2_reg, ARRAY_SIZE(ov5642_contrast_P2_reg) },
    { (struct ov5642_reg_t *)ov5642_contrast_P1_reg, ARRAY_SIZE(ov5642_contrast_P1_reg) },
    { (struct ov5642_reg_t *)ov5642_contrast_P0_reg, ARRAY_SIZE(ov5642_contrast_P0_reg) },
    { (struct ov5642_reg_t *)ov5642_contrast_N1_reg, ARRAY_SIZE(ov5642_contrast_N1_reg) },
    { (struct ov5642_reg_t *)ov5642_contrast_N2_reg, ARRAY_SIZE(ov5642_contrast_N2_reg) },
};

struct ov5642_reg_t const ov5642_auto_wb_reg[] =
{
    { OV5642_BIT_OPERATION_TAG, 0x01 },
    { 0x3406, 0x00 },
};

struct ov5642_reg_t const ov5642_sun_light_reg[] =
{
    { OV5642_BIT_OPERATION_TAG, 0x01 },
    { 0x3406, 0x01 }, 
    { 0x3400, 0x5  }, 
    { 0x3401, 0x80 }, 
    { 0x3402, 0x4  }, 
    { 0x3403, 0x0  },
    { 0x3404, 0x5  }, 
    { 0x3405, 0x38 }, 
};

struct ov5642_reg_t const ov5642_fluorescent_reg[] =
{
    { OV5642_BIT_OPERATION_TAG, 0x01 },
    { 0x3406, 0x01 }, 
    { 0x3400, 0x3  }, 
    { 0x3401, 0xb2 }, 
    { 0x3402, 0x3  }, 
    { 0x3403, 0x0  },
    { 0x3404, 0x4  }, 
    { 0x3405, 0xf0 }, 
};

struct ov5642_reg_t const ov5642_incandscent_reg[] =
{
    { OV5642_BIT_OPERATION_TAG, 0x01 },
    { 0x3406, 0x01 }, 
    { 0x3400, 0x2  }, 
    { 0x3401, 0xc4 }, 
    { 0x3402, 0x3  }, 
    { 0x3403, 0x0  },
    { 0x3404, 0x6  }, 
    { 0x3405, 0xc8 }, 
};

struct ov5642_reg_group_t const ov5642_white_balance_group[] =
{
    { (struct ov5642_reg_t *)ov5642_auto_wb_reg, ARRAY_SIZE(ov5642_auto_wb_reg) },
    { (struct ov5642_reg_t *)ov5642_sun_light_reg, ARRAY_SIZE(ov5642_sun_light_reg) },
    { (struct ov5642_reg_t *)ov5642_fluorescent_reg, ARRAY_SIZE(ov5642_fluorescent_reg) },
    { (struct ov5642_reg_t *)ov5642_incandscent_reg, ARRAY_SIZE(ov5642_incandscent_reg) },
};

struct ov5642_reg_t const ov5642_quality_80_reg[] =
{
    { 0x4407, 0x06 },
};

struct ov5642_reg_t const ov5642_quality_70_reg[] =
{
    { 0x4407, 0x09 },
};

struct ov5642_reg_t const ov5642_quality_60_reg[] =
{
    { 0x4407, 0x0C },
};

struct ov5642_reg_group_t const ov5642_jpeg_quality_group[] =
{
    { (struct ov5642_reg_t *)ov5642_quality_80_reg, ARRAY_SIZE(ov5642_quality_80_reg) },
    { (struct ov5642_reg_t *)ov5642_quality_70_reg, ARRAY_SIZE(ov5642_quality_70_reg) },
    { (struct ov5642_reg_t *)ov5642_quality_60_reg, ARRAY_SIZE(ov5642_quality_60_reg) },
};

struct ov5642_reg_t const ov5642_general_target_reg[] =
{
    { 0x3a0f, 0x2c }, 
    { 0x3a10, 0x24 }, 
    { 0x3a1b, 0x2c }, 
    { 0x3a1e, 0x24 }, 
    { 0x3a11, 0x61 }, 
    { 0x3a1f, 0x10 }, 
};

struct ov5642_reg_t const ov5642_night_target_n1p0ev_reg[] =
{
    { 0x3a0f, 0x18 }, 
    { 0x3a10, 0x10 }, 
    { 0x3a1b, 0x18 }, 
    { 0x3a1e, 0x10 }, 
    { 0x3a11, 0x30 }, 
    { 0x3a1f, 0x10 }, 
};

struct ov5642_reg_t const ov5642_night_target_n1p3ev_reg[] =
{
    { 0x3a0f, 0x10 }, 
    { 0x3a10, 0x08 }, 
    { 0x3a1b, 0x10 }, 
    { 0x3a1e, 0x08 }, 
    { 0x3a11, 0x20 }, 
    { 0x3a1f, 0x10 }, 
};

struct ov5642_reg_group_t const ov5642_ae_target_group[] =
{
    { (struct ov5642_reg_t *)ov5642_general_target_reg, ARRAY_SIZE(ov5642_general_target_reg) },
    { (struct ov5642_reg_t *)ov5642_night_target_n1p0ev_reg, ARRAY_SIZE(ov5642_night_target_n1p0ev_reg) },
    { (struct ov5642_reg_t *)ov5642_night_target_n1p3ev_reg, ARRAY_SIZE(ov5642_night_target_n1p3ev_reg) },
};

char const ov5642_general_gamma_reg[] =
{
    0x54, 
    0x80,
    0x08,
    0x19, 
    0x2f, 
    0x4d, 
    0x5b, 
    0x64, 
    0x6f, 
    0x79, 
    0x82, 
    0x8a, 
    0x96, 
    0xa1, 
    0xb3, 
    0xc7, 
    0xdb, 
    0x31, 
    0x5 , 
    0x0 , 
    0x4 , 
    0x20, 
    0x3 , 
    0x60, 
    0x2 , 
    0xb8, 
    0x2 , 
    0x86, 
    0x2 , 
    0x5b, 
    0x2 , 
    0x3b, 
    0x2 , 
    0x1c, 
    0x2 , 
    0x4 , 
    0x1 , 
    0xed, 
    0x1 , 
    0xc5, 
    0x1 , 
    0xa5, 
    0x1 , 
    0x6c, 
    0x1 , 
    0x41, 
    0x1 , 
    0x20, 
    0x0 , 
    0x16, 
    0x1 , 
    0x20, 
    0x1 , 
    0x40, 
    0x0 , 
    0xf0, 
    0x1 , 
    0xdf, 
};

char const ov5642_night_gamma_reg[] =
{
    0x54, 
    0x80,
    0x2 ,
    0xd , 
    0x1a, 
    0x32, 
    0x3b, 
    0x41, 
    0x4a, 
    0x52, 
    0x5a, 
    0x63, 
    0x72, 
    0x84, 
    0xa3, 
    0xbc, 
    0xd4, 
    0x3a, 
    0x5 , 
    0x0 , 
    0x4 , 
    0x20, 
    0x3 , 
    0x60, 
    0x2 , 
    0xb8, 
    0x2 , 
    0x86, 
    0x2 , 
    0x5b, 
    0x2 , 
    0x3b, 
    0x2 , 
    0x1c, 
    0x2 , 
    0x4 , 
    0x1 , 
    0xed, 
    0x1 , 
    0xc5, 
    0x1 , 
    0xa5, 
    0x1 , 
    0x6c, 
    0x1 , 
    0x41, 
    0x1 , 
    0x20, 
    0x0 , 
    0x16, 
    0x1 , 
    0x20, 
    0x1 , 
    0x40, 
    0x0 , 
    0xf0, 
    0x1 , 
    0xdf, 
};

struct ov5642_reg_burst_group_t const ov5642_gamma_group[] =
{
    { (char *)ov5642_general_gamma_reg, ARRAY_SIZE(ov5642_general_gamma_reg) },
    { (char *)ov5642_night_gamma_reg, ARRAY_SIZE(ov5642_night_gamma_reg) },
};

struct ov5642_reg_t const ov5642_general_gamma_reg_latch[] =
{
    {0x3200, 0x40},
    {0x3201, 0x78},
    {0x3202, 0xb0},
    {0x3203, 0xf0},
    {0x3212, 0x00},
    {0x5480, 0x8 },
    {0x5481, 0x19},
    {0x5482, 0x2f},
    {0x5483, 0x4d},
    {0x5484, 0x5b},
    {0x5485, 0x64},
    {0x5486, 0x6f},
    {0x5487, 0x79},
    {0x5488, 0x82},
    {0x5489, 0x8a},
    {0x548a, 0x96},
    {0x548b, 0xa1},
    {0x548c, 0xb3},
    {0x548d, 0xc7},
    {0x548e, 0xdb},
    {0x548f, 0x31},
    {0x5490, 0x5 },
    {0x5491, 0x0 },
    {0x5492, 0x4 },
    {0x5493, 0x20},
    {0x5494, 0x3 },
    {0x5495, 0x60},
    {0x5496, 0x2 },
    {0x5497, 0xb8},
    {0x5498, 0x2 },
    {0x5499, 0x86},
    {0x549a, 0x2 },
    {0x549b, 0x5b},
    {0x549c, 0x2 },
    {0x549d, 0x3b},
    {0x549e, 0x2 },
    {0x549f, 0x1c},
    {0x54a0, 0x2 },
    {0x54a1, 0x4 },
    {0x54a2, 0x1 },
    {0x54a3, 0xed},
    {0x54a4, 0x1 },
    {0x54a5, 0xc5},
    {0x54a6, 0x1 },
    {0x54a7, 0xa5},
    {0x54a8, 0x1 },
    {0x54a9, 0x6c},
    {0x54aa, 0x1 },
    {0x54ab, 0x41},
    {0x54ac, 0x1 },
    {0x54ad, 0x20},
    {0x54ae, 0x0 },
    {0x54af, 0x16},
    {0x54b0, 0x1 },
    {0x54b1, 0x20},
    {0x54b2, 0x1 },
    {0x54b3, 0x40},
    {0x54b4, 0x0 },
    {0x54b5, 0xf0},
    {0x54b6, 0x1 },
    {0x54b7, 0xdf},
    {0x3212, 0x10},
    {0x3212, 0xa0},
};

struct ov5642_reg_t const ov5642_night_gamma_reg_latch[] =
{
    {0x3200, 0x40},
    {0x3201, 0x78},
    {0x3202, 0xb0},
    {0x3203, 0xf0},
    {0x3212, 0x01},
    {0x5480, 0x2 },
    {0x5481, 0xd },
    {0x5482, 0x1a},
    {0x5483, 0x32},
    {0x5484, 0x3b},
    {0x5485, 0x41},
    {0x5486, 0x4a},
    {0x5487, 0x52},
    {0x5488, 0x5a},
    {0x5489, 0x63},
    {0x548a, 0x72},
    {0x548b, 0x84},
    {0x548c, 0xa3},
    {0x548d, 0xbc},
    {0x548e, 0xd4},
    {0x548f, 0x3a},
    {0x5490, 0x5 },
    {0x5491, 0x0 },
    {0x5492, 0x4 },
    {0x5493, 0x20},
    {0x5494, 0x3 },
    {0x5495, 0x60},
    {0x5496, 0x2 },
    {0x5497, 0xb8},
    {0x5498, 0x2 },
    {0x5499, 0x86},
    {0x549a, 0x2 },
    {0x549b, 0x5b},
    {0x549c, 0x2 },
    {0x549d, 0x3b},
    {0x549e, 0x2 },
    {0x549f, 0x1c},
    {0x54a0, 0x2 },
    {0x54a1, 0x4 },
    {0x54a2, 0x1 },
    {0x54a3, 0xed},
    {0x54a4, 0x1 },
    {0x54a5, 0xc5},
    {0x54a6, 0x1 },
    {0x54a7, 0xa5},
    {0x54a8, 0x1 },
    {0x54a9, 0x6c},
    {0x54aa, 0x1 },
    {0x54ab, 0x41},
    {0x54ac, 0x1 },
    {0x54ad, 0x20},
    {0x54ae, 0x0 },
    {0x54af, 0x16},
    {0x54b0, 0x1 },
    {0x54b1, 0x20},
    {0x54b2, 0x1 },
    {0x54b3, 0x40},
    {0x54b4, 0x0 },
    {0x54b5, 0xf0},
    {0x54b6, 0x1 },
    {0x54b7, 0xdf},
    {0x3212, 0x11},
    {0x3212, 0xa1},
};

struct ov5642_reg_group_t const ov5642_gamma_group_latch[] =
{
    { (struct ov5642_reg_t *)ov5642_general_gamma_reg_latch, ARRAY_SIZE(ov5642_general_gamma_reg_latch) },
    { (struct ov5642_reg_t *)ov5642_night_gamma_reg_latch, ARRAY_SIZE(ov5642_night_gamma_reg_latch) },
};

struct ov5642_reg_t const ov5642_denoise_auto_def_reg[] =
{
    { 0x528a, 0x02 }, 
    { 0x528b, 0x06 }, 
    { 0x528c, 0x20 }, 
    { 0x528d, 0x30 }, 
    { 0x528e, 0x40 }, 
    { 0x528f, 0x50 }, 
    { 0x5290, 0x60 }, 

    { 0x5292, 0x00 }, 
    { 0x5293, 0x02 }, 
    { 0x5294, 0x00 }, 
    { 0x5295, 0x04 }, 
    { 0x5296, 0x00 }, 
    { 0x5297, 0x08 }, 
    { 0x5298, 0x00 }, 
    { 0x5299, 0x10 }, 
    { 0x529a, 0x00 }, 
    { 0x529b, 0x20 }, 
    { 0x529c, 0x00 }, 
    { 0x529d, 0x28 }, 
    { 0x529e, 0x00 }, 
    { 0x529f, 0x30 }, 
    { 0x5282, 0x0  },
};

struct ov5642_reg_t const ov5642_denoise_auto_p1_reg[] =
{
    { 0x528a, 0x04 },
    { 0x528b, 0x08 },
    { 0x528c, 0x10 },
    { 0x528d, 0x18 },
    { 0x528e, 0x28 },
    { 0x528f, 0x50 },
    { 0x5290, 0x60 },
    { 0x5292, 0x0  },
    { 0x5293, 0x8  },
    { 0x5294, 0x0  },
    { 0x5295, 0x10 },
    { 0x5296, 0x0  },
    { 0x5297, 0x20 },
    { 0x5298, 0x0  },
    { 0x5299, 0x30 },
    { 0x529a, 0x0  },
    { 0x529b, 0xc0 },
    { 0x529c, 0x0  },
    { 0x529d, 0xe0 },
    { 0x529e, 0x1  },
    { 0x529f, 0x10 },
    { 0x5282, 0x0  },
};

struct ov5642_reg_group_t const ov5642_denoise_group[] =
{
    { (struct ov5642_reg_t *)ov5642_denoise_auto_def_reg, ARRAY_SIZE(ov5642_denoise_auto_def_reg) },
    { (struct ov5642_reg_t *)ov5642_denoise_auto_p1_reg, ARRAY_SIZE(ov5642_denoise_auto_p1_reg) },
};

struct ov5642_reg_t const ov5642_sharpness_auto_p3_reg[] =
{
    { OV5642_BIT_OPERATION_TAG, 0x08 },
    { 0x530a, 0x00 },
    { 0x530c, 0x10 },
    { 0x530d, 0x38 },
    { 0x5312, 0x08 },
};

struct ov5642_reg_t const ov5642_sharpness_auto_def_reg[] =
{
    { OV5642_BIT_OPERATION_TAG, 0x08 },
    { 0x530a, 0x00 },
    { 0x530c, 0x00 },
    { 0x530d, 0x0c },
    { 0x5312, 0x40 },
};

struct ov5642_reg_group_t const ov5642_sharpness_group[] =
{
    { (struct ov5642_reg_t *)ov5642_sharpness_auto_p3_reg, ARRAY_SIZE(ov5642_sharpness_auto_p3_reg) },
    { (struct ov5642_reg_t *)ov5642_sharpness_auto_def_reg, ARRAY_SIZE(ov5642_sharpness_auto_def_reg) },
};

struct ov5642_special_reg_groups ov5642_special_regs =
{
    .frame_rate_group    = ov5642_frame_rate_group,
    .ae_weighting_group  = ov5642_ae_weighting_group,
    .color_matrix_group  = ov5642_color_matrix_group,
    .anti_banding_group  = ov5642_anti_banding_group,
    .brightness_group    = ov5642_brightness_group,
    .contrast_group      = ov5642_contrast_group,
    .white_balance_group = ov5642_white_balance_group,
    .jpeg_quality_group  = ov5642_jpeg_quality_group,
    .ae_target_group     = ov5642_ae_target_group,
    .lens_c_group        = ov5642_lens_c_group,
    .gamma_group         = ov5642_gamma_group,
    .denoise_group       = ov5642_denoise_group,
    .sharpness_group     = ov5642_sharpness_group,
};

#define UNKNOWN_MODE_STATE 0
#define NORMAL_MODE_STATE  1
#define NIGHT_MODE_STATE   2

#define UNKNOWN_EXPOSURE_STATE 0
#define NORMAL_EXPOSURE_STATE  1
#define NIGHT_EXPOSURE_STATE   2

static struct delayed_work auto_night_mode_work;
static int night_mode_state;
static int night_mode_exposure;
static char auto_night_mode_flag;
static char auto_night_mode_active;
DECLARE_MUTEX(ov5642_night_mode_sem);

static void ov5642_detect_night_mode(int *pState, int *pExposure)
{
    struct ov5642_reg_t night[] =
    {
        { 0x350c, 0x00 },
        { 0x350d, 0x00 },
        { 0x3501, 0x00 },
        { 0x3502, 0x00 },
    };
    int vts = 0;
    int exposure = 0;

    if( ov5642_i2c_read(night, ARRAY_SIZE(night)) ) {
        *pState = UNKNOWN_MODE_STATE;
        *pExposure = UNKNOWN_EXPOSURE_STATE;
        return;
    }else {
        vts = (night[0].data << 8) | (night[1].data);
        exposure = (night[2].data << 8) | (night[3].data);
    }

    if( vts > 0x3f0 )
        *pState = NIGHT_MODE_STATE;
    else
        *pState = NORMAL_MODE_STATE;

    if( exposure > 0xb000 )
        *pExposure = NIGHT_EXPOSURE_STATE;
    else
        *pExposure = NORMAL_EXPOSURE_STATE;

    return;
}

static long ov5642_config_night_gamma(int state)
{
    long rc = 0;

    enum ov5642_gamma_t gamma;
#ifndef USE_GROUP_LATCH
    struct ov5642_reg_burst_group_t const *pSpecRegBurstGroup;
#else
    struct ov5642_reg_group_t const *pSpecRegGroup;
#endif

    if( NIGHT_EXPOSURE_STATE == state ) {
        gamma = NIGHT_GAMMA;
    }else {
        gamma = GENERAL_GAMMA;
    }

#ifndef USE_GROUP_LATCH
    pSpecRegBurstGroup = ov5642_special_regs.gamma_group;
    rc = ov5642_i2c_burst_write( (pSpecRegBurstGroup+gamma)->start,
                                 (pSpecRegBurstGroup+gamma)->len );
#else
    pSpecRegGroup = ov5642_gamma_group_latch;
    rc = ov5642_i2c_write( (pSpecRegGroup+gamma)->start,
                           (pSpecRegGroup+gamma)->len );
#endif
    return rc;
};

static long ov5642_verify_gamma(int state)
{
    long rc = 0;
    int i;
    struct ov5642_reg_t reg;
    struct ov5642_reg_t *regs;

    enum ov5642_gamma_t gamma;
    struct ov5642_reg_group_t const *pSpecRegGroup;
    struct ov5642_reg_burst_group_t const *pSpecRegBurstGroup;

    if (NIGHT_EXPOSURE_STATE == state) {
        gamma = NIGHT_GAMMA;
    } else {
        gamma = GENERAL_GAMMA;
    }

    pSpecRegBurstGroup = ov5642_special_regs.gamma_group;
    pSpecRegGroup = ov5642_gamma_group_latch;
    regs = (pSpecRegGroup+gamma)->start;
    for (i = 0; i < (pSpecRegGroup+gamma)->len; i++) {
        if (regs->addr < 0x5480) {
            regs++;
            continue;
        }

        reg.addr = regs->addr;
        if (ov5642_i2c_read(&reg, 1)) {
            rc |= -EIO;
            break;
        }
       
        if (reg.data != regs->data) {
            CSDBG("%s: reconfig since %04xh=0x%02x\n", __func__, reg.addr, reg.data);
            rc |= -EIO;
        }
        regs++; 
    }
    if (rc)
        rc = ov5642_i2c_burst_write( (pSpecRegBurstGroup+gamma)->start,
                                     (pSpecRegBurstGroup+gamma)->len );
    return rc;
};

static long ov5642_config_night_mode(int state)
{
    long rc = 0;

    enum ov5642_lens_c_t lensCorrection;
    enum ov5642_denoise_t denoise;
    enum ov5642_sharpness_t sharpness;
#ifndef USE_GROUP_LATCH
    struct ov5642_reg_burst_group_t const *pSpecRegBurstGroup;
#else
    struct ov5642_reg_group_t const *pSpecRegGroup;
#endif

    if( NIGHT_MODE_STATE == state ) {
        lensCorrection = NIGHT_LENS_C;
        denoise = DENOISE_AUTO_P1;
        sharpness = SHARPNESS_AUTO_DEFAULT;
    }else {
        lensCorrection = GENERAL_LENS_C;
        denoise = DENOISE_AUTO_DEFAULT;
        sharpness = SHARPNESS_AUTO_DEFAULT;
    }

    pSpecRegGroup = ov5642_special_regs.denoise_group;
    rc = ov5642_i2c_write( (pSpecRegGroup+denoise)->start,
                           (pSpecRegGroup+denoise)->len );

    if( rc )
        return rc;

    pSpecRegGroup = ov5642_special_regs.sharpness_group;
    rc = ov5642_i2c_write( (pSpecRegGroup+sharpness)->start,
                           (pSpecRegGroup+sharpness)->len );
    if( rc )
        return rc;

#ifndef USE_GROUP_LATCH
    pSpecRegBurstGroup = ov5642_special_regs.lens_c_group;
    rc = ov5642_i2c_burst_write( (pSpecRegBurstGroup+lensCorrection)->start,
                                 (pSpecRegBurstGroup+lensCorrection)->len );
#else
    pSpecRegGroup = ov5642_lens_c_group_latch;
    rc = ov5642_i2c_write( (pSpecRegGroup+lensCorrection)->start,
                           (pSpecRegGroup+lensCorrection)->len );
#endif
    return rc;
};

static long ov5642_verify_lens_c(int state)
{
    long rc = 0;
    int i;
    struct ov5642_reg_t reg;
    struct ov5642_reg_t *regs;

    enum ov5642_lens_c_t lensCorrection;
    struct ov5642_reg_group_t const *pSpecRegGroup;
    struct ov5642_reg_burst_group_t const *pSpecRegBurstGroup;

    if( NIGHT_MODE_STATE == state ) {
        lensCorrection = NIGHT_LENS_C;
    }else {
        lensCorrection = GENERAL_LENS_C;
    }

    pSpecRegBurstGroup = ov5642_special_regs.lens_c_group;
    pSpecRegGroup = ov5642_lens_c_group_latch;
    regs = (pSpecRegGroup+lensCorrection)->start;
    for (i = 0; i < (pSpecRegGroup+lensCorrection)->len; i++) {
        if (regs->addr < 0x5800) {
            regs++;
            continue;
        }

        reg.addr = regs->addr;
        if (ov5642_i2c_read(&reg, 1)) {
            rc |= -EIO;
            break;
        }
       
        if (reg.data != regs->data) {
            CSDBG("%s: reconfig since %04xh=0x%02x\n", __func__, reg.addr, reg.data);
            rc |= -EIO;
        }
        regs++; 
    }

    if (rc)
        rc = ov5642_i2c_burst_write( (pSpecRegBurstGroup+lensCorrection)->start,
                                     (pSpecRegBurstGroup+lensCorrection)->len );
    return rc;
};

static void ov5642_set_night_mode(char on)
{
    struct ov5642_reg_t night_mode[] = {
               { OV5642_BIT_OPERATION_TAG, 0x04 },
               { 0x3a00, 0x00 }
               };

    if( on )
        night_mode[1].data = 0x04;

    ov5642_i2c_write( night_mode,
                      ARRAY_SIZE(night_mode) );

    return;
}

static void ov5642_night_mode_work_func(struct work_struct *work)
{
    static bool verifyGamma = false;
    static bool verifyLensC = false;
    int new_state, new_exposure;
    int old_state, old_exposure;
    long changeMode, changeGamma, rc;
    long delayMs = 2000;

    down(&ov5642_night_mode_sem);
    rc = auto_night_mode_flag;
    old_state = night_mode_state;
    old_exposure = night_mode_exposure;
    up(&ov5642_night_mode_sem);

    if( !rc )
    {
        CSDBG("%s: terminated, state is %d, exposure is %d\n",
              __func__, old_state, old_exposure);
        return;
    }

    ov5642_detect_night_mode(&new_state, &new_exposure);
    if (UNKNOWN_MODE_STATE == new_state) {
        verifyLensC = false;
        changeMode = true;
    } else {
        changeMode = (new_state != old_state);
    }
    if (UNKNOWN_EXPOSURE_STATE == new_exposure) {
        verifyGamma = false;
        changeGamma = true;
    } else {
        changeGamma = (new_exposure != old_exposure);
    }

    if (verifyGamma) {
        CSDBG("%s: verify gamma %d\n", __func__, old_exposure);
        rc = ov5642_verify_gamma(old_exposure);
        if (rc)
            delayMs = 500;
        else
            verifyGamma = false;
    } else if (changeGamma) {
        CSDBG("%s: exposure %d -> %d\n", __func__, old_exposure, new_exposure);
        rc = ov5642_config_night_gamma(new_exposure);
        down(&ov5642_night_mode_sem);
        night_mode_exposure = rc? UNKNOWN_EXPOSURE_STATE : new_exposure;
        up(&ov5642_night_mode_sem);
#ifdef USE_GROUP_LATCH
        if (!rc) {
            verifyGamma = true;
            delayMs = 500;
            changeMode = false;
        }
#endif
    } 

    if (verifyLensC) {
        CSDBG("%s: verify lensc %d\n", __func__, old_state);
        rc = ov5642_verify_lens_c(old_state);
        if (rc)
            delayMs = 500;
        else
            verifyLensC = false;
    } else if (changeMode) {
        CSDBG("%s: state %d -> %d\n", __func__, old_state, new_state);
        rc = ov5642_config_night_mode(new_state);
        down(&ov5642_night_mode_sem);
        night_mode_state = rc? UNKNOWN_MODE_STATE : new_state;
        up(&ov5642_night_mode_sem);
#ifdef USE_GROUP_LATCH
        if (!rc) {
            verifyLensC = true;
            delayMs = 500;
        }
#endif
    } 

    schedule_delayed_work(&auto_night_mode_work, delayMs*HZ/1000);
}

static void ov5642_query_night_mode(int *pSharpness)
{
    int state;

    down(&ov5642_night_mode_sem);
    state = night_mode_state;
    up(&ov5642_night_mode_sem);

    if( NIGHT_MODE_STATE == state )
        *pSharpness = SHARPNESS_AUTO_DEFAULT;
    else
        *pSharpness = SHARPNESS_AUTO_P3;

    return;
}

static void ov5642_disable_night_mode(void)
{
    ov5642_set_night_mode(0);
    return;
}

static void ov5642_convert_night_mode(int scene, int second)
{
    down(&ov5642_night_mode_sem);
    auto_night_mode_flag = (CAMERA_BESTSHOT_OFF == scene)? 1: 0;
    if( auto_night_mode_active && auto_night_mode_flag )
    {
        up(&ov5642_night_mode_sem);
        CDBG("%s: night mode work has already run\n", __func__);
        return;
    }
    up(&ov5642_night_mode_sem);

    if( CAMERA_BESTSHOT_SPORTS != scene && CAMERA_BESTSHOT_SNOW != scene )
    {
        ov5642_set_night_mode(1);
        if (auto_night_mode_flag) {
            CDBG("%s: schedule night mode work\n", __func__);
            down(&ov5642_night_mode_sem);
            auto_night_mode_active = 1;
            up(&ov5642_night_mode_sem);
            schedule_delayed_work(&auto_night_mode_work, second*HZ);
        }
    }

    return;
}

static void ov5642_init_night_mode(void)
{
    night_mode_state = UNKNOWN_MODE_STATE;
    night_mode_exposure = UNKNOWN_EXPOSURE_STATE;
    auto_night_mode_flag = 0;
    auto_night_mode_active = 0;
    INIT_DELAYED_WORK(&auto_night_mode_work, ov5642_night_mode_work_func);
    return;
}

static void ov5642_finish_night_mode(char flushed)
{
    down(&ov5642_night_mode_sem);
    auto_night_mode_flag = 0;
    if( !auto_night_mode_active )
        goto skip_finish_night_work;

    up(&ov5642_night_mode_sem);
    if( cancel_delayed_work_sync(&auto_night_mode_work) )
        CDBG("%s: work has been canceled\n", __func__);
    else
        CDBG("%s: work has already terminated\n", __func__);
    down(&ov5642_night_mode_sem);

skip_finish_night_work:
    if( flushed )
    {
        night_mode_state = UNKNOWN_MODE_STATE;
        night_mode_exposure = UNKNOWN_EXPOSURE_STATE;
    }
    auto_night_mode_active = 0;
    up(&ov5642_night_mode_sem);

    return;
}

struct ov5642_night_mode_func_array ov5642_night_mode_func =
{
    .init_night_mode = ov5642_init_night_mode,
    .convert_night_mode = ov5642_convert_night_mode,
    .finish_night_mode = ov5642_finish_night_mode,
    .disable_night_mode = ov5642_disable_night_mode,
    .query_night_mode = ov5642_query_night_mode,
};

static struct delayed_work awb_work;
static char awb_work_active;
DECLARE_MUTEX(ov5642_awb_sem);

static long ov5642_is_simple_awb(void)
{
    long rc;
    struct ov5642_reg_t awb = { 0x5183, 0x00 };

    rc = ov5642_i2c_read( &awb,
                          1 );
    if( !rc )
        rc = awb.data & 0x80;

    return rc;
}

static long ov5642_simple_awb(int enabled)
{
    if( enabled ) {
        struct ov5642_reg_t awb[] = {
            { 0x5191, 0xff },
            { 0x5192, 0x00 },
            { 0x5183, 0x94 } };

        return ov5642_i2c_write( awb,
                                 ARRAY_SIZE(awb) );
    }
    else {
        struct ov5642_reg_t awb[] = {
            { 0x5191, 0xf8 },
            { 0x5192, 0x04 },
            { 0x5183, 0x14 } };

        return ov5642_i2c_write( awb,
                                 ARRAY_SIZE(awb) );
    }
}

static void ov5642_awb_work_func(struct work_struct *work)
{
    CDBG("%s()\n", __func__);
    ov5642_simple_awb(0);
}

static void ov5642_convert_awb(void)
{
    if( ov5642_is_simple_awb() )
    {
        ov5642_simple_awb(1);
        down(&ov5642_awb_sem);
        awb_work_active = 1;
        up(&ov5642_awb_sem);
        schedule_delayed_work(&awb_work, HZ);
    }
    return;
}

static void ov5642_init_awb(void)
{
    awb_work_active = 0;
    ov5642_simple_awb(1);
    INIT_DELAYED_WORK(&awb_work, ov5642_awb_work_func);
    return;
}

static void ov5642_finish_awb(char flushed)
{
    down(&ov5642_awb_sem);
    if( !awb_work_active )
        goto skip_finish_awb;

    up(&ov5642_awb_sem);
    if( flushed ) {
        flush_work(&awb_work.work);
    }else {
        if(  cancel_delayed_work_sync(&awb_work) )
            CDBG("%s: work has been canceled\n", __func__);
        else
            CDBG("%s: work has already terminated\n", __func__);
    }
    down(&ov5642_awb_sem);

skip_finish_awb:
    awb_work_active = 0;
    up(&ov5642_awb_sem);

    return;
}

struct ov5642_awb_func_array ov5642_awb_func =
{
    .init_awb = ov5642_init_awb,
    .convert_awb = ov5642_convert_awb,
    .finish_awb = ov5642_finish_awb,
};

#ifdef USE_AF

static char doAutoFocus;
static wait_queue_head_t waitAutoFocus;
static enum ov5642_af_status_t afStatus;
static struct work_struct af_download_work;
DECLARE_MUTEX(ov5642_af_sem);

static long ov5642_enable_AF(void)
{
    return ov5642_i2c_write( (struct ov5642_reg_t *)ov5642_enable_af,
                             (sizeof(ov5642_enable_af)/sizeof(ov5642_enable_af[0])) );
}

static long ov5642_reset_AF(void)
{
    return ov5642_i2c_write( (struct ov5642_reg_t *)ov5642_reset_af,
                             ARRAY_SIZE(ov5642_reset_af) );
}

static long ov5642_start_single_AF(void)
{
    return ov5642_i2c_write( (struct ov5642_reg_t *)ov5642_single_af,
                             ARRAY_SIZE(ov5642_single_af) );
}

static long ov5642_load_AF(void)
{
    long rc;
    int i;
    uint32_t writeCnt;

    rc = ov5642_i2c_write( (struct ov5642_reg_t *) ov5642_download_af,
                           sizeof(ov5642_download_af)/sizeof(ov5642_download_af[0]) );
    if( rc )
        goto exit_load_AF;

    writeCnt = 258;
    for( i=0; (i+writeCnt-1)<ARRAY_SIZE(ov5642_autofocus_reg); i+=writeCnt )
    {
        rc = ov5642_i2c_burst_write( &ov5642_autofocus_reg[i], writeCnt );
    }
    if( i < sizeof(ov5642_autofocus_reg) )
    {
        writeCnt= sizeof(ov5642_autofocus_reg) - i;
        rc = ov5642_i2c_burst_write( &ov5642_autofocus_reg[i], writeCnt );
    }

exit_load_AF:
    return rc;
}


static long ov5642_config_af(void)
{
    long rc = 0;
    struct ov5642_reg_t command_reg;

    command_reg.addr = OV5642_AF_CMD_TAG;
    command_reg.data = CMD_TAG_VALUE_NOT_ZERO;
    rc |= ov5642_i2c_write( &command_reg, 1 );

    command_reg.addr = OV5642_AF_CMD_MAIN;
    command_reg.data = CMD_MAIN_RETURN_TO_IDLE;
    rc |= ov5642_i2c_write( &command_reg, 1 );


    command_reg.addr = OV5642_AF_STA_FOCUS;
    ov5642_i2c_read( &command_reg, 1 );
    CDBG("%s: read %04Xh=0x%02X\n", __func__, command_reg.addr, command_reg.data);

    return rc;
}

static long ov5642_af_ready(void)
{
    long rc;
    struct ov5642_reg_t status_reg;

    status_reg.addr = OV5642_AF_STA_FOCUS;
    rc = ov5642_i2c_read(&status_reg, 1);
    if( rc )
        return rc;

    if( (status_reg.data != STA_FOCUS_S_IDLE) &&
        (status_reg.data != STA_FOCUS_S_FOCUSED) )
    {
        CSDBG("%s: status(0x%02X) is not idle or focused, reset it\n",
                 __func__, status_reg.data);
        ov5642_reset_AF();
        ov5642_enable_AF();
        rc = -1;
    }
    return rc;
}

static long ov5642_set_focus(struct auto_focus_status *pStatus)
{
    long rc = 0;

    struct ov5642_reg_t status_reg;
    int i;
    struct timespec focusStart;


    status_reg.addr = OV5642_AF_STA_FOCUS;

    down(&ov5642_af_sem);

    doAutoFocus = 1;
    ov5642_config_af();
    CDBG("ov5642_set_focus: starting single focus\n");
    rc = ov5642_start_single_AF();
    if( rc )
        goto not_focused;

    focusStart = CURRENT_TIME;

    for(i=0; i<20; i++)
    {
        up(&ov5642_af_sem);
        rc = wait_event_interruptible_timeout(
                    waitAutoFocus,
                    !doAutoFocus,
                    msecs_to_jiffies(100));
        down(&ov5642_af_sem);

        if( !doAutoFocus )
        {
            CSDBG("ov5642_set_focus: canceled\n");
            break;
        }

        if( rc < 0 )
        {
            CSDBG("ov5642_set_focus: signaled\n");
            break;
        }

        rc = ov5642_i2c_read( &status_reg, 1 );
        if( !rc )
        {
            if( status_reg.data == STA_FOCUS_S_FOCUSING )
                continue;
            else if( status_reg.data == STA_FOCUS_S_FOCUSED )
            {
                focusStart = timespec_sub(CURRENT_TIME, focusStart);
                pStatus->duration = (focusStart.tv_sec * 1000) + (focusStart.tv_nsec / 1000000);

                pStatus->steps = 0;

                CDBG("ov5642_set_focus: focused with %d steps, %d ms, status is %X\n",
                       pStatus->steps, pStatus->duration, status_reg.data);
                rc = 0;
                goto exit_focus;
            }
            else
            {
                CSDBG("ov5642_set_focus: exit due to %X\n", status_reg.data);
                break;
            }
        }
        CDBG("ov5642_set_focus: (i=%d)status is %x\n", i, status_reg.data);
    }

not_focused:
    ov5642_config_af();
    rc = -1;
    CDBG("ov5642_set_focus: not focused\n");

exit_focus:
    doAutoFocus = 0;
    up(&ov5642_af_sem);
    CDBG("ov5642_set_focus()<-, rc=%ld\n", rc);

    return rc;
}

static long ov5642_set_default_focus(void)
{
    long rc = 0;

    down(&ov5642_af_sem);

    if( doAutoFocus )
    {
        doAutoFocus = 0;
        wake_up(&waitAutoFocus);
    }else {
        rc = ov5642_config_af();
    }

    up(&ov5642_af_sem);

    CDBG("ov5642_set_default_focus(), rc=%ld\n", rc);

    return rc;
}

static void ov5642_af_download_func(struct work_struct *work)
{
#ifdef CHECK_PERF
    struct timespec start;

    start = CURRENT_TIME;
#endif
    ov5642_load_AF();
    ov5642_enable_AF();

    down(&ov5642_af_sem);
    afStatus = IDLE_STATUS;
    up(&ov5642_af_sem);

#ifdef CHECK_PERF
    start = timespec_sub(CURRENT_TIME, start);
    CSDBG("ov5642_af_download_func: %ld ms\n",
             ((long)start.tv_sec*1000)+((long)start.tv_nsec/1000000));
#endif
}

static long ov5642_detect_AF(void)
{
    long rc = 0;

    down(&ov5642_af_sem);

    if( IDLE_STATUS == afStatus )
    {
        rc = ov5642_af_ready();
    }else {
        rc = -1;
    }

    up(&ov5642_af_sem);

    return rc;
}

static long ov5642_prepare_AF(void)
{
    long rc = 0;

    down(&ov5642_af_sem);

    if( UNKNOWN_STATUS == afStatus )
    {
        afStatus = DOWNLOADING_STATUS;
        schedule_work(&af_download_work);
    }else if( IDLE_STATUS == afStatus ){
        rc |= ov5642_reset_AF();
        rc |= ov5642_enable_AF();
    }

    up(&ov5642_af_sem);

    return rc;
}

static void ov5642_init_AF(void)
{
    down(&ov5642_af_sem);
    afStatus = UNKNOWN_STATUS;
    INIT_WORK(&af_download_work, ov5642_af_download_func);
    init_waitqueue_head(&waitAutoFocus);
    up(&ov5642_af_sem);
}

static void ov5642_deinit_AF(void)
{
    cancel_work_sync(&af_download_work);
    down(&ov5642_af_sem);
    if( doAutoFocus )
    {
        doAutoFocus = 0;
        wake_up(&waitAutoFocus);
    }
    up(&ov5642_af_sem);
}

struct ov5642_af_func_array ov5642_af_func =
{
    .set_default_focus = ov5642_set_default_focus,
    .set_auto_focus = ov5642_set_focus,
    .init_auto_focus = ov5642_init_AF,
    .prepare_auto_focus = ov5642_prepare_AF,
    .detect_auto_focus = ov5642_detect_AF,
    .deinit_auto_focus = ov5642_deinit_AF,
};
#endif


