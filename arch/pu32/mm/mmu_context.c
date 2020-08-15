// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

//#include <linux/mm.h>
//#include <linux/init.h>

//#include <asm/tlbflush.h>
//#include <asm/mmu_context.h>

unsigned long *mm_context_bitmap = 0; // [LAST_CONTEXT / BITS_PER_LONG + 1];

/*
 * Steal a context from a task that has one at the moment.
 *
 * This isn't an LRU system, it just frees up each context in
 * turn (sort-of pseudo-random replacement :).  This would be the
 * place to implement an LRU scheme if anyone were motivated to do it.
 */
void steal_context(void)
{
	struct mm_struct *mm;

	/* free up context `next_mmu_context' */
	/* if we shouldn't free context 0, don't... */
	if (next_mmu_context < FIRST_CONTEXT)
		next_mmu_context = FIRST_CONTEXT;
	mm = context_mm[next_mmu_context];
	flush_tlb_mm(mm);
	destroy_context(mm);
}
