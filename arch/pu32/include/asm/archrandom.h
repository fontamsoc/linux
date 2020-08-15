// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef __ASM_PU32_ARCHRANDOM_H
#define __ASM_PU32_ARCHRANDOM_H

#include <asm/timex.h>

static inline bool arch_get_random_long (unsigned long *v) {
	*v = random_get_entropy();
	return true;
}

static inline bool arch_get_random_int(unsigned int *v) {
	unsigned long _v;
	arch_get_random_long(&_v);
	*v = _v;
	return true;
}

static inline bool arch_has_random(void) {
	return true;
}

static inline bool arch_get_random_seed_long (unsigned long *v) {
	return arch_get_random_long(v);
}

static inline bool arch_get_random_seed_int(unsigned int *v) {
	return arch_get_random_int(v);
}

static inline bool arch_has_random_seed(void) {
	return arch_has_random();
}

#endif /* __ASM_PU32_ARCHRANDOM_H */
