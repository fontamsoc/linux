// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#include <linux/cpumask.h>
#include <linux/seq_file.h>

// /proc/cpuinfo callbacks.

static int c_show (struct seq_file *m, void *v) {

	unsigned i;

	for_each_online_cpu(i) {
		seq_printf(m, "processor:\t%d\n", i);
	}

	seq_printf(m, "\n");

	return 0;
}

static void *c_start (struct seq_file *m, loff_t *pos) {
	return *pos < 1 ? (void *)1 : NULL;
}

static void *c_next (struct seq_file *m, void *v, loff_t *pos) {
	++*pos;
	return NULL;
}

static void c_stop (struct seq_file *m, void *v) {}

const struct seq_operations cpuinfo_op = {
	.start	= c_start,
	.next	= c_next,
	.stop	= c_stop,
	.show	= c_show,
};
