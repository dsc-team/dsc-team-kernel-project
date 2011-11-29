#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define	MAX_MODE_LENGTH	1024*4

#define	BOOTLOADER_PHYS	0x000F2000
#define	BOOTLOADER_SIZE 1024*4		// minimum 4k bytes

#define	SECURE_BOOT_PHYS 0x000F0000
#define	SECURE_BOOT_SIZE 1024*4

static struct proc_dir_entry	*proc_loader;
static char			*loader_mode;
static struct proc_dir_entry	*proc_recovery;
static char			*recovery_mode;
static struct proc_dir_entry	*proc_secure;
static uint32_t			*secure_code;

#define	LOADER_BOOT	0
#define	LOADER_RECOVERY	1

static char	mode[][10] = {
	"boot",
	"recovery"
};

static int proc_calc_metrics(char *page, char **start, off_t off,
				int count, int *eof, int len)
{
        if (len <= off+count) *eof = 1;
        *start = page + off;
        len -= off;
        if (len>count) len = count;
        if (len<0) len = 0;
        return len;
}

static int proc_loader_read(char *page, char **start, off_t off,
				int count, int *eof, void *data)
{
	int len;
	len = sprintf(page, "%s\n", loader_mode);
	return proc_calc_metrics(page, start, off, count, eof, len);
}

static int proc_loader_write(struct file *filp, const char __user *buffer,
				unsigned long count, void *data)
{
	int	i;
	int	num = (count<MAX_MODE_LENGTH) ? count : (MAX_MODE_LENGTH-1);
	if (copy_from_user(loader_mode, buffer, num)) {
		printk(KERN_ERR "%s: invalid mode\n", __func__);
		strcpy(loader_mode, mode[LOADER_BOOT]);
		return -EFAULT;
	}
	loader_mode[num]='\0';
	for(i=0;i<2;i++) {
		if (!strncmp(loader_mode,mode[i],strlen(mode[i]))) break;
	}
	if (i==2) strcpy(loader_mode, mode[LOADER_BOOT]);
	return num;
}

static int proc_recovery_read(char *page, char **start, off_t off,
				int count, int *eof, void *data)
{
	int len;
	len = sprintf(page, "%s\n", recovery_mode);
	return proc_calc_metrics(page, start, off, count, eof, len);
}

static int proc_secure_read(char *page, char **start, off_t off,
				int count, int *eof, void *data)
{
	int len;
	len = sprintf(page, "%x\n", *(secure_code+0x1C/sizeof(uint32_t)));
	return proc_calc_metrics(page, start, off, count, eof, len);
}

static int __init proc_loader_init(void)
{
	loader_mode = ioremap_nocache(BOOTLOADER_PHYS, BOOTLOADER_SIZE);
	if (!loader_mode) {
		printk(KERN_ERR "%s: couldn't mapping loader mode address\n", __func__);
		return -ENOMEM;
	}

	proc_loader = create_proc_entry("loader", S_IRUSR|S_IWUSR, NULL);
	if (!proc_loader) {
		printk(KERN_ERR "%s: couldn't create proc entry\n", __func__);
		iounmap(loader_mode);
		return -EPERM;
	}
	proc_loader->read_proc = proc_loader_read;
	proc_loader->write_proc = proc_loader_write;

	recovery_mode = loader_mode + (0xF2300-0xF2000);
	proc_recovery = create_proc_entry("recovery", S_IRUSR|S_IWUSR, NULL);
	if (!proc_recovery) {
		printk(KERN_ERR "%s: couldn't create proc entry\n", __func__);
		iounmap(loader_mode);
		return -EPERM;
	}
	proc_recovery->read_proc = proc_recovery_read;

	strcpy(loader_mode,mode[LOADER_BOOT]);
	if (strncmp(recovery_mode, "SD", 3)) *recovery_mode = 0;

	secure_code = (uint32_t*) ioremap_nocache(SECURE_BOOT_PHYS, SECURE_BOOT_SIZE);
	if (!secure_code) {
		printk(KERN_ERR "%s: couldn't mapping secure code address\n", __func__);
		return -ENOMEM;
	}

	proc_secure = create_proc_entry("secure_mode", S_IRUSR, NULL);
	if (!proc_secure) {
		printk(KERN_ERR "%s: couldn't create proc entry\n", __func__);
		iounmap(secure_code);
		return -EPERM;
	}
	proc_secure->read_proc = proc_secure_read;

	return 0;
}

static void __exit proc_loader_exit(void)
{
	iounmap(loader_mode);
}
module_init(proc_loader_init);
module_exit(proc_loader_exit);
