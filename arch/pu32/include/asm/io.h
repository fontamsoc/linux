// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef __ASM_PU32_IO_H
#define __ASM_PU32_IO_H

#include <linux/compiler.h>
#include <linux/types.h>
#include <linux/pgtable.h>

#include <asm/page.h>

void __iomem *__ioremap(phys_addr_t paddr, size_t size, pgprot_t prot);
void __iounmap(void __iomem *vaddr);

// By default ioremap() is uncached.
#define ioremap(paddr, size) \
	__ioremap((paddr), (size), PAGE_KERNEL_NOCACHE)

#define ioremap_cache(paddr, size) \
	__ioremap((paddr), (size), PAGE_KERNEL)

#define iounmap(vaddr) \
	__iounmap(vaddr)

// Probably not necessary; only used by a few drivers.
#define readb_relaxed readb
#define readw_relaxed readw
#define readl_relaxed readl

#define virt_to_phys __pa
#define phys_to_virt __va

#include <asm-generic/io.h>

#endif /* __ASM_PU32_IO_H */
