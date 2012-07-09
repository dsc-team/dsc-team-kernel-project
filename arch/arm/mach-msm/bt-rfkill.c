/*
 * Copyright (C) 2008 Google, Inc.
 * Author: Nick Pelly <npelly@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/rfkill.h>
#include <linux/delay.h>
#include <asm/gpio.h>
#include <mach/vreg.h>
#include <mach/pm_log.h>


#define DEBUG
#ifdef DEBUG
#define BTRFKILL_INFO(fmt, arg...) printk(KERN_INFO "bt_rfkill: " fmt "\n" , ## arg)
#define BTRFKILL_DBG(fmt, arg...)  printk(KERN_INFO "%s: " fmt "\n" , __FUNCTION__ , ## arg)
#define BTRFKILL_ERR(fmt, arg...)  printk(KERN_ERR  "%s: " fmt "\n" , __FUNCTION__ , ## arg)
#else
#define BTRFKILL_INFO(fmt, arg...)
#define BTRFKILL_DBG(fmt, arg...)
#define BTRFKILL_ERR(fmt, arg...)
#endif

static struct rfkill *bt_rfk;

#ifdef CONFIG_HW_AUSTIN
static const char bt_name[] = "bcm4325";
#else
static const char bt_name[] = "bcm4329";
#endif	//CONFIG_HW_AUSTIN

#define BT_RST_N   	27		
#define BT_EN   		29		
#define BT_INT   		36		
#define BT_WAKEUP   22		

static unsigned bt_config_power_on[] = {
	GPIO_CFG(139, 1, GPIO_CFG_INPUT,	GPIO_CFG_PULL_DOWN, 	GPIO_CFG_2MA),	/* RX */
	GPIO_CFG(140, 1, GPIO_CFG_OUTPUT,  	GPIO_CFG_NO_PULL, 	GPIO_CFG_2MA),	/* TX */
	GPIO_CFG(141, 1, GPIO_CFG_INPUT,  	GPIO_CFG_PULL_DOWN, 	GPIO_CFG_2MA),	/* CTS */
	GPIO_CFG(157, 1, GPIO_CFG_OUTPUT, 	GPIO_CFG_NO_PULL, 	GPIO_CFG_2MA),	/* RFR */
};
static unsigned bt_config_power_off[] = {
	GPIO_CFG(139, 0, GPIO_CFG_INPUT, 	GPIO_CFG_PULL_UP, 	GPIO_CFG_2MA),	/* RX */
	GPIO_CFG(140, 0, GPIO_CFG_INPUT,  	GPIO_CFG_PULL_DOWN, 	GPIO_CFG_2MA),	/* TX */
	GPIO_CFG(141, 0, GPIO_CFG_INPUT,  	GPIO_CFG_PULL_DOWN, 	GPIO_CFG_2MA),	/* CTS */
	GPIO_CFG(157, 0, GPIO_CFG_INPUT,	GPIO_CFG_PULL_DOWN, 	GPIO_CFG_2MA),	/* RFR */
};

static int bluetooth_set_power(void *data, bool blocked)
{
	struct vreg *vreg_rftx;
	int rc = 0;
	int pin =0;

	vreg_rftx = vreg_get(NULL, "rftx");
	
	if (!blocked) {
		rc = vreg_set_level(vreg_rftx, 2500);
		if (rc) {
			printk(KERN_ERR "%s: vreg set level failed (%d)\n",
			       __func__, rc);
			return -EIO;
		}
		rc = vreg_enable(vreg_rftx);
		if (rc) {
			printk(KERN_ERR "%s: vreg enable failed (%d)\n",
			       __func__, rc);
			return -EIO;
		}
		for (pin = 0; pin < ARRAY_SIZE(bt_config_power_on); pin++) {
			rc = gpio_tlmm_config(bt_config_power_on[pin],
					      GPIO_CFG_ENABLE);
			if (rc) {
				printk(KERN_ERR
				       "%s: gpio_tlmm_config(%#x)=%d\n",
				       __func__, bt_config_power_on[pin], rc);
				return -EIO;
			}
		}
		
		printk(KERN_DEBUG "bluetooth_power: Turn on BT power Class 1.5 DONE\n");

#ifdef CONFIG_HW_AUSTIN
		if (1 == gpio_get_value(BT_RST_N) )
		{
			printk(KERN_DEBUG "+ Wifi plusing BT_RST_N wait for complete +\n");
			mdelay(200);
			printk(KERN_DEBUG "- Wifi plusing BT_RST_N wait for complete -\n");
		}
#endif	//CONFIG_HW_AUSTIN		

		printk(KERN_DEBUG "bluetooth_power: Turn on chip \n");
		gpio_set_value(BT_EN, 1);
		mdelay(100);
		printk(KERN_DEBUG "bluetooth_power: Turn on chip, Sleep done\n");

		gpio_set_value(BT_RST_N, 1);

		printk(KERN_DEBUG "bluetooth_power: Pull low for BT_WAKEUP\n");

		gpio_set_value(BT_WAKEUP, 0);

		PM_LOG_EVENT(PM_LOG_ON, PM_LOG_BLUETOOTH);
		
	} else {

		gpio_set_value(BT_RST_N, 0);

		printk(KERN_DEBUG "bluetooth_power: Turn off chip \n" );
		gpio_set_value(BT_EN, 0);

		rc = vreg_disable(vreg_rftx);
		printk(KERN_DEBUG "bluetooth_power: Turn off BT power Class 1.5 \n");
		if (rc) {
			printk(KERN_ERR "%s: vreg disable failed (%d)\n",
			       __func__, rc);
			return -EIO;

		}
		printk(KERN_DEBUG "bluetooth_power: Pull high for BT_WAKEUP\n");

		gpio_set_value(BT_WAKEUP, 1);

		PM_LOG_EVENT(PM_LOG_OFF, PM_LOG_BLUETOOTH);
		
		for (pin = 0; pin < ARRAY_SIZE(bt_config_power_off); pin++) {
			rc = gpio_tlmm_config(bt_config_power_off[pin],
					      GPIO_CFG_ENABLE);
			if (rc) {
				printk(KERN_ERR
				       "%s: gpio_tlmm_config(%#x)=%d\n",
				       __func__, bt_config_power_off[pin], rc);
				return -EIO;
			}
		}
	}		
	return 0;
}

static struct rfkill_ops qisda_rfkill_ops = {
	.set_block = bluetooth_set_power,
};


static int bt_rfkill_probe(struct platform_device *pdev)
{
	int rc = 0;
	bool default_state = true;
	
	BTRFKILL_DBG();

	bluetooth_set_power(NULL, default_state);

	bt_rfk = rfkill_alloc(bt_name, &pdev->dev, RFKILL_TYPE_BLUETOOTH,
				&qisda_rfkill_ops, NULL);

	if (!bt_rfk) {
		rc = -ENOMEM;
		goto err_rfkill_alloc;
	}

	rfkill_set_states(bt_rfk, default_state, false);
	
	rc = rfkill_register(bt_rfk);
	if (rc)
		goto err_rfkill_reg;

	return 0;
		
err_rfkill_reg:
	rfkill_destroy(bt_rfk);
err_rfkill_alloc:

	return rc;
}

static int bt_rfkill_remove(struct platform_device *dev)
{
	rfkill_unregister(bt_rfk);
	rfkill_destroy(bt_rfk);

	return 0;
}

static struct platform_driver bt_rfkill_driver = {
	.probe = bt_rfkill_probe,
	.remove = bt_rfkill_remove,	
	.driver = {
		.name = "bt_rfkill",
		.owner = THIS_MODULE,
	},
};

static int __init bt_rfkill_init(void)
{
	int ret;
	printk(KERN_INFO "BootLog, +%s\n", __func__);
	ret = platform_driver_register(&bt_rfkill_driver);
	printk(KERN_INFO "BootLog, -%s, ret=%d\n", __func__, ret); 
	return ret;
}

static void __exit bt_rfkill_exit(void)
{
	platform_driver_unregister(&bt_rfkill_driver);
}

module_init(bt_rfkill_init);
module_exit(bt_rfkill_exit);
MODULE_DESCRIPTION("bcm 4325 rfkill");
MODULE_AUTHOR("Nick Pelly <npelly@google.com>");
MODULE_LICENSE("GPL");
