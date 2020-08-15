// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#include <linux/init.h>
#include <linux/mm.h>
#include <linux/memblock.h>

#include <asm/sections.h>

pgd_t swapper_pg_dir[PTRS_PER_PGD] __page_aligned_bss;
unsigned long empty_zero_page[PAGE_SIZE / sizeof(unsigned long)] __page_aligned_bss;
EXPORT_SYMBOL(empty_zero_page);

void __init mem_init (void) {
	memblock_free_all();
}

// Remove W bit on private pages for COW support.
// Shared pages can have exact HW mapping.
static const pgprot_t protection_map[16] = {
	[VM_NONE]                                       = MKP(0, 0, 0),
	[VM_READ]                                       = MKP(0, 0, 1),
	[VM_WRITE]                                      = MKP(0, 0, 0), /* COW */
	[VM_WRITE | VM_READ]                            = MKP(0, 0, 1), /* COW */
	[VM_EXEC]                                       = MKP(1, 0, 0),
	[VM_EXEC | VM_READ]                             = MKP(1, 0, 1),
	[VM_EXEC | VM_WRITE]                            = MKP(1, 0, 0), /* COW */
	[VM_EXEC | VM_WRITE | VM_READ]                  = MKP(1, 0, 1), /* COW */
	[VM_SHARED]                                     = MKP(0, 0, 0),
	[VM_SHARED | VM_READ]                           = MKP(0, 0, 1),
	[VM_SHARED | VM_WRITE]                          = MKP(0, 1, 0),
	[VM_SHARED | VM_WRITE | VM_READ]                = MKP(0, 1, 1),
	[VM_SHARED | VM_EXEC]                           = MKP(1, 0, 0),
	[VM_SHARED | VM_EXEC | VM_READ]                 = MKP(1, 0, 1),
	[VM_SHARED | VM_EXEC | VM_WRITE]                = MKP(1, 1, 0),
	[VM_SHARED | VM_EXEC | VM_WRITE | VM_READ]      = MKP(1, 1, 1)
};
DECLARE_VM_GET_PAGE_PROT
