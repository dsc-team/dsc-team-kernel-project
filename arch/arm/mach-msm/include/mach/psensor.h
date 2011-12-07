#ifndef _PSENSOR_H_
#define _PSENSOR_H_
#include <asm/ioctl.h>


struct psensor_ioctl_status {
	int ps_out_status;
	int ps_sd_status;
};

//IOCTL
#define PSENSOR_IOC_MAGIC 'p'
#define PSENSOR_IOC_RESET		_IO(PSENSOR_IOC_MAGIC, 0)
#define PSENSOR_IOC_ENABLE		_IO(PSENSOR_IOC_MAGIC, 1)
#define PSENSOR_IOC_DISABLE		_IO(PSENSOR_IOC_MAGIC, 2)
#define PSENSOR_IOC_GET_STATUS		_IOR(PSENSOR_IOC_MAGIC, 3, unsigned int)

#endif

