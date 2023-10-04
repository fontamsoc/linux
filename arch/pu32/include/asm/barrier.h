// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef __ASM_PU32_BARRIER_H
#define __ASM_PU32_BARRIER_H

#ifdef CONFIG_SMP
#define mb() asm volatile ("dcacherst\n" ::: "memory")
#endif

#include <asm-generic/barrier.h>

#endif /* __ASM_PU32_BARRIER_H */
