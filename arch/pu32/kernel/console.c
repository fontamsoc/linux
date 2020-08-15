// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#include <linux/spinlock.h>
#include <linux/console.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/string.h>

#include <pu32.h>

#include <hwdrvchar.h>
static hwdrvchar hwdrvchar_dev;

extern unsigned long pu32_ishw;

static unsigned long pu32_ttys_irq = -1;

static DEFINE_SPINLOCK(tty_console_lock);

static void tty_console_write (
	struct console *con, const char *s, unsigned n) {
	spin_lock(&tty_console_lock);
	unsigned long i;
	if (pu32_ttys_irq != -1 && pu32_ishw) {
		for (i = 0; i < n;)
			i += hwdrvchar_write_ (&hwdrvchar_dev, (void *)s+i, n-i);
	} else {
		for (i = 0; i < n;)
			i += pu32syswrite (PU32_BIOS_FD_STDOUT, (void *)s+i, n-i);
	}
	spin_unlock(&tty_console_lock);
}

static struct tty_driver *tty_console_driver;

static struct tty_driver * tty_console_device (
	struct console *con, int *idx) {
	*idx = con->index;
	return tty_console_driver;
}

static struct console tty_console = {
	.name	= "ttyS",
	.write	= tty_console_write,
	.device = tty_console_device,
	.flags	= CON_PRINTBUFFER,
	.index	= -1
};

static struct tty_port tty_console_port;

static void tty_console_poll (struct timer_list *_);
static DEFINE_TIMER (tty_console_timer, tty_console_poll);
#define TTY_CONSOLE_POLL_DELAY (HZ / 100)

static void tty_console_poll (struct timer_list *_) {
	unsigned char c;
	unsigned long n = 0;
	while (pu32sysread (PU32_BIOS_FD_STDIN, &c, 1)) {
		tty_insert_flip_char (&tty_console_port, c, TTY_NORMAL);
		++n;
	}
	if (n)
		tty_flip_buffer_push(&tty_console_port);
	if (pu32_ttys_irq != -1) {
		if (pu32_ishw)
			hwdrvchar_interrupt (&hwdrvchar_dev, 1); // Resume using interrupt instead.
	} else
		mod_timer (&tty_console_timer, jiffies + TTY_CONSOLE_POLL_DELAY);
}

static irqreturn_t tty_console_isr (int irq, void *_) {
	unsigned char c;
	unsigned long n = 0;
	unsigned long hwchardev_read (void) {
		if (pu32_ttys_irq != -1 && pu32_ishw)
			return hwdrvchar_read (&hwdrvchar_dev, &c, 1);
		else
			return pu32sysread (PU32_BIOS_FD_STDIN, &c, 1);
	}
	while (hwchardev_read()) {
		tty_insert_flip_char (&tty_console_port, c, TTY_NORMAL);
		++n;
	}
	if (n)
		tty_flip_buffer_push(&tty_console_port);
	static unsigned long expires = 0;
	if (pu32_ttys_irq != -1 && jiffies >= expires) {
		expires = (jiffies + TTY_CONSOLE_POLL_DELAY);
		if (pu32_ishw)
			hwdrvchar_interrupt (&hwdrvchar_dev, 1);
	} else { // Preventing too frequent interrupts.
		expires = (jiffies + TTY_CONSOLE_POLL_DELAY);
		mod_timer (&tty_console_timer, expires);
	}
	return IRQ_HANDLED;
}

static int tty_console_ops_open (struct tty_struct *tty, struct file *filp) {
	tty_port_tty_set (tty->port, tty);
	// Flush stdin.
	if (pu32_ttys_irq != -1 && pu32_ishw)
		while (hwdrvchar_read (&hwdrvchar_dev, &((char){0}), 1));
	else
		while (pu32sysread (PU32_BIOS_FD_STDIN, &((char){0}), 1));
	if (pu32_ttys_irq != -1) {
		int ret = request_irq (
			pu32_ttys_irq, tty_console_isr,
			IRQF_SHARED, "ttyS", tty);
		if (ret) {
			pr_err("request_irq(%lu) == %d\n", pu32_ttys_irq, ret);
			tty_port_tty_set (tty->port, NULL);
		} else if (pu32_ishw)
			hwdrvchar_interrupt (&hwdrvchar_dev, 1);
		return ret;
	} else {
		mod_timer (&tty_console_timer, jiffies + TTY_CONSOLE_POLL_DELAY);
		return 0;
	}
}

static void tty_console_ops_close (struct tty_struct *tty, struct file *filp) {
	if (tty->count == 1) {
		if (pu32_ttys_irq != -1)
			free_irq (pu32_ttys_irq, tty);
		else
			del_timer_sync(&tty_console_timer);
		tty_port_tty_set (tty->port, NULL);
	}
}

static int tty_console_ops_write (struct tty_struct *tty, const unsigned char *s, int n) {
	tty_console_write (NULL, s, n);
	return n;
}

static int tty_console_ops_write_room (struct tty_struct *tty) {
	return 32768; // No limit, no buffer used.
}

static int tty_console_chars_in_buffer (struct tty_struct *tty) {
	return 0; // No buffer.
}

static const struct tty_operations tty_console_ops = {
	.open = tty_console_ops_open,
	.close = tty_console_ops_close,
	.write = tty_console_ops_write,
	.write_room = tty_console_ops_write_room,
	.chars_in_buffer = tty_console_chars_in_buffer,
};

#include <hwdrvdevtbl.h>
hwdrvdevtbl hwdrvdevtbl_dev = {.e = (devtblentry *)0, .id = 5 /* Character device */};

void hwchardev_init (void) {
	hwdrvdevtbl_find (&hwdrvdevtbl_dev);
	if ((pu32_ttys_irq = hwdrvdevtbl_dev.intridx) != -1) {
		hwdrvchar_dev.addr = hwdrvdevtbl_dev.addr;
		hwdrvchar_init (&hwdrvchar_dev, 115200);
	}
}

static int __init tty_console_driver_init (void) {

	struct console *tmp;
	console_lock();
	for_each_console(tmp)
		if (tmp == &tty_console)
			break;
	console_unlock();
	if (!tmp)
		register_console(&tty_console);
	tty_console.flags &= ~CON_BOOT;

	tty_console_driver = alloc_tty_driver(1);

	if (!tty_console_driver)
		return -ENOMEM;

	tty_console_driver->driver_name = "pu32tty";
	tty_console_driver->name = "ttyS";
	tty_console_driver->major = TTY_MAJOR;
	tty_console_driver->minor_start = 64;
	tty_console_driver->type = TTY_DRIVER_TYPE_SERIAL;
	tty_console_driver->subtype = SERIAL_TYPE_NORMAL;
	tty_console_driver->init_termios = tty_std_termios;
	tty_console_driver->init_termios.c_cflag |= CLOCAL;
	tty_console_driver->flags = TTY_DRIVER_REAL_RAW;
	tty_set_operations (tty_console_driver, &tty_console_ops);
	tty_port_init(&tty_console_port);
	tty_port_link_device (&tty_console_port, tty_console_driver, 0);

	if (pu32_ishw) {
		if (pu32_ttys_irq == -1)
			hwchardev_init();
	} else
		pu32_ttys_irq = PU32_VM_IRQ_TTYS0;

	int err = tty_register_driver(tty_console_driver);
	if (err) {
		pr_err("unable to register console TTY driver\n");
		tty_port_destroy(&tty_console_port);
		return err;
	}

	return 0;
}
device_initcall(tty_console_driver_init);

#ifdef CONFIG_EARLY_PRINTK
static int __init setup_early_printk (char *buf) {

	if (!buf || early_console)
		return 0;

	if (pu32_ishw)
		hwchardev_init();

	early_console = &tty_console;
	register_console(early_console);

	return 0;
}
early_param ("earlyprintk", setup_early_printk);
#endif
