// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef __ASM_PU32_UACCESS_H
#define __ASM_PU32_UACCESS_H

#include <linux/pgtable.h>

#define __access_ok(addr, size) \
	(((unsigned long)(addr) >= FIRST_USER_ADDRESS) && \
	(((unsigned long)(addr) + (size)) <= USER_PGTABLES_CEILING) && \
	(((unsigned long)(addr) + (size)) >= (unsigned long)(addr)))

#include <asm-generic/uaccess.h>

#endif /* __ASM_PU32_UACCESS_H */
