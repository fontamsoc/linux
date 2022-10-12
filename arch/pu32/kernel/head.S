// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#include <linux/linkage.h>
#include <linux/init.h>

#include <asm/thread_info.h>
#include <generated/asm-offsets.h>

__INIT

ENTRY(_start)

	# The following convention is used:
	# pu32_start (char **argv, char **envp);
	# Where kernel arguments are passed in argv,
	# and environment variables are passed in envp.
	# Hence argv is expected in %1 and envp is
	# expected in %2.
	# The stack is expected laid-out as follow:
	# - argc
	# - null-terminated argv pointers array.
	# - null-terminated envp pointers array.
	ld32 %3, %sp
	cpy %1, %sp
	inc8 %1, 4
	cpy %2, %3
	li8 %sr, 2
	sll %2, %sr
	add %2, %1
	inc8 %2, 4

	rli %tp, init_thread_info

	# Must match (pu32_stack_bottom() - sizeof(struct pu32_pt_regs)),
	# where sizeof(struct pu32_pt_regs) account for the space used
	# only if the thread ever become a user-thread.
	rli %sp, init_stack + ((OFFSETOF_TI - __SIZEOF_POINTER__) - SIZEOF_PU32_PT_REGS)

	# Set thread_info.ksp; the other fields
	# have been initialized by INIT_THREAD_INFO().
	CURRENT_THREAD_INFO(%3)
	inc %3, TASK_TI_KSP
	st32 %sp, %3

	# Set ((struct pu32_pt_regs *)thread_info.ksp)->prev_ksp_offset .
	li %3, PU32_PT_REGS_PREV_KSP_OFFSET
	add %3, %sp
	cpy %4, %sp
	inc %4, SIZEOF_PU32_PT_REGS
	st32 %4, %3

	rli %sr, pu32_start
	j %sr

END(_start)

#ifdef CONFIG_SMP

ENTRY(_inc_cpu_up_arg)
	# Restore default value at PARKPU_RLI16IMM_ADDR.
	li %sr, PARKPU_RLI16IMM_ADDR
	li %1, -10
	st16v %1, %sr

	rli %sr, cpu_up_arg
	ld32v %1, %sr
	inc8 %1, 1
	st32v %1, %sr

	li %sr, PARKPU_ADDR
	j %sr

END(_inc_cpu_up_arg)

ENTRY(_start_smp)
	# Restore default value at PARKPU_RLI16IMM_ADDR.
	li %sr, PARKPU_RLI16IMM_ADDR
	li %1, -10
	st16v %1, %sr

	rli %sr, cpu_up_arg
	ld32v %sp, %sr

	li %fp, 0

	rli %sr, pu32_start_smp
	j %sr

END(_start_smp)

#endif
