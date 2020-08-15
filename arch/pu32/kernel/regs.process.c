// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

// Save pu32-usermode registers and pu32_pt_regs_which to struct thread_info.
void save_pu32umode_regs (
	struct thread_info *ti,
	pu32_pt_regs_which which,
	pu32FaultReason faultreason,
	unsigned long sysopcode) {

	unsigned long sp;
	asm volatile ("setkgpr %0, %%sp\n" : "=r"(sp) :: "memory");

	unsigned long ksp = (pu32_in_userspace(ti) ? ti->ksp : sp);

	#if defined(CONFIG_PU32_DEBUG)
	if ((ksp&(THREAD_SIZE-1)) < (sizeof(struct thread_info) + sizeof(struct pu32_pt_regs)))
		pu32hang ("save_pu32umode_regs: kernel stack full\n");
	#endif

	ksp -= sizeof(struct pu32_pt_regs);

	struct pu32_pt_regs *ppr = (struct pu32_pt_regs *)ksp;

	ppr->prev_ksp_offset = (ti->ksp & (THREAD_SIZE - 1));
	ti->ksp = ksp;

	ppr->faultreason = faultreason;
	ppr->sysopcode = sysopcode;

	ppr->which = which;

	switch (which) {

		case PU32_PT_REGS_WHICH_SPFPRP:
			// Also save %pc.
			asm volatile (

				"st32 %1, %0\n"

				"setkgpr %%sr, %%14\n"
				"inc8 %0, 4*14\n"
				"st32 %%sr, %0\n"

				"setkgpr %%sr, %%15\n"
				"inc8 %0, 4\n"
				"st32 %%sr, %0\n"

				"getuip %%sr\n"
				"inc8 %0, 4\n"
				"st32 %%sr, %0\n"

				::
				"r"(&ppr->regs), "r"(sp)
				:
				"memory");

			return;

		case PU32_PT_REGS_WHICH_ALL:

			asm volatile (

				"st32 %1, %0\n"

				"setkgpr %%sr, %%1\n"
				"inc8 %0, 4\n"
				"st32 %%sr, %0\n"

				"setkgpr %%sr, %%2\n"
				"inc8 %0, 4\n"
				"st32 %%sr, %0\n"

				"setkgpr %%sr, %%3\n"
				"inc8 %0, 4\n"
				"st32 %%sr, %0\n"

				"setkgpr %%sr, %%4\n"
				"inc8 %0, 4\n"
				"st32 %%sr, %0\n"

				"setkgpr %%sr, %%5\n"
				"inc8 %0, 4\n"
				"st32 %%sr, %0\n"

				"setkgpr %%sr, %%6\n"
				"inc8 %0, 4\n"
				"st32 %%sr, %0\n"

				"setkgpr %%sr, %%7\n"
				"inc8 %0, 4\n"
				"st32 %%sr, %0\n"

				"setkgpr %%sr, %%8\n"
				"inc8 %0, 4\n"
				"st32 %%sr, %0\n"

				"setkgpr %%sr, %%9\n"
				"inc8 %0, 4\n"
				"st32 %%sr, %0\n"

				"setkgpr %%sr, %%10\n"
				"inc8 %0, 4\n"
				"st32 %%sr, %0\n"

				"setkgpr %%sr, %%11\n"
				"inc8 %0, 4\n"
				"st32 %%sr, %0\n"

				"setkgpr %%sr, %%12\n"
				"inc8 %0, 4\n"
				"st32 %%sr, %0\n"

				"setkgpr %%sr, %%13\n"
				"inc8 %0, 4\n"
				"st32 %%sr, %0\n"

				"setkgpr %%sr, %%14\n"
				"inc8 %0, 4\n"
				"st32 %%sr, %0\n"

				"setkgpr %%sr, %%15\n"
				"inc8 %0, 4\n"
				"st32 %%sr, %0\n"

				"getuip %%sr\n"
				"inc8 %0, 4\n"
				"st32 %%sr, %0\n"

				::
				"r"(&ppr->regs), "r"(sp)
				:
				"memory");

			return;

		default:
			pu32hang ("save_pu32umode_regs: invalid pu32_pt_regs_which\n");
	}
}

// Restore pu32-usermode registers from struct thread_info.
void restore_pu32umode_regs (struct thread_info *ti) {

	unsigned long ksp = ti->ksp;

	#if defined(CONFIG_PU32_DEBUG)
	if (ksp == pu32_stack_bottom(ksp))
		pu32hang ("restore_pu32umode_regs: kernel stack empty\n");
	#endif

	ti->ksp =
		(ti->ksp & ~(THREAD_SIZE - 1)) +
		((struct pu32_pt_regs *)ksp)->prev_ksp_offset;

	struct pu32_pt_regs *ppr = (struct pu32_pt_regs *)ksp;

	switch (ppr->which) {

		case PU32_PT_REGS_WHICH_SPFPRP:
			// Restore only %sp, %fp and %rp.
			asm volatile (

				"ld32 %%sr, %0\n"
				"setugpr %%0, %%sr\n"

				"inc8 %0, 4*14\n"
				"ld32 %%sr, %0\n"
				"setugpr %%14, %%sr\n"

				"inc8 %0, 4\n"
				"ld32 %%sr, %0\n"
				"setugpr %%15, %%sr\n"

				::
				"r"(&ppr->regs)
				:
				"memory");

			return;

		case PU32_PT_REGS_WHICH_ALL:
			// Restore all registers except %1 and %pc.
			asm volatile (

				"ld32 %%sr, %0\n"
				"setugpr %%0, %%sr\n"

				//"inc8 %0, 4\n"
				//"ld32 %%sr, %0\n"
				//"setugpr %%1, %%sr\n"

				"inc8 %0, 4*2\n" // Increment by 8 since saving %1 is skipped.
				"ld32 %%sr, %0\n"
				"setugpr %%2, %%sr\n"

				"inc8 %0, 4\n"
				"ld32 %%sr, %0\n"
				"setugpr %%3, %%sr\n"

				"inc8 %0, 4\n"
				"ld32 %%sr, %0\n"
				"setugpr %%4, %%sr\n"

				"inc8 %0, 4\n"
				"ld32 %%sr, %0\n"
				"setugpr %%5, %%sr\n"

				"inc8 %0, 4\n"
				"ld32 %%sr, %0\n"
				"setugpr %%6, %%sr\n"

				"inc8 %0, 4\n"
				"ld32 %%sr, %0\n"
				"setugpr %%7, %%sr\n"

				"inc8 %0, 4\n"
				"ld32 %%sr, %0\n"
				"setugpr %%8, %%sr\n"

				"inc8 %0, 4\n"
				"ld32 %%sr, %0\n"
				"setugpr %%9, %%sr\n"

				"inc8 %0, 4\n"
				"ld32 %%sr, %0\n"
				"setugpr %%10, %%sr\n"

				"inc8 %0, 4\n"
				"ld32 %%sr, %0\n"
				"setugpr %%11, %%sr\n"

				"inc8 %0, 4\n"
				"ld32 %%sr, %0\n"
				"setugpr %%12, %%sr\n"

				"inc8 %0, 4\n"
				"ld32 %%sr, %0\n"
				"setugpr %%13, %%sr\n"

				"inc8 %0, 4\n"
				"ld32 %%sr, %0\n"
				"setugpr %%14, %%sr\n"

				"inc8 %0, 4\n"
				"ld32 %%sr, %0\n"
				"setugpr %%15, %%sr\n"

				//"inc8 %0, 4\n"
				//"ld32 %%sr, %0\n"
				//"setuip %%sr\n"

				::
				"r"(&ppr->regs)
				:
				"memory");

			return;

		default:
			pu32hang ("restore_pu32umode_regs: invalid pu32_pt_regs_which\n");
	}
}
