#ifndef	__LOGFILTER_H_
#define	__LOGFILTER_H_

#include <linux/kernel.h>

struct filter_header {
	int	version;
	int	tag_num;
	int	tag_offset;
	int	val_offset;
};

typedef enum {
	LOG_EMERG,
	LOG_ALERT,
	LOG_CRIT,
	LOG_ERR,
	LOG_WARNING,
	LOG_NOTICE,
	LOG_INFO,
	LOG_DEBUG,
} log_level;

unsigned *filter_get(const char *tag);

#ifdef CONFIG_LOG_FILTER

#define	LOGF_INIT(name)	filter_get(name)

#define LOGF_PRINTK(filter, level, message, ...) \
	do { \
		printk(KERN_INFO pr_fmt(message), ## __VA_ARGS__); \
		if (*filter >= level) {  \
			switch (level) { \
			case LOG_EMERG:  \
				printk(KERN_EMERG pr_fmt(message), ## __VA_ARGS__); break; \
			case LOG_ALERT:  \
				printk(KERN_ALERT pr_fmt(message), ## __VA_ARGS__); break; \
			case LOG_CRIT: \
				printk(KERN_CRIT pr_fmt(message), ## __VA_ARGS__); break; \
			case LOG_ERR: \
				printk(KERN_ERR pr_fmt(message), ## __VA_ARGS__); break; \
			case LOG_WARNING: \
				printk(KERN_WARNING pr_fmt(message), ## __VA_ARGS__); break; \
			case LOG_NOTICE: \
				printk(KERN_NOTICE pr_fmt(message), ## __VA_ARGS__); break; \
			case LOG_INFO: \
				printk(KERN_INFO pr_fmt(message), ## __VA_ARGS__); break; \
			case LOG_DEBUG: \
				printk(KERN_DEBUG pr_fmt(message), ## __VA_ARGS__); break; \
			default: \
				printk(KERN_INFO pr_fmt(message), ## __VA_ARGS__); break; \
			} \
		} \
	} while(0)
#else

#define	LOGF_INIT(name)	do {} while(0)
#define	LOGF_PRINTK(filter, level, message, ...) \
	do { \
		switch (level) { \
		case LOG_EMERG:  \
			printk(KERN_EMERG pr_fmt(message), ## __VA_ARGS__); break; \
		case LOG_ALERT:  \
			printk(KERN_ALERT pr_fmt(message), ## __VA_ARGS__); break; \
		case LOG_CRIT: \
			printk(KERN_CRIT pr_fmt(message), ## __VA_ARGS__); break; \
		case LOG_ERR: \
			printk(KERN_ERR pr_fmt(message), ## __VA_ARGS__); break; \
		case LOG_WARNING: \
			printk(KERN_WARNING pr_fmt(message), ## __VA_ARGS__); break; \
		case LOG_NOTICE: \
			printk(KERN_NOTICE pr_fmt(message), ## __VA_ARGS__); break; \
		case LOG_INFO: \
			printk(KERN_INFO pr_fmt(message), ## __VA_ARGS__); break; \
		case LOG_DEBUG: \
			printk(KERN_DEBUG pr_fmt(message), ## __VA_ARGS__); break; \
		} \
	} while(0)

#endif

#endif
