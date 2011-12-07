/*
 * Misc system wide definitions
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 * $Id: bcmdefs.h,v 13.38.4.10.2.7.20.1 2009/07/09 00:03:10 Exp $
 */


#ifndef	_bcmdefs_h_
#define	_bcmdefs_h_






#ifdef DONGLEBUILD

extern bool bcmreclaimed;

#define BCMATTACHDATA(_data)	__attribute__ ((__section__ (".dataini2." #_data))) _data
#define BCMATTACHFN(_fn)	__attribute__ ((__section__ (".textini2." #_fn))) _fn

#if defined(BCMRECLAIM)
#define BCMINITDATA(_data)	__attribute__ ((__section__ (".dataini1." #_data))) _data
#define BCMINITFN(_fn)		__attribute__ ((__section__ (".textini1." #_fn))) _fn
#define CONST
#else
#define BCMINITDATA(_data)	_data
#define BCMINITFN(_fn)		_fn
#define CONST	const
#endif

#ifdef BCMNODOWN
#define BCMUNINITFN(_fn)	BCMINITFN(_fn)
#else
#define BCMUNINITFN(_fn)	_fn
#endif

#else 

#define bcmreclaimed 		0
#define BCMATTACHDATA(_data)	_data
#define BCMATTACHFN(_fn)	_fn
#define BCMINITDATA(_data)	_data
#define BCMINITFN(_fn)		_fn
#define BCMUNINITFN(_fn)	_fn
#define CONST	const

#endif 


#if defined(BCMROMOFFLOAD)
#define BCMROMDATA(_data)	__attribute__ ((weak, __section__ (".datarom."#_data))) _data
#define BCMROMFN(_fn)		__attribute__ ((long_call, __section__ (".textrom."#_fn))) _fn
#define BCMROMFN_NAME(_fn)	_fn
#define STATIC
#elif defined(BCMROMBUILD)
#define BCMROMDATA(_data)	__attribute__ ((__section__ (".datarom."#_data))) _data

#include <bcmjmptbl.h>
#define STATIC
#else 
#define BCMROMDATA(_data)	_data
#define BCMROMFN(_fn)		_fn
#define BCMROMFN_NAME(_fn)	_fn
#define STATIC	static
#endif 


#define	SI_BUS			0	
#define	PCI_BUS			1	
#define	PCMCIA_BUS		2	
#define SDIO_BUS		3	
#define JTAG_BUS		4	
#define USB_BUS			5	
#define SPI_BUS			6	


#ifdef BCMBUSTYPE
#define BUSTYPE(bus) 	(BCMBUSTYPE)
#else
#define BUSTYPE(bus) 	(bus)
#endif


#ifdef BCMCHIPTYPE
#define CHIPTYPE(bus) 	(BCMCHIPTYPE)
#else
#define CHIPTYPE(bus) 	(bus)
#endif



#if defined(BCMSPROMBUS)
#define SPROMBUS	(BCMSPROMBUS)
#elif defined(SI_PCMCIA_SROM)
#define SPROMBUS	(PCMCIA_BUS)
#else
#define SPROMBUS	(PCI_BUS)
#endif


#ifdef BCMCHIPID
#define CHIPID(chip)	(BCMCHIPID)
#else
#define CHIPID(chip)	(chip)
#endif


#define DMADDR_MASK_32 0x0		
#define DMADDR_MASK_30 0xc0000000	
#define DMADDR_MASK_0  0xffffffff	

#define	DMADDRWIDTH_30  30 
#define	DMADDRWIDTH_32  32 
#define	DMADDRWIDTH_63  63 
#define	DMADDRWIDTH_64  64 

#if defined(MACOSX)

#define MAX_DMA_SEGS 8
#else
#define MAX_DMA_SEGS 4
#endif


#define BCMEXTRAHDROOM 160


#define BCMDONGLEHDRSZ 12

#ifdef BCMDBG

#ifndef BCMDBG_ERR
#define BCMDBG_ERR
#endif 

#ifndef BCMDBG_ASSERT
#define BCMDBG_ASSERT
#endif 

#endif 


#define BITFIELD_MASK(width) \
		(((unsigned)1 << (width)) - 1)
#define GFIELD(val, field) \
		(((val) >> field ## _S) & field ## _M)
#define SFIELD(val, field, bits) \
		(((val) & (~(field ## _M << field ## _S))) | \
		 ((unsigned)(bits) << field ## _S))


#ifdef BCMSMALL
#undef	BCMSPACE
#define bcmspace	FALSE	
#else
#define	BCMSPACE
#define bcmspace	TRUE	
#endif


#define	MAXSZ_NVRAM_VARS	4096



#ifdef WL_LOCATOR
#define LOCATOR_EXTERN
#else
#define LOCATOR_EXTERN static
#endif

#endif 
