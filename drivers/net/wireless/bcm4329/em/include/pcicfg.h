/*
 * pcicfg.h: PCI configuration constants and structures.
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: pcicfg.h,v 1.41.12.3 2008/06/26 22:49:41 Exp $
 */


#ifndef	_h_pcicfg_
#define	_h_pcicfg_


#define	PCI_CFG_VID		0
#define	PCI_CFG_CMD		4
#define	PCI_CFG_REV		8
#define	PCI_CFG_BAR0		0x10
#define	PCI_CFG_BAR1		0x14
#define	PCI_BAR0_WIN		0x80	
#define	PCI_INT_STATUS		0x90	
#define	PCI_INT_MASK		0x94	

#define PCIE_EXTCFG_OFFSET	0x100
#if defined(WLTEST)
#define PCIE_ADVERRREP_CAPID	0x0001
#define PCIE_VC_CAPID		0x0002
#define PCIE_DEVSNUM_CAPID	0x0003
#define PCIE_PWRBUDGET_CAPID	0x0004


typedef struct _pcie_enhanced_caphdr {
	unsigned short capID;
	unsigned short cap_ver : 4;
	unsigned short next_ptr : 12;
} pcie_enhanced_caphdr;






#define cap_list	rsvd_a[0]
#define bar0_window	dev_dep[0x80 - 0x40]
#define bar1_window	dev_dep[0x84 - 0x40]
#define sprom_control	dev_dep[0x88 - 0x40]
#define	PCI_BAR1_WIN		0x84	
#define	PCI_SPROM_CONTROL	0x88	

#define	PCI_BAR1_CONTROL	0x8c	
#define PCI_TO_SB_MB		0x98	
#define PCI_BACKPLANE_ADDR	0xa0	
#define PCI_BACKPLANE_DATA	0xa4	
#define	PCI_CLK_CTL_ST		0xa8	
#define	PCI_BAR0_WIN2		0xac	
#define	PCI_GPIO_IN		0xb0	
#define	PCI_GPIO_OUT		0xb4	
#define	PCI_GPIO_OUTEN		0xb8	

#define	PCI_BAR0_SHADOW_OFFSET	(2 * 1024)	
#define	PCI_BAR0_SPROM_OFFSET	(4 * 1024)	
#endif 
#define	PCI_BAR0_PCIREGS_OFFSET	(6 * 1024)	
#define	PCI_BAR0_PCISBR_OFFSET	(4 * 1024)	

#define PCI_BAR0_WINSZ		(16 * 1024)	


#define	PCI_16KB0_PCIREGS_OFFSET (8 * 1024)	
#define	PCI_16KB0_CCREGS_OFFSET	(12 * 1024)	
#define PCI_16KBB0_WINSZ	(16 * 1024)	

#if defined(WLTEST) || defined(BCMDBG_ERR) || defined(BCMDBG_ASSERT) || \
	defined(BCMDBG_DUMP)
#define	PCI_SBIM_STATUS_SERR	0x4	


#define	PCI_SBIM_SHIFT		8	
#define	PCI_SBIM_MASK		0xff00	
#define	PCI_SBIM_MASK_SERR	0x4	


#define SPROM_SZ_MSK		0x02	
#define SPROM_LOCKED		0x08	
#define	SPROM_BLANK		0x04	
#define SPROM_WRITEEN		0x10	
#define SPROM_BOOTROM_WE	0x20	
#define SPROM_BACKPLANE_EN	0x40	
#define SPROM_OTPIN_USE		0x80	

#define	SPROM_SIZE		256	
#define SPROM_CRC_RANGE		64	


#define PCI_CMD_IO		0x00000001	
#define PCI_CMD_MEMORY		0x00000002	
#define PCI_CMD_MASTER		0x00000004	
#define PCI_CMD_SPECIAL		0x00000008	
#define PCI_CMD_INVALIDATE	0x00000010	
#define PCI_CMD_VGA_PAL		0x00000040	
#define PCI_STAT_TA		0x08000000	
#endif 
#endif	
