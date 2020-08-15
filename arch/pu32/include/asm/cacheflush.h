// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef __ASM_PU32_CACHEFLUSH_H
#define __ASM_PU32_CACHEFLUSH_H

#include <linux/mm_types.h>

static inline void flush_cache_all (void) {
	asm volatile ("icacherst; dcacherst" ::: "memory");
}
#define flush_cache_all flush_cache_all

static inline void flush_cache_mm (struct mm_struct *mm) {
	flush_cache_all();
}
#define flush_cache_mm flush_cache_mm

static inline void flush_cache_dup_mm (struct mm_struct *mm) {
	flush_cache_all();
}
#define flush_cache_dup_mm flush_cache_dup_mm

static inline void flush_cache_range (
	struct vm_area_struct *vma, unsigned long start, unsigned long end) {
	flush_cache_all();
}
#define flush_cache_range flush_cache_range

static inline void flush_cache_page (
	struct vm_area_struct *vma, unsigned long vmaddr, unsigned long pfn) {
	flush_cache_all();
}
#define flush_cache_page flush_cache_page

#define PG_dcache_clean PG_arch_1
static inline void flush_dcache_page (struct page *page) {
	clear_bit (PG_dcache_clean, &page->flags);
}
#define ARCH_IMPLEMENTS_FLUSH_DCACHE_PAGE 1
#define flush_dcache_page flush_dcache_page

static inline void flush_dcache_mmap_lock (struct address_space *mapping) {
	asm volatile ("dcacherst" ::: "memory");
}
#define flush_dcache_mmap_lock flush_dcache_mmap_lock

static inline void flush_dcache_mmap_unlock (struct address_space *mapping) {
	asm volatile ("dcacherst" ::: "memory");
}
#define flush_dcache_mmap_unlock flush_dcache_mmap_unlock

static inline void flush_icache_range (unsigned long start, unsigned long end) {
	asm volatile ("icacherst" ::: "memory");
}
#define flush_icache_range flush_icache_range

#define flush_icache_user_range flush_icache_range

static inline void flush_icache_page (
	struct vm_area_struct *vma, struct page *page) {
	asm volatile ("icacherst" ::: "memory");
}
#define flush_icache_page flush_icache_page

static inline void flush_icache_user_page (
	struct vm_area_struct *vma, struct page *page,
	unsigned long addr, int len) {
	asm volatile ("icacherst" ::: "memory");
}
#define flush_icache_user_page flush_icache_user_page

static inline void flush_cache_vmap (unsigned long start, unsigned long end) {
	flush_cache_all();
}
#define flush_cache_vmap flush_cache_vmap

static inline void flush_cache_vunmap (unsigned long start, unsigned long end) {
	flush_cache_all();
}
#define flush_cache_vunmap flush_cache_vunmap

#include <asm-generic/cacheflush.h>

#endif /* __ASM_PU32_CACHEFLUSH_H */
