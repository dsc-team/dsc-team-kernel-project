
#include "ov7690.h"

struct ov7690_reg_t const ov7690_default_reg[] =
{
    { 0x12, 0x80 },
    { 0x11, 0x00 },
    { 0x0C, 0x06 },
    { 0x27, 0x80 },
    { 0x64, 0x10 },

    { 0x69, 0x12 },
    { 0x2F, 0x60 },
    { 0x48, 0x42 },
    { 0x49, 0x0C },   
    { 0x41, 0x43 },
    { 0x44, 0x24 },
    { 0x4B, 0x8C },
    { 0x4C, 0x7B },
    { 0x4D, 0x0A },
    { 0x28, 0x02 },   
    { 0x29, 0x50 },   
    { 0x1B, 0x19 },
    { 0x39, 0x80 },
    { 0x80, 0x7F },
    { 0x81, 0xFF },
    { 0x91, 0x20 },
    { 0x21, 0x23 }, 
    
    { 0x12, 0x00 },
    { 0x82, 0x03 },
    { 0xD0, 0x48 }, 
    { 0x22, 0x00 },

    { 0x17, 0x69 },
    { 0x18, 0xa4 },
    { 0x19, 0x0c },
    { 0x1a, 0xf6 },
    { 0xc8, 0x02 },
    { 0xc9, 0x80 }, 
    { 0xca, 0x01 },
    { 0xcb, 0xe0 }, 
    { 0xcc, 0x02 },
    { 0xcd, 0x80 }, 
    { 0xce, 0x01 },
    { 0xcf, 0xe0 }, 
};

struct ov7690_reg_t const ov7690_IQ_reg[] =
{

    { 0x85, 0x90 },
    { 0x86, 0x00 },
    { 0x87, 0x9B }, 
    { 0x88, 0x9B }, 
    { 0x89, 0x33 }, 
    { 0x8A, 0x30 },   
    { 0x8B, 0x2E },   

    { 0xBB, 0xC8 },
    { 0xBC, 0xBE },
    { 0xBD, 0x0A },
    { 0xBE, 0x3F },
    { 0xBF, 0x80 },
    { 0xC0, 0xBF },
    { 0xC1, 0x1E },

    { 0xB4, 0x06 }, 
    { 0xB5, 0x08 }, 
    { 0xB6, 0x04 }, 
    { 0xB7, 0x04 }, 
    { 0xB8, 0x09 }, 
    { 0xB9, 0x00 },
    { 0xBA, 0x18 },

    { 0x5A, 0x16 },
    { 0x5B, 0xBF },
    { 0x5C, 0x38 },
    { 0x5D, 0x22 },

    { 0x24, 0x68 },
    { 0x25, 0x58 },
    { 0x26, 0xa3 },
    
    { 0x80, 0x7F }, 
    { 0xA3, 0x06 }, 
    { 0xA4, 0x0F }, 
    { 0xA5, 0x20 }, 
    { 0xA6, 0x42 }, 
    { 0xA7, 0x51 }, 
    { 0xA8, 0x5D }, 
    { 0xA9, 0x67 }, 
    { 0xAA, 0x70 }, 
    { 0xAB, 0x79 }, 
    { 0xAC, 0x7E }, 
    { 0xAD, 0x88 }, 
    { 0xAE, 0x92 }, 
    { 0xAF, 0xA2 }, 
    { 0xB0, 0xAF }, 
    { 0xB1, 0xBD }, 
    { 0xB2, 0x22 }, 

    { 0x8C, 0x5C },
    { 0x8D, 0x11 },
    { 0x8E, 0x12 },
    { 0x8F, 0x19 },
    { 0x90, 0x50 },
    { 0x91, 0x20 },
    { 0x96, 0xFF },
    { 0x97, 0x00 },
    { 0x9C, 0xF0 },
    { 0x9D, 0xF0 },
    { 0x9E, 0xF0 },
    { 0x9F, 0xFF },
    { 0x98, 0x40 },    
    { 0x9A, 0x64 },    
    { 0x92, 0x92 },    
    { 0x95, 0x0D },    
    { 0x99, 0x34 },    
    { 0x9B, 0x38 },    
    { 0x93, 0x1C },    
    { 0x94, 0x0D },    
    { 0xA0, 0x5C },    
    { 0xA1, 0x55 },    
    { 0xA2, 0x0C },    
    { 0x14, 0x28 },    
    { 0x13, 0xFF },
    { 0x1E, 0xB1 },    

    { 0x15, 0x94 },

    { 0xD2, 0x04 },
    { 0xd5, 0x20 },
    { 0xD4, 0x26 },

    { 0x2A, 0x30 },
    { 0x2B, 0x0B },
    { 0x2C, 0x00 },
    { 0x50, 0x99 },
    { 0x51, 0x7f },
};

struct ov7690_reg_t const ov7690_preview_reg[] =
{
   {  0xc8,0x02  },
   {  0xc9,0x80  },
   {  0xca,0x01  },
   {  0xcb,0xe0  },

   {  0xcc,0x02  },
   {  0xcd,0x80  },
   {  0xce,0x01  },
   {  0xcf,0xe0  },
};

struct ov7690_reg_t const ov7690_snapshot_reg[] =
{
   {  0xc8,0x02  },
   {  0xc9,0x80  },
   {  0xca,0x01  },
   {  0xcb,0xe0  },

   {  0xcc,0x02  },
   {  0xcd,0x80  },
   {  0xce,0x01  },
   {  0xcf,0xe0  },
};

struct ov7690_basic_reg_arrays ov7690_regs = {
	.default_array = &ov7690_default_reg[0],
	.default_length = ARRAY_SIZE(ov7690_default_reg),
	.preview_array = &ov7690_preview_reg[0],
	.preview_length = ARRAY_SIZE(ov7690_preview_reg),
	.snapshot_array = &ov7690_snapshot_reg[0],
	.snapshot_length = ARRAY_SIZE(ov7690_snapshot_reg),
    .IQ_array = &ov7690_IQ_reg[0],
    .IQ_length = ARRAY_SIZE(ov7690_IQ_reg),
};



struct ov7690_reg_t const ov7690_1_of_1_fps_reg[] =
{
    { OV7690_BIT_OPERATION_TAG, 0x80 },
    { 0x15, 0x00 },
    { 0x2d, 0x00 },
    { 0x2e, 0x00 },
};

struct ov7690_reg_t const ov7690_1_of_2_fps_reg[] =
{
    { OV7690_BIT_OPERATION_TAG, 0xf0 },
    { 0x15, 0x90 },
};

struct ov7690_reg_t const ov7690_1_of_8_fps_reg[] =
{
    { OV7690_BIT_OPERATION_TAG, 0xf0 },
    { 0x15, 0xc0 },
};

struct ov7690_reg_group_t const ov7690_frame_rate_group[] =
{
    { (struct ov7690_reg_t *)ov7690_1_of_1_fps_reg, ARRAY_SIZE(ov7690_1_of_1_fps_reg) },
    { (struct ov7690_reg_t *)ov7690_1_of_2_fps_reg, ARRAY_SIZE(ov7690_1_of_2_fps_reg) },
    { (struct ov7690_reg_t *)ov7690_1_of_8_fps_reg, ARRAY_SIZE(ov7690_1_of_8_fps_reg) },
};

struct ov7690_reg_t const ov7690_whole_image_ae_reg[] =
{
    { OV7690_BIT_OPERATION_TAG, 0x02 },
    { 0x5d, 0x00 },
};

struct ov7690_reg_t const ov7690_center_quarter_ae_reg[] =
{
    { OV7690_BIT_OPERATION_TAG, 0x02 },
    { 0x5d, 0x02 },
};

struct ov7690_reg_group_t const ov7690_ae_weighting_group[] =
{
    { (struct ov7690_reg_t *)ov7690_whole_image_ae_reg, ARRAY_SIZE(ov7690_whole_image_ae_reg) },
    { (struct ov7690_reg_t *)ov7690_center_quarter_ae_reg, ARRAY_SIZE(ov7690_center_quarter_ae_reg) },
};

struct ov7690_reg_t const ov7690_50Hz_banding_reg[] =
{
    { OV7690_BIT_OPERATION_TAG, 0x01 },
    { 0x14, 0x01 },
    { 0x2a, 0x30 },
    { 0x2b, 0xa8 },
    { 0x20, 0x00 },
    { 0x21, 0x34 },
    { 0x50, 0x7f },
    { 0x51, 0x6a },
};

struct ov7690_reg_t const ov7690_60Hz_banding_reg[] =
{
    { OV7690_BIT_OPERATION_TAG, 0x01 },
    { 0x14, 0x00 },
    { 0x2a, 0x30 },
    { 0x2b, 0x0b },
    { 0x20, 0x00 },
    { 0x21, 0x23 },
    { 0x50, 0x99 },
    { 0x51, 0x7f },
};

struct ov7690_reg_group_t const ov7690_anti_banding_group[] =
{
    { (struct ov7690_reg_t *)ov7690_50Hz_banding_reg, ARRAY_SIZE(ov7690_50Hz_banding_reg) },
    { (struct ov7690_reg_t *)ov7690_60Hz_banding_reg, ARRAY_SIZE(ov7690_60Hz_banding_reg) },
};

struct ov7690_reg_t const ov7690_brightness_P2_reg[] =
{
    { 0x81, 0xff },
    { 0xD2, 0x04 },
    { 0xDC, 0x01 },
    { 0xD3, 0x20 },
};

struct ov7690_reg_t const ov7690_brightness_P1_reg[] =
{
    { 0x81, 0xff },
    { 0xD2, 0x04 },
    { 0xDC, 0x01 },
    { 0xD3, 0x10 },
};

struct ov7690_reg_t const ov7690_brightness_P0_reg[] =
{
    { 0x81, 0xff },
    { 0xD2, 0x04 },
    { 0xDC, 0x01 },
    { 0xD3, 0x00 },
};

struct ov7690_reg_t const ov7690_brightness_N1_reg[] =
{
    { 0x81, 0xff },
    { 0xD2, 0x04 },
    { 0xDC, 0x09 },
    { 0xD3, 0x10 },
};

struct ov7690_reg_t const ov7690_brightness_N2_reg[] =
{
    { 0x81, 0xff },
    { 0xD2, 0x04 },
    { 0xDC, 0x09 },
    { 0xD3, 0x20 },
};

struct ov7690_reg_group_t const ov7690_brightness_group[] =
{
    { (struct ov7690_reg_t *)ov7690_brightness_P2_reg, ARRAY_SIZE(ov7690_brightness_P2_reg) },
    { (struct ov7690_reg_t *)ov7690_brightness_P1_reg, ARRAY_SIZE(ov7690_brightness_P1_reg) },
    { (struct ov7690_reg_t *)ov7690_brightness_P0_reg, ARRAY_SIZE(ov7690_brightness_P0_reg) },
    { (struct ov7690_reg_t *)ov7690_brightness_N1_reg, ARRAY_SIZE(ov7690_brightness_N1_reg) },
    { (struct ov7690_reg_t *)ov7690_brightness_N2_reg, ARRAY_SIZE(ov7690_brightness_N2_reg) },
};

struct ov7690_reg_t const ov7690_contrast_P2_reg[] =
{
    { 0x81, 0xff },
    { 0xD2, 0x04 },
    { 0xd5, 0x10 },
    { 0xd4, 0x2e },
};

struct ov7690_reg_t const ov7690_contrast_P1_reg[] =
{
    { 0x81, 0xff },
    { 0xD2, 0x04 },
    { 0xd5, 0x18 },
    { 0xd4, 0x2a },
};

struct ov7690_reg_t const ov7690_contrast_P0_reg[] =
{
    { 0x81, 0xff },
    { 0xD2, 0x04 },
    { 0xd5, 0x20 },
    { 0xd4, 0x26 },
};

struct ov7690_reg_t const ov7690_contrast_N1_reg[] =
{
    { 0x81, 0xff },
    { 0xD2, 0x04 },
    { 0xd5, 0x28 },
    { 0xd4, 0x22 },
};

struct ov7690_reg_t const ov7690_contrast_N2_reg[] =
{
    { 0x81, 0xff },
    { 0xD2, 0x04 },
    { 0xd5, 0x30 },
    { 0xd4, 0x1e },
};

struct ov7690_reg_group_t const ov7690_contrast_group[] =
{
    { (struct ov7690_reg_t *)ov7690_contrast_P2_reg, ARRAY_SIZE(ov7690_contrast_P2_reg) },
    { (struct ov7690_reg_t *)ov7690_contrast_P1_reg, ARRAY_SIZE(ov7690_contrast_P1_reg) },
    { (struct ov7690_reg_t *)ov7690_contrast_P0_reg, ARRAY_SIZE(ov7690_contrast_P0_reg) },
    { (struct ov7690_reg_t *)ov7690_contrast_N1_reg, ARRAY_SIZE(ov7690_contrast_N1_reg) },
    { (struct ov7690_reg_t *)ov7690_contrast_N2_reg, ARRAY_SIZE(ov7690_contrast_N2_reg) },
};

struct ov7690_reg_t const ov7690_auto_wb_reg[] =
{
    { 0x13, 0xf7 },
};

struct ov7690_reg_t const ov7690_sun_light_reg[] =
{
    { 0x13, 0xf5 },
    { 0x01, 0x4f },
    { 0x02, 0x54 },
    { 0x03, 0x42 },
};

struct ov7690_reg_t const ov7690_fluorescent_reg[] =
{
    { 0x13, 0xf5 },
    { 0x01, 0x5c },
    { 0x02, 0x50 },
    { 0x03, 0x40 },
};

struct ov7690_reg_t const ov7690_incandscent_reg[] =
{
    { 0x13, 0xf5 },
    { 0x01, 0x6b },
    { 0x02, 0x3a },
    { 0x03, 0x40 },
};

struct ov7690_reg_group_t const ov7690_white_balance_group[] =
{
    { (struct ov7690_reg_t *)ov7690_auto_wb_reg, ARRAY_SIZE(ov7690_auto_wb_reg) },
    { (struct ov7690_reg_t *)ov7690_sun_light_reg, ARRAY_SIZE(ov7690_sun_light_reg) },
    { (struct ov7690_reg_t *)ov7690_fluorescent_reg, ARRAY_SIZE(ov7690_fluorescent_reg) },
    { (struct ov7690_reg_t *)ov7690_incandscent_reg, ARRAY_SIZE(ov7690_incandscent_reg) },
};

struct ov7690_reg_t const ov7690_general_cmx_reg[] =
{
    { 0xC1, 0x1E },
    { 0xBB, 0xD8 },
    { 0xBC, 0xCE },
    { 0xBD, 0x0A },
    { 0xBE, 0x4F },
    { 0xBF, 0x90 },
    { 0xC0, 0xDF },
};

struct ov7690_reg_t const ov7690_person_cmx_reg[] =
{
    { 0xBB, 0xC8 },
    { 0xBC, 0xBE },
    { 0xBD, 0x0A },
    { 0xBE, 0x3F },
    { 0xBF, 0x80 },
    { 0xC0, 0xBF },
    { 0xC1, 0x1E },
};

struct ov7690_reg_t const ov7690_land_cmx_reg[] =
{
    { 0xC1, 0x5E },
    { 0xBB, 0xD8 },
    { 0xBC, 0xCE },
    { 0xBD, 0x0A },
    { 0xBE, 0x4F },
    { 0xBF, 0x90 },
    { 0xC0, 0xDF },
};

struct ov7690_reg_group_t const ov7690_color_matrix_group[] =
{
    { (struct ov7690_reg_t *)ov7690_general_cmx_reg, ARRAY_SIZE(ov7690_general_cmx_reg) },
    { (struct ov7690_reg_t *)ov7690_person_cmx_reg, ARRAY_SIZE(ov7690_person_cmx_reg) },
    { (struct ov7690_reg_t *)ov7690_land_cmx_reg, ARRAY_SIZE(ov7690_land_cmx_reg) },
};

struct ov7690_reg_t const ov7690_general_target_reg[] =
{
    { 0x24, 0x68 },
    { 0x25, 0x58 },
    { 0x26, 0xa3 },
};

struct ov7690_reg_t const ov7690_night_target_reg[] =
{
    { 0x24, 0x40 },
    { 0x25, 0x30 },
    { 0x26, 0x82 },
};

struct ov7690_reg_group_t const ov7690_ae_target_group[] =
{
    { (struct ov7690_reg_t *)ov7690_general_target_reg, ARRAY_SIZE(ov7690_general_target_reg) },
    { (struct ov7690_reg_t *)ov7690_night_target_reg, ARRAY_SIZE(ov7690_night_target_reg) },
};

struct ov7690_special_reg_groups ov7690_special_regs =
{
    .frame_rate_group    = ov7690_frame_rate_group,
    .ae_weighting_group  = ov7690_ae_weighting_group,
    .anti_banding_group  = ov7690_anti_banding_group,
    .brightness_group    = ov7690_brightness_group,
    .contrast_group      = ov7690_contrast_group,
    .white_balance_group = ov7690_white_balance_group,
    .color_matrix_group  = ov7690_color_matrix_group,
    .ae_target_group     = ov7690_ae_target_group,
};

