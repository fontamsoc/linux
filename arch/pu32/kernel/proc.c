// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#include <linux/cpumask.h>
#include <linux/seq_file.h>

#include <asm/cpuinfo.h>

DEFINE_PER_CPU(struct c_info, c_info);

unsigned long c_info_rcache; // In KB. Set by pu32_start().

void c_setup (void) { // Best called before setting cpu online.

	unsigned long ver;
	asm volatile ("getver %0\n" : "=r"(ver));

	unsigned long cap;
	asm volatile ("getcap %0\n" : "=r"(cap));

	unsigned long freq;
	asm volatile ("getclkfreq %0\n" : "=r"(freq));

	unsigned long icache;
	asm volatile ("geticachesize %0\n" : "=r"(icache));

	unsigned long dcache;
	asm volatile ("getdcachesize %0\n" : "=r"(dcache));

	unsigned long coreid;
	asm volatile ("getcoreid %0\n" : "=r"(coreid));

	per_cpu(c_info, coreid).version_major = ((ver & 0xff00) >> 8);
	per_cpu(c_info, coreid).version_minor = (ver & 0xff);
	per_cpu(c_info, coreid).feature_mmu = (cap & 0b1);
	per_cpu(c_info, coreid).feature_hptw = !!(cap & 0b10);
	per_cpu(c_info, coreid).freq = (freq / 1000000);
	per_cpu(c_info, coreid).icache = (icache * sizeof(unsigned long) / 1024);
	per_cpu(c_info, coreid).dcache = (dcache * sizeof(unsigned long) / 1024);
}

// /proc/cpuinfo callbacks.

static int c_show (struct seq_file *m, void *v) {

	unsigned long i, n = 0;

	for_each_online_cpu(i) {
		if (n)
			seq_printf(m, "\n");
		++n;
		seq_printf(m, "processor     : %ld\n", i);
		seq_printf(m, "cpu-family    : PU32\n");
		seq_printf(m, "cpu-version   : %ld.%ld\n",
			(unsigned long)per_cpu(c_info, i).version_major,
			(unsigned long)per_cpu(c_info, i).version_minor);
		seq_printf(m, "cpu-features  :"),
			(per_cpu(c_info, i).feature_mmu  ? seq_printf(m, " mmu")  : (unsigned long)0),
			(per_cpu(c_info, i).feature_hptw ? seq_printf(m, " hptw") : (unsigned long)0),
			seq_printf(m, "\n");
		seq_printf(m, "cpu-MHz       : %ld\n", per_cpu(c_info, i).freq);
		seq_printf(m, "cpu-icache-KB : %ld\n", per_cpu(c_info, i).icache);
		seq_printf(m, "cpu-dcache-KB : %ld\n", per_cpu(c_info, i).dcache);
		seq_printf(m, "ram-cache-KB  : %ld\n", c_info_rcache);
	}

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
