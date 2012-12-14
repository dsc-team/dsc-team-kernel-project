/* arch/arm/mach-msm/smd.c
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2008-2010, Code Aurora Forum. All rights reserved.
 * Author: Brian Swetland <swetland@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/termios.h>
#include <linux/ctype.h>
#include <linux/remote_spinlock.h>
#include <linux/uaccess.h>
#include <mach/msm_smd.h>
#include <mach/msm_iomap.h>
#include <mach/system.h>

#include "smd_private.h"
#include "proc_comm.h"
#include "modem_notifier.h"

#include <linux/logger.h>

#include <linux/workqueue.h>
#include <linux/input.h>
#include <mach/kevent.h>

#include <linux/wakelock.h>
#include <linux/pm_qos_params.h>


#if defined (CONFIG_QSD_ARM9_CRASH_FUNCTION)
#if defined (DUMPXXX_TO_SDCARD)
#include <linux/ashmem.h>
#include <linux/file.h>
#include <linux/time.h>
#include <linux/rtc.h>
#include <linux/namei.h>
#include <linux/mount.h>
#include <linux/security.h>
#include <linux/logger.h>
#include <asm/atomic.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <linux/swap.h>
#include <linux/mman.h>
#include <linux/hugetlb.h>
#endif

#endif
/* DUMPXXX */

#if defined(CONFIG_ARCH_QSD8X50) || defined(CONFIG_ARCH_MSM8X60)
#define CONFIG_QDSP6 1
#endif

#if defined(CONFIG_ARCH_MSM8X60)
#define CONFIG_DSPS 1
#endif

#define MODULE_NAME "msm_smd"
#define SMEM_VERSION 0x000B
#define SMD_VERSION 0x00020000

enum {
    MSM_SMD_DEBUG = 1U << 0,
    MSM_SMSM_DEBUG = 1U << 1,
    MSM_SMD_INFO = 1U << 2,
    MSM_SMSM_INFO = 1U << 3,
};

struct smsm_shared_info {
    uint32_t *state;
    uint32_t *intr_mask;
    uint32_t *intr_mux;
};

static struct smsm_shared_info 		smsm_info;
static struct pm_qos_request_list       *smd_qos_req_list;

#define SMSM_STATE_ADDR(entry)           (smsm_info.state + entry)
#define SMSM_INTR_MASK_ADDR(entry, host) (smsm_info.intr_mask + \
					  entry * SMSM_NUM_HOSTS + host)
#define SMSM_INTR_MUX_ADDR(entry)        (smsm_info.intr_mux + entry)

/* Internal definitions which are not exported in some targets */
enum {
    SMSM_Q6_I = 2,
};

enum {
    SMSM_APPS_DEM_I = 3,
};

enum {
    SMD_APPS_QDSP_I = 1,
    SMD_MODEM_QDSP_I = 2
};

static int msm_smd_debug_mask;
module_param_named(debug_mask, msm_smd_debug_mask,
		   int, S_IRUGO | S_IWUSR | S_IWGRP);

#if defined(CONFIG_MSM_SMD_DEBUG)
#define SMD_DBG(x...) do {				\
		if (msm_smd_debug_mask & MSM_SMD_DEBUG) \
			printk(KERN_DEBUG x);		\
	} while (0)

#define SMSM_DBG(x...) do {					\
		if (msm_smd_debug_mask & MSM_SMSM_DEBUG)	\
			printk(KERN_DEBUG x);			\
	} while (0)

#define SMD_INFO(x...) do {			 	\
		if (msm_smd_debug_mask & MSM_SMD_INFO)	\
			printk(KERN_INFO x);		\
	} while (0)

#define SMSM_INFO(x...) do {				\
		if (msm_smd_debug_mask & MSM_SMSM_INFO) \
			printk(KERN_INFO x);		\
	} while (0)
#else
#define SMD_DBG(x...) do { } while (0)
#define SMSM_DBG(x...) do { } while (0)
#define SMD_INFO(x...) do { } while (0)
#define SMSM_INFO(x...) do { } while (0)
#endif

static unsigned last_heap_free = 0xffffffff;

#if defined(CONFIG_ARCH_MSM7X30)
#define MSM_TRIG_A2M_SMD_INT     (writel(1 << 0, MSM_GCC_BASE + 0x8))
#define MSM_TRIG_A2Q6_SMD_INT    (writel(1 << 8, MSM_GCC_BASE + 0x8))
#define MSM_TRIG_A2M_SMSM_INT    (writel(1 << 5, MSM_GCC_BASE + 0x8))
#define MSM_TRIG_A2Q6_SMSM_INT   (writel(1 << 8, MSM_GCC_BASE + 0x8))
#define MSM_TRIG_A2DSPS_SMD_INT
#elif defined(CONFIG_ARCH_MSM8X60)
#define MSM_TRIG_A2M_SMD_INT     (writel(1 << 3, MSM_GCC_BASE + 0x8))
#define MSM_TRIG_A2Q6_SMD_INT    (writel(1 << 15, MSM_GCC_BASE + 0x8))
#define MSM_TRIG_A2M_SMSM_INT    (writel(1 << 4, MSM_GCC_BASE + 0x8))
#define MSM_TRIG_A2Q6_SMSM_INT   (writel(1 << 14, MSM_GCC_BASE + 0x8))
#define MSM_TRIG_A2DSPS_SMD_INT  (writel(1, MSM_SIC_NON_SECURE_BASE + 0x4080))
#else
#define MSM_TRIG_A2M_SMD_INT     (writel(1, MSM_CSR_BASE + 0x400 + (0) * 4))
#define MSM_TRIG_A2Q6_SMD_INT    (writel(1, MSM_CSR_BASE + 0x400 + (8) * 4))
#define MSM_TRIG_A2M_SMSM_INT    (writel(1, MSM_CSR_BASE + 0x400 + (5) * 4))
#define MSM_TRIG_A2Q6_SMSM_INT   (writel(1, MSM_CSR_BASE + 0x400 + (8) * 4))
#define MSM_TRIG_A2DSPS_SMD_INT
#endif

#define SMD_LOOPBACK_CID 100

#if defined (CONFIG_QSD_ARM9_CRASH_FUNCTION)
/* #define SMD_CRASH_LOG(x...) do { logger_write(0, 7, logger_tag, x); } while (0) */
#define SMD_CRASH_LOG(x...) printk(x);
#else
#define SMD_CRASH_LOG(x...) do { } while (0)
#endif

#if defined (CONFIG_QSD_ARM9_CRASH_FUNCTION)

#define MODEM_CRASH_RESTART_TIMEOUT_MS  1000
#define MODEM_CRASH_RESTART_TIMEOUT     (msecs_to_jiffies(MODEM_CRASH_RESTART_TIMEOUT_MS))
#define MODEM_CRASH_LOGGER_PREFIX       "/sdcard/systemlog"

#ifdef CONFIG_HW_TOUCAN

#define KEY_MODEM_RESET KEY_CAMERA

#endif

#ifdef CONFIG_HW_AUSTIN

#define KEY_MODEM_RESET KEY_CAMERA
#if !(defined (CONFIG_MACH_EVB) || defined (CONFIG_MACH_EVT0))

#define MODEM_CRASH_NOTIFY_USER

extern void qi2ccapkybd_set_led(unsigned int light);

#define MODEM_CRASH_NOTIFY_USER_0 do {qi2ccapkybd_set_led(0x0);} while(0)
#define MODEM_CRASH_NOTIFY_USER_1 do {qi2ccapkybd_set_led(0xFFFFFF);} while(0)

#endif

#endif

struct wake_lock crash_lock;

static atomic_t dumpxxxed;

extern int gpio_keypad_detect_reset_modem(void);
static void smd_crash_show_errmsg(struct work_struct *work);

static DECLARE_WORK(smd_crash_worker, smd_crash_show_errmsg);
static struct delayed_work smd_crash_restart_worker;

static const char logger_tag[] = "PrintK\0";

#define ASHMEM_NAME_PREFIX "dev/ashmem/"
#define ASHMEM_NAME_PREFIX_LEN (sizeof(ASHMEM_NAME_PREFIX) - 1)
#define ASHMEM_FULL_NAME_LEN (ASHMEM_NAME_LEN + ASHMEM_NAME_PREFIX_LEN)

#ifdef DUMPXXX_TO_SDCARD

struct ashmem_area {
    char name[ASHMEM_FULL_NAME_LEN];/* optional name for /proc/pid/maps */
    struct list_head unpinned_list; /* list of all ashmem areas */
    struct file *file;      /* the shmem-based backing file */
    size_t size;            /* size of the mapping, in bytes */
    unsigned long prot_mask;    /* allowed prot bits, as vm_flags */
};
extern struct ashmem_area *system_properties_asma;

static int mk_logging_dir(const char *dirname)
{
    mm_segment_t oldfs;
    int ret = -1;
    struct nameidata nd;
    struct dentry *dentry;

    oldfs = get_fs();
    set_fs(get_ds());

    if (path_lookup(dirname, 0777, &nd))
    {
        printk(KERN_INFO "%s: path_lookup failed\n", __func__);
        goto out_fs;
    }
    
    dentry=lookup_create(&nd, 0);
    if (IS_ERR(dentry))
    {
        printk(KERN_INFO "%s: lookup_create failed\n", __func__);
        goto out_unlock;
    }
    
    if (mnt_want_write(nd.path.mnt))
    {
        printk(KERN_INFO "%s: mnt_want_write failed\n", __func__);
        goto out_dput;
    }
    if (security_path_mkdir(&nd.path, dentry, 0777))
    {
        printk(KERN_INFO "%s: security_path_mkdir failed\n", __func__);
        goto out_drop_write;
    }
    if (vfs_mkdir(nd.path.dentry->d_inode, dentry, 0777))
    {
        printk(KERN_INFO "%s: vfs_mkdir failed\n", __func__);
        goto out_drop_write;
    }
    ret = 0;
out_drop_write:
    mnt_drop_write(nd.path.mnt);
out_dput:
    dput(dentry);
out_unlock:
    mutex_unlock(&nd.path.dentry->d_inode->i_mutex);
    path_put(&nd.path);
out_fs:
    set_fs(oldfs);

    return ret;
}

#define TIME_TID_MAX_LEN        100
#define logger_offset(n)        ((n) & (size - 1))

#define DUMP_TBUF(a,b,c) if(b > 0 && b < c) \
        { \
            a[b] = 0; \
            dump2storage(a); \
        }

#define STORAGE_BUF_SIZE (2*PAGE_SIZE)
extern size_t get_head_of_mainlog(void);
extern size_t get_size_of_mainlog(void);
extern unsigned char* get_buf_of_mainlog(void);
extern size_t get_head_of_ooblog(void);
extern size_t get_size_of_ooblog(void);
extern unsigned char* get_buf_of_ooblog(void);

static unsigned char logger_storage_buf[STORAGE_BUF_SIZE];
static unsigned char logger_entry[LOGGER_ENTRY_MAX_LEN];

static int len_of_storage_buf=0;
static unsigned long long storage_offset;

struct file *storage_filp = NULL;

static int mystrlen(char *tagaddr)//include \0
{
    int i;
    for(i=0;;i++)
    {
        if(tagaddr[i] == 0)
        {
            return i+1;
        }
    }
    return 0;
}

static int is_valid_entry(unsigned char* entry) //todo
{
    struct logger_entry * ptr = (struct logger_entry *)(entry);
    char* pri = (char*)(entry+sizeof(struct logger_entry));
    char* tagaddr = (char*)(pri+1);
    char* stringaddr;
    int i,taglen;
    int tag_end_flag = 0;
    int string_end_flag = 0;
    
    if(ptr->len == 0 || ptr->len > LOGGER_ENTRY_MAX_PAYLOAD) return 0;
    
    for(i=0;i<ptr->len-1;i++)
    {
        if(tagaddr[i] == 0)
        {
            tag_end_flag = 1;
            break;
        }
    }
    if(tag_end_flag == 0) return 0;
    taglen = i+1;
    
    stringaddr = tagaddr+taglen;
    for(i=0;i<ptr->len-1-taglen;i++)
    {
        if(stringaddr[i] == 0)
        {
            string_end_flag = 1;
            break;
        }
    }
    if(string_end_flag == 0)
    {
        *(char*)(entry+sizeof(struct logger_entry)+ptr->len-1) = 0;
    }
    return 1;
}

static int flush_storage_buf(void)
{
    vfs_write(storage_filp, logger_storage_buf, len_of_storage_buf, &storage_offset);
    len_of_storage_buf = 0;

    memset(logger_storage_buf, 0, sizeof(logger_storage_buf));

    return 0;
}

static void dump2storage_len(char *data,int len)
{
    if(len_of_storage_buf + len < STORAGE_BUF_SIZE)
    {
        memcpy(logger_storage_buf+len_of_storage_buf,data,len);
        len_of_storage_buf += len;
        return;
    }
    flush_storage_buf();

    memcpy(logger_storage_buf, data, len);
    len_of_storage_buf += len;
}

static void dump2storage(char *data)
{
    int len = mystrlen(data);
    dump2storage_len(data,len);     
}

static int dump_one_entry_of_mainlog(unsigned char* buf, int offset, size_t size)
{
    struct logger_entry * ptr = (struct logger_entry *)(logger_entry);
    size_t len,t_len;
    int str_len;
    char* tagaddr;
    char* str_ptr;
    char tbuf[TIME_TID_MAX_LEN];
    
    len = min((unsigned long)LOGGER_ENTRY_MAX_LEN, (unsigned long)(size - offset));
    memcpy(logger_entry, buf+offset, len);

    if (LOGGER_ENTRY_MAX_LEN != len)
        memcpy(logger_entry+len, buf, LOGGER_ENTRY_MAX_LEN - len);
    
    if(is_valid_entry(logger_entry))
    {
        //todo : print sec
        t_len = sprintf(tbuf, "[%lu.%02lu]",(unsigned long) ptr->sec,(unsigned long)ptr->nsec / 10000000);
        DUMP_TBUF(tbuf,t_len,TIME_TID_MAX_LEN);
        //todo : print tid
        t_len = sprintf(tbuf, "(%d)",ptr->pid);
        DUMP_TBUF(tbuf,t_len,TIME_TID_MAX_LEN);
        //print tag
        tagaddr = logger_entry + sizeof(struct logger_entry) + 1;
        dump2storage((char*)(tagaddr));
        dump2storage(":");
        //print string
        dump2storage((char*)(tagaddr+mystrlen(tagaddr)));
        str_ptr = (char*)(tagaddr+mystrlen(tagaddr));
        str_len = mystrlen(str_ptr);
        if(str_ptr[str_len-2]!= '\n')
        {
            dump2storage("\n");
        }
        return sizeof(struct logger_entry) + ptr->len;
    }
    else
    {
        return -1;
    }
}

static void dump_log(size_t head, size_t size, unsigned char* buf)
{
    //size_t head = get_head_of_mainlog();
    //unsigned char* buf = get_buf_of_mainlog();
    int total_dump_size=0;
    int dump_entry_size;
    size_t offset = head;
    

    for(;;)
    {
        dump_entry_size = dump_one_entry_of_mainlog(buf,offset,size);
        if( dump_entry_size == -1) break;
        else 
            offset = logger_offset( offset + dump_entry_size);
        total_dump_size += dump_entry_size;
        if(total_dump_size >= size) break;
    }
}

static int do_dump_all_log(const char *filename)
{
    mm_segment_t oldfs;
    oldfs = get_fs();
    set_fs(get_ds());

    if (storage_filp != NULL)
    {
    printk("%s: ERROR, storage_filp should be NULL\n", __func__);
    return -1;
    }

    storage_filp = filp_open(filename, O_CREAT|O_RDWR|O_APPEND, S_IRWXU);
    if ( IS_ERR(storage_filp) )
    {
        set_fs(oldfs);
        printk(KERN_INFO "%s: Open file failed\n", __func__);
        return -1;
    }

    printk("%s: start write mainlog\n", __func__);

    dump2storage("=== dump mainlog start ===\n");
    dump_log(get_head_of_mainlog(),
        get_size_of_mainlog(),
        get_buf_of_mainlog());
    dump2storage("=== dump mainlog end ===\n");

    printk("%s: start write ooblog\n", __func__);
    
    dump2storage("=== dump ooblog start ===\n");
    dump_log(get_head_of_ooblog(),
        get_size_of_ooblog(),
        get_buf_of_ooblog());
    dump2storage("=== dump ooblog end ===\n");      

    vfs_fsync(storage_filp, storage_filp->f_dentry, 0);
    filp_close(storage_filp, NULL);
    set_fs(oldfs);

    storage_filp = NULL;

    return 0;
}

static int do_write_sysprop_log(struct ashmem_area *sysprop_ashmem, const char *filename)
{
    mm_segment_t oldfs;
    int ret = -1;
    int iindex = 0;
    unsigned long long offset = 0;
    int write_log_loop = sysprop_ashmem->size / STORAGE_BUF_SIZE;
    struct file *filp = NULL;

    memset(logger_storage_buf, 0, sizeof(logger_storage_buf));

    oldfs = get_fs();
    set_fs(get_ds());

    offset = 0;

    fput(sysprop_ashmem->file);

    filp = filp_open(filename, O_CREAT|O_RDWR|O_APPEND, S_IRWXU);
    printk("%s: start write system property\n", __func__);

    if ( IS_ERR(filp) )
    {
        set_fs(oldfs);
        printk(KERN_INFO "%s: Open file failed\n", __func__);
        return -1;
    }

    for (iindex = 0; iindex < write_log_loop; iindex++)
    {
        ret = vfs_read(system_properties_asma->file, logger_storage_buf, STORAGE_BUF_SIZE, &offset);
        ret = vfs_write(filp, logger_storage_buf, STORAGE_BUF_SIZE, &offset);
    }
    printk("%s: end write system property\n", __func__);

    vfs_fsync(filp, filp->f_dentry, 0);
    filp_close(filp, NULL);
    set_fs(oldfs);

    return 0;
}

static int do_write_smem_log(const char *filename)
{
    mm_segment_t oldfs;
    int ret = -1;
    int iindex = 0;
    unsigned long long offset = 0;
    int write_log_loop = MSM_SHARED_RAM_SIZE / STORAGE_BUF_SIZE;
    struct file *filp = NULL;
    char *psmembuf;

    memset(logger_storage_buf, 0, sizeof(logger_storage_buf));

    oldfs = get_fs();
    set_fs(get_ds());

    filp = filp_open(filename, O_CREAT|O_RDWR|O_APPEND, S_IRWXU);
    if ( IS_ERR(filp) )
    {
        set_fs(oldfs);
        printk(KERN_INFO "%s: Open file failed\n", __func__);
        return -1;
    }

    printk("%s: start write smem\n", __func__);

    psmembuf = MSM_SHARED_RAM_BASE;
    for (iindex = 0; iindex < write_log_loop; iindex++)
    {
        memcpy(logger_storage_buf, psmembuf, STORAGE_BUF_SIZE);
        ret = vfs_write(filp, logger_storage_buf, STORAGE_BUF_SIZE, &offset);
        psmembuf += STORAGE_BUF_SIZE;
    }

    printk("%s: end write smem\n", __func__);

    vfs_fsync(filp, filp->f_dentry, 0);
    filp_close(filp, NULL);
    set_fs(oldfs);

    return 0;
}
static int do_write_mtrace_log(const char *filename)
{
    mm_segment_t oldfs;
    int ret = -1;
    int iindex = 0;
    unsigned long long offset = 0;
    int write_log_loop = MSM_MTRACE_BUF_SIZE / STORAGE_BUF_SIZE;
    struct file *filp = NULL;
    char *pmtracebuf;

    memset(logger_storage_buf, 0, sizeof(logger_storage_buf));

    oldfs = get_fs();
    set_fs(get_ds());

    filp = filp_open(filename, O_CREAT|O_RDWR|O_APPEND, S_IRWXU);
    if ( IS_ERR(filp) )
    {
        set_fs(oldfs);
        printk(KERN_INFO "%s: Open file failed\n", __func__);
        return -1;
    }

    printk("%s: start write mtrace\n", __func__);

    pmtracebuf = MSM_MTRACE_BUF_BASE;
    for (iindex = 0; iindex < write_log_loop; iindex++)
    {
        memcpy(logger_storage_buf, pmtracebuf, STORAGE_BUF_SIZE);
        ret = vfs_write(filp, logger_storage_buf, STORAGE_BUF_SIZE, &offset);
        pmtracebuf += STORAGE_BUF_SIZE;
    }

    printk("%s: end write mtrace\n", __func__);

    vfs_fsync(filp, filp->f_dentry, 0);
    filp_close(filp, NULL);
    set_fs(oldfs);

    return 0;
}

struct vmalloc_info {
    unsigned long   used;
    unsigned long   largest_chunk;
};
#define get_vmalloc_info(vmi)           \
do {                        \
    (vmi)->used = 0;            \
    (vmi)->largest_chunk = 0;       \
} while(0)

#define VMALLOC_TOTAL (VMALLOC_END - VMALLOC_START)

static int do_write_meminfo(const char *filename)
{
    struct sysinfo i;
    unsigned long committed;
    unsigned long allowed;
    struct vmalloc_info vmi;
    long cached;
    unsigned long pages[NR_LRU_LISTS];
    int lru;

    int len;

    memset(logger_storage_buf, 0, sizeof(logger_storage_buf));
/*
 * display in kilobytes.
 */
#define K(x) ((x) << (PAGE_SHIFT - 10))
    si_meminfo(&i);
    si_swapinfo(&i);
    committed = atomic_long_read(&vm_committed_space);
    allowed = ((totalram_pages - hugetlb_total_pages())
        * sysctl_overcommit_ratio / 100) + total_swap_pages;

    cached = global_page_state(NR_FILE_PAGES) -
            total_swapcache_pages - i.bufferram;
    if (cached < 0)
        cached = 0;

    get_vmalloc_info(&vmi);

    for (lru = LRU_BASE; lru < NR_LRU_LISTS; lru++)
        pages[lru] = global_page_state(NR_LRU_BASE + lru);

    /*
     * Tagged format, for easy grepping and expansion.
     */
    len = sprintf(logger_storage_buf,
        "MemTotal:       %8lu kB\n"
        "MemFree:        %8lu kB\n"
        "Buffers:        %8lu kB\n"
        "Cached:         %8lu kB\n"
        "SwapCached:     %8lu kB\n"
        "Active:         %8lu kB\n"
        "Inactive:       %8lu kB\n"
        "Active(anon):   %8lu kB\n"
        "Inactive(anon): %8lu kB\n"
        "Active(file):   %8lu kB\n"
        "Inactive(file): %8lu kB\n"
#ifdef CONFIG_UNEVICTABLE_LRU
        "Unevictable:    %8lu kB\n"
        "Mlocked:        %8lu kB\n"
#endif
#ifdef CONFIG_HIGHMEM
        "HighTotal:      %8lu kB\n"
        "HighFree:       %8lu kB\n"
        "LowTotal:       %8lu kB\n"
        "LowFree:        %8lu kB\n"
#endif
#ifndef CONFIG_MMU
        "MmapCopy:       %8lu kB\n"
#endif
        "SwapTotal:      %8lu kB\n"
        "SwapFree:       %8lu kB\n"
        "Dirty:          %8lu kB\n"
        "Writeback:      %8lu kB\n"
        "AnonPages:      %8lu kB\n"
        "Mapped:         %8lu kB\n"
        "Slab:           %8lu kB\n"
        "SReclaimable:   %8lu kB\n"
        "SUnreclaim:     %8lu kB\n"
        "PageTables:     %8lu kB\n"
#ifdef CONFIG_QUICKLIST
        "Quicklists:     %8lu kB\n"
#endif
        "NFS_Unstable:   %8lu kB\n"
        "Bounce:         %8lu kB\n"
        "WritebackTmp:   %8lu kB\n"
        "CommitLimit:    %8lu kB\n"
        "Committed_AS:   %8lu kB\n"
        "VmallocTotal:   %8lu kB\n"
        "VmallocUsed:    %8lu kB\n"
        "VmallocChunk:   %8lu kB\n",
        K(i.totalram),
        K(i.freeram),
        K(i.bufferram),
        K(cached),
        K(total_swapcache_pages),
        K(pages[LRU_ACTIVE_ANON]   + pages[LRU_ACTIVE_FILE]),
        K(pages[LRU_INACTIVE_ANON] + pages[LRU_INACTIVE_FILE]),
        K(pages[LRU_ACTIVE_ANON]),
        K(pages[LRU_INACTIVE_ANON]),
        K(pages[LRU_ACTIVE_FILE]),
        K(pages[LRU_INACTIVE_FILE]),
#ifdef CONFIG_UNEVICTABLE_LRU
        K(pages[LRU_UNEVICTABLE]),
        K(global_page_state(NR_MLOCK)),
#endif
#ifdef CONFIG_HIGHMEM
        K(i.totalhigh),
        K(i.freehigh),
        K(i.totalram-i.totalhigh),
        K(i.freeram-i.freehigh),
#endif
#ifndef CONFIG_MMU
        K((unsigned long) atomic_read(&mmap_pages_allocated)),
#endif
        K(i.totalswap),
        K(i.freeswap),
        K(global_page_state(NR_FILE_DIRTY)),
        K(global_page_state(NR_WRITEBACK)),
        K(global_page_state(NR_ANON_PAGES)),
        K(global_page_state(NR_FILE_MAPPED)),
        K(global_page_state(NR_SLAB_RECLAIMABLE) +
                global_page_state(NR_SLAB_UNRECLAIMABLE)),
        K(global_page_state(NR_SLAB_RECLAIMABLE)),
        K(global_page_state(NR_SLAB_UNRECLAIMABLE)),
        K(global_page_state(NR_PAGETABLE)),
#ifdef CONFIG_QUICKLIST
        K(quicklist_total_size()),
#endif
        K(global_page_state(NR_UNSTABLE_NFS)),
        K(global_page_state(NR_BOUNCE)),
        K(global_page_state(NR_WRITEBACK_TEMP)),
        K(allowed),
        K(committed),
        (unsigned long)VMALLOC_TOTAL >> 10,
        vmi.used >> 10,
        vmi.largest_chunk >> 10
        );

#undef K
    {
    mm_segment_t oldfs;
    int ret = -1;
    unsigned long long offset = 0;
    struct file *filp = NULL;

    oldfs = get_fs();
    set_fs(get_ds());

    filp = filp_open(filename, O_CREAT|O_RDWR|O_APPEND, S_IRWXU);
    if ( IS_ERR(filp) )
    {
        set_fs(oldfs);
        printk(KERN_INFO "%s: Open file failed\n", __func__);
        return -1;
    }
    printk("%s: start write meminfo\n", __func__);

    ret = vfs_write(filp, logger_storage_buf, len, &offset);

    printk("%s: end write meminfo\n", __func__);

    vfs_fsync(filp, filp->f_dentry, 0);
    filp_close(filp, NULL);
    set_fs(oldfs);
    }
    return 0;
}

static void dumpxxx(void)
{
    char filename0[128];
    time_t time_tv_sec;
    struct timespec now;
    struct rtc_time tm;
    int dirlen;

    now = current_kernel_time();
    time_tv_sec = now.tv_sec;
    rtc_time_to_tm(time_tv_sec, &tm);

    memset(filename0, 0, sizeof(filename0));

    sprintf(filename0, "%s", MODEM_CRASH_LOGGER_PREFIX);

    printk("%s, mk_logging_dir %s...\n", __func__, filename0);
    mk_logging_dir(filename0);

    sprintf(filename0, "%s/%d%02d%02d_%02d%02d%02dM",
        MODEM_CRASH_LOGGER_PREFIX,
        tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
        tm.tm_hour, tm.tm_min, tm.tm_sec);

    dirlen = strlen(filename0);

    printk("%s, %s\n", __func__, filename0);

    if (mk_logging_dir(filename0) != 0)
    {
        printk("%s, mk_logging_dir %s failed...\n", __func__, filename0);
        return;
    }

    printk("%s +sysprop\n", __func__);

    strcpy(filename0 + dirlen, "/sysprop");
    do_write_sysprop_log(system_properties_asma, filename0);

    printk("%s +mtrace\n", __func__);

    strcpy(filename0 + dirlen, "/mtrace");
    do_write_mtrace_log(filename0);

    printk("%s +smem\n", __func__);

    strcpy(filename0 + dirlen, "/smem");
    do_write_smem_log(filename0);

    printk("%s +meminfo\n", __func__);

    strcpy(filename0 + dirlen, "/meminfo");
    do_write_meminfo(filename0);

    strcpy(filename0 + dirlen, "/main");
    do_dump_all_log(filename0);

    printk("%s done\n", __func__);
}
#else /* DUMPXXX_TO_SDCARD */
static void dumpxxx(void) {}
#endif /* DUMPXXX_TO_SDCARD */

#if defined (MODEM_CRASH_NOTIFY_USER)
void smd_crash_notify_user(void)
{
    uint32_t loop;
    for(loop = 0; loop < 10; loop++)
    {
        MODEM_CRASH_NOTIFY_USER_1;
        msleep(100);
        MODEM_CRASH_NOTIFY_USER_0;
        msleep(100);
    }
}
#else
void smd_crash_notify_user(void) {}
#endif

static struct workqueue_struct *crash_work_queue;
static struct workqueue_struct *restart_work_queue;

void smd_restart_modem(struct work_struct *work)
{
    unsigned mode;
    SMD_CRASH_LOG("%s...\n", __func__);

    if (atomic_read(&dumpxxxed) == 1)
    {
        return;
    }

    atomic_set(&dumpxxxed, 1);

    dumpxxx();

    mode = SMSM_RESET | SMSM_SYSTEM_REBOOT;
    smsm_change_state(SMSM_APPS_STATE, mode, mode);
}
EXPORT_SYMBOL(smd_restart_modem);

#define PRINTK_BUFF_SIZE 1024
#define SMEM_LOG_ITEM_PRINT_SIZE 160
#define SMEM_LOG_NUM_ENTRIES 2000
#define EVENTS_PRINT_SIZE \
(SMEM_LOG_ITEM_PRINT_SIZE * SMEM_LOG_NUM_ENTRIES)
int debug_dump_sym(char *buf, int max, uint32_t cont);
static char debug_buffer[EVENTS_PRINT_SIZE];
static void smd_crash_show_errmsg(struct work_struct *work)
{
    char *crash_string;

    SMD_CRASH_LOG( "+%s\n", __func__);
    crash_string = smem_find(ID_DIAG_ERR_MSG, SZ_DIAG_ERR_MSG);
    SMD_CRASH_LOG( "smem: DIAG '%s'\n", crash_string);

    wake_lock(&crash_lock);
//    pm_qos_update_requirement(PM_QOS_CPU_DMA_LATENCY,
//            "arm9_crash_dump", 0);

    pm_qos_update_request(smd_qos_req_list, 0);	      /* Jagan+ ... Jagan- */

#if 0
//#ifdef CONFIG_BUILDTYPE_SHIP
    SMD_CRASH_LOG( "%s: restart system\n", __func__);
    queue_delayed_work(restart_work_queue, &smd_crash_restart_worker,
                        MODEM_CRASH_RESTART_TIMEOUT);
#endif /* CONFIG_BUILDTYPE_SHIP */
    {
        int dump_loop;
        char* dump_ptr = debug_buffer;
#if 0
        debug_dump_sym(debug_buffer, EVENTS_PRINT_SIZE, 0);
        for (dump_loop = 0; dump_loop < (EVENTS_PRINT_SIZE / PRINTK_BUFF_SIZE); dump_loop++)
        {
            printk("%s", dump_ptr);
            dump_ptr += PRINTK_BUFF_SIZE;
        }
#endif
    }

    while(1)
    {
        int size = 0;
        uint32_t reset_key;

        smd_crash_notify_user();

        crash_string = smem_find(ID_DIAG_ERR_MSG, SZ_DIAG_ERR_MSG);
        SMD_CRASH_LOG( "smem: DIAG '%s'\n", crash_string);

        crash_string = smem_get_entry(SMEM_ERR_CRASH_LOG, &size);
        if (crash_string != 0)
        {
            crash_string[size - 1] = 0;
            SMD_CRASH_LOG(  "smem: CRASH LOG\n'%s'\n", crash_string);
        }

        if ((reset_key = gpio_keypad_detect_reset_modem()) != 0)
        {
            if (reset_key == KEY_MODEM_RESET)
            {
                SMD_CRASH_LOG( "%s, RESET KEY PRESSED\n", __func__);
                queue_delayed_work(restart_work_queue, &smd_crash_restart_worker,
                        MODEM_CRASH_RESTART_TIMEOUT);
            }
        }

        msleep(1000);
    }
}
#endif

static LIST_HEAD(smd_ch_list_loopback);

static void notify_other_smsm(uint32_t smsm_entry, uint32_t notify_mask)
{
	/* older protocol don't use smsm_intr_mask,
	   but still communicates with modem */
	if (!smsm_info.intr_mask ||
	    (readl(SMSM_INTR_MASK_ADDR(smsm_entry, SMSM_MODEM)) & notify_mask))
		MSM_TRIG_A2M_SMSM_INT;

	if (smsm_info.intr_mask &&
	    (readl(SMSM_INTR_MASK_ADDR(smsm_entry, SMSM_Q6)) & notify_mask)) {
#if !defined(CONFIG_ARCH_MSM8X60)
		uint32_t mux_val;

		if (smsm_info.intr_mux) {
			mux_val = readl(SMSM_INTR_MUX_ADDR(SMEM_APPS_Q6_SMSM));
			mux_val++;
			writel(mux_val, SMSM_INTR_MUX_ADDR(SMEM_APPS_Q6_SMSM));
		}
#endif
		MSM_TRIG_A2Q6_SMSM_INT;
	}
}

static inline void notify_modem_smd(void)
{
	MSM_TRIG_A2M_SMD_INT;
}

static inline void notify_dsp_smd(void)
{
	MSM_TRIG_A2Q6_SMD_INT;
}

static inline void notify_dsps_smd(void)
{
	MSM_TRIG_A2DSPS_SMD_INT;
}

#if defined (CONFIG_QSD_ARM9_CRASH_FUNCTION)
void smd_diag(void)
{
    char *x;

    x = smem_find(ID_DIAG_ERR_MSG, SZ_DIAG_ERR_MSG);
    if (x != 0)
    {
        SMD_CRASH_LOG( "%s, smem: DIAG '%s'\n", __func__, x);
        queue_work(crash_work_queue, &smd_crash_worker);
    }
}
#else /* CONFIG_QSD_ARM9_CRASH_FUNCTION */
void smd_diag(void)
{
	char *x;
	int size;

	x = smem_find(ID_DIAG_ERR_MSG, SZ_DIAG_ERR_MSG);
	if (x != 0) {
		x[SZ_DIAG_ERR_MSG - 1] = 0;
		SMD_INFO("smem: DIAG '%s'\n", x);
	}

	x = smem_get_entry(SMEM_ERR_CRASH_LOG, &size);
	if (x != 0) {
		x[size - 1] = 0;
		pr_err("smem: CRASH LOG\n'%s'\n", x);
	}
}
#endif /* CONFIG_QSD_ARM9_CRASH_FUNCTION */

static void handle_modem_crash(void)
{
    SMD_CRASH_LOG( "ARM9 has CRASHED\n");
    smd_diag();

	/* hard reboot if possible FIXME
	if (msm_reset_hook)
		msm_reset_hook();
	*/
    /* in this case the modem or watchdog should reboot us */
#ifndef CONFIG_QSD_ARM9_CRASH_FUNCTION /* return imediately for kernel to do something */
    for (;;)
#endif /* CONFIG_QSD_ARM9_CRASH_FUNCTION */
        ;
}

int smsm_check_for_modem_crash(void)
{
	/* if the modem's not ready yet, we have to hope for the best */
	if (!smsm_info.state)
		return 0;

	if (readl(SMSM_STATE_ADDR(SMSM_MODEM_STATE)) & SMSM_RESET) {
		handle_modem_crash();
		return -1;
	}
	return 0;
}

/* the spinlock is used to synchronize between the
 * irq handler and code that mutates the channel
 * list or fiddles with channel state
 */
static DEFINE_SPINLOCK(smd_lock);
DEFINE_SPINLOCK(smem_lock);

/* the mutex is used during open() and close()
 * operations to avoid races while creating or
 * destroying smd_channel structures
 */
static DEFINE_MUTEX(smd_creation_mutex);

static int smd_initialized;

struct smd_shared_v1 {
	struct smd_half_channel ch0;
	unsigned char data0[SMD_BUF_SIZE];
	struct smd_half_channel ch1;
	unsigned char data1[SMD_BUF_SIZE];
};

struct smd_shared_v2 {
	struct smd_half_channel ch0;
	struct smd_half_channel ch1;
};

struct smd_channel {
	volatile struct smd_half_channel *send;
	volatile struct smd_half_channel *recv;
	unsigned char *send_data;
	unsigned char *recv_data;
	unsigned fifo_size;
	unsigned fifo_mask;
	struct list_head ch_list;

	unsigned current_packet;
	unsigned n;
	void *priv;
	void (*notify)(void *priv, unsigned flags);

	int (*read)(smd_channel_t *ch, void *data, int len, int user_buf);
	int (*write)(smd_channel_t *ch, const void *data, int len,
			int user_buf);
	int (*read_avail)(smd_channel_t *ch);
	int (*write_avail)(smd_channel_t *ch);
	int (*read_from_cb)(smd_channel_t *ch, void *data, int len,
			int user_buf);

	void (*update_state)(smd_channel_t *ch);
	unsigned last_state;
	void (*notify_other_cpu)(void);

	char name[20];
	struct platform_device pdev;
	unsigned type;
};

static struct platform_device loopback_tty_pdev = {.name = "LOOPBACK_TTY"};

static LIST_HEAD(smd_ch_closed_list);
static LIST_HEAD(smd_ch_list_modem);
static LIST_HEAD(smd_ch_list_dsp);
static LIST_HEAD(smd_ch_list_dsps);

static unsigned char smd_ch_allocated[64];
static struct work_struct probe_work;

static int smd_alloc_channel(struct smd_alloc_elm *alloc_elm);

/* on smp systems, the probe might get called from multiple cores,
   hence use a lock */
static DEFINE_MUTEX(smd_probe_lock);

static void smd_channel_probe_worker(struct work_struct *work)
{
	struct smd_alloc_elm *shared;
	unsigned n;
	uint32_t type;

	shared = smem_find(ID_CH_ALLOC_TBL, sizeof(*shared) * 64);

	if (!shared) {
		pr_err("%s: allocation table not initialized\n", __func__);
		return;
	}

	mutex_lock(&smd_probe_lock);
	for (n = 0; n < 64; n++) {
		if (smd_ch_allocated[n])
			continue;

		/* channel should be allocated only if APPS
		   processor is involved */
		type = SMD_CHANNEL_TYPE(shared[n].type);
		if ((type != SMD_APPS_MODEM) && (type != SMD_APPS_QDSP) &&
		    (type != SMD_APPS_DSPS))
			continue;
		if (!shared[n].ref_count)
			continue;
		if (!shared[n].name[0])
			continue;

		if (!smd_alloc_channel(&shared[n]))
			smd_ch_allocated[n] = 1;
		else
			SMD_INFO("Probe skipping ch %d, not allocated\n", n);
	}
	mutex_unlock(&smd_probe_lock);
}

/* how many bytes are available for reading */
static int smd_stream_read_avail(struct smd_channel *ch)
{
	return (ch->recv->head - ch->recv->tail) & ch->fifo_mask;
}

/* how many bytes we are free to write */
static int smd_stream_write_avail(struct smd_channel *ch)
{
	return ch->fifo_mask -
		((ch->send->head - ch->send->tail) & ch->fifo_mask);
}

static int smd_packet_read_avail(struct smd_channel *ch)
{
	if (ch->current_packet) {
		int n = smd_stream_read_avail(ch);
		if (n > ch->current_packet)
			n = ch->current_packet;
		return n;
	} else {
		return 0;
	}
}

static int smd_packet_write_avail(struct smd_channel *ch)
{
	int n = smd_stream_write_avail(ch);
	return n > SMD_HEADER_SIZE ? n - SMD_HEADER_SIZE : 0;
}

static int ch_is_open(struct smd_channel *ch)
{
	return (ch->recv->state == SMD_SS_OPENED ||
		ch->recv->state == SMD_SS_FLUSHING)
		&& (ch->send->state == SMD_SS_OPENED);
}

/* provide a pointer and length to readable data in the fifo */
static unsigned ch_read_buffer(struct smd_channel *ch, void **ptr)
{
	unsigned head = ch->recv->head;
	unsigned tail = ch->recv->tail;
	*ptr = (void *) (ch->recv_data + tail);

	if (tail <= head)
		return head - tail;
	else
		return ch->fifo_size - tail;
}

static int read_intr_blocked(struct smd_channel *ch)
{
	return ch->recv->fBLOCKREADINTR;
}

/* advance the fifo read pointer after data from ch_read_buffer is consumed */
static void ch_read_done(struct smd_channel *ch, unsigned count)
{
	BUG_ON(count > smd_stream_read_avail(ch));
	ch->recv->tail = (ch->recv->tail + count) & ch->fifo_mask;
	ch->send->fTAIL = 1;
}

/* basic read interface to ch_read_{buffer,done} used
 * by smd_*_read() and update_packet_state()
 * will read-and-discard if the _data pointer is null
 */
static int ch_read(struct smd_channel *ch, void *_data, int len, int user_buf)
{
	void *ptr;
	unsigned n;
	unsigned char *data = _data;
	int orig_len = len;
	int r = 0;

	while (len > 0) {
		n = ch_read_buffer(ch, &ptr);
		if (n == 0)
			break;

		if (n > len)
			n = len;
		if (_data) {
			if (user_buf) {
				r = copy_to_user(data, ptr, n);
				if (r > 0) {
					pr_err("%s: "
						"copy_to_user could not copy "
						"%i bytes.\n",
						__func__,
						r);
				}
			} else
				memcpy(data, ptr, n);
		}

		data += n;
		len -= n;
		ch_read_done(ch, n);
	}

	return orig_len - len;
}

static void update_stream_state(struct smd_channel *ch)
{
	/* streams have no special state requiring updating */
}

static void update_packet_state(struct smd_channel *ch)
{
	unsigned hdr[5];
	int r;

	/* can't do anything if we're in the middle of a packet */
	while (ch->current_packet == 0) {
		/* discard 0 length packets if any */

		/* don't bother unless we can get the full header */
		if (smd_stream_read_avail(ch) < SMD_HEADER_SIZE)
			return;

		r = ch_read(ch, hdr, SMD_HEADER_SIZE, 0);
		BUG_ON(r != SMD_HEADER_SIZE);

		ch->current_packet = hdr[0];
	}
}

/* provide a pointer and length to next free space in the fifo */
static unsigned ch_write_buffer(struct smd_channel *ch, void **ptr)
{
	unsigned head = ch->send->head;
	unsigned tail = ch->send->tail;
	*ptr = (void *) (ch->send_data + head);

	if (head < tail) {
		return tail - head - 1;
	} else {
		if (tail == 0)
			return ch->fifo_size - head - 1;
		else
			return ch->fifo_size - head;
	}
}

/* advace the fifo write pointer after freespace
 * from ch_write_buffer is filled
 */
static void ch_write_done(struct smd_channel *ch, unsigned count)
{
	BUG_ON(count > smd_stream_write_avail(ch));
	ch->send->head = (ch->send->head + count) & ch->fifo_mask;
	ch->send->fHEAD = 1;
}

static void ch_set_state(struct smd_channel *ch, unsigned n)
{
	if (n == SMD_SS_OPENED) {
		ch->send->fDSR = 1;
		ch->send->fCTS = 1;
		ch->send->fCD = 1;
	} else {
		ch->send->fDSR = 0;
		ch->send->fCTS = 0;
		ch->send->fCD = 0;
	}
	ch->send->state = n;
	ch->send->fSTATE = 1;
	ch->notify_other_cpu();
}

static void do_smd_probe(void)
{
	struct smem_shared *shared = (void *) MSM_SHARED_RAM_BASE;
	if (shared->heap_info.free_offset != last_heap_free) {
		last_heap_free = shared->heap_info.free_offset;
		schedule_work(&probe_work);
	}
}

static void smd_state_change(struct smd_channel *ch,
			     unsigned last, unsigned next)
{
	ch->last_state = next;

	SMD_INFO("SMD: ch %d %d -> %d\n", ch->n, last, next);

	switch (next) {
	case SMD_SS_OPENING:
		if (ch->send->state == SMD_SS_CLOSING ||
		    ch->send->state == SMD_SS_CLOSED) {
			ch->recv->tail = 0;
			ch->send->head = 0;
			ch_set_state(ch, SMD_SS_OPENING);
		}
		break;
	case SMD_SS_OPENED:
		if (ch->send->state == SMD_SS_OPENING) {
			ch_set_state(ch, SMD_SS_OPENED);
			ch->notify(ch->priv, SMD_EVENT_OPEN);
		}
		break;
	case SMD_SS_FLUSHING:
	case SMD_SS_RESET:
		/* we should force them to close? */
		break;
	case SMD_SS_CLOSED:
		if (ch->send->state == SMD_SS_OPENED) {
			ch_set_state(ch, SMD_SS_CLOSING);
			ch->notify(ch->priv, SMD_EVENT_CLOSE);
		}
		break;
	}
}

static void handle_smd_irq(struct list_head *list, void (*notify)(void))
{
	unsigned long flags;
	struct smd_channel *ch;
	int do_notify = 0;
	unsigned ch_flags;
	unsigned tmp;

	spin_lock_irqsave(&smd_lock, flags);
	list_for_each_entry(ch, list, ch_list) {
		ch_flags = 0;
		if (ch_is_open(ch)) {
			if (ch->recv->fHEAD) {
				ch->recv->fHEAD = 0;
				ch_flags |= 1;
				do_notify |= 1;
			}
			if (ch->recv->fTAIL) {
				ch->recv->fTAIL = 0;
				ch_flags |= 2;
				do_notify |= 1;
			}
			if (ch->recv->fSTATE) {
				ch->recv->fSTATE = 0;
				ch_flags |= 4;
				do_notify |= 1;
			}
		}
		tmp = ch->recv->state;
		if (tmp != ch->last_state)
			smd_state_change(ch, ch->last_state, tmp);
		if (ch_flags) {
			ch->update_state(ch);
			ch->notify(ch->priv, SMD_EVENT_DATA);
		}
	}
	if (do_notify)
		notify();
	spin_unlock_irqrestore(&smd_lock, flags);
	do_smd_probe();
}

static irqreturn_t smd_modem_irq_handler(int irq, void *data)
{
	handle_smd_irq(&smd_ch_list_modem, notify_modem_smd);
	return IRQ_HANDLED;
}

#if defined(CONFIG_QDSP6)
static irqreturn_t smd_dsp_irq_handler(int irq, void *data)
{
	handle_smd_irq(&smd_ch_list_dsp, notify_dsp_smd);
	return IRQ_HANDLED;
}
#endif

#if defined(CONFIG_DSPS)
static irqreturn_t smd_dsps_irq_handler(int irq, void *data)
{
	handle_smd_irq(&smd_ch_list_dsps, notify_dsps_smd);
	return IRQ_HANDLED;
}
#endif

static void smd_fake_irq_handler(unsigned long arg)
{
	handle_smd_irq(&smd_ch_list_modem, notify_modem_smd);
	handle_smd_irq(&smd_ch_list_dsp, notify_dsp_smd);
	handle_smd_irq(&smd_ch_list_dsps, notify_dsps_smd);
}

static DECLARE_TASKLET(smd_fake_irq_tasklet, smd_fake_irq_handler, 0);

static inline int smd_need_int(struct smd_channel *ch)
{
	if (ch_is_open(ch)) {
		if (ch->recv->fHEAD || ch->recv->fTAIL || ch->recv->fSTATE)
			return 1;
		if (ch->recv->state != ch->last_state)
			return 1;
	}
	return 0;
}

void smd_sleep_exit(void)
{
	unsigned long flags;
	struct smd_channel *ch;
	int need_int = 0;

	spin_lock_irqsave(&smd_lock, flags);
	list_for_each_entry(ch, &smd_ch_list_modem, ch_list) {
		if (smd_need_int(ch)) {
			need_int = 1;
			break;
		}
	}
	list_for_each_entry(ch, &smd_ch_list_dsp, ch_list) {
		if (smd_need_int(ch)) {
			need_int = 1;
			break;
		}
	}
	list_for_each_entry(ch, &smd_ch_list_dsps, ch_list) {
		if (smd_need_int(ch)) {
			need_int = 1;
			break;
		}
	}
	spin_unlock_irqrestore(&smd_lock, flags);
	do_smd_probe();

	if (need_int) {
		SMD_DBG("smd_sleep_exit need interrupt\n");
		tasklet_schedule(&smd_fake_irq_tasklet);
	}
}

static int smd_is_packet(struct smd_alloc_elm *alloc_elm)
{
	if (SMD_XFER_TYPE(alloc_elm->type) == 1)
		return 0;
	else if (SMD_XFER_TYPE(alloc_elm->type) == 2)
		return 1;

	/* for cases where xfer type is 0 */
	if (!strncmp(alloc_elm->name, "DAL", 3))
		return 0;

	if (alloc_elm->cid > 4 || alloc_elm->cid == 1)
		return 1;
	else
		return 0;
}

static int smd_stream_write(smd_channel_t *ch, const void *_data, int len,
				int user_buf)
{
	void *ptr;
	const unsigned char *buf = _data;
	unsigned xfer;
	int orig_len = len;
	int r = 0;

	SMD_DBG("smd_stream_write() %d -> ch%d\n", len, ch->n);
	if (len < 0)
		return -EINVAL;
	else if (len == 0)
		return 0;

	while ((xfer = ch_write_buffer(ch, &ptr)) != 0) {
		if (!ch_is_open(ch))
			break;
		if (xfer > len)
			xfer = len;
		if (user_buf) {
			r = copy_from_user(ptr, buf, xfer);
			if (r > 0) {
				pr_err("%s: "
					"copy_from_user could not copy %i "
					"bytes.\n",
					__func__,
					r);
			}
		} else
			memcpy(ptr, buf, xfer);
		ch_write_done(ch, xfer);
		len -= xfer;
		buf += xfer;
		if (len == 0)
			break;
	}

	if (orig_len - len)
		ch->notify_other_cpu();

	return orig_len - len;
}

static int smd_packet_write(smd_channel_t *ch, const void *_data, int len,
				int user_buf)
{
	int ret;
	unsigned hdr[5];

	SMD_DBG("smd_packet_write() %d -> ch%d\n", len, ch->n);
	if (len < 0)
		return -EINVAL;
	else if (len == 0)
		return 0;

	if (smd_stream_write_avail(ch) < (len + SMD_HEADER_SIZE))
		return -ENOMEM;

	hdr[0] = len;
	hdr[1] = hdr[2] = hdr[3] = hdr[4] = 0;


	ret = smd_stream_write(ch, hdr, sizeof(hdr), 0);
	if (ret < 0 || ret != sizeof(hdr)) {
		SMD_DBG("%s failed to write pkt header: "
			"%d returned\n", __func__, ret);
		return -1;
	}


	ret = smd_stream_write(ch, _data, len, user_buf);
	if (ret < 0 || ret != len) {
		SMD_DBG("%s failed to write pkt data: "
			"%d returned\n", __func__, ret);
		return ret;
	}

	return len;
}

static int smd_stream_read(smd_channel_t *ch, void *data, int len, int user_buf)
{
	int r;

	if (len < 0)
		return -EINVAL;

	r = ch_read(ch, data, len, user_buf);
	if (r > 0)
		if (!read_intr_blocked(ch))
			ch->notify_other_cpu();

	return r;
}

static int smd_packet_read(smd_channel_t *ch, void *data, int len, int user_buf)
{
	unsigned long flags;
	int r;

	if (len < 0)
		return -EINVAL;

	if (len > ch->current_packet)
		len = ch->current_packet;

	r = ch_read(ch, data, len, user_buf);
	if (r > 0)
		if (!read_intr_blocked(ch))
			ch->notify_other_cpu();

	spin_lock_irqsave(&smd_lock, flags);
	ch->current_packet -= r;
	update_packet_state(ch);
	spin_unlock_irqrestore(&smd_lock, flags);

	return r;
}

static int smd_packet_read_from_cb(smd_channel_t *ch, void *data, int len,
					int user_buf)
{
	int r;

	if (len < 0)
		return -EINVAL;

	if (len > ch->current_packet)
		len = ch->current_packet;

	r = ch_read(ch, data, len, user_buf);
	if (r > 0)
		if (!read_intr_blocked(ch))
			ch->notify_other_cpu();

	ch->current_packet -= r;
	update_packet_state(ch);

	return r;
}

static int smd_alloc_v2(struct smd_channel *ch)
{
	struct smd_shared_v2 *shared2;
	void *buffer;
	unsigned buffer_sz;

	shared2 = smem_alloc(SMEM_SMD_BASE_ID + ch->n, sizeof(*shared2));
	if (!shared2) {
		SMD_INFO("smem_alloc failed ch=%d\n", ch->n);
		return -1;
	}
	buffer = smem_get_entry(SMEM_SMD_FIFO_BASE_ID + ch->n, &buffer_sz);
	if (!buffer) {
		SMD_INFO("smem_get_entry failed \n");
		return -1;
	}

	/* buffer must be a power-of-two size */
	if (buffer_sz & (buffer_sz - 1))
		return -1;

	buffer_sz /= 2;
	ch->send = &shared2->ch0;
	ch->recv = &shared2->ch1;
	ch->send_data = buffer;
	ch->recv_data = buffer + buffer_sz;
	ch->fifo_size = buffer_sz;
	return 0;
}

static int smd_alloc_v1(struct smd_channel *ch)
{
	struct smd_shared_v1 *shared1;
	shared1 = smem_alloc(ID_SMD_CHANNELS + ch->n, sizeof(*shared1));
	if (!shared1) {
		pr_err("smd_alloc_channel() cid %d does not exist\n", ch->n);
		return -1;
	}
	ch->send = &shared1->ch0;
	ch->recv = &shared1->ch1;
	ch->send_data = shared1->data0;
	ch->recv_data = shared1->data1;
	ch->fifo_size = SMD_BUF_SIZE;
	return 0;
}

static int smd_alloc_channel(struct smd_alloc_elm *alloc_elm)
{
	struct smd_channel *ch;

	ch = kzalloc(sizeof(struct smd_channel), GFP_KERNEL);
	if (ch == 0) {
		pr_err("smd_alloc_channel() out of memory\n");
		return -1;
	}
	ch->n = alloc_elm->cid;

	if (smd_alloc_v2(ch) && smd_alloc_v1(ch)) {
		kfree(ch);
		return -1;
	}

	ch->fifo_mask = ch->fifo_size - 1;
	ch->type = SMD_CHANNEL_TYPE(alloc_elm->type);

	if (ch->type == SMD_APPS_MODEM)
		ch->notify_other_cpu = notify_modem_smd;
	else if (ch->type == SMD_APPS_QDSP)
		ch->notify_other_cpu = notify_dsp_smd;
	else
		ch->notify_other_cpu = notify_dsps_smd;

	if (smd_is_packet(alloc_elm)) {
		ch->read = smd_packet_read;
		ch->write = smd_packet_write;
		ch->read_avail = smd_packet_read_avail;
		ch->write_avail = smd_packet_write_avail;
		ch->update_state = update_packet_state;
		ch->read_from_cb = smd_packet_read_from_cb;
	} else {
		ch->read = smd_stream_read;
		ch->write = smd_stream_write;
		ch->read_avail = smd_stream_read_avail;
		ch->write_avail = smd_stream_write_avail;
		ch->update_state = update_stream_state;
		ch->read_from_cb = smd_stream_read;
	}

	memcpy(ch->name, alloc_elm->name, 20);
	ch->name[19] = 0;

	ch->pdev.name = ch->name;
	ch->pdev.id = ch->type;

	SMD_INFO("smd_alloc_channel() '%s' cid=%d\n",
		 ch->name, ch->n);

	mutex_lock(&smd_creation_mutex);
	list_add(&ch->ch_list, &smd_ch_closed_list);
	mutex_unlock(&smd_creation_mutex);

	platform_device_register(&ch->pdev);
	if (!strcmp(ch->name, "LOOPBACK")) {
		/* create a platform driver to be used by smd_tty driver
		 * so that it can access the loopback port
		 */
		loopback_tty_pdev.id = ch->type;
		platform_device_register(&loopback_tty_pdev);
	}
	return 0;
}

static inline void notify_loopback_smd(void)
{
	unsigned long flags;
	struct smd_channel *ch;

	spin_lock_irqsave(&smd_lock, flags);
	list_for_each_entry(ch, &smd_ch_list_loopback, ch_list) {
		ch->notify(ch->priv, SMD_EVENT_DATA);
	}
	spin_unlock_irqrestore(&smd_lock, flags);
}

static int smd_alloc_loopback_channel(void)
{
	static struct smd_half_channel smd_loopback_ctl;
	static char smd_loopback_data[SMD_BUF_SIZE];
	struct smd_channel *ch;

	ch = kzalloc(sizeof(struct smd_channel), GFP_KERNEL);
	if (ch == 0) {
		pr_err("%s: out of memory\n", __func__);
		return -1;
	}
	ch->n = SMD_LOOPBACK_CID;

	ch->send = &smd_loopback_ctl;
	ch->recv = &smd_loopback_ctl;
	ch->send_data = smd_loopback_data;
	ch->recv_data = smd_loopback_data;
	ch->fifo_size = SMD_BUF_SIZE;

	ch->fifo_mask = ch->fifo_size - 1;
	ch->type = SMD_LOOPBACK_TYPE;
	ch->notify_other_cpu = notify_loopback_smd;

	ch->read = smd_stream_read;
	ch->write = smd_stream_write;
	ch->read_avail = smd_stream_read_avail;
	ch->write_avail = smd_stream_write_avail;
	ch->update_state = update_stream_state;
	ch->read_from_cb = smd_stream_read;

	memset(ch->name, 0, 20);
	memcpy(ch->name, "local_loopback", 14);

	ch->pdev.name = ch->name;
	ch->pdev.id = ch->type;

	SMD_INFO("%s: '%s' cid=%d\n", __func__, ch->name, ch->n);

	mutex_lock(&smd_creation_mutex);
	list_add(&ch->ch_list, &smd_ch_closed_list);
	mutex_unlock(&smd_creation_mutex);

	platform_device_register(&ch->pdev);
	return 0;
}

static void do_nothing_notify(void *priv, unsigned flags)
{
}

struct smd_channel *smd_get_channel(const char *name, uint32_t type)
{
	struct smd_channel *ch;

	mutex_lock(&smd_creation_mutex);
	list_for_each_entry(ch, &smd_ch_closed_list, ch_list) {
		if (!strcmp(name, ch->name) &&
			(type == ch->type)) {
			list_del(&ch->ch_list);
			mutex_unlock(&smd_creation_mutex);
			return ch;
		}
	}
	mutex_unlock(&smd_creation_mutex);

	return NULL;
}

int smd_named_open_on_edge(const char *name, uint32_t edge,
			   smd_channel_t **_ch,
			   void *priv, void (*notify)(void *, unsigned))
{
	struct smd_channel *ch;
	unsigned long flags;

	if (smd_initialized == 0) {
		SMD_INFO("smd_open() before smd_init()\n");
		return -ENODEV;
	}

	SMD_DBG("smd_open('%s', %p, %p)\n", name, priv, notify);

	ch = smd_get_channel(name, edge);
	if (!ch)
		return -ENODEV;

	if (notify == 0)
		notify = do_nothing_notify;

	ch->notify = notify;
	ch->current_packet = 0;
	ch->last_state = SMD_SS_CLOSED;
	ch->priv = priv;

	if (edge == SMD_LOOPBACK_TYPE) {
		ch->last_state = SMD_SS_OPENED;
		ch->send->state = SMD_SS_OPENED;
		ch->send->fDSR = 1;
		ch->send->fCTS = 1;
		ch->send->fCD = 1;
	}

	*_ch = ch;

	SMD_DBG("smd_open: opening '%s'\n", ch->name);

	spin_lock_irqsave(&smd_lock, flags);
	if (SMD_CHANNEL_TYPE(ch->type) == SMD_APPS_MODEM)
		list_add(&ch->ch_list, &smd_ch_list_modem);
	else if (SMD_CHANNEL_TYPE(ch->type) == SMD_APPS_QDSP)
		list_add(&ch->ch_list, &smd_ch_list_dsp);
	else if (SMD_CHANNEL_TYPE(ch->type) == SMD_APPS_DSPS)
		list_add(&ch->ch_list, &smd_ch_list_dsps);
	else
		list_add(&ch->ch_list, &smd_ch_list_loopback);

	SMD_DBG("%s: opening ch %d\n", __func__, ch->n);

	if (edge != SMD_LOOPBACK_TYPE)
		smd_state_change(ch, ch->last_state, SMD_SS_OPENING);

	spin_unlock_irqrestore(&smd_lock, flags);

	return 0;
}
EXPORT_SYMBOL(smd_named_open_on_edge);


int smd_open(const char *name, smd_channel_t **_ch,
	     void *priv, void (*notify)(void *, unsigned))
{
	return smd_named_open_on_edge(name, SMD_APPS_MODEM, _ch, priv,
				      notify);
}
EXPORT_SYMBOL(smd_open);

int smd_close(smd_channel_t *ch)
{
	unsigned long flags;

	if (ch == 0)
		return -1;

	SMD_INFO("smd_close(%s)\n", ch->name);

	spin_lock_irqsave(&smd_lock, flags);
	ch->notify = do_nothing_notify;
	list_del(&ch->ch_list);
	if (ch->n == SMD_LOOPBACK_CID) {
		ch->send->fDSR = 0;
		ch->send->fCTS = 0;
		ch->send->fCD = 0;
		ch->send->state = SMD_SS_CLOSED;
	} else
		ch_set_state(ch, SMD_SS_CLOSED);
	spin_unlock_irqrestore(&smd_lock, flags);

	mutex_lock(&smd_creation_mutex);
	list_add(&ch->ch_list, &smd_ch_closed_list);
	mutex_unlock(&smd_creation_mutex);

	return 0;
}
EXPORT_SYMBOL(smd_close);

int smd_read(smd_channel_t *ch, void *data, int len)
{
	return ch->read(ch, data, len, 0);
}
EXPORT_SYMBOL(smd_read);

int smd_read_user_buffer(smd_channel_t *ch, void *data, int len)
{
	return ch->read(ch, data, len, 1);
}
EXPORT_SYMBOL(smd_read_user_buffer);

int smd_read_from_cb(smd_channel_t *ch, void *data, int len)
{
	return ch->read_from_cb(ch, data, len, 0);
}
EXPORT_SYMBOL(smd_read_from_cb);

int smd_write(smd_channel_t *ch, const void *data, int len)
{
	return ch->write(ch, data, len, 0);
}
EXPORT_SYMBOL(smd_write);

int smd_write_user_buffer(smd_channel_t *ch, const void *data, int len)
{
	return ch->write(ch, data, len, 1);
}
EXPORT_SYMBOL(smd_write_user_buffer);

int smd_read_avail(smd_channel_t *ch)
{
	return ch->read_avail(ch);
}
EXPORT_SYMBOL(smd_read_avail);

int smd_write_avail(smd_channel_t *ch)
{
	return ch->write_avail(ch);
}
EXPORT_SYMBOL(smd_write_avail);

void smd_enable_read_intr(smd_channel_t *ch)
{
	if (ch)
		ch->send->fBLOCKREADINTR = 0;
}
EXPORT_SYMBOL(smd_enable_read_intr);

void smd_disable_read_intr(smd_channel_t *ch)
{
	if (ch)
		ch->send->fBLOCKREADINTR = 1;
}
EXPORT_SYMBOL(smd_disable_read_intr);

int smd_wait_until_readable(smd_channel_t *ch, int bytes)
{
	return -1;
}

int smd_wait_until_writable(smd_channel_t *ch, int bytes)
{
	return -1;
}

int smd_cur_packet_size(smd_channel_t *ch)
{
	return ch->current_packet;
}
EXPORT_SYMBOL(smd_cur_packet_size);

int smd_tiocmget(smd_channel_t *ch)
{
	return  (ch->recv->fDSR ? TIOCM_DSR : 0) |
		(ch->recv->fCTS ? TIOCM_CTS : 0) |
		(ch->recv->fCD ? TIOCM_CD : 0) |
		(ch->recv->fRI ? TIOCM_RI : 0) |
		(ch->send->fCTS ? TIOCM_RTS : 0) |
		(ch->send->fDSR ? TIOCM_DTR : 0);
}
EXPORT_SYMBOL(smd_tiocmget);

int smd_tiocmset(smd_channel_t *ch, unsigned int set, unsigned int clear)
{
	unsigned long flags;

	spin_lock_irqsave(&smd_lock, flags);
	if (set & TIOCM_DTR)
		ch->send->fDSR = 1;

	if (set & TIOCM_RTS)
		ch->send->fCTS = 1;

	if (clear & TIOCM_DTR)
		ch->send->fDSR = 0;

	if (clear & TIOCM_RTS)
		ch->send->fCTS = 0;

	ch->send->fSTATE = 1;
	barrier();
	ch->notify_other_cpu();
	spin_unlock_irqrestore(&smd_lock, flags);

	return 0;
}
EXPORT_SYMBOL(smd_tiocmset);


/* -------------------------------------------------------------------------- */

/* smem_alloc returns the pointer to smem item if it is already allocated.
 * Otherwise, it returns NULL.
 */
void *smem_alloc(unsigned id, unsigned size)
{
	return smem_find(id, size);
}

#define SMEM_SPINLOCK_SMEM_ALLOC       "S:3"
static remote_spinlock_t remote_spinlock;

/* smem_alloc2 returns the pointer to smem item.  If it is not allocated,
 * it allocates it and then returns the pointer to it.
 */
static void *smem_alloc2(unsigned id, unsigned size_in)
{
	struct smem_shared *shared = (void *) MSM_SHARED_RAM_BASE;
	struct smem_heap_entry *toc = shared->heap_toc;
	unsigned long flags;
	void *ret = NULL;

	if (!shared->heap_info.initialized) {
		pr_err("%s: smem heap info not initialized\n", __func__);
		return NULL;
	}

	if (id >= SMEM_NUM_ITEMS)
		return NULL;

	size_in = ALIGN(size_in, 8);
	remote_spin_lock_irqsave(&remote_spinlock, flags);
	if (toc[id].allocated) {
		SMD_DBG("%s: %u already allocated\n", __func__, id);
		if (size_in != toc[id].size)
			pr_err("%s: wrong size %u (expected %u)\n",
			       __func__, toc[id].size, size_in);
		else
			ret = (void *)(MSM_SHARED_RAM_BASE + toc[id].offset);
	} else if (id > SMEM_FIXED_ITEM_LAST) {
		SMD_DBG("%s: allocating %u\n", __func__, id);
		if (shared->heap_info.heap_remaining >= size_in) {
			toc[id].allocated = 1;
			toc[id].offset = shared->heap_info.free_offset;
			toc[id].size = size_in;

			shared->heap_info.free_offset += size_in;
			shared->heap_info.heap_remaining -= size_in;
			ret = (void *)(MSM_SHARED_RAM_BASE + toc[id].offset);
		} else
			pr_err("%s: not enough memory %u (required %u)\n",
			       __func__, shared->heap_info.heap_remaining,
			       size_in);
	}
	/* TODO: system/hardware barrier required? */
	barrier();
	remote_spin_unlock_irqrestore(&remote_spinlock, flags);
	return ret;
}

void *smem_get_entry(unsigned id, unsigned *size)
{
	struct smem_shared *shared = (void *) MSM_SHARED_RAM_BASE;
	struct smem_heap_entry *toc = shared->heap_toc;

	if (id >= SMEM_NUM_ITEMS)
		return 0;

	if (toc[id].allocated) {
		*size = toc[id].size;
		return (void *) (MSM_SHARED_RAM_BASE + toc[id].offset);
	} else {
		*size = 0;
	}

	return 0;
}

void *smem_find(unsigned id, unsigned size_in)
{
	unsigned size;
	void *ptr;

	ptr = smem_get_entry(id, &size);
	if (!ptr)
		return 0;

	size_in = ALIGN(size_in, 8);
	if (size_in != size) {
		pr_err("smem_find(%d, %d): wrong size %d\n",
		       id, size_in, size);
		return 0;
	}

	return ptr;
}

static int smsm_init(void)
{
	struct smem_shared *shared = (void *) MSM_SHARED_RAM_BASE;
	int i;

	i = remote_spin_lock_init(&remote_spinlock, SMEM_SPINLOCK_SMEM_ALLOC);
	if (i) {
		pr_err("%s: remote spinlock init failed %d\n", __func__, i);
		return i;
	}

	if (!smsm_info.state) {
		smsm_info.state = smem_alloc2(ID_SHARED_STATE,
					      SMSM_NUM_ENTRIES *
					      sizeof(uint32_t));

		if (smsm_info.state) {
			writel(0, SMSM_STATE_ADDR(SMSM_APPS_STATE));
			if ((shared->version[VERSION_MODEM] >> 16) >= 0xB)
				writel(0, SMSM_STATE_ADDR(SMSM_APPS_DEM_I));
		}
	}

	if (!smsm_info.intr_mask) {
		smsm_info.intr_mask = smem_alloc2(SMEM_SMSM_CPU_INTR_MASK,
						  SMSM_NUM_ENTRIES *
						  SMSM_NUM_HOSTS *
						  sizeof(uint32_t));

		if (smsm_info.intr_mask)
			for (i = 0; i < SMSM_NUM_ENTRIES; i++)
				writel(0xffffffff,
				       SMSM_INTR_MASK_ADDR(i, SMSM_APPS));
	}

	if (!smsm_info.intr_mux)
		smsm_info.intr_mux = smem_alloc2(SMEM_SMD_SMSM_INTR_MUX,
						 SMSM_NUM_INTR_MUX *
						 sizeof(uint32_t));

	return 0;
}

void smsm_reset_modem(unsigned mode)
{
    if (mode == SMSM_SYSTEM_DOWNLOAD) {
        mode = SMSM_RESET | SMSM_SYSTEM_DOWNLOAD;
    } else if (mode == SMSM_MODEM_WAIT) {
        mode = SMSM_RESET | SMSM_MODEM_WAIT;
#if defined (CONFIG_QSD_ARM9_CRASH_FUNCTION)
    } else if (mode == SMSM_SYSTEM_REBOOT) {
        mode = SMSM_SYSTEM_REBOOT;
#endif
    } else { /* reset_mode is SMSM_RESET or default */
        mode = SMSM_RESET;
    }

	smsm_change_state(SMSM_APPS_STATE, mode, mode);
}
EXPORT_SYMBOL(smsm_reset_modem);

void smsm_reset_modem_cont(void)
{
	unsigned long flags;
	uint32_t state;

	if (!smsm_info.state)
		return;

	spin_lock_irqsave(&smem_lock, flags);
	state = readl(SMSM_STATE_ADDR(SMSM_APPS_STATE)) & ~SMSM_MODEM_WAIT;
	writel(state, SMSM_STATE_ADDR(SMSM_APPS_STATE));
	spin_unlock_irqrestore(&smem_lock, flags);
}
EXPORT_SYMBOL(smsm_reset_modem_cont);

static irqreturn_t smsm_irq_handler(int irq, void *data)
{
	unsigned long flags;

#if !defined(CONFIG_ARCH_MSM8X60)
	uint32_t mux_val;
	static uint32_t prev_smem_q6_apps_smsm;

	if (irq == INT_ADSP_A11_SMSM) {
		if (!smsm_info.intr_mux)
			return IRQ_HANDLED;
		mux_val = readl(SMSM_INTR_MUX_ADDR(SMEM_Q6_APPS_SMSM));
		if (mux_val != prev_smem_q6_apps_smsm)
			prev_smem_q6_apps_smsm = mux_val;
		return IRQ_HANDLED;
	}
#else
	if (irq == INT_ADSP_A11_SMSM)
		return IRQ_HANDLED;
#endif


	spin_lock_irqsave(&smem_lock, flags);
	if (!smsm_info.state) {
		SMSM_INFO("<SM NO STATE>\n");
	} else {
		unsigned old_apps, apps;
		unsigned modm = readl(SMSM_STATE_ADDR(SMSM_MODEM_STATE));

		old_apps = apps = readl(SMSM_STATE_ADDR(SMSM_APPS_STATE));

		SMSM_DBG("<SM %08x %08x>\n", apps, modm);
		if (apps & SMSM_RESET) {
			/* If we get an interrupt and the apps SMSM_RESET
			   bit is already set, the modem is acking the
			   app's reset ack. */
			apps &= ~SMSM_RESET;

			/* Issue a fake irq to handle any
			 * smd state changes during reset
			 */
			smd_fake_irq_handler(0);

			/* queue modem restart notify chain */
			modem_queue_start_reset_notify();

		} else if (modm & SMSM_RESET) {
#if defined (CONFIG_QSD_ARM9_CRASH_FUNCTION)
            char *crash_string;

            crash_string = smem_find(ID_DIAG_ERR_MSG, SZ_DIAG_ERR_MSG);
            SMD_CRASH_LOG( "%s: modem crash: '%s'\n", __func__, crash_string);

            if (crash_work_queue == NULL)
            {
                crash_work_queue = create_singlethread_workqueue("arm9_crash_dump");
            }

            if (crash_work_queue != NULL)
            {
                queue_work(crash_work_queue, &smd_crash_worker);
            }
#endif /* CONFIG_QSD_ARM9_CRASH_FUNCTION */
			apps |= SMSM_RESET;

			pr_err("\nSMSM: Modem SMSM state changed to SMSM_RESET.");
			modem_queue_start_reset_notify();

		} else if (modm & SMSM_INIT) {
			if (!(apps & SMSM_INIT)) {
				apps |= SMSM_INIT;
				modem_queue_smsm_init_notify();
			}

			if (modm & SMSM_SMDINIT)
				apps |= SMSM_SMDINIT;
			if ((apps & (SMSM_INIT | SMSM_SMDINIT | SMSM_RPCINIT)) ==
				(SMSM_INIT | SMSM_SMDINIT | SMSM_RPCINIT))
				apps |= SMSM_RUN;
		} else if (modm & SMSM_SYSTEM_DOWNLOAD) {
			pr_err("\nSMSM: Modem SMSM state changed to SMSM_SYSTEM_DOWNLOAD.");
			modem_queue_start_reset_notify();
		}

		if (old_apps != apps) {
			SMSM_DBG("<SM %08x NOTIFY>\n", apps);
			writel(apps, SMSM_STATE_ADDR(SMSM_APPS_STATE));
			do_smd_probe();
			notify_other_smsm(SMSM_APPS_STATE, (old_apps ^ apps));
		}
	}
	spin_unlock_irqrestore(&smem_lock, flags);
	return IRQ_HANDLED;
}

int smsm_change_intr_mask(uint32_t smsm_entry,
			  uint32_t clear_mask, uint32_t set_mask)
{
	uint32_t  old_mask, new_mask;
	unsigned long flags;

	if (smsm_entry >= SMSM_NUM_ENTRIES) {
		pr_err("smsm_change_state: Invalid entry %d\n",
		       smsm_entry);
		return -EINVAL;
	}

	if (!smsm_info.intr_mask) {
		pr_err("smsm_change_intr_mask <SM NO STATE>\n");
		return -EIO;
	}

	spin_lock_irqsave(&smem_lock, flags);

	old_mask = readl(SMSM_INTR_MASK_ADDR(smsm_entry, SMSM_APPS));
	new_mask = (old_mask & ~clear_mask) | set_mask;
	writel(new_mask, SMSM_INTR_MASK_ADDR(smsm_entry, SMSM_APPS));

	spin_unlock_irqrestore(&smem_lock, flags);

	return 0;
}

int smsm_get_intr_mask(uint32_t smsm_entry, uint32_t *intr_mask)
{
	if (smsm_entry >= SMSM_NUM_ENTRIES) {
		pr_err("smsm_change_state: Invalid entry %d\n",
		       smsm_entry);
		return -EINVAL;
	}

	if (!smsm_info.intr_mask) {
		pr_err("smsm_change_intr_mask <SM NO STATE>\n");
		return -EIO;
	}

	*intr_mask = readl(SMSM_INTR_MASK_ADDR(smsm_entry, SMSM_APPS));
	return 0;
}

int smsm_change_state(uint32_t smsm_entry,
		      uint32_t clear_mask, uint32_t set_mask)
{
	unsigned long flags;
	uint32_t  old_state, new_state;

	if (smsm_entry >= SMSM_NUM_ENTRIES) {
		pr_err("smsm_change_state: Invalid entry %d",
		       smsm_entry);
		return -EINVAL;
	}

	if (!smsm_info.state) {
		pr_err("smsm_change_state <SM NO STATE>\n");
		return -EIO;
	}
	spin_lock_irqsave(&smem_lock, flags);

	old_state = readl(SMSM_STATE_ADDR(smsm_entry));
	new_state = (old_state & ~clear_mask) | set_mask;
	writel(new_state, SMSM_STATE_ADDR(smsm_entry));
	SMSM_DBG("smsm_change_state %x\n", new_state);
	notify_other_smsm(SMSM_APPS_STATE, (old_state ^ new_state));

	spin_unlock_irqrestore(&smem_lock, flags);

	return 0;
}

uint32_t smsm_get_state(uint32_t smsm_entry)
{
	uint32_t rv = 0;

	/* needs interface change to return error code */
	if (smsm_entry >= SMSM_NUM_ENTRIES) {
		pr_err("smsm_change_state: Invalid entry %d",
		       smsm_entry);
		return 0;
	}

	if (!smsm_info.state)
		pr_err("smsm_get_state <SM NO STATE>\n");
	else
		rv = readl(SMSM_STATE_ADDR(smsm_entry));

	return rv;
}

int smd_core_init(void)
{
	int r;
	unsigned long flags = IRQF_TRIGGER_RISING;
	SMD_INFO("smd_core_init()\n");

	r = request_irq(INT_A9_M2A_0, smd_modem_irq_handler,
			flags, "smd_dev", 0);
	if (r < 0)
		return r;
	r = enable_irq_wake(INT_A9_M2A_0);
	if (r < 0)
		pr_err("smd_core_init: "
		       "enable_irq_wake failed for INT_A9_M2A_0\n");

	r = request_irq(INT_A9_M2A_5, smsm_irq_handler,
			flags, "smsm_dev", 0);
	if (r < 0) {
		free_irq(INT_A9_M2A_0, 0);
		return r;
	}
	r = enable_irq_wake(INT_A9_M2A_5);
	if (r < 0)
		pr_err("smd_core_init: "
		       "enable_irq_wake failed for INT_A9_M2A_5\n");

#if defined(CONFIG_QDSP6)
#if (INT_ADSP_A11 == INT_ADSP_A11_SMSM)
		flags |= IRQF_SHARED;
#endif
	r = request_irq(INT_ADSP_A11, smd_dsp_irq_handler,
			flags, "smd_dev", smd_dsp_irq_handler);
	if (r < 0) {
		free_irq(INT_A9_M2A_0, 0);
		free_irq(INT_A9_M2A_5, 0);
		return r;
	}

	r = request_irq(INT_ADSP_A11_SMSM, smsm_irq_handler,
			flags, "smsm_dev", smsm_irq_handler);
	if (r < 0) {
		free_irq(INT_A9_M2A_0, 0);
		free_irq(INT_A9_M2A_5, 0);
		free_irq(INT_ADSP_A11, smd_dsp_irq_handler);
		return r;
	}

	r = enable_irq_wake(INT_ADSP_A11);
	if (r < 0)
		pr_err("smd_core_init: "
		       "enable_irq_wake failed for INT_ADSP_A11\n");

#if (INT_ADSP_A11 != INT_ADSP_A11_SMSM)
	r = enable_irq_wake(INT_ADSP_A11_SMSM);
	if (r < 0)
		pr_err("smd_core_init: enable_irq_wake "
		       "failed for INT_ADSP_A11_SMSM\n");
#endif
	flags &= ~IRQF_SHARED;
#endif

#if defined(CONFIG_DSPS)
	r = request_irq(INT_DSPS_A11, smd_dsps_irq_handler,
			flags, "smd_dev", smd_dsps_irq_handler);
	if (r < 0) {
		free_irq(INT_A9_M2A_0, 0);
		free_irq(INT_A9_M2A_5, 0);
		free_irq(INT_ADSP_A11, smd_dsp_irq_handler);
		free_irq(INT_ADSP_A11_SMSM, smsm_irq_handler);
		return r;
	}

	r = enable_irq_wake(INT_DSPS_A11);
	if (r < 0)
		pr_err("smd_core_init: "
		       "enable_irq_wake failed for INT_ADSP_A11\n");
#endif

	/* we may have missed a signal while booting -- fake
	 * an interrupt to make sure we process any existing
	 * state
	 */
	smsm_irq_handler(0, 0);

	SMD_INFO("smd_core_init() done\n");

	return 0;
}

static int __devinit msm_smd_probe(struct platform_device *pdev)
{
    /* enable smd and smsm info messages */
    msm_smd_debug_mask = 0x0 ;

	SMD_INFO("smd probe\n");
//#if defined (CONFIG_QSD_ARM9_CRASH_FUNCTION)
       smd_qos_req_list = pm_qos_add_request(PM_QOS_CPU_DMA_LATENCY,
                                            PM_QOS_DEFAULT_VALUE);
       if (IS_ERR(smd_qos_req_list)) {
       	   printk("%s: Failed to request pm_qos\n",__func__);
           goto err_probe_add_pm_qos;
       }

	INIT_WORK(&probe_work, smd_channel_probe_worker);
#if defined (CONFIG_QSD_ARM9_CRASH_FUNCTION)
    INIT_WORK(&smd_crash_worker, smd_crash_show_errmsg);
    INIT_DELAYED_WORK(&smd_crash_restart_worker, smd_restart_modem);
    if (crash_work_queue == NULL)
    {
        crash_work_queue = create_singlethread_workqueue("arm9_crash_dump");
        if (!crash_work_queue)
        {
            return -ENOMEM;
        }
    }

    if (restart_work_queue == NULL)
    {
        restart_work_queue = create_singlethread_workqueue("smd_modem_start");
        if (!restart_work_queue)
        {
            return -ENOMEM;
        }
    }

    wake_lock_init(&crash_lock, WAKE_LOCK_SUSPEND, "arm9_crash_dump");
//    pm_qos_add_requirement(PM_QOS_CPU_DMA_LATENCY, "arm9_crash_dump",
//                            PM_QOS_DEFAULT_VALUE);
     pm_qos_add_request(PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);    /* Jagan+ ... Jagan- */
    
    atomic_set(&dumpxxxed, 0);
#endif /* CONFIG_QSD_ARM9_CRASH_FUNCTION */
    if (smsm_init()) {
        pr_err("smsm_init() failed\n");
        return -1;
    }

	if (smd_core_init()) {
		pr_err("smd_core_init() failed\n");
		return -1;
	}

	smd_initialized = 1;

	smd_alloc_loopback_channel();

	return 0;

err_probe_add_pm_qos:
    return -1;
}

static struct platform_driver msm_smd_driver = {
	.probe = msm_smd_probe,
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init msm_smd_init(void)
{
	return platform_driver_register(&msm_smd_driver);
}

module_init(msm_smd_init);

MODULE_DESCRIPTION("MSM Shared Memory Core");
MODULE_AUTHOR("Brian Swetland <swetland@google.com>");
MODULE_LICENSE("GPL");
