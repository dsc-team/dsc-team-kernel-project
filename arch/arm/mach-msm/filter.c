#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/types.h>
#include <linux/string.h>
#include <asm/setup.h>
#include <mach/logfilter.h>

#define	ATAG_LOG_FILTER	 0xafd137cb

struct log_filter_entry {
	__u32	addr;
	__u32	size;
};

static unsigned	default_filter = 1;

unsigned int	log_filter_addr;
EXPORT_SYMBOL(log_filter_addr);
void __iomem*	log_filter_base;
unsigned int	log_filter_size;
EXPORT_SYMBOL(log_filter_size);


unsigned *filter_get(const char *tag)
{
	struct filter_header	*header = (struct filter_header*)log_filter_base;
	char	*str;
	int	*val;
	if (log_filter_base) {
		str = (char*)(log_filter_base + header->tag_offset);
		val = (int*)(log_filter_base + header->val_offset);
		while( unlikely(str < (char*)(log_filter_base + header->val_offset)) ) {
			if (!strncmp(str,tag,24)) {
				printk(KERN_INFO "%s: tag found. val:%d\n",
					__func__, *val);
				return (val);
			}
			str += 24;
			val ++;
		}
	}
	return &default_filter;
}
EXPORT_SYMBOL(filter_get);

static int __init parse_tag_log_filter(const struct tag *tag)
{
	struct log_filter_entry	*entry = (void *)&tag->u;
	log_filter_addr = entry->addr;
	log_filter_size = entry->size;
	log_filter_base = 0;
	return 0;
}

__tagtable(ATAG_LOG_FILTER, parse_tag_log_filter);

static int __init filter_init(void)
{
	if (log_filter_addr) {
		printk("%s\n",__func__);
		log_filter_base = ioremap(log_filter_addr, log_filter_size);
		if (!log_filter_base) {
			printk(KERN_ERR "%s: unable to map filter addr",__func__);
		}
	}
	return 0;
}

postcore_initcall(filter_init);
