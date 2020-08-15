// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef __ASM_PU32_PGTABLE_H
#define __ASM_PU32_PGTABLE_H

#include <linux/mm_types.h>
#include <linux/sched.h>

#include <asm-generic/pgtable-nopmd.h>
#include <asm/page.h>
#include <asm/processor.h>
#include <asm/fixmap.h>

#ifndef __ASSEMBLY__

// 2-level page tables
//
// Page Directory:
// - size: 4KB
// - 1024 entries of 32-bit words
// - each entry covers 4MB
//
// Page Table:
// - size: 4KB
// - 1024 entries of 32-bit words
// - each entry covers 4KB

#define PGD_ORDER	0 /* PGD is one page */
#define PTE_ORDER	0 /* PTE is one page */

#define PGD_T_LOG2	(__builtin_ffs(sizeof(pgd_t)) - 1) /* 2 */
#define PTE_T_LOG2	(__builtin_ffs(sizeof(pte_t)) - 1) /* 2 */

#define PTRS_PER_PGD_LOG2	(PAGE_SHIFT + PGD_ORDER - PGD_T_LOG2) /* 10 */
#define PTRS_PER_PTE_LOG2	(PAGE_SHIFT + PTE_ORDER - PTE_T_LOG2) /* 10 */

#define PTRS_PER_PGD		(1 << PTRS_PER_PGD_LOG2) /* 1024 */
#define PTRS_PER_PTE		(1 << PTRS_PER_PTE_LOG2) /* 1024 */

#define PGDIR_SHIFT		(PTRS_PER_PTE_LOG2 + PAGE_SHIFT) /* 10 + 12 */
#define PGDIR_SIZE		(1 << PGDIR_SHIFT)
#define PGDIR_MASK		(~(PGDIR_SIZE - 1))

#define FIRST_USER_ADDRESS	TASK_UNMAPPED_BASE
#define USER_PGTABLES_CEILING	TASK_SIZE

#define _PAGE_DIRTY		(1 << 7) /* was written to when 1 */
#define _PAGE_ACCESSED		(1 << 6) /* was accessed when 1 */
#define _PAGE_PRESENT		(1 << 5) /* is present when 1 */
#define _PAGE_USER		(1 << 4) /* accessible from userspace when 1 */
#define _PAGE_CACHED		(1 << 3) /* cached when 1 */
#define _PAGE_READABLE		(1 << 2) /* readable when 1 */
#define _PAGE_WRITABLE		(1 << 1) /* writable when 1 */
#define _PAGE_EXECUTABLE	(1 << 0) /* executable when 1 */

#define MKP(x, w, r)                            \
	__pgprot(_PAGE_PRESENT | _PAGE_CACHED | \
	         _PAGE_ACCESSED |               \
	((x) ? _PAGE_EXECUTABLE : 0) |          \
	((w) ? _PAGE_WRITABLE   : 0) |          \
	((r) ? _PAGE_READABLE   : 0) )

// Masks of bits that are to be preserved across pgprot changes.
#define _PAGE_CHG_MASK	(_PAGE_DIRTY | _PAGE_ACCESSED | _PAGE_USER)

#define PAGE_SHARED 		__pgprot(_PAGE_PRESENT | _PAGE_CACHED | _PAGE_ACCESSED | _PAGE_READABLE | _PAGE_WRITABLE)
#define PAGE_KERNEL		PAGE_SHARED
#define PAGE_KERNEL_NOCACHE	__pgprot(_PAGE_PRESENT | _PAGE_READABLE | _PAGE_ACCESSED | _PAGE_WRITABLE)
#define PAGE_COPY 		MKP(0, 0, 1)

// Vmalloc range.
#define VMALLOC_START	(TASK_SIZE)
#define VMALLOC_END	(FIXADDR_START)

// Zero page.
extern unsigned long empty_zero_page[PAGE_SIZE / sizeof(unsigned long)];
#define ZERO_PAGE(vaddr) (virt_to_page(empty_zero_page))

// Kernel pg table.
extern pgd_t swapper_pg_dir[PTRS_PER_PGD];

#define pte_ERROR(e) \
	printk(KERN_ERR "%s:%d: bad pte %08llx.\n", \
			__FILE__, __LINE__, pte_val(e))
#define pgd_ERROR(e) \
	printk(KERN_ERR "%s:%d: bad pgd %08lx.\n", \
			__FILE__, __LINE__, pgd_val(e))

#define pte_pfn(x)		(pte_val(x) >> PAGE_SHIFT)
#define pte_page(x)		pfn_to_page(pte_pfn(x))
#define pte_none(x)		(!pte_val(x))
#define pte_present(pte)	(pte_val(pte) & _PAGE_PRESENT)
#define set_pte_at(mm, addr, ptr, val) \
	do { \
		struct thread_info *ti = current_thread_info(); \
		unsigned long pte_val_val = pte_val(val); \
		asm volatile ("ldst %0, %1\n" :: \
			"r" ((unsigned long){(pte_val_val?(pte_val_val|((ti->task->flags&(PF_KTHREAD | PF_IO_WORKER))?0:_PAGE_USER)):0)}), \
			"r"  (ptr) : \
			"memory"); \
		/* if above change was done as part of a pagefault,
		   update_mmu_cache() gets called and updates the tlb */ \
	} while (0)

#define pte_clear(mm, addr, ptep) \
	set_pte_at(mm, addr, ptep, __pte(0))
#define pfn_pte(pfn, prot)	__pte(((pfn) << PAGE_SHIFT) | pgprot_val(prot))
#define mk_pte(page, prot)	pfn_pte(page_to_pfn(page), prot)

// PTE attributes (only works if pte_present() is true).
#define pte_write(pte)		(pte_val(pte) & _PAGE_WRITABLE)
#define pte_dirty(pte)		(pte_val(pte) & _PAGE_DIRTY)
#define pte_young(pte)		(pte_val(pte) & _PAGE_ACCESSED)
static inline pte_t pte_wrprotect (pte_t pte)
	{ pte_val(pte) &= ~_PAGE_WRITABLE; return pte; }
static inline pte_t pte_mkwrite (pte_t pte)
	{ pte_val(pte) |= _PAGE_WRITABLE; return pte; }
static inline pte_t pte_mkclean (pte_t pte)
	{ pte_val(pte) &= ~_PAGE_DIRTY; return pte; }
static inline pte_t pte_mkdirty (pte_t pte)
	{ pte_val(pte) |= _PAGE_DIRTY; return pte; }
static inline pte_t pte_mkold (pte_t pte)
	{ pte_val(pte) &= ~_PAGE_ACCESSED; return pte; }
static inline pte_t pte_mkyoung (pte_t pte)
	{ pte_val(pte) |= _PAGE_ACCESSED; return pte; }
static inline pte_t pte_modify (pte_t pte, pgprot_t newprot)
	{ return __pte((pte_val(pte) & _PAGE_CHG_MASK) | pgprot_val(newprot)); }

#define pmd_pfn(x)		(pmd_val(x) >> PAGE_SHIFT)
#define pmd_page(x)		pfn_to_page(pmd_pfn(x))
static inline unsigned long pmd_page_vaddr (pmd_t pmd) {
	return (unsigned long)(pmd_val(pmd) & PAGE_MASK);
}
#define pmd_none(x)		(!pmd_val(x))
#define pmd_present(x)		(pmd_val(x) & _PAGE_PRESENT)
#define pmd_bad(x)		(!pmd_present(x))
#define set_pmd(ptr, val) \
	asm volatile ("ldst %0, %1\n" :: \
		"r" ((unsigned long){pmd_val(val)}), \
		"r" (ptr) : \
		"memory")
#define set_pmd_at(mm, addr, ptr, val) \
	set_pmd(ptr, __pmd(pmd_val(val)))
#define pmd_clear(x) \
	set_pmd(x, __pmd(0))

// Make a page protection as uncacheable.
#define pgprot_noncached(prot) \
	__pgprot(pgprot_val(prot) & ~_PAGE_CACHED)

#define pgprot_writecombine pgprot_noncached

// Encode and decode PTEs representing swap pages.

// Swap entries are stored in the Linux
// page tables as follows:
//   1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
//   offset................................. type..... 0 0 0 0 0 0 0

// This implement up to 2**5 == 32 swap-files
// and 2**20 * 4K == 4G per swap-file.
#define __SWP_TYPE_SHIFT	7
#define __SWP_TYPE_BITS		5
#define __SWP_TYPE_MASK		((1 << __SWP_TYPE_BITS) - 1)
#define __SWP_OFFSET_SHIFT	(__SWP_TYPE_BITS + __SWP_TYPE_SHIFT)

#define __swp_type(x) \
	(((x).val >> __SWP_TYPE_SHIFT) & __SWP_TYPE_MASK)
#define __swp_offset(x) \
	((x).val >> __SWP_OFFSET_SHIFT)
#define __swp_entry(type, offset) \
	((swp_entry_t){ ((type) << __SWP_TYPE_SHIFT) | ((offset) << __SWP_OFFSET_SHIFT) })
#define __pte_to_swp_entry(pte) \
	((swp_entry_t) { pte_val(pte) })
#define __swp_entry_to_pte(swp) \
	((pte_t) { (swp).val })

#define MAX_SWAPFILES_CHECK() \
	BUILD_BUG_ON(MAX_SWAPFILES_SHIFT > __SWP_TYPE_BITS)

#define kern_addr_valid(addr)	(1)

void update_mmu_cache (struct vm_area_struct *vma, unsigned long addr, pte_t *ptep);

#endif /* !__ASSEMBLY__ */

#endif /* __ASM_PU32_PGTABLE_H */
