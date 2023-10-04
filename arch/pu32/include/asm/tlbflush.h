// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef __ASM_PU32_TLBFLUSH_H__
#define __ASM_PU32_TLBFLUSH_H__

#ifndef __ASSEMBLY__

#include <asm/thread_info.h>
#include <pu32.h>

static inline unsigned long pu32_tlb_size (void) {
	unsigned long sz;
	asm volatile ("gettlbsize %0\n" : "=r"(sz) :: "memory");
	return sz;
}

static inline unsigned long pu32_tlb_lookup (unsigned long addr) {
	unsigned long d = ((addr & PAGE_MASK) | current->active_mm->context[raw_smp_processor_id()]);
	asm volatile ("gettlb %0, %0\n" : "+r"(d) :: "memory");
	return d;
}

static inline void pu32_tlb_update (struct pu32tlbentry tlbentry) {
	asm volatile ("settlb %0, %1\n" :: "r"(tlbentry.d1), "r"(tlbentry.d2) : "memory");
}

static inline void local_flush_tlb_page (struct vm_area_struct *vma, unsigned long addr) {
	unsigned long d = ((addr & PAGE_MASK) | current->active_mm->context[raw_smp_processor_id()]);
	asm volatile ("clrtlb %0, %1\n" :: "r"(-1), "r"(d) : "memory");
}

static inline void local_flush_tlb_all (void) {
	unsigned long flags;
	local_irq_save(flags);
	unsigned long sz = (pu32_tlb_size() << PAGE_SHIFT);
	do asm volatile ("clrtlb %0, %1\n" :: "r"(0), "r"(sz -= PAGE_SIZE) : "memory");
		while (sz);
	local_irq_restore(flags);
}

static inline void local_flush_tlb_mm (struct mm_struct *mm) {
	unsigned long flags;
	local_irq_save(flags);
	unsigned long context = mm->context[raw_smp_processor_id()];
	unsigned long sz = (pu32_tlb_size() << PAGE_SHIFT);
	do asm volatile ("clrtlb %0, %1\n" :: "r"(PAGE_SIZE-1), "r"((sz -= PAGE_SIZE) | context) : "memory");
		while (sz);
	local_irq_restore(flags);
}

// start address is inclusive; end address is exclusive;
// it is safe to round those addresses down.
static inline void local_flush_tlb_range (
	struct vm_area_struct *vma, unsigned long start, unsigned long end) {
	unsigned long flags;
	local_irq_save(flags);
	for (start &= PAGE_MASK; start < end; start += PAGE_SIZE) {
		unsigned long d = (start | current->active_mm->context[raw_smp_processor_id()]);
		asm volatile ("clrtlb %0, %1\n" :: "r"(-1), "r"(d) : "memory");
	}
	local_irq_restore(flags);
}

static inline void local_flush_tlb_kernel_range (unsigned long start, unsigned long end) {
	local_flush_tlb_range(((struct vm_area_struct *){0}), start, end);
}

#ifdef CONFIG_SMP
extern void flush_tlb_page (struct vm_area_struct *vma, unsigned long addr);
extern void flush_tlb_all (void);
extern void flush_tlb_mm (struct mm_struct *mm);
extern void flush_tlb_range (struct vm_area_struct *vma, unsigned long start, unsigned long end);
extern void flush_tlb_kernel_range (unsigned long start, unsigned long end);
#else
#define flush_tlb_page              local_flush_tlb_page
#define flush_tlb_all               local_flush_tlb_all
#define flush_tlb_mm                local_flush_tlb_mm
#define flush_tlb_range             local_flush_tlb_range
#define flush_tlb_kernel_range(s,e) local_flush_tlb_range(((struct vm_area_struct *){0}), s, e)
#endif

#endif

#endif /* __ASM_PU32_TLBFLUSH_H__ */
