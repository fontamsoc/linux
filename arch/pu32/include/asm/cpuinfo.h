// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef __ASM_PU32_CPUINFO_H
#define __ASM_PU32_CPUINFO_H

struct c_info {
	unsigned long version_major :8;
	unsigned long version_minor :8;
	unsigned long feature_mmu   :1;
	unsigned long feature_hptw  :1;
	unsigned long freq; // In MHz.
	unsigned long icache; // In KB.
	unsigned long dcache; // In KB.
	unsigned long tlbsz;
};

extern struct c_info c_info[NR_CPUS];

extern unsigned long c_info_rcache;

void c_setup (void);

#endif /* __ASM_PU32_CPUINFO_H */
