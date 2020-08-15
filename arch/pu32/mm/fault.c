// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/sched/debug.h>
#include <linux/sched/task.h>

#include <asm/extable.h>

#include <pu32.h>

// return -1: could not map the page.
// return  0: pagefault handled, but no new mapping.
// return  1: pagefault handled, with new mapping.
int do_fault (unsigned long addr, pu32FaultReason faultreason) {

	struct task_struct *tsk = current;
	struct pt_regs *regs = task_pt_regs(tsk);
	struct thread_info *ti = task_thread_info(tsk);

	ti->in_fault = 1;
	ti->faultreason = faultreason;

	unsigned long faulted_in_userspace = pu32_ret_to_userspace(ti);

	int ret;

	switch (faultreason) {
		case pu32ReadFaultIntr:
		case pu32WriteFaultIntr:
		case pu32ExecFaultIntr:
			break;
		case pu32SysOpIntr:
			if (!faulted_in_userspace)
				goto exception;
			force_sig_fault(SIGILL, ILL_ILLOPC, (void *)addr);
			ret = -1;
			goto done;
		case pu32AlignFaultIntr:
			if (!faulted_in_userspace)
				goto exception;
			force_sig_fault(SIGBUS, BUS_ADRALN, (void *)addr);
			ret = -1;
			goto done;
		default:
			show_stack (NULL, NULL, KERN_DEFAULT);
			panic ("%s: unexpected %s @ 0x%x\n",
				__FUNCTION__,
				pu32faultreasonstr (
					faultreason, (addr == regs->pc)),
				(unsigned)regs->pc);
	}

	if (addr >= VMALLOC_START && addr <= VMALLOC_END && !faulted_in_userspace)
		goto vmalloc_fault;

	struct mm_struct *mm = tsk->mm;

	// If we are in an interrupt or have no user
	// context, we must not take the fault.
	if (faulthandler_disabled() || !mm) {
		if (!faulted_in_userspace)
			goto exception;
		// faulthandler_disabled() in usermode is
		// really bad, as is current->mm == NULL.
		pr_emerg ("bad pagefault; mm == 0x%x\n", (unsigned)mm);
		force_sig_fault(SIGSEGV, SEGV_BNDERR, (void *)addr);
		ret = -1;
		goto done;
	}

	unsigned int flags = FAULT_FLAG_DEFAULT;

	if (faulted_in_userspace)
		flags |= FAULT_FLAG_USER;

	int si_code = SEGV_MAPERR;

retry:;

	mmap_read_lock(mm);
	struct vm_area_struct *vma = find_vma(mm, addr);
	if (!vma)
		goto bad_area;
	if (vma->vm_start <= addr)
		goto good_area;
	if (!(vma->vm_flags & VM_GROWSDOWN))
		goto bad_area;
	if (faulted_in_userspace) {
		if ((addr + PAGE_SIZE) < regs->sp)
			goto bad_area;
	}
	if (expand_stack(vma, addr))
		goto bad_area;

good_area:;

	si_code = SEGV_ACCERR;

	switch (faultreason) {
		case pu32ReadFaultIntr:
			if (!(vma->vm_flags & VM_READ))
				goto bad_area;
			break;
		case pu32WriteFaultIntr:
			if (!(vma->vm_flags & VM_WRITE))
				goto bad_area;
			flags |= FAULT_FLAG_WRITE;
			break;
		case pu32ExecFaultIntr:
			if (!(vma->vm_flags & VM_EXEC))
				goto bad_area;
			flags |= FAULT_FLAG_INSTRUCTION;
			break;
		default: {
			show_stack (NULL, NULL, KERN_DEFAULT);
			panic ("%s: unexpected %s @ 0x%x\n",
				__FUNCTION__,
				pu32faultreasonstr (
					faultreason, (addr == regs->pc)),
				(unsigned)regs->pc);
		}
	}

	vm_fault_t fault = handle_mm_fault (vma, addr, flags, regs);

	if (fault_signal_pending(fault, regs)) {
		ret = -1;
		goto done;
	}

	if (fault & VM_FAULT_ERROR) {
		if (fault & VM_FAULT_OOM)
			goto out_of_memory;
		else if (fault & VM_FAULT_SIGBUS)
			goto do_sigbus;
		BUG();
	}

	// The fault is fully completed (including releasing mmap lock).
	if (fault & VM_FAULT_COMPLETED) {
		ret = 1;
		goto done;
	}

	if (fault & VM_FAULT_RETRY) {
		flags |= FAULT_FLAG_TRIED;
		/* No need to mmap_read_unlock(mm) as we would
		   have already released it in __lock_page_or_retry
		   in mm/filemap.c. */
		goto retry;
	}

	mmap_read_unlock(mm);

	ret = 1;
	goto done;

bad_area:;

	mmap_read_unlock(mm);

	if (faulted_in_userspace) {
		force_sig_fault(SIGSEGV, si_code, (void *)addr);
		ret = -1;
		goto done;
	}

exception:;

	if (fixup_exception(regs)) {
		ret = 0;
		goto done;
	}

	if (!(tsk->flags&(PF_KTHREAD | PF_IO_WORKER))) {
		force_sig_fault(SIGSEGV, SEGV_MAPERR, (void *)addr);
		ret = -1;
		goto done;
	}

	bust_spinlocks(1);
	console_verbose();
	show_stack (NULL, NULL, KERN_DEFAULT);
	panic ("unable to handle %s @ 0x%lx\n",
		pu32faultreasonstr(faultreason, (addr == regs->pc)),
		addr);
	make_task_dead(SIGKILL);

out_of_memory:;

	mmap_read_unlock(mm);
	pagefault_out_of_memory();
	ret = -1;
	goto done;

do_sigbus:;

	mmap_read_unlock(mm);
	if (!faulted_in_userspace)
		goto exception;
	force_sig_fault(SIGBUS, BUS_ADRERR, (void *)addr);
	ret = -1;
	goto done;

vmalloc_fault:;

	int index = pgd_index(addr);

	pgd_t *pgd_k = init_mm.pgd + index;

	pmd_t *pmd_k = pmd_offset((pud_t *)pgd_k, addr); // There is no pmd; this does pmd_k = pgd_k.
	if (!pmd_present(*pmd_k))
		goto exception;

	// Make sure the actual PTE exists as well to
	// catch kernel vmalloc-area accesses to non-mapped
	// addresses. If we don't do this, this will just
	// silently loop forever.

	pte_t pte_k = *pte_offset_kernel(pmd_k, addr);
	if (!pte_present(pte_k))
		goto exception;

	struct mm_struct *active_mm = tsk->active_mm;

	pgd_t *pgd = active_mm->pgd + index;
	pmd_t *pmd = pmd_offset((pud_t *)pgd, addr); // There is no pmd; this does pmd = pgd.
	set_pmd(pmd, *pmd_k);

	asm volatile (
		"settlb %0, %1\n"
		:: "r"(pte_val(pte_k) & ~(/* Modify only either the itlb or dtlb */
			(faultreason == pu32ExecFaultIntr) ?
				(_PAGE_READABLE | _PAGE_WRITABLE) :
				_PAGE_EXECUTABLE)),
			"r"((addr&PAGE_MASK)|active_mm->context) : "memory");

	ret = 1;

done:;
	if (console_loglevel > 7 && ret < 0) {
		WARN (1, "could not handle %s @ 0x%lx\n",
			pu32faultreasonstr(faultreason, (addr == regs->pc)),
			addr);
	}
	ti->in_fault = 0;
	return ret;
}
