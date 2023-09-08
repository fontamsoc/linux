// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#include <linux/ptrace.h>
#include <linux/start_kernel.h>

#include <asm/ptrace.h>
#include <asm/switch_to.h>
#include <asm/thread_info.h>
#include <asm/tlbflush.h>
#include <asm/irq_regs.h>
#include <asm/irq.h>

#include <pu32.h>

#include <hwdrvintctrl.h>

int do_fault (unsigned long addr, pu32FaultReason faultreason);
void do_IRQ (unsigned long irq);
void pu32_timer_intr (void);

extern unsigned long loops_per_jiffy;

extern unsigned long pu32irqflags[NR_CPUS];
extern unsigned long pu32hwflags[NR_CPUS];

// Get set to the start of the pu32-kernelmode stack.
unsigned long pu32_kernelmode_stack[NR_CPUS];

// Store the task_struct pointer of the task
// currently running on a CPU.
// PER_CPU variables are created in section bss,
// hence this variable will get zeroed.
// ### This wouldn't be needed if it was possible
// to use cpu_curr() to retrieve the task_struct
// currently running on a CPU.
struct task_struct *pu32_cpu_curr[NR_CPUS];

extern unsigned long pu32_ishw;

// Implemented in kernel/entry.S .
void ret_from_syscall (void);
void ret_from_exception (void);

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

static void pu32sysrethdlr_sysOpIntr (unsigned long sysopcode) {

	struct thread_info *ti = current_thread_info();
	struct task_struct *tsk = ti->task;

	switch (sysopcode&0xff) {

		case 0x01: /* syscall */ {
			unsigned syscallnr;
			asm volatile ("setkgpr %0, %%sr\n" : "=r"(syscallnr) :: "memory");

			if (pu32_in_userspace(ti)) {
				// Syscalls from userspace.

				if ((syscallnr >= __NR_syscalls) ||
					(syscallnr >= __NR_PU32_syscalls_start &&
						syscallnr <= __NR_PU32_syscalls_end)) {

					asm volatile (
						"setugpr %%1, %0\n"
						"getuip %%sr\n"
						"inc8 %%sr, 2\n"
						"setuip %%sr\n" ::
						"r"(-ENOSYS) :
						"memory");

					return;
				}
				#if 0
				else switch (syscallnr) {

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
				#endif

				save_pu32umode_regs(ti, PU32_PT_REGS_WHICH_ALL, pu32SysOpIntr, sysopcode);

				unsigned long hwflags = pu32hwflags[raw_smp_processor_id()];
				hwflags &= ~PU32_FLAGS_USERSPACE;
				hwflags |= PU32_FLAGS_KERNELSPACE;
				pu32hwflags[raw_smp_processor_id()] = hwflags;

				asm volatile ("setugpr %%tp, %0\n" :: "r"(ti) : "memory");
				asm volatile ("setugpr %%sp, %0\n" :: "r"(ti->ksp) : "memory");
				asm volatile ("setugpr %%rp, %0\n" :: "r"(ret_from_syscall) : "memory");
				if (test_thread_flag(TIF_SYSCALL_TRACE)) {
					long do_syscall_trace (void) {
						struct pt_regs *regs = &pu32_tsk_pt_regs(current)->regs;
						if (ptrace_report_syscall_entry(regs))
							return -ENOSYS;
						typedef long (*sys_call_fn)(unsigned long, unsigned long,
							unsigned long, unsigned long, unsigned long, unsigned long);
						sys_call_fn syscall_fn;
						syscall_fn = syscall_table[regs->sr];
						return syscall_fn(regs->r1, regs->r2, regs->r3, regs->r4, regs->r5, regs->r6);
					}
					asm volatile ("setuip %0\n" :: "r"(do_syscall_trace) : "memory");
				} else
					asm volatile ("setuip %0\n" :: "r"(syscall_table[syscallnr]) : "memory");
				struct mm_struct *mm = tsk->active_mm;
				asm volatile (
					"cpy %%sr, %1\n"
					"setasid %0\n" ::
					"r"(mm->context),
					"r"(mm->pgd) :
					"memory");
				asm volatile ("setflags %0\n" :: "r"(hwflags) : "memory");

				return;

			} else switch (syscallnr) {

				case __NR_PU32_switch_to: {
					// %1: struct task_struct *prev;
					// %2: struct task_struct *next;

					// prev is null when this syscall
					// was not invoked from __switch_to().

					struct task_struct *prev;
					asm volatile ("setkgpr %0, %%1\n" : "=r"(prev) :: "memory");
					struct task_struct *next;
					asm volatile ("setkgpr %0, %%2\n" : "=r"(next) :: "memory");

					struct thread_info *next_ti = task_thread_info(next);

					unsigned long prev_pc, next_r1, next_pc;
					pu32FaultReason next_faultreason;

					if (prev) {
						#if defined(CONFIG_PU32_DEBUG)
						if (prev != tsk) {
							pu32hang ("__NR_PU32_switch_to(): prev(0x%x:%u:%s) != tsk(0x%x:%u:%s)\n",
								prev, prev->pid, prev->comm,
								tsk, tsk->pid, tsk->comm);
							return;
							// pu32hang() will infinite-loop;
							// so `return` will not be executed, but
							// it is left in place for completeness.
						}
						#endif
						save_pu32umode_regs(
							task_thread_info(prev),
							PU32_PT_REGS_WHICH_SPFPRP,
							pu32SysOpIntr, sysopcode);
						// Capture prev_pc after save_pu32umode_regs().
						prev_pc = pu32_tsk_pt_regs(prev)->regs.pc;
						next_faultreason = pu32SysOpIntr; // __switch_to() uses syscall.

						pu32_cpu_curr[raw_smp_processor_id()] = next;
						asm volatile ("cpy %%tp, %0\n" :: "r"(next_ti) : "memory");
						ti = next_ti;
						tsk = next;

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

					// Note that %1 and %pc are not restored.
					restore_pu32umode_regs(next_ti);

					// Must be done after restore_pu32umode_regs().
					unsigned long next_ti_in_userspace =
						(prev ? 0 : pu32_in_userspace(next_ti));

					if (!next_ti_in_userspace)
						asm volatile ("setugpr %%tp, %0\n" :: "r"(next_ti) : "memory");

					struct mm_struct *next_mm = next->active_mm;
					unsigned long asid = (next_mm->context|(next_ti_in_userspace<<12));

					unsigned long hwflags = pu32hwflags[raw_smp_processor_id()];
					if (prev || !next_ti_in_userspace) {
						hwflags &= ~PU32_FLAGS_USERSPACE;
						hwflags |= PU32_FLAGS_KERNELSPACE;
					} else {
						hwflags &= ~PU32_FLAGS_KERNELSPACE;
						hwflags |= PU32_FLAGS_USERSPACE;
					}
					pu32hwflags[raw_smp_processor_id()] = hwflags;

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
						"setuip %3\n" ::
						"r"(asid),
						"r"(hwflags),
						"r"((unsigned long)prev ?: next_r1),
						"r"((prev ? prev_pc : next_pc) +
							((next_faultreason == pu32SysOpIntr) ?
								sizeof(uint16_t) : 0)),
						"r"(next_mm->pgd) :
						"memory");

					if (next_ti_in_userspace) {
						#if defined(CONFIG_PU32_DEBUG)
						if (pu32irqflags[raw_smp_processor_id()] == ARCH_IRQ_DISABLED)
							pu32printf("returning to userspace with interrupts disabled !!!\n");
						#endif
					}

					return;
				}

				case __NR_write: {

					int fd; asm volatile ("setkgpr %0, %%1\n" : "=r"(fd) :: "memory");
					char *s; asm volatile ("setkgpr %0, %%2\n" : "=r"(s) :: "memory");
					size_t n; asm volatile ("setkgpr %0, %%3\n" : "=r"(n) :: "memory");
					asm volatile ("setugpr %%1, %0\n" :: "r"(pu32syswrite(fd, s, n)) : "memory");
					asm volatile ("getuip %%sr; inc8 %%sr, 2; setuip %%sr\n" ::: "memory");

					return;
				}

				case __NR_read: {

					int fd; asm volatile ("setkgpr %0, %%1\n" : "=r"(fd) :: "memory");
					char *s; asm volatile ("setkgpr %0, %%2\n" : "=r"(s) :: "memory");
					size_t n; asm volatile ("setkgpr %0, %%3\n" : "=r"(n) :: "memory");
					asm volatile ("setugpr %%1, %0\n" :: "r"(pu32sysread(fd, s, n)) : "memory");
					asm volatile ("getuip %%sr; inc8 %%sr, 2; setuip %%sr\n" ::: "memory");

					return;
				}

				case __NR_lseek: {

					int fd; asm volatile ("setkgpr %0, %%1\n" : "=r"(fd) :: "memory");
					off_t offset; asm volatile ("setkgpr %0, %%2\n" : "=r"(offset) :: "memory");
					int whence; asm volatile ("setkgpr %0, %%3\n" : "=r"(whence) :: "memory");
					asm volatile ("setugpr %%1, %0\n" :: "r"(pu32syslseek(fd, offset, whence)) : "memory");
					asm volatile ("getuip %%sr; inc8 %%sr, 2; setuip %%sr\n" ::: "memory");

					return;
				}

				case __NR_exit: {

					asm volatile (
						"setkgpr %%1, %%1; li %%sr, %0; syscall\n" ::
						"i"(__NR_exit) :
						"%1", "memory");

					pu32hang ("hypercall __NR_exit failed\n");
					return;
					// pu32hang() will infinite-loop;
					// so `return` will not be executed, but
					// it is left in place for completeness.
				}
			}

			asm volatile ("setugpr %%1, %0\n" :: "r"(-ENOSYS) : "memory");
			unsigned pc; asm volatile ("getuip %0\n" : "=r"(pc) :: "memory");

			pu32hang ("Invalid syscallnr 0x%x @ 0x%x\n", syscallnr, pc);
			return;
			// pu32hang() will infinite-loop;
			// so `return` will not be executed, but
			// it is left in place for completeness.
		}

		default: {

			#if defined(CONFIG_PU32_DEBUG)
			if (ti->in_fault) {
				unsigned pc; asm volatile ("getuip %0\n" : "=r"(pc) :: "memory");
				pu32hang ("pu32SysOpIntr: recursive @ 0x%x\n", pc);
				return;
				// pu32hang() will infinite-loop;
				// so `return` will not be executed, but
				// it is left in place for completeness.
			}
			#endif

			save_pu32umode_regs(ti, PU32_PT_REGS_WHICH_ALL, pu32SysOpIntr, sysopcode);

			unsigned long hwflags = pu32hwflags[raw_smp_processor_id()];
			hwflags &= ~PU32_FLAGS_USERSPACE;
			hwflags |= PU32_FLAGS_KERNELSPACE;
			pu32hwflags[raw_smp_processor_id()] = hwflags;

			asm volatile ("setugpr %%tp, %0\n" :: "r"(ti) : "memory");
			asm volatile ("setugpr %%sp, %0\n" :: "r"(ti->ksp) : "memory");
			// Execute do_fault(pu32_ti_pt_regs(ti)->regs.pc, pu32SysOpIntr)
			asm volatile ("setugpr %%1, %0\n" :: "r"(pu32_ti_pt_regs(ti)->regs.pc) : "memory");
			asm volatile ("setugpr %%2, %0\n" :: "r"(pu32SysOpIntr) : "memory");
			asm volatile ("setugpr %%rp, %0\n" :: "r"(ret_from_exception) : "memory");
			asm volatile ("setuip %0\n" :: "r"(do_fault) : "memory");
			struct mm_struct *mm = tsk->active_mm;
			asm volatile (
				"cpy %%sr, %1\n"
				"setasid %0\n" ::
				"r"(mm->context),
				"r"(mm->pgd) :
				"memory");
			asm volatile ("setflags %0\n" :: "r"(hwflags) : "memory");

			return;
		}
	}
}

static void pu32sysrethdlr_faultIntr (pu32FaultReason faultreason, unsigned long sysopcode) {

	struct thread_info *ti = current_thread_info();
	struct task_struct *tsk = ti->task;

	unsigned long faultaddr;
	asm volatile ("getfaultaddr %0\n" : "=r"(faultaddr) :: "memory");

	unsigned long pucap;
	asm volatile ("getcap %0\n" : "=r"(pucap) :: "memory");

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
				return;
			}

		} else {
			// It is an unexpected fault if an entry was already in the tlb.
			unsigned pc; asm volatile ("getuip %0\n" : "=r"(pc) :: "memory");
			pu32hang ("%s: unexpected fault 0x%x @ 0x%x\n",
				pu32faultreasonstr(faultreason, (faultaddr == pc)),
				faultaddr, pc);
			return; // pu32hang() will infinite-loop;
					// so this will not be executed, but
					// it is left in place for completeness.
		}
	}

	#if defined(CONFIG_PU32_DEBUG)
	if (pu32irqflags[raw_smp_processor_id()] == ARCH_IRQ_DISABLED) {
		unsigned pc; asm volatile ("getuip %0\n" : "=r"(pc) :: "memory");
		pu32hang ("%s: unexpected interrupt 0x%x @ 0x%x\n",
			pu32faultreasonstr (faultreason, (faultaddr == pc)),
			faultaddr, pc);
		return; // pu32hang() will infinite-loop;
				// so this will not be executed, but
				// it is left in place for completeness.
	}

	if (ti->in_fault) {
		unsigned pc; asm volatile ("getuip %0\n" : "=r"(pc) :: "memory");
		pu32hang ("%s: recursive 0x%x @ 0x%x\n",
			pu32faultreasonstr (faultreason, (faultaddr == pc)),
			faultaddr, pc);
	}
	#endif

	save_pu32umode_regs(ti, PU32_PT_REGS_WHICH_ALL, faultreason, sysopcode);

	unsigned long hwflags = pu32hwflags[raw_smp_processor_id()];
	hwflags &= ~PU32_FLAGS_USERSPACE;
	hwflags |= PU32_FLAGS_KERNELSPACE;
	pu32hwflags[raw_smp_processor_id()] = hwflags;

	asm volatile ("setugpr %%tp, %0\n" :: "r"(ti) : "memory");
	asm volatile ("setugpr %%sp, %0\n" :: "r"(ti->ksp) : "memory");
	// Execute do_fault(faultaddr, faultreason)
	asm volatile ("setugpr %%1, %0\n" :: "r"(faultaddr) : "memory");
	asm volatile ("setugpr %%2, %0\n" :: "r"(faultreason) : "memory");
	asm volatile ("setugpr %%rp, %0\n" :: "r"(ret_from_exception) : "memory");
	asm volatile ("setuip %0\n" :: "r"(do_fault) : "memory");
	struct mm_struct *mm = tsk->active_mm;
	asm volatile (
		"cpy %%sr, %1\n"
		"setasid %0\n" ::
		"r"(mm->context),
		"r"(mm->pgd) :
		"memory");
	asm volatile ("setflags %0\n" :: "r"(hwflags) : "memory");
}

// Acknowledge interrupt and return its number.
// Must be very simple as it gets called from _inc_cpu_up_arg()
// which uses a very small stack.
unsigned long pu32_irq_ack (unsigned long en) {
	unsigned long irqsrc;
	if (pu32_ishw) {
		if ((irqsrc = hwdrvintctrl_ack(raw_smp_processor_id(), en)) == -2) {
			irqsrc = -1; // Should be an invalid interrupt number.
			goto skip_irq;
		}
	} else if (pu32sysread (PU32_BIOS_FD_INTCTRLDEV, &irqsrc, sizeof(unsigned long)) != sizeof(unsigned long)) {
		irqsrc = -1; // Should be an invalid interrupt number.
		goto skip_irq;
	}
	if (irqsrc == -1)
		irqsrc = PU32_IPI_IRQ;
	skip_irq:;
	return irqsrc;
}

static void pu32sysrethdlr_extIntr (unsigned long sysopcode) {
	#ifdef CONFIG_SMP
	if (!cpu_online(raw_smp_processor_id())) {
		pu32_irq_ack(0);
		kfree((void *)pu32_kernelmode_stack[raw_smp_processor_id()]);
		asm volatile ("j %0\n" :: "r"(PARKPU_ADDR) : "memory");
		// It should never reach here.
		BUG();
	}
	#endif

	struct thread_info *ti = current_thread_info();
	struct task_struct *tsk = ti->task;

	#if defined(CONFIG_PU32_DEBUG)
	if (pu32irqflags[raw_smp_processor_id()] == ARCH_IRQ_DISABLED) {
		pu32hang ("pu32ExtIntr: unexpected interrupt !!!\n");
		return;
		// pu32hang() will infinite-loop;
		// so `return` will not be executed, but
		// it is left in place for completeness.

	}
	#endif

	unsigned long irqsrc = pu32_irq_ack(1);
	if (irqsrc != -1) {
		do_IRQ(irqsrc);
	}

	if (ti->preempt_count == PREEMPT_ENABLED && (ti->flags&_TIF_WORK_MASK)) {

		save_pu32umode_regs(ti, PU32_PT_REGS_WHICH_ALL, pu32ExtIntr, sysopcode);

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

static void pu32sysrethdlr_preemptIntr (unsigned long sysopcode) {

	struct thread_info *ti = current_thread_info();
	struct task_struct *tsk = ti->task;

	#if defined(CONFIG_PU32_DEBUG)
	if (pu32irqflags[raw_smp_processor_id()] == ARCH_IRQ_DISABLED) {
		pu32hang ("pu32PreemptIntr: unexpected interrupt !!!\n");
		return;
		// pu32hang() will infinite-loop;
		// so `return` will not be executed, but
		// it is left in place for completeness.
	}
	#endif

	if (ti->preempt_count == PREEMPT_ENABLED) {

		ti->flags |= _TIF_NEED_RESCHED;

		save_pu32umode_regs(ti, PU32_PT_REGS_WHICH_ALL, pu32PreemptIntr, sysopcode);

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

__attribute__((__noinline__)) // used to force registers flushing.
static void pu32sysrethdlr (void) {

	pu32FaultReason faultreason;
	unsigned long sysopcode;
	asm volatile (
		"getfaultreason %0\n"
		"getsysopcode %1\n" :
		"=r"(faultreason),
		"=r"(sysopcode) ::
		"memory");

	switch (faultreason) {

		case pu32SysOpIntr:
			pu32sysrethdlr_sysOpIntr (sysopcode);
			break;

		case pu32ReadFaultIntr:
		case pu32WriteFaultIntr:
		case pu32ExecFaultIntr:
		case pu32AlignFaultIntr:
			pu32sysrethdlr_faultIntr (faultreason, sysopcode);
			break;

		case pu32ExtIntr:
			pu32sysrethdlr_extIntr (sysopcode);
			break;

		case pu32TimerIntr:
			pu32sysrethdlr_timerIntr (sysopcode);
			break;

		case pu32PreemptIntr:
			pu32sysrethdlr_preemptIntr (sysopcode);
			break;

		default:
			pu32hang ("Unexpected pu32FaultReason 0x%x\n", faultreason);
			break;
			// pu32hang() will infinite-loop;
			// so `return` will not be executed, but
			// it is left in place for completeness.
	}
}

__attribute__((__noinline__)) // used to force registers flushing.
static void pu32sysret (unsigned long _do) {
	if (!_do)
		return;
	for (;;) {
		#ifdef HAVE_CONTEXT_TRACKING_USER
		#ifndef CONFIG_CONTEXT_TRACKING_USER
		#error CONFIG_CONTEXT_TRACKING_USER not defined
		#endif
		#endif
		#ifdef CONFIG_CONTEXT_TRACKING_USER
		void user_enter_callable(void);
		void user_exit_callable(void);
		unsigned long ti_in_userspace;
		if ((ti_in_userspace = pu32_in_userspace(current_thread_info())))
			user_enter_callable();
		#endif
		asm volatile ("sysret\n" ::: "memory");
		asm volatile (
			"cpy %%tp, %0\n" ::
			"r"(task_thread_info(pu32_cpu_curr[raw_smp_processor_id()])) :
			"memory");
		#ifdef CONFIG_CONTEXT_TRACKING_USER
		if (ti_in_userspace)
			user_exit_callable();
		#endif
		pu32sysrethdlr();
	}
}

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
		pgd_t *pgd = init_mm.pgd + pgd_index(addr);
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
