// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef __ASM_PU32_MMU_H
#define __ASM_PU32_MMU_H

typedef unsigned long mm_context_t[NR_CPUS];

#define PU32_NO_CONTEXT   0
#define PU32_INIT_CONTEXT 1

#define INIT_MM_CONTEXT(mm) \
	.context = {[0 ... NR_CPUS - 1] = PU32_INIT_CONTEXT},

#endif /* __ASM_PU32_MMU_H */
