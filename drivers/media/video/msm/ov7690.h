#ifndef __VIDEO_MSM_OV7690_SENSOR_H
#define __VIDEO_MSM_OV7690_SENSOR_H

#include <linux/delay.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <media/msm_camera.h>
#include <mach/gpio.h>

#include <mach/camera.h>

#define OV7690_SLAVE_ADDR (0x42 >> 1)

#define OV7690_PIDH_ADDR 0x0A
#define OV7690_PIDL_ADDR 0x0B
#define OV7690_PIDH_DEFAULT_VALUE 0x76
#define OV7690_PIDL_DEFAULT_VALUE 0x91

#define SWRESET_STABLE_TIME_MS 3

#define OV7690_BIT_OPERATION_TAG 0xFF

struct ov7690_reg_t {
    unsigned char addr;
    unsigned char data;
};

struct ov7690_reg_group_t {
    struct ov7690_reg_t *start;
    uint32_t            len;
};


enum ov7690_frame_rate_t {
    ONE_OF_ONE_FPS,
    ONE_OF_TWO_FPS,
    ONE_OF_EIGHT_FPS,
};

enum ov7690_ae_weighting_t {
    WHOLE_IMAGE_AE,
    CENTER_QUARTER_AE,
};

enum ov7690_anti_banding_t {
    MANUAL_50HZ_BANDING,
    MANUAL_60HZ_BANDING,
};

enum ov7690_brightness_t {
    BRIGHTNESS_POSITIVE_TWO,
    BRIGHTNESS_POSITIVE_ONE,
    BRIGHTNESS_POSITIVE_ZERO,
    BRIGHTNESS_NEGATIVE_ONE,
    BRIGHTNESS_NEGATIVE_TWO,
};

enum ov7690_contrast_t {
    CONTRAST_POSITIVE_TWO,
    CONTRAST_POSITIVE_ONE,
    CONTRAST_POSITIVE_ZERO,
    CONTRAST_NEGATIVE_ONE,
    CONTRAST_NEGATIVE_TWO,
};

enum ov7690_white_balance_t {
    AUTO_WB,
    SUN_LIGHT_WB,
    FLUORESCENT_WB,
    INCANDESCENT_WB,
};

enum ov7690_color_matrix_t {
    GENERAL_CMX,
    PERSON_CMX,
    LAND_CMX,
};

enum ov7690_ae_target_t {
    GENERAL_AE_TARGET,
    NIGHT_AE_TARGET,
};

struct ov7690_special_reg_groups {
    const struct ov7690_reg_group_t *frame_rate_group;
    const struct ov7690_reg_group_t *ae_weighting_group;
    const struct ov7690_reg_group_t *anti_banding_group;
    const struct ov7690_reg_group_t *brightness_group;
    const struct ov7690_reg_group_t *contrast_group;
    const struct ov7690_reg_group_t *white_balance_group;
    const struct ov7690_reg_group_t *color_matrix_group;
    const struct ov7690_reg_group_t *ae_target_group;
};


struct ov7690_basic_reg_arrays {
    const struct ov7690_reg_t *default_array;
    const uint16_t default_length;
    const struct ov7690_reg_t *preview_array;
    const uint16_t preview_length;
    const struct ov7690_reg_t *snapshot_array;
    const uint16_t snapshot_length;
    const struct ov7690_reg_t *IQ_array;
    const uint16_t IQ_length;
};


#endif //__VIDEO_MSM_OV7690_SENSOR_H

