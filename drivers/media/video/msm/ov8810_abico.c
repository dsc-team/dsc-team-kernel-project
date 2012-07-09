/*
Copyright (c) 2010, Code Aurora Forum. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of Code Aurora Forum, Inc. nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Alternatively, and instead of the terms immediately above, this
software may be relicensed by the recipient at their option under the
terms of the GNU General Public License version 2 ("GPL") and only
version 2.  If the recipient chooses to relicense the software under
the GPL, then the recipient shall replace all of the text immediately
above and including this paragraph with the text immediately below
and between the words START OF ALTERNATE GPL TERMS and END OF
ALTERNATE GPL TERMS and such notices and license terms shall apply
INSTEAD OF the notices and licensing terms given above.

START OF ALTERNATE GPL TERMS

Copyright (c) 2010, Code Aurora Forum. All rights reserved.

This software was originally licensed under the Code Aurora Forum
Inc. Dual BSD/GPL License version 1.1 and relicensed as permitted
under the terms thereof by a recipient under the General Public
License Version 2.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 and
only version 2 as published by the Free Software Foundation.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
02110-1301, USA.

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

END OF ALTERNATE GPL TERMS
*/

#include <linux/delay.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <media/msm_camera.h>
#include <mach/gpio.h>

#include <mach/camera.h>
#include "ov8810.h"
#include <mach/austin_hwid.h>

#if 0
#include <asm/io.h>
#include <mach/custmproc.h>
#include <mach/smem_pc_oem_cmd.h>

typedef enum
{
  QISDA_DIAGPKT_REQ_RSP_IDLE = 0,
  QISDA_DIAGPKT_REQ_RSP_ON_GOING,
  QISDA_DIAGPKT_REQ_RSP_DONE
}diagpkt_req_rsp_process_status_type;

#define GCC_PACKED __attribute__((packed))
typedef struct GCC_PACKED {
  short           iRequestSize;
  short           piResponseSize;
  unsigned char   piRequestBytes[1024];
  unsigned char   piResponseBytes[1024];
  diagpkt_req_rsp_process_status_type process_status;
} qisda_diagpkt_req_rsp_type;
typedef struct GCC_PACKED {
  unsigned char   nv_operation;
  unsigned short  nv_item_index;
  unsigned char   item_data[128];
  unsigned short  nv_operation_status;
} nv_read_command;

#define NV_QISDA_LENS_CORRECTION_DATA_FIRST_PART_I 		30028
#define NV_QISDA_LENS_CORRECTION_DATA_SECOND_PART_I 	30029
#define NV_QISDA_LENS_CORRECTION_DATA_THIRD_PART_I 	30030
#define NV_QISDA_LENS_CORRECTION_DATA_FOURTH_PART_I 	30031

#define 		NV_LENS_VALID_DATA_PER_NV_WITH_HEADER_LEN				120
#define 		NV_LENS_VALID_DATA_PER_NV_WITHOUT_HEADER_LEN			110
#define 		NV_LENS_MAGIC_HEADER_LEN									10
static unsigned char NV_magic_header[5] = {0x51, 0x49, 0x53, 0x44, 0x41};
#endif

/*=============================================================
	SENSOR REGISTER DEFINES
==============================================================*/
#define Q8    0x00000100

/* Omnivision8810 product ID register address */
#define OV8810_PIDH_REG                       0x300A
#define OV8810_PIDL_REG                       0x300B
/* Omnivision8810 product ID */
#define OV8810_PID                    0x88
/* Omnivision8810 version */
#define OV8810_VER                    0x13
/* Time in milisecs for waiting for the sensor to reset */
#define OV8810_RESET_DELAY_MSECS    66
#define OV8810_DEFAULT_CLOCK_RATE   24000000

/* Registers*/
#define OV8810_GAIN         0x3000
#define OV8810_AEC_MSB      0x3002
#define OV8810_AEC_LSB      0x3003
#define OV8810_AF_MSB       0x30EC
#define OV8810_AF_LSB       0x30ED
#define OV8810_GROUP_LATCH	0x30B7

#define OV8810_LENC		0x3300

/* AF Total steps parameters */
#define OV8810_STEPS_NEAR_TO_CLOSEST_INF  43
#define OV8810_TOTAL_STEPS_NEAR_TO_FAR    43
/*Test pattern*/
/* Color bar pattern selection */
#define OV8810_COLOR_BAR_PATTERN_SEL_REG      0x307B
/* Color bar enabling control */
#define OV8810_COLOR_BAR_ENABLE_REG           0x307D
/* Time in milisecs for waiting for the sensor to reset*/
#define OV8810_RESET_DELAY_MSECS    66
/* I2C Address of the Sensor */
#define OV8810_I2C_SLAVE_ID    0x6c

static uint16_t snapshot_aec_msb;
static uint16_t snapshot_aec_lsb;
static uint16_t snapshot_gain;

static int  panorama_mode 	= 0;
module_param(panorama_mode, int, 0664);

static int ov8810_sersor_version = 0;
module_param(ov8810_sersor_version, int, 0664);

static int ov8810_module_vendor_id = 0;
module_param(ov8810_module_vendor_id, int, 0664);

int ov8810_camera_module_probe_status = 0;

/*============================================================================
							 TYPE DECLARATIONS
============================================================================*/

typedef unsigned char byte;
/* 16bit address - 8 bit context register structure */
typedef struct reg_addr_val_pair_struct
{
	uint16_t  reg_addr;
	byte      reg_val;
}reg_struct_type;

enum ov8810_test_mode_t {
	TEST_OFF,
	TEST_1,
	TEST_2,
	TEST_3
};

enum ov8810_resolution_t {
	QTR_SIZE,
	FULL_SIZE,
	INVALID_SIZE
};

#define OV8810_EACH_LENSC_LEN		144
#define OV8810_THREE_LENSC_LEN	432

/*
OV8810_MODULE_SERIAL_INFO_LEN
Vendor ID, version, year/mon/day, number, total number
*/
#define OV8810_MODULE_SERIAL_INFO_LEN	8
#define ABICO_MODULE_VENDOR_ID	0xa
#define ABICO_EEPROM_I2C_ADDR_BANK1	0x50
#define ABICO_EEPROM_I2C_ADDR_BANK2	0x51
#define ABICO_EEPROM_PER_BANK_LEN	256
#define I2C_RETRY_TIMES		2
static unsigned char ov8810_module_serial_info[OV8810_MODULE_SERIAL_INFO_LEN] = {0};

static int is_lens_correction_data_vaild = 0;
module_param(is_lens_correction_data_vaild, int, 0664);

typedef enum
{
  	LENS_AWB_OUTDOOR_SUNLIGHT =  0,  // D65
  	LENS_AWB_INDOOR_WARM_FLO,        // TL84
  	LENS_AWB_INDOOR_INCANDESCENT,    // A
  	LENS_AWB_MAX_LIGHT,
  	LENS_AWB_INVALID_LIGHT = LENS_AWB_MAX_LIGHT
} lens_correction_awb_light_type;

static unsigned char ov8810_lensc_awb_parameters[OV8810_THREE_LENSC_LEN+6] = {0};

//Default use TL84
static lens_correction_awb_light_type ov8810_curr_lenc_light_type = LENS_AWB_INDOOR_WARM_FLO;

/*============================================================================
							DATA DECLARATIONS
============================================================================*/

static reg_struct_type ov8810_init_settings_array[] =
{
	{0x3012, 0x80},
	{0x30fa, 0x00},
	{0x300f, 0x04},
	{0x300e, 0x05},
	{0x3010, 0x28},
	{0x3011, 0x22},
	{0x3000, 0x30},
	{0x3002, 0x09},
	{0x3003, 0xb2},
	{0x3302, 0x20},
	{0x30b2, 0x10},// IO_CTRL2 increase drive strength
	{0x30a0, 0x40},
	{0x3098, 0x24},
	{0x3099, 0x81},
	{0x309a, 0x64},
	{0x309b, 0x00},
	{0x309d, 0x64},
	{0x309e, 0x2d},
	{0x3320, 0xC2},//;turn, 0xof, 0xAWB, 0xby, 0xsetting, 0xAWB, 0xgain, 0xto, 0xbe, 0xfixed, 0x1x
	{0x3321, 0x02},
	{0x3322, 0x04},
	{0x3328, 0x40},
	{0x3329, 0xe3},
	{0x3306, 0x00},
	{0x3316, 0x03},
	{0x3079, 0x0a},
	{0x3058, 0x01},
	{0x3059, 0xa0},
	{0x306b, 0x00},
	{0x3065, 0x50},
	{0x3067, 0x40},
	{0x3069, 0x80},
	{0x3071, 0x40},
	{0x3300, 0xff}, //0xef LENC bit 4 0 off 1 on
	{0x3334, 0x02},
	{0x3331, 0x08},
	{0x3332, 0x08},
	{0x3333, 0x41},
	{0x3091, 0x00},
	{0x3006, 0x00},
	{0x3082, 0x80},
	{0x331e, 0x94},
	{0x331f, 0x6e},
	{0x3092, 0x00},
	{0x3094, 0x01},
	{0x3090, 0x2b},
	{0x30ab, 0x44},
	{0x3095, 0x0a},
	{0x308d, 0x00},
	{0x3082, 0x00},
	{0x3080, 0x40},
	{0x30aa, 0x59},
	{0x30a9, 0x08},
	{0x30be, 0x08},
	{0x309f, 0x23},
	{0x3065, 0x40},
	{0x3068, 0x00},
	{0x30bf, 0x80},
	{0x309c, 0x00},
	{0x3084, 0x44},
	{0x3016, 0x03},
	{0x30e9, 0x09},
	{0x3075, 0x29},
	{0x3076, 0x29},
	{0x3077, 0x29},
	{0x3078, 0x29},
	{0x306a, 0x05},
	{0x3000, 0x00},
	{0x3015, 0x33},
	{0x3090, 0x36},
	{0x333e, 0x00},
	{0x3000, 0x7f},
	{0x3087, 0x41},
	{0x3090, 0x99},
	{0x309e, 0x1b},
	{0x30e3, 0x0e},
	{0x30f0, 0x00},
	{0x30f2, 0x00},
	{0x30f4, 0x90},
	{0x3071, 0x40},
	{0x3347, 0x00},
	{0x3071, 0x40},
	{0x3347, 0x00},
	{0x3092, 0x00},
	{0x3090, 0x97},

	{0x30F0, 0x10},
	{0x30F1, 0x56},
	{0x30FB, 0x8e},
	{0x30F3, 0xa7},

	{0x308d, 0x02},
	{0x30e7, 0x41},
	{0x30b3, 0x08},
	{0x33e5, 0x00},
	{0x350e, 0x40},
	{0x301f, 0x00},
	{0x309f, 0x23},
	{0x3013, 0x00},//;turn, 0xoff, 0xAEC/AGC, 0xfunction 0xc0
	{0x30e1, 0x90},
	{0x3058, 0x01},
	{0x3500, 0x40}, //hsync new [2] pclk invert
	{0x30f9, 0x0e},

	{0x30fa, 0x01}
};

/*1632x1224; 24MHz MCLK 96MHz PCLK*/
static reg_struct_type ov8810_qtr_settings_array[] =
{
{0x3071, 0x50},
{0x30f8, 0x45},
{0x3020, 0x04},
{0x3021, 0xf4},
{0x3022, 0x09},
{0x3023, 0xda},
{0x3024, 0x00},
{0x3025, 0x04},
{0x3026, 0x00},
{0x3027, 0x00},//0x8
{0x3028, 0x0c},
{0x3029, 0xd8},//0xd7 //0xd8
{0x302a, 0x09},
{0x302b, 0x9f},
{0x302c, 0x06},// ;1632
{0x302d, 0x60},
{0x302e, 0x04},// ;1224
{0x302f, 0xc8},
{0x3068, 0x00},
{0x307e, 0x00},
{0x3301, 0x0B},
{0x331c, 0x00},
{0x331d, 0x00},
{0x308a, 0x02},
{0x3072, 0x0d},
{0x3319, 0x04},
{0x309e, 0x09},
{0x300e, 0x05},
{0x300f, 0x04},
{0x33E4, 0x07},
};

/* 2064x1544 Sensor Raw; 24MHz MCLK 96MHz PCLK*/
static reg_struct_type ov8810_full_settings_array[] =
{
{0x3071, 0x40},
{0x30f8, 0x40},
{0x3020, 0x09},
{0x3021, 0xc4},
{0x3022, 0x0f},
{0x3023, 0x88},//0x30 //0x2c
{0x3024, 0x00},
{0x3025, 0x02},//0x6//0x6
{0x3026, 0x00},
{0x3027, 0x00},//0x0
{0x3028, 0x0c},
{0x3029, 0xdd},//0xd9
{0x302a, 0x09},
{0x302b, 0x9f},
{0x302c, 0x0c},
{0x302d, 0xd0},//0xd0 //0xcc
{0x302e, 0x09},
{0x302f, 0x98},
{0x3068, 0x00},
{0x307e, 0x00},
{0x3301, 0x0B},
{0x331c, 0x28},
{0x331d, 0x21},
{0x308a, 0x01},
{0x3072, 0x01},
{0x3319, 0x06},
{0x309e, 0x1b},
{0x300e, 0x05},
{0x300F, 0x04},

{0x33E4, 0x02},
};
/* 816x612, 24MHz MCLK 96MHz PCLK */

static reg_struct_type ov8810_lenc_settings_array[] =
{
	/* lens shading */
	{0x3350, 0x06},
	{0x3351, 0xab},
	{0x3352, 0x05},
	{0x3353, 0x00},
	{0x3354, 0x04},
	{0x3355, 0xf8},
	{0x3356, 0x07},
	{0x3357, 0x74},
	{0x3358, 0x28},
	{0x3359, 0x19},
	{0x335a, 0x13},
	{0x335b, 0x10},
	{0x335c, 0x10},
	{0x335d, 0x13},
	{0x335e, 0x1c},
	{0x335f, 0x2b},
	{0x3360, 0x10},
	{0x3361, 0xc},
	{0x3362, 0x9},
	{0x3363, 0x8},
	{0x3364, 0x8},
	{0x3365, 0xa},
	{0x3366, 0xe},
	{0x3367, 0x11},
	{0x3368, 0x9},
	{0x3369, 0x6},
	{0x336a, 0x4},
	{0x336b, 0x3},
	{0x336c, 0x3},
	{0x336d, 0x5},
	{0x336e, 0x7},
	{0x336f, 0xa},
	{0x3370, 0x7},
	{0x3371, 0x4},
	{0x3372, 0x1},
	{0x3373, 0x0},
	{0x3374, 0x0},
	{0x3375, 0x2},
	{0x3376, 0x5},
	{0x3377, 0x7},
	{0x3378, 0x6},
	{0x3379, 0x4},
	{0x337a, 0x1},
	{0x337b, 0x0},
	{0x337c, 0x0},
	{0x337d, 0x2},
	{0x337e, 0x5},
	{0x337f, 0x7},
	{0x3380, 0x9},
	{0x3381, 0x6},
	{0x3382, 0x4},
	{0x3383, 0x3},
	{0x3384, 0x3},
	{0x3385, 0x5},
	{0x3386, 0x8},
	{0x3387, 0xa},
	{0x3388, 0x10},
	{0x3389, 0xc},
	{0x338a, 0xa},
	{0x338b, 0x9},
	{0x338c, 0x9},
	{0x338d, 0xa},
	{0x338e, 0xf},
	{0x338f, 0x10},
	{0x3390, 0x2f},
	{0x3391, 0x17},
	{0x3392, 0x13},
	{0x3393, 0xf},
	{0x3394, 0x10},
	{0x3395, 0x14},
	{0x3396, 0x1a},
	{0x3397, 0x2f},
	{0x3398, 0xd},
	{0x3399, 0xc},
	{0x339a, 0xc},
	{0x339b, 0xd},
	{0x339c, 0xd},
	{0x339d, 0x9},
	{0x339e, 0xe},
	{0x339f, 0xe},
	{0x33a0, 0xf},
	{0x33a1, 0xe},
	{0x33a2, 0xe},
	{0x33a3, 0xc},
	{0x33a4, 0x11},
	{0x33a5, 0xe},
	{0x33a6, 0x11},
	{0x33a7, 0x10},
	{0x33a8, 0xf},
	{0x33a9, 0xc},
	{0x33aa, 0x12},
	{0x33ab, 0xe},
	{0x33ac, 0x11},
	{0x33ad, 0x10},
	{0x33ae, 0xf},
	{0x33af, 0xc},
	{0x33b0, 0x12},
	{0x33b1, 0xe},
	{0x33b2, 0x10},
	{0x33b3, 0x10},
	{0x33b4, 0x10},
	{0x33b5, 0xc},
	{0x33b6, 0x13},
	{0x33b7, 0xa},
	{0x33b8, 0xc},
	{0x33b9, 0xc},
	{0x33ba, 0xb},
	{0x33bb, 0xe},
	{0x33bc, 0x1f},
	{0x33bd, 0x1f},
	{0x33be, 0x1f},
	{0x33bf, 0x1f},
	{0x33c0, 0x1f},
	{0x33c1, 0x1f},
	{0x33c2, 0x1f},
	{0x33c3, 0x1b},
	{0x33c4, 0x18},
	{0x33c5, 0x18},
	{0x33c6, 0x1b},
	{0x33c7, 0x1f},
	{0x33c8, 0x1f},
	{0x33c9, 0x15},
	{0x33ca, 0x10},
	{0x33cb, 0x10},
	{0x33cc, 0x15},
	{0x33cd, 0x1e},
	{0x33ce, 0x1f},
	{0x33cf, 0x16},
	{0x33d0, 0x10},
	{0x33d1, 0x10},
	{0x33d2, 0x16},
	{0x33d3, 0x1e},
	{0x33d4, 0x1f},
	{0x33d5, 0x1b},
	{0x33d6, 0x1a},
	{0x33d7, 0x19},
	{0x33d8, 0x1a},
	{0x33d9, 0x1f},
	{0x33da, 0x1f},
	{0x33db, 0x1f},
	{0x33dc, 0x1f},
	{0x33dd, 0x1f},
	{0x33de, 0x1f},
	{0x33df, 0x1f},
};

static uint32_t OV8810_FULL_SIZE_WIDTH        = 3280;
static uint32_t OV8810_FULL_SIZE_HEIGHT       = 2456;
static uint32_t  OV8810_QTR_SIZE_WIDTH         = 1632;
static uint32_t OV8810_QTR_SIZE_HEIGHT        = 1224;

static uint32_t OV8810_HRZ_FULL_BLK_PIXELS    = 696;
static uint32_t OV8810_VER_FULL_BLK_LINES     =  44;
static uint32_t OV8810_HRZ_QTR_BLK_PIXELS     = 890;
static uint32_t OV8810_VER_QTR_BLK_LINES      =  44;

/* AF Tuning Parameters */
static uint16_t ov8810_step_position_table[OV8810_TOTAL_STEPS_NEAR_TO_FAR+1];
static uint8_t ov8810_damping_threshold = 10;
static uint8_t ov8810_damping_course_step = 4;
static uint8_t ov8810_damping_fine_step = 10;
static uint16_t ov8810_focus_debug = 0;
static uint16_t ov8810_use_default_damping = 1;
static uint16_t ov8810_use_threshold_damping = 1;
static uint32_t stored_line_length_ratio = 1 * Q8;
static uint8_t ov8810_damping_time_wait;
static uint8_t S3_to_0 = 0x1;
/* static Variables*/
static uint16_t step_position_table[OV8810_TOTAL_STEPS_NEAR_TO_FAR+1];
/* FIXME: Changes from here */
struct ov8810_work_t {
	struct work_struct work;
};
static struct  ov8810_work_t *ov8810_sensorw;
static struct  i2c_client *ov8810_client;
struct ov8810_ctrl_t {
	const struct  msm_camera_sensor_info *sensordata;
	uint32_t sensormode;
	uint32_t fps_divider; 		/* init to 1 * 0x00000400 */
	uint32_t pict_fps_divider; 	/* init to 1 * 0x00000400 */
	uint16_t fps;
	int16_t  curr_lens_pos;
	uint16_t curr_step_pos;
	uint16_t my_reg_gain;
	uint32_t my_reg_line_count;
	uint16_t total_lines_per_frame;
	enum ov8810_resolution_t prev_res;
	enum ov8810_resolution_t pict_res;
	enum ov8810_resolution_t curr_res;
	enum ov8810_test_mode_t  set_test;
	unsigned short imgaddr;
};
static struct ov8810_ctrl_t *ov8810_ctrl;
static DECLARE_WAIT_QUEUE_HEAD(ov8810_wait_queue);
DECLARE_MUTEX(ov8810_sem_abico);

/*=============================================================*/

static int ov8810_read_eeprom(unsigned short saddr, uint8_t regaddr,
		     uint8_t *buf, uint32_t rdlen)
{

	uint8_t ldat = regaddr;
	int rc = 0;
	int retry = 0;

	struct i2c_msg msgs[] = {
		[0] = {
			.addr	= saddr,
			.flags	= 0,
			.buf	= (void *)&ldat,
			.len	= 1
		},
		[1] = {
			.addr	= saddr,
			.flags	= I2C_M_RD,
			.buf	= (void *)buf,
			.len	= rdlen
		}
	};
	do{
		rc = i2c_transfer(ov8810_client->adapter, msgs, 2);
		CDBG("retry = %d, rc = %d", retry, rc);
		retry ++;
		mdelay(5);
	}while( ((2 != rc) && (I2C_RETRY_TIMES > retry)) );

	return rc;
}

static int ov8810_i2c_rxdata(unsigned short saddr,
	unsigned char *rxdata, int length)
{
	struct i2c_msg msgs[] = {
	{
		.addr  = saddr << 1,
		.flags = 0,
		.len   = 2,
		.buf   = rxdata,
	},
	{
		.addr  = saddr << 1,
		.flags = I2C_M_RD,
		.len   = length,
		.buf   = rxdata,
	},
	};

	if (i2c_transfer(ov8810_client->adapter, msgs, 2) < 0) {
		CDBG("ov8810_i2c_rxdata failed!\n");
		return -EIO;
	}

	return 0;
}
static int32_t ov8810_i2c_txdata(unsigned short saddr,
				unsigned char *txdata, int length)
{
	struct i2c_msg msg[] = {
		{
		 .addr = saddr << 1,
		 .flags = 0,
		 .len = length,
		 .buf = txdata,
		 },
	};

	if (i2c_transfer(ov8810_client->adapter, msg, 1) < 0) {
		CDBG("ov8810_i2c_txdata faild 0x%x\n", ov8810_client->addr);
		return -EIO;
	}

	return 0;
}


static int32_t ov8810_i2c_read(unsigned short raddr,
				unsigned short *rdata, int rlen)
{
	int32_t rc = 0;
	unsigned char buf[2];

	if (!rdata)
		return -EIO;

	memset(buf, 0, sizeof(buf));

	buf[0] = (raddr & 0xFF00) >> 8;
	buf[1] = (raddr & 0x00FF);

	rc = ov8810_i2c_rxdata(ov8810_client->addr, buf, rlen);

	if (rc < 0) {
		CDBG("ov8810_i2c_read 0x%x failed!\n", raddr);
		return rc;
	}

	*rdata = (rlen == 2 ? buf[0] << 8 | buf[1] : buf[0]);

	return rc;

}
static int32_t ov8810_i2c_write_b(unsigned short saddr,unsigned short waddr, uint8_t bdata)
{
	int32_t rc = -EFAULT;
	unsigned char buf[3];

	memset(buf, 0, sizeof(buf));
	buf[0] = (waddr & 0xFF00) >> 8;
	buf[1] = (waddr & 0x00FF);
	buf[2] = bdata;

	CDBG("i2c_write_b addr = 0x%x, val = 0x%xd\n", waddr, bdata);
	rc = ov8810_i2c_txdata(saddr, buf, 3);

	if (rc < 0) {
		pr_err("i2c_write_b failed, addr = 0x%x, val = 0x%x!\n",
			 waddr, bdata);
	}

	return rc;
}
static int32_t ov8810_af_i2c_write(uint16_t data)
{
	uint8_t code_val_msb, code_val_lsb;
	uint32_t rc = 0;
	code_val_msb = data >> 4;
	code_val_lsb = ((data & 0x000F) << 4) | S3_to_0;
	CDBG("code value = %d ,D[9:4] = %d ,D[3:0] = %d", data, code_val_msb, code_val_lsb);
	rc = ov8810_i2c_write_b(0x6C >>2 , OV8810_AF_MSB, code_val_msb);
	if (rc < 0)
	{
	CDBG("Unable to write code_val_msb = %d\n", code_val_msb);
	return rc;
	}
	rc = ov8810_i2c_write_b( 0x6C >>2, OV8810_AF_LSB, code_val_lsb);
	if ( rc < 0)
	{
	CDBG("Unable to write code_val_lsb = %disclaimer\n", code_val_lsb);
	return rc;
	}

	return rc;
}

static int32_t ov8810_move_focus( int direction,
	int32_t num_steps)
{

	int8_t step_direction;
	int8_t dest_step_position;
	uint16_t dest_lens_position, target_dist, small_step;
	int16_t next_lens_position;
	int32_t rc = 0;
	if (num_steps == 0)
	{
	return rc;
	}

	if ( direction == MOVE_NEAR )
	{
		step_direction = 1;
	}
	else if ( direction == MOVE_FAR)
	{
	step_direction = -1;
	}
	else
	{

	CDBG("Illegal focus direction\n");
	return -EINVAL;;
	}

	CDBG("interpolate/n");
	dest_step_position = ov8810_ctrl->curr_step_pos + (step_direction * num_steps);
	if (dest_step_position < 0)
	{
	dest_step_position = 0;
	}
	else if (dest_step_position > OV8810_TOTAL_STEPS_NEAR_TO_FAR)
	{

	dest_step_position = OV8810_TOTAL_STEPS_NEAR_TO_FAR;
	}

	dest_lens_position = ov8810_step_position_table[dest_step_position];

		/* Taking small damping steps */
		target_dist = step_direction * (dest_lens_position - ov8810_ctrl->curr_lens_pos);

	if (target_dist == 0) {
		return rc;
	}
	if(ov8810_use_threshold_damping && (step_direction < 0)
			&& (target_dist >= ov8810_step_position_table[ov8810_damping_threshold]))//change to variable
	{
	small_step = (uint16_t)((target_dist/ov8810_damping_fine_step));
	if (small_step == 0)
		small_step = 1;
	ov8810_damping_time_wait = 1;

	}
	else
	{
	small_step = (uint16_t)(target_dist/ov8810_damping_course_step);
	 if (small_step == 0)
		small_step = 1;
	ov8810_damping_time_wait = 4;

	}
	for (next_lens_position = ov8810_ctrl->curr_lens_pos + (step_direction * small_step);
	(step_direction * next_lens_position) <= (step_direction * dest_lens_position);
	next_lens_position += (step_direction * small_step))
	{
		if(ov8810_af_i2c_write(next_lens_position) < 0)
			return -EBUSY;;
		ov8810_ctrl->curr_lens_pos = next_lens_position;
		if(ov8810_ctrl->curr_lens_pos != dest_lens_position){
			mdelay(10);//mdelay(ov8810_damping_time_wait);//*1000
		}
	}

	if(ov8810_ctrl->curr_lens_pos != dest_lens_position) {
		if(ov8810_af_i2c_write(dest_lens_position) < 0) {
			return -EBUSY;
		}
				mdelay(10);
	}
	/* Storing the current lens Position */
	ov8810_ctrl->curr_lens_pos = dest_lens_position;
	ov8810_ctrl->curr_step_pos = dest_step_position;
	CDBG("done\n");
	return rc;
}
static int32_t ov8810_set_default_focus(uint8_t af_step)
{

	int16_t position;
	int32_t rc = 0;
	ov8810_damping_time_wait= 4;
	if(ov8810_use_default_damping) {
		/* when lens is uninitialized */
		if(ov8810_ctrl->curr_lens_pos == -1 || (ov8810_focus_debug == 1) ) {
			position = ov8810_step_position_table[ov8810_damping_threshold];
			rc =  ov8810_af_i2c_write(position);
			if (rc < 0)
				return rc;
			ov8810_ctrl->curr_step_pos = ov8810_damping_threshold;
			ov8810_ctrl->curr_lens_pos = position;
			mdelay(ov8810_damping_time_wait);;
		}
		ov8810_use_threshold_damping = 0;
		rc = ov8810_move_focus(MOVE_FAR, ov8810_ctrl->curr_step_pos);
		ov8810_use_threshold_damping = 1;
		if(rc < 0)
		return rc;
	}
	else {
	rc = ov8810_af_i2c_write(ov8810_step_position_table[0]);
	if ( rc < 0)
		return rc;
	ov8810_ctrl->curr_step_pos = 0;
	ov8810_ctrl->curr_lens_pos = ov8810_step_position_table[0];
	}
	return rc;
}
static void ov8810_get_pict_fps(uint16_t fps, uint16_t *pfps)
{
    uint32_t divider;	/*Q10 */
	uint32_t d1;
	uint32_t d2;
	uint16_t snapshot_height, preview_height, preview_width,snapshot_width;
    if (ov8810_ctrl->prev_res == QTR_SIZE) {
		preview_width = OV8810_QTR_SIZE_WIDTH  + OV8810_HRZ_QTR_BLK_PIXELS ;
		preview_height= OV8810_QTR_SIZE_HEIGHT + OV8810_VER_QTR_BLK_LINES ;
	}
	else {
		/* full size resolution used for preview. */
		preview_width = OV8810_FULL_SIZE_WIDTH + OV8810_HRZ_FULL_BLK_PIXELS ;
		preview_height= OV8810_FULL_SIZE_HEIGHT + OV8810_VER_FULL_BLK_LINES ;
	}
	if (ov8810_ctrl->pict_res == QTR_SIZE){
		snapshot_width  = OV8810_QTR_SIZE_WIDTH + OV8810_HRZ_QTR_BLK_PIXELS ;
		snapshot_height = OV8810_QTR_SIZE_HEIGHT + OV8810_VER_QTR_BLK_LINES ;
	}
	else {
		snapshot_width  = OV8810_FULL_SIZE_WIDTH + OV8810_HRZ_FULL_BLK_PIXELS;
		snapshot_height = OV8810_FULL_SIZE_HEIGHT + OV8810_VER_FULL_BLK_LINES;
	}

	d1 =
		(uint32_t)(
		(preview_height *
		0x00000400) /
		snapshot_height);

	d2 =
		(uint32_t)(
		(preview_width *
		0x00000400) /
		 snapshot_width);


	divider = (uint32_t) (d1 * d2) / 0x00000400;
	/* Verify PCLK settings and frame sizes. */
	*pfps = (uint16_t)(fps * divider / 0x00000400);
	/* input fps is preview fps in Q8 format */


}/*endof ov8810_get_pict_fps*/

static uint16_t ov8810_get_prev_lines_pf(void)
{
	if (ov8810_ctrl->prev_res == QTR_SIZE) {
		return (OV8810_QTR_SIZE_HEIGHT + OV8810_VER_QTR_BLK_LINES);
	} else {
		return (OV8810_FULL_SIZE_HEIGHT + OV8810_VER_FULL_BLK_LINES);
	}
}

static uint16_t ov8810_get_prev_pixels_pl(void)
{
	if (ov8810_ctrl->prev_res == QTR_SIZE) {
		return (OV8810_QTR_SIZE_WIDTH + OV8810_HRZ_QTR_BLK_PIXELS);
	}
	else{
		return (OV8810_FULL_SIZE_WIDTH + OV8810_HRZ_FULL_BLK_PIXELS);
	}
}
static uint16_t ov8810_get_pict_lines_pf(void)
{
	if (ov8810_ctrl->pict_res == QTR_SIZE) {
		return (OV8810_QTR_SIZE_HEIGHT + OV8810_VER_QTR_BLK_LINES);
	} else {
		return (OV8810_FULL_SIZE_HEIGHT + OV8810_VER_FULL_BLK_LINES);
	}
}
static uint16_t ov8810_get_pict_pixels_pl(void)
{
	if (ov8810_ctrl->pict_res == QTR_SIZE) {
		return (OV8810_QTR_SIZE_WIDTH + OV8810_HRZ_QTR_BLK_PIXELS);
	} else {
		return (OV8810_FULL_SIZE_WIDTH + OV8810_HRZ_FULL_BLK_PIXELS);
	}
}

static uint32_t ov8810_get_pict_max_exp_lc(void)
{
	if (ov8810_ctrl->pict_res == QTR_SIZE) {
		return (OV8810_QTR_SIZE_HEIGHT + OV8810_VER_QTR_BLK_LINES)*24;
	} else {
		return (OV8810_FULL_SIZE_HEIGHT + OV8810_VER_FULL_BLK_LINES)*24;
	}
}

static int32_t ov8810_set_fps(struct fps_cfg	*fps)
{
	int32_t rc = 0;
	ov8810_ctrl->fps_divider = fps->fps_div;
	ov8810_ctrl->pict_fps_divider = fps->pict_fps_div;
	ov8810_ctrl->fps = fps->f_mult;
	return rc;
}
static int32_t ov8810_write_exp_gain(uint16_t gain, uint32_t line)
{
	uint16_t aec_msb;
	uint16_t aec_lsb;
	int32_t rc = 0;
	uint32_t total_lines_per_frame;
	uint32_t total_pixels_per_line;
	uint32_t line_length_ratio = 1 * Q8;
	uint8_t ov8810_offset = 2;
	CDBG("%s,%d: gain: %d: Linecount: %d\n",__func__,__LINE__,gain,line);

	if (ov8810_ctrl->curr_res == QTR_SIZE) {
		total_lines_per_frame = (OV8810_QTR_SIZE_HEIGHT+ OV8810_VER_QTR_BLK_LINES);
		total_pixels_per_line= OV8810_QTR_SIZE_WIDTH + OV8810_HRZ_QTR_BLK_PIXELS;

	}
	else {
		total_lines_per_frame = (OV8810_FULL_SIZE_HEIGHT + OV8810_VER_FULL_BLK_LINES);
		total_pixels_per_line= OV8810_FULL_SIZE_WIDTH + OV8810_HRZ_FULL_BLK_PIXELS;

	}


	if ((total_lines_per_frame - ov8810_offset) < line) {
		line_length_ratio = ((line * Q8) / (total_lines_per_frame - ov8810_offset));
		line = total_lines_per_frame - ov8810_offset;
		CDBG("%s,Line:%d: line \n",__func__,__LINE__);

	}
	else {
		CDBG("%s,%d: gain: %d: Linecount: %d\n",__func__,__LINE__,gain,line);
		line_length_ratio = 1 * Q8;
	}
	CDBG("%s,%dline length ratio is %d\n",__func__,__LINE__,line_length_ratio);
	aec_msb = (uint16_t)(line & 0xFF00) >> 8;
	aec_lsb = (uint16_t)(line & 0x00FF);
	CDBG("%s,%dline length ratio is %d\n",__func__,__LINE__,line_length_ratio);
	if (line_length_ratio != stored_line_length_ratio || (ov8810_ctrl->sensormode == SENSOR_SNAPSHOT_MODE)) {
			CDBG("%s,%dline length ratio is %d\n",__func__,__LINE__,line_length_ratio);
		total_pixels_per_line = total_pixels_per_line*line_length_ratio / Q8;
			CDBG("%s,%dTotal pixels per line is %d\n",__func__,__LINE__,total_pixels_per_line);
		rc = ov8810_i2c_write_b(ov8810_client->addr, 0x3022, ((total_pixels_per_line & 0xFF00) >> 8));
		if (rc < 0) {
			return rc;
		}
		rc = ov8810_i2c_write_b(ov8810_client->addr,0x3023, (total_pixels_per_line & 0x00FF));
		if (rc < 0) {
			return rc;
		}
	}

	if(panorama_mode == 0)
	{
		snapshot_aec_msb = aec_msb;
		snapshot_aec_lsb = 	aec_lsb;
		snapshot_gain = gain;
	}

	CDBG("panorama_mode = %d\n", panorama_mode);

	if (ov8810_ctrl->sensormode  == SENSOR_PREVIEW_MODE)
	{
		mdelay(10);
		rc = ov8810_i2c_write_b(ov8810_client->addr, OV8810_GROUP_LATCH, 0x8C);
		if (rc < 0) {
			return rc;
		}
		rc = ov8810_i2c_write_b(ov8810_client->addr, OV8810_AEC_MSB, (uint8_t)aec_msb);
		if (rc < 0) {
			return rc;
		}
		rc = ov8810_i2c_write_b(ov8810_client->addr, OV8810_AEC_LSB, (uint8_t)aec_lsb);
		if(rc < 0)
			return rc;
		rc = ov8810_i2c_write_b(ov8810_client->addr, OV8810_GAIN, (uint8_t)gain);
		if (rc < 0)
			return rc;

		rc = ov8810_i2c_write_b(ov8810_client->addr, OV8810_GROUP_LATCH, 0x84);
		if (rc < 0) {
			return rc;
		}

		rc = ov8810_i2c_write_b(ov8810_client->addr, 0x30ff, 0xff);
		if (rc < 0) {
			return rc;
		}
	}
	else
	{
		printk("snapshot_aec_msb = %d, snapshot_aec_lsb = %d, snapshot_gain = %d\n", snapshot_aec_msb, snapshot_aec_lsb, snapshot_gain);
		rc = ov8810_i2c_write_b(ov8810_client->addr, OV8810_AEC_MSB, (uint8_t)snapshot_aec_msb);
		if (rc < 0) {
			return rc;
		}
		rc = ov8810_i2c_write_b(ov8810_client->addr, OV8810_AEC_LSB, (uint8_t)snapshot_aec_lsb);
		if(rc < 0)
			return rc;
		rc = ov8810_i2c_write_b(ov8810_client->addr, OV8810_GAIN, (uint8_t)snapshot_gain);
		if (rc < 0)
			return rc;
	}
	stored_line_length_ratio = line_length_ratio;
	return rc;
}/* endof ov8810_write_exp_gain*/
static int32_t ov8810_set_pict_exp_gain(uint16_t gain, uint32_t line)
{
	int32_t rc = 0;
	rc = ov8810_write_exp_gain(gain, line);
	return rc;
}/* endof ov8810_set_pict_exp_gain*/
static int32_t ov8810_test(enum ov8810_test_mode_t mo)
{
	int32_t rc = 0;
	if (mo == TEST_OFF){
		return rc;
	}
	/* Activate  the Color bar test pattern */
	if(mo == TEST_1) {

	rc = ov8810_i2c_write_b(ov8810_client->addr,0x3303, 0x01);
	if (rc < 0){
		return rc;
	}
	rc = ov8810_i2c_write_b(ov8810_client->addr, 0x3316, 0x02);
	if (rc < 0 ){
		return rc;
	}
	}
	return rc;
}

static int ov8810_read_lenc_data_from_eeprom(void)
{


	int rc = 0;
	unsigned char curre_reg = 0x00;
	int lenc_index = 0;
	int len= 0;
	int i = 0;

	len = sizeof(ov8810_module_serial_info);
	rc = ov8810_read_eeprom(ABICO_EEPROM_I2C_ADDR_BANK1, curre_reg, ov8810_module_serial_info, len);

	if (rc < 0) {
		if ((system_rev >= TOUCAN_DVT1_Band125) )
		{		
			pr_err("ov8810_read_lenc_data_from_eeprom sensor info failed!, system_rev = %d\n", system_rev);
			return rc;

		}	
		else
		{
			pr_err("HW is older version. ov8810_read_lenc_data_from_eeprom sensor info failed!, system_rev = %d\n", system_rev);
			rc = 0;
			return rc;
		}
	}
	ov8810_module_vendor_id = ov8810_module_serial_info[0];

	if (ov8810_module_vendor_id == ABICO_MODULE_VENDOR_ID)
		rc = 0;
	else
		rc = -1;
	if (rc < 0)
		goto check_done;
#ifdef DEBUG_OV8810
	for(i = 0;i<OV8810_MODULE_SERIAL_INFO_LEN;i++)
	{
		printk("ov8810_module_serial_info[%d] = 0x%x\n",i,  ov8810_module_serial_info[i]);
	}
#endif 	//DEBUG_OV8810
	curre_reg = OV8810_MODULE_SERIAL_INFO_LEN;
	len = (ABICO_EEPROM_PER_BANK_LEN - OV8810_MODULE_SERIAL_INFO_LEN);
	rc = ov8810_read_eeprom(ABICO_EEPROM_I2C_ADDR_BANK1, curre_reg, ov8810_lensc_awb_parameters,  len);

	if (rc < 0) {
		pr_err("ov8810_read_lenc_data_from_eeprom lens correctin bank 1 failed!\n");
		return rc;
	}
	//Read next bank
	curre_reg = 0x00;
	lenc_index = (ABICO_EEPROM_PER_BANK_LEN - OV8810_MODULE_SERIAL_INFO_LEN);
	len = (sizeof(ov8810_lensc_awb_parameters) -(ABICO_EEPROM_PER_BANK_LEN - OV8810_MODULE_SERIAL_INFO_LEN));
	rc = ov8810_read_eeprom(ABICO_EEPROM_I2C_ADDR_BANK2, curre_reg, ov8810_lensc_awb_parameters+lenc_index,  len);

	if (rc < 0) {
		pr_err("ov8810_read_lenc_data_from_eeprom lens correctin bank 2 failed!\n");
		return rc;
	}
#ifdef DEBUG_OV8810
	for(i = 0;i<sizeof(ov8810_lensc_awb_parameters) ;i++)
	{
		printk("ov8810_lensc_awb_parameters[%d] = 0x%x\n",i,  ov8810_lensc_awb_parameters[i]);
	}
#endif 	//DEBUG_OV8810
	//Check the lens correction parameters whether vaild
	for ( i = 0; i <LENS_AWB_MAX_LIGHT ; i++ )
	{
		int32_t index = i*OV8810_EACH_LENSC_LEN;

		//Check ov8810_lensc_awb_parameters whether has right format
		if( 	(ov8810_lensc_awb_parameters[index] != 0x06) ||
			(ov8810_lensc_awb_parameters[index+1] != 0xab) ||
			(ov8810_lensc_awb_parameters[index+2] != 0x05)  ||
			(ov8810_lensc_awb_parameters[index+3] != 0x00)  ||
			(ov8810_lensc_awb_parameters[index+4] != 0x04) ||
			(ov8810_lensc_awb_parameters[index+5] != 0xf8) ||
			(ov8810_lensc_awb_parameters[index+6] != 0x07) ||
			(ov8810_lensc_awb_parameters[index+7] != 0x74) )
		{
			pr_err("ov8810_lensc_awb_parameters format does not match OV Lenc settings");
			is_lens_correction_data_vaild = 0;
			return -1;
		}
	}
	is_lens_correction_data_vaild = 1;

check_done:
	return rc;
}

static void ov8810_update_lenc_accord_awb_light_type(lens_correction_awb_light_type new_awb_light_type)
{
	int32_t i, array_length;
	int32_t index = new_awb_light_type*OV8810_EACH_LENSC_LEN;

	pr_err("ov8810_update_lenc_accord_awb_light_type new_awb_light_type = %d", new_awb_light_type);

	array_length = sizeof(ov8810_lenc_settings_array) /
					sizeof(ov8810_lenc_settings_array[0]);
	//Check ov8810_lensc_awb_parameters whether has right format
	if( 	(ov8810_lensc_awb_parameters[index] != 0x06) ||
		(ov8810_lensc_awb_parameters[index+1] != 0xab) ||
		(ov8810_lensc_awb_parameters[index+2] != 0x05)  ||
		(ov8810_lensc_awb_parameters[index+3] != 0x00)  ||
		(ov8810_lensc_awb_parameters[index+4] != 0x04) ||
		(ov8810_lensc_awb_parameters[index+5] != 0xf8) ||
		(ov8810_lensc_awb_parameters[index+6] != 0x07) ||
		(ov8810_lensc_awb_parameters[index+7] != 0x74) )
	{
		pr_err("ov8810_lensc_awb_parameters format does not match OV Lenc settings");
		return;
	}
	for (i=0; i<array_length; i++)
	{
		ov8810_lenc_settings_array[i].reg_val = ov8810_lensc_awb_parameters[index+i];
	}
}

static int32_t ov8810_set_lc(void)
{
	int32_t rc = 0;
	int32_t i, array_length;

	array_length = sizeof(ov8810_lenc_settings_array) /
					sizeof(ov8810_lenc_settings_array[0]);
	CDBG("ov8810_set_lc  array_length= %d\n", array_length);
	for (i=0; i<array_length; i++)
	{
		rc = ov8810_i2c_write_b(ov8810_client->addr, ov8810_lenc_settings_array[i].reg_addr,
			ov8810_lenc_settings_array[i].reg_val);
		if ( rc < 0)
			return rc;
	}

	CDBG("ov8810_set_lc\n");

	return rc;
}
static int32_t ov8810_lens_shading_enable(uint8_t is_enable)
{
	int32_t rc = 0;
	uint8_t lenc_reg = 0xef;

	CDBG("%s: entered. enable = %d\n", __func__, is_enable);


	lenc_reg |= (is_enable <<4);
	CDBG("%s: entered. lenc_reg = 0x%x\n", __func__, lenc_reg);


	rc = ov8810_i2c_write_b(ov8810_client->addr, OV8810_LENC, lenc_reg );
	if (rc < 0) {
		return rc;
	}
	CDBG("%s: entered. enable = %d done\n", __func__, is_enable);

	//Update lens correction register
	if (is_enable == 1)
	{
		rc = ov8810_set_lc();
	}
	return rc;
}

static int32_t initialize_ov8810_registers(void)
{
	int32_t i, array_length;
	int32_t rc = 0;

	ov8810_ctrl->sensormode = SENSOR_PREVIEW_MODE ;
	array_length = sizeof(ov8810_init_settings_array) /
	sizeof(ov8810_init_settings_array[0]);
	/* Configure sensor for Preview mode and Snapshot mode */
	for (i=0; i<array_length; i++)
	{
		rc = ov8810_i2c_write_b(ov8810_client->addr, ov8810_init_settings_array[i].reg_addr,
				ov8810_init_settings_array[i].reg_val);
		if ( rc < 0) {
			return rc;
		}
	}
	rc = ov8810_set_lc();

	return rc;
} /* end of initialize_ov8810_ov8m0vc_registers. */
static int32_t ov8810_setting( int rt)
{
	int32_t rc = 0;
	int32_t i, array_length;
	uint16_t  checkRegVal;
	rc = ov8810_i2c_write_b(ov8810_client->addr,0x30fa, 0x00);
	if(rc < 0) {
		return rc;
	}
	switch(rt)
	{
		case QTR_SIZE:
			array_length = sizeof(ov8810_qtr_settings_array) /
					sizeof(ov8810_qtr_settings_array[0]);
			/* Configure sensor for XGA preview mode */
			for (i=0; i<array_length; i++)
			{
			rc = ov8810_i2c_write_b(ov8810_client->addr, ov8810_qtr_settings_array[i].reg_addr,
					ov8810_qtr_settings_array[i].reg_val);
			if ( rc < 0) {
				return rc;
			}
			}
			ov8810_ctrl->curr_res = QTR_SIZE;
			break;
		case FULL_SIZE:
			if ( rc < 0)
				return rc;
			array_length = sizeof(ov8810_full_settings_array) /
				sizeof(ov8810_full_settings_array[0]);
			/* Configure sensor for QXGA capture mode */
			for (i=0; i<array_length; i++)
			{
				do
				{
				rc = ov8810_i2c_write_b(ov8810_client->addr, ov8810_full_settings_array[i].reg_addr,
						ov8810_full_settings_array[i].reg_val);
			if ( rc < 0)
				return rc;
					rc = ov8810_i2c_read( ov8810_full_settings_array[i].reg_addr, &checkRegVal,1);
					if ( rc < 0)
						return rc;				
					if ( checkRegVal !=  ov8810_full_settings_array[i].reg_val)
					{
						printk("ov8810_full_settings_array not vaild in ov8810_full_settings_array[%d].reg_val = 0x%x, checkRegVal = 0x%x", i, ov8810_full_settings_array[i].reg_val, checkRegVal);
						mdelay(10);
					}					
				}while(checkRegVal !=  ov8810_full_settings_array[i].reg_val);				
			}
			ov8810_ctrl->curr_res = FULL_SIZE;
			break;
		default:
			rc = -EFAULT;
			return rc;
	}
	rc = ov8810_i2c_write_b(ov8810_client->addr,0x30fa, 0x01);
	if (rc < 0)
		return rc;
	rc = ov8810_test(ov8810_ctrl->set_test);
	if ( rc < 0)
		return rc;
	return rc;
} /*endof  ov8810_setting*/
static int32_t ov8810_video_config(int mode )
{
	int32_t rc = 0;
	if( ov8810_ctrl->curr_res != ov8810_ctrl->prev_res){
		rc = ov8810_setting(ov8810_ctrl->prev_res);
		if (rc < 0)
			return rc;
	}
	else {
		ov8810_ctrl->curr_res = ov8810_ctrl->prev_res;
	}
	ov8810_ctrl->sensormode = mode;
	return rc;
}/*end of ov354_video_config*/

static int32_t ov8810_snapshot_config(int mode)
{
	int32_t rc = 0;
	if (ov8810_ctrl->curr_res != ov8810_ctrl->pict_res) {
		rc = ov8810_setting(ov8810_ctrl->pict_res);
		if (rc < 0)
			return rc;
	}
	else {
		ov8810_ctrl->curr_res = ov8810_ctrl->pict_res;
	}
	ov8810_ctrl->sensormode = mode;
	return rc;
}/*end of ov8810_snapshot_config*/

static int32_t ov8810_raw_snapshot_config(int mode)
{
	int32_t rc = 0;
	ov8810_ctrl->sensormode = mode;
	if (ov8810_ctrl->curr_res != ov8810_ctrl->pict_res){
	rc = ov8810_setting(ov8810_ctrl->pict_res);
	if (rc < 0)
		return rc;
	}
	else {
		ov8810_ctrl->curr_res = ov8810_ctrl->pict_res;
	}/* Update sensor resolution */
	ov8810_ctrl->sensormode = mode;
	return rc;
}/*end of ov8810_raw_snapshot_config*/
static int32_t ov8810_set_sensor_mode(int  mode,
			int  res)
{
	int32_t rc = 0;
	switch (mode) {
	case SENSOR_PREVIEW_MODE:
		rc = ov8810_video_config(mode);
		rc = ov8810_set_default_focus(0);
		break;
	case SENSOR_SNAPSHOT_MODE:
		rc = ov8810_snapshot_config(mode);
		break;
	case SENSOR_RAW_SNAPSHOT_MODE:
		rc = ov8810_raw_snapshot_config(mode);
		break;

	default:
		rc = -EINVAL;
		break;
	}
	return rc;
}
static int32_t ov8810_power_down(void)
{
	return 0;
}
static int ov8810_probe_init_sensor(const struct msm_camera_sensor_info *data)
{
	int32_t rc = 0;
	uint16_t  chipidl, chipidh;

	rc = gpio_request(data->sensor_pwd, "ov8810_abico");
	if (!rc)
    {
        msleep(5);
		gpio_direction_output(data->sensor_pwd, 0);
    }
	else
		goto init_probe_done;
    msleep(3);


	rc = gpio_request(data->sensor_reset, "ov8810_abico");
	if (!rc)
		gpio_direction_output(data->sensor_reset, 1);
	else
		goto init_probe_fail_2;
	mdelay(20);

	if (ov8810_i2c_read(OV8810_PIDH_REG, &chipidh,1) < 0)
		goto init_probe_fail;
	if (ov8810_i2c_read(OV8810_PIDL_REG, &chipidl,1) < 0)
		goto init_probe_fail;

	ov8810_sersor_version = (chipidh << 8) | chipidl;
	printk("ov8810 model_id = 0x%x  0x%x \n", chipidh, chipidl );

	if (chipidh != OV8810_PID || chipidl != OV8810_VER) {
		rc = -ENODEV;
		pr_err("Compare sensor ID to OV8810 ID FAIL");
		goto init_probe_fail;
	}

	mdelay(OV8810_RESET_DELAY_MSECS);
  goto init_probe_done;
init_probe_fail:
	gpio_direction_output(data->sensor_reset, 0);
	gpio_free(data->sensor_reset);
init_probe_fail_2:
	gpio_direction_output(data->sensor_pwd, 1);
	gpio_free(data->sensor_pwd);
init_probe_done:
	CDBG(" ov8810_probe_init_sensor finishes\n");
	return rc;
}
static int ov8810_sensor_open_init(const struct msm_camera_sensor_info *data)
{
	int i;
	int32_t  rc;
	uint16_t ov8810_nl_region_boundary = 3;
	uint16_t ov8810_nl_region_code_per_step = 101;
	uint16_t ov8810_l_region_code_per_step = 18;
	CDBG("Calling ov8810_sensor_open_init\n");
	ov8810_ctrl = kzalloc(sizeof(struct ov8810_ctrl_t), GFP_KERNEL);
	if (!ov8810_ctrl) {
		CDBG("ov8810_init failed!\n");
		rc = -ENOMEM;
		goto init_done;
	}
	ov8810_ctrl->curr_lens_pos = -1;
	ov8810_ctrl->fps_divider = 1 * 0x00000400;
	ov8810_ctrl->pict_fps_divider = 1 * 0x00000400;
	ov8810_ctrl->set_test = TEST_OFF;
	ov8810_ctrl->prev_res = QTR_SIZE;
	ov8810_ctrl->pict_res = FULL_SIZE;
	ov8810_ctrl->curr_res = INVALID_SIZE;
	if (data)
		ov8810_ctrl->sensordata = data;
	/* enable mclk first */
	msm_camio_clk_rate_set(OV8810_DEFAULT_CLOCK_RATE);
	mdelay(20);
	msm_camio_camif_pad_reg_reset();
	mdelay(20);
	rc = ov8810_probe_init_sensor(data);
	if (rc < 0)
		goto init_fail;
	/* Initialize Sensor registers */
	rc = initialize_ov8810_registers();
	if (rc < 0) {
		return rc;
	}
	if( ov8810_ctrl->curr_res != ov8810_ctrl->prev_res) {
		rc = ov8810_setting(ov8810_ctrl->prev_res);
		if (rc < 0) {
			return rc;
		}
	}
	else {
		 ov8810_ctrl->curr_res = ov8810_ctrl->prev_res;
		}
	ov8810_ctrl->fps = 30*Q8;
	step_position_table[0] = 0;
	for(i = 1; i <= OV8810_TOTAL_STEPS_NEAR_TO_FAR; i++)
	{
		if ( i <= ov8810_nl_region_boundary) {
			ov8810_step_position_table[i] = ov8810_step_position_table[i-1] + ov8810_nl_region_code_per_step;
		}
		else {
			ov8810_step_position_table[i] = ov8810_step_position_table[i-1] + ov8810_l_region_code_per_step;
		}
	}
	 /* generate test pattern */
	if (rc < 0)
		goto init_fail;
	else
		goto init_done;
	/* reset the driver state */
	init_fail:
		CDBG(" init_fail \n");
		kfree(ov8810_ctrl);
	init_done:
		CDBG("init_done \n");
		return rc;
}/*endof ov8810_sensor_open_init*/
static int ov8810_init_client(struct i2c_client *client)
{
	/* Initialize the MSM_CAMI2C Chip */
	init_waitqueue_head(&ov8810_wait_queue);
	return 0;
}

static const struct i2c_device_id ov8810_i2c_id[] = {
	{ "ov8810_abico", 0},
	{ }
};

static int ov8810_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	int rc = 0;
	CDBG("ov8810_probe called!\n");
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		CDBG("i2c_check_functionality failed\n");
		goto probe_failure;
	}
	ov8810_sensorw = kzalloc(sizeof(struct ov8810_work_t), GFP_KERNEL);
	if (!ov8810_sensorw) {
		CDBG("kzalloc failed.\n");
		rc = -ENOMEM;
		goto probe_failure;
	}
	i2c_set_clientdata(client, ov8810_sensorw);
	ov8810_init_client(client);
	ov8810_client = client;
	mdelay(50);
	CDBG("ov8810_probe successed! rc = %d\n", rc);
	return 0;
probe_failure:
	CDBG("ov8810_probe failed! rc = %d\n", rc);
	return rc;
}

static int __exit ov8810_remove(struct i2c_client *client)
{
	struct ov8810_work_t_t *sensorw = i2c_get_clientdata(client);
	free_irq(client->irq, sensorw);
	ov8810_client = NULL;
	kfree(sensorw);
	return 0;
}

static struct i2c_driver ov8810_i2c_driver = {
	.id_table = ov8810_i2c_id,
	.probe	= ov8810_i2c_probe,
	.remove = __exit_p(ov8810_i2c_remove),
	.driver = {
		.name = "ov8810_abico",
	},
};

static int ov8810_sensor_config(void __user *argp)
{
	struct sensor_cfg_data cdata;
	long   rc = 0;

	if (copy_from_user(&cdata,
				(void *)argp,
				sizeof(struct sensor_cfg_data)))
		return -EFAULT;
	down(&ov8810_sem_abico);
	CDBG("ov8810_sensor_config: cfgtype = %d\n",
		cdata.cfgtype);
		switch (cdata.cfgtype) {
		case CFG_GET_PICT_FPS:
				ov8810_get_pict_fps(
				cdata.cfg.gfps.prevfps,
				&(cdata.cfg.gfps.pictfps));
			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;
		case CFG_GET_PREV_L_PF:
			cdata.cfg.prevl_pf =
			ov8810_get_prev_lines_pf();
			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;
		case CFG_GET_PREV_P_PL:
			cdata.cfg.prevp_pl =
				ov8810_get_prev_pixels_pl();
			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;
		case CFG_GET_PICT_L_PF:
			cdata.cfg.pictl_pf =
				ov8810_get_pict_lines_pf();
			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;
		case CFG_GET_PICT_P_PL:
			cdata.cfg.pictp_pl =
				ov8810_get_pict_pixels_pl();
			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;
		case CFG_GET_PICT_MAX_EXP_LC:
			cdata.cfg.pict_max_exp_lc =
				ov8810_get_pict_max_exp_lc();
			if (copy_to_user((void *)argp,
				&cdata,
				sizeof(struct sensor_cfg_data)))
				rc = -EFAULT;
			break;
		case CFG_SET_FPS:
		case CFG_SET_PICT_FPS:
			rc = ov8810_set_fps(&(cdata.cfg.fps));
			break;
		case CFG_SET_EXP_GAIN:
			rc =
				ov8810_write_exp_gain(
					cdata.cfg.exp_gain.gain,
					cdata.cfg.exp_gain.line);
			break;
		case CFG_SET_PICT_EXP_GAIN:
			rc =
				ov8810_set_pict_exp_gain(
					cdata.cfg.exp_gain.gain,
					cdata.cfg.exp_gain.line);
			break;
		case CFG_SET_MODE:
			rc = ov8810_set_sensor_mode(cdata.mode,
						cdata.rs);
			break;
		case CFG_PWR_DOWN:
			rc = ov8810_power_down();
			break;
		case CFG_MOVE_FOCUS:
			rc =
				ov8810_move_focus(
					cdata.cfg.focus.dir,
					cdata.cfg.focus.steps);
			break;
		case CFG_SET_DEFAULT_FOCUS:
			rc =
				ov8810_set_default_focus(
					cdata.cfg.focus.steps);
			break;
		case CFG_SET_EFFECT:
			rc = ov8810_set_default_focus(
						cdata.cfg.effect);
			break;

		case CFG_SET_LENS_SHADING:
			printk("%s: CFG_SET_LENS_SHADING\n", __func__);
			rc = ov8810_lens_shading_enable(cdata.cfg.lens_shading);
			break;

		case CFG_GET_WB_GAINS:
			printk("%s: CFG_GET_WB_GAINS\n", __func__);
	 	 	cdata.cfg.wb_gains.day_g_gain = 0;
	  		cdata.cfg.wb_gains.day_r_gain = 0;
	  		cdata.cfg.wb_gains.day_b_gain = 0; 
	 	 	cdata.cfg.wb_gains.tl_g_gain = 	0;
	  		cdata.cfg.wb_gains.tl_r_gain = 	0;
	  		cdata.cfg.wb_gains.tl_b_gain = 	0; 
	 	 	cdata.cfg.wb_gains.a_g_gain = 	0;
	  		cdata.cfg.wb_gains.a_r_gain = 	0;
	  		cdata.cfg.wb_gains.a_b_gain = 	0; 			

			if ( (ov8810_lensc_awb_parameters[OV8810_THREE_LENSC_LEN] > 0 ) 
				&& (ov8810_lensc_awb_parameters[OV8810_THREE_LENSC_LEN] < 255))
			{
				cdata.cfg.wb_gains.day_r_gain = (uint32_t) ov8810_lensc_awb_parameters[OV8810_THREE_LENSC_LEN]; 
			}

			if ( (ov8810_lensc_awb_parameters[OV8810_THREE_LENSC_LEN+1] >0) 
				&& (ov8810_lensc_awb_parameters[OV8810_THREE_LENSC_LEN+1] < 255))
			{
				cdata.cfg.wb_gains.day_b_gain = (uint32_t) ov8810_lensc_awb_parameters[OV8810_THREE_LENSC_LEN+1]; 
			}

			if ( (ov8810_lensc_awb_parameters[OV8810_THREE_LENSC_LEN+2] >0 ) 
				&&  (ov8810_lensc_awb_parameters[OV8810_THREE_LENSC_LEN+2] < 255 ))
			{
				cdata.cfg.wb_gains.tl_r_gain = (uint32_t) ov8810_lensc_awb_parameters[OV8810_THREE_LENSC_LEN+2]; 
			}

			if ( (ov8810_lensc_awb_parameters[OV8810_THREE_LENSC_LEN+3] >0 ) 
				&&  (ov8810_lensc_awb_parameters[OV8810_THREE_LENSC_LEN+3] < 255 ))
			{
				cdata.cfg.wb_gains.tl_b_gain = (uint32_t) ov8810_lensc_awb_parameters[OV8810_THREE_LENSC_LEN+3]; 
			}			

			if ( (ov8810_lensc_awb_parameters[OV8810_THREE_LENSC_LEN+4] >0 ) 
				&&  (ov8810_lensc_awb_parameters[OV8810_THREE_LENSC_LEN+4] < 255 ))
			{
				cdata.cfg.wb_gains.a_r_gain  =  (uint32_t) ov8810_lensc_awb_parameters[OV8810_THREE_LENSC_LEN+4]; 
			}
			if ( (ov8810_lensc_awb_parameters[OV8810_THREE_LENSC_LEN+5] >0 ) 
				&&  (ov8810_lensc_awb_parameters[OV8810_THREE_LENSC_LEN+5] < 255))
			{
				cdata.cfg.wb_gains.a_b_gain  =  (uint32_t) ov8810_lensc_awb_parameters[OV8810_THREE_LENSC_LEN+5]; 
			}

     			if (copy_to_user((void *)argp, &cdata,
			  		sizeof(struct sensor_cfg_data)))
		  		rc = -EFAULT;
			printk("%s: CFG_GET_WB_GAINS done\n", __func__);
	  		break;

		case CFG_SET_LENS_LIGHT_TYPE:

			if( is_lens_correction_data_vaild == 1)
			{
				printk("CFG_SET_LENS_LIGHT_TYPE cfg.cfg.lenc_light_type = %d", cdata.cfg.lenc_light_type);
				//Map
				if(cdata.cfg.lenc_light_type == TL84_LC)
					ov8810_curr_lenc_light_type = LENS_AWB_INDOOR_WARM_FLO;
				else if (cdata.cfg.lenc_light_type == D65_LC)
					ov8810_curr_lenc_light_type = LENS_AWB_OUTDOOR_SUNLIGHT;
				else if (cdata.cfg.lenc_light_type == A_LC)
					ov8810_curr_lenc_light_type = LENS_AWB_INDOOR_INCANDESCENT;

				ov8810_update_lenc_accord_awb_light_type(ov8810_curr_lenc_light_type);

				rc = ov8810_set_lc();
			}
			break;
		default:
			rc = -EFAULT;
			break;
		}
	up(&ov8810_sem_abico);
	return rc;
}

static int ov8810_sensor_release(void)
{
	int rc = -EBADF;
	down(&ov8810_sem_abico);
	ov8810_power_down();
	gpio_direction_output(ov8810_ctrl->sensordata->sensor_reset,
		0);
	gpio_free(ov8810_ctrl->sensordata->sensor_reset);

	gpio_free(ov8810_ctrl->sensordata->sensor_pwd);

	kfree(ov8810_ctrl);
	ov8810_ctrl = NULL;
	CDBG("ov8810_release completed\n");
	up(&ov8810_sem_abico);
	return rc;
}

static int ov8810_sensor_probe(const struct msm_camera_sensor_info *info,
		struct msm_sensor_ctrl *s)
{
	int rc = 0;

	rc = i2c_add_driver(&ov8810_i2c_driver);
	if (rc < 0 || ov8810_client == NULL) {
		rc = -ENOTSUPP;
		goto probe_fail;
	}

	rc = ov8810_read_lenc_data_from_eeprom();
	if (rc < 0)
	{
		pr_err("ov8810_read_lenc_data_from_eeprom FAIL or check module vendor fail");
		goto probe_fail;
	}
	else
	{
		CDBG("is_lens_correction_data_vaild = %d\n", is_lens_correction_data_vaild);
		if (is_lens_correction_data_vaild == 1)
		{
			ov8810_update_lenc_accord_awb_light_type(ov8810_curr_lenc_light_type);
		}
	}
#if 0
	msm_camio_clk_rate_set(24000000);
	mdelay(20);
	rc = ov8810_probe_init_sensor(info);
	if (rc < 0)
		goto probe_fail;
#endif
	s->s_init = ov8810_sensor_open_init;
	s->s_release = ov8810_sensor_release;
	s->s_config  = ov8810_sensor_config;
#if 0
	ov8810_probe_init_done(info);
#endif
	return rc;

probe_fail:
	CDBG("SENSOR PROBE FAILS!\n");
	return rc;
}

static int __ov8810_probe(struct platform_device *pdev)
{
	int rc = 0;
	printk(KERN_INFO "BootLog, +%s\n", __func__);
	if(ov8810_camera_module_probe_status == 0)
	{
		rc = msm_camera_drv_start(pdev, ov8810_sensor_probe);
	}
	if (rc == 0)
		ov8810_camera_module_probe_status =1;
	else
		ov8810_camera_module_probe_status =0;
	printk(KERN_INFO "BootLog, -%s, ret=%d, ov8810_camera_module_probe_status = %d\n", __func__, rc, ov8810_camera_module_probe_status);

	return rc;
}

static struct platform_driver msm_camera_driver = {
	.probe = __ov8810_probe,
	.driver = {
		.name = "msm_camera_ov8810_abico",
		.owner = THIS_MODULE,
	},
};

static int __init ov8810_abico_init(void)
{
	int rc = 0;
	printk(KERN_INFO "BootLog, +%s\n", __func__);
	rc = platform_driver_register(&msm_camera_driver);
	printk(KERN_INFO "BootLog, -%s, ret=%d\n", __func__, rc);

	return rc;
}

module_init(ov8810_abico_init);
void ov8810_abico_exit(void)
{
	i2c_del_driver(&ov8810_i2c_driver);
}

