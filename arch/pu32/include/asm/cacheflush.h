// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef __ASM_PU32_CACHEFLUSH_H
#define __ASM_PU32_CACHEFLUSH_H

#include <linux/mm.h>

static inline void flush_cache_all (void) {
	asm volatile ("dcacherst; icacherst\n" ::: "memory");
}
#define flush_cache_all flush_cache_all

static inline void flush_cache_mm (struct mm_struct *mm) {
	flush_cache_all();
}
#define flush_cache_mm flush_cache_mm

#define flush_cache_dup_mm flush_cache_mm

static inline void flush_cache_range (
	struct vm_area_struct *vma, unsigned long start, unsigned long end) {
	asm volatile ("dcacherst\n" ::: "memory");
	if (vma->vm_flags & VM_EXEC)
		asm volatile ("icacherst\n" ::: "memory");
}
#define flush_cache_range flush_cache_range

static inline void flush_cache_page (
	struct vm_area_struct *vma, unsigned long vaddr, unsigned long pfn) {
	asm volatile ("dcacherst\n" ::: "memory");
	if (vma->vm_flags & VM_EXEC)
		asm volatile ("icacherst\n" ::: "memory");
}
#define flush_cache_page flush_cache_page

static inline void flush_icache_range (unsigned long start, unsigned long end) {
	asm volatile ("icacherst\n" ::: "memory");
}
#define flush_icache_range flush_icache_range

#define flush_icache_user_range flush_icache_range

static inline void flush_icache_page (
	struct vm_area_struct *vma, struct page *page) {
	if (vma->vm_flags & VM_EXEC)
		asm volatile ("icacherst\n" ::: "memory");
}
#define flush_icache_page flush_icache_page

static inline void flush_icache_user_page (
	struct vm_area_struct *vma, struct page *page,
	unsigned long addr, int len) {
	if (vma->vm_flags & VM_EXEC)
		asm volatile ("icacherst\n" ::: "memory");
}
#define flush_icache_user_page flush_icache_user_page

static inline void flush_cache_vmap (unsigned long start, unsigned long end) {
	flush_cache_all();
}
#define flush_cache_vmap flush_cache_vmap

#include <asm-generic/cacheflush.h>

#endif /* __ASM_PU32_CACHEFLUSH_H */
