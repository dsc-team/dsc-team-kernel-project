
#ifndef EDID_PARSE_H
#define EDID_PARSE_H




typedef unsigned char HTX_BYTE;     

typedef unsigned short HTX_WORD;    
   
typedef int HTX_INT;                
typedef unsigned char HTX_BOOL;     

#define HTX_TRUE  1
#define HTX_FALSE 0

#ifndef NULL
#define NULL  0
#endif






#define ST_MAX         1
#define DTD_MAX        8
#define SAD_MAX        10
#define SVD_MAX        32








enum mytiming {
    HTX_480i,
    HTX_576i,
    HTX_720_480p,
    HTX_720_576p,
    HTX_720p_60,
    HTX_720p_50,
    HTX_1080i_30,
    HTX_1080i_25,
    HTX_640_480p
};
typedef enum mytiming Timing;

enum myaspectratio {
    _4x3,
    _16x9
};
typedef enum myaspectratio AspectRatio;

enum mycolorspace {
    RGB,
    YCbCr601,
    YCbCr709
};
typedef enum mycolorspace Colorspace;

enum myrange {
    HTX_0_255,
    HTX_16_235
};
typedef enum myrange Range;



#define CLK_74_25_MHZ  7425
#define CLK_27_MHZ     2700
#define CLK_25_175_MHZ 2517

typedef struct MyVendorProductId *VendorProductId;

struct MyVendorProductId {
    HTX_BYTE manuf[4]; 
    HTX_BOOL prodcode[3];
    HTX_WORD serial[2];
    HTX_INT week;
    HTX_INT year;
};

typedef struct MyEdidVerRev *EdidVerRev;

struct MyEdidVerRev {
    HTX_INT ver; 
    HTX_INT rev;	
};

typedef struct MyBasicDispParams *BasicDispParams;

struct MyBasicDispParams {
    HTX_BOOL anadig;
    HTX_INT siglev;
    HTX_BOOL setup;
    HTX_BOOL sepsyn;
    HTX_BOOL compsync;
    HTX_BOOL sog;
    HTX_BOOL vsync_ser;
    HTX_BOOL dfp1x;
    HTX_INT maxh;
    HTX_INT maxv;
    HTX_INT dtc_gam;
    HTX_BOOL standby;
    HTX_BOOL suspend;
    HTX_BOOL aovlp;
    HTX_INT disptype;
    HTX_BOOL def_col_sp;
    HTX_BOOL pt_mode;
    HTX_BOOL def_gtf;
    
    
    
    HTX_INT color_bit_depth;
    HTX_INT interface_type;
    
    HTX_INT color_encoding_format;
    HTX_BOOL display_frequency;
};

typedef struct MyColorCharacteristics *ColorCharacteristics;

struct MyColorCharacteristics { 
    HTX_INT rx;
    HTX_INT ry;
    HTX_INT gx;
    HTX_INT gy;
    HTX_INT bx;
    HTX_INT by;
    HTX_INT wx;
    HTX_INT wy;
};

typedef struct MyEstablishedTiming *EstablishedTiming;

struct MyEstablishedTiming {
    
    HTX_BOOL m720_400_70;
    HTX_BOOL m720_400_88;
    HTX_BOOL m640_480_60;
    HTX_BOOL m640_480_67;
    HTX_BOOL m640_480_72;
    HTX_BOOL m640_480_75;
    HTX_BOOL m800_600_56;
    HTX_BOOL m800_600_60;
    
    HTX_BOOL m800_600_72;
    HTX_BOOL m800_600_75;
    HTX_BOOL m832_624_75;
    HTX_BOOL m1024_768_87;
    HTX_BOOL m1024_768_60;
    HTX_BOOL m1024_768_70;
    HTX_BOOL m1024_768_75;
    HTX_BOOL m1280_1024_75;
    
    HTX_BOOL m1152_870_75;
    
    HTX_BOOL res_7;
    HTX_BOOL res_6;
    HTX_BOOL res_5;
    HTX_BOOL res_4;
    HTX_BOOL res_3;
    HTX_BOOL res_2;
    HTX_BOOL res_1;
};

typedef struct MyStandardTimingList *StandardTimingList;

struct MyStandardTimingList {
    HTX_INT hap; 
    HTX_INT imasp; 
    HTX_INT ref;
};

typedef struct MyDetailedTimingBlockList *DetailedTimingBlockList;

struct MyDetailedTimingBlockList { 
    HTX_BOOL preferred_timing;
    HTX_WORD pix_clk;  
    HTX_INT h_active; 
    HTX_INT h_blank; 
    HTX_INT h_act_blank; 
    HTX_INT v_active; 
    HTX_INT v_blank;
    HTX_INT v_act_blank; 
    HTX_INT hsync_offset; 
    HTX_INT hsync_pw; 
    HTX_INT vsync_offset; 
    HTX_INT vsync_pw; 
    HTX_INT h_image_size; 
    HTX_INT v_image_size; 
    HTX_INT h_border; 
    HTX_INT v_border; 
    HTX_BOOL ilace;
    HTX_INT stereo; 
    HTX_INT comp_sep; 
    HTX_BOOL serr_v_pol;
    HTX_BOOL sync_comp_h_pol;
};

typedef struct MyMonitorDescriptorBlock *MonitorDescriptorBlock;

struct MyMonitorDescriptorBlock {
    HTX_BYTE mon_snum[14];
    HTX_BYTE ascii_data[14];
    HTX_INT mrl_min_vrate; 
    HTX_INT mrl_max_vrate; 
    HTX_INT mrl_min_hrate; 
    HTX_INT mrl_max_hrate; 
    HTX_INT mrl_max_pix_clk; 
    HTX_INT mrl_gtf_support; 
    HTX_BOOL mrl_bad_stf;
    HTX_INT mrl_reserved_11; 
    HTX_INT mrl_gtf_start_freq; 
    HTX_INT mrl_gtf_c; 
    HTX_INT mrl_gtf_m; 
    HTX_INT mrl_gtf_k; 
    HTX_INT mrl_gtf_j; 
    HTX_BYTE mon_name[14];
    HTX_INT cp_wpoint_index1; 
    HTX_INT cp_w_lb1; 
    HTX_BOOL cp_w_x1;
    HTX_BOOL cp_w_y1;
    HTX_BOOL cp_w_gam1;
    HTX_INT cp_wpoint_index2; 
    HTX_INT cp_w_lb2; 
    HTX_BOOL cp_w_x2;
    HTX_BOOL cp_w_y2;
    HTX_BOOL cp_w_gam2;
    HTX_BOOL cp_bad_set; 
    HTX_INT cp_pad1;
    HTX_INT cp_pad2; 
    HTX_INT cp_pad3; 
    HTX_INT st9_hap; 
    HTX_INT st9_imasp; 
    HTX_INT st9_ref; 
    HTX_INT st10_hap; 
    HTX_INT st10_imasp; 
    HTX_INT st10_ref; 
    HTX_INT st11_hap; 
    HTX_INT st11_imasp; 
    HTX_INT st11_ref; 
    HTX_INT st12_hap; 
    HTX_INT st12_imasp; 
    HTX_INT st12_ref; 
    HTX_INT st13_hap; 
    HTX_INT st13_imasp; 
    HTX_INT st13_ref; 
    HTX_INT st14_hap; 
    HTX_INT st14_imasp; 
    HTX_INT st14_ref; 
    HTX_BOOL st_set; 
    HTX_INT ms_HTX_BYTE[13]; 
    
    
    
    HTX_INT mrl_hrange_offset;
    HTX_INT mrl_vrange_offset;
    
    HTX_BOOL mrl_def_gtf_supported;
    HTX_BOOL mrl_cvt_supported;
    
    HTX_INT mrl_max_active_lines;
    
    HTX_BOOL mrl_ar_43;
    HTX_BOOL mrl_ar_169;
    HTX_BOOL mrl_ar_1610;
    HTX_BOOL mrl_ar_54;
    HTX_BOOL mrl_ar_159;
    
    HTX_INT mrl_par;
    
    HTX_BOOL mrl_std_cvt_blanking;
    HTX_BOOL mrl_red_cvt_blanking;
    
    HTX_BOOL mrl_hshrink;
    HTX_BOOL mrl_hstretch;
    HTX_BOOL mrl_vshrink;
    HTX_BOOL mrl_vstretch;
    
    HTX_INT mrl_pref_vrefresh_rate;  
    
    HTX_INT cvt1_Vactive_lines;
    HTX_INT cvt1_aspect_ratio;
    HTX_INT cvt1_pref_vrate;
    HTX_BOOL cvt1_50hz_sb;
    HTX_BOOL cvt1_60hz_sb;
    HTX_BOOL cvt1_75hz_sb;
    HTX_BOOL cvt1_85hz_sb;
    HTX_BOOL cvt1_60hz_rb;
    
    HTX_INT cvt2_Vactive_lines;
    HTX_INT cvt2_aspect_ratio;
    HTX_INT cvt2_pref_vrate;
    HTX_BOOL cvt2_50hz_sb;
    HTX_BOOL cvt2_60hz_sb;
    HTX_BOOL cvt2_75hz_sb;
    HTX_BOOL cvt2_85hz_sb;
    HTX_BOOL cvt2_60hz_rb;  
    
    HTX_INT cvt3_Vactive_lines;
    HTX_INT cvt3_aspect_ratio;
    HTX_INT cvt3_pref_vrate;
    HTX_BOOL cvt3_50hz_sb;
    HTX_BOOL cvt3_60hz_sb;
    HTX_BOOL cvt3_75hz_sb;
    HTX_BOOL cvt3_85hz_sb;
    HTX_BOOL cvt3_60hz_rb;
    
    HTX_INT cvt4_Vactive_lines;
    HTX_INT cvt4_aspect_ratio;
    HTX_INT cvt4_pref_vrate;
    HTX_BOOL cvt4_50hz_sb;
    HTX_BOOL cvt4_60hz_sb;
    HTX_BOOL cvt4_75hz_sb;
    HTX_BOOL cvt4_85hz_sb;
    HTX_BOOL cvt4_60hz_rb;
    
    HTX_INT et3_byte6;
    HTX_INT et3_byte7;
    HTX_INT et3_byte8;
    HTX_INT et3_byte9;
    HTX_INT et3_byte10;
    HTX_INT et3_byte11;
};

typedef struct MySVDescriptorList *SVDescriptorList;

struct MySVDescriptorList {
    HTX_BOOL native;
    HTX_INT vid_id;
};

typedef struct MySADescriptorList *SADescriptorList;

struct MySADescriptorList {
    HTX_BOOL rsvd1;
    HTX_INT aud_format; 
    HTX_INT max_num_chan; 
    HTX_BOOL rsvd2;
    HTX_BOOL khz_192;
    HTX_BOOL khz_176;
    HTX_BOOL khz_96;
    HTX_BOOL khz_88;
    HTX_BOOL khz_48;
    HTX_BOOL khz_44;
    HTX_BOOL khz_32;
    HTX_INT unc_rsrvd; 
    HTX_BOOL unc_24bit; 
    HTX_BOOL unc_20bit; 
    HTX_BOOL unc_16bit; 
    HTX_INT comp_maxbitrate; 
    HTX_BYTE sample_sizes;
    HTX_BYTE sample_rates;
};

typedef struct MySpadPayload *SpadPayload;

struct MySpadPayload {
    HTX_BOOL rlc_rrc; 
    HTX_BOOL flc_frc; 
    HTX_BOOL rc; 
    HTX_BOOL rl_rr; 
    HTX_BOOL fc; 
    HTX_BOOL lfe; 
    HTX_BOOL fl_fr; 
};

typedef struct MyColorimetryDataBlock *ColorimetryDataBlock;

struct MyColorimetryDataBlock {
    HTX_BOOL xvycc709;
    HTX_BOOL xvycc601;
    HTX_BYTE md;
};

typedef struct MyCeaDataBlock *CeaDataBlock;

struct MyCeaDataBlock {
    struct MySVDescriptorList svd[SVD_MAX]; 
    struct MySADescriptorList sad[SAD_MAX];
    SpadPayload spad;
    ColorimetryDataBlock cdb;
    HTX_WORD ieee_reg[2];
    HTX_BOOL vsdb_hdmi;
    HTX_BYTE *vsdb_payload;
    HTX_INT hdmi_addr_a; 
    HTX_INT hdmi_addr_b; 
    HTX_INT hdmi_addr_c; 
    HTX_INT hdmi_addr_d; 
    HTX_INT vsdb_ext[26];  
    HTX_BOOL supports_ai; 
};

typedef struct MyCeaTimingExtension *CeaTimingExtension;

struct MyCeaTimingExtension {
    HTX_INT cea_rev; 
    HTX_BOOL underscan; 
    HTX_BOOL audio;
    HTX_BOOL YCC444;
    HTX_BOOL YCC422;
    HTX_INT native_formats; 
    CeaDataBlock cea;
    HTX_INT checksum; 
};

typedef struct MyEdidInfo *EdidInfo;

struct MyEdidInfo {
    HTX_BOOL edid_header; 
    HTX_INT edid_extensions; 
    HTX_INT edid_checksum;
    HTX_INT ext_block_count;
    VendorProductId vpi;
    EdidVerRev evr;
    BasicDispParams bdp;
    ColorCharacteristics cc;
    EstablishedTiming et;
    struct MyStandardTimingList st[ST_MAX];
    struct MyDetailedTimingBlockList dtb[DTD_MAX];
    MonitorDescriptorBlock mdb;
    CeaTimingExtension cea;
};

HTX_INT parse_edid( EdidInfo e, HTX_BYTE *d );
EdidInfo init_edid_info( EdidInfo edid );
void clear_edid( EdidInfo e );



#endif 
