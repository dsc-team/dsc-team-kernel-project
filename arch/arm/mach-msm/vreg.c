/* arch/arm/mach-msm/vreg.c
 *
 * Copyright (C) 2008 Google, Inc.
 * Copyright (c) 2009, Code Aurora Forum. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/debugfs.h>
#include <linux/string.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/list.h>
#include <mach/vreg.h>

#include "proc_comm.h"

#if defined(CONFIG_MSM_VREG_SWITCH_INVERTED)
#define VREG_SWITCH_ENABLE 0
#define VREG_SWITCH_DISABLE 1
#else
#define VREG_SWITCH_ENABLE 1
#define VREG_SWITCH_DISABLE 0
#endif

static DEFINE_SPINLOCK(vreg_lock);
static struct workqueue_struct *vreg_workqueue = NULL;

struct vreg_source {
	const char *name;
	unsigned id;
	int status;
	unsigned refcnt;
	unsigned num_clients;
	struct list_head head_clients;
        struct blocking_notifier_head notifiers;
        struct work_struct work;
	unsigned num_notifiers;
};

struct vreg {
	void *dev;
	unsigned refcnt;
	struct list_head list;
	struct vreg_source *vreg_s;
};

#define VREG(_name, _id, _status, _refcnt) \
	{ .name = _name, .id = _id, .status = _status, .refcnt = _refcnt, .num_clients = 0, .num_notifiers = 0 }

static struct vreg_source vregs[] = {
	VREG("msma",	0, 0, 0),
	VREG("msmp",	1, 0, 0),
	VREG("msme1",	2, 0, 0),
	VREG("msmc1",	3, 0, 0),
	VREG("msmc2",	4, 0, 0),
	VREG("gp3",	5, 0, 0),
	VREG("msme2",	6, 0, 0),
	VREG("gp4",	7, 0, 0),
	VREG("gp1",	8, 0, 0),
	VREG("tcxo",	9, 0, 0),
	VREG("pa",	10, 0, 0),
	VREG("rftx",	11, 0, 0),
	VREG("rfrx1",	12, 0, 0),
	VREG("rfrx2",	13, 0, 0),
	VREG("synt",	14, 0, 0),
	VREG("wlan",	15, 0, 0),
	VREG("usb",	16, 0, 0),
	VREG("boost",	17, 0, 0),
	VREG("mmc",	18, 0, 0),
	VREG("ruim",	19, 0, 0),
	VREG("msmc0",	20, 0, 0),
	VREG("gp2",	21, 0, 0),
	VREG("gp5",	22, 0, 0),
	VREG("gp6",	23, 0, 0),
	VREG("rf",	24, 0, 0),
	VREG("rf_vco",	26, 0, 0),
	VREG("mpll",	27, 0, 0),
	VREG("s2",	28, 0, 0),
	VREG("s3",	29, 0, 0),
	VREG("rfubm",	30, 0, 0),
	VREG("ncp",	31, 0, 0),
	VREG("gp7",	32, 0, 0),
	VREG("gp8",	33, 0, 0),
	VREG("gp9",	34, 0, 0),
	VREG("gp10",	35, 0, 0),
	VREG("gp11",	36, 0, 0),
	VREG("gp12",	37, 0, 0),
	VREG("gp13",	38, 0, 0),
	VREG("gp14",	39, 0, 0),
	VREG("gp15",	40, 0, 0),
	VREG("gp16",	41, 0, 0),
	VREG("gp17",	42, 0, 0),
	VREG("s4",	43, 0, 0),
	VREG("usb2",	44, 0, 0),
	VREG("wlan2",	45, 0, 0),
	VREG("xo_out",	46, 0, 0),
	VREG("lvsw0",	47, 0, 0),
	VREG("lvsw1",	48, 0, 0),
};

struct vreg *vreg_get(struct device *dev, const char *id)
{
	int n;
	unsigned long flags;

	for (n = 0; n < ARRAY_SIZE(vregs); n++) {
		if (!strcmp(vregs[n].name, id)) {
			struct vreg_source *vreg_s = vregs + n;
			struct vreg *cur = NULL;

			spin_lock_irqsave(&vreg_lock, flags);

			if (unlikely(!vreg_s->num_clients)) INIT_LIST_HEAD(&vreg_s->head_clients);
			if (unlikely(!list_empty(&vreg_s->head_clients))) {
				struct vreg *tmp;
				list_for_each_entry_safe(cur, tmp, &vreg_s->head_clients, list) {
					if( cur->dev == dev) break;
				}
				if ( likely(cur->dev != dev)) cur = NULL;
			}
			if (likely(cur == NULL)) {
				cur = (struct vreg*) kmalloc( sizeof( struct vreg), GFP_ATOMIC);
				if ( likely(cur)) {
					INIT_LIST_HEAD( &cur->list);
					cur->dev = dev;
					cur->refcnt = 0;
					cur->vreg_s = vreg_s;
					list_add_tail(&cur->list, &vreg_s->head_clients);
					vreg_s->num_clients++;
				}
				else {
					spin_unlock_irqrestore(&vreg_lock, flags);
					return ERR_PTR(-ENOENT);
				}
			}

			spin_unlock_irqrestore(&vreg_lock, flags);

			return cur;
		}
	}
	return ERR_PTR(-ENOENT);
}
EXPORT_SYMBOL(vreg_get);

void vreg_put(struct vreg *vreg)
{
}

int vreg_enable(struct vreg *vreg)
{
	struct vreg_source *vreg_s = vreg->vreg_s;
	unsigned id = vreg_s->id;
	int enable = VREG_SWITCH_ENABLE;
	unsigned long flags;

	spin_lock_irqsave(&vreg_lock, flags);

	if (likely(!vreg->refcnt)) {
		if (likely(!vreg_s->refcnt)) {
			vreg_s->status = msm_proc_comm(PCOM_VREG_SWITCH, &id, &enable);

			if (likely(!vreg_s->status)) {
				if ( vreg_s->num_notifiers && vreg_workqueue && likely(!work_pending(&vreg_s->work))) {
					queue_work(vreg_workqueue, &vreg_s->work);
				}
				printk(KERN_INFO "%s: %s enabled\n", __func__, vreg_s->name);
			}
		}
		if (likely(vreg->refcnt < UINT_MAX)) vreg_s->refcnt++;
	}
	if (likely(vreg->refcnt < UINT_MAX)) vreg->refcnt++;

	spin_unlock_irqrestore(&vreg_lock, flags);

	return vreg_s->status;
}
EXPORT_SYMBOL(vreg_enable);

int vreg_disable(struct vreg *vreg)
{
	struct vreg_source *vreg_s = vreg->vreg_s;
	unsigned id = vreg_s->id;
	int disable = VREG_SWITCH_DISABLE;
	unsigned long flags;

	if (!vreg->refcnt)
		return 0;

	spin_lock_irqsave(&vreg_lock, flags);

	vreg->refcnt--;
	if (likely(!vreg->refcnt)) {
		vreg_s->refcnt--;
		if (likely(!vreg_s->refcnt)) {
			vreg_s->status = msm_proc_comm(PCOM_VREG_SWITCH, &id, &disable);

			if (!vreg_s->status) {
				if( vreg_s->num_notifiers && vreg_workqueue && likely(!work_pending(&vreg_s->work))) {
					queue_work(vreg_workqueue, &vreg_s->work);
				}
				printk(KERN_INFO "%s: %s disabled\n", __func__, vreg_s->name);
			}
		}
	}

	spin_unlock_irqrestore(&vreg_lock, flags);

	return vreg_s->status;
}
EXPORT_SYMBOL(vreg_disable);

int vreg_set_level(struct vreg *vreg, unsigned mv)
{
	struct vreg_source *vreg_s = vreg->vreg_s;
	unsigned id = vreg_s->id;

	vreg_s->status = msm_proc_comm(PCOM_VREG_SET_LEVEL, &id, &mv);
	return vreg_s->status;
}
EXPORT_SYMBOL(vreg_set_level);

int vreg_add_notifier(struct vreg *vreg, struct notifier_block *notifier)
{
        int retval;
	struct vreg_source *vreg_s = vreg->vreg_s;
	unsigned long flags;
        notifier->notifier_call(notifier, (vreg_s->refcnt>0) , NULL);
	spin_lock_irqsave(&vreg_lock, flags);
        retval = blocking_notifier_chain_register(&vreg_s->notifiers, notifier);
	vreg_s->num_notifiers++;
	spin_unlock_irqrestore(&vreg_lock, flags);
        return retval;
}
EXPORT_SYMBOL(vreg_add_notifier);

int vreg_remove_notifier(struct vreg *vreg, struct notifier_block *notifier)
{
        int retval;
	struct vreg_source *vreg_s = vreg->vreg_s;
	unsigned long flags;
	spin_lock_irqsave(&vreg_lock, flags);
        retval = blocking_notifier_chain_unregister(&vreg_s->notifiers, notifier);
	vreg_s->num_notifiers--;
	spin_unlock_irqrestore(&vreg_lock, flags);
        return retval;
}
EXPORT_SYMBOL(vreg_remove_notifier);

void vreg_work(struct work_struct *work)
{
        struct vreg_source *vreg_s = container_of(work, struct vreg_source, work);
        unsigned long state;
        unsigned long flags;

        spin_lock_irqsave( &vreg_lock, flags);
        state = (unsigned long) (vreg_s->refcnt>0);
        spin_unlock_irqrestore( &vreg_lock, flags);

        blocking_notifier_call_chain(&vreg_s->notifiers, state, NULL);
}

static __init int vreg_init(void)
{
	int n;
        for (n = 0; n < ARRAY_SIZE(vregs); n++) {
                BLOCKING_INIT_NOTIFIER_HEAD(&vregs[n].notifiers);
                INIT_WORK(&vregs[n].work, vreg_work);
        }
        vreg_workqueue = create_singlethread_workqueue("vreg_notifiers");
        if (!vreg_workqueue) {
                printk(KERN_ERR "Failed to create workqueue\n");
                return -ENOMEM;
        }
        return 0;
}
arch_initcall(vreg_init);

#if defined(CONFIG_DEBUG_FS)

static int vreg_debug_set(void *data, u64 val)
{
	struct vreg_source *vreg_s = data;
	struct vreg *vreg = vreg_get(NULL, vreg_s->name);
	switch (val) {
	case 0:
		vreg_disable(vreg);
		break;
	case 1:
		vreg_enable(vreg);
		break;
	default:
		vreg_set_level(vreg, val);
		break;
	}
	return 0;
}

static int vreg_debug_get(void *data, u64 *val)
{
	struct vreg_source *vreg_s = data;

	if (!vreg_s->status)
		*val = 0;
	else
		*val = 1;

	return 0;
}

static int vreg_debug_count_set(void *data, u64 val)
{
	struct vreg_source *vreg_s = data;
	if (val > UINT_MAX)
		val = UINT_MAX;
	vreg_s->refcnt = val;
	return 0;
}

static int vreg_debug_count_get(void *data, u64 *val)
{
	struct vreg_source *vreg_s = data;

	*val = vreg_s->refcnt;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(vreg_fops, vreg_debug_get, vreg_debug_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(vreg_count_fops, vreg_debug_count_get,
			vreg_debug_count_set, "%llu\n");

static int __init vreg_debug_init(void)
{
	struct dentry *dent;
	int n;
	char name[32];
	const char *refcnt_name = "_refcnt";

	dent = debugfs_create_dir("vreg", 0);
	if (IS_ERR(dent))
		return 0;

	for (n = 0; n < ARRAY_SIZE(vregs); n++) {
		(void) debugfs_create_file(vregs[n].name, 0644,
					   dent, vregs + n, &vreg_fops);

		strlcpy(name, vregs[n].name, sizeof(name));
		strlcat(name, refcnt_name, sizeof(name));
		(void) debugfs_create_file(name, 0644,
					   dent, vregs + n, &vreg_count_fops);
	}

	return 0;
}

device_initcall(vreg_debug_init);
#endif
