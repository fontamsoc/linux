// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef __ASM_PU32_TLBFLUSH_H__
#define __ASM_PU32_TLBFLUSH_H__

#ifndef __ASSEMBLY__

// TLB Management
// ==============
//
// The TLB specific code is expected to perform whatever tests
// it needs to determine if it should invalidate the TLB for each call.
// Start addresses are inclusive and end addresses are exclusive;
// it is safe to round these addresses down.
//
// flush_tlb_all()
// 	Invalidate the entire TLB.
//
// flush_tlb_mm(mm)
// 	Invalidate all TLB entries in a particular address space.
// 	- mm:		mm_struct describing address space
//
// flush_tlb_range(vma,start,end)
// 	Invalidate a range of TLB entries in the specified address space.
// 	- vma:		vma_struct describing address range
// 	- start:	start address (may not be aligned)
// 	- end:		end address (exclusive, may not be aligned)
//
// flush_tlb_page(vma,addr)
// 	Invalidate the specified page in the specified address range.
// 	- vma:		vma_struct describing address range
// 	- addr:		virtual address (may not be aligned)

#include <asm/thread_info.h>
#include <pu32.h>

static inline unsigned long pu32_tlb_size (void) {
	unsigned long sz;
	asm volatile ("gettlbsize %0\n" : "=r"(sz) :: "memory");
	return sz;
}

static inline unsigned long pu32_tlb_lookup (unsigned long addr) {
	unsigned long d = ((addr & PAGE_MASK) | current->active_mm->context);
	asm volatile ("gettlb %0, %0\n" : "+r"(d) :: "memory");
	return d;
}

static inline void pu32_tlb_update (struct pu32tlbentry tlbentry) {
	asm volatile ("settlb %0, %1\n" :: "r"(tlbentry.d1), "r"(tlbentry.d2) : "memory");
}

static inline void local_flush_tlb_page (
	struct vm_area_struct *vma, unsigned long addr) {
	unsigned long d = ((addr & PAGE_MASK) | current->active_mm->context);
	asm volatile ("clrtlb %0, %1\n" :: "r"(-1), "r"(d) : "memory");
}

static inline void local_flush_tlb_all (void) {
	unsigned long flags;
	raw_local_irq_save(flags);
	unsigned long sz = (pu32_tlb_size() << PAGE_SHIFT);
	do asm volatile ("clrtlb %0, %1\n" :: "r"(0), "r"(sz -= PAGE_SIZE) : "memory");
		while (sz);
	raw_local_irq_restore(flags);
}

static inline void local_flush_tlb_range (
	struct vm_area_struct *vma,
	unsigned long start,
	unsigned long end) {
	unsigned long flags;
	raw_local_irq_save(flags);
	for (; start < end; start += PAGE_SIZE) {
		unsigned long d = ((start & PAGE_MASK) | current->active_mm->context);
		asm volatile ("clrtlb %0, %1\n" :: "r"(-1), "r"(d) : "memory");
	}
	raw_local_irq_restore(flags);
}

static inline void local_flush_tlb_mm (struct mm_struct *mm) {
	unsigned long flags;
	raw_local_irq_save(flags);
	unsigned long context = mm->context;
	struct vm_area_struct *vma;
	for (vma = mm->mmap; vma != NULL; vma = vma->vm_next) {
		unsigned long start = vma->vm_start;
		unsigned long end = vma->vm_end;
		for (; start < end; start += PAGE_SIZE) {
			unsigned long d = ((start & PAGE_MASK) | context);
			asm volatile ("clrtlb %0, %1\n" :: "r"(-1), "r"(d) : "memory");
		}
	}
	raw_local_irq_restore(flags);
}

#define flush_tlb_all			local_flush_tlb_all
#define flush_tlb_mm			local_flush_tlb_mm
#define flush_tlb_range			local_flush_tlb_range
#define flush_tlb_page			local_flush_tlb_page
#define flush_tlb_kernel_range(s,e)	local_flush_tlb_range(((struct vm_area_struct *){0}), s, e)
#define flush_tlb_kernel_page(a)	local_flush_tlb_page(((struct vm_area_struct *){0}), a)

#endif

#endif /* __ASM_PU32_TLBFLUSH_H__ */
