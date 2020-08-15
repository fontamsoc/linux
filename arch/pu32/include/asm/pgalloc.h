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
	pgd_t *pgd = (pgd_t *)__get_free_page(GFP_KERNEL);
	if (pgd != NULL) {
		memcpy(pgd, init_mm.pgd, PTRS_PER_PGD*sizeof(pgd_t));
	}
	return pgd;
}

#include <asm-generic/pgalloc.h>

#define __pte_free_tlb(tlb, pte, addr) \
do { \
	pgtable_pte_page_dtor(pte); \
	tlb_remove_page((tlb), (pte)); \
} while (0)

#endif /* __ASM_PU32_PGALLOC_H */
