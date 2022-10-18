// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef __ASM_PU32_IRQ_REGS_H
#define __ASM_PU32_IRQ_REGS_H

#define ARCH_HAS_OWN_IRQ_REGS

#include <asm/thread_info.h>

static inline struct pt_regs *get_irq_regs (void) {
	return (struct pt_regs *)0;
}

static inline struct pt_regs *set_irq_regs (struct pt_regs *new_regs) {
	return (struct pt_regs *)0;
}

#endif /* __ASM_PU32_IRQ_REGS_H */
