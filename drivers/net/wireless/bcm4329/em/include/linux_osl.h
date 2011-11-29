/*
 * Linux OS Independent Layer
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id: linux_osl.h,v 13.131.30.8 2010/04/26 05:42:18 Exp $
 */


#ifndef _linux_osl_h_
#define _linux_osl_h_

#include <typedefs.h>


#include <linuxver.h>


#ifdef BCMDBG_ASSERT
#define ASSERT(exp) \
	do { if (!(exp)) osl_assert(#exp, __FILE__, __LINE__); } while (0)
extern void osl_assert(char *exp, char *file, int line);
#else 
#ifdef __GNUC__
#define GCC_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#if GCC_VERSION > 30100
#define	ASSERT(exp)		do {} while (0)
#else

#define	ASSERT(exp)
#endif 
#endif 
#endif 


#define	OSL_DELAY(usec)		osl_delay(usec)
extern void osl_delay(uint usec);

#if defined(DSLCPE_DELAY)
typedef struct {
	int long_delay;
	spinlock_t *lock;
	void *wl;
	unsigned long MIPS;
} shared_osl_t;

#define	OSL_LONG_DELAY(osh, usec)		osl_long_delay(osh, usec, 0)
#define OSL_YIELD_EXEC(osh, usec)		osl_long_delay(osh, usec, 1)
extern void osl_long_delay(osl_t *osh, uint usec, bool yield);
extern int in_long_delay(osl_t *osh);
extern void osl_oshsh_init(osl_t *osh, shared_osl_t *oshsh);
#define IN_LONG_DELAY(osh)	in_long_delay(osh)
#endif


#if defined(CONFIG_PCMCIA) || defined(CONFIG_PCMCIA_MODULE)
struct pcmcia_dev {
	dev_link_t link;	
	dev_node_t node;	
	void *base;		
	size_t size;		
	void *drv;		
};
#endif 
#define	OSL_PCMCIA_READ_ATTR(osh, offset, buf, size) \
	osl_pcmcia_read_attr((osh), (offset), (buf), (size))
#define	OSL_PCMCIA_WRITE_ATTR(osh, offset, buf, size) \
	osl_pcmcia_write_attr((osh), (offset), (buf), (size))
extern void osl_pcmcia_read_attr(osl_t *osh, uint offset, void *buf, int size);
extern void osl_pcmcia_write_attr(osl_t *osh, uint offset, void *buf, int size);


#define	OSL_PCI_READ_CONFIG(osh, offset, size) \
	osl_pci_read_config((osh), (offset), (size))
#define	OSL_PCI_WRITE_CONFIG(osh, offset, size, val) \
	osl_pci_write_config((osh), (offset), (size), (val))
extern uint32 osl_pci_read_config(osl_t *osh, uint offset, uint size);
extern void osl_pci_write_config(osl_t *osh, uint offset, uint size, uint val);


#define OSL_PCI_BUS(osh)	osl_pci_bus(osh)
#define OSL_PCI_SLOT(osh)	osl_pci_slot(osh)
extern uint osl_pci_bus(osl_t *osh);
extern uint osl_pci_slot(osl_t *osh);


typedef struct {
	bool pkttag;
	uint pktalloced; 	
	bool mmbus;		
	pktfree_cb_fn_t tx_fn;  
	void *tx_ctx;		
} osl_pubinfo_t;


extern osl_t *osl_attach(void *pdev, uint bustype, bool pkttag);
extern void osl_detach(osl_t *osh);

#define PKTFREESETCB(osh, _tx_fn, _tx_ctx) \
	do { \
	   ((osl_pubinfo_t*)osh)->tx_fn = _tx_fn; \
	   ((osl_pubinfo_t*)osh)->tx_ctx = _tx_ctx; \
	} while (0)


#define BUS_SWAP32(v)		(v)

#ifdef BCMDBG_MEM

#define	MALLOC(osh, size)	osl_debug_malloc((osh), (size), __LINE__, __FILE__)
#define	MFREE(osh, addr, size)	osl_debug_mfree((osh), (addr), (size), __LINE__, __FILE__)
#define MALLOCED(osh)		osl_malloced((osh))
#define	MALLOC_DUMP(osh, b) osl_debug_memdump((osh), (b))
extern void *osl_debug_malloc(osl_t *osh, uint size, int line, char* file);
extern void osl_debug_mfree(osl_t *osh, void *addr, uint size, int line, char* file);
struct bcmstrbuf;
extern int osl_debug_memdump(osl_t *osh, struct bcmstrbuf *b);

#else

#define	MALLOC(osh, size)	osl_malloc((osh), (size))
#define	MFREE(osh, addr, size)	osl_mfree((osh), (addr), (size))
#define MALLOCED(osh)		osl_malloced((osh))

#endif	

#define	MALLOC_FAILED(osh)	osl_malloc_failed((osh))

extern void *osl_malloc(osl_t *osh, uint size);
extern void osl_mfree(osl_t *osh, void *addr, uint size);
extern uint osl_malloced(osl_t *osh);
extern uint osl_malloc_failed(osl_t *osh);


#define	DMA_CONSISTENT_ALIGN	PAGE_SIZE
#define	DMA_ALLOC_CONSISTENT(osh, size, pap, dmah, alignbits) \
	osl_dma_alloc_consistent((osh), (size), (pap))
#define	DMA_FREE_CONSISTENT(osh, va, size, pa, dmah) \
	osl_dma_free_consistent((osh), (void*)(va), (size), (pa))
extern void *osl_dma_alloc_consistent(osl_t *osh, uint size, ulong *pap);
extern void osl_dma_free_consistent(osl_t *osh, void *va, uint size, ulong pa);


#define	DMA_TX	1	
#define	DMA_RX	2	


#define	DMA_MAP(osh, va, size, direction, p, dmah) \
	osl_dma_map((osh), (va), (size), (direction))
#define	DMA_UNMAP(osh, pa, size, direction, p, dmah) \
	osl_dma_unmap((osh), (pa), (size), (direction))
extern uint osl_dma_map(osl_t *osh, void *va, uint size, int direction);
extern void osl_dma_unmap(osl_t *osh, uint pa, uint size, int direction);


#define OSL_DMADDRWIDTH(osh, addrwidth) do {} while (0)


#include <bcmsdh.h>
#define OSL_WRITE_REG(osh, r, v) (bcmsdh_reg_write(NULL, (uintptr)(r), sizeof(*(r)), (v)))
#define OSL_READ_REG(osh, r) (bcmsdh_reg_read(NULL, (uintptr)(r), sizeof(*(r))))

#define SELECT_BUS_WRITE(osh, mmap_op, bus_op) if (((osl_pubinfo_t*)(osh))->mmbus) \
	mmap_op else bus_op
#define SELECT_BUS_READ(osh, mmap_op, bus_op) (((osl_pubinfo_t*)(osh))->mmbus) ? \
	mmap_op : bus_op


#ifndef BINOSL


#ifndef printf
#define	printf(fmt, args...)	printk(fmt, ## args)
#endif 
#include <linux/kernel.h>
#include <linux/string.h>


#ifndef IL_BIGENDIAN
#define R_REG(osh, r) (\
	SELECT_BUS_READ(osh, sizeof(*(r)) == sizeof(uint8) ? readb((volatile uint8*)(r)) : \
	sizeof(*(r)) == sizeof(uint16) ? readw((volatile uint16*)(r)) : \
	readl((volatile uint32*)(r)), OSL_READ_REG(osh, r)) \
)
#define W_REG(osh, r, v) do { \
	SELECT_BUS_WRITE(osh,  \
		switch (sizeof(*(r))) { \
			case sizeof(uint8):	writeb((uint8)(v), (volatile uint8*)(r)); break; \
			case sizeof(uint16):	writew((uint16)(v), (volatile uint16*)(r)); break; \
			case sizeof(uint32):	writel((uint32)(v), (volatile uint32*)(r)); break; \
		}, \
		(OSL_WRITE_REG(osh, r, v))); \
	} while (0)
#else	
#define R_REG(osh, r) (\
	SELECT_BUS_READ(osh, \
		({ \
			__typeof(*(r)) __osl_v; \
			switch (sizeof(*(r))) { \
				case sizeof(uint8):	__osl_v = \
					readb((volatile uint8*)((uintptr)(r)^3)); break; \
				case sizeof(uint16):	__osl_v = \
					readw((volatile uint16*)((uintptr)(r)^2)); break; \
				case sizeof(uint32):	__osl_v = \
					readl((volatile uint32*)(r)); break; \
			} \
			__osl_v; \
		}), \
		OSL_READ_REG(osh, r)) \
)
#define W_REG(osh, r, v) do { \
	SELECT_BUS_WRITE(osh,  \
		switch (sizeof(*(r))) { \
			case sizeof(uint8):	writeb((uint8)(v), \
					(volatile uint8*)((uintptr)(r)^3)); break; \
			case sizeof(uint16):	writew((uint16)(v), \
					(volatile uint16*)((uintptr)(r)^2)); break; \
			case sizeof(uint32):	writel((uint32)(v), \
					(volatile uint32*)(r)); break; \
		}, \
		(OSL_WRITE_REG(osh, r, v))); \
	} while (0)
#endif 

#define	AND_REG(osh, r, v)		W_REG(osh, (r), R_REG(osh, r) & (v))
#define	OR_REG(osh, r, v)		W_REG(osh, (r), R_REG(osh, r) | (v))


#define	bcopy(src, dst, len)	memcpy((dst), (src), (len))
#define	bcmp(b1, b2, len)	memcmp((b1), (b2), (len))
#define	bzero(b, len)		memset((b), '\0', (len))


#ifdef mips
#define OSL_UNCACHED(va)	((void*)KSEG1ADDR((va)))
#include <asm/addrspace.h>
#else
#define OSL_UNCACHED(va)	((void*)va)
#endif 


#if defined(mips)
#if defined DSLCPE_DELAY
#define	OSL_GETCYCLES(x)	((x) = read_c0_count())
#define TICKDIFF(_x2_, _x1_)	\
	((_x2_ >= _x1_) ? (_x2_ - _x1_) : ((unsigned long)(-1) - _x2_ + _x1_))
#else
#define	OSL_GETCYCLES(x)	((x) = read_c0_count() * 2)
#endif
#elif defined(__i386__)
#define	OSL_GETCYCLES(x)	rdtscl((x))
#else
#define OSL_GETCYCLES(x)	((x) = 0)
#endif 


#ifdef mips
#if defined(MODULE) && (LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 17))
#define BUSPROBE(val, addr)	panic("get_dbe() will not fixup a bus exception when compiled into"\
					" a module")
#else
#define	BUSPROBE(val, addr)	get_dbe((val), (addr))
#include <asm/paccess.h>
#endif 
#else
#define	BUSPROBE(val, addr)	({ (val) = R_REG(NULL, (addr)); 0; })
#endif 


#if !defined(CONFIG_MMC_MSM7X00A)
#define	REG_MAP(pa, size)	ioremap_nocache((unsigned long)(pa), (unsigned long)(size))
#else
#define REG_MAP(pa, size)       (void *)(0)
#endif 
#define	REG_UNMAP(va)		iounmap((va))


#define	R_SM(r)			*(r)
#define	W_SM(r, v)		(*(r) = (v))
#define	BZERO_SM(r, len)	memset((r), '\0', (len))


#define	PKTGET(osh, len, send)		osl_pktget((osh), (len))
#define	PKTFREE(osh, skb, send)		osl_pktfree((osh), (skb), (send))
#ifdef DHD_USE_STATIC_BUF
#define	PKTGET_STATIC(osh, len, send)		osl_pktget_static((osh), (len))
#define	PKTFREE_STATIC(osh, skb, send)		osl_pktfree_static((osh), (skb), (send))
#endif 
#define	PKTDATA(osh, skb)		(((struct sk_buff*)(skb))->data)
#define	PKTLEN(osh, skb)		(((struct sk_buff*)(skb))->len)
#define PKTHEADROOM(osh, skb)		(PKTDATA(osh, skb)-(((struct sk_buff*)(skb))->head))
#define PKTTAILROOM(osh, skb) ((((struct sk_buff*)(skb))->end)-(((struct sk_buff*)(skb))->tail))
#define	PKTNEXT(osh, skb)		(((struct sk_buff*)(skb))->next)
#define	PKTSETNEXT(osh, skb, x)		(((struct sk_buff*)(skb))->next = (struct sk_buff*)(x))
#define	PKTSETLEN(osh, skb, len)	__skb_trim((struct sk_buff*)(skb), (len))
#define	PKTPUSH(osh, skb, bytes)	skb_push((struct sk_buff*)(skb), (bytes))
#define	PKTPULL(osh, skb, bytes)	skb_pull((struct sk_buff*)(skb), (bytes))
#define	PKTDUP(osh, skb)		osl_pktdup((osh), (skb))
#define	PKTTAG(skb)			((void*)(((struct sk_buff*)(skb))->cb))
#define PKTALLOCED(osh)			((osl_pubinfo_t *)(osh))->pktalloced
#define PKTSETPOOL(osh, skb, x, y)	do {} while (0)
#define PKTPOOL(osh, skb)		FALSE
#define PKTPOOLLEN(osh, pktp)		(0)
#define PKTPOOLAVAIL(osh, pktp)		(0)
#define PKTPOOLADD(osh, pktp, p)	BCME_ERROR
#define PKTPOOLGET(osh, pktp)		NULL
#ifdef BCMDBG_PKT     
#define PKTLIST_DUMP(osh, buf) 		osl_pktlist_dump(osh, buf)
#else 
#define PKTLIST_DUMP(osh, buf)
#endif 

extern void *osl_pktget(osl_t *osh, uint len);
extern void osl_pktfree(osl_t *osh, void *skb, bool send);
extern void *osl_pktget_static(osl_t *osh, uint len);
extern void osl_pktfree_static(osl_t *osh, void *skb, bool send);
extern void *osl_pktdup(osl_t *osh, void *skb);

#ifdef BCMDBG_PKT     
extern void osl_pktlist_add(osl_t *osh, void *p);
extern void osl_pktlist_remove(osl_t *osh, void *p);
extern char *osl_pktlist_dump(osl_t *osh, char *buf);
#endif 


static INLINE void *
osl_pkt_frmnative(osl_pubinfo_t *osh, struct sk_buff *skb)
{
	struct sk_buff *nskb;

	if (osh->pkttag)
		bzero((void*)skb->cb, OSL_PKTTAG_SZ);

	
	for (nskb = skb; nskb; nskb = nskb->next) {
#ifdef BCMDBG_PKT
		osl_pktlist_add((osl_t *)osh, (void *) nskb);
#endif  
		osh->pktalloced++;
	}

	return (void *)skb;
}
#define PKTFRMNATIVE(osh, skb)	osl_pkt_frmnative(((osl_pubinfo_t *)osh), (struct sk_buff*)(skb))


static INLINE struct sk_buff *
osl_pkt_tonative(osl_pubinfo_t *osh, void *pkt)
{
	struct sk_buff *nskb;

	if (osh->pkttag)
		bzero(((struct sk_buff*)pkt)->cb, OSL_PKTTAG_SZ);

	
	for (nskb = (struct sk_buff *)pkt; nskb; nskb = nskb->next) {
#ifdef BCMDBG_PKT
		osl_pktlist_remove((osl_t *)osh, (void *) nskb);
#endif  
		osh->pktalloced--;
	}

	return (struct sk_buff *)pkt;
}
#define PKTTONATIVE(osh, pkt)		osl_pkt_tonative((osl_pubinfo_t *)(osh), (pkt))

#define	PKTLINK(skb)			(((struct sk_buff*)(skb))->prev)
#define	PKTSETLINK(skb, x)		(((struct sk_buff*)(skb))->prev = (struct sk_buff*)(x))
#define	PKTPRIO(skb)			(((struct sk_buff*)(skb))->priority)
#define	PKTSETPRIO(skb, x)		(((struct sk_buff*)(skb))->priority = (x))
#define PKTSUMNEEDED(skb)		(((struct sk_buff*)(skb))->ip_summed == CHECKSUM_HW)
#define PKTSETSUMGOOD(skb, x)		(((struct sk_buff*)(skb))->ip_summed = \
						((x) ? CHECKSUM_UNNECESSARY : CHECKSUM_NONE))

#define PKTSHARED(skb)                  (((struct sk_buff*)(skb))->cloned)

#else	


#ifndef LINUX_OSL
#undef printf
#define	printf(fmt, args...)		osl_printf((fmt), ## args)
#undef sprintf
#define sprintf(buf, fmt, args...)	osl_sprintf((buf), (fmt), ## args)
#undef strcmp
#define	strcmp(s1, s2)			osl_strcmp((s1), (s2))
#undef strncmp
#define	strncmp(s1, s2, n)		osl_strncmp((s1), (s2), (n))
#undef strlen
#define strlen(s)			osl_strlen((s))
#undef strcpy
#define	strcpy(d, s)			osl_strcpy((d), (s))
#undef strncpy
#define	strncpy(d, s, n)		osl_strncpy((d), (s), (n))
#endif 
extern int osl_printf(const char *format, ...);
extern int osl_sprintf(char *buf, const char *format, ...);
extern int osl_strcmp(const char *s1, const char *s2);
extern int osl_strncmp(const char *s1, const char *s2, uint n);
extern int osl_strlen(const char *s);
extern char* osl_strcpy(char *d, const char *s);
extern char* osl_strncpy(char *d, const char *s, uint n);


#define R_REG(osh, r) OSL_READ_REG(osh, r)
#define W_REG(osh, r, v) do { OSL_WRITE_REG(osh, r, v); } while (0)

#define	AND_REG(osh, r, v)		W_REG(osh, (r), R_REG(osh, r) & (v))
#define	OR_REG(osh, r, v)		W_REG(osh, (r), R_REG(osh, r) | (v))
extern uint8 osl_readb(volatile uint8 *r);
extern uint16 osl_readw(volatile uint16 *r);
extern uint32 osl_readl(volatile uint32 *r);
extern void osl_writeb(uint8 v, volatile uint8 *r);
extern void osl_writew(uint16 v, volatile uint16 *r);
extern void osl_writel(uint32 v, volatile uint32 *r);


extern void bcopy(const void *src, void *dst, size_t len);
extern int bcmp(const void *b1, const void *b2, size_t len);
extern void bzero(void *b, size_t len);


#define OSL_UNCACHED(va)	osl_uncached((va))
extern void *osl_uncached(void *va);


#define OSL_GETCYCLES(x)	((x) = osl_getcycles())
extern uint osl_getcycles(void);


#define	BUSPROBE(val, addr)	osl_busprobe(&(val), (addr))
extern int osl_busprobe(uint32 *val, uint32 addr);


#define	REG_MAP(pa, size)	osl_reg_map((pa), (size))
#define	REG_UNMAP(va)		osl_reg_unmap((va))
extern void *osl_reg_map(uint32 pa, uint size);
extern void osl_reg_unmap(void *va);


#define	R_SM(r)			*(r)
#define	W_SM(r, v)		(*(r) = (v))
#define	BZERO_SM(r, len)	bzero((r), (len))


#define	PKTGET(osh, len, send)		osl_pktget((osh), (len))
#define	PKTFREE(osh, skb, send)		osl_pktfree((osh), (skb), (send))
#define	PKTDATA(osh, skb)		osl_pktdata((osh), (skb))
#define	PKTLEN(osh, skb)		osl_pktlen((osh), (skb))
#define PKTHEADROOM(osh, skb)		osl_pktheadroom((osh), (skb))
#define PKTTAILROOM(osh, skb)		osl_pkttailroom((osh), (skb))
#define	PKTNEXT(osh, skb)		osl_pktnext((osh), (skb))
#define	PKTSETNEXT(osh, skb, x)		osl_pktsetnext((skb), (x))
#define	PKTSETLEN(osh, skb, len)	osl_pktsetlen((osh), (skb), (len))
#define	PKTPUSH(osh, skb, bytes)	osl_pktpush((osh), (skb), (bytes))
#define	PKTPULL(osh, skb, bytes)	osl_pktpull((osh), (skb), (bytes))
#define	PKTDUP(osh, skb)		osl_pktdup((osh), (skb))
#define PKTTAG(skb)			osl_pkttag((skb))
#define PKTFRMNATIVE(osh, skb)		osl_pkt_frmnative((osh), (struct sk_buff*)(skb))
#define PKTTONATIVE(osh, pkt)		osl_pkt_tonative((osh), (pkt))
#define	PKTLINK(skb)			osl_pktlink((skb))
#define	PKTSETLINK(skb, x)		osl_pktsetlink((skb), (x))
#define	PKTPRIO(skb)			osl_pktprio((skb))
#define	PKTSETPRIO(skb, x)		osl_pktsetprio((skb), (x))
#define PKTSHARED(skb)                  osl_pktshared((skb))
#define PKTALLOCED(osh)			osl_pktalloced((osh))
#ifdef BCMDBG_PKT
#define PKTLIST_DUMP(osh, buf) 		osl_pktlist_dump(osh, buf)
#else 
#define PKTLIST_DUMP(osh, buf)
#endif 

extern void *osl_pktget(osl_t *osh, uint len);
extern void osl_pktfree(osl_t *osh, void *skb, bool send);
extern uchar *osl_pktdata(osl_t *osh, void *skb);
extern uint osl_pktlen(osl_t *osh, void *skb);
extern uint osl_pktheadroom(osl_t *osh, void *skb);
extern uint osl_pkttailroom(osl_t *osh, void *skb);
extern void *osl_pktnext(osl_t *osh, void *skb);
extern void osl_pktsetnext(void *skb, void *x);
extern void osl_pktsetlen(osl_t *osh, void *skb, uint len);
extern uchar *osl_pktpush(osl_t *osh, void *skb, int bytes);
extern uchar *osl_pktpull(osl_t *osh, void *skb, int bytes);
extern void *osl_pktdup(osl_t *osh, void *skb);
extern void *osl_pkttag(void *skb);
extern void *osl_pktlink(void *skb);
extern void osl_pktsetlink(void *skb, void *x);
extern uint osl_pktprio(void *skb);
extern void osl_pktsetprio(void *skb, uint x);
extern void *osl_pkt_frmnative(osl_t *osh, struct sk_buff *skb);
extern struct sk_buff *osl_pkt_tonative(osl_t *osh, void *pkt);
extern bool osl_pktshared(void *skb);
extern uint osl_pktalloced(osl_t *osh);

#ifdef BCMDBG_PKT     
extern char *osl_pktlist_dump(osl_t *osh, char *buf);
extern void osl_pktlist_add(osl_t *osh, void *p);
extern void osl_pktlist_remove(osl_t *osh, void *p);
#endif 

#endif	

#define OSL_ERROR(bcmerror)	osl_error(bcmerror)
extern int osl_error(int bcmerror);


#define	PKTBUFSZ	2048   


#define OSL_SYSUPTIME()		((uint32)jiffies * (1000 / HZ))
#endif	
