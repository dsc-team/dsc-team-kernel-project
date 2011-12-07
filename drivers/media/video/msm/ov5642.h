#ifndef __VIDEO_MSM_OV5642_SENSOR_H
#define __VIDEO_MSM_OV5642_SENSOR_H

#include <linux/delay.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <media/msm_camera.h>
#include <mach/gpio.h>

#include <mach/camera.h>
#include <mach/austin_hwid.h>

#define USE_AF
#define USE_GROUP_LATCH
#define OV5642_SLAVE_ADDR (0x78 >> 1)

#define OV5642_PIDH_ADDR 0x300A
#define OV5642_PIDL_ADDR 0x300B
#define OV5642_PIDH_DEFAULT_VALUE 0x56
#define OV5642_PIDL_DEFAULT_VALUE 0x42

#define OV5642_BIT_OPERATION_TAG 0xFFFF

struct ov5642_reg_t {
    unsigned short addr;
    unsigned char  data;
};

struct ov5642_reg_group_t {
    struct ov5642_reg_t *start;
    uint32_t            len;
};

struct ov5642_reg_burst_group_t {
    char     *start;
    uint32_t len;
};


enum ov5642_frame_rate_t {
    ONE_OF_ONE_FPS,
    ONE_OF_THREE_FPS,
    ONE_OF_FIVE_FPS,
};

enum ov5642_ae_weighting_t {
    GENERAL_AE,
    PORTRAIT_AE,
    LANDSCAPE_AE,
};

enum ov5642_color_matrix_t {
    GENERAL_CMX,
    PERSON_CMX,
    LAND_CMX,
    SNOW_CMX,
};

enum ov5642_anti_banding_t {
    AUTO_BANDING,
    MANUAL_50HZ_BANDING,
    MANUAL_60HZ_BANDING,
};

enum ov5642_brightness_t {
    BRIGHTNESS_POSITIVE_TWO,
    BRIGHTNESS_POSITIVE_ONE,
    BRIGHTNESS_POSITIVE_ZERO,
    BRIGHTNESS_NEGATIVE_ONE,
    BRIGHTNESS_NEGATIVE_TWO,
};

enum ov5642_contrast_t {
    CONTRAST_POSITIVE_TWO,
    CONTRAST_POSITIVE_ONE,
    CONTRAST_POSITIVE_ZERO,
    CONTRAST_NEGATIVE_ONE,
    CONTRAST_NEGATIVE_TWO,
};

enum ov5642_white_balance_t {
    AUTO_WB,
    SUN_LIGHT_WB,
    FLUORESCENT_WB,
    INCANDESCENT_WB,
};

enum ov5642_jpeg_quality_t {
    QUALITY_80,
    QUALITY_70,
    QUALITY_60,
};

enum ov5642_ae_target_t {
    GENERAL_AE_TARGET,
    NIGHT_AE_N1P0EV_TARGET,
    NIGHT_AE_N1P3EV_TARGET,
};

enum ov5642_lens_c_t {
    GENERAL_LENS_C,
    NIGHT_LENS_C,
};

enum ov5642_denoise_t {
    DENOISE_AUTO_DEFAULT,
    DENOISE_AUTO_P1,
};

enum ov5642_gamma_t {
    GENERAL_GAMMA,
    NIGHT_GAMMA,
};

enum ov5642_sharpness_t {
    SHARPNESS_AUTO_P3,
    SHARPNESS_AUTO_DEFAULT,
};

struct ov5642_special_reg_groups {
    const struct ov5642_reg_group_t *frame_rate_group;
    const struct ov5642_reg_burst_group_t *ae_weighting_group;
    const struct ov5642_reg_burst_group_t *color_matrix_group;
    const struct ov5642_reg_group_t *anti_banding_group;
    const struct ov5642_reg_group_t *brightness_group;
    const struct ov5642_reg_group_t *contrast_group;
    const struct ov5642_reg_group_t *white_balance_group;
    const struct ov5642_reg_group_t *jpeg_quality_group;
    const struct ov5642_reg_group_t *ae_target_group;
    const struct ov5642_reg_burst_group_t *lens_c_group;
    const struct ov5642_reg_group_t *denoise_group;
    const struct ov5642_reg_burst_group_t *gamma_group;
    const struct ov5642_reg_group_t *sharpness_group;
};

struct ov5642_basic_iq_misc_groups {
    const struct ov5642_reg_burst_group_t *lens_c_group;
    const struct ov5642_reg_burst_group_t *awb_group;
};

struct ov5642_basic_reg_arrays {
    const struct ov5642_reg_t *default_array;
    const uint16_t default_length;
    const struct ov5642_reg_t *preview_array;
    const uint16_t preview_length;
    const struct ov5642_reg_t *snapshot_array;
    const uint16_t snapshot_length;
    const struct ov5642_reg_t *IQ_array;
    const uint16_t IQ_length;
    const struct ov5642_basic_iq_misc_groups *IQ_misc_groups;
};

struct ov5642_af_func_array {
    long (*set_default_focus)(void);
    long (*set_auto_focus)(struct auto_focus_status *);
    void (*init_auto_focus)(void);
    long (*prepare_auto_focus)(void);
    long (*detect_auto_focus)(void);
    void (*deinit_auto_focus)(void);
};

struct ov5642_ae_func_array {
    long (*preview_to_snapshot)(enum ov5642_anti_banding_t );
    long (*snapshot_to_preview)(enum ov5642_white_balance_t );
    long (*prepare_snapshot)(void);
};

struct ov5642_awb_func_array {
    void (*init_awb)(void);
    void (*convert_awb)(void);
    void (*finish_awb)(char );
};

struct ov5642_night_mode_func_array {
    void (*init_night_mode)(void);
    void (*convert_night_mode)(int ,int );
    void (*finish_night_mode)(char );
    void (*disable_night_mode)(void);
    void (*query_night_mode)(int *);
};


int32_t ov5642_i2c_read( struct ov5642_reg_t *regs, uint32_t size);
int32_t ov5642_i2c_write( struct ov5642_reg_t *regs, uint32_t size);
int32_t ov5642_i2c_burst_read( unsigned short addr, unsigned char *data, uint32_t size);
int32_t ov5642_i2c_burst_write( char const *data, uint32_t size);

#endif //__VIDEO_MSM_OV5642_SENSOR_H

