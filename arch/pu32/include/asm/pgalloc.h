// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef __ASM_PU32_PGALLOC_H
#define __ASM_PU32_PGALLOC_H

#include <linux/mm.h>

static inline void pmd_populate_kernel (
	struct mm_struct *mm, pmd_t *pmd, pte_t *pte) {
	set_pmd (pmd, __pmd ((virt_to_pfn(pte) << PAGE_SHIFT) | _PAGE_PRESENT));
}

static inline void pmd_populate (
	struct mm_struct *mm, pmd_t *pmd, pgtable_t pte) {
	set_pmd (pmd, __pmd ((virt_to_pfn(page_address(pte)) << PAGE_SHIFT) | _PAGE_PRESENT));
}

#define pmd_pgtable(pmd) pmd_page(pmd)

static inline pgd_t *pgd_alloc (
	struct mm_struct *mm) {
	pgd_t *pgd = (pgd_t *)__get_free_page(GFP_ATOMIC);
	if (pgd != NULL) {
		pgd[0] = swapper_pg_dir[0];
		extern char __pu32tramp_start[], __pu32tramp_end[];
		unsigned long addr = (unsigned long)__pu32tramp_start;
		unsigned long pgd_idx = pgd_index(addr);
		if (pgd_idx > 0)
			memset (pgd+1, 0, ((pgd_idx-1) * sizeof(pgd_t)));
		// Copy trampoline mappings which are part of kernel mappings.
		// 1-to-1 mapping is always done in kernelspace regardless of TLB entries;
		// hence trampoline mappings, which are used in userspace,
		// are the only kernel mappings that needs to be copied.
		for (; addr < (unsigned long)__pu32tramp_end; addr += PGDIR_SIZE) {
			unsigned long addr_pgd_index = pgd_index(addr);
			pgd[addr_pgd_index] = swapper_pg_dir[addr_pgd_index];
		}
		memset (pgd + pgd_index(addr), 0,
			((PTRS_PER_PGD - pgd_index(addr)) * sizeof(pgd_t)));
	}
	return pgd;
}

#include <asm-generic/pgalloc.h>

#define __pte_free_tlb(tlb, pte, addr) pte_free((tlb)->mm, pte)

#endif /* __ASM_PU32_PGALLOC_H */
