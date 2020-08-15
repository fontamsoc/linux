// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifdef CONFIG_SMP
#include <linux/spinlock.h>
#endif

#include <asm/mmu_context.h>

#ifdef CONFIG_SMP
static DEFINE_SPINLOCK(mmu_context_lock);
#endif

#define PU32_NR_ASIDS (1<<12)
static DECLARE_BITMAP(mmu_context, PU32_NR_ASIDS);

static volatile unsigned long last_mmu_context = PU32_NO_CONTEXT;

unsigned long get_mmu_context (void) {

	#ifdef CONFIG_SMP
	spin_lock(&mmu_context_lock);
	#endif

	unsigned long context = find_next_zero_bit (mmu_context, PU32_NR_ASIDS, last_mmu_context+1);
	if (context == PU32_NR_ASIDS) {
		if (last_mmu_context != PU32_NO_CONTEXT)
			context = find_next_zero_bit (mmu_context, PU32_NR_ASIDS, PU32_NO_CONTEXT+1);
		if (context == PU32_NR_ASIDS)
			context = PU32_NO_CONTEXT;
	}

	if (context != PU32_NO_CONTEXT)
		__set_bit (context, mmu_context);

	last_mmu_context = context;

	#ifdef CONFIG_SMP
	spin_unlock(&mmu_context_lock);
	#endif

	return context;
}

void put_mmu_context (unsigned long context) {

	#if defined(CONFIG_PU32_DEBUG)
	BUG_ON (context == PU32_NO_CONTEXT);
	#endif

	#ifdef CONFIG_SMP
	spin_lock(&mmu_context_lock);
	#endif

	__clear_bit (context, mmu_context);

	#ifdef CONFIG_SMP
	spin_unlock(&mmu_context_lock);
	#endif
}

void __init init_mmu_context (void) {
	__set_bit (PU32_NO_CONTEXT, mmu_context);
	__set_bit (PU32_INIT_CONTEXT, mmu_context);
}
