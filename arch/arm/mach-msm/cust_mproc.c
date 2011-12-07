#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/debugfs.h>
#include <mach/custmproc.h>

#include "proc_comm.h"

int cust_mproc_comm1(unsigned *arg1, unsigned *arg2)
{
	int	err;
	err = msm_proc_comm(PCOM_CUSTOMER_CMD1, arg1, arg2);
	return err;
}
EXPORT_SYMBOL(cust_mproc_comm1);

int cust_mproc_comm2(unsigned *arg1, unsigned *arg2)
{
	int	err;
	err = msm_proc_comm(PCOM_CUSTOMER_CMD2, arg1, arg2);
	return err;
}
EXPORT_SYMBOL(cust_mproc_comm2);

int cust_mproc_comm3(unsigned *arg1, unsigned *arg2)
{
	int	err;
	err = msm_proc_comm(PCOM_CUSTOMER_CMD3, arg1, arg2);
	return err;
}
EXPORT_SYMBOL(cust_mproc_comm3);


