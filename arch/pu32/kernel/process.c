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

// Buffer used in various locations with snprintf().
char pu32strbuf[PU32STRBUFSZ];

unsigned long pu32_ret_to_kernelspace (struct thread_info *ti) {
	/* To be used after save_pu32umode_regs() or before restore_pu32umode_regs().
	   When checking whether the task returns to userspace,
	   pu32_ret_to_userspace() must be used instead of (!pu32_ret_to_kernelspace()),
	   because it also checks whether the task is a kernel-thread. */ \
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
void ret_from_interrupt (void);
void ret_from_fork (void);
void ret_from_kernel_thread (void);

// Create a new thread by copying from an existing one:
// - clone_flags: flags.
// - usp: user-thread stack-pointer; function-pointer for kernel-thread.
// - arg: NULL for user-thread; argument for kernel-thread.
// - p: newly created task.
int copy_thread (
	unsigned long clone_flags, unsigned long usp,
	unsigned long arg, struct task_struct *p, unsigned long tls) {

	struct thread_info *pti = task_thread_info(p);

	if (p->flags & (PF_KTHREAD | PF_IO_WORKER)) {
		// Kernel-thread.

		// Additional sizeof(struct pu32_pt_regs) space
		// is allocated and used only if the thread ever become
		// a user-thread; it further gets modified by start_thread().
		struct pu32_pt_regs *eos = (struct pu32_pt_regs *)end_of_stack(p);
		struct pu32_pt_regs *ppr = eos-1;
		ppr->prev_ksp_offset = ((unsigned long)eos & (THREAD_SIZE - 1));

		unsigned long sp = (unsigned long)ppr;

		ppr -= 1;
		// ppr->faultreason will not be used by __NR_PU32_switch_to.
		ppr->prev_ksp_offset = (sp & (THREAD_SIZE - 1));
		ppr->which = PU32_PT_REGS_WHICH_SPFPRP;
		ppr->regs.sp = sp;
		ppr->regs.fp = 0;
		ppr->regs.rp = (unsigned long)ret_from_kernel_thread;
		// ppr->regs.pc will not be used by __NR_PU32_switch_to.

		pti->ksp = (unsigned long)ppr;

		pti->kr1 = arg;
		pti->kpc = usp;

		pti->pu32flags = PU32_FLAGS_KERNELSPACE;

	} else {
		// User-thread.

		struct pu32_pt_regs *ppr = (struct pu32_pt_regs *)pti->ksp;

		ppr->regs.r1 = 0; // Child gets zero as return value.
		if (usp) // Setup usermode stack if specified.
			ppr->regs.sp = usp;
		if (clone_flags & CLONE_SETTLS)
			ppr->regs.tp = tls;

		unsigned long sp = (unsigned long)ppr;

		ppr -= 1;
		// ppr->faultreason will not be used by __NR_PU32_switch_to.
		ppr->prev_ksp_offset = (sp & (THREAD_SIZE - 1));
		ppr->which = PU32_PT_REGS_WHICH_SPFPRP;
		ppr->regs.sp = sp;
		ppr->regs.fp = 0;
		ppr->regs.rp = (unsigned long)ret_from_fork;
		// ppr->regs.pc will not be used by __NR_PU32_switch_to.

		pti->ksp = (unsigned long)ppr;

		pti->pu32flags = PU32_FLAGS_USERSPACE;
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

unsigned long get_wchan (struct task_struct *tsk) {

	if (!tsk || tsk == current || task_is_running(tsk))
		return 0;

	if (!task_stack_page(tsk))
		return 0;

	return task_pt_regs(tsk)->pc;
}

void arch_cpu_idle (void) {
	raw_local_irq_enable();
	asm volatile ("halt");
}

int do_fault (unsigned long addr, pu32FaultReason faultreason);
void do_IRQ (unsigned long irq);
void pu32_timer_intr (void);

extern unsigned long loops_per_jiffy;

extern unsigned long pu32irqflags[NR_CPUS];

// Get set to the start of the pu32-kernelmode stack.
unsigned long pu32_kernelmode_stack[NR_CPUS];

#include <hwdrvintctrl.h>

// This function is the only piece of code
// that will execute in pu32-kernelmode after
// the first use of sysret; it is also where
// transition between userspace and kernelspace occurs.
// Within this function, pu32printf() must be
// used instead of printk() or pr_*() functions.
// All pu32core must run this function,
// as it does work common to all cores.
__attribute__((__noinline__,optimize("O1"))) void pu32ctxswitchhdlr (void) {

	struct task_struct *tsk = current_thread_info()->task;
	pu32_cpu_curr[raw_smp_processor_id()] = tsk;

	struct mm_struct *mm = tsk->active_mm;
	asm volatile (
		"setugpr %%tp, %%tp\n"
		"setugpr %%sp, %%sp\n"
		"setugpr %%fp, %%fp\n"
		"setugpr %%rp, %%rp\n"
		"cpy %%sr, %1\n"
		"setasid %0\n"
		"setflags %2\n"
		:: "r"(mm->context),
		   "r"(mm->pgd),
		   "r"(PU32_FLAGS_KERNELSPACE |
			((pu32irqflags[raw_smp_processor_id()] == ARCH_IRQ_DISABLED) ?
			PU32_FLAGS_disIntr : 0)));
	asm goto ("setuip %0\n" :: "r"(&&return_label) : "memory" : return_label);

	/* Set up a different stack to be used by this function so that
	   it does not corrupt the stack of the task running in pu32-usermode.
	   In other words, we are setting up the stack to be used in pu32-kernelmode. */ {
		unsigned long osp; asm volatile ("cpy %0, %%sp" : "=r"(osp));
		unsigned long kmode_stack = (unsigned long)kmalloc(THREAD_SIZE, GFP_ATOMIC);
		pu32_kernelmode_stack[raw_smp_processor_id()] = kmode_stack;
		unsigned long pu32_stack_top_osp = pu32_stack_top(osp);
		unsigned long o = (osp - pu32_stack_top_osp);
		unsigned long nsp = kmode_stack + o;
		memcpy((void *)nsp, (void *)osp, (THREAD_SIZE - o));
		pu32_get_thread_info(nsp)->ksp -= (pu32_stack_top_osp - kmode_stack);
		asm volatile ("cpy %%sp, %0" :: "r"(nsp));
	}

	local_flush_tlb_all();

	extern unsigned long pu32_ishw;
	if (pu32_ishw) // Enable core to receive device interrupts.
		hwdrvintctrl_ack(raw_smp_processor_id(), 1);

	goto sysret;

	pu32FaultReason faultreason;
	unsigned long sysopcode;

	// Includes that can start being used only
	// after the core has been configured above.
	#include "hang.process.c"
	#include "regs.process.c"

	// current_thread_info() cannot be used
	// within this loop to retrieve "current"
	// because I could be coming from userspace;
	// pu32_cpu_curr[] is used instead.
	while (1) {

		struct task_struct *tsk = pu32_cpu_curr[raw_smp_processor_id()];
		struct thread_info *ti = task_thread_info(tsk);
		asm volatile ("cpy %%tp, %0" :: "r"(ti));

		switch (faultreason) {

			case pu32SysOpIntr: {

				switch (sysopcode&0xff) {

					case 0x01: /* syscall */ {
						unsigned syscallnr;
						asm volatile ("setkgpr %0, %%sr" : "=r"(syscallnr));

						if (pu32_in_userspace(ti)) {
							// Syscalls from userspace.

							if ((syscallnr >= __NR_syscalls) ||
								(syscallnr >= __NR_PU32_syscalls_start && syscallnr <= __NR_PU32_syscalls_end)) {

								asm volatile ("setugpr %%1, %0" :: "r"(-ENOSYS));
								asm volatile ("getuip %%sr; inc8 %%sr, 2; setuip %%sr":);
								goto sysret;

							} else switch (syscallnr) {

								// TODO: Implement fast syscall without
								// 	saving context for the following syscalls:
								// - __NR_gettid:
								// 	pid_t gettid(void); // Crucial for fast recursive locking ...
								// 	Implement also getpid() and getppid().
								// - __NR_clock_getres:
								// 	int clock_getres(clockid_t clock_id, struct timespec *res);
								// - __NR_clock_gettime:
								// 	int clock_gettime(clockid_t clock_id, struct timespec *tp);
								// - __NR_getcpu:
								// 	int getcpu(unsigned *cpu, unsigned *node, void *unused);
								// - __NR_gettimeofday:
								// 	int gettimeofday(struct timeval *tv, struct timezone *tz);
							}

							save_pu32umode_regs(ti, PU32_PT_REGS_WHICH_ALL, pu32SysOpIntr, sysopcode);

							asm volatile ("setugpr %%tp, %0" :: "r"(ti));
							asm volatile ("setugpr %%sp, %0" :: "r"(ti->ksp));
							asm volatile ("setugpr %%rp, %0" :: "r"(ret_from_syscall));
							asm volatile ("setuip %0" :: "r"(syscall_table[syscallnr]));
							struct mm_struct *mm = tsk->active_mm;
							asm volatile (
								"cpy %%sr, %1\n"
								"setasid %0\n"
								:: "r"(mm->context),
								   "r"(mm->pgd));
							asm volatile (
								"setflags %0\n"
								:: "r"(PU32_FLAGS_KERNELSPACE /* ARCH_IRQ_ENABLED assumed since from userspace *//*|
									((pu32irqflags[raw_smp_processor_id()] == ARCH_IRQ_DISABLED) ?
										PU32_FLAGS_disIntr : 0)*/));

							goto sysret;

						} else switch (syscallnr) {

							case __NR_PU32_switch_to: {
								// %1: struct task_struct *prev;
								// %2: struct task_struct *next;

								// prev is null when this syscall
								// was not invoked from __switch_to().

								struct task_struct *prev;
								asm volatile ("setkgpr %0, %%1" : "=r"(prev));
								struct task_struct *next;
								asm volatile ("setkgpr %0, %%2" : "=r"(next));

								pu32_cpu_curr[raw_smp_processor_id()] = next;

								struct thread_info *next_ti = task_thread_info(next);

								unsigned long prev_pc, next_r1, next_pc;
								pu32FaultReason next_faultreason;

								if (prev) {

									save_pu32umode_regs(
										task_thread_info(prev),
										PU32_PT_REGS_WHICH_ALL /* PU32_PT_REGS_WHICH_SPFPRP */,
										pu32SysOpIntr, sysopcode);

									// Capture prev_pc after save_pu32umode_regs().
									prev_pc = task_pt_regs(prev)->pc;

									next_faultreason = pu32SysOpIntr; // __switch_to() uses syscall.

								} else {
									// Capture next_r1, next_pc and next_faultreason
									// before restore_pu32umode_regs().
									struct pu32_pt_regs *pu32_next_ti_pt_regs =
										pu32_ti_pt_regs(next_ti);
									next_faultreason = pu32_next_ti_pt_regs->faultreason;
									struct pt_regs *next_pt_regs = &pu32_next_ti_pt_regs->regs;
									next_r1 = next_pt_regs->r1;
									next_pc = next_pt_regs->pc;
								}

								if (next_ti->preempt_count == PREEMPT_ENABLED)
									raw_local_irq_enable();

								// Note that %1 and %pc are not restored.
								restore_pu32umode_regs(next_ti);

								// Must be done after restore_pu32umode_regs().
								unsigned long next_ti_in_userspace =
									(prev ? 0 : pu32_in_userspace(next_ti));

								if (!next_ti_in_userspace)
									asm volatile ("setugpr %%tp, %0" :: "r"(next_ti));

								struct mm_struct *next_mm = next->active_mm;
								unsigned long asid = (next_mm->context|(next_ti_in_userspace<<12));
								unsigned long pu32flags = (((prev || !next_ti_in_userspace) ?
									PU32_FLAGS_KERNELSPACE : next_ti->pu32flags) |
										((pu32irqflags[raw_smp_processor_id()] == ARCH_IRQ_DISABLED) ?
											PU32_FLAGS_disIntr : 0));

								#ifdef CONFIG_SMP
								if (next_ti->last_cpu != raw_smp_processor_id()) {
									local_flush_tlb_mm(next_mm);
									next_ti->last_cpu = raw_smp_processor_id();
								}
								#endif

								asm volatile (
									"cpy %%sr, %4\n"
									"setasid %0\n"
									"setflags %1\n"
									"setugpr %%1, %2\n"
									"setuip %3\n"
									:: "r"(asid), "r"(pu32flags),
									   "r"((unsigned long)prev ?: next_r1),
									   "r"((prev ? prev_pc : next_pc) +
										((next_faultreason == pu32SysOpIntr) ? sizeof(uint16_t) : 0)),
									   "r"(next_mm->pgd));

								if (next_ti_in_userspace) {
									#if defined(CONFIG_PU32_DEBUG)
									if (pu32irqflags[raw_smp_processor_id()] == ARCH_IRQ_DISABLED)
										pu32printf("returning to userspace with interrupts disabled !!!\n");
									#endif
								}

								goto sysret;
							}

							case __NR_write: {

								int fd; asm volatile ("setkgpr %0, %%1" : "=r"(fd));
								char *s; asm volatile ("setkgpr %0, %%2" : "=r"(s));
								size_t n; asm volatile ("setkgpr %0, %%3" : "=r"(n));
								asm volatile ("setugpr %%1, %0" :: "r"(pu32syswrite(fd, s, n)));
								asm volatile ("getuip %%sr; inc8 %%sr, 2; setuip %%sr":);

								goto sysret;
							}

							case __NR_read: {

								int fd; asm volatile ("setkgpr %0, %%1" : "=r"(fd));
								char *s; asm volatile ("setkgpr %0, %%2" : "=r"(s));
								size_t n; asm volatile ("setkgpr %0, %%3" : "=r"(n));
								asm volatile ("setugpr %%1, %0" :: "r"(pu32sysread(fd, s, n)));
								asm volatile ("getuip %%sr; inc8 %%sr, 2; setuip %%sr":);

								goto sysret;
							}

							case __NR_lseek: {

								int fd; asm volatile ("setkgpr %0, %%1" : "=r"(fd));
								off_t offset; asm volatile ("setkgpr %0, %%2" : "=r"(offset));
								int whence; asm volatile ("setkgpr %0, %%3" : "=r"(whence));
								asm volatile ("setugpr %%1, %0" :: "r"(pu32syslseek(fd, offset, whence)));
								asm volatile ("getuip %%sr; inc8 %%sr, 2; setuip %%sr":);

								goto sysret;
							}

							case __NR_exit: {

								asm volatile (
									"setkgpr %%1, %%1; li %%sr, %0; syscall\n"
									:: "i"(__NR_exit) : "%1");

								pu32hang ("hypercall __NR_exit failed\n");

								goto sysret; // pu32hang() will infinite-loop;
								             // so this will not be executed, but
								             // it is left in place for completeness.
							}
						}

						asm volatile ("setugpr %%1, %0" :: "r"(-ENOSYS));
						unsigned pc; asm volatile ("getuip %0" : "=r"(pc));
						pu32hang ("Invalid syscallnr 0x%x @ 0x%x\n", syscallnr, pc);
						goto sysret; // pu32hang() will infinite-loop;
						             // so this will not be executed, but
						             // it is left in place for completeness.
					}

					default: {

						#if defined(CONFIG_PU32_DEBUG)
						if (ti->in_fault) {
							unsigned pc; asm volatile ("getuip %0" : "=r"(pc));
							pu32hang ("%s: recursive @ 0x%x\n",
								pu32faultreasonstr (faultreason, 1),
								pc);
						}
						#endif

						save_pu32umode_regs(ti, PU32_PT_REGS_WHICH_ALL, pu32SysOpIntr, sysopcode);

						asm volatile ("setugpr %%tp, %0" :: "r"(ti));
						asm volatile ("setugpr %%sp, %0" :: "r"(ti->ksp));
						// Execute do_fault(pu32_ti_pt_regs(ti)->regs.pc, pu32SysOpIntr)
						asm volatile ("setugpr %%1, %0" :: "r"(pu32_ti_pt_regs(ti)->regs.pc));
						asm volatile ("setugpr %%2, %0" :: "r"(pu32SysOpIntr));
						asm volatile ("setugpr %%rp, %0" :: "r"(ret_from_interrupt));
						asm volatile ("setuip %0" :: "r"(do_fault));
						struct mm_struct *mm = tsk->active_mm;
						asm volatile (
							"cpy %%sr, %1\n"
							"setasid %0\n"
							:: "r"(mm->context),
							   "r"(mm->pgd));
						asm volatile (
							"setflags %0\n"
							:: "r"(PU32_FLAGS_KERNELSPACE | PU32_FLAGS_disIntr));

						goto sysret;
					}
				}
			}

			case pu32ReadFaultIntr:
			case pu32WriteFaultIntr:
			case pu32ExecFaultIntr:
			case pu32AlignFaultIntr: {

				unsigned long faultaddr;
				asm volatile ("getfaultaddr %0" : "=r"(faultaddr));

				unsigned long pucap;
				asm volatile ("getcap %0" : "=r"(pucap));

				if (!(pucap & PU32_CAP_hptw) && faultreason != pu32AlignFaultIntr) {
					// Check whether an entry was already in
					// the tlb before walking the page table.
					unsigned long permtocheck = ((
						(faultreason == pu32ReadFaultIntr) ? _PAGE_READABLE :
						(faultreason == pu32WriteFaultIntr) ? _PAGE_WRITABLE :
						(faultreason == pu32ExecFaultIntr) ? _PAGE_EXECUTABLE : 0) |
							(pu32_in_userspace(ti) ? _PAGE_USER : 0));
					unsigned long tlblookup = pu32_tlb_lookup(faultaddr);
					if ((tlblookup & permtocheck) != permtocheck) {
						struct pu32tlbentry walk_page_table (void) {
							struct pu32tlbentry tlbentry = {.d1 = 0, .d2 = 0};
							struct mm_struct *mm = tsk->active_mm;
							pgd_t *pgd = mm->pgd + pgd_index(faultaddr);
							pmd_t *pmd = pmd_offset((pud_t *)pgd, faultaddr); // There is no pmd; this does pmd = pgd.
							pmd_t pmdval = *(volatile pmd_t *)pmd;
							if (!pmd_present(pmdval))
								goto out;
							pte_t pte = *(volatile pte_t *)((pte_t *)pmd_page_vaddr(pmdval) + pte_index(faultaddr));
							if (!pte_present(pte) || !(pte_val(pte) & permtocheck))
								goto out;
							tlbentry.d1 = (pte_val(pte) & ~(
								// Modify only either the itlb or dtlb.
								(faultreason == pu32ExecFaultIntr) ?
									(_PAGE_READABLE | _PAGE_WRITABLE) :
									_PAGE_EXECUTABLE));
							tlbentry.d2 = ((faultaddr&PAGE_MASK)|mm->context);
							out: return tlbentry;
						}
						struct pu32tlbentry tlbentry = walk_page_table();
						if ((tlbentry.d1 & permtocheck) == permtocheck) {
							pu32_tlb_update(tlbentry);
							goto sysret;
						}

					} else {
						// It is an unexpected fault if an entry was already in the tlb.
						unsigned pc; asm volatile ("getuip %0" : "=r"(pc));
						pu32hang ("%s: unexpected fault 0x%x @ 0x%x\n",
							pu32faultreasonstr(faultreason, (faultaddr == pc)),
							faultaddr, pc);
						goto sysret; // pu32hang() will infinite-loop;
						             // so this will not be executed, but
						             // it is left in place for completeness.
					}
				}

				#if defined(CONFIG_PU32_DEBUG)
				if (pu32irqflags[raw_smp_processor_id()] == ARCH_IRQ_DISABLED) {
					unsigned pc; asm volatile ("getuip %0" : "=r"(pc));
					pu32hang ("%s: unexpected interrupt 0x%x @ 0x%x\n",
						pu32faultreasonstr (faultreason, (faultaddr == pc)),
						faultaddr, pc);
					goto sysret; // pu32hang() will infinite-loop;
					             // so this will not be executed, but
					             // it is left in place for completeness.
				}

				if (ti->in_fault) {
					unsigned pc; asm volatile ("getuip %0" : "=r"(pc));
					pu32hang ("%s: recursive 0x%x @ 0x%x\n",
						pu32faultreasonstr (faultreason, (faultaddr == pc)),
						faultaddr, pc);
				}
				#endif

				save_pu32umode_regs(ti, PU32_PT_REGS_WHICH_ALL, faultreason, sysopcode);

				asm volatile ("setugpr %%tp, %0" :: "r"(ti));
				asm volatile ("setugpr %%sp, %0" :: "r"(ti->ksp));
				// Execute do_fault(faultaddr, faultreason)
				asm volatile ("setugpr %%1, %0" :: "r"(faultaddr));
				asm volatile ("setugpr %%2, %0" :: "r"(faultreason));
				asm volatile ("setugpr %%rp, %0" :: "r"(ret_from_interrupt));
				asm volatile ("setuip %0" :: "r"(do_fault));
				struct mm_struct *mm = tsk->active_mm;
				asm volatile (
					"cpy %%sr, %1\n"
					"setasid %0\n"
					:: "r"(mm->context),
					   "r"(mm->pgd));
				asm volatile (
					"setflags %0\n"
					:: "r"(PU32_FLAGS_KERNELSPACE | PU32_FLAGS_disIntr));

				goto sysret;
			}

			case pu32ExtIntr: {

				#if defined(CONFIG_PU32_DEBUG)
				if (pu32irqflags[raw_smp_processor_id()] == ARCH_IRQ_DISABLED) {
					pu32hang ("pu32ExtIntr: unexpected interrupt !!!\n");
					goto sysret; // pu32hang() will infinite-loop;
					             // so this will not be executed, but
					             // it is left in place for completeness.
				}
				#endif

				unsigned long irqsrc, ret;
				if (pu32_ishw) {
					if ((irqsrc = hwdrvintctrl_ack(raw_smp_processor_id(), 1)) == -2)
						goto skip_irq;
					ret = sizeof(unsigned long);
				} else
					ret = pu32sysread (PU32_BIOS_FD_INTCTRLDEV, &irqsrc, sizeof(unsigned long));

				if (irqsrc == -1)
					irqsrc = PU32_IPI_IRQ;

				if (ret == sizeof(unsigned long)) {
					// There are no saved interrupt context, but
					// set_irq_regs() must be called with a non-null value.
					struct pt_regs *old_regs = set_irq_regs((struct pt_regs *)-1);
					do_IRQ(irqsrc);
					set_irq_regs(old_regs);
				}

				skip_irq:;

				if (ti->preempt_count == PREEMPT_ENABLED && (ti->flags&_TIF_WORK_MASK)) {

					save_pu32umode_regs(ti, PU32_PT_REGS_WHICH_ALL, pu32ExtIntr, sysopcode);

					asm volatile ("setugpr %%tp, %0" :: "r"(ti));
					asm volatile ("setugpr %%sp, %0" :: "r"(ti->ksp));
					asm volatile ("setugpr %%1, %0" :: "r"(ti->flags));
					//asm volatile ("setugpr %%rp, %0" :: "r"());
					asm volatile ("setuip %0" :: "r"(ret_from_interrupt));
					struct mm_struct *mm = tsk->active_mm;
					asm volatile (
						"cpy %%sr, %1\n"
						"setasid %0\n"
						:: "r"(mm->context),
						   "r"(mm->pgd));
					asm volatile (
						"setflags %0\n"
						:: "r"(PU32_FLAGS_KERNELSPACE | PU32_FLAGS_disIntr));
				}

				goto sysret;
			}

			case pu32TimerIntr: {

				#if defined(CONFIG_PU32_DEBUG)
				if (pu32irqflags[raw_smp_processor_id()] == ARCH_IRQ_DISABLED) {
					pu32hang ("pu32TimerIntr: unexpected interrupt !!!\n");
					goto sysret; // pu32hang() will infinite-loop;
					             // so this will not be executed, but
					             // it is left in place for completeness.
				}
				#endif

				// There are no saved interrupt context, but
				// set_irq_regs() must be called with a non-null value.
				struct pt_regs *old_regs = set_irq_regs((struct pt_regs *)-1);
				pu32_timer_intr();
				set_irq_regs(old_regs);

				if (ti->preempt_count == PREEMPT_ENABLED && (ti->flags&_TIF_WORK_MASK)) {

					save_pu32umode_regs(ti, PU32_PT_REGS_WHICH_ALL, pu32TimerIntr, sysopcode);

					asm volatile ("setugpr %%tp, %0" :: "r"(ti));
					asm volatile ("setugpr %%sp, %0" :: "r"(ti->ksp));
					asm volatile ("setugpr %%1, %0" :: "r"(ti->flags));
					//asm volatile ("setugpr %%rp, %0" :: "r"());
					asm volatile ("setuip %0" :: "r"(ret_from_interrupt));
					struct mm_struct *mm = tsk->active_mm;
					asm volatile (
						"cpy %%sr, %1\n"
						"setasid %0\n"
						:: "r"(mm->context),
						   "r"(mm->pgd));
					asm volatile (
						"setflags %0\n"
						:: "r"(PU32_FLAGS_KERNELSPACE | PU32_FLAGS_disIntr));
				}

				goto sysret;
			}

			case pu32PreemptIntr: {

				#if defined(CONFIG_PU32_DEBUG)
				if (pu32irqflags[raw_smp_processor_id()] == ARCH_IRQ_DISABLED) {
					pu32hang ("pu32PreemptIntr: unexpected interrupt !!!\n");
					goto sysret; // pu32hang() will infinite-loop;
					             // so this will not be executed, but
					             // it is left in place for completeness.
				}
				#endif

				if (ti->preempt_count == PREEMPT_ENABLED) {

					ti->flags |= _TIF_NEED_RESCHED;

					save_pu32umode_regs(ti, PU32_PT_REGS_WHICH_ALL, pu32PreemptIntr, sysopcode);

					asm volatile ("setugpr %%tp, %0" :: "r"(ti));
					asm volatile ("setugpr %%sp, %0" :: "r"(ti->ksp));
					asm volatile ("setugpr %%1, %0" :: "r"(ti->flags));
					//asm volatile ("setugpr %%rp, %0" :: "r"());
					asm volatile ("setuip %0" :: "r"(ret_from_interrupt));
					struct mm_struct *mm = tsk->active_mm;
					asm volatile (
						"cpy %%sr, %1\n"
						"setasid %0\n"
						:: "r"(mm->context),
						   "r"(mm->pgd));
					asm volatile (
						"setflags %0\n"
						:: "r"(PU32_FLAGS_KERNELSPACE | PU32_FLAGS_disIntr));

				} else
					asm volatile ("halt");

				goto sysret;
			}

			default: {

				pu32hang ("Unexpected pu32FaultReason 0x%x\n", faultreason);
				goto sysret; // pu32hang() will infinite-loop;
				             // so this will not be executed, but
				             // it is left in place for completeness.
			}
		}

		return_label: asm volatile ("" ::: "memory"); return;

		sysret:;
		#ifdef HAVE_CONTEXT_TRACKING
		#ifndef CONFIG_CONTEXT_TRACKING
		#error CONFIG_CONTEXT_TRACKING not defined
		#endif
		#endif
		#ifdef CONFIG_CONTEXT_TRACKING
		void context_tracking_user_enter(void);
		void context_tracking_user_exit(void);
		unsigned long ti_in_userspace;
		if ((ti_in_userspace = pu32_in_userspace(ti)))
			context_tracking_user_enter();
		#endif
		asm volatile (
			"sysret\n"
			"getfaultreason %0\n"
			"getsysopcode %1\n"
			:  "=r"(faultreason),
			   "=r"(sysopcode));
		#ifdef CONFIG_CONTEXT_TRACKING
		if (ti_in_userspace)
			context_tracking_user_exit();
		#endif
	}
}

// Linux declares arch_call_rest_init() with
// the attribute __init, which means the memory
// that it occupies lives between __init_begin
// and __init_end and gets freed.
void arch_call_rest_init (void) {
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
	unsigned long addr;
	extern char __pu32tramp_start[], __pu32tramp_end[];
	for (addr = (unsigned long)__pu32tramp_start; addr < (unsigned long)__pu32tramp_end; addr += PAGE_SIZE)
		add_pte(addr, _PAGE_PRESENT | _PAGE_USER | _PAGE_CACHED | _PAGE_EXECUTABLE);
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
