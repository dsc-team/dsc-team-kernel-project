/*
 * Expose some of the kernel scheduler routines
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: dhd_linux_sched.c,v 1.1.34.1.6.1 2009/01/16 01:17:40 Exp $
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linuxver.h>

int setScheduler(struct task_struct *p, int policy, struct sched_param *param)
{
	int rc = 0;
	return rc;
}
