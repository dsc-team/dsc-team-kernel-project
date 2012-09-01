/* arch/arm/mach-msm/proc_comm.c
 *
 * Copyright (C) 2007-2008 Google, Inc.
 * Copyright (c) 2009-2010, Code Aurora Forum. All rights reserved.
 * Author: Brian Swetland <swetland@google.com>
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

#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include <mach/msm_iomap.h>
#include <mach/system.h>
#include <linux/workqueue.h>

#include "proc_comm.h"
#include "smd_private.h"

#if defined (CONFIG_QSD_OEM_RPC_VERSION_CHECK)
#include <mach/smem_pc_oem_cmd.h>
#include "oem_rpc_version_check.h"

#define RPC_VERCHECK_ERROR_NOTIFY_PERIOD 60

static int rpc_version_checked;

static void rpc_version_check_show_errmsg_sys(struct work_struct *work);
static void rpc_version_check_show_errmsg(struct work_struct *work);

static DECLARE_WORK(crash_worker, rpc_version_check_show_errmsg);
static DECLARE_WORK(crash_worker_sys, rpc_version_check_show_errmsg_sys);
static struct workqueue_struct *crash_work_queue;

static int version_check = 0;

#ifdef CONFIG_HW_AUSTIN

#define KEY_MODEM_RESET KEY_CAMERA
#if !(defined (CONFIG_MACH_EVB) || defined (CONFIG_MACH_EVT0))

#define MODEM_CRASH_NOTIFY_USER

extern void qi2ccapkybd_set_led(unsigned int light);

#define MODEM_CRASH_NOTIFY_USER_0 do {qi2ccapkybd_set_led(0x0);} while(0)
#define MODEM_CRASH_NOTIFY_USER_1 do {qi2ccapkybd_set_led(0xFFFFFF);} while(0)

#endif

#endif

#if defined (MODEM_CRASH_NOTIFY_USER)
void rpc_version_check_show_err(void)
{
	uint32_t loop;
	for(loop = 0; loop < 10; loop++)
	{
		MODEM_CRASH_NOTIFY_USER_1;
		msleep(100);
        MODEM_CRASH_NOTIFY_USER_0;
		msleep(100);
	}
}
#else
void rpc_version_check_show_err(void) {}
#endif

static void rpc_version_check_show_errmsg_sys(struct work_struct *work)
{
	INIT_WORK(&crash_worker, rpc_version_check_show_errmsg);

	crash_work_queue = create_singlethread_workqueue("rpc_version_check");

	queue_work(crash_work_queue, &crash_worker);
}

static void rpc_version_check_show_errmsg(struct work_struct *work)
{
	while(1)
	{
		rpc_version_check_show_err();
		msleep(RPC_VERCHECK_ERROR_NOTIFY_PERIOD * 1000);
	}
}

static int rpc_version_check_func(void)
{
	int arg1, arg2 = 0, ret_result;

	arg1 = SMEM_PC_OEM_CHECK_VERSION;
	ret_result = msm_proc_comm(PCOM_CUSTOMER_CMD1, &arg1, &arg2);

	if ((ret_result != 0) ||
			(arg2 != rpc_version_check_magic_number))
	{
		printk(KERN_ERR "%s, msm_proc_comm result %d, magic number: %x(M):%x(A)\n",
				__func__, ret_result, arg2, rpc_version_check_magic_number);
		INIT_WORK(&crash_worker_sys, rpc_version_check_show_errmsg_sys);

		schedule_work(&crash_worker_sys);
	}
	return ret_result;
}
#endif /* CONFIG_QSD_OEM_RPC_VERSION_CHECK */

#if defined(CONFIG_ARCH_MSM7X30)
#define MSM_TRIG_A2M_PC_INT (writel(1 << 6, MSM_GCC_BASE + 0x8))
#elif defined(CONFIG_ARCH_MSM8X60)
#define MSM_TRIG_A2M_PC_INT (writel(1 << 5, MSM_GCC_BASE + 0x8))
#else
#define MSM_TRIG_A2M_PC_INT (writel(1, MSM_CSR_BASE + 0x400 + (6) * 4))
#endif

static inline void notify_other_proc_comm(void)
{
	MSM_TRIG_A2M_PC_INT;
}

#define APP_COMMAND 0x00
#define APP_STATUS  0x04
#define APP_DATA1   0x08
#define APP_DATA2   0x0C

#define MDM_COMMAND 0x10
#define MDM_STATUS  0x14
#define MDM_DATA1   0x18
#define MDM_DATA2   0x1C

static DEFINE_SPINLOCK(proc_comm_lock);
#if defined (CONFIG_QSD_OEM_RPC_VERSION_CHECK)
static DEFINE_SPINLOCK(proc_comm_version_lock);
#endif /* CONFIG_QSD_OEM_RPC_VERSION_CHECK */

int (*msm_check_for_modem_crash)(void);

/* Poll for a state change, checking for possible
 * modem crashes along the way (so we don't wait
 * forever while the ARM9 is blowing up.
 *
 * Return an error in the event of a modem crash and
 * restart so the msm_proc_comm() routine can restart
 * the operation from the beginning.
 */
static int proc_comm_wait_for(unsigned addr, unsigned value)
{
	while (1) {
		if (readl(addr) == value)
			return 0;
#if defined (CONFIG_QSD_ARM9_CRASH_FUNCTION)
		if (smsm_check_for_modem_crash())
		{
			int ret = smsm_check_for_modem_crash();
			if (ret)
			{
				return ret;
			}
		}
#else /* CONFIG_QSD_ARM9_CRASH_FUNCTION */
		if (smsm_check_for_modem_crash())
			return -EAGAIN;
#endif /* CONFIG_QSD_ARM9_CRASH_FUNCTION */
		udelay(5);
	}
}

void msm_proc_comm_reset_modem_now(void)
{
	unsigned base = (unsigned)MSM_SHARED_RAM_BASE;
	unsigned long flags;

	spin_lock_irqsave(&proc_comm_lock, flags);

again:
	if (proc_comm_wait_for(base + MDM_STATUS, PCOM_READY))
		goto again;

	writel(PCOM_RESET_MODEM, base + APP_COMMAND);
	writel(0, base + APP_DATA1);
	writel(0, base + APP_DATA2);

	spin_unlock_irqrestore(&proc_comm_lock, flags);

	notify_other_proc_comm();

	return;
}
EXPORT_SYMBOL(msm_proc_comm_reset_modem_now);

int msm_proc_comm(unsigned cmd, unsigned *data1, unsigned *data2)
{
	unsigned base = (unsigned)MSM_SHARED_RAM_BASE;
	unsigned long flags;
	int ret;
#if defined (CONFIG_QSD_OEM_RPC_VERSION_CHECK)
	if (version_check && rpc_version_checked == 0)
	{
		spin_lock_irqsave(&proc_comm_version_lock, flags);
		rpc_version_checked = 1;
		spin_unlock_irqrestore(&proc_comm_version_lock, flags);

		rpc_version_check_func();
	}
#endif /* CONFIG_QSD_OEM_RPC_VERSION_CHECK */
	spin_lock_irqsave(&proc_comm_lock, flags);

again:
#if defined (CONFIG_QSD_ARM9_CRASH_FUNCTION)
	if ((ret = proc_comm_wait_for(base + MDM_STATUS, PCOM_READY)) == -EAGAIN)
		goto again;
	else if (ret != 0)
		goto fail;
#else /* CONFIG_QSD_ARM9_CRASH_FUNCTION */
	if (proc_comm_wait_for(base + MDM_STATUS, PCOM_READY))
		goto again;
#endif /* CONFIG_QSD_ARM9_CRASH_FUNCTION */
	writel(cmd, base + APP_COMMAND);
	writel(data1 ? *data1 : 0, base + APP_DATA1);
	writel(data2 ? *data2 : 0, base + APP_DATA2);

	notify_other_proc_comm();

	if (proc_comm_wait_for(base + APP_COMMAND, PCOM_CMD_DONE))
		goto again;

	if (readl(base + APP_STATUS) == PCOM_CMD_SUCCESS) {
		if (data1)
			*data1 = readl(base + APP_DATA1);
		if (data2)
			*data2 = readl(base + APP_DATA2);
		ret = 0;
	} else {
		ret = -EIO;
	}

	writel(PCOM_CMD_IDLE, base + APP_COMMAND);
#if defined (CONFIG_QSD_ARM9_CRASH_FUNCTION)
fail:
#endif /* CONFIG_QSD_ARM9_CRASH_FUNCTION */
	spin_unlock_irqrestore(&proc_comm_lock, flags);
	return ret;
}
EXPORT_SYMBOL(msm_proc_comm);
static int __init msm_proc_comm_late_init(void)
{
#if defined (CONFIG_QSD_OEM_RPC_VERSION_CHECK)
	version_check = 1;
#endif
	return 0;
}
late_initcall(msm_proc_comm_late_init);
