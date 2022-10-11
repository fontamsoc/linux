// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/blkdev.h>
#include <linux/blk-mq.h>
#include <linux/hdreg.h>
#include <linux/wait.h>
#include <linux/interrupt.h>

#include <pu32.h>

#include <hwdrvblkdev.h>
static hwdrvblkdev hwdrvblkdev_dev;

#include <hwdrvintctrl.h>

static long pu32hdd_param_hw_en = 0;
static long pu32hdd_param_irq_en = 0;

static unsigned long pu32hdd_ishw = 0;
static unsigned long pu32hdd_irq = -1;

static int pu32hdd_param_hw_en_ops_set (const char *val, const struct kernel_param *kp) {
	if (pu32hdd_ishw != 0)
		return param_set_int(val, kp);
	return 0;
}
static int pu32hdd_param_hw_en_ops_get (char *buf, const struct kernel_param *kp) {
	return param_get_int(buf, kp);
}
static const struct kernel_param_ops pu32hdd_param_hw_en_ops = {
        .set = pu32hdd_param_hw_en_ops_set,
        .get = pu32hdd_param_hw_en_ops_get
};

static int pu32hdd_param_irq_en_ops_set (const char *val, const struct kernel_param *kp) {
	if (pu32hdd_irq != -1)
		return param_set_int(val, kp);
	return 0;
}
static int pu32hdd_param_irq_en_ops_get (char *buf, const struct kernel_param *kp) {
	return param_get_int(buf, kp);
}
static const struct kernel_param_ops pu32hdd_param_irq_en_ops = {
        .set = pu32hdd_param_irq_en_ops_set,
        .get = pu32hdd_param_irq_en_ops_get
};

module_param_cb(hw_en, &pu32hdd_param_hw_en_ops, &pu32hdd_param_hw_en, 0644);
MODULE_PARM_DESC(hw_en, "use hw instead of bios");

module_param_cb(irq_en, &pu32hdd_param_irq_en_ops, &pu32hdd_param_irq_en, 0644);
MODULE_PARM_DESC(irq_en, "enable interrupt");

static long pu32hdd_param_usebios = 0;
static int __init pu32hdd_param_usebios_fn (char *buf) {
	if (buf)
		pu32hdd_param_usebios =
			(buf[0] == '1' && !buf[1]);
	return 0;
}
early_param ("pu32hdd_usebios", pu32hdd_param_usebios_fn);

extern unsigned long pu32_ishw;

// PU32_BIOS_FD_STORAGEDEV block size in bytes.
#define BLKSZ 512

// We can tweak our hardware sector size, but the kernel
// talks to us in terms of small sectors, always.
#if SECTOR_SIZE < BLKSZ
#error SECTOR_SIZE less than PU32_BIOS_FD_STORAGEDEV BLKSZ
#endif

// Internal representation of device.
static struct pu32hdd_device {
	unsigned long capacity;
	struct gendisk *gd;
	struct blk_mq_tag_set tag_set;
	spinlock_t lock;
} pu32hdd_dev;

static DECLARE_WAIT_QUEUE_HEAD(pu32hdd_wq);

volatile unsigned long hwdrvblkdev_rqst = 0;

static void hwdrvblkdev_isbsy_ (void) {
	unsigned long en = (pu32hdd_param_irq_en && preemptible() && !rcu_preempt_depth());
	if (pu32hdd_irq != -1)
		hwdrvintctrl_ena (pu32hdd_irq, en);
	if (en)
		wait_event_interruptible(pu32hdd_wq, (hwdrvblkdev_rqst == 0));
	else
		hwdrvblkdev_rqst = 0;
}

static irqreturn_t pu32hdd_isr (int _, void *__) {
	if (hwdrvblkdev_rqst) {
		hwdrvblkdev_rqst = 0;
		wake_up_interruptible(&pu32hdd_wq);
	}
	return IRQ_HANDLED;
}

static blk_status_t pu32hdd_do_request (struct request *rq) {
	struct pu32hdd_device *dev = rq->q->queuedata;
	loff_t devsz = (loff_t)(dev->capacity * SECTOR_SIZE);
	loff_t pos = (blk_rq_pos(rq) * SECTOR_SIZE);
	unsigned long bufsz = blk_rq_cur_bytes(rq);
	void* buf = bio_data(rq->bio);
	if ((pos + bufsz) > devsz)
		bufsz = (unsigned long)(devsz - pos);
	if (pu32hdd_irq != -1)
		hwdrvintctrl_ena (pu32hdd_irq, (pu32hdd_param_hw_en && pu32hdd_param_irq_en));
	unsigned long i = 0;
	if (pu32hdd_param_hw_en) {
		signed long ret;
		if (rq_data_dir(rq) == WRITE) {
			while (i < bufsz) {
				do {
					if ((ret = hwdrvblkdev_isrdy (&hwdrvblkdev_dev)) == -1)
						return BLK_STS_IOERR;
				} while (ret == 0);
				hwdrvblkdev_rqst = 1;
				hwdrvblkdev_write (&hwdrvblkdev_dev, buf+i, (pos+i)/BLKSZ, ((i + BLKSZ) < bufsz));
				i += BLKSZ;
			}
		} else {
			while (i < bufsz) {
				do {
					do {
						if ((ret = hwdrvblkdev_isrdy (&hwdrvblkdev_dev)) == -1)
							return BLK_STS_IOERR;
					} while (ret == 0);
					hwdrvblkdev_rqst = 1;
				} while (!hwdrvblkdev_read (&hwdrvblkdev_dev, buf+i, (pos+i)/BLKSZ, ((i + BLKSZ) < bufsz)));
				i += BLKSZ;
			}
		}
	} else {
		if (((loff_t)pu32syslseek (PU32_BIOS_FD_STORAGEDEV, pos/BLKSZ, SEEK_SET) * BLKSZ) != pos)
			return BLK_STS_IOERR;
		if (rq_data_dir(rq) == WRITE) {
			while (i < bufsz)
				i += ((loff_t)pu32syswrite (PU32_BIOS_FD_STORAGEDEV, buf+i, (bufsz-i)/BLKSZ) * BLKSZ);
		} else {
			while (i < bufsz)
				i += ((loff_t)pu32sysread (PU32_BIOS_FD_STORAGEDEV, buf+i, (bufsz-i)/BLKSZ) * BLKSZ);
		}
	}
	return BLK_STS_OK;
}

// Queue callback function.
static blk_status_t pu32hdd_queue_rq (
	struct blk_mq_hw_ctx *hctx,
	const struct blk_mq_queue_data* bd) {
	struct request *rq = bd->rq;
	struct pu32hdd_device *dev = rq->q->queuedata;
	if (!spin_trylock (&dev->lock))
		return BLK_STS_DEV_RESOURCE;
	blk_status_t status = BLK_STS_OK;
	blk_mq_start_request(rq);
	do {
		status = pu32hdd_do_request(rq);
	} while (blk_update_request(rq, status, blk_rq_cur_bytes(rq)));
	__blk_mq_end_request (rq, status);
	spin_unlock (&dev->lock);
	return status;
}

static struct blk_mq_ops pu32hdd_mq_ops = {
	.queue_rq = pu32hdd_queue_rq,
};

// Device operations structure.
static struct block_device_operations pu32hdd_ops = {
	.owner = THIS_MODULE,
};

#define BLKDEVADDR (0x0 /* By convention, the first block device is located at 0x0 */)
#define BLKDEVINTR (0 /* By convention, intr 0 is reserved for the first block device */)

#include <hwdrvdevtbl.h>
static hwdrvdevtbl hwdrvdevtbl_dev = {
	.e = (devtblentry *)0, .id = 4 /* Block device */,
	.mapsz = 128, .addr = BLKDEVADDR, .intridx = BLKDEVINTR };

void pu32hdd_irq_free (void) {
	hwdrvintctrl_ena (pu32hdd_irq, 0);
	free_irq (pu32hdd_irq, &pu32hdd_dev);
	pu32hdd_irq = -1;
}

unsigned long hwdrvblkdev_init_ (void) {
	//hwdrvdevtbl_find (&hwdrvdevtbl_dev, NULL); // Not needed since using conventional BLKDEVADDR BLKDEVINTR to match BIOS.
	if (!hwdrvdevtbl_dev.mapsz)
		goto out;
	pu32hdd_ishw = 1;
	pu32hdd_param_hw_en = 1;
	if (hwdrvdevtbl_dev.intridx >= 0) {
		int ret = request_irq (
			hwdrvdevtbl_dev.intridx, pu32hdd_isr,
			IRQF_SHARED, "pu32hdd", &((unsigned){0}));
		if (ret)
			pr_err("request_irq(%lu) == %d\n", hwdrvdevtbl_dev.intridx, ret);
		else {
			hwdrvblkdev_isbsy = hwdrvblkdev_isbsy_;
			hwdrvintctrl_ena (hwdrvdevtbl_dev.intridx, 1);
			pu32hdd_irq = hwdrvdevtbl_dev.intridx;
		}
	}
	hwdrvblkdev_dev.addr = hwdrvdevtbl_dev.addr;
	hwdrvblkdev_rqst = 1;
	if (!hwdrvblkdev_init (&hwdrvblkdev_dev, 0))
		goto out_free_irq;
	pu32hdd_param_irq_en = (pu32hdd_irq != -1);
	return hwdrvblkdev_dev.blkcnt;
	out_free_irq:
	if (pu32hdd_irq != -1)
		pu32hdd_irq_free();
	out:
	return 0;
}

static int __init pu32hdd_init (void) {
	if (pu32hdd_param_usebios || !pu32_ishw || !(pu32hdd_dev.capacity = hwdrvblkdev_init_())) {
		pu32hdd_dev.capacity = // Fallback to using BIOS.
			pu32syslseek (PU32_BIOS_FD_STORAGEDEV, 0, SEEK_END);
	}
	if (!pu32hdd_dev.capacity)
		goto out;
	pu32hdd_dev.capacity = // Device capacity in SECTOR_SIZE bytes.
		((pu32hdd_dev.capacity * BLKSZ) / SECTOR_SIZE);
	spin_lock_init(&pu32hdd_dev.lock);
	int major_num = register_blkdev(0, "hd");
	if (major_num <= 0) {
		printk(KERN_WARNING "pu32hdd: unable to get major number\n");
		goto out;
	}
	pu32hdd_dev.tag_set.ops = &pu32hdd_mq_ops;
	pu32hdd_dev.tag_set.nr_hw_queues = 1;
	pu32hdd_dev.tag_set.nr_maps = 1;
	pu32hdd_dev.tag_set.queue_depth = 128;
	pu32hdd_dev.tag_set.numa_node = NUMA_NO_NODE;
	pu32hdd_dev.tag_set.flags = BLK_MQ_F_SHOULD_MERGE;
	if (blk_mq_alloc_tag_set(&pu32hdd_dev.tag_set))
		goto out_unregister_blkdev;
	pu32hdd_dev.gd = blk_mq_alloc_disk(&pu32hdd_dev.tag_set, &pu32hdd_dev);
	if (IS_ERR(pu32hdd_dev.gd))
		goto out_unregister_blkdev;
	pu32hdd_dev.gd->major = major_num;
	pu32hdd_dev.gd->first_minor = 0;
	pu32hdd_dev.gd->minors = 128;
	pu32hdd_dev.gd->fops = &pu32hdd_ops;
	pu32hdd_dev.gd->private_data = &pu32hdd_dev;
	strcpy (pu32hdd_dev.gd->disk_name, "hda");
	blk_queue_logical_block_size (pu32hdd_dev.gd->queue, SECTOR_SIZE);
	blk_queue_physical_block_size (pu32hdd_dev.gd->queue, SECTOR_SIZE);
	set_capacity(pu32hdd_dev.gd, pu32hdd_dev.capacity);
	add_disk(pu32hdd_dev.gd);
	return 0;
	out_unregister_blkdev:
	unregister_blkdev (major_num, "hd");
	out:
	return -EIO;
}

static void __exit pu32hdd_exit (void) {
	if (pu32hdd_irq != -1)
		pu32hdd_irq_free();
	int major_num = pu32hdd_dev.gd->major;
	del_gendisk(pu32hdd_dev.gd);
	put_disk(pu32hdd_dev.gd);
	unregister_blkdev (major_num, "hd");
}

module_init(pu32hdd_init);
module_exit(pu32hdd_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("William Fonkou Tambe");
MODULE_DESCRIPTION("PU32 HDD Block Device Driver");
