// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifdef CONFIG_SMP
#include <linux/spinlock.h>
#endif

#include <asm/mmu_context.h>

#define PU32_NR_ASIDS (1<<12)
static DECLARE_BITMAP(mmu_context[NR_CPUS], PU32_NR_ASIDS);

unsigned long get_mmu_context (void) {

	static unsigned long last_mmu_context[NR_CPUS] = {
		[0 ... NR_CPUS - 1] = PU32_NO_CONTEXT
	};

	unsigned long context = find_next_zero_bit (
		mmu_context[raw_smp_processor_id()],
		PU32_NR_ASIDS,
		last_mmu_context[raw_smp_processor_id()]+1);
	if (context == PU32_NR_ASIDS) {
		if (last_mmu_context[raw_smp_processor_id()] != PU32_NO_CONTEXT)
			context = find_next_zero_bit (
				mmu_context[raw_smp_processor_id()], PU32_NR_ASIDS, PU32_NO_CONTEXT+1);
		if (context == PU32_NR_ASIDS)
			context = PU32_NO_CONTEXT;
	}

	if (context != PU32_NO_CONTEXT)
		__set_bit (context, mmu_context[raw_smp_processor_id()]);

	last_mmu_context[raw_smp_processor_id()] = context;

	return context;
}

void put_mmu_context (unsigned long context, unsigned long cpu) {
	#if defined(CONFIG_PU32_DEBUG)
	BUG_ON (context == PU32_NO_CONTEXT);
	#endif
	__clear_bit (context, mmu_context[cpu]);
}

void __init init_mmu_context (void) {
	int i;
	for_each_possible_cpu(i) {
		__set_bit (PU32_NO_CONTEXT, mmu_context[i]);
		__set_bit (PU32_INIT_CONTEXT, mmu_context[i]);
	}
}
