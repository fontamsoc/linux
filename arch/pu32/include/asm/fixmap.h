// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef __ASM_PU32_FIXMAP_H
#define __ASM_PU32_FIXMAP_H

#include <linux/pgtable.h>

#include <asm/page.h>

enum fixed_addresses {
	__end_of_fixed_addresses
};

#define FIXADDR_TOP		(-(unsigned long)(PAGE_SIZE))
#define FIXADDR_START		(FIXADDR_TOP - (__end_of_fixed_addresses << PAGE_SHIFT))
#define FIXMAP_PAGE_IO		PAGE_KERNEL

#include <asm-generic/fixmap.h>

#endif /* __ASM_PU32_FIXMAP_H */
