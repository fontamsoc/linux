// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#include <linux/mm.h>

#include <asm/tlbflush.h>

void update_mmu_cache (struct vm_area_struct *vma, unsigned long addr, pte_t *ptep) {

	pte_t pte = (*ptep);

	unsigned long pfn = pte_pfn(pte);
	if (!pfn_valid(pfn))
		return;

	struct page *page = pfn_to_page(pfn);
	if (page == ZERO_PAGE(0))
		return;

	#ifdef CONFIG_SMP
	flush_tlb_page(vma, addr);
	#else
	unsigned long pte_val_pte = pte_val(pte);
	asm volatile (
		"settlb %0, %1\n"
		:: "r"(pte_val_pte & ~(/* Modify only either the itlb or dtlb */
			(pte_val_pte&_PAGE_EXECUTABLE) ?
				(_PAGE_READABLE | _PAGE_WRITABLE) :
				_PAGE_EXECUTABLE)),
			"r"((addr&PAGE_MASK)|vma->vm_mm->context[raw_smp_processor_id()])
		: "memory");
	#endif
}

#ifdef CONFIG_SMP

struct flush_tlb_args {
	struct vm_area_struct *vma;
	unsigned long start;
	unsigned long end;
};

static inline void flush_tlb_page_ipi (void *arg) {
	struct flush_tlb_args *args = arg;
	local_flush_tlb_page(args->vma, args->start);
}

void flush_tlb_page (struct vm_area_struct *vma, unsigned long addr) {
	struct flush_tlb_args args = {
		.vma = vma,
		.start = addr
	};
	on_each_cpu_mask(mm_cpumask(vma->vm_mm), flush_tlb_page_ipi, &args, 1);
}

void flush_tlb_all (void) {
	on_each_cpu((smp_call_func_t)local_flush_tlb_all, NULL, 1);
}

void flush_tlb_mm (struct mm_struct *mm) {
	on_each_cpu_mask(mm_cpumask(mm), (smp_call_func_t)local_flush_tlb_mm, mm, 1);
}

static inline void flush_tlb_range_ipi (void *arg) {
	struct flush_tlb_args *args = arg;
	local_flush_tlb_range(args->vma, args->start, args->end);
}

void flush_tlb_range (struct vm_area_struct *vma, unsigned long start, unsigned long end) {
	struct flush_tlb_args args = {
		.vma = vma,
		.start = start,
		.end = end
	};
	on_each_cpu_mask(mm_cpumask(vma->vm_mm), flush_tlb_range_ipi, &args, 1);
}

static inline void flush_tlb_kernel_range_ipi (void *arg) {
	struct flush_tlb_args *args = (struct flush_tlb_args *)arg;
	local_flush_tlb_kernel_range(args->start, args->end);
}

void flush_tlb_kernel_range (unsigned long start, unsigned long end) {
	struct flush_tlb_args args = {
		.start = start,
		.end = end
	};
	on_each_cpu(flush_tlb_kernel_range_ipi, &args, 1);
}

#endif
