/*
 * Broadcom SDIO/PCMCIA
 * Software-specific definitions shared between device and host side
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: bcmsdpcm.h,v 13.4.26.1 2009/07/06 11:56:22 Exp $
 */

#ifndef	_bcmsdpcm_h_
#define	_bcmsdpcm_h_


/*
 * Shared structure between dongle and the host
 * The structure contains pointers to trap or assert information shared with the host
 */
#define SDPCM_SHARED_VERSION       0x0001
#define SDPCM_SHARED_VERSION_MASK  0x00FF
#define SDPCM_SHARED_ASSERT_BUILT  0x0100
#define SDPCM_SHARED_ASSERT        0x0200
#define SDPCM_SHARED_TRAP          0x0400

typedef struct {
	uint32	flags;
	uint32  trap_addr;
	uint32  assert_exp_addr;
	uint32  assert_file_addr;
	uint32  assert_line;
	uint32	console_addr;		/* Address of hndrte_cons_t */
	uint32  msgtrace_addr;
} sdpcm_shared_t;

extern sdpcm_shared_t sdpcm_shared;

#endif	/* _bcmsdpcm_h_ */
