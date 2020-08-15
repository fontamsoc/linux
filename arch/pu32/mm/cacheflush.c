// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#include <linux/mm.h>

#include <asm/cacheflush.h>

void update_mmu_cache (struct vm_area_struct *vma, unsigned long addr, pte_t *ptep) {

	pte_t pte = (*ptep);

	unsigned long pfn = pte_pfn(pte);
	if (!pfn_valid(pfn))
		return;

	struct page *page = pfn_to_page(pfn);
	if (page == ZERO_PAGE(0))
		return;

	unsigned long pte_val_pte = pte_val(pte);
	asm volatile (
		"settlb %0, %1\n"
		:: "r"(pte_val_pte & ~(/* Modify only either the itlb or dtlb */
			(pte_val_pte&_PAGE_EXECUTABLE) ?
				(_PAGE_READABLE | _PAGE_WRITABLE) :
				_PAGE_EXECUTABLE)),
			"r"((addr&PAGE_MASK)|vma->vm_mm->context) : "memory");
}
