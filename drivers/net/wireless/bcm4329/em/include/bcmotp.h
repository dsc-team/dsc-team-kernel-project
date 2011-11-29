/*
 * OTP support.
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: bcmotp.h,v 13.10.108.4.4.5.2.3 2009/08/15 00:51:26 Exp $
 */

/* OTP layout */
/* CC revs 21, 24, and 27 OTP General Use Region word offset */
#define REVA4_OTPGU_BASE	12

/* CC revs 23, 25 and above OTP General Use Region word offset */
#define REVB8_OTPGU_BASE	20

/* Subregion word offsets in General Use region */
#define OTPGU_HSB_OFF		0
#define OTPGU_SFB_OFF		1
#define OTPGU_CI_OFF		2
#define OTPGU_SROM_OFF		4

/* Fixed size subregions sizes in words */
#define OTPGU_CI_SZ			2

/* Flag bit offsets in General Use region  */
#define OTPGU_HWP_OFF		60
#define OTPGU_SWP_OFF		61
#define OTPGU_CIP_OFF		62
#define OTPGU_FUSEP_OFF		63

/* Maximum OTP redundancy entries.  */
#define MAXNUMRDES	9

/* OTP regions */
#define OTP_HW_RGN	1
#define OTP_SW_RGN	2
#define OTP_CI_RGN	4
#define OTP_FUSE_RGN	8

/* OTP usage */
#define OTP4325_FM_DISABLED_OFFSET	188


extern int BCMINITFN(otp_status)(void *oh);
extern int BCMINITFN(otp_size)(void *oh);

extern int BCMINITFN(otp_read_region)(void *oh, int region, uint16 *data, uint *wlen);
extern uint16 otpr(void *oh, chipcregs_t *cc, uint wn);
extern uint16 otp_read_one_bit(void *oh, uint offset);
extern int otp_nvread(void *oh, char *data, uint *len);

#ifdef BCMNVRAMW
#endif /* BCMNVRAMW */

#if (defined(BCMNVRAMR) || defined(BCMNVRAMW))
extern int otp_dump(void *oh, int arg, char *buf, uint size);
#endif
