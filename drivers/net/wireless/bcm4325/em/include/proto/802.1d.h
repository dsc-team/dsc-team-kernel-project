/*
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * Fundamental types and constants relating to 802.1D
 *
 * $Id: 802.1d.h,v 9.3 2007/04/10 21:33:06 Exp $
 */


#ifndef _802_1_D_
#define _802_1_D_


#define	PRIO_8021D_NONE		2	
#define	PRIO_8021D_BK		1	
#define	PRIO_8021D_BE		0	
#define	PRIO_8021D_EE		3	
#define	PRIO_8021D_CL		4	
#define	PRIO_8021D_VI		5	
#define	PRIO_8021D_VO		6	
#define	PRIO_8021D_NC		7	
#define	MAXPRIO			7	
#define NUMPRIO			(MAXPRIO + 1)

#define ALLPRIO		-1	


#define PRIO2PREC(prio) \
	(((prio) == PRIO_8021D_NONE || (prio) == PRIO_8021D_BE) ? ((prio^2)) : (prio))

#endif 
