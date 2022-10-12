// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

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
