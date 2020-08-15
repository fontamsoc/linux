// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

static void pu32sysrethdlr_sysOpIntr (
	unsigned long sysopcode) {

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
						prev_pc = task_pt_regs(prev)->pc;
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
