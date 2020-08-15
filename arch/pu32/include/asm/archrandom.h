// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef __ASM_PU32_ARCHRANDOM_H
#define __ASM_PU32_ARCHRANDOM_H

#include <asm/timex.h>

static inline size_t __must_check arch_get_random_longs (unsigned long *v, size_t max_longs) {
	*v = random_get_entropy();
	return 1;
}

static inline size_t __must_check arch_get_random_seed_longs (unsigned long *v, size_t max_longs) {
	return arch_get_random_longs(v, max_longs);
}

#endif /* __ASM_PU32_ARCHRANDOM_H */
