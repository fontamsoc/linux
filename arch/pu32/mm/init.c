// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#include <linux/init.h>
#include <linux/mm.h>
#include <linux/memblock.h>

#include <asm/sections.h>

pgd_t swapper_pg_dir[PTRS_PER_PGD] __page_aligned_bss;
unsigned long empty_zero_page[PAGE_SIZE / sizeof(unsigned long)] __page_aligned_bss;
EXPORT_SYMBOL(empty_zero_page);

static void __init paging_init (void) {
	void add_pte (unsigned long addr, unsigned long prot) {
		pgd_t *pgd = swapper_pg_dir + pgd_index(addr);
		pmd_t *pmd = pmd_offset((pud_t *)pgd, addr); // There is no pmd; this does pmd = pgd.
		if (pmd_present(*pmd)) {
			pte_t pte = *pte_offset_map(pmd, addr);
			if (pte_present(pte)) // The mapping must not already exist.
				panic("add_pte: invalid pgd: pte already exist: 0x%x\n",
					(unsigned)pte_val(pte));
		}
		(void)pte_alloc(&init_mm, pmd);
		set_pmd(pmd, __pmd(pmd_val(*pmd)));
		set_pte_at(
			&init_mm, addr,
			pte_offset_map(pmd, addr),
			__pte((addr & PAGE_MASK) | prot));
	}
	add_pte(0, _PAGE_PRESENT | _PAGE_READABLE | _PAGE_WRITABLE);
	unsigned long addr;
	extern char __pu32tramp_start[], __pu32tramp_end[];
	for (addr = (unsigned long)__pu32tramp_start; addr < (unsigned long)__pu32tramp_end; addr += PAGE_SIZE)
		add_pte(addr, _PAGE_PRESENT | _PAGE_USER | _PAGE_CACHED | _PAGE_EXECUTABLE);
	#if 0
	// ### Since setksl is used, these are no longer needed, but kept for future reference.
	for (addr = (unsigned long)_text; addr < (unsigned long)_etext; addr += PAGE_SIZE)
		add_pte(addr,
			((addr >= (unsigned long)__pu32tramp_start && addr < (unsigned long)__pu32tramp_end) ?
				_PAGE_USER : 0) | _PAGE_PRESENT | _PAGE_EXECUTABLE);
	for (addr = (unsigned long)__start_rodata; addr < (unsigned long)__end_rodata; addr += PAGE_SIZE)
		add_pte(addr, _PAGE_PRESENT | _PAGE_READABLE);
	extern char __start_rwdata[], __end_rwdata[];
	for (addr = (unsigned long)__start_rwdata; addr < (unsigned long)__end_rwdata; addr += PAGE_SIZE)
		add_pte(addr, _PAGE_PRESENT | _PAGE_READABLE | _PAGE_WRITABLE);
	#endif
}

void __init mem_init (void) {
	memblock_free_all();
	mem_init_print_info(NULL);
	paging_init();
}
