/* drivers/video/msm/lcdc_adv7520.c
 *
 * Copyright (c) 2008 QUALCOMM USA, INC.
 *
 * All source code in this file is licensed under the following license
 * except where indicated.
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

#include "msm_fb.h"

#ifdef CONFIG_FB_MSM_MDDI_AUTO_DETECT_LCDC_PRISM_WVGA_FALLBACK
#include "mddihosti.h"
#endif

#include <mach/gpio.h>
#include <mach/vreg.h>
#include "edid_parse.h"
#include "hdmi_cmd.h"
#include <linux/delay.h>
#include "hdmi.h"
#include <mach/pm_log.h>
#include <mach/austin_hwid.h>

#define SUPPORT_DUAL_DISPLAY
#define INCREASE_OUTPUT_CURRENT


#define HDMI_INT_GPIO 39
#define HDMI_CABLEIN_GPIO 35

int hdmi_os_engineer_mode_test(void __user *p);
static bool cable_out = HTX_FALSE;

atomic_t cable_in = ATOMIC_INIT(0);
atomic_t video_out_flag = ATOMIC_INIT(0);

atomic_t hdmi_initialed = ATOMIC_INIT(0);
atomic_t hdmi_audio_only_flag = ATOMIC_INIT(0);

void lcdc_hdmi_init(void);
void hdmi_init(void);
void hdmi_chip_powerup(void);
void hdmi_chip_powerdown(void);
void hdmi_all_powerup(void);
void hdmi_all_powerdown(void);
void hdmi_lcdc_all_off(void);
void modify_pkg_slave_addr(void);
void modify_cec_slave_addr(void);

static struct work_struct qhdmi_irqwork;

#ifndef HDMI_TESTMODE
static struct work_struct qhdmi_cablein_irqwork;
#endif

extern struct platform_device *pdev_hdmi;
extern struct platform_device *pdev_hdmi_01;
static int hdmi_param=1;
extern int first_pixel_start_x;
extern int first_pixel_start_y;

static bool dynamic_720p_disable = 0;
static bool dynamic_480p_disable = 0;
static bool dynamic_480p_aspect_4x3 = 0; 
int current_hdmi_timing = HTX_720p_60; 
static int last_hdmi_timing = HTX_720p_60;
static bool dynamic_force_edid = 0;
bool hdmi_block_power_status=0; 
static bool dynamic_log_ebable = 0;
static int hdmi_pwr_counter = 0;



void set_resolution_mdp_lcdc_on(struct msm_fb_data_type *mfd)
{
	int lcdc_width;
	int lcdc_height;
	int lcdc_bpp;
	int lcdc_border_clr;
	int lcdc_underflow_clr;
	int lcdc_hsync_skew;

	int hsync_period;
	int hsync_ctrl;
	int vsync_period;
	int display_hctl;
	int display_v_start;
	int display_v_end;
	int active_hctl;
	int active_h_start;
	int active_h_end;
	int active_v_start;
	int active_v_end;
	int ctrl_polarity;
	int h_back_porch;
	int h_front_porch;
	int v_back_porch;
	int v_front_porch;
	int hsync_pulse_width;
	int vsync_pulse_width;
	int hsync_polarity;
	int vsync_polarity;
	int data_en_polarity;
	int hsync_start_x;
	int hsync_end_x;
	uint8 *buf;
	int bpp;
	uint32 dma2_cfg_reg;
	struct fb_info *fbi;
	struct fb_var_screeninfo *var;


printk(KERN_ERR "####### set_resolution_mdp_lcdc_on #########\n");









	fbi = mfd->fbi;
	var = &fbi->var;

	
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);

	bpp = fbi->var.bits_per_pixel / 8;
	buf = (uint8 *) fbi->fix.smem_start;
	buf +=
	    (fbi->var.xoffset + fbi->var.yoffset * fbi->var.xres_virtual) * bpp;

	dma2_cfg_reg = DMA_PACK_ALIGN_LSB | DMA_DITHER_EN | DMA_OUT_SEL_LCDC;

	if (mfd->fb_imgType == MDP_BGR_565)
		dma2_cfg_reg |= DMA_PACK_PATTERN_BGR;
	else
		dma2_cfg_reg |= DMA_PACK_PATTERN_RGB;

	if (bpp == 2)
		dma2_cfg_reg |= DMA_IBUF_FORMAT_RGB565;
	else if (bpp == 3)
		dma2_cfg_reg |= DMA_IBUF_FORMAT_RGB888;
	else
		dma2_cfg_reg |= DMA_IBUF_FORMAT_xRGB8888_OR_ARGB8888;

	switch (mfd->panel_info.bpp) {
	case 24:
		dma2_cfg_reg |= DMA_DSTC0G_8BITS |
		    DMA_DSTC1B_8BITS | DMA_DSTC2R_8BITS;
		break;

	case 18:
		dma2_cfg_reg |= DMA_DSTC0G_6BITS |
		    DMA_DSTC1B_6BITS | DMA_DSTC2R_6BITS;
		break;

	case 16:
		dma2_cfg_reg |= DMA_DSTC0G_6BITS |
		    DMA_DSTC1B_5BITS | DMA_DSTC2R_5BITS;
		break;

	default:
		printk(KERN_ERR "mdp lcdc can't support format %d bpp!\n",
		       mfd->panel_info.bpp);

	}

	

	
	MDP_OUTP(MDP_BASE + 0x90008, (uint32) buf);
	
	MDP_OUTP(MDP_BASE + 0x90004, ((fbi->var.yres) << 16) | (fbi->var.xres));
	
	MDP_OUTP(MDP_BASE + 0x9000c, fbi->var.xres * bpp);
	
	MDP_OUTP(MDP_BASE + 0x90010, 0);
	
	MDP_OUTP(MDP_BASE + 0x90000, dma2_cfg_reg);

	
	h_back_porch = var->left_margin;
	h_front_porch = var->right_margin;
	v_back_porch = var->upper_margin;
	v_front_porch = var->lower_margin;
	hsync_pulse_width = var->hsync_len;
	vsync_pulse_width = var->vsync_len;
	lcdc_border_clr = mfd->panel_info.lcdc.border_clr;
	lcdc_underflow_clr = mfd->panel_info.lcdc.underflow_clr;
	lcdc_hsync_skew = mfd->panel_info.lcdc.hsync_skew;

	lcdc_width = mfd->panel_info.xres;
	lcdc_height = mfd->panel_info.yres;
	lcdc_bpp = mfd->panel_info.bpp;

	hsync_period =
	    hsync_pulse_width + h_back_porch + lcdc_width + h_front_porch;
	hsync_ctrl = (hsync_period << 16) | hsync_pulse_width;
	hsync_start_x = hsync_pulse_width + h_back_porch;
	hsync_end_x = hsync_period - h_front_porch - 1;
	display_hctl = (hsync_end_x << 16) | hsync_start_x;

	vsync_period =
	    (vsync_pulse_width + v_back_porch + lcdc_height +
	     v_front_porch) * hsync_period;
	display_v_start =
	    (vsync_pulse_width + v_back_porch) * hsync_period + lcdc_hsync_skew;
	display_v_end =
	    vsync_period - (v_front_porch * hsync_period) + lcdc_hsync_skew - 1;

	if (lcdc_width != var->xres) {
		active_h_start = hsync_start_x + first_pixel_start_x;
		active_h_end = active_h_start + var->xres - 1;
		active_hctl =
		    ACTIVE_START_X_EN | (active_h_end << 16) | active_h_start;
	} else {
		active_hctl = 0;
	}

	if (lcdc_height != var->yres) {
		active_v_start =
		    display_v_start + first_pixel_start_y * hsync_period;
		active_v_end = active_v_start + (var->yres) * hsync_period - 1;
		active_v_start |= ACTIVE_START_Y_EN;
	} else {
		active_v_start = 0;
		active_v_end = 0;
	}


	
	if(current_hdmi_timing == HTX_720p_60)
	{
		hsync_polarity = 0;
		vsync_polarity = 0;
		data_en_polarity = 0;
	}
	else 
	{
		hsync_polarity = 1;
		vsync_polarity = 1;
		data_en_polarity = 0;
	}

	ctrl_polarity =
	    (data_en_polarity << 2) | (vsync_polarity << 1) | (hsync_polarity);

	MDP_OUTP(MDP_BASE + 0xE0004, hsync_ctrl);
	MDP_OUTP(MDP_BASE + 0xE0008, vsync_period);
	MDP_OUTP(MDP_BASE + 0xE000c, vsync_pulse_width * hsync_period);
	MDP_OUTP(MDP_BASE + 0xE0010, display_hctl);
	MDP_OUTP(MDP_BASE + 0xE0014, display_v_start);
	MDP_OUTP(MDP_BASE + 0xE0018, display_v_end);
	MDP_OUTP(MDP_BASE + 0xE0028, lcdc_border_clr);
	MDP_OUTP(MDP_BASE + 0xE002c, lcdc_underflow_clr);
	MDP_OUTP(MDP_BASE + 0xE0030, lcdc_hsync_skew);
	MDP_OUTP(MDP_BASE + 0xE0038, ctrl_polarity);
	MDP_OUTP(MDP_BASE + 0xE001c, active_hctl);
	MDP_OUTP(MDP_BASE + 0xE0020, active_v_start);
	MDP_OUTP(MDP_BASE + 0xE0024, active_v_end);



		



	
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);


}




static int lcdc_set_hdmi_timing(int hdmi_timing )
{

	struct msm_fb_data_type *mfd;
	struct fb_info *fbi;
	struct platform_device *mdp_dev = NULL;

	printk(KERN_ERR "lcdc_set_hdmi_timing()+, hdmi_timing=%d\n", hdmi_timing);

	
	mfd = platform_get_drvdata(pdev_hdmi_01);
	printk(KERN_ERR "pdev_hdmi=0x%x, pdev_hdmi_01=0x%x, mfd=0x%x\n", (unsigned int) pdev_hdmi, (unsigned int) pdev_hdmi_01, (unsigned int) mfd);

	if (!mfd)
	{
		printk(KERN_ERR "[ERR] lcdc_set_hdmi_timing(), ENODEV\n");
		return -ENODEV;
	}

	if (mfd->key != MFD_KEY)
	{
		printk(KERN_ERR "[ERR] lcdc_set_hdmi_timing(), EINVAL\n");
		return -EINVAL;
	}

	mdp_dev =mfd->pdev;
	
	
	
	platform_get_drvdata(mdp_dev);

	
	if(mfd->dest != DISPLAY_LCDC)
	{
		printk(KERN_ERR "mfd->dest != DISPLAY_LCDC!!\n");
		return 0;
	}

	if(hdmi_timing==HTX_720p_60)
	{
		printk("lcdc_set_hdmi_timing(), HTX_720p_60\n");

		mfd->panel_info.xres = 1280;
		mfd->panel_info.yres = 720;
		mfd->panel_info.type = LCDC_PANEL;
		mfd->panel_info.pdest = DISPLAY_1;
		mfd->panel_info.wait_cycle = 0;
		mfd->panel_info.bpp = 24;
		mfd->panel_info.fb_num = 2;
		mfd->panel_info.clk_rate = 74250*1000;

		mfd->panel_info.lcdc.h_back_porch = 220;
		mfd->panel_info.lcdc.h_front_porch = 110;
		mfd->panel_info.lcdc.h_pulse_width = 40;
		mfd->panel_info.lcdc.v_back_porch = 20;
		mfd->panel_info.lcdc.v_front_porch = 5;
		mfd->panel_info.lcdc.v_pulse_width = 5;
		mfd->panel_info.lcdc.border_clr = 0;
		mfd->panel_info.lcdc.underflow_clr = 0xff;
		mfd->panel_info.lcdc.hsync_skew = 0;


		
		
		printk(KERN_ERR "set LCD_MD_REG and LCD_NS_REG for 74.25Mhz\n");

		outpdw(MSM_CLK_CTL_BASE + 0x3EC, 0x9ABDFFF); 
		outpdw(MSM_CLK_CTL_BASE + 0x3F0, 0xE9AA1B44); 

		printk(KERN_ERR "read_back LCD_MD_REG=0x%x, LCD_NS_REG=0x%x\n", inpdw(MSM_CLK_CTL_BASE + 0x3EC), inpdw(MSM_CLK_CTL_BASE + 0x3F0));
		
		last_hdmi_timing = HTX_720p_60;

	}
	else if(hdmi_timing==HTX_720_480p)
	{
		printk("lcdc_set_hdmi_timing(), HTX_720_480p\n");

		mfd->panel_info.xres = 720;
		mfd->panel_info.yres = 480;
		mfd->panel_info.type = LCDC_PANEL;
		mfd->panel_info.pdest = DISPLAY_1;
		mfd->panel_info.wait_cycle = 0;
		mfd->panel_info.bpp = 24;
		mfd->panel_info.fb_num = 2;
		mfd->panel_info.clk_rate = 27027*1000;

		mfd->panel_info.lcdc.h_back_porch = 60;
		mfd->panel_info.lcdc.h_front_porch = 16;
		mfd->panel_info.lcdc.h_pulse_width = 62;
		mfd->panel_info.lcdc.v_back_porch = 30;
		mfd->panel_info.lcdc.v_front_porch = 9;
		mfd->panel_info.lcdc.v_pulse_width = 6;
		mfd->panel_info.lcdc.border_clr = 0;	
		mfd->panel_info.lcdc.underflow_clr = 0xff;	
		mfd->panel_info.lcdc.hsync_skew = 16;


		
		
		printk(KERN_ERR "set LCD_MD_REG and LCD_NS_REG for 27.027Mhz\n");

		outpdw(MSM_CLK_CTL_BASE + 0x3EC, 0x11995FFA); 
		outpdw(MSM_CLK_CTL_BASE + 0x3F0, 0x71931B44); 

		printk(KERN_ERR "read_back LCD_MD_REG=0x%x, LCD_NS_REG=0x%x\n", inpdw(MSM_CLK_CTL_BASE + 0x3EC), inpdw(MSM_CLK_CTL_BASE + 0x3F0));
		
		last_hdmi_timing = HTX_720_480p;
	}
	
	else
	{
		printk("lcdc_set_hdmi_timing(), HTX_640_480p\n");
		mfd->panel_info.xres = 640;
		mfd->panel_info.yres = 480;
		mfd->panel_info.type = LCDC_PANEL;
		mfd->panel_info.pdest = DISPLAY_1;
		mfd->panel_info.wait_cycle = 0;
		mfd->panel_info.bpp = 24;
		mfd->panel_info.fb_num = 2;
		mfd->panel_info.clk_rate = 25175*1000;

		mfd->panel_info.lcdc.h_back_porch = 48;
		mfd->panel_info.lcdc.h_front_porch = 16;
		mfd->panel_info.lcdc.h_pulse_width = 96;
		mfd->panel_info.lcdc.v_back_porch = 33;
		mfd->panel_info.lcdc.v_front_porch = 10;
		mfd->panel_info.lcdc.v_pulse_width = 2;
		mfd->panel_info.lcdc.border_clr = 0;
		mfd->panel_info.lcdc.underflow_clr = 0xff;
		mfd->panel_info.lcdc.hsync_skew = 16;


		
		printk(KERN_ERR "lcdc_set_hdmi_timing(), read LCD_MD_REG=0x%x, LCD_NS_REG=0x%x\n", inpdw(MSM_CLK_CTL_BASE + 0x3EC), inpdw(MSM_CLK_CTL_BASE + 0x3F0));
		

		outpdw(MSM_CLK_CTL_BASE + 0x3EC, 0x13AB3FFF); 
		outpdw(MSM_CLK_CTL_BASE + 0x3F0, 0x53AA1B44); 

		printk(KERN_ERR "read_back LCD_MD_REG=0x%x, LCD_NS_REG=0x%x\n", inpdw(MSM_CLK_CTL_BASE + 0x3EC), inpdw(MSM_CLK_CTL_BASE + 0x3F0));
		
		last_hdmi_timing = HTX_640_480p;
	}

	printk(KERN_ERR "========= lcdc_set_hdmi_timing() =========\n clk_rate=%d", mfd->panel_info.clk_rate);
	printk(KERN_ERR "h_back_porch=%d, h_front_porch=%d, h_pulse_width=%d\n", mfd->panel_info.lcdc.h_back_porch, mfd->panel_info.lcdc.h_front_porch, mfd->panel_info.lcdc.h_pulse_width);
	printk(KERN_ERR "v_back_porch=%d, v_front_porch=%d, v_pulse_width=%d\n", mfd->panel_info.lcdc.v_back_porch, mfd->panel_info.lcdc.v_front_porch, mfd->panel_info.lcdc.v_pulse_width);







	
	
	



	fbi = mfd->fbi;
	fbi->var.pixclock = mfd->panel_info.clk_rate;
	fbi->var.left_margin = mfd->panel_info.lcdc.h_back_porch;
	fbi->var.right_margin = mfd->panel_info.lcdc.h_front_porch;
	fbi->var.upper_margin = mfd->panel_info.lcdc.v_back_porch;
	fbi->var.lower_margin = mfd->panel_info.lcdc.v_front_porch;
	fbi->var.hsync_len = mfd->panel_info.lcdc.h_pulse_width;
	fbi->var.vsync_len = mfd->panel_info.lcdc.v_pulse_width;


	fbi->var.xres = mfd->panel_info.xres;
	fbi->var.yres = mfd->panel_info.yres;
	fbi->var.xres_virtual = mfd->panel_info.xres;
	fbi->var.yres_virtual = mfd->panel_info.yres;

	

	

	

	
	
	
	
	if((hdmi_timing==HTX_720p_60) || (hdmi_timing==HTX_720_480p) || (hdmi_timing==HTX_640_480p))
	{
		platform_set_drvdata(mdp_dev, mfd);
		set_resolution_mdp_lcdc_on(mfd);
	}

	printk(KERN_ERR "lcdc_set_hdmi_timing()-\n");

	return 0;
}



static int __init lcdc_adv7520_init(void)
{
	int ret;
	struct msm_panel_info pinfo;

	printk("BootLog, +%s\n", __func__);

	
	#if (defined(CONFIG_MACH_EVT0_1) || defined(CONFIG_MACH_EVT0) || defined(CONFIG_MACH_EVB))
		return 0;
	#endif

	
	#ifndef SUPPORT_DUAL_DISPLAY
#ifdef CONFIG_FB_MSM_TRY_MDDI_CATCH_LCDC_PRISM
	ret = msm_fb_detect_client("lcdc_auo_wqvga");
	if (ret == -ENODEV)
		return 0;

	if (ret && (mddi_get_client_id() != 0))
		return 0;
#endif
	#endif

		pinfo.xres = 1280;
		pinfo.yres = 720;
		pinfo.type = LCDC_PANEL;
		pinfo.pdest = DISPLAY_1;
		pinfo.wait_cycle = 0;
		pinfo.bpp = 24;
		pinfo.fb_num = 2;
		pinfo.clk_rate = 74250*1000;

		pinfo.lcdc.h_back_porch = 220;
		pinfo.lcdc.h_front_porch = 110;
		pinfo.lcdc.h_pulse_width = 40;
		pinfo.lcdc.v_back_porch = 20;
		pinfo.lcdc.v_front_porch = 5;
		pinfo.lcdc.v_pulse_width = 5;
		pinfo.lcdc.border_clr = 0;	
		pinfo.lcdc.underflow_clr = 0xff;	
		pinfo.lcdc.hsync_skew = 0;

		current_hdmi_timing = HTX_720p_60;
		last_hdmi_timing = HTX_720p_60;
		printk("lcdc_adv7520_init(), HTX_720p_60\n");

	ret = lcdc_device_register(&pinfo);
	if (ret)
		printk(KERN_ERR "%s: failed to register device!\n", __func__);
	else
	{
		
		#ifdef CONFIG_MACH_EVT0
		printk("EVT0 workaround for MDDI resume isse");
		return 0;
		#endif

		
		lcdc_hdmi_init();
		printk("call lcdc_hdmi_init()");
	}

	printk("BootLog, -%s\n, ret=%d", __func__, ret);
	return ret;
}






int hdmi_get_cable_status(void __user *p)
{
	int ret;
	unsigned short int	cable_status;

	ret = copy_from_user(&cable_status, p, sizeof(unsigned short int)); 
	if (ret)
		return ret;

	
	
	
	msleep(200);
	cable_status = atomic_read(&cable_in);
	printk(KERN_ERR "hdmi_get_cable_status(), cable_in=%d\n", cable_status);

	if(copy_to_user(p, &cable_status, sizeof(unsigned short int))) 
		return -EFAULT;
	ret = 0;

	return ret;
}





int hdmi_os_engineer_mode_test(void __user *p)
{
	struct hdmi_cmd hdmiCmd;
	int ret;

	ret = copy_from_user(&hdmiCmd, p, sizeof(hdmiCmd)); 

	switch(hdmiCmd.cmd)
	{
		case HDMI_ON:
		{
			printk(KERN_ERR "[os_engineer_mode] case HDMI_ON\n");
			hdmi_all_powerup();

			break;
		}

		case HDMI_OFF:
		{
			printk(KERN_ERR "[os_engineer_mode] case HDMI_OFF ... ver 2 \n");
			hdmi_all_powerdown();

			break;
		}

		case HDMI_GET_CABLE_STATUS:
		{
			printk(KERN_ERR "[os_engineer_mode] call IC POWER ON\n");
			hdmi_all_powerup();
			msleep(200);
			
			ret = atomic_read(&cable_in);
			printk(KERN_ERR "case HDMI_GET_CABLE_STATUS, cable_in=%d\n", ret);
			
			printk(KERN_ERR "[os_engineer_mode] call IC POWER OFF\n");
			hdmi_all_powerdown();

			break;
		}

		default:
			printk(KERN_ERR "[HDMI Engr mode] unknown ioctl received! cmd=%d\n", hdmiCmd.cmd);
			break;
	}

	return ret;
}







#define HDMI_NAME "hdmi_main"
#define HDMIEDID_NAME "hdmi_edid"
#define HDMICEC_NAME "hdmi_cec"
#define HDMIPKT_NAME "hdmi_pkt"

#ifdef HDCP_OPTION
#define CHECK_PERIOD_MS 2000
#define BKSV_MAX       75
#define MAX_HDCP_NACK  4

void av_inhibit( HTX_BOOL enable, Colorspace colorspace_in, Range colorspace_range, Timing vic );
static struct delayed_work g_hdcpWork;
HTX_INT bksv_flag_count = 0;
HTX_BOOL hdcp_status = 0;
HTX_BOOL hdcp_enabled = 0;
HTX_BOOL hdcp_requested = 0;
HTX_INT bksv_flag_counter = 0;
HTX_INT bksv_count = 0;
HTX_INT total_bksvs = 0;
HTX_BYTE bksv[ BKSV_MAX ];
HTX_BYTE hdcp_bstatus[ 2 ];
HTX_BOOL rx_sense_status = HTX_FALSE;
#endif

#define MAX_EDID_TRIES 10

#define ADV7520NK                     0
#define ADV7520                       1
#define ADV7521                       2
#ifdef CONFIG_MACH_EVT0
#define HDMI_TX                       ADV7520NK
#else
#define HDMI_TX                       ADV7520
#endif

#define    HPD_INT     0x80
#define    HDCP_INT    ( 0x40 << 8 )
#define    EDID_INT    0x04

#define    VSYNC_INT   0x20
#define    INT_ERROR   ( 0x80 << 8 )
#define    HDCP_AUTH   0x02
#define    RX_SENSE    0x40
#define HTX_FALSE 0
#define HTX_TRUE 1

Colorspace av_inh_colorspace_in;
Range av_inh_colorspace_range;
Timing av_inh_video_timing;
HTX_BYTE* edid_log = NULL;
EdidInfo edid;
HTX_BYTE* default_edid = NULL;
HTX_BOOL csc_444_422;

HTX_INT next_segment_number = HTX_FALSE;
HTX_BOOL edid_ready       = HTX_FALSE;
HTX_BOOL edid_retry_mode  = HTX_FALSE;
HTX_BYTE edid_retry_count = 0;





HTX_INT handle_edid_interrupt(void);

enum myerrorcode {
    NO_ERROR        = 0,
    BAD_BKSV        = 1,
    RI_MISMATCH     = 2,
    PJ_MISMATCH     = 3,
    I2C_ERROR       = 4,
    RPTR_TIMEOUT    = 5,
    MAX_CASCADE     = 6,
    SHA1_FAIL       = 7,
    MAX_DEV         = 8,
    HDCP_LOST       = 10,
    BAD_VID_SETUP   = 11,
    BAD_INPUT_SETUP = 12,
    EDID_ERROR      = 13,
    HDCP_NOT_SUP    = 14,
    EDID_NACK       = 15,
    EDID_CS_FAIL    = 16
};


HTX_INT next_segment          = 0;
HTX_INT edid_block_map_offset = 0;

const HTX_BYTE identity_matrix[24] =
    { 0xA8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0 };
const HTX_BYTE rgb_0_sdtv_ycrcb_16[24] =
    { 0x87, 0x06, 0x1A, 0x1D, 0x1E, 0xDE, 0x08, 0x00, 0x04, 0x1C, 0x08, 0x10,
      0x01, 0x91, 0x01, 0x00, 0x1D, 0xA2, 0x1B, 0x59, 0x07, 0x06, 0x08, 0x00 };
const HTX_BYTE hdtv_ycrcb_16_rgb_0[24] =
    { 0xE7, 0x2C, 0x04, 0xA7, 0x00, 0x00, 0x1C, 0x1F, 0x1D, 0xDD, 0x04, 0xA7,
      0x1F, 0x26, 0x01, 0x34, 0x00, 0x00, 0x04, 0xA7, 0x08, 0x75, 0x1B, 0x7A };
const HTX_BYTE ycbcr_0_ycbcr_16[24] =
    { 0x8E, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0D, 0xBC,
      0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0E, 0x0D, 0x01, 0x00 };
const HTX_BYTE sdtv_ycrcb_0_rgb_16[24] =
    { 0xA9, 0x6C, 0x06, 0xDF, 0x00, 0x00, 0x1B, 0xBA, 0x1B, 0x33, 0x06, 0xDF,
      0x1D, 0xB0, 0x03, 0xFC, 0x00, 0x00, 0x06, 0xDF, 0x0B, 0xE7, 0x1A, 0x7A };
const HTX_BYTE sdtv_ycrcb_16_rgb_0[24] =
    { 0xE6, 0x62, 0x04, 0xA7, 0x00, 0x00, 0x1C, 0x84, 0x1C, 0xBF, 0x04, 0xA7,
      0x1E, 0x6F, 0x02, 0x1D, 0x00, 0x00, 0x04, 0xA7, 0x08, 0x12, 0x1B, 0xAC };
const HTX_BYTE sdtv_ycrcb_rgb[24] =
    { 0xAA, 0xE6, 0x07, 0xF3, 0x00, 0x00, 0x1A, 0x98, 0x1A, 0x73, 0x07, 0xF3,
      0x1D, 0x54, 0x04, 0x28, 0x00, 0x00, 0x07, 0xF3, 0x0D, 0xC5, 0x19, 0x28 };
const HTX_BYTE hdtv_ycrcb_rgb[24] =
    { 0xAC, 0x45, 0x07, 0xF7, 0x00, 0x00, 0x19, 0xE8, 0x1C, 0x58, 0x07, 0xF7,
      0x1E, 0x97, 0x02, 0x90, 0x00, 0x00, 0x07, 0xF7, 0x0E, 0x78, 0x18, 0xD0 };
const HTX_BYTE hdtv_ycrcb_0_rgb_16[24] =
    { 0xAA, 0x93, 0x06, 0xDF, 0x00, 0x00, 0x1B, 0x24, 0x1C, 0xD9, 0x06, 0xDF,
      0x1E, 0xBE, 0x02, 0xA2, 0x00, 0x00, 0x06, 0xDF, 0x0C, 0x72, 0x1A, 0x35 };
const HTX_BYTE rgb_0_hdtv_ycrcb_16[24] =
    { 0x87, 0x06, 0x19, 0x9E, 0x1F, 0x5D, 0x08, 0x00, 0x02, 0xED, 0x09, 0xD2,
      0x00, 0xFD, 0x01, 0x00, 0x1E, 0x63, 0x1A, 0x98, 0x07, 0x06, 0x08, 0x00 };
const HTX_BYTE rgb_sdtv_ycrcb[24] =
    { 0x88, 0x2D, 0x19, 0x27, 0x1E, 0xAD, 0x08, 0x00, 0x03, 0xA9, 0x09, 0x64,
      0x01, 0xD2, 0x00, 0x00, 0x1D, 0x40, 0x1A, 0x94, 0x08, 0x2D, 0x08, 0x00 };
const HTX_BYTE rgb_hdtv_ycrcb[24] =
    { 0x88, 0x2D, 0x18, 0x94, 0x1F, 0x40, 0x08, 0x00, 0x03, 0x68, 0x0B, 0x70,
      0x01, 0x26, 0x00, 0x00, 0x1E, 0x21, 0x19, 0xB3, 0x08, 0x2D, 0x08, 0x00 };
const HTX_BYTE rgb_16_rgb_0[24] =
    { 0x8D, 0xBC, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0D, 0xBC,
      0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0D, 0xBC, 0x01, 0x20 };
const HTX_BYTE rgb_0_rgb_16[24] =
    { 0xA9, 0x50, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x6B, 0x00, 0x00, 0x09, 0x50,
      0x00, 0x00, 0x1F, 0x6B, 0x00, 0x00, 0x00, 0x00, 0x09, 0x50, 0x1F, 0x6B };




struct hdmi_driver_data_t
{
	struct i2c_client *i2c_hdmi_client;
	struct input_dev *input_hdmi_dev;
	int	irq;
	int     gpio_num;
};


struct hdmi_driver_data_t hdmimain_driver_data;
struct hdmi_driver_data_t hdmicec_driver_data;
static struct hdmi_driver_data_t hdmiedid_driver_data;
static struct hdmi_driver_data_t hdmipkt_driver_data;

static int i2c_hdmi_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int i2c_hdmi_remove(struct i2c_client *client);
static int i2c_hdmiedid_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int i2c_hdmiedid_remove(struct i2c_client *client);
static int i2c_hdmicec_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int i2c_hdmicec_remove(struct i2c_client *client);
static int i2c_hdmipkt_probe(struct i2c_client *client, const struct i2c_device_id *id);
static int i2c_hdmipkt_remove(struct i2c_client *client);



int i2c_hdmi_read(struct i2c_client *client, uint8_t regaddr, uint8_t *buf, uint32_t len);
int i2c_hdmi_write(struct i2c_client *client, uint8_t regaddr, uint8_t *buf, uint32_t len);




static const struct i2c_device_id i2c_hdmiedid_id[] = {
	{ "hdmi_edid", 0 },
	{ }
};
static struct i2c_driver i2c_hdmiedid_driver = {
	.driver = {
		.name = HDMIEDID_NAME,
		.owner = THIS_MODULE,
	},
	.probe	 = i2c_hdmiedid_probe,
	.remove	 = i2c_hdmiedid_remove,
	.id_table = i2c_hdmiedid_id,
};

static int i2c_hdmiedid_probe(struct i2c_client *client, const struct i2c_device_id *id)
{

	struct hdmi_driver_data_t *hdmiedid = &hdmiedid_driver_data;

	printk(KERN_ERR "[HDMI] i2c_hdmiedid_probe() + !!\n");

	client->driver = &i2c_hdmiedid_driver;
	i2c_set_clientdata(client, &hdmiedid_driver_data);
	hdmiedid->i2c_hdmi_client = client;

	printk(KERN_ERR "[HDMI] i2c_hdmiedid_probe() - !!\n");

	return 0;
}
static int i2c_hdmiedid_remove(struct i2c_client *client)
{
	printk(KERN_DEBUG "[HDMI] i2c_hdmiedid_remove+\n[HDMI] i2c_hdmiedid_remove-\n");
	return 0;
}





static const struct i2c_device_id i2c_hdmicec_id[] = {
	{ "hdmi_cec", 0 },
	{ }
};
static struct i2c_driver i2c_hdmicec_driver = {
	.driver = {
		.name = HDMICEC_NAME,
		.owner = THIS_MODULE,
	},
	.probe	 = i2c_hdmicec_probe,
	.remove	 = i2c_hdmicec_remove,
	.id_table = i2c_hdmicec_id,

};
static int i2c_hdmicec_probe(struct i2c_client *client, const struct i2c_device_id *id)
{

	struct hdmi_driver_data_t *hdmicec = &hdmicec_driver_data;

printk(KERN_ERR "[HDMI ]i2c_hdmicec_probe()+\n");

	client->driver = &i2c_hdmicec_driver;
	i2c_set_clientdata(client, &hdmicec_driver_data);
	hdmicec->i2c_hdmi_client = client;



printk(KERN_ERR "[HDMI ]i2c_hdmicec_probe()-\n");
	return 0;
}
static int i2c_hdmicec_remove(struct i2c_client *client)
{
	printk(KERN_DEBUG "[HDMI] i2c_hdmicec_remove+\n[HDMI] i2c_hdmicec_remove-\n");
	return 0;
}





static const struct i2c_device_id i2c_hdmipkt_id[] = {
	{ "hdmi_pkt", 0 },
	{ }
};
static struct i2c_driver i2c_hdmipkt_driver = {
	.driver = {
		.name = HDMIPKT_NAME,
		.owner = THIS_MODULE,
	},
	.probe	 = i2c_hdmipkt_probe,
	.remove	 = i2c_hdmipkt_remove,
	.id_table = i2c_hdmipkt_id,
};
static int i2c_hdmipkt_probe(struct i2c_client *client, const struct i2c_device_id *id)
{

	struct hdmi_driver_data_t *hdmipkt = &hdmipkt_driver_data;

printk(KERN_ERR "[HDMI ] i2c_hdmipkt_probe()+\n");

	client->driver = &i2c_hdmipkt_driver;
	i2c_set_clientdata(client, &hdmipkt_driver_data);
	hdmipkt->i2c_hdmi_client = client;

printk(KERN_ERR "[HDMI ] i2c_hdmipkt_probe()+\n");

	return 0;
}
static int i2c_hdmipkt_remove(struct i2c_client *client)
{
	printk(KERN_DEBUG "[HDMI] i2c_hdmipkt_remove+\n[HDMI] i2c_hdmipkt_remove-\n");
	return 0;
}





static const struct i2c_device_id i2c_hdmi_id[] = {
	{ "hdmi_main", 0 },
	{ }
};

static struct i2c_driver i2c_hdmi_driver = {
	.driver = {
		.name = HDMI_NAME,
		.owner = THIS_MODULE,
	},
	.probe	 = i2c_hdmi_probe,
	.remove	 = i2c_hdmi_remove,
	.id_table = i2c_hdmi_id,
};


void adi_set_i2c_reg( struct i2c_client *client,  uint8_t reg_add, uint8_t mask_reg0,
		      uint16_t bit2shift, uint16_t val )
{
	uint8_t read_i2_data;
	int ret = 0;
	struct hdmi_driver_data_t *hdmi_driver_data_p = &hdmimain_driver_data;


    	read_i2_data=0;
	ret = i2c_hdmi_read(hdmi_driver_data_p->i2c_hdmi_client, reg_add, &read_i2_data, 1);
	if (ret != 0)
	{
		printk(KERN_ERR "[HDMI] adi_set_i2c_reg()::i2c_hdmi_write  fail!!-ret:%d\n", ret);
		
	}

	read_i2_data = ( read_i2_data & ~mask_reg0 ) | ( uint8_t )( ( val << bit2shift ) & mask_reg0 );

	ret = i2c_hdmi_write(hdmi_driver_data_p->i2c_hdmi_client, reg_add, &read_i2_data, 1);
	if (ret != 0)
	{
		printk(KERN_ERR "[HDMI] adi_set_i2c_reg()::i2c_hdmi_write fail!!-ret:%d\n", ret);
		
	}
}

uint8_t adi_get_i2c_reg( struct i2c_client *client, uint8_t reg_add, uint8_t mask_reg0,
			 uint16_t bit2shift )
{
	uint8_t    read_data = 0;
	int ret = 0;
	struct hdmi_driver_data_t *hdmi_driver_data_p = &hdmimain_driver_data;

	read_data=0;
	ret = i2c_hdmi_read(hdmi_driver_data_p->i2c_hdmi_client, reg_add, &read_data, 1);
	if (ret != 0)
	{
		printk(KERN_ERR "[HDMI] adi_get_i2c_reg()::i2c_hdmi_write fail!!-ret:%d\n", ret);
		return 0;
	}

    return( ( uint8_t )read_data & mask_reg0 ) >> bit2shift;
}

void set_EDID_ready( uint8_t value )
{
    edid_ready = value;
}

uint8_t edid_reading_complete( void )
{
    return edid_ready;
}

HTX_BOOL edid_retry( void )
{
	struct hdmi_driver_data_t *hdmi_driver_data_p = &hdmimain_driver_data;

    if( edid_retry_mode ) {
	adi_set_i2c_reg(hdmi_driver_data_p->i2c_hdmi_client,0xC4, 0xff, 0, 0);
	edid_retry_mode = HTX_FALSE;
    }
    else {
        if( edid_retry_count == MAX_EDID_TRIES ) {
	    printk(KERN_ERR "\nMax EDID Tries reached, myerrorcode=[EDID_CS_FAIL]\n" );
            printk(KERN_ERR "EDID_CS_FAIL Controller Error: %x\n", EDID_CS_FAIL );

            if( default_edid ) {
	        handle_edid_interrupt();
            }

            return( HTX_TRUE );
        }
        else {
	    edid_retry_count++;
        }

	adi_set_i2c_reg(hdmi_driver_data_p->i2c_hdmi_client,0xC4, 0xff, 0, 1);
	edid_retry_mode = HTX_TRUE;
    }

    return( HTX_FALSE );
}

uint8_t edid_dat[256];
HTX_INT handle_edid_interrupt( void )
{
	int i;
	
	struct hdmi_driver_data_t *hdmiedid = &hdmiedid_driver_data;
	uint8_t reg_add, interrupt_registers;
	int ret = 0;
	struct hdmi_driver_data_t *hdmi_driver_data_p = &hdmimain_driver_data;
	struct i2c_client *client = hdmi_driver_data_p->i2c_hdmi_client;

	if( edid_retry_mode )
	{
		edid_retry();
		return( -2 );
	}

	
	if( default_edid )
	{
		memcpy( edid_dat, default_edid, 256 );
		default_edid = NULL;
	}
	else
	{
		
		for (i=0;i<256;i++)
			edid_dat[i]=0;

	    	reg_add = 0;
	    	interrupt_registers=0;
	    	ret = i2c_hdmi_read(hdmiedid->i2c_hdmi_client, reg_add, edid_dat, 256);
	    	if (ret != 0)
	    	{
	    		printk(KERN_ERR "[HDMI] handle_edid_interrupt()::i2c_hdmi_read  fail!!-ret:%d\n", ret);
	    		
	    	}
#ifdef HDMI_EDID_DEBUG
		
		printk(KERN_ERR "\n@@@@@@@@@@@@@@@@@@@@\n");
		for (i=0;i<256;i++)
		{
			if((i%16)==0)
				printk(KERN_ERR "\n");
			printk(KERN_ERR "%3x ", edid_dat[i]);
		}
		printk(KERN_ERR "\n@@@@@@@@@@@@@@@@@@@@\n");
#endif
	}

	if( edid_log ) 
	{
		if( !next_segment_number )
			memcpy( edid_log, edid_dat, 256 );
		else if( next_segment_number == 1 )
			memcpy( edid_log + 256, edid_dat, 256 );
	}

	edid = init_edid_info( edid );
	next_segment_number = parse_edid( edid, edid_dat );




	printk(KERN_ERR "[EDID Info] ################################\n");
	printk(KERN_ERR "\n\n== VendorProductId ==\n");
	printk(KERN_ERR "[EDID Info] edid->vpi->manuf=%s\n", edid->vpi->manuf);
	printk(KERN_ERR "[EDID Info] edid->vpi->prodcode=%d %d %d\n", edid->vpi->prodcode[2], edid->vpi->prodcode[1], edid->vpi->prodcode[0]);
	printk(KERN_ERR "[EDID Info] edid->vpi->serial=%d %d\n", edid->vpi->serial[1], edid->vpi->serial[0]);
	printk(KERN_ERR "[EDID Info] edid->vpi->week=%d\n", edid->vpi->week);
	printk(KERN_ERR "[EDID Info] edid->vpi->year=%d\n", edid->vpi->year);
	printk(KERN_ERR "[EDID Info] edid->mdb->mon_name=%s\n", edid->mdb->mon_name); 
	printk(KERN_ERR "[EDID Info] edid->cea->cea->vsdb_hdmi=%d\n", edid->cea->cea->vsdb_hdmi);
	printk(KERN_ERR "[EDID Info] ################################\n");


	printk(KERN_ERR "[EDID Info] next_segment_number=%d\n", next_segment_number);

	
	
	if(next_segment_number == 0)
	{
		adi_set_i2c_reg( client, 0x94, 0x04, 2, 0x0);
		printk(KERN_ERR "[EDID Info] no more next_segment_number[%d], we disable EDID_INT here!!\n", next_segment_number);
	}

	return next_segment_number;
}


#if defined( HDCP_OPTION )

void enable_hdcp(void)
{
	struct hdmi_driver_data_t *hdmi_driver_data_p = &hdmimain_driver_data;

	hdcp_requested = 1;
	adi_set_i2c_reg(  hdmi_driver_data_p->i2c_hdmi_client, 0xAF, 0x80, 7, 1 );
	adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client,0xAF, 0x10, 4, 1);
}

void disable_hdcp( void )
{
	
	struct hdmi_driver_data_t *hdmi_driver_data_p = &hdmimain_driver_data;

	adi_set_i2c_reg(hdmi_driver_data_p->i2c_hdmi_client,0xAF, 0x10, 4, 0);
	
	adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0xAF, 0x80, 7, 0);
	printk(KERN_ERR "Restoring Video\n" );
	av_inhibit( HTX_FALSE, av_inh_colorspace_in, av_inh_colorspace_range, av_inh_video_timing );

	hdcp_enabled = 0;
	hdcp_requested = 0;
	hdcp_status = 0;
}

HTX_BOOL get_hdcp_status( void )
{
    return hdcp_status;
}

HTX_BOOL get_hdcp_requested( void )
{
    return hdcp_requested;
}

void send_audio_video_from_MPEG_decoder( void )
{
    
}




HTX_BYTE bksv_callback_function( HTX_BYTE *bksvs, int count,HTX_BYTE *hdcp_bstatus )
{
    int i;

    printk(KERN_ERR"Comparing BKSV list with revocation list\n");
    printk(KERN_ERR"BKSV =");

    for(i=0; i < count*5; i++) {
        printk(KERN_ERR"%2.2x  ", bksvs[i]);
    }
    printk(KERN_ERR"\n");

    
    
    send_audio_video_from_MPEG_decoder();

    return 0;
}




static void hdcp_timer_2_second_handler( struct work_struct *work )
{
	struct hdmi_driver_data_t *hdmi_driver_data_p = &hdmimain_driver_data;

	

	if(get_hdcp_requested())
	{
		printk(KERN_ERR "get_hdcp_requested(), disable_hdcp\n" );
		disable_hdcp();
	}
	else
	{
		printk(KERN_ERR "get_hdcp_requested(), enable_hdcp\n" );
		enable_hdcp();
	}

	if( ( hdcp_enabled ) && ( !adi_get_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0xB8, 0x40, 6 ) )
		&& adi_get_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0x42, 0x20, 5 ) )
	{
	        printk(KERN_ERR  "Unexpected loss of HDCP, Stop audio/video transmission and reauthenticate\n" );
		adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0xAF, 0x80, 7, 1 );
		hdcp_status = HTX_FALSE;
		hdcp_enabled = HTX_FALSE;
		hdcp_requested = HTX_TRUE;
		printk(KERN_ERR "HDCP_LOST Controller Error: %x\n", HDCP_LOST );
	}
	schedule_delayed_work( &g_hdcpWork, (CHECK_PERIOD_MS * HZ / 1000) );

	
}
#endif

void av_inhibit( HTX_BOOL enable, Colorspace colorspace_in, Range colorspace_range, Timing vic )
{
	struct hdmi_driver_data_t *hdmi_driver_data_p = &hdmimain_driver_data;

	if( enable ) {
	        if( colorspace_in == RGB ) {
			adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0x16, 0x1, 0, 0 );
	        }
	        else {
			adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0x16, 0x1, 0, 1 );
	        }
		adi_set_i2c_reg(  hdmi_driver_data_p->i2c_hdmi_client, 0xD5, 0x1, 0, 1 );
	}
	else {
		adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0xD5, 0x1, 0, 0 );
	}
}

void handle_rx_sense_interrupt( void )
{
	struct hdmi_driver_data_t *hdmi_driver_data_p = &hdmimain_driver_data;

	
	if( adi_get_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0x42, 0x20, 5 ) && adi_get_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0x42, 0x40, 6 ) )
	{
	#if defined( AUTO_MUTE )
        	tmds_powerdown( 0xF );
	#endif
	        adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0x41, 0x40, 6, 0 );
        	printk(KERN_ERR "POWER UP\n" );

	#if defined( HDCP_OPTION )

	#endif
	}
	else
	{
	#if defined( AUTO_MUTE )
		tmds_powerdown( 0xF );
	#endif
        	printk(KERN_ERR "POWER DOWN\n" );
	}
	
}

int handle_hpd_interrupt(void)
{
	struct hdmi_driver_data_t *hdmi_driver_data_p = &hdmimain_driver_data;
	struct i2c_client *client = hdmi_driver_data_p->i2c_hdmi_client;
	int ret;

	
	if( edid )
	{
		printk(KERN_ERR "clearing EDID\n" );
		clear_edid( edid );
	}

	set_EDID_ready( HTX_FALSE );

	edid_retry_mode  = HTX_FALSE;
	edid_retry_count = 0;
	next_segment_number = 0;

	if( !adi_get_i2c_reg( client, 0x42, 0x40, 6 ))
	{
		printk(KERN_ERR "[HPD] HPD unplugged\n");
		adi_set_i2c_reg( client, 0x96, 0x80, 7, 1 );
		
		ret = HTX_FALSE; 
	}
	else
	{
		printk(KERN_ERR "[HPD] HPD plugged\n");
		adi_set_i2c_reg( client, 0xA1, 0x40, 6, 0 );
		adi_set_i2c_reg( client, 0x96, 0x80, 7, 1 );


#ifdef HDCP_OPTION
		
		adi_set_i2c_reg( client, 0x94, 0xff, 0, 0xC6);
#else
		
		adi_set_i2c_reg( client, 0x94, 0xff, 0, 0xC4);
#endif

	#if defined( CEC_AVAILABLE )
		
		adi_set_i2c_reg( client, 0x95, 0xfc, 2, 0x3f );
	#else
		
		adi_set_i2c_reg( client, 0x95, 0xfc, 2, 0x30 );
	#endif

	#if defined( CEC_EDID )
		
		adi_set_i2c_reg( client, 0x41, 0x40, 6, 0 );
	#endif

	#if defined( NO_REPEATED_REREAD )
		adi_set_i2c_reg( client, 0xC9, 0xf, 0, MAX_EDID_TRIES );
	#endif
	        if( adi_get_i2c_reg( client, 0x42, 0x20, 5 ) )
		{
			handle_rx_sense_interrupt();
			printk(KERN_ERR "[HPD] Rx Sense already high before HPD\n");
	        }
		ret = HTX_TRUE; 
	}

	return ret;
}

int i2c_hdmi_read(struct i2c_client *client, uint8_t regaddr, uint8_t *buf, uint32_t len)
{
	int ret = 0;
	uint8_t ldat = regaddr;
	struct i2c_msg msgs[] = {
		[0] = {
			.addr	= client->addr,
			.flags	= 0,
			.buf	= (void *)&ldat,
			.len	= 1
		},
			[1] = {
			.addr	= client->addr,
			.flags	= I2C_M_RD,
			.buf	= (void *)buf,
			.len	= len
		}
	};

	ret = i2c_transfer(client->adapter, msgs, 2);

    if( ret != ARRAY_SIZE(msgs) )
    {
        printk(KERN_ERR "i2c_hdmi_read: read %d bytes return failure,buf=0x%xh , len=%d\n", ret, (unsigned int)buf, len );
        return ret;
    }

    return 0;
}

int i2c_hdmi_write(struct i2c_client *client, uint8_t regaddr, uint8_t *buf, uint32_t len)
{
	int ret = 0;
	uint8_t write_buf[len+1];
	struct i2c_msg msgs[] = {
		[0] = {
			.addr	= client->addr,
			.flags	= 0,
			.buf	= (void *)write_buf,
			.len	= len +1,
			},
		};

	write_buf[0] = regaddr;
	memcpy((void *)(write_buf + 1), buf, len);
	ret = i2c_transfer(client->adapter, msgs, 1);

    if( ret != ARRAY_SIZE(msgs) )
    {
        printk(KERN_ERR "i2c_hdmi_read: read %d bytes return failure, buf=0x%xh, len=%d\n", ret, (unsigned int)write_buf, len );
        return ret;
    }

    return 0;
}
EXPORT_SYMBOL(i2c_hdmi_read);
EXPORT_SYMBOL(i2c_hdmi_write);

static int lcdc_hdmi_release_gpio(int hdmi_irqpin)
{
	printk("lcdc_hdmi_release_gpio: releasing keyboard gpio pins %d\n",hdmi_irqpin);
	gpio_free(hdmi_irqpin);
	return 0;
}

void hdmi_all_powerup(void)
{
	printk(KERN_ERR "[HDMI]hdmi_all_powerup\n");

	
	atomic_set(&hdmi_initialed, HTX_FALSE);



	printk(KERN_ERR "GPIO 161 on\n");
	
	gpio_set_value(161,1);

	hdmi_chip_powerup();

	hdmi_init();
	
	
	atomic_set(&hdmi_initialed, HTX_TRUE);

	
	
	modify_cec_slave_addr();
}

void hdmi_all_powerdown(void)
{
	printk(KERN_ERR "[HDMI]hdmi_all_powerdown\n");

	
	hdmi_chip_powerdown();

	printk(KERN_ERR "GPIO 161 off\n");
	
	gpio_set_value(161,0);



}


void hdmi_chip_powerup(void)
{
	struct vreg	*vreg_hdmi = vreg_get(0, "rfrx2");
	struct vreg	*vreg_hdmi_5v = vreg_get(0, "boost");

	
	vreg_enable(vreg_hdmi_5v);
	vreg_set_level(vreg_hdmi_5v, 5000);
	printk(KERN_ERR "[HDMI] turn on vreg[boost] as 5.0V for HDMI IC\n");

	
	vreg_enable(vreg_hdmi);
	vreg_set_level(vreg_hdmi, 1800);
	printk(KERN_ERR "[HDMI] turn on vreg[rfrx2] as 1.8V for HDMI IC\n");

	hdmi_pwr_counter++;

	if(dynamic_log_ebable)
		printk(KERN_ERR "UP: hdmi_pwr_counter=%d\n", hdmi_pwr_counter);

	PM_LOG_EVENT(PM_LOG_ON, PM_LOG_HDMI);
}

void hdmi_chip_powerdown(void)
{
	struct vreg	*vreg_hdmi = vreg_get(0, "rfrx2");
	struct vreg	*vreg_hdmi_5v = vreg_get(0, "boost");

	
	vreg_disable(vreg_hdmi);
	printk(KERN_ERR "[HDMI] turn off vreg[rfrx2] for HDMI IC\n");

	
	vreg_disable(vreg_hdmi_5v);
	printk(KERN_ERR "[HDMI] turn off vreg[boost] for HDMI IC\n");

	hdmi_pwr_counter--;

	if(dynamic_log_ebable)
		printk(KERN_ERR "Down: hdmi_pwr_counter=%d\n", hdmi_pwr_counter);
	if(hdmi_pwr_counter>0)
		printk(KERN_ERR "Warning !!  HDMI power exist!! hdmi_pwr_counter=%d\n", hdmi_pwr_counter);

	PM_LOG_EVENT(PM_LOG_OFF, PM_LOG_HDMI);
}



 static int lcdc_hdmi_config_gpio(int hdmi_irqpin)
{
	int ret;

#ifdef INCREASE_OUTPUT_CURRENT
	int gpio_num;

	for(gpio_num=111 ; gpio_num<=138 ; gpio_num++)
	{
		gpio_tlmm_config(GPIO_CFG(gpio_num, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), GPIO_CFG_ENABLE);
	}
	
	gpio_tlmm_config(GPIO_CFG(135, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_8MA), GPIO_CFG_ENABLE);
	printk("increase LCDC output current.\n");
#endif










#ifdef HDMI_TESTMODE



	printk(KERN_ERR "HDMI run at test mode..., but not power on\n");

	gpio_tlmm_config(GPIO_CFG(44, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_set_value(44,0);

	gpio_tlmm_config(GPIO_CFG(35, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);

	if(system_rev <= EVT1_4G_4G) 
	{
		gpio_tlmm_config(GPIO_CFG(85, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
		gpio_set_value(85,1);
	}
	else 
	{
		gpio_tlmm_config(GPIO_CFG(48, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
		gpio_set_value(48,1);
	}

#if HDMI_PWR_ONOFF_BY_AP

#else
	hdmi_chip_powerup();
#endif
	gpio_tlmm_config(GPIO_CFG(161, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_set_value(161,0);

	printk(KERN_ERR "set HDMI related GPIOs done. (test mode)\n");


#else



	printk(KERN_ERR "HDMI run at normal mode...\n");

	
	hdmi_chip_powerdown();

	gpio_tlmm_config(GPIO_CFG(44, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_set_value(44,1);

	
	if(system_rev <= EVT1_4G_4G) 
	{
		gpio_tlmm_config(GPIO_CFG(85, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
		gpio_set_value(85,0);
	}
	else 
	{
		gpio_tlmm_config(GPIO_CFG(48, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
		gpio_set_value(48,0);
	}

	gpio_tlmm_config(GPIO_CFG(161, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
#ifndef HDMI_5V_ALWAYS_ON
	gpio_set_value(161,0);
#else 
	gpio_set_value(161,1);
	printk(KERN_ERR "HDMI_5v always ON\n");
#endif

	gpio_tlmm_config(GPIO_CFG(HDMI_CABLEIN_GPIO, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);

	ret = gpio_request(HDMI_CABLEIN_GPIO, "gpio_hdmi_cablein_irq");
	if (ret) {
		printk("lcdc_hdmi_config_gpio: gpio_request failed on pin %d\n",HDMI_CABLEIN_GPIO);
		goto err_gpioconfig1;
	}
	ret = gpio_direction_input(HDMI_CABLEIN_GPIO);
	if (ret) {
		printk("lcdc_hdmi_config_gpio: gpio_direction_input failed on pin %d\n",HDMI_CABLEIN_GPIO);
		goto err_gpioconfig1;
	}






#endif

	gpio_tlmm_config(GPIO_CFG(hdmi_irqpin, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);

	ret = gpio_request(hdmi_irqpin, "gpio_hdmi_irq");
	if (ret) {
		printk("lcdc_hdmi_config_gpio: gpio_request failed on pin %d\n",hdmi_irqpin);
		goto err_gpioconfig;
	}
	ret = gpio_direction_input(hdmi_irqpin);
	if (ret) {
		printk("lcdc_hdmi_config_gpio: gpio_direction_input failed on pin %d\n",hdmi_irqpin);
		goto err_gpioconfig;
	}





	return ret;

#ifdef HDMI_TESTMODE
err_gpioconfig:
	lcdc_hdmi_release_gpio(hdmi_irqpin);
#else
err_gpioconfig:
	lcdc_hdmi_release_gpio(hdmi_irqpin);
err_gpioconfig1:
	lcdc_hdmi_release_gpio(HDMI_CABLEIN_GPIO);
#endif
	return ret;
}

void initialize_HDMI_Tx( void )
{
	struct hdmi_driver_data_t *hdmi_driver_data_p = &hdmimain_driver_data;

#if defined( HDCP_OPTION )
    
    if( HDMI_TX == ADV7520 )
    {
    	adi_set_i2c_reg(hdmi_driver_data_p->i2c_hdmi_client,0xBA, 0x10, 4, 1);
    }
    else
    {
	adi_set_i2c_reg(hdmi_driver_data_p->i2c_hdmi_client,0xBA, 0x10, 4, 0);
    }
#endif

    
#if defined( AUTO_MUTE )
    av_mute_on();
    tmds_powerdown( 0xF );
    ADV752X_MAIN_I2S_ENABLE_0_SET( 0 );
    ADV752X_MAIN_I2S_ENABLE_1_SET( 0 );
    ADV752X_MAIN_I2S_ENABLE_2_SET( 0 );
    ADV752X_MAIN_I2S_ENABLE_3_SET( 0 );
#endif

    
    adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0x41, 0x40, 6, 0 );

    
    adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0x0A, 0x60, 5, 0 );
    adi_set_i2c_reg(hdmi_driver_data_p->i2c_hdmi_client,0x98, 0xf, 0, 3);

    adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0x9C, 0xff, 0, 0x38 );
    adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0x9D, 0x3, 0, 1 );
    adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0xA2, 0xFF, 0, 0x94 );
    adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0xA3, 0xFF, 0, 0x94 );

    adi_set_i2c_reg(hdmi_driver_data_p->i2c_hdmi_client,0xBB, 0xff, 0, 0xFF);
    adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0xBA, 0xe0, 5, 3 );
    adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0x56, 0xf, 0, 0x8 );
    adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0xDE, 0xc0, 6, 2 );
    adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0xDE, 0x3e, 1, 4 );

    printk(KERN_ERR "HDMI Tx Presets Loaded\n" );
}

HTX_INT dvi_supported_video_format( void )
{
    HTX_INT formats = 0;
    HTX_INT i = 0;

    for( i = 0; i < DTD_MAX; i++ )
    {
        if( ( edid->dtb[i].pix_clk == CLK_74_25_MHZ ) && ( edid->dtb[i].h_active == 1920 )
	    && ( edid->dtb[i].v_active == 540 ) && ( edid->dtb[i].h_blank == 280 ) ) {

	    formats |= ( 1 << HTX_1080i_30 );  
	}

	if( ( edid->dtb[i].pix_clk == CLK_74_25_MHZ ) && ( edid->dtb[i].h_active == 1920 )
	    && ( edid->dtb[i].v_active == 540 ) && ( edid->dtb[i].h_blank == 720 ) ) {

	   formats |= ( 1 << HTX_1080i_25 );   
	}

	if( ( edid->dtb[i].pix_clk == CLK_74_25_MHZ ) && ( edid->dtb[i].h_active == 1280 )
	    && ( edid->dtb[i].v_active == 720 ) && ( edid->dtb[i].h_blank == 370 ) ) {
	    formats |= ( 1 << HTX_720p_60 );   
	}

	if( ( edid->dtb[i].pix_clk == CLK_74_25_MHZ ) && ( edid->dtb[i].h_active == 1280 )
	    && ( edid->dtb[i].v_active == 720 ) && ( edid->dtb[i].h_blank == 700 ) ) {
	    formats |= ( 1 << HTX_720p_50 );   
	}

	if( ( edid->dtb[i].pix_clk == CLK_27_MHZ ) && ( edid->dtb[i].h_active == 720 )
	    && ( edid->dtb[i].v_active == 480 ) ) {
	    formats |= ( 1 << HTX_720_480p );  
	}

	if( ( edid->dtb[i].pix_clk == CLK_27_MHZ ) && ( edid->dtb[i].h_active == 720 )
	    && ( edid->dtb[i].v_active == 576 ) ) {
	    formats |= ( 1 << HTX_720_576p );  
	}

	if( ( edid->dtb[i].pix_clk == CLK_27_MHZ ) && ( edid->dtb[i].h_active == 1440 )
	    && ( edid->dtb[i].v_active == 240 ) && ( edid->dtb[i].h_blank == 280 ) ) {
	    formats |= ( 1 << HTX_480i );      
	}

	if( ( edid->dtb[i].pix_clk == CLK_27_MHZ ) && ( edid->dtb[i].h_active == 1440 )
	    && ( edid->dtb[i].v_active == 288 ) ) {
	    formats |= ( 1 << HTX_576i );      
	}

	if( ( ( edid->dtb[i].pix_clk == CLK_25_175_MHZ )
	      || ( edid->dtb[i].pix_clk == CLK_25_175_MHZ + 1 ) )
	      && ( edid->dtb[i].h_active == 640 ) && ( edid->dtb[i].v_active == 480 ) )
	{
	    
	    formats |= ( 1 << HTX_640_480p );
	}

	if( edid->et->m640_480_60 )
	{
	    formats |= ( 1 << HTX_640_480p );  
	}
    }

    return formats;
}

void set_input_format( HTX_INT input_format, HTX_INT input_style,
		       HTX_INT input_bit_width, HTX_INT clock_edge )
{
	struct hdmi_driver_data_t *hdmi_driver_data_p = &hdmimain_driver_data;

    adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0x15, 0xe, 1, input_format );
    adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0x16, 0x30, 4, input_bit_width );
    adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0x16, 0xc, 2, input_style );
    adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0x16, 0x2, 1, clock_edge );

    
    if( input_format == 6 || input_format == 5 ) {
        adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0xD0, 0xc, 2, 3 );
    }
    else {
        adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0xD0, 0xc, 2, 0 );
    }

    

        if( ( input_format == 1 ) || ( input_format == 2 ) ) {
	    adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0xD6, 0x80, 7, 1 );
        }
        else {
	    adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0xD6, 0x80, 7, 0 );
        }

    
    

    if( edid->cea->YCC444 ) {
        csc_444_422 = HTX_TRUE;
    }
    else if( edid->cea->YCC422 ) {
        csc_444_422 = HTX_FALSE;
    }
    else {
        csc_444_422 = HTX_TRUE;
    }
}

void hdmi_or_dvi( void )
{
    struct hdmi_driver_data_t *hdmi_driver_data_p = &hdmimain_driver_data;

    if( edid_ready ) {
        if( edid->cea->cea->vsdb_hdmi )
        {
            adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0xAF, 0x2, 1, 1 );
            printk(KERN_ERR "\n set as HDMI interface\n");
        }
	else
	{
            adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0xAF, 0x2, 1, 0 );
            printk(KERN_ERR "\n DVI interface\n");
        }
    }
    else
    {
        adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0xAF, 0x2, 1, 0 );
        printk(KERN_ERR "\n default DVI interface\n");
    }
}

void setup_colorspace( Colorspace colorspace_in, Range colorspace_range, Timing vic )
{
    
    HTX_INT space_in  = 1;   
    HTX_INT space_out = 1;   
    HTX_INT rgb_out = 0;
    struct hdmi_driver_data_t *hdmi_driver_data_p = &hdmimain_driver_data;

    printk(KERN_ERR  "VIC = %d\n", vic );

    if( colorspace_in == RGB ) {
        if( colorspace_range == HTX_0_255 ) {
	    space_in = 0;
        }
        else if( colorspace_range == HTX_16_235 ) {
	    space_in = 1;
        }
        else {
	    printk(KERN_ERR "Error: Colorspace in not supported, [BAD_INPUT_SETUP]" );
            printk(KERN_ERR "BAD_INPUT_SETUP Controller Error: %x\n", BAD_INPUT_SETUP );
        }
    }
    else if( colorspace_in == YCbCr709 ) {
        if( colorspace_range == HTX_0_255 ) {
	    space_in = 2;
        }
        else if( colorspace_range == HTX_16_235 ) {
	    space_in = 3;
        }
        else {
	    printk(KERN_ERR "Error: Colorspace in not supported, [BAD_INPUT_SETUP]" );
            printk(KERN_ERR "BAD_INPUT_SETUP Controller Error: %x\n", BAD_INPUT_SETUP );
        }
    }
    else if( colorspace_in == YCbCr601 ) {
        if( colorspace_range == HTX_0_255 ) {
	    space_in = 4;
	}
        else if( colorspace_range == HTX_16_235 ) {
	    space_in = 5;
        }
        else {
	    printk(KERN_ERR "Error: Problem selecting colorspace, BAD_INPUT_SETUP" );
            printk(KERN_ERR "BAD_INPUT_SETUP Controller Error: %x\n", BAD_INPUT_SETUP );
        }
    }
    else {
        printk(KERN_ERR  "Error: Problem selecting colorspace, [BAD_INPUT_SETUP]" );
        printk(KERN_ERR "BAD_INPUT_SETUP Controller Error: %x\n", BAD_INPUT_SETUP );
    }

    if( vic == HTX_640_480p ) {
        rgb_out = 0;
    }
    else {
        rgb_out = 1;
    }

    
    if( ( space_in == 0 ) || ( space_in == 1 ) ) {
        space_out = rgb_out;
    }

    if( space_in == 2 ) {
        if( ( ( edid->cea->YCC444 ) == HTX_TRUE ) || ( ( edid->cea->YCC422 ) == HTX_TRUE ) )
            space_out = 3;
        else
            space_out = rgb_out;
    }

    if( space_in == 4 ) {
        if( ( ( edid->cea->YCC444 ) == HTX_TRUE ) || ( ( edid->cea->YCC422 ) == HTX_TRUE ) )
	    space_out = 5;
	else
            space_out = rgb_out;
    }

    if( ( space_in == 3 ) || ( space_in == 5 ) ) {
        if( ( ( edid->cea->YCC444 ) == HTX_TRUE ) || ( ( edid->cea->YCC422 ) == HTX_TRUE ) )
            space_out = space_in;
	else
            space_out = rgb_out;
    }

    
    if( !edid->cea->cea->vsdb_hdmi ) {
        space_out = rgb_out;
    }

    
    if( space_out == space_in ) {
        if( colorspace_in == RGB ) {
	    adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0x16, 0xc0, 6, 0 );
	    adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0x55, 0x60, 5, 0 );
	}
	else if( ( ( colorspace_in == YCbCr709 ) || ( colorspace_in == YCbCr601 ) )
		 && !csc_444_422 ) {
		adi_set_i2c_reg(  hdmi_driver_data_p->i2c_hdmi_client, 0x16, 0xc0, 6, 2 );
		adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0x55, 0x60, 5, 1 );
	    
		adi_set_i2c_reg(  hdmi_driver_data_p->i2c_hdmi_client, 0x18, 0x80, 7, 0 );
	    return;
	}
	else if( ( ( colorspace_in == YCbCr709 ) || ( colorspace_in == YCbCr601 ) )
		 && csc_444_422 ) {
		adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0x16, 0xc0, 6, 1 );
		adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0x55, 0x60, 5, 2 );
	}
	else {
		adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0x16, 0xc0, 6, 0 );
		adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0x55, 0x60, 5, 0 );
	}
    }
    else {
	adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0x16, 0xc0, 6, 0 );
	adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0x55, 0x60, 5, 0 );
    }

    printk(KERN_ERR  "SPACE IN  = %d\n", space_in );
     printk(KERN_ERR  "SPACE OUT = %d\n", space_out );

    if( space_in == space_out ) {
        i2c_hdmi_write( hdmi_driver_data_p->i2c_hdmi_client, 0x18, ( HTX_BYTE* )identity_matrix, 24 );
    }
    if( ( space_in == 0 ) && ( space_out == 1 ) ) {
        i2c_hdmi_write( hdmi_driver_data_p->i2c_hdmi_client, 0x18, ( HTX_BYTE* )rgb_0_rgb_16, 24 );
    }
    if( ( space_in == 1 ) && ( space_out == 0 ) ) {
        i2c_hdmi_write( hdmi_driver_data_p->i2c_hdmi_client, 0x18, ( HTX_BYTE* )rgb_16_rgb_0, 24 );
    }
    if( ( space_in == 0 ) && ( space_out == 3 ) ) {
        i2c_hdmi_write( hdmi_driver_data_p->i2c_hdmi_client, 0x18, ( HTX_BYTE* )rgb_0_hdtv_ycrcb_16, 24 );
    }
    if( ( space_in == 1 ) && ( space_out == 3 ) ) {
        i2c_hdmi_write( hdmi_driver_data_p->i2c_hdmi_client, 0x18, ( HTX_BYTE* )rgb_hdtv_ycrcb, 24 );
    }
    if( ( space_in == 2 ) && ( space_out == 0 ) ) {
        i2c_hdmi_write( hdmi_driver_data_p->i2c_hdmi_client, 0x18, ( HTX_BYTE* )hdtv_ycrcb_rgb, 24 );
    }
    if( ( space_in == 2 ) && ( space_out == 1 ) ) {
        i2c_hdmi_write( hdmi_driver_data_p->i2c_hdmi_client, 0x18, ( HTX_BYTE* )hdtv_ycrcb_0_rgb_16, 24 );
    }
    if( ( space_in == 2 ) && ( space_out == 3 ) ) {
        i2c_hdmi_write( hdmi_driver_data_p->i2c_hdmi_client, 0x18, ( HTX_BYTE* )ycbcr_0_ycbcr_16, 24 );
    }
    if( ( space_in == 3 ) && ( space_out == 0 ) ) {
        i2c_hdmi_write( hdmi_driver_data_p->i2c_hdmi_client, 0x18, ( HTX_BYTE* )hdtv_ycrcb_16_rgb_0, 24 );
    }
    if( ( space_in == 3 ) && ( space_out == 1 ) ) {
        i2c_hdmi_write( hdmi_driver_data_p->i2c_hdmi_client, 0x18, ( HTX_BYTE* )hdtv_ycrcb_rgb, 24 );
    }
    if( ( space_in == 0 ) && ( space_out == 5 ) ) {
        i2c_hdmi_write( hdmi_driver_data_p->i2c_hdmi_client, 0x18, ( HTX_BYTE* )rgb_0_sdtv_ycrcb_16, 24 );
    }
    if( ( space_in == 1 ) && ( space_out == 5 ) ) {
        i2c_hdmi_write( hdmi_driver_data_p->i2c_hdmi_client, 0x18, ( HTX_BYTE* )rgb_sdtv_ycrcb, 24 );
    }
    if( ( space_in == 4 ) && ( space_out == 0 ) ) {
        i2c_hdmi_write( hdmi_driver_data_p->i2c_hdmi_client, 0x18, ( HTX_BYTE* )sdtv_ycrcb_rgb, 24 );
    }
    if( ( space_in == 4 ) && ( space_out == 1 ) ) {
        i2c_hdmi_write( hdmi_driver_data_p->i2c_hdmi_client, 0x18, ( HTX_BYTE* )sdtv_ycrcb_0_rgb_16, 24 );
    }
    if( ( space_in == 4 ) && ( space_out == 5 ) ) {
        i2c_hdmi_write( hdmi_driver_data_p->i2c_hdmi_client, 0x18, ( HTX_BYTE* )ycbcr_0_ycbcr_16, 24 );
    }
    if( ( space_in == 5 ) && ( space_out == 0 ) ) {
        i2c_hdmi_write( hdmi_driver_data_p->i2c_hdmi_client, 0x18, ( HTX_BYTE* )sdtv_ycrcb_16_rgb_0, 24 );
    }
    if( ( space_in == 5 ) && ( space_out == 1 ) ) {
        i2c_hdmi_write( hdmi_driver_data_p->i2c_hdmi_client, 0x18, ( HTX_BYTE* )sdtv_ycrcb_rgb, 24 );
    }
}

void set_video_mode( Timing video_timing, Colorspace colorspace_in,
		     Range colorspace_range, AspectRatio aspect_ratio )
{
	struct hdmi_driver_data_t *hdmi_driver_data_p = &hdmimain_driver_data;


    if( aspect_ratio == _4x3 ) {
        adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0x17, 0x2, 1, 0 );
        adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0x56, 0x30, 4, 1 );
    }
    if( aspect_ratio == _16x9 ) {
        adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0x17, 0x2, 1, 1 );
        adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0x56, 0x30, 4, 2 );
    }

    hdmi_or_dvi();
}

void setup_audio(void )
{
	
	uint8_t n[3];
	int 	sample[8] = { 0, 4096, 6272, 6144, 12544, 12288, 25088, 24576 };
	struct hdmi_driver_data_t *hdmi_driver_data_p = &hdmimain_driver_data;


	printk(KERN_ERR " setup HDMI audio start\n");
	
	
	adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0x0a, 0x10, 4, 0 );
	
	adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0x0c, 0x03, 0, 0 );

	
	
	adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0x15, 0xf0, 4, 2 );
	
	n[0] = (uint8_t)( ( sample[3] >> 16 ) & 0x0F );
    	n[1] = (uint8_t)( ( sample[3] >> 8 ) & 0xFF );
   	n[2] = (uint8_t)( sample[3] & 0xFF );
	i2c_hdmi_write( hdmi_driver_data_p->i2c_hdmi_client, 0x01, ( HTX_BYTE* )n, 3 );

	
	adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0x0d, 0x1f, 0, 16 );

	
	adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0x73, 0x07, 0, 1 );
	
	
	
	adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0x0c, 0x3c, 2, 1 );





	
	adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0x76, 0xff, 0, 0 );
	printk(KERN_ERR " setup HDMI audio done\n");
}

void use_internal_DE(void)
{
	struct hdmi_driver_data_t *hdmi_driver_data_p = &hdmimain_driver_data;
	uint8_t hdmi_dummy_addr,hdmi_dummy_data;
	int ret;

	
	
	printk(KERN_ERR "PCLK lock=%d\n", adi_get_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0x9E, 0x10, 4));







	if(current_hdmi_timing == HTX_720p_60)
	{
		printk(KERN_ERR " use internal DE(), 720P\n");

		hdmi_dummy_addr=0x35;
		hdmi_dummy_data=0x40;
		ret = i2c_hdmi_write(hdmi_driver_data_p->i2c_hdmi_client, hdmi_dummy_addr, &hdmi_dummy_data, 1);
		if (ret != 0)
		{
			printk(KERN_ERR "[HDMI] use_internal_DE()::i2c_hdmi_write fail!!-ret:%d\n", ret);
		}

		hdmi_dummy_addr=0x36;
		hdmi_dummy_data=0xD9;
		ret = i2c_hdmi_write(hdmi_driver_data_p->i2c_hdmi_client, hdmi_dummy_addr, &hdmi_dummy_data, 1);
		if (ret != 0)
		{
			printk(KERN_ERR "[HDMI] use_internal_DE()::i2c_hdmi_write fail!!-ret:%d\n", ret);
		}

		hdmi_dummy_addr=0x37;
		hdmi_dummy_data=0x0A;
		ret = i2c_hdmi_write(hdmi_driver_data_p->i2c_hdmi_client, hdmi_dummy_addr, &hdmi_dummy_data, 1);
		if (ret != 0)
		{
			printk(KERN_ERR "[HDMI] use_internal_DE()::i2c_hdmi_write fail!!-ret:%d\n", ret);
		}

		hdmi_dummy_addr=0x38;
		hdmi_dummy_data=0x0;
		ret = i2c_hdmi_write(hdmi_driver_data_p->i2c_hdmi_client, hdmi_dummy_addr, &hdmi_dummy_data, 1);
		if (ret != 0)
		{
			printk(KERN_ERR "[HDMI] use_internal_DE()::i2c_hdmi_write fail!!-ret:%d\n", ret);
		}

		hdmi_dummy_addr=0x39;
		hdmi_dummy_data=0x2D;
		ret = i2c_hdmi_write(hdmi_driver_data_p->i2c_hdmi_client, hdmi_dummy_addr, &hdmi_dummy_data, 1);
		if (ret != 0)
		{
			printk(KERN_ERR "[HDMI] use_internal_DE()::i2c_hdmi_write fail!!-ret:%d\n", ret);
		}

		hdmi_dummy_addr=0x3A;
		hdmi_dummy_data=0x0;
		ret = i2c_hdmi_write(hdmi_driver_data_p->i2c_hdmi_client, hdmi_dummy_addr, &hdmi_dummy_data, 1);
		if (ret != 0)
		{
			printk(KERN_ERR "[HDMI] use_internal_DE()::i2c_hdmi_write fail!!-ret:%d\n", ret);
		}

		
		adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0x17, 0x01, 0, 1 );
	}
	else if(current_hdmi_timing == HTX_720_480p)
	{
		printk(KERN_ERR " use internal DE(), 480P\n");

		hdmi_dummy_addr=0x35;
		hdmi_dummy_data=0x1E;
		ret = i2c_hdmi_write(hdmi_driver_data_p->i2c_hdmi_client, hdmi_dummy_addr, &hdmi_dummy_data, 1);
		if (ret != 0)
		{
			printk(KERN_ERR "[HDMI] use_internal_DE()::i2c_hdmi_write fail!!-ret:%d\n", ret);
		}

		hdmi_dummy_addr=0x36;
		hdmi_dummy_data=0x63;
		ret = i2c_hdmi_write(hdmi_driver_data_p->i2c_hdmi_client, hdmi_dummy_addr, &hdmi_dummy_data, 1);
		if (ret != 0)
		{
			printk(KERN_ERR "[HDMI] use_internal_DE()::i2c_hdmi_write fail!!-ret:%d\n", ret);
		}

		hdmi_dummy_addr=0x37;
		hdmi_dummy_data=0x05;
		ret = i2c_hdmi_write(hdmi_driver_data_p->i2c_hdmi_client, hdmi_dummy_addr, &hdmi_dummy_data, 1);
		if (ret != 0)
		{
			printk(KERN_ERR "[HDMI] use_internal_DE()::i2c_hdmi_write fail!!-ret:%d\n", ret);
		}

		hdmi_dummy_addr=0x38;
		hdmi_dummy_data=0xA0;
		ret = i2c_hdmi_write(hdmi_driver_data_p->i2c_hdmi_client, hdmi_dummy_addr, &hdmi_dummy_data, 1);
		if (ret != 0)
		{
			printk(KERN_ERR "[HDMI] use_internal_DE()::i2c_hdmi_write fail!!-ret:%d\n", ret);
		}

		hdmi_dummy_addr=0x39;
		hdmi_dummy_data=0x1E;
		ret = i2c_hdmi_write(hdmi_driver_data_p->i2c_hdmi_client, hdmi_dummy_addr, &hdmi_dummy_data, 1);
		if (ret != 0)
		{
			printk(KERN_ERR "[HDMI] use_internal_DE()::i2c_hdmi_write fail!!-ret:%d\n", ret);
		}

		hdmi_dummy_addr=0x3A;
		hdmi_dummy_data=0x0;
		ret = i2c_hdmi_write(hdmi_driver_data_p->i2c_hdmi_client, hdmi_dummy_addr, &hdmi_dummy_data, 1);
		if (ret != 0)
		{
			printk(KERN_ERR "[HDMI] use_internal_DE()::i2c_hdmi_write fail!!-ret:%d\n", ret);
		}

		
		adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0x17, 0x01, 0, 1 );
	}
	else
	{
		
		adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0x17, 0x01, 0, 0 );
	}
}



void setup_audio_video( void )
{
    HTX_BYTE supported_format;
    HTX_BYTE spd_if[31];
    HTX_BYTE spd;
    int vid_16x9[] = {3,4,5,7,9,11,13,15,16,18,20,22,24,25,28,30,31,32,33,34};
    int i = 0;
    int aspect_ratio;
    int vid;

    printk(KERN_ERR "\n Cable Connected\n");

    if( !edid_reading_complete() )
        return;

    supported_format = dvi_supported_video_format();
    printk(KERN_ERR "\nDVI Supported Video Format = 0x%x \n", supported_format);

    
    current_hdmi_timing=HTX_640_480p;

    if( supported_format & ( 1<< HTX_480i ) ) {
        printk(KERN_ERR "Supported Video Format: 480i\n" );
    }
    if( supported_format & ( 1 << HTX_640_480p ) ) {
        printk(KERN_ERR "Supported Video Format: 640x480p\n" );
	 current_hdmi_timing=HTX_640_480p;
    }
    if( supported_format & ( 1 << HTX_720_480p ) ) {
        printk(KERN_ERR "Supported Video Format: 720x480p\n" );
	if(!dynamic_480p_disable)
	 current_hdmi_timing=HTX_720_480p;
    }
    if( supported_format & ( 1 << HTX_720p_60 ) ) {
        printk(KERN_ERR "Supported Video Format: 720p\n" );

	if(!dynamic_720p_disable)
	 current_hdmi_timing=HTX_720p_60;
    }
    if( supported_format & ( 1 << HTX_1080i_30 ) ) {
        printk(KERN_ERR "Supported Video Format: 1080i\n" );
    }
    
    use_internal_DE();

    for( spd = 0; spd < 31; spd++ ) {
        spd_if[spd] = 0x5A;
    }
    spd_if[2] = 0x19;
    spd_if[0] = 0x83;
    
    set_input_format(0,0,0,0);

	
	if(current_hdmi_timing != last_hdmi_timing)
	{
		
		lcdc_set_hdmi_timing(current_hdmi_timing);
	}


	if(current_hdmi_timing == HTX_720p_60)
		vid = 4; 
	else if(current_hdmi_timing == HTX_720_480p)
	{
		vid = 3; 

		
		if(dynamic_480p_aspect_4x3)
			vid = 2; 
	}
	else if(current_hdmi_timing == HTX_640_480p)
		vid = 1; 
	else
		vid = 3; 

	aspect_ratio = _4x3;
	for(i=0; i < 20;i++)
	{
	    if(vid == vid_16x9[i])
	        aspect_ratio = _16x9;
	}

	
	set_video_mode(vid,RGB,HTX_0_255,aspect_ratio);
	printk(KERN_ERR "set_video_mode(), vid=%d, aspect_ratio=%d.\n", vid, aspect_ratio);

	printk(KERN_ERR "setup_audio()\n");
	setup_audio();
}

HTX_BOOL handle_edid_abort( void )
{
    printk(KERN_ERR  "EDID Read Aborted\n" );

    if( edid ) {
        clear_edid( edid );
        return edid_retry();
    }

    return ( HTX_FALSE );
}

#ifndef HDMI_TESTMODE
static void qhdmi_cablein_WorkHandler(struct work_struct *work)
{
	int cablein_status;

	cablein_status = gpio_get_value(HDMI_CABLEIN_GPIO);
	

	if(cablein_status == 0) 
	{
		printk(KERN_ERR "[HDMI] HDMI cable insert !! \n");

		gpio_tlmm_config(GPIO_CFG(35, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);

		

		if(system_rev <= EVT1_4G_4G) 
		{
			gpio_tlmm_config(GPIO_CFG(85, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
			gpio_set_value(85,1);
		}
		else 
		{
			gpio_tlmm_config(GPIO_CFG(48, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
			gpio_set_value(48,1);
		}

		gpio_tlmm_config(GPIO_CFG(44, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
		gpio_set_value(44,0);

		
		hdmi_chip_powerup();

#ifndef HDMI_5V_ALWAYS_ON

		gpio_tlmm_config(GPIO_CFG(161, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
		gpio_set_value(161,1);
#endif

		hdmi_init();
	}
}
#endif


#ifdef PATCH_FOR_HDMI_PRETEST


void setup_adv7520_reg_patch( void )
{
	struct hdmi_driver_data_t *hdmi_driver_data_p = &hdmimain_driver_data;
	uint8_t hdmi_dummy_addr,hdmi_dummy_data;
	int ret;

	printk(KERN_ERR "[HDMI] setup_adv7520_reg_patch()+\n");

	
	
	
	hdmi_dummy_addr=0x02;
	hdmi_dummy_data=0x18;
	ret = i2c_hdmi_write(hdmi_driver_data_p->i2c_hdmi_client, hdmi_dummy_addr, &hdmi_dummy_data, 1);
	if (ret != 0)
	{
		printk(KERN_ERR "[HDMI] setup_adv7520_reg_patch()::i2c_hdmi_write fail!!-ret:%d\n", ret);
	}
	hdmi_dummy_addr=0x03;
	hdmi_dummy_data=0x80;
	ret = i2c_hdmi_write(hdmi_driver_data_p->i2c_hdmi_client, hdmi_dummy_addr, &hdmi_dummy_data, 1);
	if (ret != 0)
	{
		printk(KERN_ERR "[HDMI] setup_adv7520_reg_patch()::i2c_hdmi_write fail!!-ret:%d\n", ret);
	}

	
	hdmi_dummy_addr=0x0A;
	hdmi_dummy_data=0x41;
	ret = i2c_hdmi_write(hdmi_driver_data_p->i2c_hdmi_client, hdmi_dummy_addr, &hdmi_dummy_data, 1);
	if (ret != 0)
	{
		printk(KERN_ERR "[HDMI] setup_adv7520_reg_patch()::i2c_hdmi_write fail!!-ret:%d\n", ret);
	}

	
	hdmi_dummy_addr=0x0C;
	hdmi_dummy_data=0x84;
	ret = i2c_hdmi_write(hdmi_driver_data_p->i2c_hdmi_client, hdmi_dummy_addr, &hdmi_dummy_data, 1);
	if (ret != 0)
	{
		printk(KERN_ERR "[HDMI] setup_adv7520_reg_patch()::i2c_hdmi_write fail!!-ret:%d\n", ret);
	}

	hdmi_dummy_addr=0x41;
	hdmi_dummy_data=0x0;
	ret = i2c_hdmi_write(hdmi_driver_data_p->i2c_hdmi_client, hdmi_dummy_addr, &hdmi_dummy_data, 1);
	if (ret != 0)
	{
		printk(KERN_ERR "[HDMI] setup_adv7520_reg_patch()::i2c_hdmi_write fail!!-ret:%d\n", ret);
	}

	printk(KERN_ERR "[HDMI] setup_adv7520_reg_patch()-\n");
}
#endif


#if defined( HDCP_OPTION )
void handle_bksv_ready_interrupt( void )
{
	struct hdmi_driver_data_t *hdmi_driver_data_p = &hdmimain_driver_data;
	struct hdmi_driver_data_t *hdmiedid = &hdmiedid_driver_data;

    if( hdcp_requested == 1 ) {
        
        bksv_count = adi_get_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0xC7, 0x7f, 0 );
        hdcp_enabled = 1;

        if( bksv_count == 0 ) {
	    
            total_bksvs = 1;
            bksv_flag_counter = 0;
		bksv[4] = adi_get_i2c_reg(hdmi_driver_data_p->i2c_hdmi_client,0xBF, 0xff, 0);
		bksv[3] = adi_get_i2c_reg(hdmi_driver_data_p->i2c_hdmi_client,0xC0, 0xff, 0);
		bksv[2] = adi_get_i2c_reg(hdmi_driver_data_p->i2c_hdmi_client,0xC1, 0xff, 0);
		bksv[1] = adi_get_i2c_reg(hdmi_driver_data_p->i2c_hdmi_client,0xC2, 0xff, 0);
		bksv[0] = adi_get_i2c_reg(hdmi_driver_data_p->i2c_hdmi_client,0xC3, 0xff, 0);

            hdcp_bstatus[1] = 0;
            hdcp_bstatus[0] = 0;
            if( adi_get_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0xAF, 0x2, 1 ) )
                hdcp_bstatus[1] = 0x10;
        }

	
        if( bksv_count > 0 ) {
	    if( bksv_flag_counter == 0 ) {
		 
	        i2c_hdmi_read( hdmiedid->i2c_hdmi_client, 0xF9, hdcp_bstatus, 2 );
            }

            i2c_hdmi_read( hdmiedid->i2c_hdmi_client, 0x00, bksv + 5 + ( bksv_flag_counter * 65 ), ( bksv_count * 5 ) );
            total_bksvs += bksv_count;
            bksv_flag_counter++;
        }
    }
    else {
        printk(KERN_ERR "Error: BKSV Interrupt with no Request\n");
    }
}
#endif

static void qhdmi_irqWorkHandler(struct work_struct *work)
{
	uint8_t interrupt_registers[2] = { 0x00, 0x00 };
	int interrupt_code = 0;
	uint8_t reg_add;
	int ret = 0;
	struct hdmi_driver_data_t *hdmi_driver_data_p = &hdmimain_driver_data;
	HTX_INT error;
	char *event_string_0[] = { "status=0", NULL };
	char *event_string_1[] = { "status=1", NULL };

	printk(KERN_ERR "hdmi interrupt occurred !!\n");




	
	printk(KERN_ERR "### action 0 ###\n");

	
    	reg_add = 0x96;
	ret = i2c_hdmi_read(hdmi_driver_data_p->i2c_hdmi_client, reg_add, interrupt_registers, 2);
	if (ret != 0)
	{
		printk(KERN_ERR "[HDMI] qhdmi_irqWorkHandler()::i2c_hdmi_read  fail!!-ret:%d\n", ret);
		
	}
	printk(KERN_ERR "[HDMI INT] interrupt_registers[0]=0x%x, interrupt_registers[1]=0x%x\n", interrupt_registers[0], interrupt_registers[1]);

        interrupt_registers[0] &= 0xE6;
        interrupt_registers[1] &= 0xC0;

        reg_add = 0x96;
	ret = i2c_hdmi_write(hdmi_driver_data_p->i2c_hdmi_client, reg_add, interrupt_registers, 2);
	if (ret != 0)
	{
		printk(KERN_ERR "[HDMI] qhdmi_irqWorkHandler()::i2c_hdmi_write fail!!-ret:%d\n", ret);
		
	}

	printk(KERN_ERR "[HDMI INT] interrupt_registers[0]=0x%x, interrupt_registers[1]=0x%x\n", interrupt_registers[0], interrupt_registers[1]);

	
	
	if(hdmi_block_power_status==1)
		
		while(atomic_read(&hdmi_initialed)==0)
			msleep(10);




	

	if( interrupt_registers[0] & VSYNC_INT ) {
		printk(KERN_ERR " ### action 1 ###\n [VSYNC_INT] clear it and do nothing currently.\n");


	}




	
	if( interrupt_registers[1] & ( HTX_BYTE )( INT_ERROR >> 8 ) )
	{
		printk(KERN_ERR "### action 2 ###\n");
		
	    error = adi_get_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0xC8, 0xf0, 4 );
#if defined( HDCP_OPTION )
	    if( error == RI_MISMATCH || error == PJ_MISMATCH || error == RPTR_TIMEOUT
		|| error == MAX_CASCADE || error == SHA1_FAIL || error == MAX_DEV ) {
	        printk(KERN_ERR "HDCP Failure, error=0x%x\n", error );
		if( hdcp_requested ) {
		    av_inhibit( HTX_TRUE, RGB, RGB, HTX_640_480p );
			printk(KERN_ERR "[hdcp_requested] Black Image\n" );
		}
	    }
#endif
            
	    if( error == I2C_ERROR ) {
		
#if defined( HDCP_OPTION )
	        if( !hdcp_requested ) {
#endif

#if defined( NO_REPEATED_REREAD )
		    ;
#else
			adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0xC9, 0xf, 0, 15 );
#endif
		    printk(KERN_ERR "EDID Controller Error -01: %x\n", error );

		    if( default_edid ) {
		        handle_edid_interrupt();
			interrupt_code |= EDID_INT;
		    }

#if defined( HDCP_OPTION )
		}
		else {
		    printk(KERN_ERR "HDCP Controller Error: %x\n", I2C_ERROR );
		}

#endif
	    }
	    else if( error == 2 ) {
	        if( adi_get_i2c_reg(  hdmi_driver_data_p->i2c_hdmi_client, 0x42, 0x20, 5 ) ) {
		    printk(KERN_ERR "EDID Controller Error -02: %x\n", error );
		}
	    }
	    else {
	        printk(KERN_ERR "EDID Controller Error -03: %x\n", error );
	    }
	    interrupt_code |= INT_ERROR;
	}




	
	

	if( ( interrupt_registers[0] & RX_SENSE ) )
	{
	    printk(KERN_ERR "### action 3 ###\n");
	    handle_rx_sense_interrupt();
            
	    
	    
#if defined( HDCP_OPTION )
	    hdcp_enabled   = HTX_FALSE;
	    hdcp_requested = HTX_FALSE;
	    hdcp_status    = HTX_FALSE;
#endif
	    edid_block_map_offset = 0;
	    interrupt_code |= RX_SENSE;
	}




	

	if( ( interrupt_registers[0] & EDID_INT ) ) {
	    printk(KERN_ERR "### action 4 ###\n Checking if EDID already exists\n" );
	    if( !edid_reading_complete() ) {
	        printk(KERN_ERR "processing EDID...\n" );
		next_segment = handle_edid_interrupt();
	    	printk(KERN_ERR "next_segment=0x%x\n", next_segment );
		interrupt_code |= EDID_INT;
	    }
	}




	
#if defined( HDCP_OPTION )
	if( interrupt_registers[1] & ( HTX_BYTE )( HDCP_INT >> 8 ) ) {




printk(KERN_ERR "\n[HDMI] HDCP_INT\n" );

            handle_bksv_ready_interrupt();
	    interrupt_code |= HDCP_INT;
	}

	
	if( interrupt_registers[0] & HDCP_AUTH ) {
	    printk(KERN_ERR "\nReady For BKSV Comparison\n" );
	    hdcp_status = HTX_TRUE;
	    if( bksv_callback_function( bksv, total_bksvs, hdcp_bstatus ) ) {
	        printk(KERN_ERR "Restoring Video\n" );
		av_inhibit( HTX_FALSE, av_inh_colorspace_in, av_inh_colorspace_range, av_inh_video_timing );
	    }
	    interrupt_code |= HDCP_AUTH;
	}

#endif




	
	if( interrupt_registers[0] & HPD_INT )
	{
	    printk(KERN_ERR "### action 6 ###\n[HPD_INT] interrupt_registers[0]=0x%x, interrupt_registers[1]=0x%x\r\n", interrupt_registers[0], interrupt_registers[1]);

	    if(handle_hpd_interrupt()==0)
	    {
		if(likely(atomic_read(&cable_in)))
		{
			
			kobject_uevent_env(&pdev_hdmi->dev.kobj, KOBJ_CHANGE, event_string_0);
	
			cable_out = HTX_TRUE;
			atomic_set(&cable_in,HTX_FALSE);

			printk(KERN_ERR "[HDMI] HPD cable_out\n");

		    	#ifdef HDCP_OPTION
			    cancel_delayed_work( &g_hdcpWork);
			    printk(KERN_ERR "cable_out::del_timer\n");
			#endif
			
			printk(KERN_ERR "######cable_out########### set PowerDown pin as 1.\n");
			
			adi_set_i2c_reg(  hdmi_driver_data_p->i2c_hdmi_client, 0x41, 0x40, 6, 1 );
		}
		else
		{
			printk(KERN_ERR "=====> [cable_in =0] cable is already out @@@@@\n");
		}
	    }
	    else
	    {
		if(unlikely(atomic_read(&cable_in)))
		{
			printk(KERN_ERR "@@@@@ [cable_in =1] is already in <=====\n");
		}
		else
		{
			
			modify_pkg_slave_addr();
	
			
			kobject_uevent_env(&pdev_hdmi->dev.kobj, KOBJ_CHANGE, event_string_1);

		    	atomic_set(&cable_in,HTX_TRUE);
		    	cable_out = HTX_FALSE;
	
			printk(KERN_ERR "[HDMI] HPD cable_in\n");


			printk(KERN_ERR "enable LCDC!!\n");
			mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);
			outpdw(MDP_BASE + 0xE0000, 1); 
			mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);

	
		    	#ifdef HDCP_OPTION
	    			schedule_delayed_work( &g_hdcpWork, (CHECK_PERIOD_MS * HZ / 1000) );
	    			printk(KERN_ERR "cable_in::add_timer\n");
			#endif
	
			printk(KERN_ERR "######cable_in########### still set PowerDown pin as 1.\n");
			
			adi_set_i2c_reg(  hdmi_driver_data_p->i2c_hdmi_client, 0x41, 0x40, 6, 1 );
		}
	    }
	    interrupt_code |= HPD_INT;
	}




	
	

	if( ( interrupt_registers[0] & EDID_INT ) )
	{

	    printk(KERN_ERR "### action 7 ###\n next segment is %d\n", edid_block_map_offset + next_segment );

	    if( next_segment == -1 ) {
	        if( handle_edid_abort() ) {
		    next_segment = 0;
		}
	    }

	    if( next_segment == 0 ) {
#if defined( CEC_EDID )





	        if( !adi_get_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0x42, 0x20, 5 ) ) {
		    adi_set_i2c_reg(  hdmi_driver_data_p->i2c_hdmi_client, 0x41, 0x40, 6, 1 );
		}
		else {
#endif

#if defined( AUTO_MUTE )
		  av_mute_on();
#endif
		  initialize_HDMI_Tx();
		  set_EDID_ready( HTX_TRUE );
		  setup_audio_video();

#if defined( AUTO_MUTE )
		  tmds_powerdown( 0 );
		  av_mute_off();
#endif

#if defined( CEC_EDID )
		}
#endif
	    }
	    else if( next_segment == -2 ) {
	        
	        printk(KERN_ERR "ignore !! next_segment == -2\n");
	    }
	    else {
		adi_set_i2c_reg(hdmi_driver_data_p->i2c_hdmi_client,0xC4, 0xff, 0, edid_block_map_offset + next_segment);
	    }

	    if( next_segment == 0x40 ) {
	        edid_block_map_offset = 0x40;
	    }


#ifdef PATCH_FOR_HDMI_PRETEST

#endif



	}











#ifndef HDMI_TESTMODE
	
	if(cable_out)
	{
#ifndef HDMI_5V_ALWAYS_ON

		gpio_tlmm_config(GPIO_CFG(161, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
		gpio_set_value(161,0);
#endif

		
		hdmi_chip_powerdown();

		



		
		if(system_rev <= EVT1_4G_4G) 
		{
			gpio_tlmm_config(GPIO_CFG(85, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
			gpio_set_value(85,0);
		}
		else 
		{
			gpio_tlmm_config(GPIO_CFG(48, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
			gpio_set_value(48,0);
		}

		gpio_tlmm_config(GPIO_CFG(35, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);

		printk(KERN_ERR "[HDMI] HDMI cable removed !! turn off all HDMI related power.\n");

		cable_out = HTX_FALSE;
		atomic_set(&cable_in,HTX_FALSE);
	}
#endif




printk(KERN_ERR "### action 10 ###\n");
}

static irqreturn_t qhdmi_irqhandler(int irq, void *dev_id)
{
	schedule_work(&qhdmi_irqwork);
	return IRQ_HANDLED;
}

static int qhdmi_irqsetup(int hdmi_irqpin)
{

	int ret = request_irq(MSM_GPIO_TO_INT(hdmi_irqpin), &qhdmi_irqhandler,
			     IRQF_TRIGGER_FALLING, HDMI_NAME, NULL);

	if (ret < 0) {
		printk(KERN_ERR
		       "Could not register for  %s interrupt "
		       "(ret = %d)\n", HDMI_NAME, ret);
		ret = -EIO;
	}
	return ret;
}

#ifndef HDMI_TESTMODE
static irqreturn_t qhdmi_cablein_irqhandler(int irq, void *dev_id)
{
	schedule_work(&qhdmi_cablein_irqwork);
	return IRQ_HANDLED;
}


static int qhdmi_cablein_irqsetup(int hdmi_irqpin)
{
	int ret = request_irq(MSM_GPIO_TO_INT(hdmi_irqpin), &qhdmi_cablein_irqhandler,
			     IRQF_TRIGGER_FALLING, HDMI_NAME, NULL);
	if (ret < 0) {
		printk(KERN_ERR
		       "Could not register for  %s interrupt "
		       "(ret = %d)\n", HDMI_NAME, ret);
		ret = -EIO;
	}
	return ret;
}
#endif



void modify_cec_slave_addr(void)
{
	int ret = 0;
	struct hdmi_driver_data_t *hdmi_driver_data_p = &hdmimain_driver_data;
	int i=0;
	uint8_t    hdmi_dummy_addr = 0;
	uint8_t    hdmi_dummy_data = 0;
	int 	retry_max=10;





	for (i = 0; i < retry_max ; i++)
	{
		hdmi_dummy_addr=0xE1;
		hdmi_dummy_data=0x54; 
		ret = i2c_hdmi_write(hdmi_driver_data_p->i2c_hdmi_client, hdmi_dummy_addr, &hdmi_dummy_data, 1);
		if (ret != 0)
		{
			printk(KERN_ERR "[HDMI] initialize_power_state()::i2c_hdmi_write fail!!-ret:%d\n", ret);
			
		}
		printk(KERN_ERR "[hdmi_init] write addr[0x%x]= 0x%x\n",hdmi_dummy_addr, hdmi_dummy_data);


		hdmi_dummy_addr=0xE1;
		hdmi_dummy_data=0x0;
		ret = i2c_hdmi_read(hdmi_driver_data_p->i2c_hdmi_client, hdmi_dummy_addr, &hdmi_dummy_data, 1);
		if (ret != 0)
		{
			printk(KERN_ERR "[HDMI] :i2c_hdmi_read  fail!!-ret:%d\n", ret);
			
		}
		printk(KERN_ERR "[hdmi_init] read back addr[0x%x]= 0x%x\n",hdmi_dummy_addr, hdmi_dummy_data);

		if(hdmi_dummy_data == 0x54)
		{
			i = retry_max;
			printk(KERN_ERR "[hdmi_init] write REG[0xE1]=0x54 ### success ###\n");
		}
	}
}

void modify_pkg_slave_addr(void)
{
	int ret = 0;
	struct hdmi_driver_data_t *hdmi_driver_data_p = &hdmimain_driver_data;
	int i=0;
	uint8_t    hdmi_dummy_addr = 0;
	uint8_t    hdmi_dummy_data = 0;
	int 	retry_max=10;





        for (i = 0; i < retry_max ; i++)
	{
		hdmi_dummy_addr=0x45;
		hdmi_dummy_data=0x50; 
		ret = i2c_hdmi_write(hdmi_driver_data_p->i2c_hdmi_client, hdmi_dummy_addr, &hdmi_dummy_data, 1);
		if (ret != 0)
		{
			printk(KERN_ERR "[HDMI] initialize_power_state()::i2c_hdmi_write fail!!-ret:%d\n", ret);
			
		}
		printk(KERN_ERR "[hdmi_init] write addr[0x%x]= 0x%x\n",hdmi_dummy_addr, hdmi_dummy_data);

		hdmi_dummy_addr=0x45;
		hdmi_dummy_data=0x0;
		ret = i2c_hdmi_read(hdmi_driver_data_p->i2c_hdmi_client, hdmi_dummy_addr, &hdmi_dummy_data, 1);
		if (ret != 0)
		{
			printk(KERN_ERR "[HDMI] :i2c_hdmi_read  fail!!-ret:%d\n", ret);
			
		}
		printk(KERN_ERR "[hdmi_init] read back addr[0x%x]= 0x%x\n",hdmi_dummy_addr, hdmi_dummy_data);

		if(hdmi_dummy_data == 0x50)
		{
			i = retry_max;
			printk(KERN_ERR "[hdmi_init] write REG[0x45]=0x50 ### success ###\n");
		}
	}
}

void hdmi_init(void)
{
	int ret = 0;
	struct hdmi_driver_data_t *hdmi_driver_data_p = &hdmimain_driver_data;
        uint8_t    hdmi_i2c_init_addr_array[]  = {0x98, 0x9C, 0x9D, 0xA2, 0xA3, 0xDE, 0x0A, 0xBA, 0xAF};
        uint8_t    hdmi_i2c_init_value_array[] = {0x03, 0x38, 0x61, 0x94, 0x94, 0x88, 0x09, 0x60, 0x16};
        
        
	int i=0;
	uint8_t    hdmi_dummy_addr = 0;
	uint8_t    hdmi_dummy_data = 0;


	
	

	
	initialize_HDMI_Tx();


        for (i = 0; i < sizeof (hdmi_i2c_init_value_array) ; i++)
        {
        	ret = i2c_hdmi_write(hdmi_driver_data_p->i2c_hdmi_client, hdmi_i2c_init_addr_array[i], &(hdmi_i2c_init_value_array[i]), 1);
		if (ret != 0)
		{
			printk(KERN_ERR "[HDMI] initialize_adv7520()::i2c_hdmi_write [i]=%d fail!!-ret:%d\n", i, ret);
		
		}
	}


	hdmi_dummy_addr=0xA1;
	hdmi_dummy_data=0;
	ret = i2c_hdmi_write(hdmi_driver_data_p->i2c_hdmi_client, hdmi_dummy_addr, &hdmi_dummy_data, 1);
	if (ret != 0)
	{
		printk(KERN_ERR "[HDMI] initialize_power_state()::i2c_hdmi_write fail!!-ret:%d\n", ret);
		
	}

	
	setup_audio();

	printk(KERN_ERR "[hdmi_init] done!!\n");
}

EXPORT_SYMBOL(hdmi_init);





int hdmi_block_poweron(void)
{
	struct hdmi_driver_data_t *hdmi_driver_data_p = &hdmimain_driver_data;

	printk(KERN_ERR "[%s], system_rev=%d\n", __func__, system_rev);
	atomic_set(&cable_in,HTX_FALSE);
	
	atomic_set(&hdmi_audio_only_flag, HTX_FALSE);


	
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);
	
	if(current_hdmi_timing == HTX_720p_60)
		outpdw(MDP_BASE + 0xE0038, 0);
	else
		outpdw(MDP_BASE + 0xE0038, 3); 
	
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);

	if(hdmi_block_power_status==0)
	{
		printk(KERN_ERR "[HDMI]case HDMI_ON ... ver 10 \n");
		hdmi_all_powerup();

		
		hdmi_block_power_status=1;
	}
	else 
	{
		
		
		hdmi_lcdc_all_off();
		
		
		printk(KERN_ERR "[HDMI]case HDMI_ON ... ver 11 \n");
		hdmi_all_powerup();
		
		hdmi_block_power_status=1;
		printk(KERN_ERR "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@ \n");
	}

	printk(KERN_ERR "####[hdmi_block_poweron]######### set PowerDown pin as 1.\n");
	
	adi_set_i2c_reg(  hdmi_driver_data_p->i2c_hdmi_client, 0x41, 0x40, 6, 1 );

	return 0;
}



void hdmi_lcdc_all_off(void)
{
	
	
	
	printk(KERN_ERR "[HDMI]case HDMI_OFF ... ver 20 \n");
	hdmi_all_powerdown();

	
	hdmi_block_power_status=0;

	
	
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);
	outpdw(MDP_BASE + 0xE0000, 0); 
	
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);
	
	
	
	atomic_set(&video_out_flag, HTX_FALSE);

}
EXPORT_SYMBOL(hdmi_lcdc_all_off);

int hdmi_block_poweroff(void)
{
	
	atomic_set(&hdmi_audio_only_flag, HTX_FALSE);

	if(hdmi_block_power_status==1)
	{
		
		
		
		hdmi_lcdc_all_off();
	}
	else
	{
		
		
		mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);
		outpdw(MDP_BASE + 0xE0000, 0); 
		
		mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);
	}

	return 0;
}




int hdmi_start_video_out(void)
{
	struct hdmi_driver_data_t *hdmi_driver_data_p = &hdmimain_driver_data;

	printk(KERN_ERR "##### hdmi_start_video_out ####### set PowerDown as 0.\n");
	
	adi_set_i2c_reg(  hdmi_driver_data_p->i2c_hdmi_client, 0x41, 0x40, 6, 0 );

	
	
	
	hdmi_init();

	
	atomic_set(&video_out_flag, HTX_TRUE);

	return 0;
}




int hdmi_stop_video_out(void)
{
	struct hdmi_driver_data_t *hdmi_driver_data_p = &hdmimain_driver_data;
	
	printk(KERN_ERR "##### hdmi_stop_video_out ####### set PowerDown as 1.\n");
	
	adi_set_i2c_reg(  hdmi_driver_data_p->i2c_hdmi_client, 0x41, 0x40, 6, 1 );

	
	atomic_set(&video_out_flag, HTX_FALSE);

	return 0;
}







int hdmi_audio_only(void)
{
	
	atomic_set(&hdmi_audio_only_flag, HTX_TRUE);

	return 0;
}







void hdmi_output_blank(void)
{
	struct hdmi_driver_data_t *hdmi_driver_data_p = &hdmimain_driver_data;

	if(hdmi_block_power_status==1)
	{
		
		adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0xD5, 0x1, 0, 1 );
	}
}
EXPORT_SYMBOL(hdmi_output_blank);







void hdmi_output_retrieve(void)
{
	struct hdmi_driver_data_t *hdmi_driver_data_p = &hdmimain_driver_data;

	if(hdmi_block_power_status==1)
	{
		
		adi_set_i2c_reg( hdmi_driver_data_p->i2c_hdmi_client, 0xD5, 0x1, 0, 0 );
	}
}
EXPORT_SYMBOL(hdmi_output_retrieve);

static int hdmi_param_set(const char *val, struct kernel_param *kp)
{
	int ret;
	uint8_t my_edid_test[2] = { 0x00, 0x00 };
	struct hdmi_driver_data_t *hdmi_driver_data_p = &hdmimain_driver_data;
	struct hdmi_driver_data_t *hdmiedid = &hdmiedid_driver_data;
	uint8_t edid_dat_parm[256];
	int i;
	uint8_t reg_add, interrupt_registers;


	printk(KERN_ERR	"%s +\n", __func__);

	ret = param_set_int(val, kp);

	if(hdmi_param==0)
	{
		printk(KERN_ERR "do nothing...\n");
		lcdc_set_hdmi_timing(HTX_1080i_30); 
	}
	else  if(hdmi_param==1)
	{
		printk(KERN_ERR "set to 720P (1208x720)\n");
		lcdc_set_hdmi_timing(HTX_720p_60);
	}
	else  if(hdmi_param==2)
	{
		printk(KERN_ERR "set to 480P (720x480)\n");
		lcdc_set_hdmi_timing(HTX_720_480p);
	}
	else  if(hdmi_param==3)
	{
		printk(KERN_ERR "set to VGA (640x480)\n");
		lcdc_set_hdmi_timing(HTX_640_480p);
	}
	else  if(hdmi_param==11)
	{
		printk(KERN_ERR "set to 720P (1208x720)\n");
		
		current_hdmi_timing = HTX_720p_60;
	}
	else  if(hdmi_param==12)
	{
		printk(KERN_ERR "set to 480P (720x480)\n");
		
		current_hdmi_timing = HTX_720_480p;
	}
	else  if(hdmi_param==13)
	{
		printk(KERN_ERR "set to VGA (640x480)\n");
		
		current_hdmi_timing = HTX_640_480p;
	}
	else if(hdmi_param==90)
	{
		printk(KERN_ERR "### turn off Dynamic Debug log ###\n");
		dynamic_log_ebable = 0;
	}
	else if(hdmi_param==91)
	{
		printk(KERN_ERR "### turn on Dynamic Debug log ###\n");
		dynamic_log_ebable = 1;
	}

	else  if(hdmi_param==100)
	{
		printk(KERN_ERR "hdmi_param_set(), 100\n");
	
		
		
		
		printk(KERN_ERR "we re-read from HDMI IC, clear local buffer first\n");
		for (i=0;i<256;i++)
			edid_dat_parm[i]=0;

	    	reg_add = 0;
	    	interrupt_registers=0;
	    	ret = i2c_hdmi_read(hdmiedid->i2c_hdmi_client, reg_add, edid_dat_parm, 256);
	    	if (ret != 0)
	    	{
	    		printk(KERN_ERR "[HDMI] handle_edid_interrupt()::i2c_hdmi_read  fail!!-ret:%d\n", ret);
	    		
	    	}
		
		
		printk(KERN_ERR "\n========================\n");
		for (i=0;i<256;i++)
		{
			if((i%16)==0)
				printk(KERN_ERR "\n");
			printk(KERN_ERR "%3x ", edid_dat_parm[i]);
		}
		printk(KERN_ERR "\n========================\n");
		
	}

	else  if(hdmi_param==101)
	{
		printk(KERN_ERR "hdmi_param_set(), 101\n");
		
		


		
		
		
	    	printk(KERN_ERR "adk HDMI IC re-read from monitor\n");
	    	my_edid_test[0]=1;
	    	my_edid_test[1]=0;
	        
	        reg_add = 0xC4;
		ret = i2c_hdmi_write(hdmi_driver_data_p->i2c_hdmi_client, reg_add, my_edid_test, 1);
		if (ret != 0)
		{
			printk(KERN_ERR "[HDMI] qhdmi_irqWorkHandler()::i2c_hdmi_write fail!!-ret:%d\n", ret);
			
		}
		printk(KERN_ERR "my_edid_test[0]=0x%x, my_edid_test[1]=0x%x\n", my_edid_test[0], my_edid_test[1]);



		
		
		
		printk(KERN_ERR "we re-read from HDMI IC\n");
		for (i=0;i<256;i++)
			edid_dat_parm[i]=0;

	    	reg_add = 0;
	    	interrupt_registers=0;
	    	ret = i2c_hdmi_read(hdmiedid->i2c_hdmi_client, reg_add, edid_dat_parm, 256);
	    	if (ret != 0)
	    	{
	    		printk(KERN_ERR "[HDMI] handle_edid_interrupt()::i2c_hdmi_read  fail!!-ret:%d\n", ret);
	    		
	    	}
		
		
		printk(KERN_ERR "\n========================\n");
		for (i=0;i<256;i++)
		{
			if((i%16)==0)
				printk(KERN_ERR "\n");
			printk(KERN_ERR "%3x ", edid_dat_parm[i]);
		}
		printk(KERN_ERR "\n========================\n");
		
	}

	else  if(hdmi_param==102)
	{
		printk(KERN_ERR "hdmi_param_set(), 102\n");
		
		adi_set_i2c_reg(hdmi_driver_data_p->i2c_hdmi_client,0xC4, 0xff, 0, 1);


		
		
		
	    	printk(KERN_ERR "adk HDMI IC re-read from monitor\n");
	    	my_edid_test[0]=1;
	    	my_edid_test[1]=0;
	        reg_add = 0xC9;
		ret = i2c_hdmi_write(hdmi_driver_data_p->i2c_hdmi_client, reg_add, my_edid_test, 1);
		if (ret != 0)
		{
			printk(KERN_ERR "[HDMI] qhdmi_irqWorkHandler()::i2c_hdmi_write fail!!-ret:%d\n", ret);
			
		}
		printk(KERN_ERR "my_edid_test[0]=0x%x, my_edid_test[1]=0x%x\n", my_edid_test[0], my_edid_test[1]);



		
		
		
		printk(KERN_ERR "we re-read from HDMI IC\n");
		for (i=0;i<256;i++)
			edid_dat_parm[i]=0;

	    	reg_add = 0;
	    	interrupt_registers=0;
	    	ret = i2c_hdmi_read(hdmiedid->i2c_hdmi_client, reg_add, edid_dat_parm, 256);
	    	if (ret != 0)
	    	{
	    		printk(KERN_ERR "[HDMI] handle_edid_interrupt()::i2c_hdmi_read  fail!!-ret:%d\n", ret);
	    		
	    	}
		
		
		printk(KERN_ERR "\n========================\n");
		for (i=0;i<256;i++)
		{
			if((i%16)==0)
				printk(KERN_ERR "\n");
			printk(KERN_ERR "%3x ", edid_dat_parm[i]);
		}
		printk(KERN_ERR "\n========================\n");
		
	}
	else  if(hdmi_param==103)
	{
		printk(KERN_ERR "hdmi_param_set(), 103\n");
		adi_set_i2c_reg(hdmi_driver_data_p->i2c_hdmi_client,0xC4, 0xff, 0, 0);
	}
	else  if(hdmi_param==104)
	{
		printk(KERN_ERR "hdmi_param_set(), 104\n");
		adi_set_i2c_reg(hdmi_driver_data_p->i2c_hdmi_client,0xC4, 0xff, 0, 1);
	}
	else  if(hdmi_param==105)
	{
		printk(KERN_ERR "hdmi_param_set(), 105\n");
		adi_set_i2c_reg(hdmi_driver_data_p->i2c_hdmi_client,0xC4, 0xff, 0, 2);
	}
	else  if(hdmi_param==106)
	{
		printk(KERN_ERR "hdmi_param_set(), 106\n");
		adi_set_i2c_reg(hdmi_driver_data_p->i2c_hdmi_client,0xC4, 0xff, 0, 3);
	}
	else  if(hdmi_param==107)
	{
		printk(KERN_ERR "hdmi_param_set(), 107\n");
		adi_set_i2c_reg(hdmi_driver_data_p->i2c_hdmi_client,0xC4, 0xff, 0, 4);
	}
	else  if(hdmi_param==108)
	{
		printk(KERN_ERR "hdmi_param_set(), 108\n");
		adi_set_i2c_reg(hdmi_driver_data_p->i2c_hdmi_client,0xC4, 0xff, 0, 17); 
	}
	else  if(hdmi_param==109)
	{
		printk(KERN_ERR "hdmi_param_set(), 109\n");
		
		
		    	reg_add = 0xc4;
		    	my_edid_test[0]=0;
		    	my_edid_test[1]=0;
			ret = i2c_hdmi_read(hdmi_driver_data_p->i2c_hdmi_client, reg_add, my_edid_test, 2);
			if (ret != 0)
			{
				printk(KERN_ERR "[HDMI parm] i2c_hdmi_read  fail!!-ret:%d\n", ret);
				
			}
			printk(KERN_ERR "==== Reg0xc4=0x%x, reg0xc5=0x%x\n", my_edid_test[0], my_edid_test[1]);
		
	}
	else  if(hdmi_param==110)
	{
		printk(KERN_ERR "hdmi_param_set(), 110\n");
		adi_set_i2c_reg(hdmi_driver_data_p->i2c_hdmi_client,0xC4, 0xff, 0, 1);

		
		
		
	    	printk(KERN_ERR "adk HDMI IC re-read from monitor\n");
	    	my_edid_test[0]=1;
	    	my_edid_test[1]=0;
	        reg_add = 0xC9;
		ret = i2c_hdmi_write(hdmi_driver_data_p->i2c_hdmi_client, reg_add, my_edid_test, 1);
		if (ret != 0)
		{
			printk(KERN_ERR "[HDMI] qhdmi_irqWorkHandler()::i2c_hdmi_write fail!!-ret:%d\n", ret);
			
		}
		printk(KERN_ERR "my_edid_test[0]=0x%x, my_edid_test[1]=0x%x\n", my_edid_test[0], my_edid_test[1]);


		
		
		
		printk(KERN_ERR "we re-read from HDMI IC, clear local buffer first\n");
		for (i=0;i<256;i++)
			edid_dat_parm[i]=0;

	    	reg_add = 0;
	    	interrupt_registers=0;
	    	ret = i2c_hdmi_read(hdmiedid->i2c_hdmi_client, reg_add, edid_dat_parm, 256);
	    	if (ret != 0)
	    	{
	    		printk(KERN_ERR "[HDMI] handle_edid_interrupt()::i2c_hdmi_read  fail!!-ret:%d\n", ret);
	    		
	    	}
		
		
		printk(KERN_ERR "\n========================\n");
		for (i=0;i<256;i++)
		{
			if((i%16)==0)
				printk(KERN_ERR "\n");
			printk(KERN_ERR "%3x ", edid_dat_parm[i]);
		}
		printk(KERN_ERR "\n========================\n");
		
	}
	else  if(hdmi_param==480)
	{
		dynamic_480p_disable = 0; 
		printk(KERN_ERR "dynamic set 480P (EDID_480P) enable\n");
	}
	else  if(hdmi_param==481)
	{
		dynamic_480p_disable = 1;  
		printk(KERN_ERR "dynamic set 480P (EDID_480P) disable\n");
	}
	else  if(hdmi_param==485)
	{
		dynamic_480p_aspect_4x3 = 1;  
		printk(KERN_ERR "dynamic set 480P aspect ratio as 4:3\n");
	}
	else  if(hdmi_param==486)
	{
		dynamic_480p_aspect_4x3 = 0;  
		printk(KERN_ERR "dynamic set 480P aspect ratio as 16:9\n");
	}
	else  if(hdmi_param==720)
	{
		dynamic_720p_disable = 0; 
		printk(KERN_ERR "dynamic set 720P (EDID_720P) enable\n");
	}
	else  if(hdmi_param==721)
	{
		dynamic_720p_disable = 1;  
		printk(KERN_ERR "dynamic set 720P (EDID_720P) disable\n");
	}
	else  if(hdmi_param==900)
	{
		dynamic_force_edid = 0;
		printk(KERN_ERR "nornal EDID, read from monitor\n");
	}
	else  if(hdmi_param==901)
	{
		dynamic_force_edid = 1;  
		printk(KERN_ERR "force replace EDID, using ADI's test pattern\n");
	}
	else  if(hdmi_param==2000)
	{
		atomic_set(&video_out_flag, 0);
		printk(KERN_ERR "case 2000, current video_out_flag is %d\n", atomic_read(&video_out_flag));
	}
	else  if(hdmi_param==2001)
	{
		atomic_set(&video_out_flag, 1);
		printk(KERN_ERR "case 2001, current video_out_flag is %d\n", atomic_read(&video_out_flag));
	}
	else  if(hdmi_param==2002)
	{
		printk(KERN_ERR "case 2002, current video_out_flag is %d\n", atomic_read(&video_out_flag));
	}
	else  if(hdmi_param==2010)
	{
		printk(KERN_ERR "case 2010, current video_out_flag is %d\n", atomic_read(&video_out_flag));
		
		adi_set_i2c_reg(  hdmi_driver_data_p->i2c_hdmi_client, 0x41, 0x40, 6, 0 );
	}
	else  if(hdmi_param==2011)
	{
		printk(KERN_ERR "case 2011, current video_out_flag is %d\n", atomic_read(&video_out_flag));
		
		adi_set_i2c_reg(  hdmi_driver_data_p->i2c_hdmi_client, 0x41, 0x40, 6, 1 );
	}
	else  if(hdmi_param==3000)
	{
		printk(KERN_ERR "case 3000, current cable_in is %d\n", atomic_read(&cable_in));
	}
	else  if(hdmi_param==4000)
	{
		printk(KERN_ERR "case 4000\n");
		hdmi_all_powerdown();
	}
	else  if(hdmi_param==4001)
	{
		printk(KERN_ERR "case 4001\n");
		hdmi_all_powerup();
	}
	else  if(hdmi_param==4010)
	{
		printk(KERN_ERR "case 4010\n");
		hdmi_stop_video_out();
	}
	else  if(hdmi_param==4011)
	{
		printk(KERN_ERR "case 4011\n");
		hdmi_start_video_out();
	}	
	else  if(hdmi_param==5000)
	{
		printk(KERN_ERR "case 5000\n");
		atomic_set(&hdmi_audio_only_flag, HTX_FALSE);
		printk(KERN_ERR "##### case 5000 ####### set hdmi_audio_only_flag as 0.\n");
	}
	else  if(hdmi_param==5001)
	{
		printk(KERN_ERR "case 5001\n");
		atomic_set(&hdmi_audio_only_flag, HTX_TRUE);
		printk(KERN_ERR "##### case 5000 ####### set hdmi_audio_only_flag as 1.\n");
	}
	else  if(hdmi_param==5002)
	{
		printk(KERN_ERR "##### case 5002 ####### Get hdmi_audio_only_flag=%d\n", atomic_read(&hdmi_audio_only_flag));
	}
	else
	{
		printk(KERN_ERR "unknown command\n");
	}

	printk(KERN_ERR	"%s -\n", __func__);
	return ret;
}
module_param_call(hdmi, hdmi_param_set, param_get_long, &hdmi_param, S_IWUSR | S_IRUGO);

static int i2c_hdmi_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	struct hdmi_driver_data_t *hdmi_driver_data_p = &hdmimain_driver_data;

	printk(KERN_ERR "[HDMI] i2c_hdmi_probe+\n");

	client->driver = &i2c_hdmi_driver;
	i2c_set_clientdata(client, &hdmimain_driver_data);
	hdmi_driver_data_p->i2c_hdmi_client = client;

	
	hdmi_driver_data_p->gpio_num       = HDMI_INT_GPIO; 
	hdmi_driver_data_p->irq		 = MSM_GPIO_TO_INT( hdmi_driver_data_p->gpio_num );

	ret = lcdc_hdmi_config_gpio(hdmi_driver_data_p->gpio_num);
	if (!ret) {

#ifdef HDCP_OPTION
	INIT_DELAYED_WORK( &g_hdcpWork, hdcp_timer_2_second_handler );
	printk(KERN_ERR "init HDCP DELAYED_WORK\n");
#endif

#ifdef HDMI_TESTMODE
		#ifndef HDMI_PWR_ONOFF_BY_AP
		hdmi_init();
		#endif
		
		INIT_WORK(&qhdmi_irqwork, qhdmi_irqWorkHandler);
		printk(KERN_ERR "hdmi_probe: hdmi_driver_data_p->irq=%d, hdmi_driver_data_p->gpio_num=%d\n", hdmi_driver_data_p->irq, hdmi_driver_data_p->gpio_num);
		qhdmi_irqsetup(hdmi_driver_data_p->gpio_num);
#else
		
		
		printk(KERN_ERR "set cable in GPIO=%d\n", HDMI_CABLEIN_GPIO);
		INIT_WORK(&qhdmi_cablein_irqwork, qhdmi_cablein_WorkHandler);
		qhdmi_cablein_irqsetup(HDMI_CABLEIN_GPIO);

		
		printk(KERN_ERR "hdmi_probe: hdmi_driver_data_p->irq=%d, hdmi_driver_data_p->gpio_num=%d\n", hdmi_driver_data_p->irq, hdmi_driver_data_p->gpio_num);
		qhdmi_irqsetup(hdmi_driver_data_p->gpio_num);
		INIT_WORK(&qhdmi_irqwork, qhdmi_irqWorkHandler);
#endif

#if defined CEC_AVAILABLE
		printk(KERN_ERR "[HDMI] call cec_poweron()\n");
		cec_poweron();
#endif
	}

	printk(KERN_ERR "[HDMI] i2c_hdmi_probe-ret:%d\n", ret);
	return ret;
}

static int i2c_hdmi_remove(struct i2c_client *client)
{
	printk(KERN_DEBUG "[HDMI] i2c_hdmi_remove+\n");
	printk(KERN_DEBUG "[HDMI] i2c_hdmi_remove-\n");
	return 0;
}


void lcdc_hdmi_init()
{
	int ret = 0;

	printk(KERN_ERR "lcdc_hdmi_init(): i2c_add_driver...HDMI\n");
	memset(&hdmimain_driver_data, 0, sizeof(struct hdmi_driver_data_t));
	ret = i2c_add_driver(&i2c_hdmi_driver);

	memset(&hdmiedid_driver_data, 0, sizeof(struct hdmi_driver_data_t));
	ret = i2c_add_driver(&i2c_hdmiedid_driver);

	memset(&hdmicec_driver_data, 0, sizeof(struct hdmi_driver_data_t));
	ret = i2c_add_driver(&i2c_hdmicec_driver);

	memset(&hdmipkt_driver_data, 0, sizeof(struct hdmi_driver_data_t));
	ret = i2c_add_driver(&i2c_hdmipkt_driver);

	printk(KERN_ERR "%s, system_rev=%d\n", __func__, system_rev);
	printk(KERN_ERR "lcdc_hdmi_init(): Finish i2c_add_driver...ret=%d...HDMI\n",ret);
}

module_init(lcdc_adv7520_init);
