// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#include <linux/export.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/io.h>

void __iomem *__ioremap (phys_addr_t paddr, size_t size, pgprot_t prot) {

	phys_addr_t last_addr;
	unsigned long offset, vaddr;
	struct vm_struct *area;

	last_addr = paddr + size - 1;
	if (!size || last_addr < paddr)
		return NULL;

	offset = paddr & (~PAGE_MASK);
	paddr &= PAGE_MASK;
	size = PAGE_ALIGN(size + offset);

	area = get_vm_area(size, VM_ALLOC);
	if (!area)
		return NULL;

	vaddr = (unsigned long)area->addr;

	if (ioremap_page_range(vaddr, vaddr + size, paddr, prot)) {
		free_vm_area(area);
		return NULL;
	}

	return (void __iomem *)(vaddr + offset);
}

void __iounmap (void __iomem *vaddr) {
	vunmap((void *)((unsigned long)vaddr & PAGE_MASK));
}
