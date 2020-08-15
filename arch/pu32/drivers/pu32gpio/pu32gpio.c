// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/poll.h>
#include <linux/wait.h>

#include <uapi/asm/gpio.h>

#include <pu32.h>

#include <hwdrvgpio.h>
#include <hwdrvintctrl.h>

#define MAX_PERDEV_IOCNT ((sizeof(unsigned long)*8) /* Exact count is (ARCHBITSZ-1), but we need a char device that represent all the IOs of the device */)

#define DBNCR_HZ (10 /* Debounce for 100ms */)

typedef struct {
	atomic_long_t in_use;
	unsigned long oval;
	dev_t dev;
	struct class *class;
	struct cdev cdev;
	wait_queue_head_t wq;
	unsigned long irq;
	hwdrvgpio hwdev;
	struct list_head list;
} pu32gpio_devdata_t;

static LIST_HEAD(pu32gpio_devs);

static long pu32gpio_param_use_bin = 0;
static long pu32gpio_param_use_wq = 0;
static long pu32gpio_param_mask = -1;
static long pu32gpio_param_iodir = 0;
static long pu32gpio_param_dbncr_hz = DBNCR_HZ;
static long pu32gpio_param_iocnt = 0;

static int pu32gpio_param_use_bin_ops_set (const char *val, const struct kernel_param *kp) {
	unsigned long v;
	int ret = kstrtol (val, 0, &v);
	if (ret)
		return ret;
	if (v > 1)
		return -EINVAL;
	pu32gpio_param_use_bin = v;
	return 0;
}
static const struct kernel_param_ops pu32gpio_param_use_bin_ops = {
	.set = pu32gpio_param_use_bin_ops_set
};

static int pu32gpio_param_use_wq_ops_set (const char *val, const struct kernel_param *kp) {
	unsigned long v;
	int ret = kstrtol (val, 0, &v);
	if (ret)
		return ret;
	if (v > 1)
		return -EINVAL;
	pu32gpio_param_use_wq = v;
	return 0;
}
static const struct kernel_param_ops pu32gpio_param_use_wq_ops = {
	.set = pu32gpio_param_use_wq_ops_set
};

static int pu32gpio_param_mask_ops_set (const char *val, const struct kernel_param *kp) {
	unsigned long v;
	int ret = kstrtol (val, 0, &v);
	if (ret)
		return ret;
	pu32gpio_param_mask = v;
	return 0;
}
static const struct kernel_param_ops pu32gpio_param_mask_ops = {
	.set = pu32gpio_param_mask_ops_set
};

static int pu32gpio_param_iodir_ops_set (const char *val, const struct kernel_param *kp) {
	unsigned long v;
	int ret = kstrtol (val, 0, &v);
	if (ret)
		return ret;
	pu32gpio_param_iodir = v;
	return 0;
}
static const struct kernel_param_ops pu32gpio_param_iodir_ops = {
	.set = pu32gpio_param_iodir_ops_set
};

static int pu32gpio_param_dbncr_hz_ops_set (const char *val, const struct kernel_param *kp) {
	unsigned long v;
	int ret = kstrtol (val, 0, &v);
	if (ret)
		return ret;
	pu32gpio_param_dbncr_hz = v;
	return 0;
}
static const struct kernel_param_ops pu32gpio_param_dbncr_hz_ops = {
	.set = pu32gpio_param_dbncr_hz_ops_set
};

module_param_cb(IOCTL_USE_BIN, &pu32gpio_param_use_bin_ops, &pu32gpio_param_use_bin, 0220);
MODULE_PARM_DESC(IOCTL_USE_BIN, "use binary instead of text");

module_param_cb(IOCTL_USE_WQ, &pu32gpio_param_use_wq_ops, &pu32gpio_param_use_wq, 0220);
MODULE_PARM_DESC(IOCTL_USE_WQ, "use interrupt instead of polling");

module_param_cb(IOCTL_MASK, &pu32gpio_param_mask_ops, &pu32gpio_param_mask, 0220);
MODULE_PARM_DESC(IOCTL_MASK, "mask of bits to use");

module_param_cb(IOCTL_IODIR, &pu32gpio_param_iodir_ops, &pu32gpio_param_iodir, 0220);
MODULE_PARM_DESC(IOCTL_IODIR, "IO direction bits: 0(in) 1(out)");

module_param_cb(IOCTL_DBNCR_HZ, &pu32gpio_param_dbncr_hz_ops, &pu32gpio_param_dbncr_hz, 0220);
MODULE_PARM_DESC(IOCTL_DBNCR_HZ, "debouncer hz");

static long pu32gpio_param_donotuse = 0;
static int __init pu32gpio_param_donotuse_fn (char *buf) {
	if (buf)
		pu32gpio_param_donotuse =
			(buf[0] == '1' && !buf[1]);
	return 0;
}
early_param ("pu32gpio_donotuse", pu32gpio_param_donotuse_fn);

static irqreturn_t pu32gpio_isr (int _, void *isrdata) {
	pu32gpio_devdata_t *devdata = isrdata;
	wake_up_interruptible(&devdata->wq);
	return IRQ_HANDLED;
}

static int pu32gpio_open (struct inode *inode, struct file *file);
static int pu32gpio_release (struct inode *inode, struct file *file);
static long pu32gpio_ioctl (struct file *file, unsigned int cmd, unsigned long arg);
static ssize_t pu32gpio_read (struct file *file, char __user *buf, size_t count, loff_t *offset);
static ssize_t pu32gpio_write (struct file *file, const char __user *buf, size_t count, loff_t *offset);
static loff_t pu32gpio_llseek (struct file *, loff_t, int);
static __poll_t pu32gpio_poll (struct file *, struct poll_table_struct *);

static const struct file_operations pu32gpio_fops = {
	 .owner          = THIS_MODULE
	,.open           = pu32gpio_open
	,.release        = pu32gpio_release
	,.unlocked_ioctl = pu32gpio_ioctl
	,.read           = pu32gpio_read
	,.write          = pu32gpio_write
	,.llseek         = pu32gpio_llseek
	,.poll           = pu32gpio_poll
};

static int pu32gpio_uevent (struct device *dev, struct kobj_uevent_env *env) {
	//add_uevent_var (env, "DEVMODE=%#o", 0660);
	return 0;
}

static struct class *devclass;

static void pu32gpio_exit (void) {

	pu32gpio_devdata_t *devdata, *n;

	list_for_each_entry_safe (devdata, n, &pu32gpio_devs, list) {
		if (!MAJOR(devdata->dev))
			break;
		unsigned long irq = devdata->irq;
		if (irq != -1) {
			hwdrvintctrl_ena (irq, 0);
			free_irq (irq, devdata);
		}
		unsigned j;
		unsigned cdevcnt = (devdata->hwdev.iocnt + 1);
		for (j = 0; j < cdevcnt; ++j)
			device_destroy (devclass, MKDEV(MAJOR(devdata->dev), j));
		cdev_del (&devdata->cdev);
		unregister_chrdev_region (devdata->dev, MAX_PERDEV_IOCNT);
		kfree(devdata);
	}

	class_destroy (devclass);
}

extern unsigned long pu32_ishw;

#include <hwdrvdevtbl.h>
static hwdrvdevtbl hwdrvdevtbl_dev = {
	.e = (devtblentry *)0,
	.id = 6 /* GPIO device */ };

static int __init pu32gpio_init (void) {
	if (pu32gpio_param_donotuse || !pu32_ishw)
		return -EIO;
	devclass = class_create (THIS_MODULE, "pu32gpio");
	devclass->dev_uevent = pu32gpio_uevent;
	unsigned i, j;
	for (i = 0;; ++i) {
		hwdrvdevtbl_find (&hwdrvdevtbl_dev, NULL);
		if (!hwdrvdevtbl_dev.mapsz)
			break;
		pu32gpio_devdata_t *devdata = kzalloc(sizeof(pu32gpio_devdata_t), GFP_KERNEL);
		list_add_tail(&devdata->list, &pu32gpio_devs);
		char str[8];
		snprintf (str, sizeof(str), "gpio%c", 'A'+i);
		if (alloc_chrdev_region(&devdata->dev, 0, MAX_PERDEV_IOCNT, str) < 0)
			goto err;
		devdata->hwdev.addr = hwdrvdevtbl_dev.addr;
		hwdrvgpio_configureio (&devdata->hwdev, 0); // Used to retrieve IO count.
		hwdrvgpio_setdebounce (&devdata->hwdev, -1); // Used to retrieve clock frequency.
		cdev_init (&devdata->cdev, &pu32gpio_fops);
		devdata->cdev.owner = THIS_MODULE;
		unsigned cdevcnt = (devdata->hwdev.iocnt + 1);
		cdev_add (&devdata->cdev, devdata->dev, cdevcnt);
		for (j = 0; j < cdevcnt; ++j)
			device_create (
				devclass,
				NULL,
				MKDEV(MAJOR(devdata->dev), j),
				NULL,
				(j>0) ? "%s%u" : "%s", str, (j-1));
		init_waitqueue_head (&devdata->wq);
		if (hwdrvdevtbl_dev.intridx >= 0) {
			int ret = request_irq (
				hwdrvdevtbl_dev.intridx, pu32gpio_isr,
				IRQF_SHARED, "pu32gpio", devdata);
			if (ret) {
				pr_err("request_irq(%lu) == %d\n", hwdrvdevtbl_dev.intridx, ret);
				devdata->irq = -1;
			} else
				devdata->irq = hwdrvdevtbl_dev.intridx;
		} else
			devdata->irq = -1;
		pr_info("%s[%lu] @0x%lx irq(%d) freq(%lu)\n",
			str,
			devdata->hwdev.iocnt,
			(unsigned long)devdata->hwdev.addr,
			devdata->irq,
			devdata->hwdev.clkfreq);
	}
	if (!i)
		goto err;
	return 0;
	err:
	pu32gpio_exit();
	return -EIO;
}

typedef struct {
	unsigned long ival;
	bool use_bin;
	bool use_wq;
	unsigned long mask;
} pu32gpio_filedata_t;

static int pu32gpio_open (struct inode *inode, struct file *file) {

	pu32gpio_filedata_t *filedata = kzalloc(sizeof(pu32gpio_filedata_t), GFP_KERNEL);

	file->private_data = filedata;

	pu32gpio_devdata_t *devdata = container_of(inode->i_cdev, pu32gpio_devdata_t, cdev);

	filedata->use_bin = pu32gpio_param_use_bin;
	if (file->f_flags & O_NONBLOCK)
		filedata->use_wq = 0;
	else
		filedata->use_wq = (devdata->irq != -1 && pu32gpio_param_use_wq);
	unsigned long minor = MINOR(file->f_path.dentry->d_inode->i_rdev);
	unsigned long idx = (minor - 1);

	unsigned long mask;
	if (minor)
		mask = (1<<idx);
	else
		mask = pu32gpio_param_mask;
	filedata->mask = mask;

	unsigned long iodir = ((pu32gpio_param_iodir & mask) | (pu32gpio_param_iodir & ~mask));
	hwdrvgpio_configureio (&devdata->hwdev, iodir);

	hwdrvgpio_setdebounce (&devdata->hwdev, (devdata->hwdev.clkfreq / pu32gpio_param_dbncr_hz));

	if (filedata->use_wq) {
		unsigned long irq = devdata->irq;
		if (irq != -1)
			hwdrvintctrl_ena (irq, 1);
	}

	atomic_long_inc(&devdata->in_use);

	return 0;
}

static int pu32gpio_release (struct inode *inode, struct file *file) {

	pu32gpio_filedata_t *filedata = file->private_data;

	pu32gpio_devdata_t *devdata = container_of(inode->i_cdev, pu32gpio_devdata_t, cdev);

	if (filedata->use_wq && (atomic_long_dec_if_positive(&devdata->in_use) < 1)) {
		unsigned long irq = devdata->irq;
		if (irq != -1)
			hwdrvintctrl_ena (irq, 0);
	}

	kfree(filedata);

	return 0;
}

static long pu32gpio_ioctl (struct file *file, unsigned int cmd, unsigned long arg) {

	pu32gpio_devdata_t *devdata;

	pu32gpio_filedata_t *filedata = file->private_data;

	unsigned long minor = MINOR(file->f_path.dentry->d_inode->i_rdev);
	//unsigned long idx = (minor - 1);

	int ret;
	switch (cmd) {
		case GPIO_IOCTL_USE_BIN:
			filedata->use_bin = (!!arg);
			ret = 0;
			break;
		case GPIO_IOCTL_USE_WQ:
			filedata->use_wq = (!!arg);
			ret = 0;
			break;
		case GPIO_IOCTL_MASK:
			if (minor)
				return -EPERM;
			filedata->mask = arg;
			ret = 0;
			break;
		case GPIO_IOCTL_IODIR:
			if (minor) {
				if (arg > 1)
					return -EINVAL;
				arg <<= (minor-1);
			}
			unsigned long mask = filedata->mask;
			devdata = container_of(file_inode(file)->i_cdev, pu32gpio_devdata_t, cdev);
			hwdrvgpio_configureio (&devdata->hwdev, ((arg & mask) | (arg & ~mask)));
			ret = 0;
			break;
		case GPIO_IOCTL_DBNCR_HZ:
			devdata = container_of(file_inode(file)->i_cdev, pu32gpio_devdata_t, cdev);
			hwdrvgpio_setdebounce (&devdata->hwdev, (devdata->hwdev.clkfreq / arg));
			ret = 0;
			break;
		case GPIO_IOCTL_IOCNT:
			devdata = container_of(file_inode(file)->i_cdev, pu32gpio_devdata_t, cdev);
			ret = devdata->hwdev.iocnt;
			break;
		default:
			ret = -ENOIOCTLCMD;
			break;
	}
	return ret;
}

#define GPIODATASZ (2/*0x*/+((sizeof(unsigned long)*8)/4)/*hex-digits*/+1/*null*/)

static ssize_t pu32gpio_read (struct file *file, char __user *buf, size_t count, loff_t *offset) {

	pu32gpio_filedata_t *filedata = file->private_data;

	pu32gpio_devdata_t *devdata = container_of(file_inode(file)->i_cdev, pu32gpio_devdata_t, cdev);

	unsigned long val = filedata->ival;

	unsigned long minor = MINOR(file->f_path.dentry->d_inode->i_rdev);
	unsigned long idx = (minor - 1);

	if (filedata->use_wq) {
		int ret = wait_event_interruptible (
			devdata->wq,
			(((filedata->ival = *(volatile unsigned long *)devdata->hwdev.addr) ^ val) & filedata->mask));
		if (ret)
			return ret;
		*offset = 0;
	} else
		filedata->ival = *(volatile unsigned long *)devdata->hwdev.addr;

	val = (filedata->ival & filedata->mask);

	unsigned long datalen;

	char data[GPIODATASZ];

	unsigned long use_bin = filedata->use_bin;

	if (minor) {
		data[0] = ((use_bin?0:'0') + ((val>>idx)&1));
		if (use_bin)
			datalen = 1;
		else {
			data[1] = 0;
			datalen = 2;
		}
	} else if (use_bin) {
		*(unsigned long *)data = val;
		datalen = sizeof(unsigned long);
	} else {
		datalen = snprintf (data, sizeof(data), "0x%lx", val);
		data[datalen] = 0;
	}

	if (*offset <= datalen)
		datalen -= *offset;
	else
		datalen = 0;

	if (count > datalen)
		count = datalen;
	if (copy_to_user(buf, &data[*offset], count))
		return -EFAULT;

	*offset += count;

	return count;
}

static ssize_t pu32gpio_write (struct file *file, const char __user *buf, size_t count, loff_t *offset) {

	pu32gpio_filedata_t *filedata = file->private_data;

	unsigned long use_bin = filedata->use_bin;

	unsigned long minor = MINOR(file->f_path.dentry->d_inode->i_rdev);
	unsigned long idx = (minor - 1);

	unsigned long datalen;
	if (minor)
		datalen = (use_bin?1:2);
	else if (use_bin)
		datalen = sizeof(unsigned long);
	else
		datalen = GPIODATASZ;

	if (count > datalen)
		count = datalen;

	char data[GPIODATASZ];
	if (copy_from_user(data, buf, count))
		return -EFAULT;

	unsigned long val;
	if (use_bin)
		val = *(unsigned long *)data;
	else {
		data[count] = 0;
		if ((kstrtol (data, 0, &val)) || (minor && (val > 1)))
			return -EINVAL;
	}

	pu32gpio_devdata_t *devdata = container_of(file_inode(file)->i_cdev, pu32gpio_devdata_t, cdev);

	unsigned long oldval = devdata->oval;

	if (minor) {
		val = ((val&1)<<idx);
		val = (val | (oldval & ~(1<<idx)));
	} else {
		unsigned long mask = filedata->mask;
		val = ((val & mask) | (oldval & ~mask));
	}

	devdata->oval = val;

	*(volatile unsigned long *)devdata->hwdev.addr = val;

	return count;
}

static loff_t pu32gpio_llseek (struct file *file, loff_t offset, int whence) {
	pu32gpio_filedata_t *filedata = file->private_data;
	unsigned long use_bin = filedata->use_bin;
	unsigned long minor = MINOR(file->f_path.dentry->d_inode->i_rdev);
	unsigned long datalen;
	if (minor)
		datalen = (use_bin?1:2);
	else if (use_bin)
		datalen = sizeof(unsigned long);
	else
		datalen = GPIODATASZ;
	loff_t newpos;
	switch (whence) {
		case SEEK_SET:
			newpos = offset;
			break;
		case SEEK_CUR:
			newpos = (file->f_pos + offset);
			break;
		case SEEK_END:
			newpos = (datalen + offset);
			break;
	}
	if (newpos < 0 || newpos > datalen)
		return -EINVAL;
	file->f_pos = newpos;
	return newpos;
}

static __poll_t pu32gpio_poll (struct file *file, struct poll_table_struct *wait) {

	pu32gpio_filedata_t *filedata = file->private_data;

	pu32gpio_devdata_t *devdata = container_of(file_inode(file)->i_cdev, pu32gpio_devdata_t, cdev);

	if (filedata->use_wq)
		poll_wait (file, &devdata->wq, wait);

	unsigned long val = filedata->ival;
	if (((filedata->ival = *(volatile unsigned long *)devdata->hwdev.addr) ^ val) & filedata->mask) {
		file->f_pos = 0;
		return (EPOLLIN | EPOLLRDNORM);
	}
	return 0;
}

module_init(pu32gpio_init);
module_exit(pu32gpio_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("William Fonkou Tambe");
MODULE_DESCRIPTION("PU32 GPIO Device Driver");
