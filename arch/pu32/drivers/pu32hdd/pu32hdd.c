// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/genhd.h>
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

extern unsigned long pu32_ishw;

// We can tweak our hardware sector size, but the kernel
// talks to us in terms of small sectors, always.
#define KERNEL_SECTOR_SIZE SECTOR_SIZE

// PU32_BIOS_FD_STORAGEDEV block size in bytes.
#define BLKSZ 512

#if KERNEL_SECTOR_SIZE < BLKSZ
#error KERNEL_SECTOR_SIZE less than PU32_BIOS_FD_STORAGEDEV BLKSZ
#endif

// Maximum number of minor numbers that this disk can have.
// Minor numbers correspond to partitions.
#define MAX_MINORS 16

// Internal representation of device.
static struct pu32hdd_device {
	int major_num;
	unsigned long sectorsz;
	unsigned long capacity;
	struct gendisk *gd;
	struct blk_mq_tag_set tag_set;
	struct request_queue *queue;
} pu32hdd_dev = { .major_num = 0, .sectorsz = KERNEL_SECTOR_SIZE /* TODO: A diffent value like 2048 always fail */, };

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

// Serve requests.
static int pu32hdd_do_request (struct request *rq, unsigned int *nr_bytes) {
	struct bio_vec bvec;
	struct req_iterator iter;
	struct pu32hdd_device *dev = rq->q->queuedata;
	loff_t pos = (blk_rq_pos(rq) * dev->sectorsz /* TODO: KERNEL_SECTOR_SIZE seem to be always wanted here */);
	loff_t devsz = (loff_t)(dev->capacity * dev->sectorsz);
	// Iterate over all requests segments.
	rq_for_each_segment (bvec, rq, iter) {
		unsigned long bufsz = bvec.bv_len;
		void* buf = (page_address(bvec.bv_page) + bvec.bv_offset);
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
							return -EIO;
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
								return -EIO;
						} while (ret == 0);
						hwdrvblkdev_rqst = 1;
					} while (!hwdrvblkdev_read (&hwdrvblkdev_dev, buf+i, (pos+i)/BLKSZ, ((i + BLKSZ) < bufsz)));
					i += BLKSZ;
				}
			}
		} else {
			if (((loff_t)pu32syslseek (PU32_BIOS_FD_STORAGEDEV, pos/BLKSZ, SEEK_SET) * BLKSZ) != pos)
				return -EIO;
			if (rq_data_dir(rq) == WRITE) {
				while (i < bufsz)
					i += ((loff_t)pu32syswrite (PU32_BIOS_FD_STORAGEDEV, buf+i, (bufsz-i)/BLKSZ) * BLKSZ);
			} else {
				while (i < bufsz)
					i += ((loff_t)pu32sysread (PU32_BIOS_FD_STORAGEDEV, buf+i, (bufsz-i)/BLKSZ) * BLKSZ);
			}
		}
		pos += bufsz;
		*nr_bytes += bufsz;
	}
	return 0;
}

// Queue callback function.
static blk_status_t pu32hdd_queue_rq (
	struct blk_mq_hw_ctx *hctx,
	const struct blk_mq_queue_data* bd) {
	unsigned int nr_bytes = 0;
	blk_status_t status = BLK_STS_OK;
	struct request *rq = bd->rq;
	blk_mq_start_request(rq);
	if (pu32hdd_do_request (rq, &nr_bytes) != 0)
		status = BLK_STS_IOERR;
	if (blk_update_request (rq, status, nr_bytes))
		/* Shouldn't fail */ BUG();
	__blk_mq_end_request (rq, status);
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
	//hwdrvdevtbl_find (&hwdrvdevtbl_dev); // Not needed since using conventional BLKDEVADDR BLKDEVINTR to match BIOS.
	if (!hwdrvdevtbl_dev.mapsz)
		goto out;
	pu32hdd_ishw = 1;
	pu32hdd_param_hw_en = 1;
	if (hwdrvdevtbl_dev.intridx != -1) {
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
	if (!pu32_ishw || !(pu32hdd_dev.capacity = hwdrvblkdev_init_())) {
		pu32hdd_dev.capacity = // Fallback to using BIOS.
			pu32syslseek (PU32_BIOS_FD_STORAGEDEV, 0, SEEK_END);
	}
	if (!pu32hdd_dev.capacity)
		goto out;
	pu32hdd_dev.capacity = // Device capacity in pu32hdd_dev.sectorsz size.
		((pu32hdd_dev.capacity * BLKSZ) / pu32hdd_dev.sectorsz);
	pu32hdd_dev.queue = blk_mq_init_sq_queue (
		&pu32hdd_dev.tag_set, &pu32hdd_mq_ops, 128,
		BLK_MQ_F_SHOULD_MERGE);
	if (pu32hdd_dev.queue == NULL)
		goto out;
	pu32hdd_dev.queue->queuedata = &pu32hdd_dev;
	blk_queue_logical_block_size (pu32hdd_dev.queue, pu32hdd_dev.sectorsz);
	blk_queue_physical_block_size (pu32hdd_dev.queue, pu32hdd_dev.sectorsz);
	pu32hdd_dev.major_num = register_blkdev(pu32hdd_dev.major_num, "hd");
	if (pu32hdd_dev.major_num <= 0) {
		printk(KERN_WARNING "pu32hdd: unable to get major number\n");
		goto out_blk_cleanup_queue;
	}
	pu32hdd_dev.gd = alloc_disk(MAX_MINORS);
	if (!pu32hdd_dev.gd)
		goto out_unregister_blkdev;
	pu32hdd_dev.gd->major = pu32hdd_dev.major_num;
	pu32hdd_dev.gd->first_minor = 0;
	pu32hdd_dev.gd->fops = &pu32hdd_ops;
	pu32hdd_dev.gd->private_data = &pu32hdd_dev;
	strcpy (pu32hdd_dev.gd->disk_name, "hda");
	set_capacity(pu32hdd_dev.gd, pu32hdd_dev.capacity*(pu32hdd_dev.sectorsz/KERNEL_SECTOR_SIZE));
	pu32hdd_dev.gd->queue = pu32hdd_dev.queue;
	add_disk(pu32hdd_dev.gd);
	return 0;
	out_unregister_blkdev:
	unregister_blkdev (pu32hdd_dev.major_num, "hd");
	out_blk_cleanup_queue:
	//blk_cleanup_queue(pu32hdd_dev.queue);
	out:
	return -EIO;
}

static void __exit pu32hdd_exit (void) {
	if (pu32hdd_irq != -1)
		pu32hdd_irq_free();
	del_gendisk(pu32hdd_dev.gd);
	put_disk(pu32hdd_dev.gd);
	unregister_blkdev (pu32hdd_dev.major_num, "hd");
	blk_cleanup_queue(pu32hdd_dev.queue);
}

module_init(pu32hdd_init);
module_exit(pu32hdd_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("William Fonkou Tambe");
MODULE_DESCRIPTION("PU32 HDD Block Device Driver");
