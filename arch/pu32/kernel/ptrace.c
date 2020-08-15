// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#include <linux/ptrace.h>

#include <asm/elf.h>
#include <asm/ptrace.h>
#include <asm/thread_info.h>

int dump_fpu (struct pt_regs *regs, elf_fpregset_t *fpu) {
        return 0; // PU32 has no separate FPU registers.
}

void show_regs (struct pt_regs *regs, const char *loglvl) {
	pr_info ("%spc(0x%lx(%pS)) rp(0x%lx(%pS))\n", loglvl,
		regs->pc, (void *)regs->pc, regs->rp, (void *)regs->rp);
	pr_info (
		"%ssp(0x%lx) r1(0x%lx) r2(0x%lx) r3(0x%lx) r4(0x%lx) r5(0x%lx) r6(0x%lx) r7(0x%lx)\n", loglvl,
		regs->sp, regs->r1, regs->r2, regs->r3, regs->r4, regs->r5, regs->r6, regs->r7);
	pr_info (
		"%sr8(0x%lx) r9(0x%lx) r10(0x%lx) r11(0x%lx) r12(0x%lx) sr(0x%lx) fp(0x%lx)\n", loglvl,
		regs->r8, regs->r9, regs->r10, regs->r11, regs->r12, regs->sr, regs->fp);
}

// __attribute__((__noinline__)) forces %rp to be saved on the stack.
__attribute__((__noinline__)) static void stacktrace (
	unsigned long *start, unsigned long *end, const char *loglvl) {
	do {
		unsigned long addr = *start;
		extern char _text[];
		if (__kernel_text_address(addr) && (addr >= (unsigned long)_text) &&
			(*(unsigned char *)(addr-2) == 0xd2 /* jl */))
			pr_info ("%s0x%08lx 0x%08lx %pS\n", loglvl,
				(unsigned long)start, addr, (void *)addr);
		++start;
	} while (start < end);
}

void show_stack (struct task_struct *tsk, unsigned long *sp, const char *loglvl) {
	if (sp) {
		// sp is set when show_stack() is called from pu32-kernelmode.
		struct pt_regs regs;
		regs.sp = (unsigned long)sp;
		asm volatile ("cpy %0, %%sp" : "=r"(sp));
		pr_info ("%sstacktrace:\n", loglvl);
		stacktrace (sp, (unsigned long *)pu32_stack_bottom(sp), loglvl);
		sp = (unsigned long *)regs.sp;
		asm volatile ("setkgpr %0, %%1" : "=r"(regs.r1));
		asm volatile ("setkgpr %0, %%2" : "=r"(regs.r2));
		asm volatile ("setkgpr %0, %%3" : "=r"(regs.r3));
		asm volatile ("setkgpr %0, %%4" : "=r"(regs.r4));
		asm volatile ("setkgpr %0, %%5" : "=r"(regs.r5));
		asm volatile ("setkgpr %0, %%6" : "=r"(regs.r6));
		asm volatile ("setkgpr %0, %%7" : "=r"(regs.r7));
		asm volatile ("setkgpr %0, %%8" : "=r"(regs.r8));
		asm volatile ("setkgpr %0, %%9" : "=r"(regs.r9));
		asm volatile ("setkgpr %0, %%10" : "=r"(regs.r10));
		asm volatile ("setkgpr %0, %%11" : "=r"(regs.r11));
		asm volatile ("setkgpr %0, %%12" : "=r"(regs.r12));
		asm volatile ("setkgpr %0, %%sr" : "=r"(regs.sr));
		asm volatile ("setkgpr %0, %%fp" : "=r"(regs.fp));
		asm volatile ("setkgpr %0, %%rp" : "=r"(regs.rp));
		asm volatile ("getuip %0" : "=r"(regs.pc));
		pu32FaultReason faultreason;
		asm volatile ("getfaultreason %0\n" : "=r"(faultreason));
		unsigned long sysopcode;
		asm volatile ("getsysopcode %0" : "=r"(sysopcode));
		pr_info ("%s%s: %s\n", loglvl,
			pu32faultreasonstr (faultreason, (regs.pc&1)),
			pu32sysopcodestr (sysopcode));
		show_regs(&regs, loglvl);
	} else
		asm volatile ("cpy %0, %%sp" : "=r"(sp));
	// pu32_get_thread_info() is used used to retrieve thread_info
	// instead-of current_thread_info() because this function can be
	// called from interrupt context where (sp != current_thread_info()->task->stack)
	// because interrupt context uses pu32ctxswitchhdlr() dedicated stack.
	struct thread_info *ti = pu32_get_thread_info(sp);
	unsigned long ksp = ti->ksp;
	pr_info ("%sstacktrace:\n", loglvl);
	stacktrace (sp, (unsigned long *)ksp, loglvl);
	unsigned iskthread = (ti->task->flags&PF_KTHREAD);
	struct pu32_pt_regs *eos = (struct pu32_pt_regs *)pu32_stack_bottom(sp);
	while (1) {
		if (iskthread) {
			// Unlike a kernel-thread, a user-thread would always have a saved context.
			if (ksp == (unsigned long)(eos-1))
				return;
		} else if (ksp == (unsigned long)eos)
			return;
		struct pu32_pt_regs *ppr = (struct pu32_pt_regs *)ksp;
		pr_info ("%s%s: %s\n", loglvl,
			pu32faultreasonstr (ppr->faultreason, (ppr->regs.pc&1)),
			pu32sysopcodestr (ppr->sysopcode));
		show_regs(&ppr->regs, loglvl);
		sp = (unsigned long *)(ksp + sizeof(struct pu32_pt_regs));
		ksp = ((ksp & ~(THREAD_SIZE - 1)) +
			((struct pu32_pt_regs *)ksp)->prev_ksp_offset);
		if ((unsigned long)sp < ksp) {
			pr_info ("%sstacktrace:\n", loglvl);
			stacktrace (sp, (unsigned long *)ksp, loglvl);
		}
	}
}

void ptrace_disable (struct task_struct *child) {
        user_disable_single_step(child);
        clear_tsk_thread_flag(child, TIF_SYSCALL_TRACE);
}

long arch_ptrace (
	struct task_struct *child,
	long request,
	unsigned long addr,
	unsigned long data) {
	int ret;
	switch (request) {
	default:
		ret = ptrace_request(child, request, addr, data);
		break;
	}
	return ret;
}
