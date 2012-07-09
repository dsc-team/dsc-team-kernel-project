/*
 * Broadcom Ethernettype  protocol definitions
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: bcmeth.h,v 9.9.70.1 2008/10/25 00:31:24 Exp $
 */




#ifndef _BCMETH_H_
#define _BCMETH_H_

#ifndef _TYPEDEFS_H_
#include <typedefs.h>
#endif


#include <packed_section_start.h>







#define	BCMILCP_SUBTYPE_RATE		1
#define	BCMILCP_SUBTYPE_LINK		2
#define	BCMILCP_SUBTYPE_CSA		3
#define	BCMILCP_SUBTYPE_LARQ		4
#define BCMILCP_SUBTYPE_VENDOR		5
#define	BCMILCP_SUBTYPE_FLH		17

#define BCMILCP_SUBTYPE_VENDOR_LONG	32769
#define BCMILCP_SUBTYPE_CERT		32770
#define BCMILCP_SUBTYPE_SES		32771


#define BCMILCP_BCM_SUBTYPE_RESERVED		0
#define BCMILCP_BCM_SUBTYPE_EVENT		1
#define BCMILCP_BCM_SUBTYPE_SES			2


#define BCMILCP_BCM_SUBTYPE_DPT			4

#define BCMILCP_BCM_SUBTYPEHDR_MINLENGTH	8
#define BCMILCP_BCM_SUBTYPEHDR_VERSION		0


typedef BWL_PRE_PACKED_STRUCT struct bcmeth_hdr
{
	uint16	subtype;	
	uint16	length;
	uint8	version;	
	uint8	oui[3];		
	
	uint16	usr_subtype;
} BWL_POST_PACKED_STRUCT bcmeth_hdr_t;



#include <packed_section_end.h>

#endif	
