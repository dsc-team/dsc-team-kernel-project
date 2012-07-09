/* Copyright (c) 2008-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include <linux/kernel.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/android_pmem.h>
#include <linux/bootmem.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include <linux/mfd/tps65023.h>
#include <linux/bma150.h>
#include <linux/power_supply.h>
#include <linux/clk.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/io.h>
#include <asm/setup.h>

#include <mach/smem_pc_oem_cmd.h>
#include <mach/custmproc.h>
#include <asm/mach/mmc.h>
#include <mach/vreg.h>
#include <mach/mpp.h>
#include <mach/gpio.h>
#include <mach/board.h>
#include <mach/sirc.h>
#include <mach/dma.h>
#include <mach/rpc_hsusb.h>
#include <mach/rpc_pmapp.h>
#include <mach/msm_hsusb.h>
#include <mach/msm_serial_hs.h>
#include <mach/msm_touchpad.h>
#ifdef CONFIG_TOUCHSCREEN_ATMEL_mXT224
#include <mach/atmel_mXT224_touch.h>
#endif
#ifdef CONFIG_MUTEKEY
#include <mach/mutekey.h>
#endif
#include <mach/msm_i2ckbd.h>
#include <mach/pmic.h>
#include <mach/camera.h>
#include <mach/memory.h>
#include <mach/msm_spi.h>
#include <mach/msm_tsif.h>
#include <mach/austin_hwid.h>
#include <mach/rpc_server_handset.h>

#include "devices.h"
#include "timer.h"
#include "socinfo.h"
#include "msm-keypad-devices.h"
#include "pm.h"

#ifdef CONFIG_BATTERY_QSD
#include <mach/qsd_battery.h>
#endif

#ifdef CONFIG_LIGHT_SENSOR_QSD
#include <mach/lsensor.h>
#endif

#ifdef  CONFIG_BCOM_FM
#include <mach/bcom_fm.h>
#define         FM_WAKEUP               77
#endif

#ifdef CONFIG_ECOMPASS_AKM_AK8973S
#include <mach/ecompass.h>
#endif

#include "proc_comm.h"
#include "smd_private.h"
#include <linux/msm_kgsl.h>
#ifdef CONFIG_USB_ANDROID
#include <linux/usb/android_composite.h>
#endif

#define MSM_PMEM_SF_SIZE	0x1C91000

#define SMEM_SPINLOCK_I2C	"S:6"

#define MSM_PMEM_ADSP_SIZE	0x3A00000
#define MSM_FB_SIZE         0x2EE000

#define MSM_AUDIO_SIZE		0x80000
#define MSM_GPU_PHYS_SIZE 	SZ_2M

#ifdef CONFIG_HDMI_BUGFIX
#define MSM_FBNEW_SIZE	800*480*8
#endif

#ifdef CONFIG_MSM_SOC_REV_A
#define MSM_SMI_BASE		0xE0000000
#else
#define MSM_SMI_BASE		0x00000000
#endif

#define MSM_SHARED_RAM_PHYS	(MSM_SMI_BASE + 0x00100000)

#define MSM_PMEM_SMI_BASE	(MSM_SMI_BASE + 0x02B00000)
#define MSM_PMEM_SMI_SIZE	0x01500000

#define MSM_FB_BASE		MSM_PMEM_SMI_BASE
#define MSM_GPU_PHYS_BASE 	(MSM_FB_BASE + MSM_FB_SIZE)
#define MSM_PMEM_SMIPOOL_BASE	(MSM_GPU_PHYS_BASE + MSM_GPU_PHYS_SIZE)
#define MSM_PMEM_SMIPOOL_SIZE	(MSM_PMEM_SMI_SIZE - MSM_FB_SIZE \
					- MSM_GPU_PHYS_SIZE)

#define PMEM_KERNEL_EBI1_SIZE	0x28000

#define PMIC_VREG_WLAN_LEVEL	2600
#define PMIC_VREG_GP6_LEVEL	2900

#define FPGA_SDCC_STATUS	0x70000280

#ifdef CONFIG_GSENSOR_BOSCH_BMA150
#define GSENSOR_INT_GPIO_NUM 23
#define GSENSOR_I2C_ADDR 0x38
#endif
#define SLOT3_CARD_DETECT_GPIO 97
#ifdef CONFIG_ECOMPASS_AKM_AK8973S
#if defined(CONFIG_MACH_EVB)
#define ECOMPASS_RST_GPIO_NUM 112
#define ECOMPASS_INT_GPIO_NUM 111
#else
#define ECOMPASS_RST_GPIO_NUM 144
#define ECOMPASS_INT_GPIO_NUM 146
#endif
#define ECOMPASS_I2C_ADDR 0x1C
#endif

#ifdef CONFIG_TOUCHSCREEN_ATMEL_mXT224
#define TOUCHSCREEN_RST_GPIO_NUM 43
#define TOUCHSCREEN_INT_GPIO_NUM 44
#define TOUCHSCREEN_I2C_ADDR 0x4A
#endif
#ifdef CONFIG_MUTEKEY
#define Mutekey_GPIO_NUM 39
#endif

#define NV_UE_IMEI_SIZE     9
#define SERIAL_NO_SIZE      16
#define DRM_KEY_SIZE        400
#define BT_MAC_ADDR_SIZE    12
#define WLAN_MAC_ADDR_SIZE  12

typedef struct
{
   uint8_t    imei[NV_UE_IMEI_SIZE];
   uint8_t    serial_no[SERIAL_NO_SIZE];
   uint8_t    drm_key[DRM_KEY_SIZE];
   uint8_t    bt_mac_addr[BT_MAC_ADDR_SIZE];
   uint8_t    wlan_mac_addr[WLAN_MAC_ADDR_SIZE];
} device_otp_data_s_from_dt_or_diag;

char qisda_imei_serialnum[17] = "1234567890ABCDEF";
int adb_debugging = 0;
int qct_usbdiag_debugging = 0;

int q_wlan_flag = 0;
EXPORT_SYMBOL(q_wlan_flag);

#ifdef CONFIG_WLAN_ALLOC_STATIC_MEM
    #define ADDR_4329_DHD_PORT_LEN		SZ_16K
    #define ADDR_4329_BUS_RXBUF_LEN		SZ_16K
    #define ADDR_4329_BUS_DATABUF_LEN	SZ_32K
    void *addr_4329_dhd_prot,*addr_4329_bus_rxbuf,*addr_4329_bus_databuf;
    EXPORT_SYMBOL(addr_4329_dhd_prot);
    EXPORT_SYMBOL(addr_4329_bus_rxbuf);
    EXPORT_SYMBOL(addr_4329_bus_databuf);
#endif

#if 0
static struct resource smc91x_resources[] = {
	[0] = {
		.flags  = IORESOURCE_MEM,
	},
	[1] = {
		.flags  = IORESOURCE_IRQ,
	},
};
#endif

#ifdef CONFIG_USB_FUNCTION
static struct usb_mass_storage_platform_data usb_mass_storage_pdata = {
	.nluns          = 0x02,
	.buf_size       = 16384,
	.vendor         = "GOOGLE",
	.product        = "Mass storage",
	.release        = 0xffff,
};

static struct platform_device mass_storage_device = {
	.name           = "usb_mass_storage",
	.id             = -1,
	.dev            = {
		.platform_data          = &usb_mass_storage_pdata,
	},
};
#endif

#ifdef CONFIG_USB_ANDROID

static char *usb_functions_default_adb[] = {
	"adb",
	"usb_mass_storage",
};

static char *usb_functions_diag_adb[] = {
	"adb",
	"diag",
	"modem",
	"usb_mass_storage",
};

static char *usb_functions_rndis[] = {
	"rndis",
};

static char *usb_functions_rndis_adb_diag[] = {
	"rndis",
	"adb",
	"diag",
};

static char *usb_functions_all[] = {
#ifdef CONFIG_USB_ANDROID_RNDIS
	"rndis",
#endif
	"usb_mass_storage",
	"adb",
#ifdef CONFIG_USB_ANDROID_DIAG
	"diag",
#endif

#ifdef CONFIG_USB_F_SERIAL
	"modem",
	"nmea",
#endif
#ifdef CONFIG_USB_ANDROID_RMNET
	"rmnet",
#endif

#ifdef CONFIG_USB_ANDROID_ACM
	"acm",
#endif
};

static struct android_usb_product usb_products[] = {

	{
		.product_id	= 0xB103,
		.num_functions	= ARRAY_SIZE(usb_functions_default_adb),
		.functions	= usb_functions_default_adb,
	},
	{
		.product_id	= 0xB103,
		.num_functions	= ARRAY_SIZE(usb_functions_diag_adb),
		.functions	= usb_functions_diag_adb,
	},
	{
		.product_id	= 0xB101,
		.num_functions	= ARRAY_SIZE(usb_functions_rndis),
		.functions	= usb_functions_rndis,
	},
	{
		.product_id	= 0x0414,
		.num_functions	= ARRAY_SIZE(usb_functions_rndis_adb_diag),
		.functions	= usb_functions_rndis_adb_diag,
	},
};

static struct usb_mass_storage_platform_data mass_storage_pdata = {
	.nluns		= 1,
	.vendor		= "Dell",
	.product        = "Mass storage",
	.release	= 0x0100,
};

static struct platform_device usb_mass_storage_device = {
	.name	= "usb_mass_storage",
	.id	= -1,
	.dev	= {
		.platform_data = &mass_storage_pdata,
	},
};

static struct usb_ether_platform_data rndis_pdata = {
	.vendorID	= 0x05C6,
	.vendorDescr	= "Qualcomm Incorporated",
};

static struct platform_device rndis_device = {
	.name	= "rndis",
	.id	= -1,
	.dev	= {
		.platform_data = &rndis_pdata,
	},
};

static struct android_usb_platform_data android_usb_pdata = {
	.vendor_id	= 0x413C,
	.product_id	= 0xB103,
	.version	= 0x0100,
	.product_name		= "Dell Venue",
	.manufacturer_name	= "Dell Incorporated",
	.num_products = ARRAY_SIZE(usb_products),
	.products = usb_products,
	.num_functions = ARRAY_SIZE(usb_functions_all),
	.functions = usb_functions_all,
	.serial_number = "1234567890ABCDEF",
};

static struct platform_device android_usb_device = {
	.name	= "android_usb",
	.id		= -1,
	.dev		= {
		.platform_data = &android_usb_pdata,
	},
};

static int __init board_serialno_setup(char *serialno)
{
	int i;
	char *src = serialno;

	rndis_pdata.ethaddr[0] = 0x02;
	for (i = 0; *src; i++) {
		rndis_pdata.ethaddr[i % (ETH_ALEN - 1) + 1] ^= *src++;
	}

	android_usb_pdata.serial_number = serialno;
	return 1;
}
__setup("androidboot.serialno=", board_serialno_setup);
#endif

#if 0
static struct platform_device smc91x_device = {
	.name           = "smc91x",
	.id             = 0,
	.num_resources  = ARRAY_SIZE(smc91x_resources),
	.resource       = smc91x_resources,
};
#endif

#ifdef CONFIG_USB_FUNCTION
static struct usb_function_map usb_functions_map[] = {
	{"diag", 0},
	{"adb", 1},
	{"modem", 2},
	{"nmea", 3},
	{"mass_storage", 4},
	{"ethernet", 5},
};

static struct usb_composition usb_func_composition[] = {
	{
		.product_id         = 0x9012,
		.functions	    = 0x5,
	},

	{
		.product_id         = 0x9013,
		.functions	    = 0x15,
	},

	{
		.product_id         = 0x9014,
		.functions	    = 0x30,
	},

	{
		.product_id         = 0x9015,
		.functions	    = 0x12,
	},

	{
		.product_id         = 0x9016,
		.functions	    = 0xD,
	},

	{
		.product_id         = 0x9017,
		.functions	    = 0x1D,
	},

	{
		.product_id         = 0xF000,
		.functions	    = 0x10,
	},

	{
		.product_id         = 0xF009,
		.functions	    = 0x20,
	},

	{
		.product_id         = 0x9018,
		.functions	    = 0x1F,
	},

	{
		.product_id         = 0x901A,
		.functions	    = 0x0F,
	},
};
#endif

static struct msm_handset_platform_data hs_platform_data = {
	.hs_name = "8k_handset",
	.pwr_key_delay_ms = 500,
};

static struct platform_device hs_device = {
	.name   = "msm-handset",
	.id     = -1,
	.dev    = {
		.platform_data = &hs_platform_data,
	},
};

#ifdef CONFIG_USB_FS_HOST
static struct msm_gpio fsusb_config[] = {
	{ GPIO_CFG(139, 2, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), "fs_dat" },
	{ GPIO_CFG(140, 2, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), "fs_se0" },
	{ GPIO_CFG(141, 3, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), "fs_oe_n" },
};

static int fsusb_gpio_init(void)
{
	return msm_gpios_request(fsusb_config, ARRAY_SIZE(fsusb_config));
}

static void msm_fsusb_setup_gpio(unsigned int enable)
{
	if (enable)
		msm_gpios_enable(fsusb_config, ARRAY_SIZE(fsusb_config));
	else
		msm_gpios_disable(fsusb_config, ARRAY_SIZE(fsusb_config));

}
#endif

#define MSM_USB_BASE              ((unsigned)addr)

static struct msm_hsusb_platform_data msm_hsusb_pdata = {
#ifdef CONFIG_USB_FUNCTION
	.version	= 0x0100,
	.phy_info	= (USB_PHY_INTEGRATED | USB_PHY_MODEL_180NM),
	.vendor_id          = 0x5c6,
	.product_name       = "Qualcomm HSUSB Device",
	.serial_number      = "1234567890ABCDEF",
	.manufacturer_name  = "Qualcomm Incorporated",
	.compositions	= usb_func_composition,
	.num_compositions = ARRAY_SIZE(usb_func_composition),
	.function_map   = usb_functions_map,
	.num_functions	= ARRAY_SIZE(usb_functions_map),
	.config_gpio    = NULL,

#endif
};

static struct vreg *vreg_usb;
static void msm_hsusb_vbus_power(unsigned phy_info, int on)
{

	switch (PHY_TYPE(phy_info)) {
	case USB_PHY_INTEGRATED:
		if (on)
			msm_hsusb_vbus_powerup();
		else
			msm_hsusb_vbus_shutdown();
		break;
	case USB_PHY_SERIAL_PMIC:
		if (on)
			vreg_enable(vreg_usb);
		else
			vreg_disable(vreg_usb);
		break;
	default:
		pr_err("%s: undefined phy type ( %X ) \n", __func__,
						phy_info);
	}

}

static struct msm_usb_host_platform_data msm_usb_host_pdata = {
	.phy_info	= (USB_PHY_INTEGRATED | USB_PHY_MODEL_180NM),
};

#ifdef CONFIG_USB_FS_HOST
static struct msm_usb_host_platform_data msm_usb_host2_pdata = {
	.phy_info	= USB_PHY_SERIAL_PMIC,
	.config_gpio = msm_fsusb_setup_gpio,
	.vbus_power = msm_hsusb_vbus_power,
};
#endif

static struct android_pmem_platform_data android_pmem_kernel_ebi1_pdata = {
	.name = PMEM_KERNEL_EBI1_DATA_NAME,
	.cached = 0,
};

#ifdef CONFIG_KERNEL_PMEM_SMI_REGION

static struct android_pmem_platform_data android_pmem_kernel_smi_pdata = {
	.name = PMEM_KERNEL_SMI_DATA_NAME,
	.cached = 0,
};

#endif

static struct android_pmem_platform_data android_pmem_pdata = {
	.name = "pmem",
	.allocator_type = PMEM_ALLOCATORTYPE_ALLORNOTHING,
	.cached = 1,
};

static struct android_pmem_platform_data android_pmem_adsp_pdata = {
	.name = "pmem_adsp",
	.allocator_type = PMEM_ALLOCATORTYPE_BITMAP,
	.cached = 0,
};

static struct android_pmem_platform_data android_pmem_smipool_pdata = {
	.name = "pmem_smipool",
	.start = MSM_PMEM_SMIPOOL_BASE,
	.size = MSM_PMEM_SMIPOOL_SIZE,
	.allocator_type = PMEM_ALLOCATORTYPE_BITMAP,
	.cached = 0,
};


static struct platform_device android_pmem_device = {
	.name = "android_pmem",
	.id = 0,
	.dev = { .platform_data = &android_pmem_pdata },
};

static struct platform_device android_pmem_adsp_device = {
	.name = "android_pmem",
	.id = 1,
	.dev = { .platform_data = &android_pmem_adsp_pdata },
};

static struct platform_device android_pmem_smipool_device = {
	.name = "android_pmem",
	.id = 2,
	.dev = { .platform_data = &android_pmem_smipool_pdata },
};


static struct platform_device android_pmem_kernel_ebi1_device = {
	.name = "android_pmem",
	.id = 3,
	.dev = { .platform_data = &android_pmem_kernel_ebi1_pdata },
};

#ifdef CONFIG_KERNEL_PMEM_SMI_REGION
static struct platform_device android_pmem_kernel_smi_device = {
	.name = "android_pmem",
	.id = 4,
	.dev = { .platform_data = &android_pmem_kernel_smi_pdata },
};
#endif

static struct resource msm_fb_resources[] = {
	{
		.flags  = IORESOURCE_DMA,
	}
#ifdef CONFIG_HDMI_BUGFIX
	,{
		.flags  = IORESOURCE_DMA,
	}
#endif
};

static int msm_fb_detect_panel(const char *name)
{
	int ret = -EPERM;

	if (machine_is_qsd8x50_ffa() || machine_is_qsd8x50a_ffa()) {
		if (!strncmp(name, "mddi_toshiba_wvga_pt", 20))
			ret = 0;
		else
			ret = -ENODEV;
	} else if ((machine_is_qsd8x50_surf() || machine_is_qsd8x50a_surf())
			&& !strcmp(name, "lcdc_external"))
		ret = 0;

	return ret;
}

static struct msm_fb_platform_data msm_fb_pdata = {
	.detect_client = msm_fb_detect_panel,
};

static struct platform_device msm_fb_device = {
	.name   = "msm_fb",
	.id     = 0,
	.num_resources  = ARRAY_SIZE(msm_fb_resources),
	.resource       = msm_fb_resources,
	.dev    = {
		.platform_data = &msm_fb_pdata,
	}
};

#if 0
static struct msm_gpio bma_spi_gpio_config_data[] = {
	{ GPIO_CFG(22, 0, GPIO_CFG_INPUT,  GPIO_CFG_NO_PULL, GPIO_CFG_2MA), "bma_irq" },
};

static int msm_bma_gpio_setup(struct device *dev)
{
	int rc;

	rc = msm_gpios_request_enable(bma_spi_gpio_config_data,
		ARRAY_SIZE(bma_spi_gpio_config_data));

	return rc;
}

static void msm_bma_gpio_teardown(struct device *dev)
{
	msm_gpios_disable_free(bma_spi_gpio_config_data,
		ARRAY_SIZE(bma_spi_gpio_config_data));
}

static struct bma150_platform_data bma_pdata = {
	.setup    = msm_bma_gpio_setup,
	.teardown = msm_bma_gpio_teardown,
};
#endif

static struct resource qsd_spi_resources[] = {
	{
		.name   = "spi_irq_in",
		.start	= INT_SPI_INPUT,
		.end	= INT_SPI_INPUT,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name   = "spi_irq_out",
		.start	= INT_SPI_OUTPUT,
		.end	= INT_SPI_OUTPUT,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name   = "spi_irq_err",
		.start	= INT_SPI_ERROR,
		.end	= INT_SPI_ERROR,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name   = "spi_base",
		.start	= 0xA1200000,
		.end	= 0xA1200000 + SZ_4K - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name   = "spidm_channels",
		.flags  = IORESOURCE_DMA,
	},
	{
		.name   = "spidm_crci",
		.flags  = IORESOURCE_DMA,
	},
};

static struct platform_device qsd_device_spi = {
	.name	        = "spi_qsd",
	.id	        = 0,
	.num_resources	= ARRAY_SIZE(qsd_spi_resources),
	.resource	= qsd_spi_resources,
};

#if 0
static struct spi_board_info msm_spi_board_info[] __initdata = {
	{
		.modalias	= "bma150",
		.mode		= SPI_MODE_3,
		.irq		= MSM_GPIO_TO_INT(22),
		.bus_num	= 0,
		.chip_select	= 0,
		.max_speed_hz	= 10000000,
		.platform_data	= &bma_pdata,
	},
};
#endif

static struct spi_board_info msm_spi_board_info[] __initdata = {
	{
		.modalias	= "lcdc_s6e63m0",
		.mode		= SPI_MODE_3,
		.bus_num	= 0,
		.chip_select	= 0,
		.max_speed_hz	= 6000000,
	}
};

#define CT_CSR_PHYS		0xA8700000
#define TCSR_SPI_MUX		(ct_csr_base + 0x54)
static int msm_qsd_spi_dma_config(void)
{
	void __iomem *ct_csr_base = 0;
	u32 spi_mux;
	int ret = 0;

	ct_csr_base = ioremap(CT_CSR_PHYS, PAGE_SIZE);
	if (!ct_csr_base) {
		pr_err("%s: Could not remap %x\n", __func__, CT_CSR_PHYS);
		return -1;
	}

	spi_mux = readl(TCSR_SPI_MUX);
	switch (spi_mux) {
	case (1):
		qsd_spi_resources[4].start  = DMOV_HSUART1_RX_CHAN;
		qsd_spi_resources[4].end    = DMOV_HSUART1_TX_CHAN;
		qsd_spi_resources[5].start  = DMOV_HSUART1_RX_CRCI;
		qsd_spi_resources[5].end    = DMOV_HSUART1_TX_CRCI;
		break;
	case (2):
		qsd_spi_resources[4].start  = DMOV_HSUART2_RX_CHAN;
		qsd_spi_resources[4].end    = DMOV_HSUART2_TX_CHAN;
		qsd_spi_resources[5].start  = DMOV_HSUART2_RX_CRCI;
		qsd_spi_resources[5].end    = DMOV_HSUART2_TX_CRCI;
		break;
	case (3):
		qsd_spi_resources[4].start  = DMOV_CE_OUT_CHAN;
		qsd_spi_resources[4].end    = DMOV_CE_IN_CHAN;
		qsd_spi_resources[5].start  = DMOV_CE_OUT_CRCI;
		qsd_spi_resources[5].end    = DMOV_CE_IN_CRCI;
		break;
	default:
		ret = -1;
	}

	iounmap(ct_csr_base);
	return ret;
}

static struct msm_gpio qsd_spi_gpio_config_data[] = {
	{ GPIO_CFG(17, 1, GPIO_CFG_INPUT,  GPIO_CFG_NO_PULL, GPIO_CFG_2MA), "spi_clk" },
	{ GPIO_CFG(18, 1, GPIO_CFG_INPUT,  GPIO_CFG_NO_PULL, GPIO_CFG_2MA), "spi_mosi" },
	{ GPIO_CFG(19, 1, GPIO_CFG_INPUT,  GPIO_CFG_NO_PULL, GPIO_CFG_2MA), "spi_miso" },
	{ GPIO_CFG(20, 1, GPIO_CFG_INPUT,  GPIO_CFG_NO_PULL, GPIO_CFG_2MA), "spi_cs0" },
	{ GPIO_CFG(21, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), "spi_pwr" },
};

static struct msm_gpio qsd_spi_gpio_config_data_evt2[] = {
	{ GPIO_CFG(17, 1, GPIO_CFG_INPUT,  GPIO_CFG_NO_PULL, GPIO_CFG_2MA), "spi_clk" },
	{ GPIO_CFG(18, 1, GPIO_CFG_INPUT,  GPIO_CFG_NO_PULL, GPIO_CFG_2MA), "spi_mosi" },
	{ GPIO_CFG(19, 1, GPIO_CFG_INPUT,  GPIO_CFG_NO_PULL, GPIO_CFG_2MA), "spi_miso" },
	{ GPIO_CFG(20, 1, GPIO_CFG_INPUT,  GPIO_CFG_NO_PULL, GPIO_CFG_2MA), "spi_cs0" },
	{ GPIO_CFG(32, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), "oledbl_en" },

};

static int msm_qsd_spi_gpio_config(void)
{
	int rc;

	if ((system_rev <= TOUCAN_EVT1_Band148) || system_rev == HWID_UNKNOWN)
	{
	rc = msm_gpios_request_enable(qsd_spi_gpio_config_data,
		ARRAY_SIZE(qsd_spi_gpio_config_data));
	if (rc)
		return rc;
	}
	else
	{
		rc = msm_gpios_request_enable(qsd_spi_gpio_config_data_evt2,
			ARRAY_SIZE(qsd_spi_gpio_config_data_evt2));
		if (rc)
			return rc;
	}
	if ((system_rev <= TOUCAN_EVT1_Band148) || system_rev == HWID_UNKNOWN)
	{
		gpio_direction_output(21, 0);

		gpio_direction_output(102, 0);
	}
	else
	{
		gpio_direction_output(32, 0);
	}

	return 0;
}

static void msm_qsd_spi_gpio_release(void)
{
	if ((system_rev <= TOUCAN_EVT1_Band148) || system_rev == HWID_UNKNOWN)
	{
	msm_gpios_disable_free(qsd_spi_gpio_config_data,
		ARRAY_SIZE(qsd_spi_gpio_config_data));
}
	else
	{
		msm_gpios_disable_free(qsd_spi_gpio_config_data_evt2,
			ARRAY_SIZE(qsd_spi_gpio_config_data_evt2));
	}
}

static struct msm_spi_platform_data qsd_spi_pdata = {
	.max_clock_speed = 19200000,
	.clk_name = "spi_clk",
	.gpio_config  = msm_qsd_spi_gpio_config,
	.gpio_release = msm_qsd_spi_gpio_release,
	.dma_config = msm_qsd_spi_dma_config,
};

static void __init msm_qsd_spi_init(void)
{
	qsd_device_spi.dev.platform_data = &qsd_spi_pdata;
}

#if 0
static int mddi_toshiba_pmic_bl(int level)
{
	int ret = -EPERM;

	if (machine_is_qsd8x50_ffa() || machine_is_qsd8x50a_ffa()) {
		ret = pmic_set_led_intensity(LED_LCD, level);

		if (ret)
			printk(KERN_WARNING "%s: can't set lcd backlight!\n",
						__func__);
	}

	return ret;
}

static struct msm_panel_common_pdata mddi_toshiba_pdata = {
	.pmic_backlight = mddi_toshiba_pmic_bl,
};

static struct platform_device mddi_toshiba_device = {
	.name   = "mddi_toshiba",
	.id     = 0,
	.dev    = {
		.platform_data = &mddi_toshiba_pdata,
	}
};

static void msm_fb_vreg_config(const char *name, int on)
{
	struct vreg *vreg;
	int ret = 0;

	vreg = vreg_get(NULL, name);
	if (IS_ERR(vreg)) {
		printk(KERN_ERR "%s: vreg_get(%s) failed (%ld)\n",
		__func__, name, PTR_ERR(vreg));
		return;
	}

	ret = (on) ? vreg_enable(vreg) : vreg_disable(vreg);
	if (ret)
		printk(KERN_ERR "%s: %s(%s) failed!\n",
			__func__, (on) ? "vreg_enable" : "vreg_disable", name);
}

#define MDDI_RST_OUT_GPIO 100
#endif
static void msm_get_wlan_mac_addr(uint8_t *mac_addr, uint8_t length)
{
	int size;
	device_otp_data_s_from_dt_or_diag *otp_data;


	otp_data = (device_otp_data_s_from_dt_or_diag*)
		smem_get_entry(SMEM_OTP_DATA_FROM_DT_OR_DIAG, &size);

        if (otp_data == NULL) {
		printk(KERN_ERR "%s, Cannot get mac address\n", __func__);
	}
	else {
		memcpy(mac_addr, otp_data->wlan_mac_addr, length);
	}
}
EXPORT_SYMBOL(msm_get_wlan_mac_addr);

static int msm_fb_mddi_power_save(int on)
{

#if 0
	int flag_on = !!on;
	int ret = 0;


	if (mddi_power_save_on == flag_on)
		return ret;

	mddi_power_save_on = flag_on;

	if (!flag_on && (machine_is_qsd8x50_ffa()
				|| machine_is_qsd8x50a_ffa())) {
		gpio_set_value(MDDI_RST_OUT_GPIO, 0);
		mdelay(1);
	}

	ret = pmic_lp_mode_control(flag_on ? OFF_CMD : ON_CMD,
		PM_VREG_LP_MSME2_ID);
	if (ret)
		printk(KERN_ERR "%s: pmic_lp_mode_control failed!\n", __func__);

	msm_fb_vreg_config("gp5", flag_on);
	msm_fb_vreg_config("boost", flag_on);

	if (flag_on && (machine_is_qsd8x50_ffa()
			|| machine_is_qsd8x50a_ffa())) {
		gpio_set_value(MDDI_RST_OUT_GPIO, 0);
		mdelay(1);
		gpio_set_value(MDDI_RST_OUT_GPIO, 1);
		gpio_set_value(MDDI_RST_OUT_GPIO, 1);
		mdelay(1);
	}

	return ret;
#else
	return 0;
#endif
}

static int msm_fb_mddi_sel_clk(u32 *clk_rate)
{
	*clk_rate *= 2;
	return 0;
}

static struct mddi_platform_data mddi_pdata = {
	.mddi_power_save = msm_fb_mddi_power_save,
	.mddi_sel_clk = msm_fb_mddi_sel_clk,
};

static void __init msm_fb_add_devices(void)
{
	msm_fb_register_device("mdp", 0);
	msm_fb_register_device("pmdh", &mddi_pdata);
	msm_fb_register_device("emdh", &mddi_pdata);
	msm_fb_register_device("tvenc", 0);
	msm_fb_register_device("lcdc", 0);
}

static struct resource msm_audio_resources[] = {
	{
		.flags  = IORESOURCE_DMA,
	},
	{
		.name   = "aux_pcm_dout",
		.start  = 68,
		.end    = 68,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "aux_pcm_din",
		.start  = 69,
		.end    = 69,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "aux_pcm_syncout",
		.start  = 70,
		.end    = 70,
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "aux_pcm_clkin_a",
		.start  = 71,
		.end    = 71,
		.flags  = IORESOURCE_IO,
	},

	{
		.name	= "audio_base_addr",
		.start	= 0xa0700000,
		.end	= 0xa0700000 + 4,
		.flags	= IORESOURCE_MEM,
	},

};

#if 0
static unsigned audio_gpio_on[] = {
	GPIO_CFG(68, 1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
	GPIO_CFG(69, 1, GPIO_CFG_INPUT,  GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
	GPIO_CFG(70, 2, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
	GPIO_CFG(71, 2, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
	GPIO_CFG(142, 2, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
	GPIO_CFG(143, 1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
	GPIO_CFG(144, 1, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	GPIO_CFG(145, 1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
	GPIO_CFG(146, 2, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
};

static void __init audio_gpio_init(void)
{
	int pin, rc;

	for (pin = 0; pin < ARRAY_SIZE(audio_gpio_on); pin++) {
		rc = gpio_tlmm_config(audio_gpio_on[pin],
			GPIO_CFG_ENABLE);
		if (rc) {
			printk(KERN_ERR
				"%s: gpio_tlmm_config(%#x)=%d\n",
				__func__, audio_gpio_on[pin], rc);
			return;
		}
	}
}
#endif

static struct platform_device msm_audio_device = {
	.name   = "msm_audio",
	.id     = 0,
	.num_resources  = ARRAY_SIZE(msm_audio_resources),
	.resource       = msm_audio_resources,
};

static struct resource bluesleep_resources[] = {
	{
		.name	= "gpio_host_wake",
		.start	= 36,
		.end	= 36,
		.flags	= IORESOURCE_IO,
	},
	{
		.name	= "gpio_ext_wake",
		.start	= 22,
		.end	= 22,
		.flags	= IORESOURCE_IO,
	},
	{
		.name	= "host_wake",
		.start	= MSM_GPIO_TO_INT(36),
		.end	= MSM_GPIO_TO_INT(36),
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device msm_bluesleep_device = {
	.name = "bluesleep",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(bluesleep_resources),
	.resource	= bluesleep_resources,
};

#ifdef CONFIG_BT
static struct platform_device bt_rfkill = {
	.name = "bt_rfkill",
	.id = 1,
};
#endif

static struct resource headset_resources[] = {
{
		.name	= "jack_int",
		.start	= 37,
		.end		= 37,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "hook_int",
		.start	= 152,
		.end		= 152,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device qsd_headset_device = {
	.name = "headset",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(headset_resources),
	.resource	= headset_resources,
};

#ifdef CONFIG_MSM_BT_POWER
static struct platform_device msm_bt_power_device = {
	.name = "bt_power",
};

enum {
	BT_SYSRST,
	BT_WAKE,
	BT_HOST_WAKE,
	BT_VDD_IO,
	BT_RFR,
	BT_CTS,
	BT_RX,
	BT_TX,
	BT_VDD_FREG
};

static struct msm_gpio bt_config_power_off[] = {
	{ GPIO_CFG(18, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		"BT SYSRST" },
	{ GPIO_CFG(19, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		"BT WAKE" },
	{ GPIO_CFG(21, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		"HOST WAKE" },
	{ GPIO_CFG(22, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		"BT VDD_IO" },
	{ GPIO_CFG(43, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		"UART1DM_RFR" },
	{ GPIO_CFG(44, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		"UART1DM_CTS" },
	{ GPIO_CFG(45, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		"UART1DM_RX" },
	{ GPIO_CFG(46, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		"UART1DM_TX" }
};

static struct msm_gpio bt_config_power_on[] = {
	{ GPIO_CFG(18, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
		"BT SYSRST" },
	{ GPIO_CFG(19, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
		"BT WAKE" },
	{ GPIO_CFG(21, 0, GPIO_CFG_INPUT,  GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
		"HOST WAKE" },
	{ GPIO_CFG(22, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
		"BT VDD_IO" },
	{ GPIO_CFG(43, 2, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
		"UART1DM_RFR" },
	{ GPIO_CFG(44, 2, GPIO_CFG_INPUT,  GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
		"UART1DM_CTS" },
	{ GPIO_CFG(45, 2, GPIO_CFG_INPUT,  GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
		"UART1DM_RX" },
	{ GPIO_CFG(46, 2, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
		"UART1DM_TX" }
};

static struct msm_gpio wlan_config_power_off[] = {
	{ GPIO_CFG(62, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		"SDC2_CLK" },
	{ GPIO_CFG(63, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		"SDC2_CMD" },
	{ GPIO_CFG(64, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		"SDC2_D3" },
	{ GPIO_CFG(65, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		"SDC2_D2" },
	{ GPIO_CFG(66, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		"SDC2_D1" },
	{ GPIO_CFG(67, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		"SDC2_D0" },
	{ GPIO_CFG(113, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		"VDD_WLAN" },
	{ GPIO_CFG(138, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
		"WLAN_PWD" }
};

static struct msm_gpio wlan_config_power_on[] = {
	{ GPIO_CFG(62, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
		"SDC2_CLK" },
	{ GPIO_CFG(63, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
		"SDC2_CMD" },
	{ GPIO_CFG(64, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
		"SDC2_D3" },
	{ GPIO_CFG(65, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
		"SDC2_D2" },
	{ GPIO_CFG(66, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
		"SDC2_D1" },
	{ GPIO_CFG(67, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
		"SDC2_D0" },
	{ GPIO_CFG(113, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
		"VDD_WLAN" },
	{ GPIO_CFG(138, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
		"WLAN_PWD" }
};

static int bluetooth_power(int on)
{
	int rc;
	struct vreg *vreg_wlan;

	vreg_wlan = vreg_get(NULL, "wlan");

	if (IS_ERR(vreg_wlan)) {
		printk(KERN_ERR "%s: vreg get failed (%ld)\n",
		       __func__, PTR_ERR(vreg_wlan));
		return PTR_ERR(vreg_wlan);
	}

	if (on) {
		rc = vreg_set_level(vreg_wlan, PMIC_VREG_WLAN_LEVEL);
		if (rc) {
			printk(KERN_ERR "%s: vreg wlan set level failed (%d)\n",
			       __func__, rc);
			return -EIO;
		}
		rc = vreg_enable(vreg_wlan);
		if (rc) {
			printk(KERN_ERR "%s: vreg wlan enable failed (%d)\n",
			       __func__, rc);
			return -EIO;
		}

		rc = msm_gpios_enable(bt_config_power_on,
					ARRAY_SIZE(bt_config_power_on));
		if (rc < 0) {
			printk(KERN_ERR
				"%s: bt power on gpio config failed: %d\n",
				__func__, rc);
			return rc;
		}

		if (machine_is_qsd8x50_ffa() || machine_is_qsd8x50a_ffa()) {
			rc = msm_gpios_enable
					(wlan_config_power_on,
					 ARRAY_SIZE(wlan_config_power_on));
			if (rc < 0) {
				printk
				 (KERN_ERR
				 "%s: wlan power on gpio config failed: %d\n",
					__func__, rc);
				return rc;
			}
		}

		gpio_set_value(22, on);
		gpio_set_value(18, on);

		if (machine_is_qsd8x50_ffa() || machine_is_qsd8x50a_ffa()) {
			gpio_set_value(138, 0);
			gpio_set_value(113, on);
		}
	} else {
		if (machine_is_qsd8x50_ffa() || machine_is_qsd8x50a_ffa()) {
			gpio_set_value(138, on);
			gpio_set_value(113, on);
		}

		gpio_set_value(18, on);
		gpio_set_value(22, on);

		rc = vreg_disable(vreg_wlan);
		if (rc) {
			printk(KERN_ERR "%s: vreg wlan disable failed (%d)\n",
					__func__, rc);
			return -EIO;
		}

		rc = msm_gpios_enable(bt_config_power_off,
					ARRAY_SIZE(bt_config_power_off));
		if (rc < 0) {
			printk(KERN_ERR
				"%s: bt power off gpio config failed: %d\n",
				__func__, rc);
			return rc;
		}

		if (machine_is_qsd8x50_ffa() || machine_is_qsd8x50a_ffa()) {
			rc = msm_gpios_enable
					(wlan_config_power_off,
					 ARRAY_SIZE(wlan_config_power_off));
			if (rc < 0) {
				printk
				 (KERN_ERR
				 "%s: wlan power off gpio config failed: %d\n",
					__func__, rc);
				return rc;
			}
		}
	}

	printk(KERN_DEBUG "Bluetooth power switch: %d\n", on);

	return 0;
}

static void __init bt_power_init(void)
{
	struct vreg *vreg_bt;
	int rc;

	if (machine_is_qsd8x50_ffa() || machine_is_qsd8x50a_ffa()) {
		gpio_set_value(138, 0);
		gpio_set_value(113, 0);
	}

	gpio_set_value(18, 0);
	gpio_set_value(22, 0);

	vreg_bt = vreg_get(NULL, "gp6");

	if (IS_ERR(vreg_bt)) {
		printk(KERN_ERR "%s: vreg get failed (%ld)\n",
		       __func__, PTR_ERR(vreg_bt));
		goto exit;
	}

	rc = vreg_set_level(vreg_bt, PMIC_VREG_GP6_LEVEL);
	if (rc) {
		printk(KERN_ERR "%s: vreg bt set level failed (%d)\n",
		       __func__, rc);
		goto exit;
	}
	rc = vreg_enable(vreg_bt);
	if (rc) {
		printk(KERN_ERR "%s: vreg bt enable failed (%d)\n",
		       __func__, rc);
		goto exit;
	}

	if (bluetooth_power(0))
		goto exit;

	msm_bt_power_device.dev.platform_data = &bluetooth_power;

	printk(KERN_DEBUG "Bluetooth power switch: initialized\n");

exit:
	return;
}
#else
#define bt_power_init(x) do {} while (0)
#endif

static struct resource kgsl_resources[] = {
       {
		.name  = "kgsl_reg_memory",
		.start = 0xA0000000,
		.end = 0xA001ffff,
		.flags = IORESOURCE_MEM,
       },
       {
		.name   = "kgsl_phys_memory",
		.start = MSM_GPU_PHYS_BASE,
		.end = MSM_GPU_PHYS_BASE + MSM_GPU_PHYS_SIZE - 1,
		.flags = IORESOURCE_MEM,
       },
       {
		.name = "kgsl_yamato_irq",
		.start = INT_GRAPHICS,
		.end = INT_GRAPHICS,
		.flags = IORESOURCE_IRQ,
       },
};
static struct kgsl_platform_data kgsl_pdata = {
	.high_axi_3d = 128000,
	.max_grp2d_freq = 0,
	.min_grp2d_freq = 0,
	.set_grp2d_async = NULL,
	.max_grp3d_freq = 0,
	.min_grp3d_freq = 0,
	.set_grp3d_async = NULL,
	.imem_clk_name = "imem_clk",
	.grp3d_clk_name = "grp_clk",
	.grp2d0_clk_name = NULL,
	.idle_timeout_3d = HZ/5,
	.idle_timeout_2d = 0,
#ifdef CONFIG_KGSL_PER_PROCESS_PAGE_TABLE
	.pt_va_size = SZ_32M,
	.pt_max_count = 32,
#else
	.pt_va_size = SZ_128M,
	.pt_max_count = 1,
#endif
};

static struct platform_device msm_device_kgsl = {
       .name = "kgsl",
       .id = -1,
       .num_resources = ARRAY_SIZE(kgsl_resources),
       .resource = kgsl_resources,
	.dev = {
		.platform_data = &kgsl_pdata,
	},
};

static struct platform_device msm_device_pmic_leds = {
	.name	= "pmic-leds",
	.id	= -1,
};
#ifdef CONFIG_MUTEKEY
static struct mutekey_platform_data_t mutekey_plat_data = {
	.gpio_mutekey = Mutekey_GPIO_NUM,
};
#endif
#ifdef CONFIG_MUTEKEY
static struct platform_device mutekey_device = {
	.name      = MUTEKEY_DRIVER_NAME,
	.id   = -1,
	.dev       = {
		.platform_data = &mutekey_plat_data,
	},
};
#endif

#ifdef CONFIG_BATTERY_QSD
  static struct platform_device qsd_battery_device = {
    .name = "qsd_battery",
    .id   = -1,
  };
#endif
#ifdef CONFIG_LEDS_QSD
  static struct platform_device qsd_led = {
  .name = "qsd_led",
	.id	= -1,
};
#endif

#if defined(CONFIG_TSIF) || defined(CONFIG_TSIF_MODULE)

#define TSIF_A_SYNC      GPIO_CFG(106, 1, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA)
#define TSIF_A_DATA      GPIO_CFG(107, 1, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA)
#define TSIF_A_EN        GPIO_CFG(108, 1, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA)
#define TSIF_A_CLK       GPIO_CFG(109, 1, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA)

static const struct msm_gpio tsif_gpios[] = {
	{ .gpio_cfg = TSIF_A_CLK,  .label =  "tsif_clk", },
	{ .gpio_cfg = TSIF_A_EN,   .label =  "tsif_en", },
	{ .gpio_cfg = TSIF_A_DATA, .label =  "tsif_data", },
	{ .gpio_cfg = TSIF_A_SYNC, .label =  "tsif_sync", },
};

static struct msm_tsif_platform_data tsif_platform_data = {
	.num_gpios = ARRAY_SIZE(tsif_gpios),
	.gpios = tsif_gpios,
	.tsif_clk = "tsif_clk",
	.tsif_ref_clk = "tsif_ref_clk",
};

#endif

#ifdef CONFIG_QSD_SVS
#define TPS65023_MAX_DCDC1	1600
#else
#define TPS65023_MAX_DCDC1	CONFIG_QSD_PMIC_DEFAULT_DCDC1
#endif

static int qsd8x50_tps65023_set_dcdc1(int mVolts)
{
	int rc = 0;
#ifdef CONFIG_QSD_SVS
	rc = tps65023_set_dcdc1_level(mVolts);

	if (rc == -ENODEV && mVolts <= CONFIG_QSD_PMIC_DEFAULT_DCDC1)
		rc = 0;
#else
	if (mVolts > CONFIG_QSD_PMIC_DEFAULT_DCDC1)
		rc = -EFAULT;
#endif
	return rc;
}

static struct msm_acpu_clock_platform_data qsd8x50_clock_data = {
	.acpu_switch_time_us = 20,
	.max_speed_delta_khz = 256000,
	.vdd_switch_time_us = 62,
	.max_vdd = TPS65023_MAX_DCDC1,
	.acpu_set_vdd = qsd8x50_tps65023_set_dcdc1,
};

#if 0
static void touchpad_gpio_release(void)
{
	gpio_free(TOUCHPAD_IRQ);
	gpio_free(TOUCHPAD_SUSPEND);
}

static int touchpad_gpio_setup(void)
{
	int rc;
	int suspend_pin = TOUCHPAD_SUSPEND;
	int irq_pin = TOUCHPAD_IRQ;
	unsigned suspend_cfg =
		GPIO_CFG(suspend_pin, 1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA);
	unsigned irq_cfg =
		GPIO_CFG(irq_pin, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA);

	rc = gpio_request(irq_pin, "msm_touchpad_irq");
	if (rc) {
		pr_err("gpio_request failed on pin %d (rc=%d)\n",
		       irq_pin, rc);
		goto err_gpioconfig;
	}
	rc = gpio_request(suspend_pin, "msm_touchpad_suspend");
	if (rc) {
		pr_err("gpio_request failed on pin %d (rc=%d)\n",
		       suspend_pin, rc);
		goto err_gpioconfig;
	}
	rc = gpio_tlmm_config(suspend_cfg, GPIO_CFG_ENABLE);
	if (rc) {
		pr_err("gpio_tlmm_config failed on pin %d (rc=%d)\n",
		       suspend_pin, rc);
		goto err_gpioconfig;
	}
	rc = gpio_tlmm_config(irq_cfg, GPIO_CFG_ENABLE);
	if (rc) {
		pr_err("gpio_tlmm_config failed on pin %d (rc=%d)\n",
		       irq_pin, rc);
		goto err_gpioconfig;
	}
	return rc;

err_gpioconfig:
	touchpad_gpio_release();
	return rc;
}

static struct msm_touchpad_platform_data msm_touchpad_data = {
	.gpioirq     = TOUCHPAD_IRQ,
	.gpiosuspend = TOUCHPAD_SUSPEND,
	.gpio_setup  = touchpad_gpio_setup,
	.gpio_shutdown = touchpad_gpio_release
};
#endif

#ifdef CONFIG_TOUCHSCREEN_ATMEL_mXT224
static struct atmel_touchscreen_platform_data_t atmel_touchscreen_plat_data = {
	.gpiorst = TOUCHSCREEN_RST_GPIO_NUM,
	.gpioirq = TOUCHSCREEN_INT_GPIO_NUM,
};
#endif

#if 0
#define KBD_RST 35
#define KBD_IRQ 36

static void kbd_gpio_release(void)
{
	gpio_free(KBD_IRQ);
	gpio_free(KBD_RST);
}

static int kbd_gpio_setup(void)
{
	int rc;
	int respin = KBD_RST;
	int irqpin = KBD_IRQ;
	unsigned rescfg =
		GPIO_CFG(respin, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA);
	unsigned irqcfg =
		GPIO_CFG(irqpin, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA);

	rc = gpio_request(irqpin, "gpio_keybd_irq");
	if (rc) {
		pr_err("gpio_request failed on pin %d (rc=%d)\n",
			irqpin, rc);
		goto err_gpioconfig;
	}
	rc = gpio_request(respin, "gpio_keybd_reset");
	if (rc) {
		pr_err("gpio_request failed on pin %d (rc=%d)\n",
			respin, rc);
		goto err_gpioconfig;
	}
	rc = gpio_tlmm_config(rescfg, GPIO_CFG_ENABLE);
	if (rc) {
		pr_err("gpio_tlmm_config failed on pin %d (rc=%d)\n",
			respin, rc);
		goto err_gpioconfig;
	}
	rc = gpio_tlmm_config(irqcfg, GPIO_CFG_ENABLE);
	if (rc) {
		pr_err("gpio_tlmm_config failed on pin %d (rc=%d)\n",
			irqpin, rc);
		goto err_gpioconfig;
	}
	return rc;

err_gpioconfig:
	kbd_gpio_release();
	return rc;
}

static void kbd_hwreset(int kbd_mclrpin)
{
	gpio_direction_output(kbd_mclrpin, 0);
	gpio_direction_output(kbd_mclrpin, 1);
}

static struct msm_i2ckbd_platform_data msm_kybd_data = {
	.hwrepeat = 0,
	.scanset1 = 1,
	.gpioreset = KBD_RST,
	.gpioirq = KBD_IRQ,
	.gpio_setup = kbd_gpio_setup,
	.gpio_shutdown = kbd_gpio_release,
	.hw_reset = kbd_hwreset,
};
#endif

#ifdef 	CONFIG_BCOM_FM
static struct bcom_fm_platform_data bcom_fm_palt_data =
{
	.gpio_fm_wakeup = FM_WAKEUP,
};
#endif

#ifdef CONFIG_ECOMPASS_AKM_AK8973S
static struct ecompass_platform_data_t ecompass_plat_data = {
        .gpioreset = ECOMPASS_RST_GPIO_NUM,
        .gpiointr = ECOMPASS_INT_GPIO_NUM,
};
#endif

static struct i2c_board_info msm_i2c_board_info[] __initdata = {

#if 0
	{
		I2C_BOARD_INFO("glidesensor", 0x2A),
		.irq           =  MSM_GPIO_TO_INT(TOUCHPAD_IRQ),
		.platform_data = &msm_touchpad_data
	},
#endif
	{
		I2C_BOARD_INFO("msm-i2ckbd", 0x3A),
		.type           = "msm-i2ckbd",
	},

#ifdef CONFIG_TOUCHSCREEN_ATMEL_mXT224
    	{
		I2C_BOARD_INFO( ATMEL_TOUCHSCREEN_DRIVER_NAME, TOUCHSCREEN_I2C_ADDR),
		.type           = ATMEL_TOUCHSCREEN_DRIVER_NAME,
		.irq            = MSM_GPIO_TO_INT(TOUCHSCREEN_INT_GPIO_NUM),
		.platform_data  = &atmel_touchscreen_plat_data
	},
#endif

#ifdef CONFIG_MT9D112
	{
		I2C_BOARD_INFO("mt9d112", 0x78 >> 1),
	},
#endif
#ifdef CONFIG_S5K3E2FX
	{
		I2C_BOARD_INFO("s5k3e2fx", 0x20 >> 1),
	},
#endif
#ifdef CONFIG_MT9P012
	{
		I2C_BOARD_INFO("mt9p012", 0x6C >> 1),
	},
#endif
#ifdef CONFIG_MT9P012_KM
	{
		I2C_BOARD_INFO("mt9p012_km", 0x6C >> 2),
	},
#endif
#if defined(CONFIG_MT9T013) || defined(CONFIG_SENSORS_MT9T013)
	{
		I2C_BOARD_INFO("mt9t013", 0x6C),
	},
#endif
	{
		I2C_BOARD_INFO("tps65023", 0x48),
	},
#ifdef CONFIG_OV5642
	{
		I2C_BOARD_INFO("ov5642", 0x78 >> 1),
	},
#endif
#ifdef CONFIG_OV7690
	{
		I2C_BOARD_INFO("ov7690", 0x42 >> 1),
	},
#endif

#ifdef CONFIG_OV8810
	{
		I2C_BOARD_INFO("ov8810_abico", 0x6C >> 2),
	},
	{
		I2C_BOARD_INFO("ov8810_semco", 0xA0 >> 2),
	},
#endif

	#ifdef CONFIG_BATTERY_QSD
	{
		I2C_BOARD_INFO("qsd_bat_i2c", GAUGE_ADDR),
	},
	#endif

	#ifdef CONFIG_LIGHT_SENSOR_QSD
	{
		I2C_BOARD_INFO("lsensor_i2c", LSENSOR_ADDR),
	},
	#endif

	#ifdef CONFIG_GSENSOR_BOSCH_BMA150
	{
		I2C_BOARD_INFO("gsensor_bma150", GSENSOR_I2C_ADDR),
		.type           = "gsensor_bma150",
		.irq            = MSM_GPIO_TO_INT(GSENSOR_INT_GPIO_NUM),
	},
	#endif

	#ifdef CONFIG_ECOMPASS_AKM_AK8973S
	{
		I2C_BOARD_INFO("ecompass_ak8973s", ECOMPASS_I2C_ADDR),
		.type           = "ecompass_ak8973s",
		.irq            = MSM_GPIO_TO_INT(ECOMPASS_INT_GPIO_NUM),
		.platform_data  = &ecompass_plat_data
	},
	#endif

	#ifdef CONFIG_BCOM_FM
	{
		I2C_BOARD_INFO("bcom_fm", 0x22),
		.type	= "bcom_fm",
		.irq		= MSM_GPIO_TO_INT(FM_WAKEUP),
		.platform_data 	= &bcom_fm_palt_data
	},
	#endif

	{
		I2C_BOARD_INFO("synaptics_capsensor", 0x2C),
		.type           = "synaptics_capsensor",
		.irq            = MSM_GPIO_TO_INT(31),
	},
	#ifdef CONFIG_FB_MSM_LCDC_ADV7520_HDMI
	{
		I2C_BOARD_INFO("hdmi_main", 0x39),
		.type           = "hdmi_main",
		.irq            = MSM_GPIO_TO_INT(39),
	},

	{
		I2C_BOARD_INFO("hdmi_edid", 0x3F),
		.type           = "hdmi_edid",
	},

	{
		I2C_BOARD_INFO("hdmi_cec", 0x2A),
		.type           = "hdmi_cec",
	},

	{
		I2C_BOARD_INFO("hdmi_pkt", 0x28),
		.type           = "hdmi_pkt",
	},
	#endif
};

#ifdef CONFIG_MSM_CAMERA
static uint32_t camera_off_gpio_table[] = {
#if 0
	GPIO_CFG(0,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	GPIO_CFG(1,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	GPIO_CFG(2,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	GPIO_CFG(3,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
#endif
	GPIO_CFG(4,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	GPIO_CFG(5,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	GPIO_CFG(6,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	GPIO_CFG(7,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	GPIO_CFG(8,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	GPIO_CFG(9,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	GPIO_CFG(10, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	GPIO_CFG(11, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	GPIO_CFG(12, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	GPIO_CFG(13, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	GPIO_CFG(14, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	GPIO_CFG(15, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),
};

static uint32_t camera_on_gpio_table[] = {
#if 0
	GPIO_CFG(0,  1, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	GPIO_CFG(1,  1, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	GPIO_CFG(2,  1, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	GPIO_CFG(3,  1, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
#endif
	GPIO_CFG(4,  1, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	GPIO_CFG(5,  1, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	GPIO_CFG(6,  1, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	GPIO_CFG(7,  1, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	GPIO_CFG(8,  1, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	GPIO_CFG(9,  1, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	GPIO_CFG(10, 1, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	GPIO_CFG(11, 1, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	GPIO_CFG(12, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	GPIO_CFG(13, 1, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	GPIO_CFG(14, 1, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	GPIO_CFG(15, 1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_16MA),
};

static uint32_t camera_on_gpio_ffa_table[] = {
	GPIO_CFG(95,  1, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA),
	GPIO_CFG(96,  1, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA),
	GPIO_CFG(137,  1, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA),
};

static uint32_t camera_off_gpio_ffa_table[] = {
	GPIO_CFG(137,  0, GPIO_CFG_INPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_16MA),
};

static uint32_t camera_on_gpio_surf_table[] = {
    GPIO_CFG(2,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA),
    GPIO_CFG(3,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
    GPIO_CFG(104, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA),
};

static uint32_t camera_off_gpio_surf_table[] = {
	GPIO_CFG(2,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
	GPIO_CFG(3,  0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
    GPIO_CFG(104, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
};

static void config_gpio_table(uint32_t *table, int len)
{
	int n, rc;
	for (n = 0; n < len; n++) {
		rc = gpio_tlmm_config(table[n], GPIO_CFG_ENABLE);
		if (rc) {
			printk(KERN_ERR "%s: gpio_tlmm_config(%#x)=%d\n",
				__func__, table[n], rc);
			break;
		}
	}
}

static struct vreg *vreg_gp2;
static struct vreg *vreg_gp3;

static void msm_camera_vreg_config(int vreg_en)
{
	int rc;

	if (vreg_gp2 == NULL) {
		vreg_gp2 = vreg_get(NULL, "gp2");
		if (IS_ERR(vreg_gp2)) {
			printk(KERN_ERR "%s: vreg_get(%s) failed (%ld)\n",
				__func__, "gp2", PTR_ERR(vreg_gp2));
			return;
		}

		rc = vreg_set_level(vreg_gp2, 1800);
		if (rc) {
			printk(KERN_ERR "%s: GP2 set_level failed (%d)\n",
				__func__, rc);
		}
	}

	if (vreg_gp3 == NULL) {
		vreg_gp3 = vreg_get(NULL, "gp3");
		if (IS_ERR(vreg_gp3)) {
			printk(KERN_ERR "%s: vreg_get(%s) failed (%ld)\n",
				__func__, "gp3", PTR_ERR(vreg_gp3));
			return;
		}

		rc = vreg_set_level(vreg_gp3, 2800);
		if (rc) {
			printk(KERN_ERR "%s: GP3 set level failed (%d)\n",
				__func__, rc);
		}
	}

	if (vreg_en) {
		rc = vreg_enable(vreg_gp2);
		if (rc) {
			printk(KERN_ERR "%s: GP2 enable failed (%d)\n",
				 __func__, rc);
		}

		rc = vreg_enable(vreg_gp3);
		if (rc) {
			printk(KERN_ERR "%s: GP3 enable failed (%d)\n",
				__func__, rc);
		}
	} else {
		rc = vreg_disable(vreg_gp2);
		if (rc) {
			printk(KERN_ERR "%s: GP2 disable failed (%d)\n",
				 __func__, rc);
		}

		rc = vreg_disable(vreg_gp3);
		if (rc) {
			printk(KERN_ERR "%s: GP3 disable failed (%d)\n",
				__func__, rc);
		}
	}
}

static int config_camera_on_gpios(void)
{
	int vreg_en = 1;

	if (machine_is_qsd8x50_ffa() || machine_is_qsd8x50a_ffa()) {
		config_gpio_table(camera_on_gpio_ffa_table,
		ARRAY_SIZE(camera_on_gpio_ffa_table));

		msm_camera_vreg_config(vreg_en);
		gpio_set_value(137, 0);
	}
	config_gpio_table(camera_on_gpio_table,
		ARRAY_SIZE(camera_on_gpio_table));
	return 0;
}

static void config_camera_off_gpios(void)
{
	int vreg_en = 0;

	if (machine_is_qsd8x50_ffa() || machine_is_qsd8x50a_ffa()) {
		config_gpio_table(camera_off_gpio_ffa_table,
		ARRAY_SIZE(camera_off_gpio_ffa_table));

		msm_camera_vreg_config(vreg_en);
	}
	config_gpio_table(camera_off_gpio_table,
		ARRAY_SIZE(camera_off_gpio_table));
}

static void config_camera_power( char *name, int mv)
{
    struct vreg *vreg;
    int ret = 0;

    vreg = vreg_get(NULL, name);
    if (IS_ERR(vreg)) {
    	printk(KERN_ERR "%s: vreg_get(%s) failed (%ld)\n",
    	__func__, name, PTR_ERR(vreg));
    	return;
    }

    if( mv > 0 ) {
        ret = vreg_set_level( vreg, mv );
        if( ret < 0 ) {
    	    printk(KERN_ERR "%s: %s(%s,%d) failed!\n",
    		   __func__, "vreg_set_level", name, mv);
            return;
        }
    }

    ret = (mv > 0) ? vreg_enable(vreg) : vreg_disable(vreg);
    if (ret)
    	printk(KERN_ERR "%s: %s(%s) failed!\n",
    		__func__, (mv > 0) ? "vreg_enable" : "vreg_disable", name);

    return;
}

static void config_camera_power_on(void)
{
    config_gpio_table(camera_on_gpio_surf_table,
	                  ARRAY_SIZE(camera_on_gpio_surf_table));

    gpio_set_value(2, 1);
    gpio_set_value(3, 0);
    gpio_set_value(104, 1);

    config_camera_power( "wlan",  2600 );
    config_camera_power( "gp5",   2850 );
    config_camera_power( "msme2", 1500 );
    config_camera_power( "gp3",   2800 );

    return;
}

static void config_camera_power_off(void)
{
    config_camera_power( "gp3",   0 );
    config_camera_power( "msme2", 0 );
    config_camera_power( "gp5",   0 );
    config_camera_power( "wlan",  0 );

    config_gpio_table(camera_off_gpio_surf_table,
                      ARRAY_SIZE(camera_off_gpio_surf_table));

    gpio_set_value(2, 0);
    mdelay(4);
    gpio_set_value(3, 0);
    gpio_set_value(104, 0);

    return;
}

static struct resource msm_camera_resources[] = {
	{
		.start	= 0xA0F00000,
		.end	= 0xA0F00000 + SZ_1M - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.start	= INT_VFE,
		.end	= INT_VFE,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct msm_camera_device_platform_data msm_camera_device_data = {
	.camera_gpio_on  = config_camera_on_gpios,
	.camera_gpio_off = config_camera_off_gpios,
	.ioext.mdcphy = MSM_MDC_PHYS,
	.ioext.mdcsz  = MSM_MDC_SIZE,
	.ioext.appphy = MSM_CLK_CTL_PHYS,
	.ioext.appsz  = MSM_CLK_CTL_SIZE,
    .camera_power_on  = config_camera_power_on,
    .camera_power_off = config_camera_power_off,
};

#if 0
int pmic_set_flash_led_current(enum pmic8058_leds id, unsigned mA)
{
	int rc;
	rc = pmic_flash_led_set_current(mA);
	return rc;
}
static struct msm_camera_sensor_flash_src msm_flash_src = {
	.flash_sr_type = MSM_CAMERA_FLASH_SRC_PMIC,
	._fsrc.pmic_src.num_of_src = 1,
	._fsrc.pmic_src.low_current  = 30,
	._fsrc.pmic_src.high_current = 100,
	._fsrc.pmic_src.led_src_1 = 0,
	._fsrc.pmic_src.led_src_2 = 0,
	._fsrc.pmic_src.pmic_set_current = pmic_set_flash_led_current,
};
#else
static struct msm_camera_sensor_flash_src msm_flash_src = {
	.flash_sr_type = MSM_CAMERA_FLASH_SRC_PMIC,
	._fsrc.pmic_src.low_current  = 60,
	._fsrc.pmic_src.high_current = 60,
};

#endif

#ifdef CONFIG_MT9D112
static struct msm_camera_sensor_flash_data flash_mt9d112 = {
	.flash_type = MSM_CAMERA_FLASH_LED,
	.flash_src  = &msm_flash_src
};

static struct msm_camera_sensor_info msm_camera_sensor_mt9d112_data = {
	.sensor_name    = "mt9d112",
	.sensor_reset   = 17,
	.sensor_pwd     = 85,
	.vcm_pwd        = 0,
	.vcm_enable     = 0,
	.pdata          = &msm_camera_device_data,
	.resource       = msm_camera_resources,
	.num_resources  = ARRAY_SIZE(msm_camera_resources),
	.flash_data     = &flash_mt9d112
};

static struct platform_device msm_camera_sensor_mt9d112 = {
	.name      = "msm_camera_mt9d112",
	.dev       = {
		.platform_data = &msm_camera_sensor_mt9d112_data,
	},
};
#endif

#ifdef CONFIG_OV8810
static struct msm_camera_sensor_flash_data flash_ov8810 = {
	.flash_type = MSM_CAMERA_FLASH_LED,
	.flash_src  = &msm_flash_src
};

#if 0
static struct msm_camera_sensor_info msm_camera_sensor_ov8810_data = {
        .sensor_name    = "ov8810",
		.sensor_reset = 105,
		.sensor_pwd   = 104,
		.vcm_pwd      = 0,
        .pdata          = &msm_camera_device_data,
        .resource       = msm_camera_resources,
        .num_resources  = ARRAY_SIZE(msm_camera_resources),
        .flash_data     = &flash_ov8810
};
static struct platform_device msm_camera_sensor_ov8810 = {
        .name      = "msm_camera_ov8810",
        .dev        = {
                .platform_data = &msm_camera_sensor_ov8810_data,
	},
};
#endif

static struct msm_camera_sensor_info msm_camera_sensor_ov8810_data_abico = {
        .sensor_name    = "ov8810_abico",
		.sensor_reset = 105,
		.sensor_pwd   = 104,
		.vcm_pwd      = 0,
        .pdata          = &msm_camera_device_data,
        .resource       = msm_camera_resources,
        .num_resources  = ARRAY_SIZE(msm_camera_resources),
        .flash_data     = &flash_ov8810
};
static struct platform_device msm_camera_sensor_ov8810_abico = {
        .name      = "msm_camera_ov8810_abico",
        .dev        = {
                .platform_data = &msm_camera_sensor_ov8810_data_abico,
	},
};

static struct msm_camera_sensor_info msm_camera_sensor_ov8810_data_semco = {
        .sensor_name    = "ov8810_semco",
		.sensor_reset = 105,
		.sensor_pwd   = 104,
		.vcm_pwd      = 0,
        .pdata          = &msm_camera_device_data,
        .resource       = msm_camera_resources,
        .num_resources  = ARRAY_SIZE(msm_camera_resources),
        .flash_data     = &flash_ov8810
};
static struct platform_device msm_camera_sensor_ov8810_semco = {
        .name      = "msm_camera_ov8810_semco",
        .dev        = {
                .platform_data = &msm_camera_sensor_ov8810_data_semco,
	},
};
#endif

#ifdef CONFIG_S5K3E2FX
static struct msm_camera_sensor_flash_data flash_s5k3e2fx = {
	.flash_type = MSM_CAMERA_FLASH_LED,
	.flash_src  = &msm_flash_src
};

static struct msm_camera_sensor_info msm_camera_sensor_s5k3e2fx_data = {
	.sensor_name    = "s5k3e2fx",
	.sensor_reset   = 17,
	.sensor_pwd     = 85,
	.vcm_enable     = 0,
	.pdata          = &msm_camera_device_data,
	.resource       = msm_camera_resources,
	.num_resources  = ARRAY_SIZE(msm_camera_resources),
	.flash_data     = &flash_s5k3e2fx
};

static struct platform_device msm_camera_sensor_s5k3e2fx = {
	.name      = "msm_camera_s5k3e2fx",
	.dev       = {
		.platform_data = &msm_camera_sensor_s5k3e2fx_data,
	},
};
#endif

#ifdef CONFIG_MT9P012
static struct msm_camera_sensor_flash_data flash_mt9p012 = {
	.flash_type = MSM_CAMERA_FLASH_LED,
	.flash_src  = &msm_flash_src
};

static struct msm_camera_sensor_info msm_camera_sensor_mt9p012_data = {
	.sensor_name    = "mt9p012",
	.sensor_reset   = 17,
	.sensor_pwd     = 85,
	.vcm_pwd        = 88,
	.vcm_enable     = 0,
	.pdata          = &msm_camera_device_data,
	.resource       = msm_camera_resources,
	.num_resources  = ARRAY_SIZE(msm_camera_resources),
	.flash_data     = &flash_mt9p012
};

static struct platform_device msm_camera_sensor_mt9p012 = {
	.name      = "msm_camera_mt9p012",
	.dev       = {
		.platform_data = &msm_camera_sensor_mt9p012_data,
	},
};
#endif

#ifdef CONFIG_MT9P012_KM
static struct msm_camera_sensor_flash_data flash_mt9p012_km = {
	.flash_type = MSM_CAMERA_FLASH_LED,
	.flash_src  = &msm_flash_src
};

static struct msm_camera_sensor_info msm_camera_sensor_mt9p012_km_data = {
	.sensor_name    = "mt9p012_km",
	.sensor_reset   = 17,
	.sensor_pwd     = 85,
	.vcm_pwd        = 88,
	.vcm_enable     = 0,
	.pdata          = &msm_camera_device_data,
	.resource       = msm_camera_resources,
	.num_resources  = ARRAY_SIZE(msm_camera_resources),
	.flash_data     = &flash_mt9p012_km
};

static struct platform_device msm_camera_sensor_mt9p012_km = {
	.name      = "msm_camera_mt9p012_km",
	.dev       = {
		.platform_data = &msm_camera_sensor_mt9p012_km_data,
	},
};
#endif

#ifdef CONFIG_MT9T013
static struct msm_camera_sensor_flash_data flash_mt9t013 = {
	.flash_type = MSM_CAMERA_FLASH_LED,
	.flash_src  = &msm_flash_src
};

static struct msm_camera_sensor_info msm_camera_sensor_mt9t013_data = {
	.sensor_name    = "mt9t013",
	.sensor_reset   = 17,
	.sensor_pwd     = 85,
	.vcm_pwd        = 0,
	.vcm_enable     = 0,
	.pdata          = &msm_camera_device_data,
	.resource       = msm_camera_resources,
	.num_resources  = ARRAY_SIZE(msm_camera_resources),
	.flash_data     = &flash_mt9t013
};

static struct platform_device msm_camera_sensor_mt9t013 = {
	.name      = "msm_camera_mt9t013",
	.dev       = {
		.platform_data = &msm_camera_sensor_mt9t013_data,
	},
};
#endif

#ifdef CONFIG_OV5642
static struct msm_camera_sensor_flash_data flash_ov5642 = {
	.flash_type = MSM_CAMERA_FLASH_LED,
	.flash_src  = &msm_flash_src
};

static struct msm_camera_sensor_info msm_camera_sensor_ov5642_data = {
	.sensor_name    = "ov5642",
	.sensor_reset   = 3,
	.sensor_pwd     = 2,
	.pdata          = &msm_camera_device_data,
	.resource       = msm_camera_resources,
	.num_resources  = ARRAY_SIZE(msm_camera_resources),
	.flash_data     = &flash_ov5642,
    .sensor_preinit = ov5642_sensor_preinit,
};

static struct platform_device msm_camera_sensor_ov5642 = {
	.name      = "msm_camera_ov5642",
	.dev       = {
		.platform_data = &msm_camera_sensor_ov5642_data,
	},
};
#endif

#ifdef CONFIG_OV7690
static struct msm_camera_sensor_flash_data flash_ov7690 = {
	.flash_type = MSM_CAMERA_FLASH_NONE,
	.flash_src  = &msm_flash_src
};

static struct msm_camera_sensor_info msm_camera_sensor_ov7690_data = {
	.sensor_name    = "ov7690",
	.sensor_pwd     = 104,
	.pdata          = &msm_camera_device_data,
	.resource       = msm_camera_resources,
	.num_resources  = ARRAY_SIZE(msm_camera_resources),
	.flash_data     = &flash_ov7690,
    .sensor_preinit = ov7690_sensor_preinit,
};

static struct platform_device msm_camera_sensor_ov7690 = {
	.name      = "msm_camera_ov7690",
	.dev       = {
		.platform_data = &msm_camera_sensor_ov7690_data,
	},
};
#endif

#endif

#if 0
static u32 msm_calculate_batt_capacity(u32 current_voltage);

static struct msm_psy_batt_pdata msm_psy_batt_data = {
	.voltage_min_design 	= 3200,
	.voltage_max_design	= 4200,
	.avail_chg_sources   	= AC_CHG | USB_CHG ,
	.batt_technology        = POWER_SUPPLY_TECHNOLOGY_LION,
	.calculate_capacity	= &msm_calculate_batt_capacity,
};

static u32 msm_calculate_batt_capacity(u32 current_voltage)
{
	u32 low_voltage   = msm_psy_batt_data.voltage_min_design;
	u32 high_voltage  = msm_psy_batt_data.voltage_max_design;

	return (current_voltage - low_voltage) * 100
		/ (high_voltage - low_voltage);
}

static struct platform_device msm_batt_device = {
	.name 		    = "msm-battery",
	.id		    = -1,
	.dev.platform_data  = &msm_psy_batt_data,
};
#endif

static int hsusb_rpc_connect(int connect)
{
	if (connect)
		return msm_hsusb_rpc_connect();
	else
		return msm_hsusb_rpc_close();
}

static int msm_hsusb_pmic_notif_init(void (*callback)(int online), int init)
{
	int ret;

	if (init) {
		ret = msm_pm_app_rpc_init(callback);
	} else {
		msm_pm_app_rpc_deinit(callback);
		ret = 0;
	}
	return ret;
}
static int msm_hsusb_ldo_init(int init);
static int msm_hsusb_ldo_enable(int enable);

static struct msm_otg_platform_data msm_otg_pdata = {
	.rpc_connect	= hsusb_rpc_connect,
	.pmic_vbus_notif_init         = msm_hsusb_pmic_notif_init,
	.pemp_level              = PRE_EMPHASIS_WITH_10_PERCENT,
	.cdr_autoreset           = CDR_AUTO_RESET_DEFAULT,
	.drv_ampl                = HS_DRV_AMPLITUDE_5_PERCENT,
	.vbus_power		 = msm_hsusb_vbus_power,
	.chg_vbus_draw		 = hsusb_chg_vbus_draw,
	.chg_connected		 = hsusb_chg_connected,
	.chg_init		 = hsusb_chg_init,
	.phy_can_powercollapse	 = 1,
	.ldo_init		 = msm_hsusb_ldo_init,
	.ldo_enable		 = msm_hsusb_ldo_enable,
};

static struct msm_hsusb_gadget_platform_data msm_gadget_pdata;

static struct platform_device *early_devices[] __initdata = {
#ifdef CONFIG_GPIOLIB
	&msm_gpio_devices[0],
	&msm_gpio_devices[1],
	&msm_gpio_devices[2],
	&msm_gpio_devices[3],
	&msm_gpio_devices[4],
	&msm_gpio_devices[5],
	&msm_gpio_devices[6],
	&msm_gpio_devices[7],
#endif
};

static struct platform_device *devices[] __initdata = {
	&msm_fb_device,
#if 0
#ifdef CONFIG_SMC91X
	&smc91x_device,
#endif
#endif
	&msm_device_smd,
	&msm_device_dmov,
	&android_pmem_kernel_ebi1_device,
#ifdef CONFIG_KERNEL_PMEM_SMI_REGION
	&android_pmem_kernel_smi_device,
#endif
	&android_pmem_device,
	&android_pmem_adsp_device,
	&android_pmem_smipool_device,
	&msm_device_nand,
	&msm_device_i2c,
	&qsd_device_spi,
#ifdef CONFIG_USB_FUNCTION
	&mass_storage_device,
#endif
#ifdef CONFIG_USB_ANDROID
	&usb_mass_storage_device,
	&rndis_device,
#ifdef CONFIG_USB_ANDROID_DIAG
	&usb_diag_device,
#endif
	&android_usb_device,
#endif
	&msm_device_tssc,
	&msm_audio_device,
	&msm_device_uart_dm2,
	&msm_bluesleep_device,
#ifdef CONFIG_MSM_BT_POWER
	&msm_bt_power_device,
#endif

#if !defined(CONFIG_MSM_SERIAL_DEBUGGER)
//	&msm_device_uart1,
#endif
	&msm_device_pmic_leds,

#ifdef CONFIG_BT
	&bt_rfkill,
#endif

#ifdef CONFIG_BATTERY_QSD
	&qsd_battery_device,
#endif
#ifdef CONFIG_LEDS_QSD
	&qsd_led,
#endif

	&msm_device_kgsl,
	&hs_device,
#if defined(CONFIG_TSIF) || defined(CONFIG_TSIF_MODULE)
	&msm_device_tsif,
#endif
#ifdef CONFIG_MT9T013
	&msm_camera_sensor_mt9t013,
#endif
#ifdef CONFIG_OV8810
    &msm_camera_sensor_ov8810_abico,
    &msm_camera_sensor_ov8810_semco,
#endif
#ifdef CONFIG_MT9D112
	&msm_camera_sensor_mt9d112,
#endif
#ifdef CONFIG_S5K3E2FX
	&msm_camera_sensor_s5k3e2fx,
#endif
#ifdef CONFIG_MT9P012
	&msm_camera_sensor_mt9p012,
#endif
#ifdef CONFIG_MT9P012_KM
	&msm_camera_sensor_mt9p012_km,
#endif
#ifdef CONFIG_OV7690
	&msm_camera_sensor_ov7690,
#endif
#ifdef CONFIG_OV5642
	&msm_camera_sensor_ov5642,
#endif
	&qsd_headset_device,
};

#ifndef CONFIG_TINY_ANDROID
static void msm_get_usb_serial(void)
{
    int size;
    int iindex;
    device_otp_data_s_from_dt_or_diag *otp_data;

    {
        unsigned int rpc_arg2 = 0, rpc_arg1 = 0;
        int retValue = 0;

        rpc_arg1 = SMEM_PC_OEM_USB_DEFAULT_SERIAL_NUMBER_STATUS;
        retValue = cust_mproc_comm1(&rpc_arg1, &rpc_arg2);

        if (retValue != 0)
        {
            printk("%s: proc_comm failed\n", __func__);
        }
        else if (rpc_arg2 == 1)
        {
            printk("%s: usb single serial number\n", __func__);
            return;
        }
    }

    otp_data = (device_otp_data_s_from_dt_or_diag*)
        smem_get_entry(SMEM_OTP_DATA_FROM_DT_OR_DIAG, &size);

    if ((otp_data == NULL) || (otp_data->imei[0] != 0x8))
    {
        printk("%s, IMEI parameter failure...\n", __func__);
    }
    else
    {
        uint8_t tmpusbsn[16];
        char *tmpptr_usbsn = tmpusbsn;
        uint8_t imei_d;
        for(iindex = 1; iindex < 8; iindex++)
        {
            imei_d = otp_data->imei[iindex];
            *tmpptr_usbsn++ = ((imei_d & 0xf0) >> 4);

            imei_d = otp_data->imei[iindex + 1];
            *tmpptr_usbsn++ = (imei_d & 0xf);
        }
        *tmpptr_usbsn++ = ((otp_data->imei[8] & 0xf0) >> 4);

        sprintf(qisda_imei_serialnum,
                "%01x%01x%01x%01x%01x%01x%01x%01x%01x%01x%01x%01x%01x%01x%01x%01x",
                0,
                tmpusbsn[0], tmpusbsn[1],
                tmpusbsn[2], tmpusbsn[3],
                tmpusbsn[4], tmpusbsn[5],
                tmpusbsn[6], tmpusbsn[7],
                tmpusbsn[8], tmpusbsn[9],
                tmpusbsn[10], tmpusbsn[11],
                tmpusbsn[12], tmpusbsn[13],
                tmpusbsn[14]);

        printk ("board-qsd8x50_austinc %s: %s\n", __func__, qisda_imei_serialnum);
    }
}
#else
static void msm_get_usb_serial(void) {}
#endif

static void msm_hsusb_get_qctusb_driver(void)
{
    unsigned int rpc_arg2 = 0, rpc_arg1 = 0;
    int retValue = 0;

    rpc_arg1 = SMEM_PC_OEM_QCT_USB_DRIVER_STATUS;
    retValue = cust_mproc_comm1(&rpc_arg1, &rpc_arg2);

    if (retValue != 0)
    {
        printk("%s: proc_comm failed\n", __func__);
        qct_usbdiag_debugging = 0;
    }
    else if (rpc_arg2 == 0)
    {
        printk("%s: disable usb debugging functions\n", __func__);
        qct_usbdiag_debugging = 0;
    }
    else
    {
        printk("%s: enable usb debugging functions\n", __func__);
        qct_usbdiag_debugging = 1;
    }
    printk("Sonia::%s:qct_usbdiag_debugging=%d rpc_arg2=%d\n",__func__,qct_usbdiag_debugging,rpc_arg2);



#ifdef CONFIG_BUILDTYPE_SHIP
    rpc_arg1 = SMEM_PC_OEM_ADB_PERMISSION_STATUS;
    rpc_arg2 = 0;
    retValue = cust_mproc_comm1(&rpc_arg1, &rpc_arg2);
    if (retValue != 0)
    {
        printk("%s: proc_comm failed(build_ship)\n", __func__);
        adb_debugging = 0;
    }
    else if (rpc_arg2 == 0)
    {
        printk("%s: disable adb debugging functions(build ship) %d\n", __func__, rpc_arg2);
        adb_debugging = 0;
    }
    else
    {
        printk("%s: enable adb debugging functions %d(buuild_ship)\n", __func__, rpc_arg2);
        adb_debugging = 1;
    }
#else
    adb_debugging = 1;
#endif

    printk("%s: enable adb debugging functions adb_debugging %d\n", __func__,adb_debugging);
}

static void __init qsd8x50_init_irq(void)
{
	msm_init_irq();
	msm_init_sirc();
}

static void kgsl_phys_memory_init(void)
{
	request_mem_region(kgsl_resources[1].start,
		resource_size(&kgsl_resources[1]), "kgsl");
}

static void usb_mpp_init(void)
{
	unsigned rc;
	unsigned mpp_usb = 20;

	if (machine_is_qsd8x50_ffa()) {
		rc = mpp_config_digital_out(mpp_usb,
			MPP_CFG(MPP_DLOGIC_LVL_VDD,
				MPP_DLOGIC_OUT_CTRL_HIGH));
		if (rc)
			pr_err("%s: configuring mpp pin"
				"to enable 3.3V LDO failed\n", __func__);
	}
}

/* TBD: 8x50 FFAs have internal 3p3 voltage regulator as opposed to
 * external 3p3 voltage regulator on Surf platform. There is no way
 * s/w can detect fi concerned regulator is internal or external to
 * to MSM. Internal 3p3 regulator is powered through boost voltage
 * regulator where as external 3p3 regulator is powered through VPH.
 * So for internal voltage regulator it is required to power on
 * boost voltage regulator first. Unfortunately some of the FFAs are
 * re-worked to install external 3p3 regulator. For now, assuming all
 * FFAs have 3p3 internal regulators and all SURFs have external 3p3
 * regulator as there is no way s/w can determine if theregulator is
 * internal or external. May be, we can implement this flag as kernel
 * boot parameters so that we can change code behaviour dynamically
 */
static int regulator_3p3_is_internal;
static struct vreg *vreg_5v;
static struct vreg *vreg_3p3;
static int msm_hsusb_ldo_init(int init)
{
	if (init) {
		if (regulator_3p3_is_internal) {
			vreg_5v = vreg_get(NULL, "boost");
			if (IS_ERR(vreg_5v))
				return PTR_ERR(vreg_5v);
			vreg_set_level(vreg_5v, 5000);
		}

		vreg_3p3 = vreg_get(NULL, "usb");
		if (IS_ERR(vreg_3p3))
			return PTR_ERR(vreg_3p3);
		vreg_set_level(vreg_3p3, 3300);
	} else {
		if (regulator_3p3_is_internal)
			vreg_put(vreg_5v);
		vreg_put(vreg_3p3);
	}

	return 0;
}

static int msm_hsusb_ldo_enable(int enable)
{
	static int ldo_status;
	int ret;

	if (ldo_status == enable)
		return 0;

	if (regulator_3p3_is_internal && (!vreg_5v || IS_ERR(vreg_5v)))
		return -ENODEV;
	if (!vreg_3p3 || IS_ERR(vreg_3p3))
		return -ENODEV;

	ldo_status = enable;

	if (enable) {
		if (regulator_3p3_is_internal) {
			ret = vreg_enable(vreg_5v);
			if (ret)
				return ret;

			/* power supply to 3p3 regulator can vary from
			 * USB VBUS or VREG 5V. If the power supply is
			 * USB VBUS cable disconnection cannot be
			 * deteted. Select power supply to VREG 5V
			 */
			/* TBD: comeup with a better name */
			ret = pmic_vote_3p3_pwr_sel_switch(1);
			if (ret)
				return ret;
		}
		ret = vreg_enable(vreg_3p3);

		return ret;
	} else {
		if (regulator_3p3_is_internal) {
			ret = vreg_disable(vreg_5v);
			if (ret)
				return ret;
			ret = pmic_vote_3p3_pwr_sel_switch(0);
			if (ret)
				return ret;
		}
			ret = vreg_disable(vreg_3p3);

			return ret;
	}
}

static void __init qsd8x50_init_usb(void)
{
	usb_mpp_init();

	if (machine_is_qsd8x50_ffa())
		regulator_3p3_is_internal = 1;

#ifdef CONFIG_USB_MSM_OTG_72K
	platform_device_register(&msm_device_otg);
#endif

#ifdef CONFIG_USB_FUNCTION_MSM_HSUSB
	platform_device_register(&msm_device_hsusb_peripheral);
#endif

#ifdef CONFIG_USB_MSM_72K
	platform_device_register(&msm_device_gadget_peripheral);
#endif

	if (machine_is_qsd8x50_ffa() || machine_is_qsd8x50a_ffa())
		return;

	vreg_usb = vreg_get(NULL, "boost");

	if (IS_ERR(vreg_usb)) {
		printk(KERN_ERR "%s: vreg get failed (%ld)\n",
		       __func__, PTR_ERR(vreg_usb));
		return;
	}

	platform_device_register(&msm_device_hsusb_otg);
	msm_add_host(0, &msm_usb_host_pdata);
#ifdef CONFIG_USB_FS_HOST
	if (fsusb_gpio_init())
		return;
	msm_add_host(1, &msm_usb_host2_pdata);
#endif
#ifdef CONFIG_MUTEKEY
	platform_device_register(&mutekey_device);
#endif
}

static struct vreg *vreg_mmc;

#if (defined(CONFIG_MMC_MSM_SDC1_SUPPORT)\
	|| defined(CONFIG_MMC_MSM_SDC2_SUPPORT)\
	|| defined(CONFIG_MMC_MSM_SDC3_SUPPORT)\
	|| defined(CONFIG_MMC_MSM_SDC4_SUPPORT))

#if 0
struct sdcc_gpio {
	struct msm_gpio *cfg_data;
	uint32_t size;
};

static struct msm_gpio sdc1_cfg_data[] = {
	{GPIO_CFG(51, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), "sdc1_dat_3"},
	{GPIO_CFG(52, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), "sdc1_dat_2"},
	{GPIO_CFG(53, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), "sdc1_dat_1"},
	{GPIO_CFG(54, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), "sdc1_dat_0"},
	{GPIO_CFG(55, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), "sdc1_cmd"},
	{GPIO_CFG(56, 1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), "sdc1_clk"},
};

static struct msm_gpio sdc2_cfg_data[] = {
	{GPIO_CFG(62, 1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), "sdc2_clk"},
	{GPIO_CFG(63, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), "sdc2_cmd"},
	{GPIO_CFG(64, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), "sdc2_dat_3"},
	{GPIO_CFG(65, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), "sdc2_dat_2"},
	{GPIO_CFG(66, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), "sdc2_dat_1"},
	{GPIO_CFG(67, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), "sdc2_dat_0"},
};

static struct msm_gpio sdc3_cfg_data[] = {
	{GPIO_CFG(88, 1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), "sdc3_clk"},
	{GPIO_CFG(89, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), "sdc3_cmd"},
	{GPIO_CFG(90, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), "sdc3_dat_3"},
	{GPIO_CFG(91, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), "sdc3_dat_2"},
	{GPIO_CFG(92, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), "sdc3_dat_1"},
	{GPIO_CFG(93, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), "sdc3_dat_0"},

#ifdef CONFIG_MMC_MSM_SDC3_8_BIT_SUPPORT
	{GPIO_CFG(158, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), "sdc3_dat_4"},
	{GPIO_CFG(159, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), "sdc3_dat_5"},
	{GPIO_CFG(160, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), "sdc3_dat_6"},
	{GPIO_CFG(161, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), "sdc3_dat_7"},
#endif
};

static struct msm_gpio sdc4_cfg_data[] = {
	{GPIO_CFG(142, 3, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), "sdc4_clk"},
	{GPIO_CFG(143, 3, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), "sdc4_cmd"},
	{GPIO_CFG(144, 2, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), "sdc4_dat_0"},
	{GPIO_CFG(145, 2, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), "sdc4_dat_1"},
	{GPIO_CFG(146, 3, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), "sdc4_dat_2"},
	{GPIO_CFG(147, 3, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), "sdc4_dat_3"},
};

static struct sdcc_gpio sdcc_cfg_data[] = {
	{
		.cfg_data = sdc1_cfg_data,
		.size = ARRAY_SIZE(sdc1_cfg_data),
	},
	{
		.cfg_data = sdc2_cfg_data,
		.size = ARRAY_SIZE(sdc2_cfg_data),
	},
	{
		.cfg_data = sdc3_cfg_data,
		.size = ARRAY_SIZE(sdc3_cfg_data),
	},
	{
		.cfg_data = sdc4_cfg_data,
		.size = ARRAY_SIZE(sdc4_cfg_data),
	},
};
#endif

#if 0
static void msm_sdcc_setup_gpio(int dev_id, unsigned int enable)
{
	int rc = 0;
	struct sdcc_gpio *curr;

	curr = &sdcc_cfg_data[dev_id - 1];
	if (!(test_bit(dev_id, &gpio_sts)^enable))
		return;

	if (enable) {
		set_bit(dev_id, &gpio_sts);
		rc = msm_gpios_request_enable(curr->cfg_data, curr->size);
		if (rc)
			printk(KERN_ERR "%s: Failed to turn on GPIOs for slot %d\n",
				__func__,  dev_id);
	} else {
		clear_bit(dev_id, &gpio_sts);
		msm_gpios_disable_free(curr->cfg_data, curr->size);
	}
}

static uint32_t msm_sdcc_setup_power(struct device *dv, unsigned int vdd)
{
	int rc = 0;
	struct platform_device *pdev;

	pdev = container_of(dv, struct platform_device, dev);
	msm_sdcc_setup_gpio(pdev->id, !!vdd);

	if (vdd == 0) {
		if (!vreg_sts)
			return 0;

		clear_bit(pdev->id, &vreg_sts);

		if (!vreg_sts) {
			rc = vreg_disable(vreg_mmc);
			if (rc)
				printk(KERN_ERR "%s: return val: %d \n",
					__func__, rc);
		}
		return 0;
	}

	if (!vreg_sts) {
		rc = vreg_set_level(vreg_mmc, PMIC_VREG_GP6_LEVEL);
		if (!rc)
			rc = vreg_enable(vreg_mmc);
		if (rc)
			printk(KERN_ERR "%s: return val: %d \n",
					__func__, rc);
	}
	set_bit(pdev->id, &vreg_sts);
	return 0;
}
#endif

#endif

#if (defined(CONFIG_MMC_MSM_SDC1_SUPPORT)\
	|| defined(CONFIG_MMC_MSM_SDC2_SUPPORT)\
	|| defined(CONFIG_MMC_MSM_SDC4_SUPPORT))

static int msm_sdcc_get_wpswitch(struct device *dv)
{
	void __iomem *wp_addr = 0;
	uint32_t ret = 0;
	struct platform_device *pdev;

	if (!(machine_is_qsd8x50_surf() || machine_is_qsd8x50a_surf()))
		return -1;

	pdev = container_of(dv, struct platform_device, dev);

	wp_addr = ioremap(FPGA_SDCC_STATUS, 4);
	if (!wp_addr) {
		pr_err("%s: Could not remap %x\n", __func__, FPGA_SDCC_STATUS);
		return -ENOMEM;
	}

	ret = (readl(wp_addr) >> ((pdev->id - 1) << 1)) & (0x03);
	pr_info("%s: WP/CD Status for Slot %d = 0x%x \n", __func__,
							pdev->id, ret);
	iounmap(wp_addr);
	return ((ret == 0x02) ? 1 : 0);

}
#endif

#ifdef CONFIG_MMC_MSM_SDC1_SUPPORT
static struct mmc_platform_data qsd8x50_sdc1_data = {
	.ocr_mask	= MMC_VDD_27_28 | MMC_VDD_28_29,
	.mmc_bus_width  = MMC_CAP_4_BIT_DATA,
	.wpswitch	= msm_sdcc_get_wpswitch,
#ifdef CONFIG_MMC_MSM_SDC1_DUMMY52_REQUIRED
	.dummy52_required = 1,
#endif
	.msmsdcc_fmin	= 144000,
	.msmsdcc_fmid	= 25000000,
	.msmsdcc_fmax	= 49152000,
	.nonremovable	= 0,
};
#endif

#ifdef CONFIG_MMC_MSM_SDC2_SUPPORT
static struct mmc_platform_data qsd8x50_sdc2_data = {
	.ocr_mask       = MMC_VDD_27_28 | MMC_VDD_28_29,
	.mmc_bus_width  = MMC_CAP_4_BIT_DATA,
	.wpswitch	= msm_sdcc_get_wpswitch,
#ifdef CONFIG_MMC_MSM_SDC2_DUMMY52_REQUIRED
	.dummy52_required = 1,
#endif
	.msmsdcc_fmin	= 144000,
	.msmsdcc_fmid	= 25000000,
	.msmsdcc_fmax	= 49152000,
	.nonremovable	= 1,
};
#endif

#ifdef CONFIG_MMC_MSM_SDC3_SUPPORT
static struct mmc_platform_data qsd8x50_sdc3_data = {
	.ocr_mask       = MMC_VDD_27_28 | MMC_VDD_28_29,
#ifdef CONFIG_MMC_MSM_SDC3_8_BIT_SUPPORT
	.mmc_bus_width  = MMC_CAP_8_BIT_DATA,
#else
	.mmc_bus_width  = MMC_CAP_4_BIT_DATA,
#endif
#ifdef CONFIG_MMC_MSM_SDC3_DUMMY52_REQUIRED
	.dummy52_required = 1,
#endif
	.status_irq	= MSM_GPIO_TO_INT(SLOT3_CARD_DETECT_GPIO),
	.msmsdcc_fmin	= 144000,
	.msmsdcc_fmid	= 25000000,
	.msmsdcc_fmax	= 49152000,
	.nonremovable	= 0,
};
#endif

#ifdef CONFIG_MMC_MSM_SDC4_SUPPORT
static struct mmc_platform_data qsd8x50_sdc4_data = {
	.ocr_mask       = MMC_VDD_27_28 | MMC_VDD_28_29,
	.mmc_bus_width  = MMC_CAP_4_BIT_DATA,
	.wpswitch	= msm_sdcc_get_wpswitch,
#ifdef CONFIG_MMC_MSM_SDC4_DUMMY52_REQUIRED
	.dummy52_required = 1,
#endif
	.msmsdcc_fmin	= 144000,
	.msmsdcc_fmid	= 25000000,
	.msmsdcc_fmax	= 49152000,
	.nonremovable	= 0,
};
#endif

static void __init qsd8x50_init_mmc(void)
{
	vreg_mmc = vreg_get(NULL, "gp6");

	if (IS_ERR(vreg_mmc)) {
		printk(KERN_ERR "%s: vreg get failed (%ld)\n",
		       __func__, PTR_ERR(vreg_mmc));
		return;
	}
#ifdef CONFIG_MMC_MSM_SDC3_SUPPORT
	msm_add_sdcc(3, &qsd8x50_sdc3_data);
#endif
#ifdef CONFIG_MMC_MSM_SDC2_SUPPORT
	msm_add_sdcc(2, &qsd8x50_sdc2_data);
#endif
#ifdef CONFIG_MMC_MSM_SDC1_SUPPORT
	msm_add_sdcc(1, &qsd8x50_sdc1_data);
#endif
#ifdef CONFIG_MMC_MSM_SDC4_SUPPORT
	msm_add_sdcc(4, &qsd8x50_sdc4_data);
#endif
}

#if 0
static void __init qsd8x50_cfg_smc91x(void)
{
	int rc = 0;

	if (machine_is_qsd8x50_surf() || machine_is_qsd8x50a_surf()) {
		smc91x_resources[0].start = 0x70000300;
		smc91x_resources[0].end = 0x700003ff;
		smc91x_resources[1].start = MSM_GPIO_TO_INT(156);
		smc91x_resources[1].end = MSM_GPIO_TO_INT(156);
	} else if (machine_is_qsd8x50_ffa() || machine_is_qsd8x50a_ffa()) {
		smc91x_resources[0].start = 0x84000300;
		smc91x_resources[0].end = 0x840003ff;
		smc91x_resources[1].start = MSM_GPIO_TO_INT(87);
		smc91x_resources[1].end = MSM_GPIO_TO_INT(87);

		rc = gpio_tlmm_config(GPIO_CFG(87, 0, GPIO_CFG_INPUT,
					       GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA),
					       GPIO_CFG_ENABLE);
		if (rc) {
			printk(KERN_ERR "%s: gpio_tlmm_config=%d\n",
					__func__, rc);
		}
	} else
		printk(KERN_ERR "%s: invalid machine type\n", __func__);
}
#endif

static struct msm_pm_platform_data msm_pm_data[MSM_PM_SLEEP_MODE_NR] = {
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE].supported = 1,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE].suspend_enabled = 1,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE].idle_enabled = 1,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE].latency = 8594,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE].residency = 23740,

	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_NO_XO_SHUTDOWN].supported = 1,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_NO_XO_SHUTDOWN].suspend_enabled = 1,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_NO_XO_SHUTDOWN].idle_enabled = 1,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_NO_XO_SHUTDOWN].latency = 4594,
	[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_NO_XO_SHUTDOWN].residency = 23740,

	[MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT].supported = 1,
	[MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT].suspend_enabled
		= 1,
	[MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT].idle_enabled = 0,
	[MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT].latency = 443,
	[MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT].residency = 1098,

	[MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT].supported = 1,
	[MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT].suspend_enabled = 1,
	[MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT].idle_enabled = 1,
	[MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT].latency = 2,
	[MSM_PM_SLEEP_MODE_WAIT_FOR_INTERRUPT].residency = 0,
};

static void
msm_i2c_gpio_config(int iface, int config_type)
{
	int gpio_scl;
	int gpio_sda;
	if (iface) {
		gpio_scl = 60;
		gpio_sda = 61;
	} else {
		gpio_scl = 95;
		gpio_sda = 96;
	}
	if (config_type) {
		gpio_tlmm_config(GPIO_CFG(gpio_scl, 1, GPIO_CFG_INPUT,
					GPIO_CFG_NO_PULL, GPIO_CFG_16MA), GPIO_CFG_ENABLE);
		gpio_tlmm_config(GPIO_CFG(gpio_sda, 1, GPIO_CFG_INPUT,
					GPIO_CFG_NO_PULL, GPIO_CFG_16MA), GPIO_CFG_ENABLE);
	} else {
		gpio_tlmm_config(GPIO_CFG(gpio_scl, 0, GPIO_CFG_OUTPUT,
					GPIO_CFG_NO_PULL, GPIO_CFG_16MA), GPIO_CFG_ENABLE);
		gpio_tlmm_config(GPIO_CFG(gpio_sda, 0, GPIO_CFG_OUTPUT,
					GPIO_CFG_NO_PULL, GPIO_CFG_16MA), GPIO_CFG_ENABLE);
	}
}

static struct msm_i2c_platform_data msm_i2c_pdata = {
	.clk_freq = 100000,
	.rsl_id = SMEM_SPINLOCK_I2C,
	.pri_clk = 95,
	.pri_dat = 96,
	.aux_clk = 60,
	.aux_dat = 61,
	.msm_i2c_config_gpio = msm_i2c_gpio_config,
};

static void __init msm_device_i2c_init(void)
{
	if (gpio_request(95, "i2c_pri_clk"))
		pr_err("failed to request gpio i2c_pri_clk\n");
	if (gpio_request(96, "i2c_pri_dat"))
		pr_err("failed to request gpio i2c_pri_dat\n");
	if (gpio_request(60, "i2c_sec_clk"))
		pr_err("failed to request gpio i2c_sec_clk\n");
	if (gpio_request(61, "i2c_sec_dat"))
		pr_err("failed to request gpio i2c_sec_dat\n");

	msm_i2c_pdata.rmutex = (uint32_t)smem_alloc(SMEM_I2C_MUTEX, 8); // needed in K35
	msm_i2c_pdata.pm_lat =
		msm_pm_data[MSM_PM_SLEEP_MODE_POWER_COLLAPSE_NO_XO_SHUTDOWN]
		.latency;
	msm_device_i2c.dev.platform_data = &msm_i2c_pdata;
}

static unsigned pmem_kernel_ebi1_size = PMEM_KERNEL_EBI1_SIZE;
static int __init pmem_kernel_ebi1_size_setup(char *p)
{
	pmem_kernel_ebi1_size = memparse(p, NULL);
	return 0;
}
early_param("pmem_kernel_ebi1_size", pmem_kernel_ebi1_size_setup);

#ifdef CONFIG_KERNEL_PMEM_SMI_REGION
static unsigned pmem_kernel_smi_size = MSM_PMEM_SMIPOOL_SIZE;
static int __init pmem_kernel_smi_size_setup(char *p)
{
	pmem_kernel_smi_size = memparse(p, NULL);

	/* Make sure that we don't allow more SMI memory then is
	   available - the kernel mapping code has no way of knowing
	   if it has gone over the edge */

	if (pmem_kernel_smi_size > MSM_PMEM_SMIPOOL_SIZE)
		pmem_kernel_smi_size = MSM_PMEM_SMIPOOL_SIZE;
	return 0;
}
early_param("pmem_kernel_smi_size", pmem_kernel_smi_size_setup);
#endif

static unsigned pmem_sf_size = MSM_PMEM_SF_SIZE;
static int __init pmem_sf_size_setup(char *p)
{
	pmem_sf_size = memparse(p, NULL);
	return 0;
}
early_param("pmem_sf_size", pmem_sf_size_setup);

static unsigned pmem_adsp_size = MSM_PMEM_ADSP_SIZE;
static int __init pmem_adsp_size_setup(char *p)
{
	pmem_adsp_size = memparse(p, NULL);
	return 0;
}
early_param("pmem_adsp_size", pmem_adsp_size_setup);


static unsigned audio_size = MSM_AUDIO_SIZE;
static int __init audio_size_setup(char *p)
{
	audio_size = memparse(p, NULL);
	return 0;
}
early_param("audio_size", audio_size_setup);

static void __init qsd8x50_init(void)
{
	if (socinfo_init() < 0)
		printk(KERN_ERR "%s: socinfo_init() failed!\n",
		       __func__);
	msm_clock_init(msm_clocks_8x50, msm_num_clocks_8x50);

	platform_add_devices(early_devices, ARRAY_SIZE(early_devices));
#ifdef CONFIG_SMC91X
	qsd8x50_cfg_smc91x();
#endif
	msm_acpu_clock_init(&qsd8x50_clock_data);

	msm_hsusb_pdata.swfi_latency =
		msm_pm_data
		[MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT].latency;
	msm_device_hsusb_peripheral.dev.platform_data = &msm_hsusb_pdata;

	msm_otg_pdata.swfi_latency =
		msm_pm_data
		[MSM_PM_SLEEP_MODE_RAMP_DOWN_AND_WAIT_FOR_INTERRUPT].latency;
	msm_device_otg.dev.platform_data = &msm_otg_pdata;
	msm_device_gadget_peripheral.dev.platform_data = &msm_gadget_pdata;
	msm_gadget_pdata.is_phy_status_timer_on = 1;

	msm_get_usb_serial();
	msm_hsusb_get_qctusb_driver();

#if defined(CONFIG_TSIF) || defined(CONFIG_TSIF_MODULE)
	msm_device_tsif.dev.platform_data = &tsif_platform_data;
#endif
	platform_add_devices(devices, ARRAY_SIZE(devices));
        {
                platform_device_register(&msm_device_uart1);
                                printk(KERN_ERR "[SEAN]UART1 Enable\n");
                                gpio_tlmm_config(GPIO_CFG(45, 1, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),GPIO_CFG_ENABLE);      // UART1 RX
                                gpio_tlmm_config(GPIO_CFG(46, 1, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),GPIO_CFG_ENABLE);     // UART1 TX
#if 0
                {
                                gpio_tlmm_config(GPIO_CFG(45, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),GPIO_CFG_ENABLE);      // UART1 RX
                                gpio_tlmm_config(GPIO_CFG(46, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA),GPIO_CFG_ENABLE);      // UART1 TX
                }
#endif
        }

	msm_fb_add_devices();
#ifdef CONFIG_MSM_CAMERA
	config_camera_off_gpios();
#endif
	qsd8x50_init_usb();
	qsd8x50_init_mmc();
	bt_power_init();
	msm_device_i2c_init();
	msm_qsd_spi_init();
	i2c_register_board_info(0, msm_i2c_board_info,
				ARRAY_SIZE(msm_i2c_board_info));
	spi_register_board_info(msm_spi_board_info,
				ARRAY_SIZE(msm_spi_board_info));
	msm_pm_set_platform_data(msm_pm_data, ARRAY_SIZE(msm_pm_data));
	kgsl_phys_memory_init();

#ifdef CONFIG_SURF_FFA_GPIO_KEYPAD
	if (machine_is_qsd8x50_ffa() || machine_is_qsd8x50a_ffa())
		platform_device_register(&keypad_device_8k_ffa);
	else
		platform_device_register(&keypad_device_surf);
#endif
}

static void __init qsd8x50_allocate_memory_regions(void)
{
	void *addr;
	unsigned long size;

	size = pmem_kernel_ebi1_size;
	if (size) {
		addr = alloc_bootmem_aligned(size, 0x100000);
		android_pmem_kernel_ebi1_pdata.start = __pa(addr);
		android_pmem_kernel_ebi1_pdata.size = size;
		pr_info("allocating %lu bytes at %p (%lx physical) for kernel"
			" ebi1 pmem arena\n", size, addr, __pa(addr));
	}

#ifdef CONFIG_KERNEL_PMEM_SMI_REGION
	size = pmem_kernel_smi_size;
	if (size > MSM_PMEM_SMIPOOL_SIZE) {
		printk(KERN_ERR "pmem kernel smi arena size %lu is too big\n",
			size);

		size = MSM_PMEM_SMIPOOL_SIZE;
	}

	android_pmem_kernel_smi_pdata.start = MSM_PMEM_SMIPOOL_BASE;
	android_pmem_kernel_smi_pdata.size = size;

	pr_info("allocating %lu bytes at %lx (%lx physical)"
		"for pmem kernel smi arena\n", size,
		(long unsigned int) MSM_PMEM_SMIPOOL_BASE,
		__pa(MSM_PMEM_SMIPOOL_BASE));
#endif

	size = pmem_sf_size;
	if (size) {
		addr = alloc_bootmem(size);
		android_pmem_pdata.start = __pa(addr);
		android_pmem_pdata.size = size;
		pr_info("allocating %lu bytes at %p (%lx physical) for sf "
			"pmem arena\n", size, addr, __pa(addr));
	}

	size = pmem_adsp_size;
	if (size) {
		addr = alloc_bootmem(size);
		android_pmem_adsp_pdata.start = __pa(addr);
		android_pmem_adsp_pdata.size = size;
		pr_info("allocating %lu bytes at %p (%lx physical) for adsp "
			"pmem arena\n", size, addr, __pa(addr));
	}


	size = MSM_FB_SIZE;
	addr = (void *)MSM_FB_BASE;
	msm_fb_resources[0].start = (unsigned long)addr;
	msm_fb_resources[0].end = msm_fb_resources[0].start + size - 1;
	pr_info("using %lu bytes of SMI at %lx physical for fb\n",
	       size, (unsigned long)addr);

#ifdef CONFIG_HDMI_BUGFIX
	addr = alloc_bootmem_aligned(MSM_FBNEW_SIZE,0x100000);
	msm_fb_resources[1].start = __pa(addr);
	msm_fb_resources[1].end = __pa(addr) + MSM_FBNEW_SIZE - 1;
	pr_info("using %lu bytes of SMI at %lx physical for fb\n",
	       (unsigned long) MSM_FBNEW_SIZE, __pa(addr));
#endif

	size = audio_size ? : MSM_AUDIO_SIZE;
	addr = alloc_bootmem(size);
	msm_audio_resources[0].start = __pa(addr);
	msm_audio_resources[0].end = msm_audio_resources[0].start + size - 1;
	pr_info("allocating %lu bytes at %p (%lx physical) for audio\n",
		size, addr, __pa(addr));

#ifdef CONFIG_WLAN_ALLOC_STATIC_MEM
	addr_4329_dhd_prot	= alloc_bootmem(ADDR_4329_DHD_PORT_LEN);
	addr_4329_bus_rxbuf	= alloc_bootmem(ADDR_4329_BUS_RXBUF_LEN);
	addr_4329_bus_databuf	= alloc_bootmem(ADDR_4329_BUS_DATABUF_LEN);
        printk(KERN_CRIT "dhd_port=0x%p,bus_rxbuf=0x%p,bus_databuf=0x%p\n",
	      		addr_4329_dhd_prot,addr_4329_bus_rxbuf,addr_4329_bus_databuf);
#endif
}

static void __init qsd8x50_map_io(void)
{
	msm_shared_ram_phys = MSM_SHARED_RAM_PHYS;
	msm_map_qsd8x50_io();
	qsd8x50_allocate_memory_regions();
}

MACHINE_START(QSD8X50_SURF, "QCT QSD8X50 SURF")
#ifdef CONFIG_MSM_DEBUG_UART
	.phys_io  = MSM_DEBUG_UART_PHYS,
	.io_pg_offst = ((MSM_DEBUG_UART_BASE) >> 18) & 0xfffc,
#endif
	.boot_params = PHYS_OFFSET + 0x100,
	.map_io = qsd8x50_map_io,
	.init_irq = qsd8x50_init_irq,
	.init_machine = qsd8x50_init,
	.timer = &msm_timer,
MACHINE_END

MACHINE_START(QSD8X50_FFA, "QCT QSD8X50 FFA")
#ifdef CONFIG_MSM_DEBUG_UART
	.phys_io  = MSM_DEBUG_UART_PHYS,
	.io_pg_offst = ((MSM_DEBUG_UART_BASE) >> 18) & 0xfffc,
#endif
	.boot_params = PHYS_OFFSET + 0x100,
	.map_io = qsd8x50_map_io,
	.init_irq = qsd8x50_init_irq,
	.init_machine = qsd8x50_init,
	.timer = &msm_timer,
MACHINE_END

MACHINE_START(QSD8X50A_SURF, "QCT QSD8X50A SURF")
#ifdef CONFIG_MSM_DEBUG_UART
	.phys_io  = MSM_DEBUG_UART_PHYS,
	.io_pg_offst = ((MSM_DEBUG_UART_BASE) >> 18) & 0xfffc,
#endif
	.boot_params = PHYS_OFFSET + 0x100,
	.map_io = qsd8x50_map_io,
	.init_irq = qsd8x50_init_irq,
	.init_machine = qsd8x50_init,
	.timer = &msm_timer,
MACHINE_END

MACHINE_START(QSD8X50A_FFA, "QCT QSD8X50A FFA")
#ifdef CONFIG_MSM_DEBUG_UART
	.phys_io  = MSM_DEBUG_UART_PHYS,
	.io_pg_offst = ((MSM_DEBUG_UART_BASE) >> 18) & 0xfffc,
#endif
	.boot_params = PHYS_OFFSET + 0x100,
	.map_io = qsd8x50_map_io,
	.init_irq = qsd8x50_init_irq,
	.init_machine = qsd8x50_init,
	.timer = &msm_timer,
MACHINE_END
