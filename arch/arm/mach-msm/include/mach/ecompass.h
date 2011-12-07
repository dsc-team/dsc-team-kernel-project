#ifndef _ECOMPASS_H_
#define _ECOMPASS_H_
#include <asm/ioctl.h>

struct ecompass_ioctl_t {
	unsigned char regaddr;
	unsigned int len;
	unsigned char *buf;
	int pid;
};

struct ecompass_platform_data_t {
	unsigned  int gpioreset;
	unsigned  int gpiointr;
};

#define ECOMPASS_IOC_MAGIC 	'Z'
#define ECOMPASS_IOC_RESET		_IO(ECOMPASS_IOC_MAGIC, 0)
#define ECOMPASS_IOC_WRITE	_IOW(ECOMPASS_IOC_MAGIC, 1, struct ecompass_ioctl_t*)
#define ECOMPASS_IOC_READ		_IOR(ECOMPASS_IOC_MAGIC, 2, struct ecompass_ioctl_t*)
#define ECOMPASS_IOC_REG_INT	_IOW(ECOMPASS_IOC_MAGIC, 3, unsigned int)

#endif

