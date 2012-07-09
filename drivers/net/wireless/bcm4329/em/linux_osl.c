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
 * $Id: linux_osl.c,v 1.125.12.3.8.7 2010/05/04 21:10:04 Exp $
 */


#define LINUX_OSL
#if defined(CHROMIUMOS_COMPAT_WIRELESS)
#include <linux/sched.h>
#endif
#include <typedefs.h>
#include <bcmendian.h>
#include <linuxver.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <linux/delay.h>
#ifdef mips
#include <asm/paccess.h>
#endif 
#include <pcicfg.h>

#define PCI_CFG_RETRY 		10

#define OS_HANDLE_MAGIC		0x1234abcd	
#define BCM_MEM_FILENAME_LEN 	24		

#ifdef DHD_USE_STATIC_BUF
#define MAX_STATIC_BUF_NUM 16
#define STATIC_BUF_SIZE	(PAGE_SIZE*2)
#define STATIC_BUF_TOTAL_LEN (MAX_STATIC_BUF_NUM*STATIC_BUF_SIZE)
typedef struct bcm_static_buf {
	struct semaphore static_sem;
	unsigned char *buf_ptr;
	unsigned char buf_use[MAX_STATIC_BUF_NUM];
} bcm_static_buf_t;

static bcm_static_buf_t *bcm_static_buf = 0;

#define MAX_STATIC_PKT_NUM 8
typedef struct bcm_static_pkt {
	struct sk_buff *skb_4k[MAX_STATIC_PKT_NUM];
	struct sk_buff *skb_8k[MAX_STATIC_PKT_NUM];
	struct semaphore osl_pkt_sem;
	unsigned char pkt_use[MAX_STATIC_PKT_NUM*2];
} bcm_static_pkt_t;
static bcm_static_pkt_t *bcm_static_skb = 0;

#endif 
typedef struct bcm_mem_link {
	struct bcm_mem_link *prev;
	struct bcm_mem_link *next;
	uint	size;
	int	line;
	char	file[BCM_MEM_FILENAME_LEN];
} bcm_mem_link_t;

struct osl_info {
	osl_pubinfo_t pub;
	uint magic;
	void *pdev;
	uint malloced;
	uint failed;
	uint bustype;
	bcm_mem_link_t *dbgmem_list;
#if defined(DSLCPE_DELAY)
	shared_osl_t *oshsh; 
#endif
#ifdef BCMDBG_PKT      
	pktlist_info_t pktlist;
#endif  
};

static int16 linuxbcmerrormap[] =
{	0, 			
	-EINVAL,		
	-EINVAL,		
	-EINVAL,		
	-EINVAL,		
	-EINVAL,		
	-EINVAL,		
	-EINVAL,		
	-EINVAL,		
	-EINVAL,		
	-EINVAL,		
	-EINVAL, 		
	-EINVAL, 		
	-EINVAL, 		
	-E2BIG,			
	-E2BIG,			
	-EBUSY, 		
	-EINVAL, 		
	-EINVAL, 		
	-EINVAL, 		
	-EINVAL, 		
	-EFAULT, 		
	-ENOMEM, 		
	-EOPNOTSUPP,		
	-EMSGSIZE,		
	-EINVAL,		
	-EPERM,			
	-ENOMEM, 		
	-EINVAL, 		
	-ERANGE, 		
	-EINVAL, 		
	-EINVAL, 		
	-EINVAL, 		
	-EINVAL, 		
	-EINVAL,		
	-EIO,			
	-ENODEV,		
	-EINVAL,		
	-EIO,			
	-EIO,			
	-EINVAL,		
	-EINVAL,		



#if BCME_LAST != -41
#error "You need to add a OS error translation in the linuxbcmerrormap \
	for new error code defined in bcmutils.h"
#endif 
};


int
osl_error(int bcmerror)
{
	if (bcmerror > 0)
		bcmerror = 0;
	else if (bcmerror < BCME_LAST)
		bcmerror = BCME_ERROR;

	
	return linuxbcmerrormap[-bcmerror];
}

void * dhd_os_prealloc(int section, unsigned long size);
osl_t *
osl_attach(void *pdev, uint bustype, bool pkttag)
{
	osl_t *osh;

	osh = kmalloc(sizeof(osl_t), GFP_ATOMIC);
	ASSERT(osh);

	bzero(osh, sizeof(osl_t));

	
	ASSERT(ABS(BCME_LAST) == (ARRAYSIZE(linuxbcmerrormap) - 1));

	osh->magic = OS_HANDLE_MAGIC;
	osh->malloced = 0;
	osh->failed = 0;
	osh->dbgmem_list = NULL;
	osh->pdev = pdev;
	osh->pub.pkttag = pkttag;
	osh->bustype = bustype;

	switch (bustype) {
		case PCI_BUS:
		case SI_BUS:
		case PCMCIA_BUS:
			osh->pub.mmbus = TRUE;
			break;
		case JTAG_BUS:
		case SDIO_BUS:
		case USB_BUS:
		case SPI_BUS:
			osh->pub.mmbus = FALSE;
			break;
		default:
			ASSERT(FALSE);
			break;
	}

#ifdef DHD_USE_STATIC_BUF


	if (!bcm_static_buf) {
		if (!(bcm_static_buf = (bcm_static_buf_t *)dhd_os_prealloc(3, STATIC_BUF_SIZE+
			STATIC_BUF_TOTAL_LEN))) {
			printk("can not alloc static buf!\n");
		}
		else
			printk("alloc static buf at %x!\n", (unsigned int)bcm_static_buf);

		
		init_MUTEX(&bcm_static_buf->static_sem);

		
		bcm_static_buf->buf_ptr = (unsigned char *)bcm_static_buf + STATIC_BUF_SIZE;

	}
	
	if (!bcm_static_skb)
	{
		int i;
		void *skb_buff_ptr = 0;
		bcm_static_skb = (bcm_static_pkt_t *)((char *)bcm_static_buf + 2048);
		skb_buff_ptr = dhd_os_prealloc(4, 0);

		bcopy(skb_buff_ptr, bcm_static_skb, sizeof(struct sk_buff *)*16);
		for (i = 0; i < MAX_STATIC_PKT_NUM*2; i++)
			bcm_static_skb->pkt_use[i] = 0;

		init_MUTEX(&bcm_static_skb->osl_pkt_sem);
	}
#endif 
#ifdef BCMDBG
	if (pkttag) {
		struct sk_buff *skb;
		ASSERT(OSL_PKTTAG_SZ <= sizeof(skb->cb));
	}
#endif
	return osh;
}

void
osl_detach(osl_t *osh)
{
	if (osh == NULL)
		return;

#ifdef DHD_USE_STATIC_BUF
	if (bcm_static_buf) {
		bcm_static_buf = 0;
	}
	if (bcm_static_skb) {
		bcm_static_skb = 0;
	}
#endif 
	ASSERT(osh->magic == OS_HANDLE_MAGIC);
	kfree(osh);
}


void*
osl_pktget(osl_t *osh, uint len)
{
	struct sk_buff *skb;

	if ((skb = dev_alloc_skb(len))) {
		skb_put(skb, len);
		skb->priority = 0;

#ifdef BCMDBG_PKT
	pktlist_add(&(osh->pktlist), (void *) skb);
#endif  

		osh->pub.pktalloced++;
	}

	return ((void*) skb);
}


void
osl_pktfree(osl_t *osh, void *p, bool send)
{
	struct sk_buff *skb, *nskb;

	skb = (struct sk_buff*) p;

	if (send && osh->pub.tx_fn)
		osh->pub.tx_fn(osh->pub.tx_ctx, p, 0);

	
	while (skb) {
		nskb = skb->next;
		skb->next = NULL;

#ifdef BCMDBG_PKT
		pktlist_remove(&(osh->pktlist), (void *) skb);
#endif  

		if (skb->destructor) {
			
			dev_kfree_skb_any(skb);
		} else {
			
			dev_kfree_skb(skb);
		}

		osh->pub.pktalloced--;

		skb = nskb;
	}
}

#ifdef DHD_USE_STATIC_BUF
void*
osl_pktget_static(osl_t *osh, uint len)
{
	int i = 0;
	struct sk_buff *skb;

	
	if (len > (PAGE_SIZE*2))
	{
		printk("Do we really need this big skb??\n");
		return osl_pktget(osh, len);
	}

	
	down(&bcm_static_skb->osl_pkt_sem);
	if (len <= PAGE_SIZE)
	{
		
		for (i = 0; i < MAX_STATIC_PKT_NUM; i++)
		{
			if (bcm_static_skb->pkt_use[i] == 0)
				break;
		}

		if (i != MAX_STATIC_PKT_NUM)
		{
			bcm_static_skb->pkt_use[i] = 1;
			up(&bcm_static_skb->osl_pkt_sem);

			skb = bcm_static_skb->skb_4k[i];
			skb->tail = skb->data + len;
			skb->len = len;
			
			return skb;
		}
	}

	
	for (i = 0; i < MAX_STATIC_PKT_NUM; i++)
	{
		if (bcm_static_skb->pkt_use[i+MAX_STATIC_PKT_NUM] == 0)
			break;
	}

	if (i != MAX_STATIC_PKT_NUM)
	{
		bcm_static_skb->pkt_use[i+MAX_STATIC_PKT_NUM] = 1;
		up(&bcm_static_skb->osl_pkt_sem);
		skb = bcm_static_skb->skb_8k[i];
		skb->tail = skb->data + len;
		skb->len = len;
		
		return skb;
	}


	
	up(&bcm_static_skb->osl_pkt_sem);
	printk("all static pkt in use!\n");
	return osl_pktget(osh, len);
}


void
osl_pktfree_static(osl_t *osh, void *p, bool send)
{
	int i;
	
	for (i = 0; i < MAX_STATIC_PKT_NUM*2; i++)
	{
		if (p == bcm_static_skb->skb_4k[i])
		{
			down(&bcm_static_skb->osl_pkt_sem);
			bcm_static_skb->pkt_use[i] = 0;
			up(&bcm_static_skb->osl_pkt_sem);

			
			return;
		}
	}
	return osl_pktfree(osh, p, send);
}
#endif 
uint32
osl_pci_read_config(osl_t *osh, uint offset, uint size)
{
	uint val = 0;
	uint retry = PCI_CFG_RETRY;

	ASSERT((osh && (osh->magic == OS_HANDLE_MAGIC)));

	
	ASSERT(size == 4);

	do {
		pci_read_config_dword(osh->pdev, offset, &val);
		if (val != 0xffffffff)
			break;
	} while (retry--);

#ifdef BCMDBG
	if (retry < PCI_CFG_RETRY)
		printk("PCI CONFIG READ access to %d required %d retries\n", offset,
		       (PCI_CFG_RETRY - retry));
#endif 

	return (val);
}

void
osl_pci_write_config(osl_t *osh, uint offset, uint size, uint val)
{
	uint retry = PCI_CFG_RETRY;

	ASSERT((osh && (osh->magic == OS_HANDLE_MAGIC)));

	
	ASSERT(size == 4);

	do {
		pci_write_config_dword(osh->pdev, offset, val);
		if (offset != PCI_BAR0_WIN)
			break;
		if (osl_pci_read_config(osh, offset, size) == val)
			break;
	} while (retry--);

#ifdef BCMDBG
	if (retry < PCI_CFG_RETRY)
		printk("PCI CONFIG WRITE access to %d required %d retries\n", offset,
		       (PCI_CFG_RETRY - retry));
#endif 
}


uint
osl_pci_bus(osl_t *osh)
{
	ASSERT(osh && (osh->magic == OS_HANDLE_MAGIC) && osh->pdev);

	return ((struct pci_dev *)osh->pdev)->bus->number;
}


uint
osl_pci_slot(osl_t *osh)
{
	ASSERT(osh && (osh->magic == OS_HANDLE_MAGIC) && osh->pdev);

	return PCI_SLOT(((struct pci_dev *)osh->pdev)->devfn);
}

static void
osl_pcmcia_attr(osl_t *osh, uint offset, char *buf, int size, bool write)
{
#if (defined(CONFIG_PCMCIA) || defined(CONFIG_PCMCIA_MODULE)) && defined(BCMPCMCIA)
	struct pcmcia_dev *dev = osh->pdev;
	dev_link_t *link = (dev_link_t *) &dev->link;
	modwin_t mod;
	memreq_t mem;
	int ret;

	
	ASSERT(link->win);
	
	ASSERT(dev->base && dev->size);

	
	mod.Attributes = WIN_ENABLE | WIN_MEMORY_TYPE_AM;
	mod.AccessSpeed = 0;
	ret = pcmcia_modify_window(link->win, &mod);
	if (ret != CS_SUCCESS) {
		cs_error(link->handle, ModifyWindow, ret);
		return;
	}

	
	offset *= 2;

	
	mem.CardOffset = offset & ~(dev->size - 1);
	mem.Page = 0;
	while (size > 0) {
		ret = pcmcia_map_mem_page(link->win, &mem);
		if (ret != CS_SUCCESS) {
			cs_error(link->handle, MapMemPage, ret);
			break;
		}
		for (offset &= (dev->size - 1);
		     offset < dev->size && size > 0;
		     offset += 2, buf++, size--) {
			if (write)
				writeb(*buf, (char *) dev->base + offset);
			else
				*buf = readb((char *) dev->base + offset);
		}
		mem.CardOffset += dev->size;
	}

	
	mod.Attributes = WIN_ENABLE | WIN_MEMORY_TYPE_CM;
	mod.AccessSpeed = 0;
	ret = pcmcia_modify_window(link->win, &mod);
	if (ret != CS_SUCCESS) {
		cs_error(link->handle, ModifyWindow, ret);
		return;
	}
#endif 
}

void
osl_pcmcia_read_attr(osl_t *osh, uint offset, void *buf, int size)
{
	osl_pcmcia_attr(osh, offset, (char *) buf, size, FALSE);
}

void
osl_pcmcia_write_attr(osl_t *osh, uint offset, void *buf, int size)
{
	osl_pcmcia_attr(osh, offset, (char *) buf, size, TRUE);
}


#ifdef BCMDBG_MEM

void*
osl_debug_malloc(osl_t *osh, uint size, int line, char* file)
{
	bcm_mem_link_t *p;
	char* basename;

	ASSERT(size);

	if ((p = (bcm_mem_link_t*)osl_malloc(osh, sizeof(bcm_mem_link_t) + size)) == NULL)
		return (NULL);

	p->size = size;
	p->line = line;

	basename = strrchr(file, '/');
	
	if (basename)
		basename++;

	if (!basename)
		basename = file;

	strncpy(p->file, basename, BCM_MEM_FILENAME_LEN);
	p->file[BCM_MEM_FILENAME_LEN - 1] = '\0';

	
	p->prev = NULL;
	p->next = osh->dbgmem_list;
	if (p->next)
		p->next->prev = p;
	osh->dbgmem_list = p;

	return p + 1;
}

void
osl_debug_mfree(osl_t *osh, void *addr, uint size, int line, char* file)
{
	bcm_mem_link_t *p = (bcm_mem_link_t *)((int8*)addr - sizeof(bcm_mem_link_t));

	ASSERT((osh && (osh->magic == OS_HANDLE_MAGIC)));

	if (p->size == 0) {
		printk("osl_debug_mfree: double free on addr %p size %d at line %d file %s\n",
			addr, size, line, file);
		ASSERT(p->size);
		return;
	}

	if (p->size != size) {
		printk("osl_debug_mfree: dealloc size %d does not match alloc size %d on addr %p"
		       " at line %d file %s\n",
		       size, p->size, addr, line, file);
		ASSERT(p->size == size);
		return;
	}

	
	if (p->prev)
		p->prev->next = p->next;
	if (p->next)
		p->next->prev = p->prev;
	if (osh->dbgmem_list == p)
		osh->dbgmem_list = p->next;
	p->next = p->prev = NULL;

	osl_mfree(osh, p, size + sizeof(bcm_mem_link_t));
}

int
osl_debug_memdump(osl_t *osh, struct bcmstrbuf *b)
{
	bcm_mem_link_t *p;

	ASSERT((osh && (osh->magic == OS_HANDLE_MAGIC)));

	bcm_bprintf(b, "   Address\tSize\tFile:line\n");
	for (p = osh->dbgmem_list; p; p = p->next)
		bcm_bprintf(b, "0x%08x\t%5d\t%s:%d\n",
			(uintptr)p + sizeof(bcm_mem_link_t), p->size, p->file, p->line);

	return 0;
}

#endif	

void*
osl_malloc(osl_t *osh, uint size)
{
	void *addr;

	
	if (osh)
		ASSERT(osh->magic == OS_HANDLE_MAGIC);

#ifdef DHD_USE_STATIC_BUF
	if (bcm_static_buf)
	{
		int i = 0;
		if ((size >= PAGE_SIZE)&&(size <= STATIC_BUF_SIZE))
		{
			down(&bcm_static_buf->static_sem);
			
			for (i = 0; i < MAX_STATIC_BUF_NUM; i++)
			{
				if (bcm_static_buf->buf_use[i] == 0)
					break;
			}
			
			if (i == MAX_STATIC_BUF_NUM)
			{
				up(&bcm_static_buf->static_sem);
				printk("all static buff in use!\n");
				goto original;
			}
			
			bcm_static_buf->buf_use[i] = 1;
			up(&bcm_static_buf->static_sem);

			bzero(bcm_static_buf->buf_ptr+STATIC_BUF_SIZE*i, size);
			if (osh)
				osh->malloced += size;

			return ((void *)(bcm_static_buf->buf_ptr+STATIC_BUF_SIZE*i));
		}
	}
original:
#endif 

	if ((addr = kmalloc(size, GFP_ATOMIC)) == NULL) {
		if (osh)
			osh->failed++;
		return (NULL);
	}
	if (osh)
		osh->malloced += size;

	return (addr);
}

void
osl_mfree(osl_t *osh, void *addr, uint size)
{
#ifdef DHD_USE_STATIC_BUF
	if (bcm_static_buf)
	{
		if ((addr > (void *)bcm_static_buf) && ((unsigned char *)addr
			<= ((unsigned char *)bcm_static_buf + STATIC_BUF_TOTAL_LEN)))
		{
			int buf_idx = 0;
			
			buf_idx = ((unsigned char *)addr - bcm_static_buf->buf_ptr)/STATIC_BUF_SIZE;
			
			down(&bcm_static_buf->static_sem);
			bcm_static_buf->buf_use[buf_idx] = 0;
			up(&bcm_static_buf->static_sem);

			if (osh) {
				ASSERT(osh->magic == OS_HANDLE_MAGIC);
				osh->malloced -= size;
			}
			return;
		}
	}
#endif 
	if (osh) {
		ASSERT(osh->magic == OS_HANDLE_MAGIC);
		osh->malloced -= size;
	}
	kfree(addr);
}

uint
osl_malloced(osl_t *osh)
{
	ASSERT((osh && (osh->magic == OS_HANDLE_MAGIC)));
	return (osh->malloced);
}

uint
osl_malloc_failed(osl_t *osh)
{
	ASSERT((osh && (osh->magic == OS_HANDLE_MAGIC)));
	return (osh->failed);
}

void*
osl_dma_alloc_consistent(osl_t *osh, uint size, ulong *pap)
{
	ASSERT((osh && (osh->magic == OS_HANDLE_MAGIC)));

	return (pci_alloc_consistent(osh->pdev, size, (dma_addr_t*)pap));
}

void
osl_dma_free_consistent(osl_t *osh, void *va, uint size, ulong pa)
{
	ASSERT((osh && (osh->magic == OS_HANDLE_MAGIC)));

	pci_free_consistent(osh->pdev, size, va, (dma_addr_t)pa);
}

uint
osl_dma_map(osl_t *osh, void *va, uint size, int direction)
{
	int dir;

	ASSERT((osh && (osh->magic == OS_HANDLE_MAGIC)));
	dir = (direction == DMA_TX)? PCI_DMA_TODEVICE: PCI_DMA_FROMDEVICE;
	return (pci_map_single(osh->pdev, va, size, dir));
}

void
osl_dma_unmap(osl_t *osh, uint pa, uint size, int direction)
{
	int dir;

	ASSERT((osh && (osh->magic == OS_HANDLE_MAGIC)));
	dir = (direction == DMA_TX)? PCI_DMA_TODEVICE: PCI_DMA_FROMDEVICE;
	pci_unmap_single(osh->pdev, (uint32)pa, size, dir);
}

#ifdef BCMDBG_ASSERT
void
osl_assert(char *exp, char *file, int line)
{
	char tempbuf[256];

	sprintf(tempbuf, "assertion \"%s\" failed: file \"%s\", line %d\n", exp, file, line);

	if (!in_interrupt()) {
		const int delay = 3;
		
		printk("%s", tempbuf);
		printk("panic in %d seconds\n", delay);
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(delay * HZ);
	}

	panic(tempbuf);
}
#endif 

void
osl_delay(uint usec)
{
	uint d;

	while (usec > 0) {
		d = MIN(usec, 1000);
		udelay(d);
		usec -= d;
	}
}

#if defined(DSLCPE_DELAY)

void
osl_oshsh_init(osl_t *osh, shared_osl_t* oshsh)
{
	extern unsigned long loops_per_jiffy;
	osh->oshsh = oshsh;
	osh->oshsh->MIPS = loops_per_jiffy / (500000/HZ);
}

int
in_long_delay(osl_t *osh)
{
	return osh->oshsh->long_delay;
}

void
osl_long_delay(osl_t *osh, uint usec, bool yield)
{
	uint d;
	bool yielded = TRUE;
	int usec_to_delay = usec;
	unsigned long tick1, tick2, tick_diff = 0;

	
	while (usec_to_delay > 0) {
		if (!yield || !yielded) {
			d = MIN(usec_to_delay, 10);
			udelay(d);
			usec_to_delay -= d;
		}
		if (usec_to_delay > 0) {
			osh->oshsh->long_delay++;
			OSL_GETCYCLES(tick1);
			spin_unlock_bh(osh->oshsh->lock);
			if (usec_to_delay > 0 && !in_irq() && !in_softirq() && !in_interrupt()) {
				schedule();
				yielded = TRUE;
			} else {
				yielded = FALSE;
			}
			spin_lock_bh(osh->oshsh->lock);
			OSL_GETCYCLES(tick2);

			if (yielded) {
				tick_diff = TICKDIFF(tick2, tick1);
				tick_diff = (tick_diff * 2)/(osh->oshsh->MIPS);
				if (tick_diff) {
					usec_to_delay -= tick_diff;
				} else
					yielded = 0;
			}
			osh->oshsh->long_delay--;
			ASSERT(osh->oshsh->long_delay >= 0);
		}
	}
}
#endif 


void *
osl_pktdup(osl_t *osh, void *skb)
{
	void * p;

	if ((p = skb_clone((struct sk_buff*)skb, GFP_ATOMIC)) == NULL)
		return NULL;

	
	if (osh->pub.pkttag)
		bzero((void*)((struct sk_buff *)p)->cb, OSL_PKTTAG_SZ);

	
	osh->pub.pktalloced++;
#ifdef BCMDBG_PKT
	pktlist_add(&(osh->pktlist), (void *) p);
#endif  
	return (p);
}

#ifdef BCMDBG_PKT
char *
osl_pktlist_dump(osl_t *osh, char *buf)
{
	pktlist_dump(&(osh->pktlist), buf);
	return buf;
}

void
osl_pktlist_add(osl_t *osh, void *p)
{
	pktlist_add(&(osh->pktlist), p);
}

void
osl_pktlist_remove(osl_t *osh, void *p)
{
	pktlist_remove(&(osh->pktlist), p);
}
#endif 


#ifdef BINOSL

uint
osl_pktalloced(osl_t *osh)
{
	return (osh->pub.pktalloced);
}

int
osl_printf(const char *format, ...)
{
	va_list args;
	char buf[1024];
	int len;

	
	va_start(args, format);
	len = vsnprintf(buf, 1024, format, args);
	va_end(args);

	if (len > sizeof(buf)) {
		printk("osl_printf: buffer overrun\n");
		return (0);
	}

	return (printk(buf));
}

int
osl_sprintf(char *buf, const char *format, ...)
{
	va_list args;
	int rc;

	va_start(args, format);
	rc = vsprintf(buf, format, args);
	va_end(args);
	return (rc);
}

int
osl_strcmp(const char *s1, const char *s2)
{
	return (strcmp(s1, s2));
}

int
osl_strncmp(const char *s1, const char *s2, uint n)
{
	return (strncmp(s1, s2, n));
}

int
osl_strlen(const char *s)
{
	return (strlen(s));
}

char*
osl_strcpy(char *d, const char *s)
{
	return (strcpy(d, s));
}

char*
osl_strncpy(char *d, const char *s, uint n)
{
	return (strncpy(d, s, n));
}

void
bcopy(const void *src, void *dst, size_t len)
{
	memcpy(dst, src, len);
}

int
bcmp(const void *b1, const void *b2, size_t len)
{
	return (memcmp(b1, b2, len));
}

void
bzero(void *b, size_t len)
{
	memset(b, '\0', len);
}

uint32
osl_readl(volatile uint32 *r)
{
	return (readl(r));
}

uint16
osl_readw(volatile uint16 *r)
{
	return (readw(r));
}

uint8
osl_readb(volatile uint8 *r)
{
	return (readb(r));
}

void
osl_writel(uint32 v, volatile uint32 *r)
{
	writel(v, r);
}

void
osl_writew(uint16 v, volatile uint16 *r)
{
	writew(v, r);
}

void
osl_writeb(uint8 v, volatile uint8 *r)
{
	writeb(v, r);
}

void *
osl_uncached(void *va)
{
#ifdef mips
	return ((void*)KSEG1ADDR(va));
#else
	return ((void*)va);
#endif 
}

uint
osl_getcycles(void)
{
	uint cycles;

#if defined(mips)
	cycles = read_c0_count() * 2;
#elif defined(__i386__)
	rdtscl(cycles);
#else
	cycles = 0;
#endif 
	return cycles;
}

void *
osl_reg_map(uint32 pa, uint size)
{
	return (ioremap_nocache((unsigned long)pa, (unsigned long)size));
}

void
osl_reg_unmap(void *va)
{
	iounmap(va);
}

int
osl_busprobe(uint32 *val, uint32 addr)
{
#ifdef mips
	return get_dbe(*val, (uint32 *)addr);
#else
	*val = readl((uint32 *)(uintptr)addr);
	return 0;
#endif 
}

bool
osl_pktshared(void *skb)
{
	return (((struct sk_buff*)skb)->cloned);
}

uchar*
osl_pktdata(osl_t *osh, void *skb)
{
	return (((struct sk_buff*)skb)->data);
}

uint
osl_pktlen(osl_t *osh, void *skb)
{
	return (((struct sk_buff*)skb)->len);
}

uint
osl_pktheadroom(osl_t *osh, void *skb)
{
	return (uint) skb_headroom((struct sk_buff *) skb);
}

uint
osl_pkttailroom(osl_t *osh, void *skb)
{
	return (uint) skb_tailroom((struct sk_buff *) skb);
}

void*
osl_pktnext(osl_t *osh, void *skb)
{
	return (((struct sk_buff*)skb)->next);
}

void
osl_pktsetnext(void *skb, void *x)
{
	((struct sk_buff*)skb)->next = (struct sk_buff*)x;
}

void
osl_pktsetlen(osl_t *osh, void *skb, uint len)
{
	__skb_trim((struct sk_buff*)skb, len);
}

uchar*
osl_pktpush(osl_t *osh, void *skb, int bytes)
{
	return (skb_push((struct sk_buff*)skb, bytes));
}

uchar*
osl_pktpull(osl_t *osh, void *skb, int bytes)
{
	return (skb_pull((struct sk_buff*)skb, bytes));
}

void*
osl_pkttag(void *skb)
{
	return ((void*)(((struct sk_buff*)skb)->cb));
}

void*
osl_pktlink(void *skb)
{
	return (((struct sk_buff*)skb)->prev);
}

void
osl_pktsetlink(void *skb, void *x)
{
	((struct sk_buff*)skb)->prev = (struct sk_buff*)x;
}

uint
osl_pktprio(void *skb)
{
	return (((struct sk_buff*)skb)->priority);
}

void
osl_pktsetprio(void *skb, uint x)
{
	((struct sk_buff*)skb)->priority = x;
}


struct sk_buff *
osl_pkt_tonative(osl_t *osh, void *pkt)
{
	struct sk_buff *nskb;

	if (osh->pub.pkttag)
		bzero((void*)((struct sk_buff *)pkt)->cb, OSL_PKTTAG_SZ);

	
	for (nskb = (struct sk_buff *)pkt; nskb; nskb = nskb->next) {
#ifdef BCMDBG_PKT
		pktlist_remove(&(osh->pktlist), (void *) nskb);
#endif  
		osh->pub.pktalloced--;
	}

	return (struct sk_buff *)pkt;
}


void *
osl_pkt_frmnative(osl_t *osh, struct sk_buff *skb)
{
	struct sk_buff *nskb;

	if (osh->pub.pkttag)
		bzero((void*)skb->cb, OSL_PKTTAG_SZ);

	
	for (nskb = skb; nskb; nskb = nskb->next) {
#ifdef BCMDBG_PKT
		pktlist_add(&(osh->pktlist), (void *) nskb);
#endif  
		osh->pub.pktalloced++;
	}

	return (void *)skb;
}

#endif	
