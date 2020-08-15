// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#include <linux/errno.h>
#include <linux/fb.h>
#include <linux/hrtimer.h>
#include <linux/wait.h>

static LIST_HEAD(pu32fbdevs);

#define PSEUDO_PALETTE_SIZE 16
struct pu32fbdev_par {
	unsigned long vsrc;
	struct fb_info *info;
	void *hwaddr;
	unsigned long pxalign;
	void *vmem;
	u32 palette[PSEUDO_PALETTE_SIZE];
	atomic_t ref_count;
	struct hrtimer timer;
	ktime_t vsync_interval;
	volatile unsigned long vsync_count;
	wait_queue_head_t wq;
	struct list_head list;
};

static const struct fb_fix_screeninfo pu32fbdev_fix = {
	.id		= "pu32fbdev",
	.type		= FB_TYPE_PACKED_PIXELS,
	.visual		= FB_VISUAL_TRUECOLOR,
	.accel		= FB_ACCEL_NONE,
};

static struct fb_var_screeninfo pu32fbdev_var = {
	.bits_per_pixel = 32,
	.red            = {0, 8},
	.green          = {8, 8},
	.blue           = {16, 8},
	.transp         = {0, 0},
	.height		= -1,
	.width		= -1,
	.activate	= FB_ACTIVATE_NOW,
	.vmode		= FB_VMODE_NONINTERLACED,
};

#include <hwdrvfbdev.h>

static enum hrtimer_restart pu32fbdev_timer_cb (struct hrtimer *timer) {

	struct pu32fbdev_par *par = container_of(timer, struct pu32fbdev_par, timer);

	++par->vsync_count;

	u64 overruns = hrtimer_forward_now (timer, par->vsync_interval);
	if (overruns > 1) {
		//pr_warn("pu32fbdev_timer_cb(): info->node(%u): par->ref_count(%u): overruns(%u)\n",
		//	par->info->node, atomic_read(&par->ref_count), (unsigned)overruns);
	}

	wake_up_interruptible(&par->wq);

	return HRTIMER_RESTART;
}

static int pu32fbdev_open (struct fb_info *info, int user) {

	struct pu32fbdev_par *par = info->par;

	unsigned long ref_count = atomic_read(&par->ref_count);

	//pr_info("pu32fbdev_open(): info->node(%u): par->ref_count(%u)\n",
	//	info->node, atomic_read(&par->ref_count));

	if (!ref_count) {
		memset32 (par->vmem, 0xff000000, info->fix.smem_len/sizeof(uint32_t));
		if (par->vsync_interval) {
			unsigned long vsrc = (info->var.yoffset * info->var.xres_virtual);
			if (par->pxalign)
				vsrc += ((unsigned long)par->vmem >> 2);
			par->vsrc = vsrc;
			hwdrvfbdev_srcset (par->hwaddr, vsrc);
			par->vsync_count = 0;
			hrtimer_start (&par->timer, par->vsync_interval, HRTIMER_MODE_REL_SOFT);
		}
	}

	atomic_inc(&par->ref_count);

	return 0;
}

static int pu32fbdev_release (struct fb_info *info, int user) {

	struct pu32fbdev_par *par = info->par;

	unsigned long ref_count = atomic_read(&par->ref_count);

	//pr_info("pu32fbdev_release(): info->node(%u): par->ref_count(%u)\n",
	//	info->node, atomic_read(&par->ref_count));

	if (!ref_count)
		return -EINVAL;

	if (ref_count == 1) {
		if (par->vsync_interval)
			hrtimer_cancel(&par->timer);
		hwdrvfbdev_srcset (par->hwaddr, -1);
		par->vsrc = 0;
		info->var = pu32fbdev_var;
	}

	atomic_dec(&par->ref_count);

	return 0;
}

static int pu32fbdev_setcolreg (
	unsigned regno, unsigned red, unsigned green,
	unsigned blue, unsigned transp, struct fb_info *info) {

	if (regno >= PSEUDO_PALETTE_SIZE)
		return -EINVAL;

	//u8 cr = red;
	//u8 cg = green;
	//u8 cb = blue;
	// TODO: Not sure why it is done as follow with use of 16:
	u32 cr = red >> (16 - info->var.red.length);
	u32 cg = green >> (16 - info->var.green.length);
	u32 cb = blue >> (16 - info->var.blue.length);

	u32 value =
		(cr << info->var.red.offset) |
		(cg << info->var.green.offset) |
		(cb << info->var.blue.offset);

	u32 *pal = info->pseudo_palette;
	pal[regno] = value;

	return 0;
}

static int pu32fbdev_pan_display (struct fb_var_screeninfo *var, struct fb_info *info) {
	int ret;
	struct pu32fbdev_par *par = info->par;
	if (par->vsync_interval) {
		info->var.yoffset = var->yoffset;
		unsigned long vsrc = (var->yoffset * info->var.xres_virtual);
		if (par->pxalign)
			vsrc += ((unsigned long)par->vmem >> 2);
		//pr_info("pu32fbdev_pan_display(): info->node(%u): par->ref_count(%u): vsync_count(%u); vsrc(0x%x)\n",
		//	par->info->node, atomic_read(&par->ref_count),
		//	(unsigned)par->vsync_count, (unsigned)(vsrc << 2));
		par->vsrc = vsrc;
		hwdrvfbdev_srcset (par->hwaddr, vsrc);
		par->vsync_count = 0;
		ret = wait_event_interruptible(par->wq, (par->vsync_count > 1));
	} else {
		// Nothing needs to be done; however 0 needs to be returned
		// so that info->var->yoffset gets set.
		ret = 0;
	}
	return ret;
}

static int pu32fbdev_ioctl (struct fb_info *info, unsigned int cmd, unsigned long arg) {
	int ret = -EINVAL;
	struct pu32fbdev_par *par = info->par;
	switch (cmd) {
		case FBIO_WAITFORVSYNC:
			if (par->vsync_interval) {
				par->vsync_count = 0;
				ret = wait_event_interruptible(par->wq, (par->vsync_count > 1));
			}
			break;
	}
	return ret;
}

static void pu32fbdev_destroy (struct fb_info *info) {
	framebuffer_release(info);
}

static const struct fb_ops pu32fbdev_ops = {
	.owner          = THIS_MODULE,
	.fb_open        = pu32fbdev_open,
	.fb_release     = pu32fbdev_release,
	//.fb_read
	//.fb_write
	//.fb_check_var
	//.fb_set_par
	.fb_setcolreg   = pu32fbdev_setcolreg,
	//.fb_setcmap
	//.fb_blank
	.fb_pan_display = pu32fbdev_pan_display,
	.fb_fillrect    = cfb_fillrect,
	.fb_copyarea    = cfb_copyarea,
	.fb_imageblit   = cfb_imageblit,
	//.fb_cursor
	//.fb_sync
	.fb_ioctl       = pu32fbdev_ioctl,
	//.fb_compat_ioctl
	//.fb_mmap
	//.fb_get_caps
	.fb_destroy     = pu32fbdev_destroy,
};

static void pu32fbdev_exit (void) {
	struct pu32fbdev_par *par, *n;
	list_for_each_entry_safe (par, n, &pu32fbdevs, list) {
		void* vmem = par->vmem;
		unregister_framebuffer(par->info);
		kfree(vmem);
	}
}

extern unsigned long pu32_ishw;

#include <hwdrvdevtbl.h>
static hwdrvdevtbl hwdrvdevtbl_dev = {
	.e = (devtblentry *)0,
	.id = 10 /* FrameBuffer device */ };

static int __init pu32fbdev_init (void) {

	if (!pu32_ishw)
		return -EIO;

	while (1) {

		hwdrvdevtbl_find (&hwdrvdevtbl_dev, NULL);
		if (!hwdrvdevtbl_dev.mapsz)
			break;

		int ret = -ENODEV;

		unsigned long pxalign = hwdrvfbdev_srcset (hwdrvdevtbl_dev.addr, -1);
		unsigned long width = hwdrvfbdev_getinfo (hwdrvdevtbl_dev.addr, HWDRVFBDEV_GETINFO_WIDTH);
		unsigned long height = hwdrvfbdev_getinfo (hwdrvdevtbl_dev.addr, HWDRVFBDEV_GETINFO_HEIGHT);
		unsigned long freq = hwdrvfbdev_getinfo (hwdrvdevtbl_dev.addr, HWDRVFBDEV_GETINFO_HZ);
		unsigned long bufcnt = hwdrvfbdev_getinfo (hwdrvdevtbl_dev.addr, HWDRVFBDEV_GETINFO_BUFCNT);
		unsigned long accelv = hwdrvfbdev_getinfo (hwdrvdevtbl_dev.addr, HWDRVFBDEV_GETINFO_ACCELV);

		struct fb_info *info = framebuffer_alloc (sizeof(struct pu32fbdev_par), NULL);
		if (!info) {
			pr_err("failed to allocate framebuffer\n");
			ret = -ENOMEM;
			goto err;
		}

		unsigned long vmemsz = (width*height*bufcnt*sizeof(u32));

		struct pu32fbdev_par *par = info->par;

		par->vmem = kmalloc(vmemsz, GFP_KERNEL);
		if (!par->vmem) {
			pr_err("failed to allocate video memory\n");
			ret = -ENOMEM;
			goto err;
		}

		par->info = info;
		par->hwaddr = hwdrvdevtbl_dev.addr;
		par->pxalign = pxalign;

		info->fix = pu32fbdev_fix;
		info->fix.smem_start = (unsigned long)(
			pxalign ? par->vmem : par->hwaddr);
		info->fix.smem_len = vmemsz;
		info->fix.ypanstep = ((bufcnt > 1) ? height : 0);
		info->fix.line_length = (width * sizeof(u32));
		if (accelv)
			info->fix.accel = (FB_ACCEL_FONTAM_FBDEV+(accelv-1));

		pu32fbdev_var.xres = width;
		pu32fbdev_var.yres = height;
		pu32fbdev_var.xres_virtual = width;
		pu32fbdev_var.yres_virtual = (height * bufcnt);
		info->var = pu32fbdev_var;

		info->fbops = &pu32fbdev_ops;
		info->flags = FBINFO_DEFAULT | FBINFO_HIDE_SMEM_START;
		info->screen_base = ioremap_wc(info->fix.smem_start, info->fix.smem_len);
		if (!info->screen_base) {
			ret = -ENOMEM;
			goto err_fb_release;
		}

		info->pseudo_palette = par->palette;

		if (freq) {
			par->vsync_interval = ktime_set(0, 1000000000/freq);
			hrtimer_init(&par->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL_SOFT);
			par->timer.function = pu32fbdev_timer_cb;
		}

		init_waitqueue_head(&par->wq);

		ret = register_framebuffer(info);
		if (ret < 0) {
			pr_err("failed to register framebuffer: %d\n", ret);
			goto err_unmap;
		}

		list_add_tail(&par->list, &pu32fbdevs);

		pr_info("fb%d @0x%lx %lux%lux%lu pxalign(%lu)\n", info->node,
			info->fix.smem_start, width, height, freq, pxalign);

		continue;

		err_unmap:
		iounmap(info->screen_base);
		err_fb_release:
		framebuffer_release(info);
		err:
		pu32fbdev_exit();
		return ret;
	}

	return 0;
}

module_init(pu32fbdev_init);
module_exit(pu32fbdev_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("William Fonkou Tambe");
MODULE_DESCRIPTION("PU32 FrameBuffer Device Driver");
