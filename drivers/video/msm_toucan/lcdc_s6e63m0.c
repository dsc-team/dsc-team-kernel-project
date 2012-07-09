#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/spi/spi.h>
#include <mach/gpio.h>
#include <mach/vreg.h>
#include <mach/pmic.h>
#include <mach/austin_hwid.h>
#include <mach/pm_log.h>
#include "msm_fb.h"

#define LONGCHARGER48HR_WAKEUP_HANG_ATLCD_WORKAROUND	1
#if	LONGCHARGER48HR_WAKEUP_HANG_ATLCD_WORKAROUND
#include <linux/wakelock.h>
static struct wake_lock lcd_suspendlock;
static struct wake_lock lcd_idlelock;
static int lcd_can_wakelock=0;
#endif


#define DBG_MONITOR 0
#define ENABLE_ESD 0
#define ENABLE_SPI_DELAY 0
#define ENABLE_CTL_PWR 1
#define ENABLE_GPIO_RESET 1
#define ENABLE_ACL_CTL 0
#define ENABLE_ELVSS_CTL 1
#define ENABLE_ROTATE_180 0
#define BYTE_PER_TRANS 3

#if ENABLE_ESD
#define ENABLE_SPI_READ 1
#else
#define ENABLE_SPI_READ 0
#endif

#define LCDC_LOG_ENABLE 1
#define LCDC_LOG_DBG 0
#define LCDC_LOG_INFO 0
#define LCDC_LOG_WARNING 1
#define LCDC_LOG_ERR 1

#if LCDC_LOG_ENABLE
	#if LCDC_LOG_DBG
		#define LCDC_LOGD printk
#else
		#define LCDC_LOGD(a...) {}
	#endif

	#if LCDC_LOG_INFO
		#define LCDC_LOGI printk
	#else
		#define LCDC_LOGI(a...) {}
	#endif

	#if LCDC_LOG_WARNING
		#define LCDC_LOGW printk
	#else
		#define LCDC_LOGW(a...) {}
	#endif

	#if LCDC_LOG_ERR
		#define LCDC_LOGE printk
	#else
		#define LCDC_LOGE(a...) {}
	#endif
#else
	#define LCDC_LOGD(a...) {}
	#define LCDC_LOGI(a...) {}
	#define LCDC_LOGW(a...) {}
	#define LCDC_LOGE(a...) {}
#endif

#define LCDC_S6E63M0_SPI_NAME "lcdc_s6e63m0_spi"
#define LCDC_S6E63M0_NAME "lcdc_s6e63m0"
#define LCDC_S6E63M0_RESET_PIN	103
#define LCDC_S6E63M0_RESET_PIN_EVT2 145
#define LCDC_S6E63M0_GAMMA_TABLE_MAX 26

#define LCDC_S6E63M0_PANEL_CON_CMD 0xF8
#define LCDC_S6E63M0_PANEL_CON_PARAM1_DOTC		0x01
#define LCDC_S6E63M0_PANEL_CON_PARAM2_CLWEA	0x27
#define LCDC_S6E63M0_PANEL_CON_PARAM3_CLWEB	0x27
#define LCDC_S6E63M0_PANEL_CON_PARAM4_CLTE		0x07
#define LCDC_S6E63M0_PANEL_CON_PARAM5_SHE		0x07
#define LCDC_S6E63M0_PANEL_CON_PARAM6_FLTE		0x54
#define LCDC_S6E63M0_PANEL_CON_PARAM7_FLWE		0x9F
#define LCDC_S6E63M0_PANEL_CON_PARAM8_SCTE		0x63
#define LCDC_S6E63M0_PANEL_CON_PARAM9_SCWE		0x86
#define LCDC_S6E63M0_PANEL_CON_PARAM10_INTE	0x1A
#define LCDC_S6E63M0_PANEL_CON_PARAM11_INWE	0x33
#define LCDC_S6E63M0_PANEL_CON_PARAM12_EMPS	0x0D
#define LCDC_S6E63M0_PANEL_CON_PARAM13_E_INTE	0x00
#define LCDC_S6E63M0_PANEL_CON_PARAM14_E_INWE	0x00

#define LCDC_S6E63M0_DISPCTL_CMD 0xF2
#define LCDC_S6E63M0_DISPCTL_PARAM1_NL			0x02
#define LCDC_S6E63M0_DISPCTL_PARAM2_VBP			0x03
#define LCDC_S6E63M0_DISPCTL_PARAM3_VFP			0x1C
#define LCDC_S6E63M0_DISPCTL_PARAM4_HBP			0x10
#define LCDC_S6E63M0_DISPCTL_PARAM5_HFP			0x10

#define LCDC_S6E63M0_GTCON_CMD 0xF7
#if ENABLE_ROTATE_180
#define LCDC_S6E63M0_GTCON_PARAM1_GTCON_SS		0x03
#else
#define LCDC_S6E63M0_GTCON_PARAM1_GTCON_SS		0x00
#endif
#define LCDC_S6E63M0_GTCON_PARAM2_DM			0x00
#define LCDC_S6E63M0_GTCON_PARAM3_XPL_RIM		0x00

#define LCDC_S6E63M0_ETC_F6_CMD 0xF6
#define LCDC_S6E63M0_ETC_F6_PARAM1			0x00
#define LCDC_S6E63M0_ETC_F6_PARAM2			0x8E
#define LCDC_S6E63M0_ETC_F6_PARAM3			0x07

#define LCDC_S6E63M0_ETC_B3_CMD 0xB3
#define LCDC_S6E63M0_ETC_B3_PARAM1			0x0C

#define LCDC_S6E63M0_TEMP_SWIRE_CMD 0xB2
#define LCDC_S6E63M0_TEMP_SWIRE_PARAM1			0x0C
#define LCDC_S6E63M0_TEMP_SWIRE_PARAM2			0x0C
#define LCDC_S6E63M0_TEMP_SWIRE_PARAM3			0x08
#define LCDC_S6E63M0_TEMP_SWIRE_PARAM4			0x02

#define LCDC_S6E63M0_ELVSS_CON_CMD 0xB1
#define LCDC_S6E63M0_ELVSS_CON_PARAM1_OFF		0x0A
#define LCDC_S6E63M0_ELVSS_CON_PARAM1_ON		0x0B

#define LCDC_S6E63M0_ACL_LUT_CMD 0xC1
#define LCDC_S6E63M0_ACL_LUT_PARAM1_AKR			0x4D
#define LCDC_S6E63M0_ACL_LUT_PARAM2_AKG			0x96
#define LCDC_S6E63M0_ACL_LUT_PARAM3_AKB			0x1D
#define LCDC_S6E63M0_ACL_LUT_PARAM4_AHSP		0x00
#define LCDC_S6E63M0_ACL_LUT_PARAM5_AHSP		0x00
#define LCDC_S6E63M0_ACL_LUT_PARAM6_AHEP		0x01
#define LCDC_S6E63M0_ACL_LUT_PARAM7_AHEP		0xDF
#define LCDC_S6E63M0_ACL_LUT_PARAM8_AVSP		0x00
#define LCDC_S6E63M0_ACL_LUT_PARAM9_AVSP		0x00
#define LCDC_S6E63M0_ACL_LUT_PARAM10_AVEP		0x03
#define LCDC_S6E63M0_ACL_LUT_PARAM11_AVEP		0x1F
#define LCDC_S6E63M0_ACL_LUT_PARAM12_DY0		0x00
#define LCDC_S6E63M0_ACL_LUT_PARAM13_DY1		0x00
#define LCDC_S6E63M0_ACL_LUT_PARAM14_DY2		0x00
#define LCDC_S6E63M0_ACL_LUT_PARAM15_DY3		0x00
#define LCDC_S6E63M0_ACL_LUT_PARAM16_DY4		0x00
#define LCDC_S6E63M0_ACL_LUT_PARAM17_DY5		0x00
#define LCDC_S6E63M0_ACL_LUT_PARAM18_DY6		0x00
#define LCDC_S6E63M0_ACL_LUT_PARAM19_DY7		0x00
#define LCDC_S6E63M0_ACL_LUT_PARAM20_DY8		0x03
#define LCDC_S6E63M0_ACL_LUT_PARAM21_DY9		0x07
#define LCDC_S6E63M0_ACL_LUT_PARAM22_DY10		0x0A
#define LCDC_S6E63M0_ACL_LUT_PARAM23_DY11		0x0E
#define LCDC_S6E63M0_ACL_LUT_PARAM24_DY12		0x11
#define LCDC_S6E63M0_ACL_LUT_PARAM25_DY13		0x14
#define LCDC_S6E63M0_ACL_LUT_PARAM26_DY14		0x18
#define LCDC_S6E63M0_ACL_LUT_PARAM27_DY15		0x1B

#define LCDC_S6E63M0_ACL_CON_CMD 0xC0
#define LCDC_S6E63M0_ACL_CON_PARAM1_ON			0x01
#define LCDC_S6E63M0_ACL_CON_PARAM1_OFF			0x00

#define LCDC_S6E63M0_GAMMA_CTL_CMD 0xFA
#define LCDC_S6E63M0_GAMMA_CTL_PARAM1_UPDATE_DISABLE	0x02
#define LCDC_S6E63M0_GAMMA_CTL_PARAM1_UPDATE_ENABLE	0x03

#define LCDC_S6E63M0_SLPIN_CMD 0x10
#define LCDC_S6E63M0_SLPOUT_CMD 0x11

#define LCDC_S6E63M0_DISPOFF_CMD 0x28
#define LCDC_S6E63M0_DISPON_CMD 0x29

#define LCDC_S6E63M0_VPW 2
#define LCDC_S6E63M0_VBP (LCDC_S6E63M0_DISPCTL_PARAM2_VBP - LCDC_S6E63M0_VPW)
#define LCDC_S6E63M0_VFP LCDC_S6E63M0_DISPCTL_PARAM3_VFP
#define LCDC_S6E63M0_HPW 2
#define LCDC_S6E63M0_HBP (LCDC_S6E63M0_DISPCTL_PARAM4_HBP - LCDC_S6E63M0_HPW)
#define LCDC_S6E63M0_HFP LCDC_S6E63M0_DISPCTL_PARAM5_HFP


static int __devinit lcdc_s6e63m0_spi_probe(struct spi_device *spi);
static int __devexit lcdc_s6e63m0_spi_remove(struct spi_device *spi);
#ifdef CONFIG_PM
static int lcdc_s6e63m0_spi_suspend(struct spi_device *spi, pm_message_t mesg);
static int lcdc_s6e63m0_spi_resume(struct spi_device *spi);
#else
#define lcdc_s6e63m0_spi_suspend NULL
#define lcdc_s6e63m0_spi_resume NULL
#endif /* CONFIG_PM */
static int __ref lcdc_s6e63m0_probe(struct platform_device *pdev);
static int lcdc_s6e63m0_panel_on(struct platform_device *pdev);
static int lcdc_s6e63m0_panel_off(struct platform_device *pdev);
static void lcdc_s6e63m0_set_backlight(struct msm_fb_data_type *mfd);

int lcdc_s6e63m0_os_engineer_mode_test(void __user *p);

static uint8_t panel_con_set_default_value[] = {
	LCDC_S6E63M0_PANEL_CON_PARAM1_DOTC,
	LCDC_S6E63M0_PANEL_CON_PARAM2_CLWEA,
	LCDC_S6E63M0_PANEL_CON_PARAM3_CLWEB,
	LCDC_S6E63M0_PANEL_CON_PARAM4_CLTE,
	LCDC_S6E63M0_PANEL_CON_PARAM5_SHE,
	LCDC_S6E63M0_PANEL_CON_PARAM6_FLTE,
	LCDC_S6E63M0_PANEL_CON_PARAM7_FLWE,
	LCDC_S6E63M0_PANEL_CON_PARAM8_SCTE,
	LCDC_S6E63M0_PANEL_CON_PARAM9_SCWE,
	LCDC_S6E63M0_PANEL_CON_PARAM10_INTE,
	LCDC_S6E63M0_PANEL_CON_PARAM11_INWE,
	LCDC_S6E63M0_PANEL_CON_PARAM12_EMPS,
	LCDC_S6E63M0_PANEL_CON_PARAM13_E_INTE,
	LCDC_S6E63M0_PANEL_CON_PARAM14_E_INWE
};

static uint8_t dispctl_set_default_value[] = {
	LCDC_S6E63M0_DISPCTL_PARAM1_NL,
	LCDC_S6E63M0_DISPCTL_PARAM2_VBP,
	LCDC_S6E63M0_DISPCTL_PARAM3_VFP,
	LCDC_S6E63M0_DISPCTL_PARAM4_HBP,
	LCDC_S6E63M0_DISPCTL_PARAM5_HFP
};

static uint8_t gtcon_set_default_value[] = {
	LCDC_S6E63M0_GTCON_PARAM1_GTCON_SS,
	LCDC_S6E63M0_GTCON_PARAM2_DM,
	LCDC_S6E63M0_GTCON_PARAM3_XPL_RIM
};

static uint8_t etc_f6_set_default_value[] = {
	LCDC_S6E63M0_ETC_F6_PARAM1,
	LCDC_S6E63M0_ETC_F6_PARAM2,
	LCDC_S6E63M0_ETC_F6_PARAM3
};

static uint8_t etc_b3_set_default_value[] = {
	LCDC_S6E63M0_ETC_B3_PARAM1
};

#if ENABLE_ELVSS_CTL
static uint8_t temp_swire_set_default_value[] = {
	LCDC_S6E63M0_TEMP_SWIRE_PARAM1,
	LCDC_S6E63M0_TEMP_SWIRE_PARAM2,
	LCDC_S6E63M0_TEMP_SWIRE_PARAM3,
	LCDC_S6E63M0_TEMP_SWIRE_PARAM4
};

static uint8_t elvss_con_set_default_value[] = {
	LCDC_S6E63M0_ELVSS_CON_PARAM1_ON
};
#endif

#if ENABLE_ACL_CTL
static uint8_t acl_lut_set_default_value[] = {
	LCDC_S6E63M0_ACL_LUT_PARAM1_AKR,
	LCDC_S6E63M0_ACL_LUT_PARAM2_AKG,
	LCDC_S6E63M0_ACL_LUT_PARAM3_AKB,
	LCDC_S6E63M0_ACL_LUT_PARAM4_AHSP,
	LCDC_S6E63M0_ACL_LUT_PARAM5_AHSP,
	LCDC_S6E63M0_ACL_LUT_PARAM6_AHEP,
	LCDC_S6E63M0_ACL_LUT_PARAM7_AHEP,
	LCDC_S6E63M0_ACL_LUT_PARAM8_AVSP,
	LCDC_S6E63M0_ACL_LUT_PARAM9_AVSP,
	LCDC_S6E63M0_ACL_LUT_PARAM10_AVEP,
	LCDC_S6E63M0_ACL_LUT_PARAM11_AVEP,
	LCDC_S6E63M0_ACL_LUT_PARAM12_DY0,
	LCDC_S6E63M0_ACL_LUT_PARAM13_DY1,
	LCDC_S6E63M0_ACL_LUT_PARAM14_DY2,
	LCDC_S6E63M0_ACL_LUT_PARAM15_DY3,
	LCDC_S6E63M0_ACL_LUT_PARAM16_DY4,
	LCDC_S6E63M0_ACL_LUT_PARAM17_DY5,
	LCDC_S6E63M0_ACL_LUT_PARAM18_DY6,
	LCDC_S6E63M0_ACL_LUT_PARAM19_DY7,
	LCDC_S6E63M0_ACL_LUT_PARAM20_DY8,
	LCDC_S6E63M0_ACL_LUT_PARAM21_DY9,
	LCDC_S6E63M0_ACL_LUT_PARAM22_DY10,
	LCDC_S6E63M0_ACL_LUT_PARAM23_DY11,
	LCDC_S6E63M0_ACL_LUT_PARAM24_DY12,
	LCDC_S6E63M0_ACL_LUT_PARAM25_DY13,
	LCDC_S6E63M0_ACL_LUT_PARAM26_DY14,
	LCDC_S6E63M0_ACL_LUT_PARAM27_DY15
};

static uint8_t acl_con_set_default_value[] = {
	LCDC_S6E63M0_ACL_CON_PARAM1_ON
};
#endif

static uint8_t brightness_gamma_table[LCDC_S6E63M0_GAMMA_TABLE_MAX][22] = {
		{
			LCDC_S6E63M0_GAMMA_CTL_PARAM1_UPDATE_DISABLE,
			0x18, 0x08, 0x24, 0x8B, 0x45, 0x63, 0xC9,
			0xC7, 0xBC, 0xC2, 0xC7, 0xB7, 0xD4, 0xD7,
			0xCB, 0x00, 0x63, 0x00, 0x5E, 0x00, 0x80},

		{
			LCDC_S6E63M0_GAMMA_CTL_PARAM1_UPDATE_DISABLE,
			0x18, 0x08, 0x24, 0x8B, 0x45, 0x63, 0xC9,
			0xC7, 0xBC, 0xC2, 0xC7, 0xB7, 0xD4, 0xD7,
			0xCB, 0x00, 0x63, 0x00, 0x5E, 0x00, 0x80},

		{
			LCDC_S6E63M0_GAMMA_CTL_PARAM1_UPDATE_DISABLE,
			0x18, 0x08, 0x24, 0x8B, 0x45, 0x63, 0xC9,
			0xC7, 0xBC, 0xC2, 0xC7, 0xB7, 0xD4, 0xD7,
			0xCB, 0x00, 0x63, 0x00, 0x5E, 0x00, 0x80},


		{
			LCDC_S6E63M0_GAMMA_CTL_PARAM1_UPDATE_DISABLE,
			0x18, 0x08, 0x24, 0x8B, 0x45, 0x63, 0xC9,
			0xC7, 0xBC, 0xC2, 0xC7, 0xB7, 0xD4, 0xD7,
			0xCB, 0x00, 0x63, 0x00, 0x5E, 0x00, 0x80},

		{
			LCDC_S6E63M0_GAMMA_CTL_PARAM1_UPDATE_DISABLE,
			0x18, 0x08, 0x24, 0x85, 0x49, 0x5C, 0xC7,
			0xC7, 0xBA, 0xC1, 0xC6, 0xB6, 0xD3, 0xD6,
			0xCA, 0x00, 0x69, 0x00, 0x64, 0x00, 0x88},

		{
			LCDC_S6E63M0_GAMMA_CTL_PARAM1_UPDATE_DISABLE,
			0x18, 0x08, 0x24, 0x80, 0x4F, 0x59, 0xC6,
			0xC7, 0xB9, 0xC0, 0xC5, 0xB5, 0xD0, 0xD4,
			0xC8, 0x00, 0x70, 0x00, 0x6A, 0x00, 0x90},

		{
			LCDC_S6E63M0_GAMMA_CTL_PARAM1_UPDATE_DISABLE,
			0x18, 0x08, 0x24, 0x7E, 0x55, 0x58, 0xC4,
			0xC5, 0xB7, 0xBF, 0xC4, 0xB3, 0xD0, 0xD4,
			0xC8, 0x00, 0x75, 0x00, 0x6F, 0x00, 0x97},

		{
			LCDC_S6E63M0_GAMMA_CTL_PARAM1_UPDATE_DISABLE,
			0x18, 0x08, 0x24, 0x7E, 0x5B, 0x59, 0xC4,
			0xC6, 0xB7, 0xBE, 0xC2, 0xB1, 0xCE, 0xD3,
			0xC6, 0x00, 0x7A, 0x00, 0x74, 0x00, 0x9F},

		{
			LCDC_S6E63M0_GAMMA_CTL_PARAM1_UPDATE_DISABLE,
			0x18, 0x08, 0x24, 0x79, 0x5B, 0x52, 0xC4,
			0xC6, 0xB7, 0xBD, 0xC2, 0xB1, 0xCE, 0xD2,
			0xC6, 0x00, 0x7E, 0x00, 0x78, 0x00, 0xA3},

		{
			LCDC_S6E63M0_GAMMA_CTL_PARAM1_UPDATE_DISABLE,
			0x18, 0x08, 0x24, 0x7A, 0x5D, 0x54, 0xC0,
			0xC5, 0xB5, 0xBD, 0xC2, 0xB1, 0xCC, 0xD0,
			0xC3, 0x00, 0x83, 0x00, 0x7D, 0x00, 0xAA},

		{
			LCDC_S6E63M0_GAMMA_CTL_PARAM1_UPDATE_DISABLE,
			0x18, 0x08, 0x24, 0x75, 0x5E, 0x4F, 0xC2,
			0xC5, 0xB5, 0xBB, 0xC1, 0xB0, 0xCC, 0xCF,
			0xC3, 0x00, 0x87, 0x00, 0x81, 0x00, 0xAF},

		{
			LCDC_S6E63M0_GAMMA_CTL_PARAM1_UPDATE_DISABLE,
			0x18, 0x08, 0x24, 0x78, 0x64, 0x52, 0xC2,
			0xC5, 0xB5, 0xB9, 0xBF, 0xAD, 0xCC, 0xD0,
			0xC4, 0x00, 0x8B, 0x00, 0x84, 0x00, 0xB4},

		{
			LCDC_S6E63M0_GAMMA_CTL_PARAM1_UPDATE_DISABLE,
			0x18, 0x08, 0x24, 0x75, 0x62, 0x50, 0xBF,
			0xC3, 0xB2, 0xBA, 0xBF, 0xAE, 0xCB, 0xCF,
			0xC3, 0x00, 0x8F, 0x00, 0x88, 0x00, 0xB9},

		{	
			LCDC_S6E63M0_GAMMA_CTL_PARAM1_UPDATE_DISABLE,
			0x18, 0x08, 0x24, 0x74, 0x65, 0x50, 0xBF,
			0xC3, 0xB2, 0xB9, 0xBE, 0xAD, 0xC9, 0xCD,
			0xC0, 0x00, 0x93, 0x00, 0x8C, 0x00, 0xBF},

		{	
			LCDC_S6E63M0_GAMMA_CTL_PARAM1_UPDATE_DISABLE,
			0x18, 0x08, 0x24, 0x73, 0x66, 0x50, 0xBE,
			0xC2, 0xB1, 0xB9, 0xBE, 0xAC, 0xC9, 0xCD,
			0xC1, 0x00, 0x96, 0x00, 0x8F, 0x00, 0xC3},

		{	
			LCDC_S6E63M0_GAMMA_CTL_PARAM1_UPDATE_DISABLE,
			0x18, 0x08, 0x24, 0x70, 0x64, 0x4D, 0xC0,
			0xC4, 0xB3, 0xB7, 0xBC, 0xAA, 0xC8, 0xCC,
			0xC0, 0x00, 0x9A, 0x00, 0x93, 0x00, 0xC8},

		{	
			LCDC_S6E63M0_GAMMA_CTL_PARAM1_UPDATE_DISABLE,
			0x18, 0x08, 0x24, 0x70, 0x64, 0x4D, 0xBF,
			0xC3, 0xB2, 0xB7, 0xBC, 0xAA, 0xC7, 0xCB,
			0xBF, 0x00, 0x9D, 0x00, 0x96, 0x00, 0xCC},

		{	
			LCDC_S6E63M0_GAMMA_CTL_PARAM1_UPDATE_DISABLE,
			0x18, 0x08, 0x24, 0x72, 0x67, 0x4E, 0xBE,
			0xC2, 0xB1, 0xB6, 0xBC, 0xAA, 0xC6, 0xCA,
			0xBE, 0x00, 0xA1, 0x00, 0x99, 0x00, 0xD0},

		{	
			LCDC_S6E63M0_GAMMA_CTL_PARAM1_UPDATE_DISABLE,
			0x18, 0x08, 0x24, 0x6E, 0x62, 0x47, 0xBD,
			0xC2, 0xB1, 0xB5, 0xBB, 0xA9, 0xC5, 0xC9,
			0xBC, 0x00, 0xA5, 0x00, 0x9D, 0x00, 0xD6},

		{	
			LCDC_S6E63M0_GAMMA_CTL_PARAM1_UPDATE_DISABLE,
			0x18, 0x08, 0x24, 0x6D, 0x67, 0x4A, 0xBE,
			0xC2, 0xB1, 0xB4, 0xBA, 0xA8, 0xC5, 0xC9,
			0xBC, 0x00, 0xA7, 0x00, 0x9F, 0x00, 0xD9},

		{	
			LCDC_S6E63M0_GAMMA_CTL_PARAM1_UPDATE_DISABLE,
			0x18, 0x08, 0x24, 0x6C, 0x66, 0x49, 0xBD,
			0xC1, 0xB2, 0xB4, 0xBA, 0xA8, 0xC5, 0xC9,
			0xBC, 0x00, 0xAA, 0x00, 0xA2, 0x00, 0xDD},

		{	
			LCDC_S6E63M0_GAMMA_CTL_PARAM1_UPDATE_DISABLE,
			0x18, 0x08, 0x24, 0x6B, 0x68, 0x48, 0xBC,
			0xC0, 0xAF, 0xB3, 0xB9, 0xA7, 0xC4, 0xC8,
			0xBB, 0x00, 0xAD, 0x00, 0xA5, 0x00, 0xE1},

		{	
			LCDC_S6E63M0_GAMMA_CTL_PARAM1_UPDATE_DISABLE,
			0x18, 0x08, 0x24, 0x6B, 0x65, 0x48, 0xBD,
			0xC2, 0xB0, 0xB3, 0xB8, 0xA7, 0xC3, 0xC8,
			0xBB, 0x00, 0xB1, 0x00, 0xA8, 0x00, 0xE5},

		{	
			LCDC_S6E63M0_GAMMA_CTL_PARAM1_UPDATE_DISABLE,
			0x18, 0x08, 0x24, 0x6B, 0x65, 0x48, 0xBC,
			0xC0, 0xAF, 0xB2, 0xB9, 0xA6, 0xC3, 0xC7,
			0xBA, 0x00, 0xB3, 0x00, 0xAA, 0x00, 0xE9},

		{	
			LCDC_S6E63M0_GAMMA_CTL_PARAM1_UPDATE_DISABLE,
			0x18, 0x08, 0x24, 0x6A, 0x66, 0x46, 0xBB,
			0xC0, 0xAF, 0xB2, 0xB8, 0xA6, 0xC2, 0xC6,
			0xB9, 0x00, 0xB6, 0x00, 0xAD, 0x00, 0xEC},

		{	
			LCDC_S6E63M0_GAMMA_CTL_PARAM1_UPDATE_DISABLE,
			0x18, 0x08, 0x24, 0x68, 0x66, 0x45, 0xBA,
			0xC0, 0xAE, 0xB2, 0xB7, 0xA5, 0xC3, 0xC8,
			0xBE, 0x00, 0xB8, 0x00, 0xAE, 0x00, 0xEF},
};

struct lcdc_s6e63m0_driver_data_t
{
	struct spi_device        *spi;
	struct work_struct        work_data;
	struct platform_device  *device;
	#if ENABLE_SPI_DELAY
	struct workqueue_struct	*spi_workqueue;
	struct delayed_work	spi_delayed_work;
	#endif
	#if ENABLE_ESD
	struct workqueue_struct	*esd_workqueue;
	struct delayed_work	esd_delayed_work;
	#endif
	unsigned int auto_backlight;
	int32 user_backlight;
	int32 curr_brightness_gamma_table_index;
	int32 first_panel_on;
	atomic_t power_flag;
};

static struct lcdc_s6e63m0_driver_data_t lcdc_s6e63m0_driver_data;

static struct spi_driver lcdc_s6e63m0_spi_driver = {
	.driver = {
		.name  = LCDC_S6E63M0_NAME,
		.owner = THIS_MODULE,
	},
	.probe         = lcdc_s6e63m0_spi_probe,
	.remove        = lcdc_s6e63m0_spi_remove,
	.suspend       = lcdc_s6e63m0_spi_suspend,
	.resume        = lcdc_s6e63m0_spi_resume,
};

static struct platform_driver lcdc_s6e63m0_driver = {
	.probe  = lcdc_s6e63m0_probe,
	.driver = {
		.name   = LCDC_S6E63M0_NAME,
		.owner = THIS_MODULE,
	},
};

static struct msm_fb_panel_data lcdc_s6e63m0_panel_data = {
	.on = lcdc_s6e63m0_panel_on,
	.off = lcdc_s6e63m0_panel_off,
	.set_backlight = lcdc_s6e63m0_set_backlight,
};


static struct platform_device lcdc_s6e63m0_device = {
	.name   = LCDC_S6E63M0_NAME,
	.id	= 1,
	.dev	= {
		.platform_data = &lcdc_s6e63m0_panel_data,
	}
};

DECLARE_MUTEX(sem_access);
DECLARE_MUTEX(sem_gamma_set);

static int lcdc_s6e63m0_gpio_setup(void)
{
	int ret = 0;
	int gpio_num;

	LCDC_LOGD(KERN_DEBUG "[LCDC] %s+\n", __func__);

	for(gpio_num=111 ; gpio_num<=134 ; gpio_num++)
	{
		gpio_tlmm_config(GPIO_CFG(gpio_num, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	}
	gpio_tlmm_config(GPIO_CFG(135, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(136, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(137, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), GPIO_CFG_ENABLE);
	gpio_tlmm_config(GPIO_CFG(138, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_4MA), GPIO_CFG_ENABLE);

	LCDC_LOGD(KERN_DEBUG "[LCDC] %s-\n", __func__);
	return ret;
}

#if ENABLE_SPI_READ
static int lcdc_s6e63m0_spi_read(struct spi_device *spi, uint32 command, uint8_t *data)
{
	#if 1
	int ret = 0;
	u8 tx_buf[4];
	u8 rx_buf[4];
	struct spi_message          m;
	struct spi_transfer         t;

	LCDC_LOGD(KERN_DEBUG "[LCDC] %s+\n", __func__);

	ret = down_interruptible(&sem_access);
	if (ret)
	{
		LCDC_LOGW(KERN_WARNING "[LCDC] %s:: The sleep is interrupted by a signal\n", __func__);
	}

	memset(&t, 0, sizeof t);
	t.tx_buf = tx_buf;
	t.rx_buf = rx_buf;
	t.len = 3;

	spi->bits_per_word = 17;
	spi_setup(spi);
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	tx_buf[0] = (command >> 1) & 0x7F;
	tx_buf[1] = ((command << 7) & 0x80) | 0x7F;
	tx_buf[2] = 0x80;

	ret = spi_sync(spi, &m);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: spi_sync err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_spi_read_err_spi_sync;
	}

	*data = rx_buf[2] & 0xFF;

lcdc_s6e63m0_spi_read_err_spi_sync:
	up(&sem_access);

	LCDC_LOGD(KERN_DEBUG "[LCDC] %s-\n", __func__);
	return ret;
	#else
	int ret = 0;
	uint32 bits, dbit;
	int bnum;

	LCDC_LOGD(KERN_DEBUG "[LCDC] %s+\n", __func__);

	ret = down_interruptible(&sem_access);
	if (ret)
	{
		LCDC_LOGW(KERN_WARNING "[LCDC] %s:: The sleep is interrupted by a signal\n", __func__);
	}

	gpio_tlmm_config(GPIO_CFG(17, 0, GPIO_CFG_OUTPUT,  GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);	//CLK
	gpio_tlmm_config(GPIO_CFG(18, 0, GPIO_CFG_OUTPUT,  GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);	//SDI
	gpio_tlmm_config(GPIO_CFG(20, 0, GPIO_CFG_OUTPUT,  GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);	//CS

	gpio_set_value(20, 0);
		gpio_set_value(17, 0);

	gpio_set_value(18, 0);
	udelay(1);
	gpio_set_value(17, 1);
	udelay(1);
	bnum = 8;
	bits = 0x80;
	while (bnum) {
		gpio_set_value(17, 0);
		if (command & bits)
			gpio_set_value(18, 1);
		else
			gpio_set_value(18, 0);
		udelay(1);
		gpio_set_value(17, 1);
		udelay(1);
		bits >>= 1;
		bnum--;
	}

	gpio_tlmm_config(GPIO_CFG(18, 0, GPIO_INPUT,  GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);

	bnum = 1 * 8;
	bits = 0;
	while (bnum) {
		bits <<= 1;
		gpio_set_value(17, 0);
		udelay(1);
		dbit = gpio_get_value(18);
		udelay(1);
		gpio_set_value(17, 1);
		bits |= dbit;
		bnum--;
	}

	*data = bits & 0xFF;

	printk("[LCDC] gpio spi cmd=%d, data=%d\n", command, bits);

	udelay(1);
	gpio_set_value(20, 1);
	udelay(1);

	gpio_tlmm_config(GPIO_CFG(17, 1, GPIO_INPUT,  GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);
	gpio_tlmm_config(GPIO_CFG(18, 1, GPIO_INPUT,  GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);
	gpio_tlmm_config(GPIO_CFG(20, 1, GPIO_INPUT,  GPIO_NO_PULL, GPIO_2MA), GPIO_ENABLE);

	up(&sem_access);
	return ret;
	#endif
}
#endif

static int lcdc_s6e63m0_spi_write_wo_param(struct spi_device *spi, uint32 command)
{
	int ret = 0;
	u8 tx_buf[2];
	struct spi_message          m;
	struct spi_transfer         t;

	LCDC_LOGD(KERN_DEBUG "[LCDC] %s+\n", __func__);

	ret = down_interruptible(&sem_access);
	if (ret)
	{
		LCDC_LOGW(KERN_WARNING "[LCDC] %s:: The sleep is interrupted by a signal\n", __func__);
	}

	memset(&t, 0, sizeof t);
	t.tx_buf = tx_buf;
	t.rx_buf = NULL;
	t.len = 2;

	spi->bits_per_word = 9;
	spi_setup(spi);
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	tx_buf[0] = (command >> 1) & 0x7F;
	tx_buf[1] = (command << 7) & 0x80;

	ret = spi_sync(spi, &m);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: spi_sync err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_spi_write_wo_param_err_spi_sync;
	}

lcdc_s6e63m0_spi_write_wo_param_err_spi_sync:
	up(&sem_access);

	LCDC_LOGD(KERN_DEBUG "[LCDC] %s-\n", __func__);
	return ret;
}

static int lcdc_s6e63m0_spi_write(struct spi_device *spi, uint32 command, uint8_t *param_buf, int32 length)
{
	#if (BYTE_PER_TRANS == 1)
	int ret = 0;
	int i;
	u8 tx_buf[2];
	struct spi_message          m;
	struct spi_transfer         t;

	LCDC_LOGD(KERN_DEBUG "[LCDC] %s+\n", __func__);

	if (length == 0)
		return lcdc_s6e63m0_spi_write_wo_param(spi, command);

	if (param_buf == NULL || length < 0)
		return -EINVAL;

	ret = down_interruptible(&sem_access);
	if (ret)
	{
		LCDC_LOGW(KERN_WARNING "[LCDC] %s:: The sleep is interrupted by a signal\n", __func__);
	}

	memset(&t, 0, sizeof t);
	t.tx_buf = tx_buf;
	t.rx_buf = NULL;
	t.len = 2;
	spi->bits_per_word = 9;

	spi_setup(spi);
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	tx_buf[0] = (command >> 1) & 0x7F;
	tx_buf[1] = (command << 7) & 0x80;

	ret = spi_sync(spi, &m);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: spi_sync err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_spi_write_err_spi_sync;
	}

	for (i = 0; i < length; i++)
	{
		tx_buf[0] = ((param_buf[i] >> 1) | 0x80) & 0xFF;
		tx_buf[1] = (param_buf[i] << 7) & 0x80;

		ret = spi_sync(spi, &m);
		if (ret)
		{
			LCDC_LOGE(KERN_ERR "[LCDC] %s:: spi_sync err ret=%d\n", __func__, ret);
			goto lcdc_s6e63m0_spi_write_err_spi_sync;
		}
	}

lcdc_s6e63m0_spi_write_err_spi_sync:
	up(&sem_access);

	LCDC_LOGD(KERN_DEBUG "[LCDC] %s-\n", __func__);
	return ret;
	#elif (BYTE_PER_TRANS == 3)
	int ret = 0;
	int i;
	u8 tx_buf[4];
	struct spi_message          m;
	struct spi_transfer         t;

	LCDC_LOGD(KERN_DEBUG "[LCDC] %s+\n", __func__);

	if (length == 0)
		return lcdc_s6e63m0_spi_write_wo_param(spi, command);

	if (param_buf == NULL || length < 0)
		return -EINVAL;

	ret = down_interruptible(&sem_access);
	if (ret)
	{
		LCDC_LOGW(KERN_WARNING "[LCDC] %s:: The sleep is interrupted by a signal\n", __func__);
	}

	memset(&t, 0, sizeof t);
	t.tx_buf = tx_buf;
	t.rx_buf = NULL;
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	if (length >= 2)
	{
		spi->bits_per_word = 27;
		spi_setup(spi);

		tx_buf[0] = (command >> 1) & 0x7F;
		tx_buf[1] = ((command << 7) | 0x40 | ((param_buf[0] >> 2) & 0x3F)) & 0xFF;
		tx_buf[2] = (((param_buf[0] << 6) & 0xC0) | 0x20 | ((param_buf[1] >> 3) & 0x1F)) & 0xFF;
		tx_buf[3] = (param_buf[1] << 5) & 0xFF;
		t.len = 4;
	}
	else
	{
		spi->bits_per_word = 18;
		spi_setup(spi);

		tx_buf[0] = (command >> 1) & 0x7F;
		tx_buf[1] = ((command << 7) | 0x40 | ((param_buf[0] >> 2) & 0x3F)) & 0xFF;
		tx_buf[2] = (param_buf[0] << 6) & 0xFF;
		t.len = 3;
	}

	ret = spi_sync(spi, &m);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: spi_sync err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_spi_write_err_spi_sync;
	}

	for (i = 2; i < length - 2; i += 3)
	{
		tx_buf[0] = ((param_buf[i] >> 1) | 0x80) & 0xFF;
		tx_buf[1] = ((param_buf[i] << 7) | 0x40 | ((param_buf[i + 1] >> 2) & 0x3F)) & 0xFF;
		tx_buf[2] = (((param_buf[i + 1] << 6) & 0xC0) | 0x20 | ((param_buf[i + 2] >> 3) & 0x1F)) & 0xFF;
		tx_buf[3] = (param_buf[i + 2] << 5) & 0xFF;
		t.len = 4;

		ret = spi_sync(spi, &m);
		if (ret)
		{
			LCDC_LOGE(KERN_ERR "[LCDC] %s:: spi_sync err ret=%d\n", __func__, ret);
			goto lcdc_s6e63m0_spi_write_err_spi_sync;
		}
	}

	if (i == length - 2)
	{
		spi->bits_per_word = 18;
		spi_setup(spi);

		tx_buf[0] = ((param_buf[i] >> 1) | 0x80) & 0xFF;
		tx_buf[1] = ((param_buf[i] << 7) | 0x40 | ((param_buf[i + 1] >> 2) & 0x3F)) & 0xFF;
		tx_buf[2] = (param_buf[i + 1] << 6) & 0xFF;
		t.len = 3;

		ret = spi_sync(spi, &m);
		if (ret)
		{
			LCDC_LOGE(KERN_ERR "[LCDC] %s:: spi_sync err ret=%d\n", __func__, ret);
			goto lcdc_s6e63m0_spi_write_err_spi_sync;
		}
	}

	if (i == length - 1)
	{
		spi->bits_per_word = 9;
		spi_setup(spi);

		tx_buf[0] = ((param_buf[i] >> 1) | 0x80) & 0xFF;
		tx_buf[1] = (param_buf[i] << 7) & 0x80;
		t.len = 2;

		ret = spi_sync(spi, &m);
		if (ret)
		{
			LCDC_LOGE(KERN_ERR "[LCDC] %s:: spi_sync err ret=%d\n", __func__, ret);
			goto lcdc_s6e63m0_spi_write_err_spi_sync;
		}
	}

lcdc_s6e63m0_spi_write_err_spi_sync:
	up(&sem_access);

	LCDC_LOGD(KERN_DEBUG "[LCDC] %s-\n", __func__);
	return ret;
	#else
	int ret = 0;
	int i;
	u8 tx_buf[3];
	struct spi_message          m;
	struct spi_transfer         t;

	LCDC_LOGD(KERN_DEBUG "[LCDC] %s+\n", __func__);

	if (length == 0)
		return lcdc_s6e63m0_spi_write_wo_param(spi, command);

	if (param_buf == NULL || length < 0)
		return -EINVAL;

	ret = down_interruptible(&sem_access);
	if (ret)
	{
		LCDC_LOGW(KERN_WARNING "[LCDC] %s:: The sleep is interrupted by a signal\n", __func__);
	}

	memset(&t, 0, sizeof t);
	t.tx_buf = tx_buf;
	t.rx_buf = NULL;
	spi->bits_per_word = 18;

	spi_setup(spi);
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	tx_buf[0] = (command >> 1) & 0x7F;
	tx_buf[1] = ((command << 7) | 0x40 | ((param_buf[0] >> 2) & 0x3F)) & 0xFF;
	tx_buf[2] = (param_buf[0] << 6) & 0xFF;
	t.len = 3;

	ret = spi_sync(spi, &m);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: spi_sync err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_spi_write_err_spi_sync;
	}

	for (i = 1; i < length - 1; i += 2)
	{
		tx_buf[0] = ((param_buf[i] >> 1) | 0x80) & 0xFF;
		tx_buf[1] = ((param_buf[i] << 7) | 0x40 | ((param_buf[i + 1] >> 2) & 0x3F)) & 0xFF;
		tx_buf[2] = (param_buf[i + 1] << 6) & 0xFF;
		t.len = 3;

		ret = spi_sync(spi, &m);
		if (ret)
		{
			LCDC_LOGE(KERN_ERR "[LCDC] %s:: spi_sync err ret=%d\n", __func__, ret);
			goto lcdc_s6e63m0_spi_write_err_spi_sync;
		}
	}

	if (i == length - 1)
	{
		spi->bits_per_word = 9;
		spi_setup(spi);

		tx_buf[0] = ((param_buf[i] >> 1) | 0x80) & 0xFF;
		tx_buf[1] = (param_buf[i] << 7) & 0x80;
		t.len = 2;

		ret = spi_sync(spi, &m);
		if (ret)
		{
			LCDC_LOGE(KERN_ERR "[LCDC] %s:: spi_sync err ret=%d\n", __func__, ret);
			goto lcdc_s6e63m0_spi_write_err_spi_sync;
		}
	}

lcdc_s6e63m0_spi_write_err_spi_sync:
	up(&sem_access);

	LCDC_LOGD(KERN_DEBUG "[LCDC] %s-\n", __func__);
	return ret;
	#endif
}

static int lcdc_s6e63m0_standby_on(struct spi_device *spi)
{
	int ret = 0;

	LCDC_LOGD(KERN_DEBUG "[LCDC] %s+\n", __func__);

	ret = lcdc_s6e63m0_spi_write(spi, LCDC_S6E63M0_SLPIN_CMD, NULL, 0);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: LCDC_S6E63M0_SLPIN_CMD err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_standby_on_err_write;
	}

	PM_LOG_EVENT(PM_LOG_OFF, PM_LOG_BL_LCD);
	msleep(120);

lcdc_s6e63m0_standby_on_err_write:
	LCDC_LOGD(KERN_DEBUG "[LCDC] %s-\n", __func__);
	return ret;
}

static int lcdc_s6e63m0_standby_off(struct spi_device *spi)
{
	int ret = 0;

	LCDC_LOGD(KERN_DEBUG "[LCDC] %s+\n", __func__);

	ret = lcdc_s6e63m0_spi_write(spi, LCDC_S6E63M0_SLPOUT_CMD, NULL, 0);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: LCDC_S6E63M0_SLPOUT_CMD err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_standby_off_err_write;
	}

	PM_LOG_EVENT(PM_LOG_ON, PM_LOG_BL_LCD);
	msleep(120);

lcdc_s6e63m0_standby_off_err_write:
	LCDC_LOGD(KERN_DEBUG "[LCDC] %s-\n", __func__);
	return ret;
}

static int lcdc_s6e63m0_display_on(struct spi_device *spi)
{
	int ret = 0;

	LCDC_LOGD(KERN_DEBUG "[LCDC] %s+\n", __func__);

	ret = lcdc_s6e63m0_spi_write(spi, LCDC_S6E63M0_DISPON_CMD, NULL, 0);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: LCDC_S6E63M0_DISPON_CMD err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_display_on_err_write;
	}

lcdc_s6e63m0_display_on_err_write:
	LCDC_LOGD(KERN_DEBUG "[LCDC] %s-\n", __func__);
	return ret;
}

#if 0
static int lcdc_s6e63m0_display_off(struct spi_device *spi)
{
	int ret = 0;

	LCDC_LOGD(KERN_DEBUG "[LCDC] %s+\n", __func__);

	ret = lcdc_s6e63m0_spi_write(spi, LCDC_S6E63M0_DISPOFF_CMD, NULL, 0);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: LCDC_S6E63M0_DISPOFF_CMD err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_display_off_err_write;
	}

lcdc_s6e63m0_display_off_err_write:
	LCDC_LOGD(KERN_DEBUG "[LCDC] %s-\n", __func__);
	return ret;
}
#endif

static int lcdc_s6e63m0_reset(void)
{
	LCDC_LOGD(KERN_DEBUG "[LCDC] %s+\n", __func__);

	#if ENABLE_GPIO_RESET
	if ((system_rev <= TOUCAN_EVT1_Band148) || system_rev == HWID_UNKNOWN)
	{
		gpio_set_value(LCDC_S6E63M0_RESET_PIN, 0);
		mdelay(1);
		gpio_set_value(LCDC_S6E63M0_RESET_PIN, 1);
		msleep(10);
	}
	else
	{
		gpio_set_value(LCDC_S6E63M0_RESET_PIN_EVT2, 0);
		mdelay(1);
		gpio_set_value(LCDC_S6E63M0_RESET_PIN_EVT2, 1);
		msleep(10);
	}
	#endif

	LCDC_LOGD(KERN_DEBUG "[LCDC] %s-\n", __func__);
	return 0;
}

static int lcdc_s6e63m0_power_on(void)
{
	int ret = 0;
	struct vreg	*vreg_lcdc = vreg_get(0, "gp1");

	LCDC_LOGD(KERN_DEBUG "[LCDC] %s+\n", __func__);

	if (atomic_read(&lcdc_s6e63m0_driver_data.power_flag))
		return ret;

	ret = pmic_vreg_pull_down_switch(OFF_CMD, PM_VREG_PDOWN_GP1_ID);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "%s: pm_vreg_pull_down_switch failed!\n", __func__);
		goto lcdc_s6e63m0_power_on_err_pmic_vreg_pull_down_switch;
	}

	ret = vreg_set_level(vreg_lcdc, 2800);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: vreg_set_level err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_power_on_err_vreg_set_level;
	}

	ret = vreg_enable(vreg_lcdc);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: vreg_enable err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_power_on_err_vreg_en;
	}

	atomic_set(&lcdc_s6e63m0_driver_data.power_flag, 1);
	PM_LOG_EVENT(PM_LOG_ON, PM_LOG_LCD);

	msleep(25);

lcdc_s6e63m0_power_on_err_vreg_en:
lcdc_s6e63m0_power_on_err_vreg_set_level:
lcdc_s6e63m0_power_on_err_pmic_vreg_pull_down_switch:
	LCDC_LOGD(KERN_DEBUG "[LCDC] %s-\n", __func__);
	return ret;
}

static int lcdc_s6e63m0_power_off(void)
{
	int ret = 0;
	struct vreg	*vreg_lcdc = vreg_get(0, "gp1");

	LCDC_LOGD(KERN_DEBUG "[LCDC] %s+\n", __func__);

	ret = pmic_vreg_pull_down_switch(ON_CMD, PM_VREG_PDOWN_GP1_ID);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "%s: pm_vreg_pull_down_switch failed!\n", __func__);
		goto lcdc_s6e63m0_power_off_err_pmic_vreg_pull_down_switch;
	}

	ret = vreg_disable(vreg_lcdc);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: vreg_disable err ret=%d\n", __func__, ret);
	}

	atomic_set(&lcdc_s6e63m0_driver_data.power_flag, 0);
	PM_LOG_EVENT(PM_LOG_OFF, PM_LOG_LCD);

lcdc_s6e63m0_power_off_err_pmic_vreg_pull_down_switch:
	LCDC_LOGD(KERN_DEBUG "[LCDC] %s-\n", __func__);
	return ret;
}

static int lcdc_s6e63m0_panel_condition_set(struct spi_device *spi)
{
	int ret = 0;

	LCDC_LOGD(KERN_DEBUG "[LCDC] %s+\n", __func__);

	ret = lcdc_s6e63m0_spi_write(spi, LCDC_S6E63M0_PANEL_CON_CMD, panel_con_set_default_value, sizeof(panel_con_set_default_value)/sizeof(uint8_t));
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: LCDC_S6E63M0_PANEL_CON_CMD err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_panel_condition_set_err_write;
	}

lcdc_s6e63m0_panel_condition_set_err_write:
	LCDC_LOGD(KERN_DEBUG "[LCDC] %s-\n", __func__);
	return ret;
}

static int lcdc_s6e63m0_display_condition_set(struct spi_device *spi)
{
	int ret = 0;

	LCDC_LOGD(KERN_DEBUG "[LCDC] %s+\n", __func__);

	ret = lcdc_s6e63m0_spi_write(spi, LCDC_S6E63M0_DISPCTL_CMD, dispctl_set_default_value, sizeof(dispctl_set_default_value)/sizeof(uint8_t));
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: LCDC_S6E63M0_DISPCTL_CMD err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_display_condition_set_err_write;
	}

	ret = lcdc_s6e63m0_spi_write(spi, LCDC_S6E63M0_GTCON_CMD, gtcon_set_default_value, sizeof(gtcon_set_default_value)/sizeof(uint8_t));
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: LCDC_S6E63M0_GTCON_CMD err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_display_condition_set_err_write;
	}

lcdc_s6e63m0_display_condition_set_err_write:
	LCDC_LOGD(KERN_DEBUG "[LCDC] %s-\n", __func__);
	return ret;
}

static int lcdc_s6e63m0_gamma_table_set(struct spi_device *spi)
{
	int ret = 0;
	uint8_t buf[1];
	struct lcdc_s6e63m0_driver_data_t *driver_data_p = &lcdc_s6e63m0_driver_data;

	LCDC_LOGD(KERN_DEBUG "[LCDC] %s+\n", __func__);


	ret = down_interruptible(&sem_gamma_set);
	if (ret)
	{
		LCDC_LOGW(KERN_WARNING "[LCDC] %s:: The sleep is interrupted by a signal\n", __func__);
	}

	ret = lcdc_s6e63m0_spi_write(spi, LCDC_S6E63M0_GAMMA_CTL_CMD, brightness_gamma_table[driver_data_p->curr_brightness_gamma_table_index], sizeof(brightness_gamma_table[0])/sizeof(uint8_t));
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: LCDC_S6E63M0_GAMMA_CTL_CMD err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_gamma_table_set_err_write;
	}

	buf[0] = LCDC_S6E63M0_GAMMA_CTL_PARAM1_UPDATE_ENABLE;
	ret = lcdc_s6e63m0_spi_write(spi, LCDC_S6E63M0_GAMMA_CTL_CMD, buf, 1);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: LCDC_S6E63M0_GAMMA_CTL_CMD err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_gamma_table_set_err_write;
	}

lcdc_s6e63m0_gamma_table_set_err_write:
	up(&sem_gamma_set);

	LCDC_LOGD(KERN_DEBUG "[LCDC] %s-\n", __func__);
	return ret;
}

static int lcdc_s6e63m0_etc_condition_set(struct spi_device *spi)
{
	int ret = 0;

	LCDC_LOGD(KERN_DEBUG "[LCDC] %s+\n", __func__);

	ret = lcdc_s6e63m0_spi_write(spi, LCDC_S6E63M0_ETC_F6_CMD, etc_f6_set_default_value, sizeof(etc_f6_set_default_value)/sizeof(uint8_t));
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: LCDC_S6E63M0_ETC_F6_CMD err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_etc_condition_set_err_write;
	}

	ret = lcdc_s6e63m0_spi_write(spi, LCDC_S6E63M0_ETC_B3_CMD, etc_b3_set_default_value, sizeof(etc_b3_set_default_value)/sizeof(uint8_t));
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: LCDC_S6E63M0_ETC_B3_CMD err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_etc_condition_set_err_write;
	}

lcdc_s6e63m0_etc_condition_set_err_write:
	LCDC_LOGD(KERN_DEBUG "[LCDC] %s-\n", __func__);
	return ret;
}

#if ENABLE_ACL_CTL
static int lcdc_s6e63m0_acl_control_set(struct spi_device *spi)
{
	int ret = 0;

	LCDC_LOGD(KERN_DEBUG "[LCDC] %s+\n", __func__);

	ret = lcdc_s6e63m0_spi_write(spi, LCDC_S6E63M0_ACL_LUT_CMD, acl_lut_set_default_value, sizeof(acl_lut_set_default_value)/sizeof(uint8_t));
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: LCDC_S6E63M0_ACL_LUT_CMD err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_acl_control_set_err_write;
	}

	ret = lcdc_s6e63m0_spi_write(spi, LCDC_S6E63M0_ACL_CON_CMD, acl_con_set_default_value, sizeof(acl_con_set_default_value)/sizeof(uint8_t));
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: LCDC_S6E63M0_ACL_CON_CMD err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_acl_control_set_err_write;
	}

lcdc_s6e63m0_acl_control_set_err_write:
	LCDC_LOGD(KERN_DEBUG "[LCDC] %s-\n", __func__);
	return ret;
}
#endif

#if ENABLE_ELVSS_CTL
static int lcdc_s6e63m0_elvss_control_set(struct spi_device *spi)
{
	int ret = 0;

	LCDC_LOGD(KERN_DEBUG "[LCDC] %s+\n", __func__);

	ret = lcdc_s6e63m0_spi_write(spi, LCDC_S6E63M0_TEMP_SWIRE_CMD, temp_swire_set_default_value, sizeof(temp_swire_set_default_value)/sizeof(uint8_t));
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: LCDC_S6E63M0_TEMP_SWIRE_CMD err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_elvss_control_set_err_write;
	}

	ret = lcdc_s6e63m0_spi_write(spi, LCDC_S6E63M0_ELVSS_CON_CMD, elvss_con_set_default_value, sizeof(elvss_con_set_default_value)/sizeof(uint8_t));
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: LCDC_S6E63M0_ELVSS_CON_CMD err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_elvss_control_set_err_write;
	}

lcdc_s6e63m0_elvss_control_set_err_write:
	LCDC_LOGD(KERN_DEBUG "[LCDC] %s-\n", __func__);
	return ret;
}
#endif

static int lcdc_s6e63m0_init_config(struct lcdc_s6e63m0_driver_data_t *driver_data)
{
	int ret = 0;

	LCDC_LOGD(KERN_DEBUG "[LCDC] %s+\n", __func__);

	#if ENABLE_CTL_PWR
	ret = lcdc_s6e63m0_power_on();
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: lcdc_s6e63m0_power_on err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_init_config_err_powr_on;
	}
	#endif

	if (driver_data->first_panel_on)
	{
		driver_data->first_panel_on = 0;
	}
	else
	{
		ret = lcdc_s6e63m0_reset();
		if (ret)
		{
			LCDC_LOGE(KERN_ERR "[LCDC] %s:: lcdc_s6e63m0_reset err ret=%d\n", __func__, ret);
			goto lcdc_s6e63m0_init_config_err_reset;
		}
	}
	#if !ENABLE_SPI_DELAY
	ret = lcdc_s6e63m0_panel_condition_set(driver_data->spi);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: lcdc_s6e63m0_panel_condition_set err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_init_config_err_panel_condition_set;
	}

	ret = lcdc_s6e63m0_display_condition_set(driver_data->spi);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: lcdc_s6e63m0_display_condition_set err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_init_config_err_display_condition_set;
	}

	ret = lcdc_s6e63m0_gamma_table_set(driver_data->spi);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: lcdc_s6e63m0_gamma_table_set err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_init_config_err_gamma_table_set;
	}

	ret = lcdc_s6e63m0_etc_condition_set(driver_data->spi);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: lcdc_s6e63m0_etc_condition_set err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_init_config_err_etc_condition_set;
	}

	#if ENABLE_ELVSS_CTL
	ret = lcdc_s6e63m0_elvss_control_set(driver_data->spi);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: lcdc_s6e63m0_elvss_control_set err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_init_config_err_elvss_control_set;
	}
	#endif

	#if ENABLE_ACL_CTL
	ret = lcdc_s6e63m0_acl_control_set(driver_data->spi);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: lcdc_s6e63m0_acl_control_set err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_init_config_err_acl_control_set;
	}
	#endif

	ret = lcdc_s6e63m0_standby_off(driver_data->spi);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: lcdc_s6e63m0_standby_off err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_init_config_err_standby_off;
	}

	ret = lcdc_s6e63m0_display_on(driver_data->spi);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: lcdc_s6e63m0_display_on err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_init_config_err_display_on;
	}
	#endif

	LCDC_LOGD(KERN_DEBUG "[LCDC] %s-\n", __func__);
	return ret;

#if !ENABLE_SPI_DELAY
lcdc_s6e63m0_init_config_err_display_on:
	lcdc_s6e63m0_standby_on(driver_data->spi);
lcdc_s6e63m0_init_config_err_standby_off:
#if ENABLE_ACL_CTL
lcdc_s6e63m0_init_config_err_acl_control_set:
#endif
#if ENABLE_ELVSS_CTL
lcdc_s6e63m0_init_config_err_elvss_control_set:
#endif
lcdc_s6e63m0_init_config_err_etc_condition_set:
lcdc_s6e63m0_init_config_err_gamma_table_set:
lcdc_s6e63m0_init_config_err_display_condition_set:
lcdc_s6e63m0_init_config_err_panel_condition_set:
#endif
lcdc_s6e63m0_init_config_err_reset:
#if ENABLE_CTL_PWR
	lcdc_s6e63m0_power_off();
lcdc_s6e63m0_init_config_err_powr_on:
#endif
	LCDC_LOGD(KERN_DEBUG "[LCDC] %s-\n", __func__);
	return ret;
}

int lcdc_s6e63m0_os_engineer_mode_test(void __user *p)
{
	int ret = 0;
	struct lcdc_s6e63m0_ioctl_cmd_t ioctl_cmd;

	LCDC_LOGD(KERN_DEBUG "[LCDC] %s+\n", __func__);
	ret = copy_from_user(&ioctl_cmd, p, sizeof(struct lcdc_s6e63m0_ioctl_cmd_t));
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: copy_from_user err ret=%d\n", __func__, ret);
		return ret;
	}

	if (ioctl_cmd.num_of_param > MAX_PARAM)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: ioctl_cmd.num_of_param > MAX_PARAM\n", __func__);
		return -EINVAL;
	}

	ret = lcdc_s6e63m0_spi_write(lcdc_s6e63m0_driver_data.spi, ioctl_cmd.command, ioctl_cmd.param, ioctl_cmd.num_of_param);
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: lcdc_s6e63m0_spi_write err ret=%d\n", __func__, ret);
		return ret;
	}

	LCDC_LOGD(KERN_DEBUG "[LCDC] %s-\n", __func__);
	return ret;
}

#if ENABLE_ESD
static void lcdc_s6e63m0_esd_recover(struct lcdc_s6e63m0_driver_data_t *driver_data)
{
	int ret = 0;

	LCDC_LOGD(KERN_DEBUG "[LCDC] %s+\n", __func__);

	ret = lcdc_s6e63m0_reset();
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: lcdc_s6e63m0_reset err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_esd_recover_err_reset;
	}

	#if ENABLE_SPI_DELAY
	queue_delayed_work(driver_data->spi_workqueue, &driver_data->spi_delayed_work, HZ / 4);
	#else
	ret = lcdc_s6e63m0_panel_condition_set(driver_data->spi);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: lcdc_s6e63m0_panel_condition_set err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_esd_recover_err_panel_condition_set;
	}

	ret = lcdc_s6e63m0_display_condition_set(driver_data->spi);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: lcdc_s6e63m0_display_condition_set err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_esd_recover_err_display_condition_set;
	}

	ret = lcdc_s6e63m0_gamma_table_set(driver_data->spi);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: lcdc_s6e63m0_gamma_table_set err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_esd_recover_err_gamma_table_set;
	}

	ret = lcdc_s6e63m0_etc_condition_set(driver_data->spi);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: lcdc_s6e63m0_etc_condition_set err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_esd_recover_err_etc_condition_set;
	}

	#if ENABLE_ELVSS_CTL
	ret = lcdc_s6e63m0_elvss_control_set(driver_data->spi);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: lcdc_s6e63m0_elvss_control_set err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_esd_recover_err_elvss_control_set;
	}
	#endif

	#if ENABLE_ACL_CTL
	ret = lcdc_s6e63m0_acl_control_set(driver_data->spi);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: lcdc_s6e63m0_acl_control_set err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_esd_recover_err_acl_control_set;
	}
	#endif

	ret = lcdc_s6e63m0_standby_off(driver_data->spi);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: lcdc_s6e63m0_standby_off err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_esd_recover_err_standby_off;
	}

	ret = lcdc_s6e63m0_display_on(driver_data->spi);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: lcdc_s6e63m0_display_on err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_esd_recover_err_display_on;
	}

	LCDC_LOGD(KERN_DEBUG "[LCDC] %s-\n", __func__);
	return;

lcdc_s6e63m0_esd_recover_err_display_on:
	lcdc_s6e63m0_standby_on(driver_data->spi);
lcdc_s6e63m0_esd_recover_err_standby_off:
#if ENABLE_ACL_CTL
lcdc_s6e63m0_esd_recover_err_acl_control_set:
#endif
#if ENABLE_ELVSS_CTL
lcdc_s6e63m0_esd_recover_err_elvss_control_set:
#endif
lcdc_s6e63m0_esd_recover_err_etc_condition_set:
lcdc_s6e63m0_esd_recover_err_gamma_table_set:
lcdc_s6e63m0_esd_recover_err_display_condition_set:
lcdc_s6e63m0_esd_recover_err_panel_condition_set:
	#if ENABLE_CTL_PWR
	lcdc_s6e63m0_power_off();
	#endif
	#endif
lcdc_s6e63m0_esd_recover_err_reset:
	LCDC_LOGD(KERN_DEBUG "[LCDC] %s-\n", __func__);
	return;
}

static void lcdc_s6e63m0_esd_workqueue_handler(struct work_struct *work)
{
	uint8_t tx_buf;
	int ret = 0;
	struct lcdc_s6e63m0_driver_data_t *driver_data = container_of(work, struct lcdc_s6e63m0_driver_data_t, esd_delayed_work.work);

	LCDC_LOGD(KERN_DEBUG "[LCDC] %s+\n", __func__);

	ret = lcdc_s6e63m0_spi_read(driver_data->spi, 0x0A, &tx_buf);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: lcdc_s6e63m0_spi_write err ret=%d\n", __func__, ret);
	}
	if (tx_buf != 0x9c)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: 0x0A resp=%d err\n", __func__, tx_buf);
		lcdc_s6e63m0_esd_recover(driver_data);
		goto lcdc_s6e63m0_esd_workqueue_handler_err_resp;
	}

	ret = lcdc_s6e63m0_spi_read(driver_data->spi, 0x0C, &tx_buf);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: lcdc_s6e63m0_spi_write err ret=%d\n", __func__, ret);
	}
	if (tx_buf != 0x77)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: 0x0C resp=%d err\n", __func__, tx_buf);
		lcdc_s6e63m0_esd_recover(driver_data);
		goto lcdc_s6e63m0_esd_workqueue_handler_err_resp;
	}

lcdc_s6e63m0_esd_workqueue_handler_err_resp:
	queue_delayed_work(driver_data->esd_workqueue, &driver_data->esd_delayed_work, 10 * HZ);
	LCDC_LOGD(KERN_DEBUG "[LCDC] %s-\n", __func__);
	return;
}
#endif

#if ENABLE_SPI_DELAY
static void lcdc_s6e63m0_spi_delay_workqueue_handler(struct work_struct *work)
{
	int ret = 0;
	struct lcdc_s6e63m0_driver_data_t *driver_data = container_of(work, struct lcdc_s6e63m0_driver_data_t, spi_delayed_work.work);

	printk("[LCDC] %s+\n", __func__);

	ret = lcdc_s6e63m0_panel_condition_set(driver_data->spi);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: lcdc_s6e63m0_panel_condition_set err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_spi_delay_workqueue_handler_err_panel_condition_set;
	}

	ret = lcdc_s6e63m0_display_condition_set(driver_data->spi);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: lcdc_s6e63m0_display_condition_set err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_spi_delay_workqueue_handler_err_display_condition_set;
	}

	ret = lcdc_s6e63m0_gamma_table_set(driver_data->spi);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: lcdc_s6e63m0_gamma_table_set err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_spi_delay_workqueue_handler_err_gamma_table_set;
	}

	ret = lcdc_s6e63m0_etc_condition_set(driver_data->spi);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: lcdc_s6e63m0_etc_condition_set err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_spi_delay_workqueue_handler_err_etc_condition_set;
	}

	#if ENABLE_ELVSS_CTL
	ret = lcdc_s6e63m0_elvss_control_set(driver_data->spi);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: lcdc_s6e63m0_elvss_control_set err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_spi_delay_workqueue_handler_err_elvss_control_set;
	}
	#endif

	#if ENABLE_ACL_CTL
	ret = lcdc_s6e63m0_acl_control_set(driver_data->spi);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: lcdc_s6e63m0_acl_control_set err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_spi_delay_workqueue_handler_err_acl_control_set;
	}
	#endif

	ret = lcdc_s6e63m0_standby_off(driver_data->spi);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: lcdc_s6e63m0_standby_off err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_spi_delay_workqueue_handler_err_standby_off;
	}

	ret = lcdc_s6e63m0_display_on(driver_data->spi);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: lcdc_s6e63m0_display_on err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_spi_delay_workqueue_handler_err_display_on;
	}

	printk("[LCDC] %s-\n", __func__);
	return;

lcdc_s6e63m0_spi_delay_workqueue_handler_err_display_on:
	lcdc_s6e63m0_standby_on(driver_data->spi);
lcdc_s6e63m0_spi_delay_workqueue_handler_err_standby_off:
#if ENABLE_ACL_CTL
lcdc_s6e63m0_spi_delay_workqueue_handler_err_acl_control_set:
#endif
#if ENABLE_ELVSS_CTL
lcdc_s6e63m0_spi_delay_workqueue_handler_err_elvss_control_set:
#endif
lcdc_s6e63m0_spi_delay_workqueue_handler_err_etc_condition_set:
lcdc_s6e63m0_spi_delay_workqueue_handler_err_gamma_table_set:
lcdc_s6e63m0_spi_delay_workqueue_handler_err_display_condition_set:
lcdc_s6e63m0_spi_delay_workqueue_handler_err_panel_condition_set:
#if ENABLE_CTL_PWR
	lcdc_s6e63m0_power_off();
#endif
	return;
}
#endif

static int __devinit lcdc_s6e63m0_spi_probe(struct spi_device *spi)
{
	int ret = 0;
	struct msm_panel_info *pinfo;

	LCDC_LOGD(KERN_DEBUG "[LCDC] %s+\n", __func__);

	lcdc_s6e63m0_driver_data.spi = spi;
	lcdc_s6e63m0_driver_data.curr_brightness_gamma_table_index = LCDC_S6E63M0_GAMMA_TABLE_MAX - 1;

	lcdc_s6e63m0_driver_data.auto_backlight = 0;
	lcdc_s6e63m0_driver_data.first_panel_on = 1;
	atomic_set(&lcdc_s6e63m0_driver_data.power_flag, 0);

	#if 0
	ret = lcdc_s6e63m0_init_config(&lcdc_s6e63m0_driver_data);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: lcdc_s6e63m0_init_config err ret=%d\n", __func__, ret);
		return ret;
	}
	#endif

	#if !ENABLE_CTL_PWR
	ret = lcdc_s6e63m0_power_on();
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: lcdc_s6e63m0_power_on err ret=%d\n", __func__, ret);
		return ret;
	}
	#endif

	spi_set_drvdata(spi, &lcdc_s6e63m0_driver_data);

	ret = platform_driver_register(&lcdc_s6e63m0_driver);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: platform_driver_register err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_spi_probe_err_reg_dri;
	}

	pinfo = &lcdc_s6e63m0_panel_data.panel_info;
	pinfo->xres = 480;
	pinfo->yres = 800;
	pinfo->type = LCDC_PANEL;
	pinfo->pdest = DISPLAY_1;
	pinfo->wait_cycle = 0;
	pinfo->bpp = 24;
	pinfo->fb_num = 2;
	pinfo->clk_rate = 27027000;
	pinfo->bl_max = LCDC_S6E63M0_GAMMA_TABLE_MAX - 1;
	pinfo->bl_min = 0;


	pinfo->lcdc.h_back_porch = LCDC_S6E63M0_HBP;
	pinfo->lcdc.h_front_porch = LCDC_S6E63M0_HFP;
	pinfo->lcdc.h_pulse_width = LCDC_S6E63M0_HPW;
	pinfo->lcdc.v_back_porch = LCDC_S6E63M0_VBP;
	pinfo->lcdc.v_front_porch = LCDC_S6E63M0_VFP;
	pinfo->lcdc.v_pulse_width = LCDC_S6E63M0_VPW;
	pinfo->lcdc.border_clr = 0;
	pinfo->lcdc.underflow_clr = 0;
	pinfo->lcdc.hsync_skew = 0;

	ret = platform_device_register(&lcdc_s6e63m0_device);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: platform_device_register err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_spi_probe_err_reg_dev;
	}

	lcdc_s6e63m0_driver_data.device = &lcdc_s6e63m0_device;

	LCDC_LOGD(KERN_DEBUG "[LCDC] %s-\n", __func__);
	return ret;

lcdc_s6e63m0_spi_probe_err_reg_dev:
	platform_driver_unregister(&lcdc_s6e63m0_driver);
lcdc_s6e63m0_spi_probe_err_reg_dri:
	spi_set_drvdata(spi, NULL);

	#if !ENABLE_CTL_PWR
	lcdc_s6e63m0_power_off();
	#endif

	LCDC_LOGD(KERN_DEBUG "[LCDC] %s-\n", __func__);
	return ret;
}

static int __devexit lcdc_s6e63m0_spi_remove(struct spi_device *spi)
{
	int ret = 0;
	LCDC_LOGD(KERN_DEBUG "[LCDC] %s+\n", __func__);

	platform_device_unregister(&lcdc_s6e63m0_device);
	platform_driver_unregister(&lcdc_s6e63m0_driver);
	spi_set_drvdata(spi, NULL);

	#if !ENABLE_CTL_PWR
	lcdc_s6e63m0_power_off();
	#endif

	LCDC_LOGD(KERN_DEBUG "[LCDC] %s-\n", __func__);
	return ret;
}

#ifdef CONFIG_PM
static int lcdc_s6e63m0_spi_suspend(struct spi_device *spi, pm_message_t mesg)
{
	LCDC_LOGD(KERN_DEBUG "[LCDC] %s+\n", __func__);
	LCDC_LOGD(KERN_DEBUG "[LCDC] %s-\n", __func__);
	return 0;
}

static int lcdc_s6e63m0_spi_resume(struct spi_device *spi)
{
	LCDC_LOGD(KERN_DEBUG "[LCDC] %s+\n", __func__);
	LCDC_LOGD(KERN_DEBUG "[LCDC] %s-\n", __func__);
	return 0;
}
#endif /* CONFIG_PM */

static int __ref lcdc_s6e63m0_probe(struct platform_device *pdev)
{
	int ret = 0;

	LCDC_LOGD(KERN_DEBUG "[LCDC] %s+\n", __func__);

	ret = lcdc_s6e63m0_gpio_setup();
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: lcdc_s6e63m0_gpio_setup err ret=%d\n", __func__, ret);
		return ret;
	}

	#if ENABLE_ESD
	lcdc_s6e63m0_driver_data.esd_workqueue = create_singlethread_workqueue("lcdc_s6e63m0_esd_wq");
	INIT_DELAYED_WORK(&lcdc_s6e63m0_driver_data.esd_delayed_work, lcdc_s6e63m0_esd_workqueue_handler);
	#endif

	#if ENABLE_SPI_DELAY
	lcdc_s6e63m0_driver_data.spi_workqueue = create_singlethread_workqueue("lcdc_s6e63m0_spi_wq");
	INIT_DELAYED_WORK(&lcdc_s6e63m0_driver_data.spi_delayed_work, lcdc_s6e63m0_spi_delay_workqueue_handler);
	#endif

	msm_fb_add_device(pdev);

#if	LONGCHARGER48HR_WAKEUP_HANG_ATLCD_WORKAROUND
	wake_lock_init(&lcd_idlelock, WAKE_LOCK_IDLE, "lcd_idleactive");
	wake_lock_init(&lcd_suspendlock, WAKE_LOCK_SUSPEND, "lcd_suspendactive");
#endif

	LCDC_LOGD(KERN_DEBUG "[LCDC] %s-\n", __func__);

	return ret;
}

static int lcdc_s6e63m0_panel_on(struct platform_device *pdev)
{
	int ret = 0;
	printk("[LCDC] %s+\n", __func__);

#if	LONGCHARGER48HR_WAKEUP_HANG_ATLCD_WORKAROUND
	if (lcd_can_wakelock==0)
	{
		lcd_can_wakelock=1;
	}
	else
	{
		wake_lock(&lcd_idlelock);
		wake_lock(&lcd_suspendlock);
		printk(KERN_ERR "[SEAN]lcd_lock\n");
	}
#endif

	ret = lcdc_s6e63m0_init_config(&lcdc_s6e63m0_driver_data);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: lcdc_s6e63m0_init_config err ret=%d\n", __func__, ret);
		return ret;
	}

	#if ENABLE_ESD
	queue_delayed_work(lcdc_s6e63m0_driver_data.esd_workqueue, &lcdc_s6e63m0_driver_data.esd_delayed_work, 10 * HZ);
	#endif

	#if ENABLE_SPI_DELAY
	queue_delayed_work(lcdc_s6e63m0_driver_data.spi_workqueue, &lcdc_s6e63m0_driver_data.spi_delayed_work, HZ / 4);
	#endif

	printk("[LCDC] %s-\n", __func__);
	return ret;
}

static int lcdc_s6e63m0_panel_off(struct platform_device *pdev)
{
	int ret = 0;
	struct lcdc_s6e63m0_driver_data_t *driver_data_p = &lcdc_s6e63m0_driver_data;

	printk("[LCDC] %s+\n", __func__);

	#if ENABLE_ESD
  	cancel_delayed_work(&driver_data_p->esd_delayed_work);
  	flush_workqueue(driver_data_p->esd_workqueue);
	#endif

	#if ENABLE_SPI_DELAY
  	cancel_delayed_work(&driver_data_p->spi_delayed_work);
  	flush_workqueue(driver_data_p->spi_workqueue);
	#endif

	#if 0
	ret = lcdc_s6e63m0_display_off(driver_data_p->spi);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: lcdc_s6e63m0_display_off err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_panel_off_err_display_off;
	}
	#endif

	ret = lcdc_s6e63m0_standby_on(driver_data_p->spi);
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: lcdc_s6e63m0_standby_on err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_panel_off_err_standby_on;
	}

	#if 0
	ret = lcdc_s6e63m0_reset();
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: lcdc_s6e63m0_reset err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_panel_off_err_reset;
	}
	#endif

	#if ENABLE_CTL_PWR
	ret = lcdc_s6e63m0_power_off();
	if (ret)
	{
		LCDC_LOGE(KERN_ERR "[LCDC] %s:: lcdc_s6e63m0_power_off err ret=%d\n", __func__, ret);
		goto lcdc_s6e63m0_panel_off_err_powr_off;
	}
	#endif

	if ((system_rev <= TOUCAN_EVT1_Band148) || system_rev == HWID_UNKNOWN)
	{
		gpio_set_value(LCDC_S6E63M0_RESET_PIN, 0);
	}
	else
	{
		gpio_set_value(LCDC_S6E63M0_RESET_PIN_EVT2, 0);
	}

#if ENABLE_CTL_PWR
lcdc_s6e63m0_panel_off_err_powr_off:
#endif
#if 0
lcdc_s6e63m0_panel_off_err_reset:
#endif
lcdc_s6e63m0_panel_off_err_standby_on:
#if 0
lcdc_s6e63m0_panel_off_err_display_off:
#endif

#if	LONGCHARGER48HR_WAKEUP_HANG_ATLCD_WORKAROUND
	wake_unlock(&lcd_idlelock);
	wake_unlock(&lcd_suspendlock);
	printk(KERN_ERR "[SEAN]lcd_unlock\n");
#endif
	
	printk("[LCDC] %s-\n", __func__);
	return ret;
}

void lcdc_lsensor_set_backlight(int auto_mode, int level)
{
	struct lcdc_s6e63m0_driver_data_t *driver_data_p = &lcdc_s6e63m0_driver_data;

	level += 3;

	if(auto_mode)
	{
		driver_data_p->auto_backlight = 1;
	}
	else
	{
		driver_data_p->auto_backlight = 0;
		level = driver_data_p->user_backlight;
	}
	if (level < 0)
		level = 0;
	else if (level >= LCDC_S6E63M0_GAMMA_TABLE_MAX)
		level = LCDC_S6E63M0_GAMMA_TABLE_MAX - 1;

	if (driver_data_p->curr_brightness_gamma_table_index <= 3 && level <= 3)
		return;

	driver_data_p->curr_brightness_gamma_table_index = level;
	lcdc_s6e63m0_gamma_table_set(driver_data_p->spi);
}
EXPORT_SYMBOL(lcdc_lsensor_set_backlight);

static void lcdc_s6e63m0_set_backlight(struct msm_fb_data_type *mfd)
{

	struct lcdc_s6e63m0_driver_data_t *driver_data_p = &lcdc_s6e63m0_driver_data;

	LCDC_LOGD(KERN_DEBUG "[LCDC] %s+\n", __func__);

	driver_data_p->user_backlight = mfd->bl_level;
	if (mfd->bl_level == driver_data_p->curr_brightness_gamma_table_index)
		return;

	if (driver_data_p->curr_brightness_gamma_table_index <= 3 && mfd->bl_level <= 3)
		return;

	driver_data_p->curr_brightness_gamma_table_index = mfd->bl_level;

	if (driver_data_p->curr_brightness_gamma_table_index < 0)
		driver_data_p->curr_brightness_gamma_table_index = 0;
	else if (driver_data_p->curr_brightness_gamma_table_index >= LCDC_S6E63M0_GAMMA_TABLE_MAX)
		driver_data_p->curr_brightness_gamma_table_index = LCDC_S6E63M0_GAMMA_TABLE_MAX - 1;

	lcdc_s6e63m0_gamma_table_set(driver_data_p->spi);

	LCDC_LOGD(KERN_DEBUG "[LCDC] %s-\n", __func__);
	return;
}

static int __ref lcdc_s6e63m0_init(void)
{
	int ret = 0;

	printk("BootLog, +%s+\n", __func__);

	ret = spi_register_driver(&lcdc_s6e63m0_spi_driver);
	printk("BootLog, -%s-, ret=%d\n", __func__, ret);
	return ret;
}

static void __exit lcdc_s6e63m0_exit(void)
{
	LCDC_LOGD(KERN_DEBUG "[LCDC] %s+\n", __func__);
	spi_unregister_driver(&lcdc_s6e63m0_spi_driver);
	LCDC_LOGD(KERN_DEBUG "[LCDC] %s-\n", __func__);
}

module_init(lcdc_s6e63m0_init);
module_exit(lcdc_s6e63m0_exit);

MODULE_LICENSE("GPL v2");
MODULE_ALIAS("s6e63m0");
MODULE_DESCRIPTION("Samsung S6E63M0 LCDC Driver");

