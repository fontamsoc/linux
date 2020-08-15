// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef __ASM_PU32_IRQ_WORK_H
#define __ASM_PU32_IRQ_WORK_H

static inline bool arch_irq_work_has_interrupt (void) {
	return IS_ENABLED(CONFIG_SMP);
}

extern void arch_irq_work_raise (void);

#endif /* __ASM_PU32_IRQ_WORK_H */
