// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef __ASM_PU32_SMP_H
#define __ASM_PU32_SMP_H

#ifndef CONFIG_SMP
#error "<asm/smp.h> included in non-SMP build"
#endif

#include <linux/cpumask.h>

//void __init setup_smp(void);
//void __init setup_smp_ipi(void);

void arch_send_call_function_ipi_mask (struct cpumask *mask);

void arch_send_call_function_single_ipi (int cpu);

#define raw_smp_processor_id() ({		\
	unsigned long n;			\
	asm volatile (				\
		"getcoreid %0"		\
		:  "=r"(n));			\
	n; })

#define INVALID_HWCPUID	ULONG_MAX

extern unsigned long __cpu_logical_map[NR_CPUS]; // Map logical to physical.
#define cpu_logical_map(cpu) __cpu_logical_map[cpu]

int __cpu_disable(void);

void __cpu_die(unsigned int cpu);

#endif /* __ASM_PU32_SMP_H */
