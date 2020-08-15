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

#define MAX_DEV_CNT 4 /* Maximum number of GPIO devices to detect */
#define MAX_PERDEV_IOCNT ((sizeof(unsigned long)*8) /* Exact count is (ARCHBITSZ-1), but we need a char device that represent all the IOs of the device */)

#define DBNCR_HZ (10 /* Debounce for 100ms */)

typedef struct {
	dev_t dev;
	struct class *class;
	struct cdev cdev;
	unsigned long iocnt;
	unsigned long ival; // Bitfield: 1bit per IO.
	unsigned long oval; // Bitfield: 1bit per IO.
	unsigned long use_bin; // Bitfield: 1bit per chardev.
	unsigned long use_wq; // Bitfield: 1bit per chardev.
	unsigned long polled; // Bitfield: 1bit per chardev.
	wait_queue_head_t wq;
	unsigned long irq;
	hwdrvgpio hwdev;
} pu32gpio_devdata_t;

static pu32gpio_devdata_t pu32gpio_devdata[MAX_DEV_CNT];

static long pu32gpio_param_use_bin = 0;
static long pu32gpio_param_use_wq = 0;

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

module_param_cb(IOCTL_USE_BIN, &pu32gpio_param_use_bin_ops, &pu32gpio_param_use_bin, 0644);
MODULE_PARM_DESC(IOCTL_USE_BIN, "use binary instead of text");

module_param_cb(IOCTL_USE_WQ, &pu32gpio_param_use_wq_ops, &pu32gpio_param_use_wq, 0644);
MODULE_PARM_DESC(IOCTL_USE_WQ, "use interrupt instead of polling");

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
	unsigned i, j;
	for (i = 0; i < MAX_DEV_CNT; ++i) {
		if (!MAJOR(pu32gpio_devdata[i].dev))
			break;
		unsigned long irq = pu32gpio_devdata[i].irq;
		if (irq != -1) {
			hwdrvintctrl_ena (irq, 0);
			free_irq (irq, &pu32gpio_devdata[i]);
		}
		unsigned cdevcnt = (pu32gpio_devdata[i].iocnt + 1);
		for (j = 0; j < cdevcnt; ++j)
			device_destroy (devclass, MKDEV(MAJOR(pu32gpio_devdata[i].dev), j));
		cdev_del (&pu32gpio_devdata[i].cdev);
		unregister_chrdev_region (pu32gpio_devdata[i].dev, MAX_PERDEV_IOCNT);
	}
	class_destroy (devclass);
}

extern unsigned long pu32_ishw;

#include <hwdrvdevtbl.h>
static hwdrvdevtbl hwdrvdevtbl_dev = {
	.e = (devtblentry *)0, .id = 6 /* GPIO device */ };

static int __init pu32gpio_init (void) {
	if (pu32gpio_param_donotuse || !pu32_ishw)
		return -EIO;
	devclass = class_create (THIS_MODULE, "pu32gpio");
	devclass->dev_uevent = pu32gpio_uevent;
	unsigned i, j;
	for (i = 0; i < MAX_DEV_CNT; ++i) {
		hwdrvdevtbl_find (&hwdrvdevtbl_dev, NULL);
		if (!hwdrvdevtbl_dev.mapsz)
			break;
		char str[16];
		snprintf (str, sizeof(str), "gpio%c", 'a'+i);
		if (alloc_chrdev_region(&pu32gpio_devdata[i].dev, 0, MAX_PERDEV_IOCNT, str) < 0)
			goto err;
		pu32gpio_devdata[i].hwdev.addr = hwdrvdevtbl_dev.addr;
		hwdrvgpio_configureio (&pu32gpio_devdata[i].hwdev, 0);
		hwdrvgpio_setdebounce (&pu32gpio_devdata[i].hwdev, -1);
		hwdrvgpio_setdebounce (&pu32gpio_devdata[i].hwdev,
			(pu32gpio_devdata[i].hwdev.clkfreq / DBNCR_HZ));
		pu32gpio_devdata[i].iocnt = pu32gpio_devdata[i].hwdev.iocnt;
		cdev_init (&pu32gpio_devdata[i].cdev, &pu32gpio_fops);
		pu32gpio_devdata[i].cdev.owner = THIS_MODULE;
		unsigned cdevcnt = (pu32gpio_devdata[i].iocnt + 1);
		cdev_add (&pu32gpio_devdata[i].cdev, pu32gpio_devdata[i].dev, cdevcnt);
		for (j = 0; j < cdevcnt; ++j)
			device_create (
				devclass,
				NULL,
				MKDEV(MAJOR(pu32gpio_devdata[i].dev), j),
				NULL,
				(j>0) ? "%s%u" : "%s", str, (j-1));
		init_waitqueue_head (&pu32gpio_devdata[i].wq);
		if (hwdrvdevtbl_dev.intridx >= 0) {
			int ret = request_irq (
				hwdrvdevtbl_dev.intridx, pu32gpio_isr,
				IRQF_SHARED, "pu32gpio", &pu32gpio_devdata[i]);
			if (ret) {
				pr_err("request_irq(%lu) == %d\n", hwdrvdevtbl_dev.intridx, ret);
				pu32gpio_devdata[i].irq = -1;
			} else
				pu32gpio_devdata[i].irq = hwdrvdevtbl_dev.intridx;
		} else
			pu32gpio_devdata[i].irq = -1;
		++hwdrvdevtbl_dev.e;
	}
	if (!i)
		goto err;
	return 0;
	err:
	pu32gpio_exit();
	return -EIO;
}

static int pu32gpio_open (struct inode *inode, struct file *file) {
	pu32gpio_devdata_t *devdata = container_of(inode->i_cdev, pu32gpio_devdata_t, cdev);
	unsigned long minor = MINOR(file->f_path.dentry->d_inode->i_rdev);
	devdata->use_bin |= (pu32gpio_param_use_bin<<minor);
	if (devdata->use_wq |= ((devdata->irq != -1 && pu32gpio_param_use_wq)<<minor)) {
		unsigned long irq = devdata->irq;
		if (irq != -1)
			hwdrvintctrl_ena (irq, 1);
	}
	pu32gpio_param_use_bin = 0;
	pu32gpio_param_use_wq = 0;
	return 0;
}

static int pu32gpio_release (struct inode *inode, struct file *file) {
	pu32gpio_devdata_t *devdata = container_of(inode->i_cdev, pu32gpio_devdata_t, cdev);
	unsigned long minor = MINOR(file->f_path.dentry->d_inode->i_rdev);
	devdata->use_bin &= ~(1<<minor);
	if (!(devdata->use_wq &= ~(1<<minor))) {
		unsigned long irq = devdata->irq;
		if (irq != -1)
			hwdrvintctrl_ena (irq, 0);
	}
	devdata->polled &= ~(1<<minor);
	return 0;
}

static long pu32gpio_ioctl (struct file *file, unsigned int cmd, unsigned long arg) {
	int ret;
	pu32gpio_devdata_t *devdata = container_of(file_inode(file)->i_cdev, pu32gpio_devdata_t, cdev);
	unsigned long minor = MINOR(file->f_path.dentry->d_inode->i_rdev);
	switch (cmd) {
		case GPIO_IOCTL_USE_BIN:
			devdata->use_bin |= ((!!arg)<<minor);
			ret = 0;
			break;
		case GPIO_IOCTL_USE_WQ:
			devdata->use_wq |= ((!!arg)<<minor);
			ret = 0;
			break;
		default:
			ret = -ENOIOCTLCMD;
			break;
	}
	return ret;
}

#define GPIODATASZ (2/*0x*/+((sizeof(unsigned long)*8)/4)/*hex-digits*/+1/*null*/)

static ssize_t pu32gpio_read (struct file *file, char __user *buf, size_t count, loff_t *offset) {
	pu32gpio_devdata_t *devdata = container_of(file_inode(file)->i_cdev, pu32gpio_devdata_t, cdev);
	unsigned long minor = MINOR(file->f_path.dentry->d_inode->i_rdev);
	unsigned long idx, use_bin;
	if (minor) {
		idx = (minor-1);
		use_bin = ((devdata->use_bin>>minor)&1);
	}
	char data[GPIODATASZ];
	if (!*offset && !((devdata->polled>>minor)&1))
		devdata->ival = *(volatile unsigned long *)devdata->hwdev.addr;
	devdata->polled &= ~(1<<minor);
	used_wq:;
	unsigned long val = devdata->ival;
	unsigned long datalen;
	if (minor) {
		data[0] = ((use_bin?0:'0') + ((val>>idx)&1));
		if (use_bin)
			datalen = 1;
		else {
			data[1] = 0;
			datalen = 2;
		}
	} else if (devdata->use_bin&1) {
		*(unsigned long *)data = val;
		datalen = sizeof(unsigned long);
	} else {
		datalen = snprintf (data, sizeof(data), "0x%lx", val);
		data[datalen] = 0;
	}
	if (*offset >= datalen) {
		if ((devdata->use_wq>>minor)&1) {
			int ret = wait_event_interruptible (devdata->wq, (
				minor ?
					((((devdata->ival = *(volatile unsigned long *)devdata->hwdev.addr)>>idx)&1) != ((val>>idx)&1)) :
					((devdata->ival = *(volatile unsigned long *)devdata->hwdev.addr) != val)));
			if (ret)
				return ret;
			*offset = 0;
			goto used_wq;
		}
		return 0;
	}
	datalen -= *offset;
	if (count > datalen)
		count = datalen;
	if (copy_to_user(buf, &data[*offset], count))
		return -EFAULT;
	*offset += count;
	return count;
}

static ssize_t pu32gpio_write (struct file *file, const char __user *buf, size_t count, loff_t *offset) {
	pu32gpio_devdata_t *devdata = container_of(file_inode(file)->i_cdev, pu32gpio_devdata_t, cdev);
	unsigned long minor = MINOR(file->f_path.dentry->d_inode->i_rdev);
	unsigned long use_bin = ((devdata->use_bin>>minor)&1);
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
	if (minor) {
		unsigned long idx = (minor-1);
		unsigned long mask = (1<<idx);
		unsigned long newval = ((!!val)<<idx);
		unsigned long oldval = devdata->oval;
		val = (newval | (oldval & ~mask));
	}
	devdata->oval = val;
	*(volatile unsigned long *)devdata->hwdev.addr = val;
	*offset = 0;
	return count;
}

static loff_t pu32gpio_llseek (struct file *file, loff_t offset, int whence) {
	pu32gpio_devdata_t *devdata = container_of(file_inode(file)->i_cdev, pu32gpio_devdata_t, cdev);
	unsigned long minor = MINOR(file->f_path.dentry->d_inode->i_rdev);
	unsigned long use_bin = ((devdata->use_bin>>minor)&1);
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
	pu32gpio_devdata_t *devdata = container_of(file_inode(file)->i_cdev, pu32gpio_devdata_t, cdev);
	unsigned long minor = MINOR(file->f_path.dentry->d_inode->i_rdev);
	if ((devdata->use_wq>>minor)&1)
		poll_wait (file, &devdata->wq, wait);
	unsigned long val = devdata->ival;
	unsigned long idx;
	if (minor ? (idx = (minor-1)),
		((((*(volatile unsigned long *)devdata->hwdev.addr)>>idx)&1) != ((val>>idx)&1)) :
		(*(volatile unsigned long *)devdata->hwdev.addr != val)) {
		devdata->ival = val;
		file->f_pos = 0;
		devdata->polled |= (1<<minor);
		return (EPOLLIN | EPOLLRDNORM);
	}
	return 0;
}

module_init(pu32gpio_init);
module_exit(pu32gpio_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("William Fonkou Tambe");
MODULE_DESCRIPTION("PU32 GPIO Device Driver");
