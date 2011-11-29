
#ifndef _ATMEL_MXT224_TOUCH_H_
#define _ATMEL_MXT224_TOUCH_H_

#define ATMEL_TOUCHSCREEN_DRIVER_NAME "mXT224_touchscreen"
#define ATMEL_TOUCHSCREEN_KEYARRAY_DRIVER_NAME "mXT224_touchscreen_keyarray"
#define NUM_OF_ID_INFO_BLOCK                     7 
#define OBJECT_TABLE_ELEMENT_SIZE                6
#define ATMEL_REPORT_POINTS     		 		 2
#define ATMEL_MULTITOUCH_BUF_LEN                 20
#define INFO_BLOCK_CHECKSUM_SIZE                 3
#define CONFIG_CHECKSUM_SIZE                     4
#define MAX_OBJ_ELEMENT_SIZE                     30
#define WRITE_T6_SIZE                            1
#define ADD_ID_INFO_BLOCK                        0x0000
#define OBJECT_TABLE_ELEMENT_1                   0x0007
#define GEN_MESSAGEPROCESOR_T5                   0x5
#define GEN_COMMANDPROCESSOR_T6                  0x6
#define SPT_USERDATA_T38                         0x26
#define GEN_POWERCONFIG_T7                       0x7              
#define GEN_ACQUISITIONCONFIG_T8                 0x8	
#define TOUCH_MULTITOUCHSCREEN_T9                0x9
#define TOUCH_KEYARRAY_T15                       0xf                        
#define SPT_COMMSCONFIG_T18                      0x12                       
#define SPT_GPIOPWM_T19                          0x13
#define PROCI_GRIPFACESUPPRESSION_T20            0x14
#define PROCG_NOISESUPPRESSION_T22               0x16
#define TOUCH_PROXIMITY_T23                      0x17        
#define PROCI_ONETOUCHGESTUREPROCESSOR_T24       0x18
#define SPT_SELFTEST_T25                         0x19
#define PROCI_TWOTOUCHGESTUREPROCESSOR_T27       0x1b
#define SPT_CTECONFIG_T28                        0x1c
#define DEBUG_DIAGNOSTIC_T37                     0x25
#define SPT_MESSAGECOUNT_T44                     0x2c
#define CFGERR_MASK                              0x8
#define T6_RESET_VALUE                           0x8
#define T6_BACKUPNV_VALUE                        0x55
#define TOUCH_PRESS_MASK                         0x40
#define TOUCH_RELEASE_MASK                       0x20
#define T6_CALIBRATE_VALUE                       0x01
#define TOUCH_PRESS_RELEASE_MASK                 0x60
#define TOUCH_MOVE_MASK                          0x10
#define TOUCH_DETECT_MASK                        0x80
#define x_channel                                18
#define y_channel                                12
#define ATMEL_X_ORIGIN                           0
#define ATMEL_Y_ORIGIN                           0
#define ATMEL_X_MAX                              1024
#define ATMEL_Y_MAX                              1024
#define ATMEL_TOUCAN_PANEL_X                     480
#define ATMEL_TOUCAN_PANEL_Y                     800                
#define Y_LINE                                   16 
#define NUM_OF_REF_MODE_PAGE                     4
#define SIZE_OF_REF_MODE_PAGE                    130             
         
struct atmel_touchscreen_platform_data_t {
    unsigned int gpioirq;
    unsigned int gpiorst;
};

enum atmel_touch_state_t {
    NOT_TOUCH_STATE = 0,
    ONE_TOUCH_STATE,
    TWO_TOUCH_STATE,
    MAX_TOUCH_STATE
};

struct obj_t                      
{                                 
	uint16_t      obj_addr;
	uint8_t       size;       
	uint8_t       instance;
	uint8_t       num_reportid;   
	uint8_t       reportid_ub;
	uint8_t       reportid_lb;
	uint8_t       *value_array;
	int (*msg_handler)(uint8_t* message_data);
	uint8_t       config_crc;
};

struct id_info_t                      
{                                 
	uint8_t  family_id;
	uint8_t  variant_id;       
	uint8_t  version;
	uint8_t  build;
	uint8_t  matrix_x_size;
	uint8_t  matrix_y_size;
	uint8_t  num_obj_element;	
};

enum touch_state_t {
    RELEASE = 0,
    PRESS,
    MOVE
};

struct atmel_point_t {
    uint   x;
    uint   y;
};

struct atmel_multipoints_t {
    struct atmel_point_t   points[ATMEL_REPORT_POINTS];
    enum atmel_touch_state_t state;
};

struct touch_point_status_t 
{
    struct atmel_point_t  coord;
    enum touch_state_t state;
    int16_t z;
    uint16_t w;
};

struct tp_multi_t {
    struct atmel_multipoints_t buf[ATMEL_MULTITOUCH_BUF_LEN];    
    uint8_t                  index_w;
    uint8_t                  index_r;
    wait_queue_head_t        wait;    
};

struct atmel_sensitivity_t {
    uint8_t   touch_threshold;
	uint8_t   key_threshold;
};

struct atmel_power_mode_t {
	uint8_t idleacqint;
	uint8_t actvacqint;
    uint8_t actv2idleto;
};

struct atmel_linerity_t {
    uint8_t	movhysti;
	uint8_t	movhystn;
};

struct atmel_merge_t {
    uint8_t	mrghyst;
	uint8_t	mrgthr;
};

struct atmel_noise_suppression_t {
    uint8_t	ctrl;
	uint8_t	noisethr;
	uint8_t	freqhopscale;
	uint8_t freq_0;
	uint8_t freq_1;
	uint8_t freq_2;
	uint8_t freq_3;
	uint8_t freq_4;
};

struct atmel_power_switch_t {
    uint   on;
};
struct atmel_references_mode_t {
	uint16_t  data[x_channel][y_channel];
};

struct atmel_deltas_mode_t {
	int16_t  deltas[x_channel][y_channel];
};

struct atmel_gain_t {
	uint8_t   touch_blen;
	uint8_t   key_blen;
};

struct atmel_cte_mode_t {
	uint16_t   cte_data[Y_LINE];
};

struct atmel_fvs_mode_t {
    uint   enter;
};
struct atmel_acquisition_t {
    uint8_t		chrgtime;
	uint8_t 	tchdrift;
	uint8_t 	driftst;
	uint8_t	    tchautocal;
	uint8_t		sync;
	uint8_t		atchcalst;
	uint8_t     atchcalsthr;
	uint8_t		atchfrccalthr;
	uint8_t     atchfrccalratio;
};

struct  atmel_keyarray_t {
    uint8_t   	ctrl;
	uint8_t     xorigin;
	uint8_t     yorigin;
	uint8_t     xsize;
	uint8_t     ysize;
	uint8_t     akscfg;
	uint8_t     blen;
	uint8_t     chthr;
	uint8_t     tchdi;
};

struct atmel_multitouchscreen_t {
    uint8_t   	ctrl;
	uint8_t     xorigin;
	uint8_t     yorigin;
	uint8_t     xsize;
	uint8_t     ysize;
	uint8_t     akscfg;
	uint8_t     tchdi;
	uint8_t     orient;
	uint8_t     mrgtimeout;
	uint8_t     movfilter;
	uint8_t     numtouch;
	uint8_t     amphyst;
	uint8_t     xrange0;
	uint8_t     xrange1;
	uint8_t     yrange0;
	uint8_t     yrange1;
	uint8_t     xloclip;
	uint8_t     xhiclip;
	uint8_t     yloclip;
	uint8_t     yhiclip;
	uint8_t     xedgectrl;
	uint8_t     xedgedist;
	uint8_t     yedgectrl;
	uint8_t     yedgedist;
	uint8_t     jumplimit;
	uint8_t     tchhyst;
};

struct atmel_id_info_t {
    uint8_t   version;
};

struct atmel_selftest_t {
	uint      on;
	uint8_t   value;
};

struct atmel_grip_face_suppression_t {
    uint8_t   	ctrl;
	uint8_t     xlogrip;
	uint8_t     xhigrip;
	uint8_t     ylogrip;
	uint8_t     yhigrip;
	uint8_t     maxtchs;
	uint8_t     szthr1;
	uint8_t     szthr2;
	uint8_t     shpthr1;
	uint8_t     shpthr2;
	uint8_t     supextto;
};

#define __ATMELTOUCHDRVIO 0xAA
#define ATMEL_TOUCH_IOCTL_SET_SENSITIVITY _IOW(__ATMELTOUCHDRVIO, 1, struct atmel_sensitivity_t )
#define ATMEL_TOUCH_IOCTL_GET_SENSITIVITY _IOR(__ATMELTOUCHDRVIO, 2, struct atmel_sensitivity_t )
#define ATMEL_TOUCH_IOCTL_SET_POWER_MODE _IOW(__ATMELTOUCHDRVIO, 3, struct atmel_power_mode_t )
#define ATMEL_TOUCH_IOCTL_GET_POWER_MODE _IOR(__ATMELTOUCHDRVIO, 4, struct atmel_power_mode_t )
#define ATMEL_TOUCH_IOCTL_SET_LINERITY _IOW(__ATMELTOUCHDRVIO, 5, struct atmel_linerity_t )
#define ATMEL_TOUCH_IOCTL_GET_LINERITY _IOR(__ATMELTOUCHDRVIO, 6, struct atmel_linerity_t )
#define ATMEL_TOUCH_IOCTL_SET_MERGE _IOW(__ATMELTOUCHDRVIO, 7, struct atmel_merge_t )
#define ATMEL_TOUCH_IOCTL_GET_MERGE _IOR(__ATMELTOUCHDRVIO, 8, struct atmel_merge_t )
#define ATMEL_TOUCH_IOCTL_SET_NOISE_SUPPRESSION _IOW(__ATMELTOUCHDRVIO, 9, struct atmel_noise_suppression_t )
#define ATMEL_TOUCH_IOCTL_GET_NOISE_SUPPRESSION _IOR(__ATMELTOUCHDRVIO, 10, struct atmel_noise_suppression_t )
#define ATMEL_TOUCH_IOCTL_SET_POWER_SWITCH _IOW(__ATMELTOUCHDRVIO, 11, struct atmel_power_switch_t )
#define ATMEL_TOUCH_IOCTL_GET_REFERENCES_MODE _IOR(__ATMELTOUCHDRVIO, 12, struct atmel_references_mode_t )
#define ATMEL_TOUCH_IOCTL_GET_DELTAS_MODE _IOR(__ATMELTOUCHDRVIO, 13, struct atmel_deltas_mode_t )
#define ATMEL_TOUCH_IOCTL_SET_GAIN _IOW(__ATMELTOUCHDRVIO, 14, struct atmel_gain_t )
#define ATMEL_TOUCH_IOCTL_GET_GAIN _IOR(__ATMELTOUCHDRVIO, 15, struct atmel_gain_t )
#define ATMEL_TOUCH_IOCTL_GET_CTE_MODE _IOR(__ATMELTOUCHDRVIO, 16, struct atmel_cte_mode_t )
#define ATMEL_TOUCH_IOCTL_SET_KEYARRAY_FVS_MODE _IOW(__ATMELTOUCHDRVIO, 17, struct atmel_fvs_mode_t)
#define ATMEL_TOUCH_IOCTL_SET_ACQUISITION _IOW(__ATMELTOUCHDRVIO, 18, struct atmel_acquisition_t)
#define ATMEL_TOUCH_IOCTL_GET_ACQUISITION _IOR(__ATMELTOUCHDRVIO, 19, struct atmel_acquisition_t)
#define ATMEL_TOUCH_IOCTL_SET_KEYARRAY _IOW(__ATMELTOUCHDRVIO, 20, struct atmel_keyarray_t)
#define ATMEL_TOUCH_IOCTL_GET_KEYARRAY _IOR(__ATMELTOUCHDRVIO, 21, struct atmel_keyarray_t)
#define ATMEL_TOUCH_IOCTL_SET_MULTITOUCHSCREEN _IOW(__ATMELTOUCHDRVIO, 22, struct atmel_multitouchscreen_t)
#define ATMEL_TOUCH_IOCTL_GET_MULTITOUCHSCREEN _IOR(__ATMELTOUCHDRVIO, 23, struct atmel_multitouchscreen_t)
#define ATMEL_TOUCH_IOCTL_GET_VERSION _IOR(__ATMELTOUCHDRVIO, 24, struct atmel_id_info_t )
#define ATMEL_TOUCH_IOCTL_SET_SELFTEST_FVS_MODE _IOR(__ATMELTOUCHDRVIO, 25, struct atmel_selftest_t )
#define ATMEL_TOUCH_IOCTL_SET_GRIP_FACE_SUPPRESSION _IOW(__ATMELTOUCHDRVIO, 26, struct atmel_grip_face_suppression_t )
#define ATMEL_TOUCH_IOCTL_GET_GRIP_FACE_SUPPRESSION _IOR(__ATMELTOUCHDRVIO, 27, struct atmel_grip_face_suppression_t )
#endif
