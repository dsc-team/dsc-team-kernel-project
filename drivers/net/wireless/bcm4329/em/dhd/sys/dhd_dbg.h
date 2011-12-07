/*
 * Debug/trace/assert driver definitions for Dongle Host Driver.
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: dhd_dbg.h,v 1.5.6.2.4.2.14.10 2010/05/21 21:49:38 Exp $
 */

#ifndef _dhd_dbg_
#define _dhd_dbg_

#if (defined BCMDBG) || (defined DHD_DEBUG)

#define DHD_ERROR(args)	       do {if (dhd_msg_level & DHD_ERROR_VAL)  printf args;} while (0)
#define DHD_TRACE(args)		do {if (dhd_msg_level & DHD_TRACE_VAL) printf args;} while (0)
#define DHD_INFO(args)		do {if (dhd_msg_level & DHD_INFO_VAL) printf args;} while (0)
#define DHD_DATA(args)		do {if (dhd_msg_level & DHD_DATA_VAL) printf args;} while (0)
#define DHD_CTL(args)		do {if (dhd_msg_level & DHD_CTL_VAL) printf args;} while (0)
#define DHD_TIMER(args)		do {if (dhd_msg_level & DHD_TIMER_VAL) printf args;} while (0)
#define DHD_HDRS(args)		do {if (dhd_msg_level & DHD_HDRS_VAL) printf args;} while (0)
#define DHD_BYTES(args)		do {if (dhd_msg_level & DHD_BYTES_VAL) printf args;} while (0)
#define DHD_INTR(args)		do {if (dhd_msg_level & DHD_INTR_VAL) printf args;} while (0)
#define DHD_GLOM(args)		do {if (dhd_msg_level & DHD_GLOM_VAL) printf args;} while (0)
#define DHD_EVENT(args)		do {if (dhd_msg_level & DHD_EVENT_VAL) printf args;} while (0)
#define DHD_BTA(args)		do {if (dhd_msg_level & DHD_BTA_VAL) printf args;} while (0)
#define DHD_ISCAN(args)		do {if (dhd_msg_level & DHD_ISCAN_VAL) printf args;} while (0)

#define DHD_ERROR_ON()		(dhd_msg_level & DHD_ERROR_VAL)
#define DHD_TRACE_ON()		(dhd_msg_level & DHD_TRACE_VAL)
#define DHD_INFO_ON()		(dhd_msg_level & DHD_INFO_VAL)
#define DHD_DATA_ON()		(dhd_msg_level & DHD_DATA_VAL)
#define DHD_CTL_ON()		(dhd_msg_level & DHD_CTL_VAL)
#define DHD_TIMER_ON()		(dhd_msg_level & DHD_TIMER_VAL)
#define DHD_HDRS_ON()		(dhd_msg_level & DHD_HDRS_VAL)
#define DHD_BYTES_ON()		(dhd_msg_level & DHD_BYTES_VAL)
#define DHD_INTR_ON()		(dhd_msg_level & DHD_INTR_VAL)
#define DHD_GLOM_ON()		(dhd_msg_level & DHD_GLOM_VAL)
#define DHD_EVENT_ON()		(dhd_msg_level & DHD_EVENT_VAL)
#define DHD_BTA_ON()		(dhd_msg_level & DHD_BTA_VAL)
#define DHD_ISCAN_ON()		(dhd_msg_level & DHD_ISCAN_VAL)

#else /* (defined BCMDBG) || (defined DHD_DEBUG) */

#define DHD_ERROR(args)
#define DHD_TRACE(args)
#define DHD_INFO(args)
#define DHD_DATA(args)
#define DHD_CTL(args)
#define DHD_TIMER(args)
#define DHD_HDRS(args)
#define DHD_BYTES(args)
#define DHD_INTR(args)
#define DHD_GLOM(args)
#define DHD_EVENT(args)
#define DHD_BTA(args)
#define DHD_ISCAN(args)

#define DHD_ERROR_ON()		0
#define DHD_TRACE_ON()		0
#define DHD_INFO_ON()		0
#define DHD_DATA_ON()		0
#define DHD_CTL_ON()		0
#define DHD_TIMER_ON()		0
#define DHD_HDRS_ON()		0
#define DHD_BYTES_ON()		0
#define DHD_INTR_ON()		0
#define DHD_GLOM_ON()		0
#define DHD_EVENT_ON()		0
#define DHD_BTA_ON()		0
#define DHD_ISCAN_ON()		0
#endif /* (defined BCMDBG) || (defined DHD_DEBUG) */

#ifdef BCMPERFSTATS
#define DHD_LOG(args)		do {if (dhd_msg_level & DHD_LOG_VAL) bcmlog args;} while (0)
#else
#define DHD_LOG(args)
#endif

#define DHD_NONE(args)
extern int dhd_msg_level;

/* Defines msg bits */
#include <dhdioctl.h>

#endif /* _dhd_dbg_ */
