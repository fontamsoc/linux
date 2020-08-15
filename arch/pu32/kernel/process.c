// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#include <linux/ptrace.h>
#include <linux/preempt.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/start_kernel.h>
#include <linux/slab.h>

#include <asm/ptrace.h>
#include <asm/switch_to.h>
#include <asm/thread_info.h>
#include <asm/syscall.h>
#include <asm/tlbflush.h>
#include <asm/irq_regs.h>
#include <asm/irq.h>

#include <pu32.h>

unsigned long pu32_ret_to_kernelspace (struct thread_info *ti) {
	/* To be used after save_pu32umode_regs() or before restore_pu32umode_regs().
	   When checking whether the task returns to userspace,
	   pu32_ret_to_userspace() must be used instead of (!pu32_ret_to_kernelspace()),
	   because it also checks whether the task is a kernel-thread. */
	return (ti->ksp < (unsigned long)((struct pu32_pt_regs *)pu32_stack_bottom(ti->ksp)-1));
}

// Store the task_struct pointer of the task
// currently running on a CPU.
// PER_CPU variables are created in section bss,
// hence this variable will get zeroed.
// ### This wouldn't be needed if it was possible
// to use cpu_curr() to retrieve the task_struct
// currently running on a CPU.
struct task_struct *pu32_cpu_curr[NR_CPUS];

void pu32_save_syscall_retval (
	unsigned long syscall_retval) {
	struct thread_info *ti = current_thread_info();
	pu32_ti_pt_regs(ti)->regs.r1 = syscall_retval;
	return;
}

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
void ret_from_syscall (void);
void ret_from_exception (void);
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

	return task_pt_regs(tsk)->pc;
}

void arch_cpu_idle (void) {
	raw_local_irq_enable();
	asm volatile ("halt\n" ::: "memory");
}

int do_fault (unsigned long addr, pu32FaultReason faultreason);
void do_IRQ (unsigned long irq);
void pu32_timer_intr (void);

extern unsigned long loops_per_jiffy;

extern unsigned long pu32irqflags[NR_CPUS];
extern unsigned long pu32hwflags[NR_CPUS];

// Get set to the start of the pu32-kernelmode stack.
unsigned long pu32_kernelmode_stack[NR_CPUS];

extern unsigned long pu32_ishw;

#include <hwdrvintctrl.h>

#include "sysret.process.c"

// This function is the only piece of code
// that will execute in pu32-kernelmode after
// the first use of sysret; it is also where
// transition between userspace and kernelspace occurs.
// Within this function, pu32printf() must be
// used instead of printk() or pr_*() functions.
// All pu32core must run this function,
// as it does work common to all cores.
__attribute__((__noinline__))
void pu32ctxswitchhdlr (void) {
	// %sp %fp %rp must be captured before any function call modify them.
	asm volatile (
		"setugpr %%sp, %%sp\n"
		"setugpr %%fp, %%fp\n"
		"setugpr %%rp, %%rp\n" :::
		"memory");

	struct thread_info *ti = current_thread_info();
	struct task_struct *tsk = ti->task;

	pu32_cpu_curr[raw_smp_processor_id()] = tsk;

	raw_local_irq_disable();

	unsigned long hwflags = pu32hwflags[raw_smp_processor_id()];
	hwflags &= ~PU32_FLAGS_USERSPACE;
	hwflags |= PU32_FLAGS_KERNELSPACE;
	pu32hwflags[raw_smp_processor_id()] = hwflags;

	struct mm_struct *mm = tsk->active_mm;
	asm volatile (
		"setugpr %%tp, %%tp\n"
		"cpy %%sr, %1\n"
		"setasid %0\n"
		"setflags %2\n"
		"li8 %%sr, 0\n"
		"setugpr %%1, %%sr\n" // pu32sysret(0)
		"setuip %3\n" ::
		"r"(mm->context),
		"r"(mm->pgd),
		"r"(hwflags),
		"r"(pu32sysret) :
		"memory");

	/* Set up a different stack to be used by this function so that
	it does not corrupt the stack of the task running in pu32-usermode.
	In other words, we are setting up the stack to be used in pu32-kernelmode. */ {
		unsigned long osp; asm volatile ("cpy %0, %%sp\n" : "=r"(osp));
		unsigned long ofp; asm volatile ("cpy %0, %%fp\n" : "=r"(ofp));
		unsigned long kmode_stack = (unsigned long)kmalloc(THREAD_SIZE, GFP_ATOMIC);
		pu32_kernelmode_stack[raw_smp_processor_id()] = kmode_stack;
		unsigned long pu32_stack_top_osp = pu32_stack_top(osp);
		unsigned long o = (osp - pu32_stack_top_osp);
		unsigned long nsp = kmode_stack + o;
		memcpy((void *)nsp, (void *)osp, (THREAD_SIZE - o));
		pu32_get_thread_info(nsp)->ksp -= (pu32_stack_top_osp - kmode_stack);
		asm volatile ("cpy %%sp, %0\n" :: "r"(nsp));
		asm volatile ("cpy %%fp, %0\n" :: "r"(kmode_stack + (ofp - pu32_stack_top_osp)));
	}

	local_flush_tlb_all();

	if (pu32_ishw) // Enable core to receive device interrupts.
		hwdrvintctrl_ack(raw_smp_processor_id(), 1);

	pu32sysret(1);
}

// Linux declares arch_call_rest_init() with
// the attribute __init, which means the memory
// that it occupies lives between __init_begin
// and __init_end and gets freed.
__attribute__((optimize("O0"))) // used otherwise for some reason entire function is not compiled.
void __init arch_call_rest_init (void) {
	void add_pte (unsigned long addr, unsigned long prot) {
		pgd_t *pgd = swapper_pg_dir + pgd_index(addr);
		pmd_t *pmd = pmd_offset((pud_t *)pgd, addr); // There is no pmd; this does pmd = pgd.
		if (pmd_present(*pmd)) {
			pte_t pte = *pte_offset_map(pmd, addr);
			if (pte_present(pte)) // The mapping must not already exist.
				panic("add_pte: invalid pgd: pte already exist: 0x%x\n",
					(unsigned)pte_val(pte));
		} else
			(void)pte_alloc(&init_mm, pmd);
		set_pte_at(
			&init_mm, addr,
			pte_offset_map(pmd, addr),
			__pte((addr & PAGE_MASK) | prot));
	}
	add_pte(0, _PAGE_PRESENT | _PAGE_READABLE | _PAGE_WRITABLE);
	extern char __pu32tramp_start[], __pu32tramp_end[];
	for (
		unsigned long addr = (unsigned long)__pu32tramp_start;
		addr < (unsigned long)__pu32tramp_end;
		addr += PAGE_SIZE) {
		add_pte(addr, _PAGE_PRESENT | _PAGE_USER | _PAGE_CACHED | _PAGE_EXECUTABLE);
	}
	#if 0
	// ### Since setksl is used, these are no longer needed, but kept for future reference.
	for (addr = (unsigned long)_text; addr < (unsigned long)_etext; addr += PAGE_SIZE)
		add_pte(addr,
			((addr >= (unsigned long)__pu32tramp_start && addr < (unsigned long)__pu32tramp_end) ?
				_PAGE_USER : 0) | _PAGE_PRESENT | _PAGE_EXECUTABLE);
	for (addr = (unsigned long)__start_rodata; addr < (unsigned long)__end_rodata; addr += PAGE_SIZE)
		add_pte(addr, _PAGE_PRESENT | _PAGE_READABLE);
	extern char __start_rwdata[], __end_rwdata[];
	for (addr = (unsigned long)__start_rwdata; addr < (unsigned long)__end_rwdata; addr += PAGE_SIZE)
		add_pte(addr, _PAGE_PRESENT | _PAGE_READABLE | _PAGE_WRITABLE);
	#endif
	pu32ctxswitchhdlr();
	rest_init();
}
