// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef __ASM_PU32_PAGE_H
#define __ASM_PU32_PAGE_H

#define PAGE_SHIFT	12
#define PAGE_SIZE	(1 << PAGE_SHIFT)
#define PAGE_MASK	(~(PAGE_SIZE-1))

#ifndef __ASSEMBLY__

#define clear_page(page)	memset((page), 0, PAGE_SIZE)
#define copy_page(to,from)	memcpy((to), (from), PAGE_SIZE)
#define clear_user_page(page, vaddr, pg)	clear_page(page)
#define copy_user_page(to, from, vaddr, pg)	copy_page(to, from)

// These are used to make use of C type-checking.
typedef struct {unsigned long pte;} pte_t;
typedef struct {unsigned long pgd;} pgd_t;
typedef struct {unsigned long pgprot;} pgprot_t;
typedef struct page *pgtable_t;

#define pte_val(x)	((x).pte)
#define pgd_val(x)	((x).pgd)
#define pgprot_val(x)	((x).pgprot)

#define __pte(x)	((pte_t){(x)})
#define __pgd(x)	((pgd_t){(x)})
#define __pgprot(x)	((pgprot_t){(x)})

#endif /* !__ASSEMBLY__ */

#define PAGE_OFFSET	(0)

#ifndef __ASSEMBLY__

#define __va(x) ((void *)((unsigned long) (x)))
#define __pa(x) ((unsigned long) (x))

#define virt_to_pfn(addr)	(__pa(addr) >> PAGE_SHIFT)
#define pfn_to_virt(pfn)	__va((pfn) << PAGE_SHIFT)

#define virt_to_page(addr)	pfn_to_page(virt_to_pfn(addr))
#define page_to_virt(page)	pfn_to_virt(page_to_pfn(page))

#define page_to_phys(page)	(page_to_pfn(page) << PAGE_SHIFT)
#define phys_to_page(addr)	(pfn_to_page((addr) >> PAGE_SHIFT))

#define pfn_valid(pfn)		((pfn) < max_mapnr)
#define	virt_addr_valid(addr)	(pfn_valid(virt_to_pfn(addr)))

#endif /* __ASSEMBLY__ */

#define VM_DATA_DEFAULT_FLAGS VM_DATA_FLAGS_NON_EXEC

#include <asm-generic/memory_model.h>
#include <asm-generic/getorder.h>

#endif /* __ASM_PU32_PAGE_H */
