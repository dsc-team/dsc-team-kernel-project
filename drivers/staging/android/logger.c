/*
 * drivers/misc/logger.c
 *
 * A Logging Subsystem
 *
 * Copyright (C) 2007-2008 Google, Inc.
 *
 * Robert Love <rlove@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/sched.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/logger.h>

#include <asm/ioctls.h>
#ifdef CONFIG_ROUTE_PRINTK_TO_MAINLOG
#include <linux/jiffies.h>
#endif

/*
 * struct logger_log - represents a specific log, such as 'main' or 'radio'
 *
 * This structure lives from module insertion until module removal, so it does
 * not need additional reference counting. The structure is protected by the
 * mutex 'mutex'.
 */
struct logger_log {
	unsigned char 		*buffer;/* the ring buffer itself */
	struct miscdevice	misc;	/* misc device representing the log */
	wait_queue_head_t	wq;	/* wait queue for readers */
	struct list_head	readers; /* this log's readers */
	struct mutex		mutex;	/* mutex protecting buffer */
	size_t			w_off;	/* current write head offset */
	size_t			head;	/* new readers start here */
	size_t			size;	/* size of the log */
};

/*
 * struct logger_reader - a logging device open for reading
 *
 * This object lives from open to release, so we don't need additional
 * reference counting. The structure is protected by log->mutex.
 */
struct logger_reader {
	struct logger_log	*log;	/* associated log */
	struct list_head	list;	/* entry in logger_log's list */
	size_t			r_off;	/* current read head offset */
};

/* logger_offset - returns index 'n' into the log via (optimized) modulus */
#define logger_offset(n)	((n) & (log->size - 1))

#ifdef CONFIG_ROUTE_PRINTK_TO_MAINLOG
#define MAX_LOGLEVEL 7 
#define DEFAULT_GLOBAL_LOGLEVEL MAX_LOGLEVEL
static DEFINE_SPINLOCK(mainlogbuf_lock);
#undef mutex_lock
#define mutex_lock(m) spin_lock_irqsave(&mainlogbuf_lock,flags)
#define mutex_unlock(m) spin_unlock_irqrestore(&mainlogbuf_lock,flags)
static int global_printk2mainlog_level = DEFAULT_GLOBAL_LOGLEVEL;
#endif

/*
 * file_get_log - Given a file structure, return the associated log
 *
 * This isn't aesthetic. We have several goals:
 *
 * 	1) Need to quickly obtain the associated log during an I/O operation
 * 	2) Readers need to maintain state (logger_reader)
 * 	3) Writers need to be very fast (open() should be a near no-op)
 *
 * In the reader case, we can trivially go file->logger_reader->logger_log.
 * For a writer, we don't want to maintain a logger_reader, so we just go
 * file->logger_log. Thus what file->private_data points at depends on whether
 * or not the file was opened for reading. This function hides that dirtiness.
 */
static inline struct logger_log *file_get_log(struct file *file)
{
	if (file->f_mode & FMODE_READ) {
		struct logger_reader *reader = file->private_data;
		return reader->log;
	} else
		return file->private_data;
}

/*
 * get_entry_len - Grabs the length of the payload of the next entry starting
 * from 'off'.
 *
 * Caller needs to hold log->mutex.
 */
static __u32 get_entry_len(struct logger_log *log, size_t off)
{
	__u16 val;

	switch (log->size - off) {
	case 1:
		memcpy(&val, log->buffer + off, 1);
		memcpy(((char *) &val) + 1, log->buffer, 1);
		break;
	default:
		memcpy(&val, log->buffer + off, 2);
	}

	return sizeof(struct logger_entry) + val;
}

/*
 * do_read_log_to_user - reads exactly 'count' bytes from 'log' into the
 * user-space buffer 'buf'. Returns 'count' on success.
 *
 * Caller must hold log->mutex.
 */
static ssize_t do_read_log_to_user(struct logger_log *log,
				   struct logger_reader *reader,
				   char __user *buf,
				   size_t count)
{
	size_t len;

	/*
	 * We read from the log in two disjoint operations. First, we read from
	 * the current read head offset up to 'count' bytes or to the end of
	 * the log, whichever comes first.
	 */
	len = min(count, log->size - reader->r_off);
#ifdef CONFIG_ROUTE_PRINTK_TO_MAINLOG
	memcpy(buf, log->buffer + reader->r_off, len);
#else	
	if (copy_to_user(buf, log->buffer + reader->r_off, len))
		return -EFAULT;
#endif


	/*
	 * Second, we read any remaining bytes, starting back at the head of
	 * the log.
	 */
	if (count != len)
#ifdef CONFIG_ROUTE_PRINTK_TO_MAINLOG
		memcpy(buf + len, log->buffer, count - len);
#else
		if (copy_to_user(buf + len, log->buffer, count - len))
			return -EFAULT;
#endif
	reader->r_off = logger_offset(reader->r_off + count);

	return count;
}

/*
 * logger_read - our log's read() method
 *
 * Behavior:
 *
 * 	- O_NONBLOCK works
 * 	- If there are no log entries to read, blocks until log is written to
 * 	- Atomically reads exactly one log entry
 *
 * Optimal read size is LOGGER_ENTRY_MAX_LEN. Will set errno to EINVAL if read
 * buffer is insufficient to hold next entry.
 */
static ssize_t logger_read(struct file *file, char __user *buf,
			   size_t count, loff_t *pos)
{
	struct logger_reader *reader = file->private_data;
	struct logger_log *log = reader->log;
	ssize_t ret;
	DEFINE_WAIT(wait);
#ifdef CONFIG_ROUTE_PRINTK_TO_MAINLOG
	//unsigned char read_tmp_buf[LOGGER_ENTRY_MAX_LEN];
	unsigned long flags;
	unsigned char *read_tmp_buf = kmalloc(LOGGER_ENTRY_MAX_LEN, GFP_KERNEL);;
	if(read_tmp_buf == NULL) return -ENOMEM;
#endif

start:
	while (1) {
		prepare_to_wait(&log->wq, &wait, TASK_INTERRUPTIBLE);

		mutex_lock(&log->mutex);
		ret = (log->w_off == reader->r_off);
		mutex_unlock(&log->mutex);
		if (!ret)
			break;

		if (file->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			break;
		}

		if (signal_pending(current)) {
			ret = -EINTR;
			break;
		}

		schedule();
	}

	finish_wait(&log->wq, &wait);
	if (ret)
	{
		kfree(read_tmp_buf);
		return ret;
	}

	mutex_lock(&log->mutex);

	/* is there still something to read or did we race? */
	if (unlikely(log->w_off == reader->r_off)) {
		mutex_unlock(&log->mutex);
		goto start;
	}

	/* get the size of the next entry */
	ret = get_entry_len(log, reader->r_off);
	if (count < ret) {
		ret = -EINVAL;
		goto out;
	}
#ifdef CONFIG_ROUTE_PRINTK_TO_MAINLOG
	/* get exactly one entry from the log */
	ret = do_read_log_to_user(log, reader, read_tmp_buf, ret); //read to local buf then copy to user buf
#else
	ret = do_read_log_to_user(log, reader, buf, ret);
#endif

out:
	mutex_unlock(&log->mutex);
#ifdef CONFIG_ROUTE_PRINTK_TO_MAINLOG	
	if(unlikely(ret > count || copy_to_user(buf, read_tmp_buf, ret)))
	{
		ret = -EFAULT;
	}
#endif	
	kfree(read_tmp_buf);
	return ret;
}

/*
 * get_next_entry - return the offset of the first valid entry at least 'len'
 * bytes after 'off'.
 *
 * Caller must hold log->mutex.
 */
static size_t get_next_entry(struct logger_log *log, size_t off, size_t len)
{
	size_t count = 0;

	do {
		size_t nr = get_entry_len(log, off);
		off = logger_offset(off + nr);
		count += nr;
	} while (count < len);

	return off;
}

/*
 * clock_interval - is a < c < b in mod-space? Put another way, does the line
 * from a to b cross c?
 */
static inline int clock_interval(size_t a, size_t b, size_t c)
{
	if (b < a) {
		if (a < c || b >= c)
			return 1;
	} else {
		if (a < c && b >= c)
			return 1;
	}

	return 0;
}

/*
 * fix_up_readers - walk the list of all readers and "fix up" any who were
 * lapped by the writer; also do the same for the default "start head".
 * We do this by "pulling forward" the readers and start head to the first
 * entry after the new write head.
 *
 * The caller needs to hold log->mutex.
 */
static void fix_up_readers(struct logger_log *log, size_t len)
{
	size_t old = log->w_off;
	size_t new = logger_offset(old + len);
	struct logger_reader *reader;

	if (clock_interval(old, new, log->head))
		log->head = get_next_entry(log, log->head, len);

	list_for_each_entry(reader, &log->readers, list)
		if (clock_interval(old, new, reader->r_off))
			reader->r_off = get_next_entry(log, reader->r_off, len);
}

/*
 * do_write_log - writes 'len' bytes from 'buf' to 'log'
 *
 * The caller needs to hold log->mutex.
 */
static void do_write_log(struct logger_log *log, const void *buf, size_t count)
{
	size_t len;

	len = min(count, log->size - log->w_off);
	memcpy(log->buffer + log->w_off, buf, len);

	if (count != len)
		memcpy(log->buffer, buf + len, count - len);

	log->w_off = logger_offset(log->w_off + count);

}

#ifndef CONFIG_ROUTE_PRINTK_TO_MAINLOG
/*
 * do_write_log_user - writes 'len' bytes from the user-space buffer 'buf' to
 * the log 'log'
 *
 * The caller needs to hold log->mutex.
 *
 * Returns 'count' on success, negative error code on failure.
 */
static ssize_t do_write_log_from_user(struct logger_log *log,
				      const void __user *buf, size_t count)
{
	size_t len;

	len = min(count, log->size - log->w_off);
	if (len && copy_from_user(log->buffer + log->w_off, buf, len))
		return -EFAULT;

	if (count != len)
		if (copy_from_user(log->buffer, buf + len, count - len))
			return -EFAULT;

	log->w_off = logger_offset(log->w_off + count);

	return count;
}
#endif

#ifdef CONFIG_ROUTE_PRINTK_TO_MAINLOG
//static void fill_log_type(enum log_type *t,struct logger_log *log);
//extern void write_entry_to_reset_key_log(char* buf, uint32_t len,enum log_type t,__s32 sec, __s32 nsec, __s32 pid, __s32 tid);
#endif
/*
 * logger_aio_write - our write method, implementing support for write(),
 * writev(), and aio_write(). Writes are our fast path, and we try to optimize
 * them above all else.
 */
ssize_t logger_aio_write(struct kiocb *iocb, const struct iovec *iov,
			 unsigned long nr_segs, loff_t ppos)
{
	struct logger_log *log = file_get_log(iocb->ki_filp);
#ifndef CONFIG_ROUTE_PRINTK_TO_MAINLOG
	size_t orig = log->w_off;
#endif	
	struct timespec now;
	
	struct logger_entry header;
	ssize_t ret = 0;
#ifdef CONFIG_ROUTE_PRINTK_TO_MAINLOG
	//unsigned long long j;
	//unsigned long rem;
	//unsigned char write_tmp_buf[LOGGER_ENTRY_MAX_LEN];
	unsigned char *write_tmp_buf = kmalloc(LOGGER_ENTRY_MAX_LEN, GFP_KERNEL);
	unsigned long flags;
//	enum log_type t = RESET_KEY_LOG_NOT_A_LOG;
	if(write_tmp_buf == NULL) return -ENOMEM;
#endif

#if 0
	j = jiffies - INITIAL_JIFFIES;
	rem = do_div(j,HZ);
	
	header.pid = current->tgid;
	header.tid = current->pid;
	header.sec = (unsigned long) j;
	header.nsec = rem*(1000000000/HZ);
#else
	now = current_kernel_time();

	header.pid = current->tgid;
	header.tid = current->pid;
	header.sec = now.tv_sec;
	header.nsec = now.tv_nsec;
#endif
	header.len = min_t(size_t, iocb->ki_left, LOGGER_ENTRY_MAX_PAYLOAD);

	/* null writes succeed, return zero */
	if (unlikely(!header.len || iocb->ki_left > LOGGER_ENTRY_MAX_PAYLOAD))
	{
		kfree(write_tmp_buf);
		return 0;
	}

#ifdef CONFIG_ROUTE_PRINTK_TO_MAINLOG
//	if(nr_segs != 3 && log == get_addr_of_mainlog()) printk(KERN_INFO "nr_segs[%ld]\n",nr_segs);	
	while (nr_segs-- > 0)
	{
		size_t len;

		/* figure out how much of this vector we can keep */
		len = min_t(size_t, iov->iov_len, header.len - ret);

		if(len && copy_from_user(write_tmp_buf+ret, iov->iov_base, len))//write to local buf then write to log buf
		{
			kfree(write_tmp_buf);
			return -EFAULT;
		}
		iov++;
		ret += len;
	}
//	write_tmp_buf[ret] = '\0';
//	printk(KERN_INFO "msg_in[%s]\n",
//	       write_tmp_buf);
#endif

	mutex_lock(&log->mutex);

	/*
	 * Fix up any readers, pulling them forward to the first readable
	 * entry after (what will be) the new write offset. We do this now
	 * because if we partially fail, we can end up with clobbered log
	 * entries that encroach on readable buffer.
	 */
	fix_up_readers(log, sizeof(struct logger_entry) + header.len);

	do_write_log(log, &header, sizeof(struct logger_entry));

#ifdef CONFIG_ROUTE_PRINTK_TO_MAINLOG
	do_write_log(log, write_tmp_buf, ret);
#else
	while (nr_segs-- > 0) {
		size_t len;
		ssize_t nr;

		/* figure out how much of this vector we can keep */
		len = min_t(size_t, iov->iov_len, header.len - ret);

		/* write out this segment's payload */
		nr = do_write_log_from_user(log, iov->iov_base, len);
		if (unlikely(nr < 0)) {
			log->w_off = orig;
			mutex_unlock(&log->mutex);
			return nr;
		}

		iov++;
		ret += nr;
	}
#endif

	mutex_unlock(&log->mutex);

	/* wake up any blocked readers */
	wake_up_interruptible(&log->wq);
	kfree(write_tmp_buf);
	return ret;
}

static struct logger_log *get_log_from_minor(int);

/*
 * logger_open - the log's open() file operation
 *
 * Note how near a no-op this is in the write-only case. Keep it that way!
 */
static int logger_open(struct inode *inode, struct file *file)
{
	struct logger_log *log;
	int ret;
#ifdef CONFIG_ROUTE_PRINTK_TO_MAINLOG
	unsigned long flags;
#endif

	ret = nonseekable_open(inode, file);
	if (ret)
		return ret;

	log = get_log_from_minor(MINOR(inode->i_rdev));
	if (!log)
		return -ENODEV;

	if (file->f_mode & FMODE_READ) {
		struct logger_reader *reader;

		reader = kmalloc(sizeof(struct logger_reader), GFP_KERNEL);
		if (!reader)
			return -ENOMEM;

		reader->log = log;
		INIT_LIST_HEAD(&reader->list);

		mutex_lock(&log->mutex);
		reader->r_off = log->head;
		list_add_tail(&reader->list, &log->readers);
		mutex_unlock(&log->mutex);

		file->private_data = reader;
	} else
		file->private_data = log;

	return 0;
}

/*
 * logger_release - the log's release file operation
 *
 * Note this is a total no-op in the write-only case. Keep it that way!
 */
static int logger_release(struct inode *ignored, struct file *file)
{
	if (file->f_mode & FMODE_READ) {
		struct logger_reader *reader = file->private_data;
		list_del(&reader->list);
		kfree(reader);
	}

	return 0;
}

/*
 * logger_poll - the log's poll file operation, for poll/select/epoll
 *
 * Note we always return POLLOUT, because you can always write() to the log.
 * Note also that, strictly speaking, a return value of POLLIN does not
 * guarantee that the log is readable without blocking, as there is a small
 * chance that the writer can lap the reader in the interim between poll()
 * returning and the read() request.
 */
static unsigned int logger_poll(struct file *file, poll_table *wait)
{
	struct logger_reader *reader;
	struct logger_log *log;
	unsigned int ret = POLLOUT | POLLWRNORM;
#ifdef CONFIG_ROUTE_PRINTK_TO_MAINLOG
	unsigned long flags;
#endif

	if (!(file->f_mode & FMODE_READ))
		return ret;

	reader = file->private_data;
	log = reader->log;

	poll_wait(file, &log->wq, wait);

	mutex_lock(&log->mutex);
	if (log->w_off != reader->r_off)
		ret |= POLLIN | POLLRDNORM;
	mutex_unlock(&log->mutex);

	return ret;
}

static long logger_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct logger_log *log = file_get_log(file);
	struct logger_reader *reader;
	long ret = -ENOTTY;
#ifdef CONFIG_ROUTE_PRINTK_TO_MAINLOG
	unsigned long flags;
#endif

	mutex_lock(&log->mutex);

	switch (cmd) {
	case LOGGER_GET_LOG_BUF_SIZE:
		ret = log->size;
		break;
	case LOGGER_GET_LOG_LEN:
		if (!(file->f_mode & FMODE_READ)) {
			ret = -EBADF;
			break;
		}
		reader = file->private_data;
		if (log->w_off >= reader->r_off)
			ret = log->w_off - reader->r_off;
		else
			ret = (log->size - reader->r_off) + log->w_off;
		break;
	case LOGGER_GET_NEXT_ENTRY_LEN:
		if (!(file->f_mode & FMODE_READ)) {
			ret = -EBADF;
			break;
		}
		reader = file->private_data;
		if (log->w_off != reader->r_off)
			ret = get_entry_len(log, reader->r_off);
		else
			ret = 0;
		break;
	case LOGGER_FLUSH_LOG:
		if (!(file->f_mode & FMODE_WRITE)) {
			ret = -EBADF;
			break;
		}
		list_for_each_entry(reader, &log->readers, list)
			reader->r_off = log->w_off;
		log->head = log->w_off;

		ret = 0;
		break;
#ifdef CONFIG_ROUTE_PRINTK_TO_MAINLOG
	case LOGGER_SET_GLOBAL_PRINTK2MAINLOG_LEVEL:
		global_printk2mainlog_level = arg;
		ret = 0;
		break;
#endif
	}

	mutex_unlock(&log->mutex);

	return ret;
}

static const struct file_operations logger_fops = {
	.owner = THIS_MODULE,
	.read = logger_read,
	.aio_write = logger_aio_write,
	.poll = logger_poll,
	.unlocked_ioctl = logger_ioctl,
	.compat_ioctl = logger_ioctl,
	.open = logger_open,
	.release = logger_release,
};

/*
 * Defines a log structure with name 'NAME' and a size of 'SIZE' bytes, which
 * must be a power of two, greater than LOGGER_ENTRY_MAX_LEN, and less than
 * LONG_MAX minus LOGGER_ENTRY_MAX_LEN.
 */

#define DEFINE_LOGGER_DEVICE(VAR, NAME, SIZE) \
static unsigned char _buf_ ## VAR[SIZE]; \
static struct logger_log VAR = { \
	.buffer = _buf_ ## VAR, \
	.misc = { \
		.minor = MISC_DYNAMIC_MINOR, \
		.name = NAME, \
		.fops = &logger_fops, \
		.parent = NULL, \
	}, \
	.wq = __WAIT_QUEUE_HEAD_INITIALIZER(VAR .wq), \
	.readers = LIST_HEAD_INIT(VAR .readers), \
	.mutex = __MUTEX_INITIALIZER(VAR .mutex), \
	.w_off = 0, \
	.head = 0, \
	.size = SIZE, \
};

#ifdef CONFIG_ROUTE_PRINTK_TO_MAINLOG
DEFINE_LOGGER_DEVICE(log_main, LOGGER_LOG_MAIN, 1*1024*1024)		
#else
DEFINE_LOGGER_DEVICE(log_main, LOGGER_LOG_MAIN, 64*1024)
#endif
DEFINE_LOGGER_DEVICE(log_events, LOGGER_LOG_EVENTS, 256*1024)
DEFINE_LOGGER_DEVICE(log_radio, LOGGER_LOG_RADIO, 512*1024)
DEFINE_LOGGER_DEVICE(log_system, LOGGER_LOG_SYSTEM, 64*1024)

/* Number of elements in this array must match 'enum logidx' */
static struct logger_log *logs[] = {
	&log_main,
	&log_events,
	&log_radio,
};

#ifdef CONFIG_ROUTE_PRINTK_TO_MAINLOG

/*
 * printk call this function after locking
 */
void emit_mainlog(char* printk_buf, int len, unsigned long long t, unsigned long nanosec_rem, int log_level)
{	
	struct logger_log *log = &log_main;
	struct logger_entry header;
	//unsigned long long j;
	//unsigned long rem;
	unsigned char pri = (MAX_LOGLEVEL + 2 - log_level);
	char tag [] = "PrintK\0";
	int tag_len = 7; //should match the content of tag
	struct timespec now;
	
	if(pri > MAX_LOGLEVEL)
	{
		pri = MAX_LOGLEVEL;
	}
	
	//j = jiffies - INITIAL_JIFFIES;
	//rem = do_div(j,HZ);
	
	header.pid = current->tgid;
	header.tid = current->pid;
	//header.sec = (unsigned long) j;
	//header.nsec = rem*(1000000000/HZ);
	now = current_kernel_time();

	header.sec = now.tv_sec;
	header.nsec = now.tv_nsec;
	header.len = min_t(size_t, len+1+1+tag_len/*should match the content of tag*/, LOGGER_ENTRY_MAX_PAYLOAD);
	
	/* null writes succeed, return zero */
	if (unlikely(!header.len || ((len+1+1+tag_len) > LOGGER_ENTRY_MAX_PAYLOAD)))
		return;
	
	spin_lock(&mainlogbuf_lock);
	
	if(log_level > global_printk2mainlog_level)
	{
		spin_unlock(&mainlogbuf_lock);
		return;
	}
	
	fix_up_readers(log, sizeof(struct logger_entry) + header.len);

	do_write_log(log, &header, sizeof(struct logger_entry));
	do_write_log(log, &pri, sizeof(pri));
	do_write_log(log, tag, tag_len);
	do_write_log(log, printk_buf, len+1);

	spin_unlock(&mainlogbuf_lock);
	
	/* wake up any blocked readers */
	wake_up_interruptible(&log->wq);	
}
#endif

#ifdef CONFIG_ROUTE_PRINTK_TO_MAINLOG
static char klw_buf[LOGGER_ENTRY_MAX_PAYLOAD];
#endif

static int kernel_logger_write(struct logger_log * const log,
				const unsigned char priority,
				const char __kernel * const tag,
				const char __kernel * const fmt,
				const va_list args)
{
#ifdef CONFIG_ROUTE_PRINTK_TO_MAINLOG
	struct logger_entry header;
	//unsigned long long j;
	//unsigned long rem;
	unsigned long flags;
	int tag_bytes = strlen(tag) + 1, msg_bytes;
	struct timespec now;
	
	//j = jiffies - INITIAL_JIFFIES;
	//rem = do_div(j,HZ);
	
	spin_lock_irqsave(&mainlogbuf_lock,flags);
	
	msg_bytes = vscnprintf(klw_buf,sizeof(klw_buf), fmt, args);
	if (unlikely(!msg_bytes))
	{
		spin_unlock_irqrestore(&mainlogbuf_lock,flags);
		return -ENOMEM;
	}
	msg_bytes += 1;
	
//	if ((msg_bytes + tag_bytes + 1) > LOGGER_ENTRY_MAX_PAYLOAD)
//	{
//		spin_unlock_irqrestore(&mainlogbuf_lock,flags);
//		return -E2BIG;
//	}	
	header.pid = current->tgid;
	header.tid = current->pid;
	//header.sec = (unsigned long) j;
	//header.nsec = rem*(1000000000/HZ);
	now = current_kernel_time();

	header.sec = now.tv_sec;
	header.nsec = now.tv_nsec;
	
	header.len = min_t(size_t, msg_bytes+1+tag_bytes, LOGGER_ENTRY_MAX_PAYLOAD);
	
	/* null writes succeed, return zero */
	if (unlikely(!header.len || ((msg_bytes+1+tag_bytes) > LOGGER_ENTRY_MAX_PAYLOAD)))
	{
		spin_unlock_irqrestore(&mainlogbuf_lock,flags);
		return 0;
	}
	
	fix_up_readers(log, sizeof(struct logger_entry) + header.len);

	do_write_log(log, &header, sizeof(struct logger_entry));
	do_write_log(log, &priority, sizeof(unsigned char));
	do_write_log(log, tag, tag_bytes);
	do_write_log(log, klw_buf, msg_bytes);

	spin_unlock_irqrestore(&mainlogbuf_lock,flags);

	/* wake up any blocked readers */
	wake_up_interruptible(&log->wq);
	return header.len;
#else
	return 0;
#endif	
}

int logger_write(const enum logidx index,
		const unsigned char prio,
		const char __kernel * const tag,
		const char __kernel * const fmt,
		...)
{
	va_list vargs;
	int ret;

	if (likely(index >= 0 && index < LOG_INVALID_IDX)) {
		va_start(vargs, fmt);
		ret = kernel_logger_write(logs[index], prio, tag, fmt, vargs);
		va_end(vargs);
	} else {
		ret = -EINVAL;
	}
	return ret;
}
EXPORT_SYMBOL(logger_write);

static struct logger_log *get_log_from_minor(int minor)
{
	if (log_main.misc.minor == minor)
		return &log_main;
	if (log_events.misc.minor == minor)
		return &log_events;
	if (log_radio.misc.minor == minor)
		return &log_radio;
	if (log_system.misc.minor == minor)
		return &log_system;
	return NULL;
}

static int __init init_log(struct logger_log *log)
{
	int ret;

	ret = misc_register(&log->misc);
	if (unlikely(ret)) {
		printk(KERN_ERR "logger: failed to register misc "
		       "device for log '%s'!\n", log->misc.name);
		return ret;
	}

	printk(KERN_INFO "logger: created %luK log '%s'\n",
	       (unsigned long) log->size >> 10, log->misc.name);

	return 0;
}

static int __init logger_init(void)
{
	int ret;

	ret = init_log(&log_main);
	if (unlikely(ret))
		goto out;

	ret = init_log(&log_events);
	if (unlikely(ret))
		goto out;

	ret = init_log(&log_radio);
	if (unlikely(ret))
		goto out;

	ret = init_log(&log_system);
	if (unlikely(ret))
		goto out;

out:
	return ret;
}
device_initcall(logger_init);
