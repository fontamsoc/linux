// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef __ASM_PU32_BARRIER_H
#define __ASM_PU32_BARRIER_H

#ifndef __ASSEMBLY__

#define mb() \
	do { \
		unsigned long x; \
		asm volatile ("ldst %%sr, %0" :: "r" (&x) : "%sr", "memory"); \
	} while (0)

#include <asm-generic/barrier.h>

#endif  /* __ASSEMBLY__ */

#endif /* __ASM_PU32_BARRIER_H */
