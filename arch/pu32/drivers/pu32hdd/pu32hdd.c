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

#include <pu32.h>

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
		if (((loff_t)pu32syslseek (PU32_BIOS_FD_STORAGEDEV, pos/BLKSZ, SEEK_SET) * BLKSZ) != pos)
			return -EIO;
		unsigned long i = 0;
		if (rq_data_dir(rq) == WRITE) {
			while (i < bufsz)
				i += ((loff_t)pu32syswrite (PU32_BIOS_FD_STORAGEDEV, buf+i, (bufsz-i)/BLKSZ) * BLKSZ);
		} else {
			while (i < bufsz)
				i += ((loff_t)pu32sysread (PU32_BIOS_FD_STORAGEDEV, buf+i, (bufsz-i)/BLKSZ) * BLKSZ);
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

/* The HDIO_GETGEO ioctl is handled in blkdev_ioctl(),
   which calls this. We need to implement getgeo, otherwise
   we can't use tools such as fdisk to partition the drive. */
static int pu32hdd_getgeo (
	struct block_device *block_device,
	struct hd_geometry *geo) {
	// We have no real geometry, so we make something up.
	geo->cylinders = ((pu32hdd_dev.capacity*(pu32hdd_dev.sectorsz/KERNEL_SECTOR_SIZE))/64);
	geo->heads = 4;
	geo->sectors = 16;
	geo->start = 0;
	return 0;
}

// Device operations structure.
static struct block_device_operations pu32hdd_ops = {
	.owner = THIS_MODULE,
	.getgeo = pu32hdd_getgeo,
};

static int __init pu32hdd_init (void) {
	pu32hdd_dev.capacity = (
		// Device capacity in pu32hdd_dev.sectorsz size.
		(pu32syslseek (
			PU32_BIOS_FD_STORAGEDEV, 0, SEEK_END) * BLKSZ) /
			pu32hdd_dev.sectorsz);
	if (!pu32hdd_dev.capacity)
		goto out;
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
