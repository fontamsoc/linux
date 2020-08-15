// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef __ASM_PU32_MMU_CONTEXT_H
#define __ASM_PU32_MMU_CONTEXT_H

#include <linux/mm.h>
#include <linux/sched.h>

#include <asm/tlbflush.h>
#include <asm-generic/mm_hooks.h>

#define enter_lazy_tlb(mm, tsk) do {} while (0)

static inline int init_new_context (
	struct task_struct *task, struct mm_struct *mm) {
	int i;
	for_each_possible_cpu(i)
		mm->context[i] = PU32_NO_CONTEXT;
	return 0;
}

unsigned long get_mmu_context (void);

static inline void switch_mm (
	struct mm_struct *prev, struct mm_struct *next, struct task_struct *tsk) {

	unsigned long flags;
	local_irq_save(flags);

	smp_mb();

	unsigned long context = next->context[raw_smp_processor_id()];
	if (context != PU32_NO_CONTEXT)
		goto done;

	context = get_mmu_context();
	if (context != PU32_NO_CONTEXT)
		next->context[raw_smp_processor_id()] = context;

	done:

	local_flush_tlb_mm(next);

	asm volatile (
		"cpy %%sr, %1\n"
		"setasid %0\n" ::
		"r"(context),
		"r"(next->pgd) :
		"memory");

	local_irq_restore(flags);
}

#define activate_mm(prev, next) switch_mm(prev, next, current)

static inline void deactivate_mm (struct task_struct *tsk, struct mm_struct *mm) {}

void put_mmu_context (unsigned long context, unsigned long cpu);

static inline void destroy_context (struct mm_struct *mm) {

	unsigned long flags;
	local_irq_save(flags);

	int i;
	for_each_possible_cpu(i) {
		unsigned long context = mm->context[i];
		if (context != PU32_NO_CONTEXT)
			put_mmu_context(context, i);
	}

	smp_mb();

	local_irq_restore(flags);
}

#endif /* __ASM_PU32_MMU_CONTEXT_H */
