// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#include <linux/console.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/string.h>
#include <linux/workqueue.h>

#include <pu32.h>
#include <hwdrvdevtbl.h>
#include <hwdrvintctrl.h>
#include <hwdrvchar.h>

static struct workqueue_struct *pu32tty_wq;

#define PU32TTY_CNT_MAX 8
#define PU32TTY_POLL_DELAY (HZ / 100)
#define PU32TTY_ISR_DELAY 0
#define PU32TTY_HWBUFSZ 512 /* used with variable allocated on the stack */

typedef struct {
	struct console console;
	struct tty_port port;
	struct timer_list timer;
	struct work_struct work;
	hwdrvchar hw;
	unsigned long irq;
	unsigned long baudrate;
	struct list_head list;
} pu32tty_dev_t;

static LIST_HEAD(pu32tty_devs);

extern unsigned long pu32_ishw;

static void pu32tty_write (
	struct console *con, const char *s, unsigned n) {
	unsigned long i;
	pu32tty_dev_t *dev = container_of(con, pu32tty_dev_t, console);
	if (pu32_ishw) {
		for (i = 0; i < n;)
			i += hwdrvchar_write (&dev->hw, (void *)s+i, n-i);
	} else {
		for (i = 0; i < n;)
			i += pu32syswrite (PU32_BIOS_FD_STDOUT, (void *)s+i, n-i);
	}
}

static struct tty_driver *pu32tty_driver;

static struct tty_driver * pu32tty_device (
	struct console *con, int *idx) {
	*idx = con->index;
	return pu32tty_driver;
}

static void pu32tty_poll (struct timer_list *timer) {
	unsigned char c[PU32TTY_HWBUFSZ];
	pu32tty_dev_t *dev = container_of(timer, pu32tty_dev_t, timer);
	unsigned long hwchardev_read (void) {
		if (pu32_ishw)
			return hwdrvchar_read (&dev->hw, &c, PU32TTY_HWBUFSZ);
		else
			return pu32sysread (PU32_BIOS_FD_STDIN, &c, 1);
	}
	unsigned long n, nn = 0;
	while ((n = hwchardev_read())) {
		tty_insert_flip_string (&dev->port, (unsigned char *)&c, n);
		nn |= n;
	}
	if (nn)
		tty_flip_buffer_push(&dev->port);
	if (dev->irq != -1) {
		if (pu32_ishw)
			hwdrvchar_interrupt (&dev->hw, 1); // Resume using interrupt instead.
	} else
		mod_timer (&dev->timer, jiffies + PU32TTY_POLL_DELAY);
}

static void pu32tty_work (struct work_struct *work) {
	pu32tty_dev_t *dev = container_of(work, pu32tty_dev_t, work);
	unsigned char c[PU32TTY_HWBUFSZ];
	unsigned long hwchardev_read (void) {
		if (pu32_ishw)
			return hwdrvchar_read (&dev->hw, &c, PU32TTY_HWBUFSZ);
		else
			return pu32sysread (PU32_BIOS_FD_STDIN, &c, 1);
	}
	unsigned long n, nn = 0;
	while ((n = hwchardev_read())) {
		tty_insert_flip_string (&dev->port, (unsigned char *)&c, n);
		nn |= n;
	}
	if (nn)
		tty_flip_buffer_push(&dev->port);
	static unsigned long expires = 0;
	if (dev->irq != -1 && jiffies >= expires) {
		expires = (jiffies + PU32TTY_ISR_DELAY);
		if (pu32_ishw)
			hwdrvchar_interrupt (&dev->hw, 1);
	} else { // Preventing too frequent interrupts.
		expires = (jiffies + PU32TTY_POLL_DELAY);
		mod_timer (&dev->timer, expires);
	}
}

static irqreturn_t pu32tty_isr (int irq, void *dev_id) {
	pu32tty_dev_t *dev = (pu32tty_dev_t *)dev_id;
	queue_work(pu32tty_wq, &dev->work);
	return IRQ_HANDLED;
}

static int pu32tty_tty_ops_open (struct tty_struct *tty, struct file *filp) {
	if (tty->count > 1)
		return 0;
	int ret = tty_port_open(tty->port, tty, filp);
	if (ret)
		return ret;
	pu32tty_dev_t *dev = container_of(tty->port, pu32tty_dev_t, port);
	// Flush stdin.
	if (pu32_ishw)
		while (hwdrvchar_read (&dev->hw, &((char){0}), 1));
	else
		while (pu32sysread (PU32_BIOS_FD_STDIN, &((char){0}), 1));
	tty_encode_baud_rate(tty, dev->baudrate, dev->baudrate);
	if (dev->irq != -1) {
		ret = request_irq (
			dev->irq, pu32tty_isr,
			IRQF_SHARED, "ttyS", dev);
		if (ret) {
			pr_err("request_irq(%lu) == %d\n", dev->irq, ret);
			tty_port_close(tty->port, tty, filp);
		} else if (pu32_ishw) {
			hwdrvchar_interrupt (&dev->hw, 1);
			hwdrvintctrl_ena (dev->irq, 1);
		}
	} else
		mod_timer (&dev->timer, jiffies + PU32TTY_POLL_DELAY);
	return ret;
}

static void pu32tty_tty_ops_close (struct tty_struct *tty, struct file *filp) {
	if (tty->count > 1)
		return;
	pu32tty_dev_t *dev = container_of(tty->port, pu32tty_dev_t, port);
	if (dev->irq != -1) {
		hwdrvintctrl_ena (dev->irq, 0);
		hwdrvchar_interrupt (&dev->hw, 0);
		free_irq (dev->irq, dev);
	} else
		del_timer_sync(&dev->timer);
	tty_port_close(tty->port, tty, filp);
}

static void pu32tty_tty_ops_hangup (struct tty_struct *tty) {
	tty_port_hangup(tty->port);
}

static int pu32tty_tty_ops_write (struct tty_struct *tty, const unsigned char *s, int n) {
	pu32tty_dev_t *dev = container_of(tty->port, pu32tty_dev_t, port);
	pu32tty_write (&dev->console, s, n);
	return n;
}

static unsigned int pu32tty_tty_ops_write_room (struct tty_struct *tty) {
	return 32768; // No limit, no buffer used.
}

static unsigned int pu32tty_chars_in_buffer (struct tty_struct *tty) {
	return 0; // No buffer.
}

static void pu32tty_set_termios (struct tty_struct *tty, struct ktermios * old) {
	pu32tty_dev_t *dev = container_of(tty->port, pu32tty_dev_t, port);
	unsigned long baudrate = tty_termios_baud_rate(&tty->termios);
	if (baudrate && baudrate != dev->baudrate) {
		dev->baudrate = baudrate;
		if (pu32_ishw)
			hwdrvchar_init (&dev->hw, baudrate);
	}
}

static int pu32tty_set_serial (struct tty_struct *tty, struct serial_struct *ss) {

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	pu32tty_dev_t *dev = container_of(tty->port, pu32tty_dev_t, port);

	unsigned long baudrate = ss->baud_base;

	tty_lock(tty);
	if (baudrate && baudrate != dev->baudrate) {
		dev->baudrate = baudrate;
		if (pu32_ishw)
			hwdrvchar_init (&dev->hw, dev->baudrate);
	}
	tty_unlock(tty);

	return 0;
}

static int pu32tty_get_serial (struct tty_struct *tty, struct serial_struct *ss) {
	pu32tty_dev_t *dev = container_of(tty->port, pu32tty_dev_t, port);
	tty_lock(tty);
	ss->line = tty->index;
	ss->port = dev->irq;
	ss->irq = dev->irq;
	ss->flags = 0;
	ss->xmit_fifo_size = dev->hw.bufsz;
	ss->baud_base = dev->baudrate;
	ss->close_delay = 0;
	ss->closing_wait = ASYNC_CLOSING_WAIT_NONE;
	ss->custom_divisor = 1;
	tty_unlock(tty);
	return 0;
}

static const struct tty_port_operations pu32tty_port_ops = {
	.activate = 0,
	.shutdown = 0
};

static const struct tty_operations pu32tty_tty_ops = {
	.open = pu32tty_tty_ops_open,
	.close = pu32tty_tty_ops_close,
	.hangup = pu32tty_tty_ops_hangup,
	.write = pu32tty_tty_ops_write,
	.write_room = pu32tty_tty_ops_write_room,
	.chars_in_buffer = pu32tty_chars_in_buffer,
	.set_termios = pu32tty_set_termios,
	.set_serial = pu32tty_set_serial,
	.get_serial = pu32tty_get_serial,
};

static hwdrvdevtbl hwdrvdevtbl_dev = {.e = (devtblentry *)0, .id = 5 /* Character device */};

// Expect pu32tty_dev_t fields hw, irq to have been set.
static int pu32tty_add (pu32tty_dev_t *dev) {

	struct console *con;
	console_lock();
	for_each_console(con) {
		if (con == &dev->console)
			break;
	}
	console_unlock();
	if (!con) {
		if (!dev->console.device) {
			strcpy(dev->console.name, "ttyS");
			dev->console.write = pu32tty_write;
			dev->console.device = pu32tty_device;
			dev->console.flags = CON_PRINTBUFFER;
			unsigned long index;
			for (index = 1; index < PU32TTY_CNT_MAX; ++index) {
				pu32tty_dev_t *d, *n, *found = 0;
				list_for_each_entry_safe (d, n, &pu32tty_devs, list) {
					if (d->console.index == index) {
						found = d;
						break;
					}
				}
				if (!found)
					break;
			}
			if (index == PU32TTY_CNT_MAX)
				return -EBUSY;
			dev->console.index = index;
		}
		register_console(&dev->console);
	}
	dev->console.flags &= ~CON_BOOT;

	tty_port_init(&dev->port);
	dev->port.ops = &pu32tty_port_ops;
	struct device *ttydev = tty_port_register_device( // Instead of tty_port_link_device() due to TTY_DRIVER_DYNAMIC_DEV.
		&dev->port, pu32tty_driver, dev->console.index, NULL);
	if (IS_ERR(ttydev)) {
		int ret = PTR_ERR(ttydev);
		unregister_console(con);
		pr_err("unable to register TTY port: ret(%d)\n", ret);
		return ret;
	}

	timer_setup(&dev->timer, pu32tty_poll, 0);

	INIT_WORK(&dev->work, pu32tty_work);

	return 0;
}

static void pu32tty_remove (pu32tty_dev_t *dev) {
	unregister_console(&dev->console);
	tty_unregister_device(pu32tty_driver, dev->console.index);
	tty_port_destroy(&dev->port);
	list_del(&dev->list);
	kfree(dev);
}

static pu32tty_dev_t pu32tty_dev0 = {
	.console = {
		.name   = "ttyS",
		.write  = pu32tty_write,
		.device = pu32tty_device,
		.flags  = CON_PRINTBUFFER,
		.index  = 0
	},
	.hw = {
		.addr = 0
	},
	.irq = -1,
	.baudrate = 115200
};

static int __init pu32tty_create_driver (void) {

	int ret;

	pu32tty_driver = tty_alloc_driver(PU32TTY_CNT_MAX,
		TTY_DRIVER_RESET_TERMIOS | TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV);

	if (IS_ERR(pu32tty_driver)) {
		ret = PTR_ERR(pu32tty_driver);
		goto err;
	}

	pu32tty_driver->driver_name = "pu32tty";
	pu32tty_driver->name = "ttyS";
	pu32tty_driver->major = TTY_MAJOR;
	pu32tty_driver->minor_start = 64;
	pu32tty_driver->type = TTY_DRIVER_TYPE_SERIAL;
	pu32tty_driver->subtype = SERIAL_TYPE_NORMAL;
	pu32tty_driver->init_termios = tty_std_termios;
	pu32tty_driver->init_termios.c_cflag |= CLOCAL;
	tty_set_operations (pu32tty_driver, &pu32tty_tty_ops);

	ret = tty_register_driver(pu32tty_driver);
	if (ret) {
		pr_err("unable to register console TTY driver\n");
		goto err_tty_driver_kref_put;
	}

	pu32tty_wq = create_workqueue("ttyS");
	if (!pu32tty_wq) {
		ret = -ENOMEM;
		goto err_tty_unregister_driver;
	}

	pu32tty_dev_t *dev = &pu32tty_dev0;

	if (pu32_ishw) {

		while (1) {

			ret = pu32tty_add(dev);
			if (ret)
				goto err_destroy_workqueue;
			list_add_tail(&dev->list, &pu32tty_devs);

			hwdrvdevtbl_find (&hwdrvdevtbl_dev, NULL);
			if (hwdrvdevtbl_dev.mapsz) {
				dev = kzalloc(sizeof(pu32tty_dev_t), GFP_KERNEL);
				dev->irq = ((hwdrvdevtbl_dev.intridx < 0) ? -1 : hwdrvdevtbl_dev.intridx);
				dev->hw.addr = hwdrvdevtbl_dev.addr;
				dev->baudrate = 115200;
				hwdrvchar_init (&dev->hw, dev->baudrate);
			} else
				break;
		}

	} else {
		dev->irq = PU32_VM_IRQ_TTYS0;
		ret = pu32tty_add(dev);
		if (ret)
			goto err_destroy_workqueue;
		list_add_tail(&dev->list, &pu32tty_devs);
	}

	return 0;

	err_destroy_workqueue:;
	pu32tty_dev_t *n;
	list_for_each_entry_safe (dev, n, &pu32tty_devs, list)
		pu32tty_remove(dev);
	flush_workqueue(pu32tty_wq);
	destroy_workqueue(pu32tty_wq);
	err_tty_unregister_driver:
	tty_unregister_driver(pu32tty_driver);
	err_tty_driver_kref_put:
	tty_driver_kref_put(pu32tty_driver);
	err:

	return ret;
}
module_init(pu32tty_create_driver);

static void pu32tty_destroy_driver (void) {
	pu32tty_dev_t *d, *n;
	list_for_each_entry_safe (d, n, &pu32tty_devs, list)
		pu32tty_remove(d);
	flush_workqueue(pu32tty_wq);
	destroy_workqueue(pu32tty_wq);
	tty_unregister_driver(pu32tty_driver);
	tty_driver_kref_put(pu32tty_driver);
}
module_exit(pu32tty_destroy_driver);

#ifdef CONFIG_EARLY_PRINTK
static int __init setup_early_printk (char *buf) {

	if (!buf || early_console)
		return 0;

	char *str = strstr(boot_command_line, "console=ttyS0,");
	if (str)
		sscanf(str, "console=ttyS0,%ld", &pu32tty_dev0.baudrate);

	if (pu32_ishw) {
		hwdrvdevtbl_find (&hwdrvdevtbl_dev, NULL);
		if (hwdrvdevtbl_dev.mapsz) {
			pu32tty_dev0.irq = ((hwdrvdevtbl_dev.intridx < 0) ? -1 : hwdrvdevtbl_dev.intridx);
			pu32tty_dev0.hw.addr = hwdrvdevtbl_dev.addr;
			hwdrvchar_init (&pu32tty_dev0.hw, pu32tty_dev0.baudrate);
		}
	}

	early_console = &pu32tty_dev0.console;
	register_console(early_console);

	return 0;
}
early_param ("earlyprintk", setup_early_printk);
#endif
