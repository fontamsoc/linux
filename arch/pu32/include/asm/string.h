// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef __ASM_PU32_STRING_H
#define __ASM_PU32_STRING_H

#include <linux/types.h>

#define __HAVE_ARCH_MEMCPY
void *memcpy (void *dest, const void *src, size_t count);

#define __HAVE_ARCH_MEMSET
void *memset (void *s, int c, size_t count);

#define __HAVE_ARCH_MEMMOVE
void *memmove (void *dest, const void *src, size_t count);

#endif /* __ASM_PU32_STRING_H */
