// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/export.h>
#include <linux/irqflags.h>
#include <pu32.h>

unsigned long pu32irqflags[NR_CPUS];

// Read interrupt enabled status.
unsigned long arch_local_save_flags (void) {
	return pu32irqflags[raw_smp_processor_id()];
}
EXPORT_SYMBOL(arch_local_save_flags);

// Set interrupt enabled status.
void arch_local_irq_restore (unsigned long flags) {
	unsigned long pu32flags = current_thread_info()->pu32flags;
	if (flags == ARCH_IRQ_DISABLED) {
		pu32flags |= PU32_FLAGS_disIntr;
		__asm__ __volatile__ ("setflags %0\n" :: "r"(pu32flags) : "memory");
		current_thread_info()->pu32flags = pu32flags;
		__asm__ __volatile__("" ::: "memory");
		pu32irqflags[raw_smp_processor_id()] = flags;
		__asm__ __volatile__("" ::: "memory");
	} else {
		pu32flags &= ~PU32_FLAGS_disIntr;
		current_thread_info()->pu32flags = pu32flags;
		__asm__ __volatile__("" ::: "memory");
		pu32irqflags[raw_smp_processor_id()] = flags;
		__asm__ __volatile__("" ::: "memory");
		__asm__ __volatile__ ("setflags %0\n" :: "r"(pu32flags) : "memory");
	}
}
EXPORT_SYMBOL(arch_local_irq_restore);

void pu32_local_irq_enable (void) {
	raw_local_irq_enable();
}
void pu32_local_irq_disable (void) {
	raw_local_irq_disable();
}

void __init init_IRQ (void) {
	int i; for (i = 0; i < NR_IRQS; ++i) {
		irq_set_chip(i, &dummy_irq_chip);
		irq_set_handler(i, handle_simple_irq);
	}
	#ifdef CONFIG_SMP
        ipi_init();
	#endif
}

void do_IRQ (unsigned long irq) {
	raw_local_irq_disable();
	irq_enter();
	generic_handle_irq(irq);
	irq_exit();
	raw_local_irq_enable();
}
