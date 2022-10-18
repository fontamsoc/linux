// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

static void pu32sysrethdlr_timerIntr (unsigned long sysopcode) {

	struct thread_info *ti = current_thread_info();
	struct task_struct *tsk = ti->task;

	#if defined(CONFIG_PU32_DEBUG)
	if (pu32irqflags[raw_smp_processor_id()] == ARCH_IRQ_DISABLED) {
		pu32hang ("pu32TimerIntr: unexpected interrupt !!! hwflags(0x%x)\n",
			pu32hwflags[raw_smp_processor_id()]);
		return;
		// pu32hang() will infinite-loop;
		// so `return` will not be executed, but
		// it is left in place for completeness.
	}
	#endif

	pu32_timer_intr();

	if (ti->preempt_count == PREEMPT_ENABLED && (ti->flags&_TIF_WORK_MASK)) {

		save_pu32umode_regs(ti, PU32_PT_REGS_WHICH_ALL, pu32TimerIntr, sysopcode);

		unsigned long hwflags = pu32hwflags[raw_smp_processor_id()];
		hwflags &= ~PU32_FLAGS_USERSPACE;
		hwflags |= PU32_FLAGS_KERNELSPACE;
		pu32hwflags[raw_smp_processor_id()] = hwflags;

		asm volatile ("setugpr %%tp, %0\n" :: "r"(ti) : "memory");
		asm volatile ("setugpr %%sp, %0\n" :: "r"(ti->ksp) : "memory");
		//asm volatile ("setugpr %%1, %0\n" :: "r"() : "memory");
		//asm volatile ("setugpr %%rp, %0\n" :: "r"() : "memory");
		asm volatile ("setuip %0\n" :: "r"(ret_from_exception) : "memory");
		struct mm_struct *mm = tsk->active_mm;
		asm volatile (
			"cpy %%sr, %1\n"
			"setasid %0\n" ::
			"r"(mm->context),
			"r"(mm->pgd) :
			"memory");
		asm volatile ("setflags %0\n" :: "r"(hwflags) : "memory");
	}
}
