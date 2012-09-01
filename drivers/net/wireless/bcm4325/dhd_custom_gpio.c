/*
* Customer code to add GPIO control during WLAN start/stop
* Copyright (C) 1999-2009, Broadcom Corporation
* 
*         Unless you and Broadcom execute a separate written software license
* agreement governing use of this software, this software is licensed to you
* under the terms of the GNU General Public License version 2 (the "GPL"),
* available at http://www.broadcom.com/licenses/GPLv2.php, with the
* following added to such license:
* 
*      As a special exception, the copyright holders of this software give you
* permission to link this software with independent modules, and to copy and
* distribute the resulting executable under terms of your choice, provided that
* you also meet, for each linked independent module, the terms and conditions of
* the license of that module.  An independent module is a module which is not
* derived from this software.  The special exception does not apply to any
* modifications of the software.
* 
*      Notwithstanding the above, under no circumstances may you combine this
* software in any way with any other Broadcom software provided under a license
* other than the GPL, without Broadcom's express prior written consent.
*
* $Id: dhd_custom_gpio.c,v 1.1.2.7 2009/11/06 11:18:43 Exp $
*/

#include <typedefs.h>
#include <linuxver.h>
#include <osl.h>
#include <bcmutils.h>

#include <dngl_stats.h>
#include <dhd.h>
#include <asm/gpio.h>
#include <mach/pm_log.h>
#include <linux/mmc/host.h>
#include <linux/mutex.h>

#include <wlioctl.h>
#include <wl_iw.h>

#define WL_ERROR(x) printf x
#define WL_TRACE(x)
extern void mmc_detect_change(struct mmc_host *host, unsigned long delay);
extern struct mmc_host *sdcc2_mmcptr;

#ifdef CUSTOMER_HW
extern  void bcm_wlan_power_off(int);
extern  void bcm_wlan_power_on(int);
#endif /* CUSTOMER_HW */

#if defined(OOB_INTR_ONLY)

#ifdef CUSTOMER_HW3
#include <mach/gpio.h>
#endif

/* Customer specific Host GPIO defintion  */
static int dhd_oob_gpio_num = -1; /* GG 19 */

module_param(dhd_oob_gpio_num, int, 0644);
MODULE_PARM_DESC(dhd_oob_gpio_num, "DHD oob gpio number");

int dhd_customer_oob_irq_map(void)
{
int  host_oob_irq = 0;
#if defined(CUSTOM_OOB_GPIO_NUM)
	if (dhd_oob_gpio_num < 0) {
		dhd_oob_gpio_num = CUSTOM_OOB_GPIO_NUM;
	}
#endif

	if (dhd_oob_gpio_num < 0) {
		WL_ERROR(("%s: ERROR customer specific Host GPIO is NOT defined \n",
		__FUNCTION__));
		return (dhd_oob_gpio_num);
	}

	WL_ERROR(("%s: customer specific Host GPIO number is (%d)\n",
	         __FUNCTION__, dhd_oob_gpio_num));

#if defined CUSTOMER_HW
	host_oob_irq = MSM_GPIO_TO_INT(dhd_oob_gpio_num);
#elif defined CUSTOMER_HW3
	gpio_request(dhd_oob_gpio_num, "oob irq");
	host_oob_irq = gpio_to_irq(dhd_oob_gpio_num);
	gpio_direction_input(dhd_oob_gpio_num);
#endif

	return (host_oob_irq);
}
#endif /* defined(OOB_INTR_ONLY) */

#ifndef CONFIG_DSC_WIFI_LOCK
extern struct mutex wl_gpio_lock;
#endif

/* Customer function to control hw specific wlan gpios */
void
dhd_customer_gpio_wlan_ctrl(int onoff)
{
#ifndef CONFIG_DSC_WIFI_LOCK
	mutex_lock(&wl_gpio_lock);
#endif
	switch (onoff) {
		case WLAN_RESET_OFF:
			WL_TRACE(("%s: call customer specific GPIO to insert WLAN RESET\n",
				__FUNCTION__));
#ifdef CUSTOMER_HW
			bcm_wlan_power_off(2);
#endif /* CUSTOMER_HW */
#if 1

#if defined(CONFIG_MACH_EVT0) || defined(CONFIG_MACH_EVT0_1) ||defined( CONFIG_MACH_EVB)
        gpio_set_value(78,0);
#elif defined CONFIG_MACH_EVT1
	 gpio_set_value(76,0);
#else
        gpio_set_value(76,0);
#endif

        mdelay(100);
#if defined(CONFIG_MACH_EVT0) || defined(CONFIG_MACH_EVT0_1) ||defined( CONFIG_MACH_EVB)
	gpio_set_value(78,1);
#elif defined CONFIG_MACH_EVT1
	gpio_set_value(76,1);
#else
       gpio_set_value(76,1);
#endif

        mdelay(100);

#if defined(CONFIG_MACH_EVT0) || defined(CONFIG_MACH_EVT0_1) ||defined( CONFIG_MACH_EVB)
        gpio_set_value(78,0);
#elif defined CONFIG_MACH_EVT1
	 gpio_set_value(76,0);
#else
        gpio_set_value(76,0);
#endif

       mdelay(100);


#if defined(CONFIG_MACH_EVT0) || defined(CONFIG_MACH_EVT0_1) || defined(CONFIG_MACH_EVT1)
       gpio_set_value(147,0);
#elif defined CONFIG_MACH_EVB
       gpio_set_value(142,0);
#else
       gpio_set_value(147,0);
#endif
	 // mmc_detect_change(sdcc2_mmcptr, 0);
#endif

			WL_ERROR(("=========== WLAN placed in RESET ========\n"));
		break;

		case WLAN_RESET_ON:
			WL_TRACE(("%s: callc customer specific GPIO to remove WLAN RESET\n",
				__FUNCTION__));
#ifdef CUSTOMER_HW
			bcm_wlan_power_on(2);
#endif /* CUSTOMER_HW */
#if 1
        gpio_tlmm_config(GPIO_CFG(62,1,GPIO_CFG_OUTPUT,GPIO_CFG_NO_PULL,GPIO_CFG_8MA),GPIO_CFG_ENABLE);
        gpio_tlmm_config(GPIO_CFG(63,1,GPIO_CFG_OUTPUT,GPIO_CFG_PULL_UP,GPIO_CFG_8MA),GPIO_CFG_ENABLE);
        gpio_tlmm_config(GPIO_CFG(64,1,GPIO_CFG_OUTPUT,GPIO_CFG_PULL_UP,GPIO_CFG_4MA),GPIO_CFG_ENABLE);
        gpio_tlmm_config(GPIO_CFG(65,1,GPIO_CFG_OUTPUT,GPIO_CFG_PULL_UP,GPIO_CFG_4MA),GPIO_CFG_ENABLE);
        gpio_tlmm_config(GPIO_CFG(66,1,GPIO_CFG_OUTPUT,GPIO_CFG_PULL_UP,GPIO_CFG_4MA),GPIO_CFG_ENABLE);
        gpio_tlmm_config(GPIO_CFG(67,1,GPIO_CFG_OUTPUT,GPIO_CFG_PULL_UP,GPIO_CFG_4MA),GPIO_CFG_ENABLE);
        gpio_tlmm_config(GPIO_CFG(94,0,GPIO_CFG_INPUT,GPIO_CFG_NO_PULL,GPIO_CFG_4MA),GPIO_CFG_ENABLE);

#if defined(CONFIG_MACH_EVT0) || defined(CONFIG_MACH_EVT0_1) || defined(CONFIG_MACH_EVT1)
        gpio_tlmm_config(GPIO_CFG(147,0,GPIO_CFG_OUTPUT,GPIO_CFG_PULL_DOWN,GPIO_CFG_2MA),GPIO_CFG_ENABLE);
#elif defined CONFIG_MACH_EVB
        gpio_tlmm_config(GPIO_CFG(142,0,GPIO_CFG_OUTPUT,GPIO_CFG_PULL_DOWN,GPIO_CFG_2MA),GPIO_CFG_ENABLE);
#else
        gpio_tlmm_config(GPIO_CFG(147,0,GPIO_CFG_OUTPUT,GPIO_CFG_PULL_DOWN,GPIO_CFG_2MA),GPIO_CFG_ENABLE);
#endif

#if defined(CONFIG_MACH_EVT0) || defined(CONFIG_MACH_EVT0_1) ||defined( CONFIG_MACH_EVB)
        gpio_tlmm_config(GPIO_CFG(78,0,GPIO_CFG_OUTPUT,GPIO_CFG_PULL_DOWN,GPIO_CFG_2MA),GPIO_CFG_ENABLE);
#elif defined CONFIG_MACH_EVT1
        gpio_tlmm_config(GPIO_CFG(76,0,GPIO_CFG_OUTPUT,GPIO_CFG_PULL_DOWN,GPIO_CFG_2MA),GPIO_CFG_ENABLE);
#else
        gpio_tlmm_config(GPIO_CFG(76,0,GPIO_CFG_OUTPUT,GPIO_CFG_PULL_DOWN,GPIO_CFG_2MA),GPIO_CFG_ENABLE);
#endif

#if !(defined(CONFIG_MACH_EVB))
        gpio_tlmm_config(GPIO_CFG(30,0,GPIO_CFG_OUTPUT,GPIO_CFG_PULL_DOWN,GPIO_CFG_2MA),GPIO_CFG_ENABLE);
#endif

#if defined(CONFIG_MACH_EVT0) || defined(CONFIG_MACH_EVT0_1) || defined(CONFIG_MACH_EVT1)
        gpio_set_value(147,1); //WLAN_EN
#elif defined CONFIG_MACH_EVB
        gpio_set_value(142,1);
#else
        gpio_set_value(147,1);
#endif 
	 
        mdelay(100);
	 
#if defined(CONFIG_MACH_EVT0) || defined(CONFIG_MACH_EVT0_1) ||defined( CONFIG_MACH_EVB)
       gpio_set_value(78,1); //WLAN_RST_N
#elif defined CONFIG_MACH_EVT1
	 gpio_set_value(76,1);
#else
        gpio_set_value(76,1);
#endif 

        PM_LOG_EVENT(PM_LOG_ON,PM_LOG_WIFI);

#if !(defined(CONFIG_MACH_EVB))

        if(gpio_get_value(29)==0){
             gpio_set_value(27,1); //BT 100ms pulse
             printk("WLAN init: bt pulse start **************\n");
             mdelay(100);
             gpio_set_value(27,0); //BT 100ms pulse
             printk("WLAN init: bt pulse done ***************\n");
        }

#endif


#if !(defined(CONFIG_MACH_EVB))
        gpio_set_value(30,1); //WLAN_WAKE
        mdelay(100);
#endif 

#endif
        // mmc_detect_change(sdcc2_mmcptr, 0);
    

			WL_ERROR(("=========== WLAN going back to live  ========\n"));
		break;

		case WLAN_POWER_OFF:
			WL_TRACE(("%s: call customer specific GPIO to turn off WL_REG_ON\n",
				__FUNCTION__));
#ifdef CUSTOMER_HW
			bcm_wlan_power_off(1);
#endif /* CUSTOMER_HW */
		break;

		case WLAN_POWER_ON:
			WL_TRACE(("%s: call customer specific GPIO to turn on WL_REG_ON\n",
				__FUNCTION__));
#ifdef CUSTOMER_HW
			bcm_wlan_power_on(1);
#endif /* CUSTOMER_HW */
			/* Lets customer power to get stable */
			OSL_DELAY(500);
		break;
	}
#ifndef CONFIG_DSC_WIFI_LOCK
	mutex_unlock(&wl_gpio_lock);
#endif
}
