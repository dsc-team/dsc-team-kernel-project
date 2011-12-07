/*
 * BCM43XX Sonics SiliconBackplane PCMCIA core hardware definitions.
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: sbpcmcia.h,v 13.31.4.1.2.5.6.3 2009/01/29 01:19:50 Exp $
 */


#ifndef	_SBPCMCIA_H
#define	_SBPCMCIA_H




#define	PCMCIA_FCR		(0x700 / 2)

#define	FCR0_OFF		0
#define	FCR1_OFF		(0x40 / 2)
#define	FCR2_OFF		(0x80 / 2)
#define	FCR3_OFF		(0xc0 / 2)

#define	PCMCIA_FCR0		(0x700 / 2)
#define	PCMCIA_FCR1		(0x740 / 2)
#define	PCMCIA_FCR2		(0x780 / 2)
#define	PCMCIA_FCR3		(0x7c0 / 2)



#define	PCMCIA_COR		0

#define	COR_RST			0x80
#define	COR_LEV			0x40
#define	COR_IRQEN		0x04
#define	COR_BLREN		0x01
#define	COR_FUNEN		0x01


#define	PCICIA_FCSR		(2 / 2)
#define	PCICIA_PRR		(4 / 2)
#define	PCICIA_SCR		(6 / 2)
#define	PCICIA_ESR		(8 / 2)


#define PCM_MEMOFF		0x0000
#define F0_MEMOFF		0x1000
#define F1_MEMOFF		0x2000
#define F2_MEMOFF		0x3000
#define F3_MEMOFF		0x4000


#define MEM_ADDR0		(0x728 / 2)
#define MEM_ADDR1		(0x72a / 2)
#define MEM_ADDR2		(0x72c / 2)


#define PCMCIA_ADDR0		(0x072e / 2)
#define PCMCIA_ADDR1		(0x0730 / 2)
#define PCMCIA_ADDR2		(0x0732 / 2)

#define MEM_SEG			(0x0734 / 2)
#define SROM_CS			(0x0736 / 2)
#define SROM_DATAL		(0x0738 / 2)
#define SROM_DATAH		(0x073a / 2)
#define SROM_ADDRL		(0x073c / 2)
#define SROM_ADDRH		(0x073e / 2)
#define	SROM_INFO2		(0x0772 / 2)	
#define	SROM_INFO		(0x07be / 2)	


#define SROM_IDLE		0
#define SROM_WRITE		1
#define SROM_READ		2
#define SROM_WEN		4
#define SROM_WDS		7
#define SROM_DONE		8


#define	SRI_SZ_MASK		0x03
#define	SRI_BLANK		0x04
#define	SRI_OTP			0x80

#if !defined(ESTA_POSTMOGRIFY_REMOVAL)



#define	CIS_SIZE		PCMCIA_FCR


#define CIS_TUPLE_LEN_MAX	0xff



#define CISTPL_NULL			0x00
#define	CISTPL_VERS_1		0x15		
#define	CISTPL_MANFID		0x20		
#define CISTPL_FUNCID		0x21		
#define	CISTPL_FUNCE		0x22		
#define	CISTPL_CFTABLE		0x1b		
#define	CISTPL_END		0xff		


#define CISTPL_FID_SDIO		0x0c		


#define	LAN_TECH		1		
#define	LAN_SPEED		2		
#define	LAN_MEDIA		3		
#define	LAN_NID			4		
#define	LAN_CONN		5		



#define CFTABLE_REGWIN_2K	0x08		
#define CFTABLE_REGWIN_4K	0x10		
#define CFTABLE_REGWIN_8K	0x20		



#define	CISTPL_BRCM_HNBU	0x80



#define HNBU_SROMREV		0x00	
#define HNBU_CHIPID		0x01	
#define HNBU_BOARDREV		0x02	
#define HNBU_PAPARMS		0x03	
#define HNBU_OEM		0x04	
#define HNBU_CC			0x05	
#define	HNBU_AA			0x06	
#define	HNBU_AG			0x07	
#define HNBU_BOARDFLAGS		0x08	
#define HNBU_LEDS		0x09	
#define HNBU_CCODE		0x0a	
#define HNBU_CCKPO		0x0b	
#define HNBU_OFDMPO		0x0c	
#define HNBU_GPIOTIMER		0x0d	
#define HNBU_PAPARMS5G		0x0e	
#define HNBU_ANT5G		0x0f	
#define HNBU_RDLID		0x10	
#define HNBU_RSSISMBXA2G	0x11	
#define HNBU_RSSISMBXA5G	0x12	
#define HNBU_XTALFREQ		0x13	
#define HNBU_TRI2G		0x14	
#define HNBU_TRI5G		0x15	
#define HNBU_RXPO2G		0x16	
#define HNBU_RXPO5G		0x17	
#define HNBU_BOARDNUM		0x18	
#define HNBU_MACADDR		0x19	
#define HNBU_RDLSN		0x1a	
#define HNBU_BOARDTYPE		0x1b	
#define HNBU_LEDDC		0x1c	
#define HNBU_HNBUCIS		0x1d	
#define HNBU_RDLRNDIS		0x20	
#define HNBU_CHAINSWITCH	0x21	
#define HNBU_REGREV		0x22	
#define HNBU_FEM		0x23	
#define HNBU_PAPARMS_C0		0x24	
#define HNBU_PAPARMS_C1		0x25	
#define HNBU_PAPARMS_C2		0x26	
#define HNBU_PAPARMS_C3		0x27	
#define HNBU_PO_CCKOFDM		0x28	
#define HNBU_PO_MCS2G		0x29	
#define HNBU_PO_MCS5GM		0x2a	
#define HNBU_PO_MCS5GLH		0x2b	
#define HNBU_PO_CDD		0x2c	
#define HNBU_PO_STBC		0x2d	
#define HNBU_PO_40M		0x2e	
#define HNBU_PO_40MDUP		0x2f	

#define HNBU_RDLRWU		0x30	
#define HNBU_WPS		0x31	
#define HNBU_USBFS		0x32	
#define HNBU_SROM3SWRGN		0x80	
#define HNBU_RESERVED		0x81	
#define HNBU_CUSTOM1		0x82	
#define HNBU_CUSTOM2		0x83	
#endif 


#define SBTML_INT_ACK		0x40000		
#define SBTML_INT_EN		0x20000		


#define SBTMH_INT_STATUS	0x40000		

#endif	
