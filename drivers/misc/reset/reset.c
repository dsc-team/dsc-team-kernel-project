#include <linux/init.h>
#include <linux/module.h>

#include "../../../arch/arm/mach-msm/proc_comm.h"

MODULE_LICENSE("Dual BSD/GPL");

static int reset_init(void)
{
    uint32_t restart_reason;
    
    msm_proc_comm(PCOM_RESET_CHIP, &restart_reason, 0);
    for (;;)
    ;
    return 0;
}

static void reset_exit(void)
{
    printk(KERN_INFO "Goodbye, reset chip\n");
}

module_init(reset_init);
module_exit(reset_exit);
