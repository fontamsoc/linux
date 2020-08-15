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
	struct task_struct *task,
	struct mm_struct *mm) {
	mm->context = PU32_NO_CONTEXT;
        return 0;
}

unsigned long get_mmu_context (void);

static inline void switch_mm (struct mm_struct *prev, struct mm_struct *next, struct task_struct *tsk) {

	unsigned long flags;
	raw_local_irq_save(flags);

	unsigned long context = next->context;
	if (context != PU32_NO_CONTEXT)
		goto done;

	context = get_mmu_context();
	if (context != PU32_NO_CONTEXT)
		next->context = context;

	local_flush_tlb_mm(next);

	done:
	asm volatile (
		"cpy %%sr, %1\n"
		"setasid %0\n" ::
		"r"(context),
		"r"(next->pgd) :
		"memory");

	raw_local_irq_restore(flags);
}

static inline void activate_mm (struct mm_struct *prev, struct mm_struct *next) {
	switch_mm (prev, next, NULL);
}

static inline void deactivate_mm (struct task_struct *tsk, struct mm_struct *mm) {}

void put_mmu_context (unsigned long context);

static inline void destroy_context (struct mm_struct *mm) {

	unsigned long flags;
	raw_local_irq_save(flags);

	unsigned long context = mm->context;
	if (context != PU32_NO_CONTEXT)
		put_mmu_context(context);

	raw_local_irq_restore(flags);
}

#endif /* __ASM_PU32_MMU_CONTEXT_H */
