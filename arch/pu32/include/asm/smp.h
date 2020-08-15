// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef __ASM_PU32_SMP_H
#define __ASM_PU32_SMP_H

#ifndef CONFIG_SMP
#error "<asm/smp.h> included in non-SMP build"
#endif

#define raw_smp_processor_id() ({	\
	int n;				\
	asm volatile (			\
		"getcoreid %0" :	\
		"=r"(n) ::		\
		"memory");		\
	n; })

struct cpumask;
void arch_send_call_function_ipi_mask (struct cpumask *mask);
void arch_send_call_function_single_ipi (int cpu);

void ipi_init (void);

#ifdef CONFIG_HOTPLUG_CPU

int __cpu_disable (void);
void __cpu_die (unsigned int cpu);

#endif /* CONFIG_HOTPLUG_CPU */

#endif /* __ASM_PU32_SMP_H */
