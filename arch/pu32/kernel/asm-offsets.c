// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

// This program is used to generate definitions needed by
// assembly language modules.
//
// We use the technique used in the OSF Mach kernel code:
// generate asm statements containing #defines,
// compile this file to assembler, and then extract the
// #defines from the assembly-language output.

#include <linux/kbuild.h>

#include <asm/thread_info.h>

void asm_offsets (void) {
	OFFSET(TASK_TI_TASK, thread_info, task);
	OFFSET(TASK_TI_FLAGS, thread_info, flags);
	OFFSET(TASK_TI_PREEMPT_COUNT, thread_info, preempt_count);
	OFFSET(TASK_TI_KSP, thread_info, ksp);
	OFFSET(TASK_TI_KR1, thread_info, kr1);
	OFFSET(TASK_TI_KPC, thread_info, kpc);

	DEFINE(OFFSETOF_TI, PU32_TI_OFFSET);
	DEFINE(SIZEOF_PU32_PT_REGS, sizeof(struct pu32_pt_regs));

	OFFSET(PU32_PT_REGS_PREV_KSP_OFFSET, pu32_pt_regs, prev_ksp_offset);
}
