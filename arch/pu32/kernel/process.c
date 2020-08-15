// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#include <linux/ptrace.h>

#include <asm/ptrace.h>
#include <asm/thread_info.h>

#include <pu32.h>

// Start a new user thread.
// This function is called after loading a user
// executable (elf, aout, etc.) to setup a code
// entry (pc), new stack (sp) and return addr (rp).
void start_thread (struct pt_regs *regs, unsigned long pc, unsigned long sp) {
	struct pu32_pt_regs *ppr =
		(struct pu32_pt_regs *)((unsigned long)pu32_stack_bottom(regs) -
			sizeof(struct pu32_pt_regs));
	ppr->faultreason = pu32PreemptIntr;
	ppr->sysopcode = PU32_OPNOTAVAIL;
	ppr->which = PU32_PT_REGS_WHICH_SPFPRP;
	// Reset regs to the pu32_pt_regs at the bottom of the thread's kernel-stack,
	// as it is the one that is going to be used when resuming in userspace.
	regs = &ppr->regs;
	regs->sp = sp;
	regs->r1 = 0; // glibc _start() implemented at sysdeps/pu32/start.S expects %1 to be null.
	regs->fp = 0;
	void __tramp_exit (int exit_status);
	regs->rp = (unsigned long)__tramp_exit;
	regs->pc = pc;
}

// Implemented in kernel/entry.S .
void ret_from_fork (void);
void ret_from_kernel_thread (void);

// Create a new thread by copying from an existing one.
int copy_thread (struct task_struct *p, const struct kernel_clone_args *args) {
	// - clone_flags: flags.
	// - p: newly created task.
	unsigned long clone_flags = args->flags;
	unsigned long tls = args->tls;

	struct thread_info *pti = task_thread_info(p);

	if (args->fn) {
		// Kernel-thread.

		// Additional sizeof(struct pu32_pt_regs) space
		// is allocated and used only if the thread ever become
		// a user-thread; it further gets modified by start_thread().
		struct pu32_pt_regs *eos = (struct pu32_pt_regs *)end_of_stack(p);
		struct pu32_pt_regs *ppr = eos-1;
		ppr->faultreason = pu32PreemptIntr; // will not be used by __NR_PU32_switch_to.
		ppr->sysopcode = PU32_OPNOTAVAIL; // will not be used by __NR_PU32_switch_to.
		ppr->prev_ksp_offset = ((unsigned long)eos & (THREAD_SIZE - 1));

		unsigned long sp = (unsigned long)ppr;

		ppr -= 1;
		ppr->faultreason = pu32PreemptIntr; // will not be used by __NR_PU32_switch_to.
		ppr->sysopcode = PU32_OPNOTAVAIL; // will not be used by __NR_PU32_switch_to.
		ppr->prev_ksp_offset = (sp & (THREAD_SIZE - 1));
		ppr->which = PU32_PT_REGS_WHICH_SPFPRP;
		ppr->regs.sp = sp;
		ppr->regs.fp = 0;
		ppr->regs.rp = (unsigned long)ret_from_kernel_thread;
		ppr->regs.pc = 0; // will not be used by __NR_PU32_switch_to.

		pti->ksp = (unsigned long)ppr;

		pti->kr1 = (unsigned long)args->fn_arg;
		pti->kpc = (unsigned long)args->fn;

	} else {
		// User-thread.

		struct pu32_pt_regs *ppr = (struct pu32_pt_regs *)pti->ksp;

		ppr->regs.r1 = 0; // Child gets zero as return value.
		unsigned long usp = args->stack;
		if (usp) // Setup user-thread stack if specified.
			ppr->regs.sp = usp;
		if (clone_flags & CLONE_SETTLS)
			ppr->regs.tp = tls;

		unsigned long sp = (unsigned long)ppr;

		ppr -= 1;
		ppr->faultreason = pu32PreemptIntr; // will not be used by __NR_PU32_switch_to.
		ppr->sysopcode = PU32_OPNOTAVAIL; // will not be used by __NR_PU32_switch_to.
		ppr->prev_ksp_offset = (sp & (THREAD_SIZE - 1));
		ppr->which = PU32_PT_REGS_WHICH_SPFPRP;
		ppr->regs.sp = sp;
		ppr->regs.fp = 0;
		ppr->regs.rp = (unsigned long)ret_from_fork;
		ppr->regs.pc = 0; // will not be used by __NR_PU32_switch_to.

		pti->ksp = (unsigned long)ppr;
	}

	return 0;
}

// Free current thread data structures etc...
// No need to define when CONFIG_HAVE_EXIT_THREAD is not used.
//void exit_thread (void) {}

// When a process does an "exec", machine state like FPU and debug
// registers need to be reset. This is a hook function for that.
// Currently we don't have any such state to reset, so this is empty.
void flush_thread (void) {}

unsigned long __get_wchan (struct task_struct *tsk) {

	if (!tsk || tsk == current || task_is_running(tsk))
		return 0;

	if (!task_stack_page(tsk))
		return 0;

	return pu32_tsk_pt_regs(tsk)->regs.pc;
}

void arch_cpu_idle (void) {
	raw_local_irq_enable();
	asm volatile ("halt\n" ::: "memory");
	raw_local_irq_disable();
}
