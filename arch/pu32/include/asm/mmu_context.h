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
	mm->context = 0; // TODO: Find context to use ...
        return 0;
}

static inline void switch_mm (
	struct mm_struct *prev,
	struct mm_struct *next,
	struct task_struct *tsk) {
	unsigned long flags;
        local_irq_save(flags);
	/* TODO: Do only if more than one task is
		using this task asid on this core. */{
		local_flush_tlb_mm(prev);
		local_flush_tlb_mm(next);
	}
	local_irq_restore(flags);
}

static inline void activate_mm (struct mm_struct *prev, struct mm_struct *next) {
	switch_mm (prev, next, current);
}

static inline void deactivate_mm (
	struct task_struct *tsk, struct mm_struct *mm) {
	// TODO:
}

static inline void destroy_context (struct mm_struct *mm) {
	// TODO:
}

#endif /* __ASM_PU32_MMU_CONTEXT_H */
