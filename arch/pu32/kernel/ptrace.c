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
	if (regs == (struct pt_regs *)0 || regs == (struct pt_regs *)-1) {
		// Assume coming from usermode if regs invalid pointer.
		struct pt_regs uregs;
		asm volatile ("setkgpr %0, %%sp" : "=r"(uregs.sp));
		asm volatile ("setkgpr %0, %%1"  : "=r"(uregs.r1));
		asm volatile ("setkgpr %0, %%2"  : "=r"(uregs.r2));
		asm volatile ("setkgpr %0, %%3"  : "=r"(uregs.r3));
		asm volatile ("setkgpr %0, %%4"  : "=r"(uregs.r4));
		asm volatile ("setkgpr %0, %%5"  : "=r"(uregs.r5));
		asm volatile ("setkgpr %0, %%6"  : "=r"(uregs.r6));
		asm volatile ("setkgpr %0, %%7"  : "=r"(uregs.r7));
		asm volatile ("setkgpr %0, %%8"  : "=r"(uregs.r8));
		asm volatile ("setkgpr %0, %%9"  : "=r"(uregs.r9));
		asm volatile ("setkgpr %0, %%10" : "=r"(uregs.r10));
		asm volatile ("setkgpr %0, %%11" : "=r"(uregs.r11));
		asm volatile ("setkgpr %0, %%12" : "=r"(uregs.r12));
		asm volatile ("setkgpr %0, %%sr" : "=r"(uregs.sr));
		asm volatile ("setkgpr %0, %%fp" : "=r"(uregs.fp));
		asm volatile ("setkgpr %0, %%rp" : "=r"(uregs.rp));
		asm volatile ("getuip %0"        : "=r"(uregs.pc));
		regs = &uregs;
	}
	if (!loglvl)
		loglvl = KERN_INFO;
	printk ("%spc(0x%lx(%pS)) rp(0x%lx(%pS))\n", loglvl,
		regs->pc, (void *)regs->pc, regs->rp, (void *)regs->rp);
	printk (
		"%ssp(0x%lx) r1(0x%lx) r2(0x%lx) r3(0x%lx) r4(0x%lx) r5(0x%lx) r6(0x%lx) r7(0x%lx)\n", loglvl,
		regs->sp, regs->r1, regs->r2, regs->r3, regs->r4, regs->r5, regs->r6, regs->r7);
	printk (
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
		pr_info ("%sstacktrace:\n", loglvl);
		asm volatile ("cpy %0, %%sp" : "=r"(sp));
		stacktrace (sp, (unsigned long *)pu32_stack_bottom(sp), loglvl);
		pu32FaultReason faultreason;
		asm volatile ("getfaultreason %0\n" : "=r"(faultreason));
		unsigned long sysopcode;
		asm volatile ("getsysopcode %0" : "=r"(sysopcode));
		pr_info ("%s%s: %s\n", loglvl,
			pu32faultreasonstr (faultreason, 0),
			pu32sysopcodestr (sysopcode));
	}
	struct thread_info *ti = current_thread_info();
	unsigned long ksp = ti->ksp;
	sp = ti->task->stack;
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
