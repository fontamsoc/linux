// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#include <linux/elf.h>
#include <linux/ptrace.h>
#include <linux/regset.h>

#include <asm/elf.h>
#include <asm/ptrace.h>
#include <asm/thread_info.h>

int dump_fpu (struct pt_regs *regs, elf_fpregset_t *fpu) {
        return 0; // PU32 has no separate FPU registers.
}

static void __show_regs (struct pt_regs *regs, const char *loglvl) {
	if (regs == (struct pt_regs *)0 || regs == (struct pt_regs *)-1) {
		// Assume coming from usermode if regs invalid pointer.
		struct pt_regs uregs;
		asm volatile ("setkgpr %0, %%sp\n" : "=r"(uregs.sp) :: "memory");
		asm volatile ("setkgpr %0, %%1\n"  : "=r"(uregs.r1) :: "memory");
		asm volatile ("setkgpr %0, %%2\n"  : "=r"(uregs.r2) :: "memory");
		asm volatile ("setkgpr %0, %%3\n"  : "=r"(uregs.r3) :: "memory");
		asm volatile ("setkgpr %0, %%4\n"  : "=r"(uregs.r4) :: "memory");
		asm volatile ("setkgpr %0, %%5\n"  : "=r"(uregs.r5) :: "memory");
		asm volatile ("setkgpr %0, %%6\n"  : "=r"(uregs.r6) :: "memory");
		asm volatile ("setkgpr %0, %%7\n"  : "=r"(uregs.r7) :: "memory");
		asm volatile ("setkgpr %0, %%8\n"  : "=r"(uregs.r8) :: "memory");
		asm volatile ("setkgpr %0, %%9\n"  : "=r"(uregs.r9) :: "memory");
		asm volatile ("setkgpr %0, %%tp\n" : "=r"(uregs.tp) :: "memory");
		asm volatile ("setkgpr %0, %%11\n" : "=r"(uregs.r11) :: "memory");
		asm volatile ("setkgpr %0, %%12\n" : "=r"(uregs.r12) :: "memory");
		asm volatile ("setkgpr %0, %%sr\n" : "=r"(uregs.sr) :: "memory");
		asm volatile ("setkgpr %0, %%fp\n" : "=r"(uregs.fp) :: "memory");
		asm volatile ("setkgpr %0, %%rp\n" : "=r"(uregs.rp) :: "memory");
		asm volatile ("getuip %0\n"        : "=r"(uregs.pc) :: "memory");
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
		"%sr8(0x%lx) r9(0x%lx) tp(0x%lx) r11(0x%lx) r12(0x%lx) sr(0x%lx) fp(0x%lx)\n", loglvl,
		regs->r8, regs->r9, regs->tp, regs->r11, regs->r12, regs->sr, regs->fp);
}

void show_regs (struct pt_regs *regs) {
	return __show_regs (regs, KERN_DEFAULT);
}

__attribute__((__noinline__)) // __attribute__((__noinline__)) forces %rp to be saved on the stack.
static void stacktrace (unsigned long *start, unsigned long *end, const char *loglvl) {
	while (start < end) {
		unsigned long addr = *start;
		extern char _text[];
		if (__kernel_text_address(addr) && (addr >= (unsigned long)_text) &&
			(*(unsigned char *)(addr-2) == 0xd2 /* jl */))
			printk ("%s0x%08lx 0x%08lx %pS\n", loglvl,
				(unsigned long)start, addr, (void *)addr);
		++start;
	}
}

__attribute__((__noinline__)) // __attribute__((__noinline__)) forces %rp to be saved on the stack.
void show_stack (struct task_struct *tsk, unsigned long *sp, const char *loglvl) {
	#ifdef CONFIG_SMP
	static DEFINE_SPINLOCK(show_stack_lock);
	spin_lock(&show_stack_lock);
	#endif
	struct thread_info *ti = (tsk ? task_thread_info(tsk) : current_thread_info());
	unsigned long is_current_ti = (ti == current_thread_info());
	unsigned long ksp = ti->ksp;
	unsigned iskthread = (ti->task->flags&(PF_KTHREAD | PF_IO_WORKER));
	printk ("%s---- begin:show_stack(%s):cpu(%u:%u):pid(%u):comm(%s)%s\n", loglvl,
		(is_current_ti ? "current" : ""),
		ti->cpu, raw_smp_processor_id(), ti->task->pid, ti->task->comm,
		iskthread ? ":kthread" : "");
	struct pu32_pt_regs *eos;
	if (sp) {
		eos = (struct pu32_pt_regs *)pu32_stack_bottom(sp);
		printk ("%sstacktrace(0x%x):\n", loglvl, (unsigned)sp);
		stacktrace (sp, (unsigned long *)eos, loglvl);
	}
	asm volatile ("cpy %0, %%sp\n" : "=r"(sp) :: "memory");
	if (pu32_stack_top(sp) == pu32_kernelmode_stack[raw_smp_processor_id()]) {
		printk ("%sstacktrace(kmode):\n", loglvl);
		stacktrace (sp, (unsigned long *)pu32_stack_bottom(sp), loglvl);
		unsigned long pu32_ret_to_kernelspace (struct thread_info *ti);
		if (is_current_ti && (iskthread || pu32_ret_to_kernelspace(ti))) {
			asm volatile ("setkgpr %0, %%sp\n" : "=r"(sp) :: "memory");
			if (pu32_stack_top(sp) != pu32_stack_top(ksp)) {
				printk ("%s!!! stack_top(sp(0x%x)) != stack_top(ksp(0x%x)) !!!\n",
					loglvl, (unsigned)sp, (unsigned)ksp);
				sp = (unsigned long *)ksp;
			}
		} else
			sp = (unsigned long *)ksp;
	} else if (pu32_stack_top(sp) != pu32_stack_top(ksp)) {
		eos = (struct pu32_pt_regs *)pu32_stack_bottom(sp);
		printk ("%sstacktrace(0x%x):\n", loglvl, (unsigned)sp);
		stacktrace (sp, (unsigned long *)eos, loglvl);
		sp = (unsigned long *)ksp;
	}
	eos = (struct pu32_pt_regs *)pu32_stack_bottom(sp);
	if ((unsigned long)sp < ksp) {
		printk ("%sstacktrace:\n", loglvl);
		stacktrace (sp, (unsigned long *)ksp, loglvl);
	}
	while (1) {
		if (iskthread) {
			// Unlike a kernel-thread, a user-thread would always have a saved context.
			if (ksp == (unsigned long)(eos-1))
				break;
		} else if (ksp == (unsigned long)eos)
			break;
		struct pu32_pt_regs *ppr = (struct pu32_pt_regs *)ksp;
		printk ("%s%s: %s\n", loglvl,
			pu32faultreasonstr (ppr->faultreason, (ppr->regs.pc&1)),
			pu32sysopcodestr (ppr->sysopcode));
		__show_regs(&ppr->regs, loglvl);
		sp = (unsigned long *)(ksp + sizeof(struct pu32_pt_regs));
		ksp = ((ksp & ~(THREAD_SIZE - 1)) +
			((struct pu32_pt_regs *)ksp)->prev_ksp_offset);
		printk ("%sstacktrace:\n", loglvl);
		stacktrace (sp, (unsigned long *)ksp, loglvl);
	}
	printk ("%s---- end:show_stack()\n", loglvl);
	#ifdef CONFIG_SMP
	spin_unlock(&show_stack_lock);
	#endif
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
		case PTRACE_GET_THREAD_AREA:
			ret = put_user(task_pt_regs(child)->tp, (unsigned long __user *)data);
			break;
		default:
			ret = ptrace_request(child, request, addr, data);
			break;
		}
	return ret;
}

static int pu32_regset_get (
	struct task_struct *target,
	const struct user_regset *regset,
	struct membuf to) {
	return membuf_write(&to, task_pt_regs(target), sizeof(struct pt_regs));
}

static int pu32_regset_set (
	struct task_struct *target,
	const struct user_regset *regset,
	unsigned int pos, unsigned int count,
	const void *kbuf, const void __user *ubuf) {
	struct pt_regs *regs = task_pt_regs(target);
	return user_regset_copyin(&pos, &count, &kbuf, &ubuf, regs, 0, -1);
}

static const struct user_regset pu32_regsets[] = {
	[0] = {
		.core_note_type = NT_PRSTATUS,
		.n = ELF_NGREG,
		.size = sizeof(elf_greg_t),
		.align = sizeof(elf_greg_t),
		.regset_get = pu32_regset_get,
		.set = pu32_regset_set,
	},
};

static const struct user_regset_view user_pu32_view = {
	.name           = "pu32",
	.e_machine      = ELF_ARCH,
	.ei_osabi       = ELF_OSABI,
	.regsets        = pu32_regsets,
	.n              = ARRAY_SIZE(pu32_regsets)
};

const struct user_regset_view *task_user_regset_view (struct task_struct *task) {
	return &user_pu32_view;
}
