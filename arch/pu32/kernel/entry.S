// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#include <linux/linkage.h>
#include <linux/init.h>

#include <asm/thread_info.h>
#include <asm/syscall.h>
#include <generated/asm-offsets.h>

__REF

GLOBAL(ret_from_syscall)
	# %1 has syscall return value.
	rli %sr, pu32_save_syscall_retval
	jl %rp, %sr
GLOBAL(ret_from_exception)
	CURRENT_THREAD_INFO(%1)
	rli %sr, pu32_ret_to_kernelspace
	jl %rp, %sr
	rli %sr, resume_kernel
	jnz %1, %sr
	CURRENT_THREAD_INFO(%1)
	inc %1, TASK_TI_FLAGS
	ld32 %1, %1
	cpy %2, %1
	li %sr, _TIF_WORK_MASK
	and %2, %sr
	rli %sr, sysret
	jz %2, %sr
	# %1 has thread_info.flags.
	cpy %2, %1
	li %sr, _TIF_NEED_RESCHED
	and %2, %sr
	rli %sr, 0f
	jz %2, %sr
	rli %sr, schedule
	rli %rp, ret_from_exception
	j %sr
	0: # Handle (_TIF_SIGPENDING | _TIF_NOTIFY_RESUME).
	# %1 has thread_info.flags.
	rli %sr, do_notify_resume
	rli %rp, sysret
	j %sr
resume_kernel:
	#ifdef CONFIG_PREEMPTION
	CURRENT_THREAD_INFO(%1)
	inc %1, TASK_TI_PREEMPT_COUNT
	ld32 %1, %1
	rli %sr, sysret
	jnz %1, %sr
	CURRENT_THREAD_INFO(%1)
	inc %1, TASK_TI_FLAGS
	ld32 %1, %1
	li %sr, _TIF_NEED_RESCHED
	and %1, %sr
	rli %sr, sysret
	jz %1, %sr
	rli %sr, arch_local_save_flags
	jl %rp, %sr
	# arch_local_save_flags() returns
	# ARCH_IRQ_ENABLED (0) or ARCH_IRQ_DISABLED (1).
	rli %sr, sysret
	jz %1, %sr # sysret if irq enabled.
	rli %sr, preempt_schedule_irq
	rli %rp, ret_from_exception
	j %sr
	#endif
sysret:
	CURRENT_THREAD_INFO(%2)
	inc %2, TASK_TI_TASK
	ld32 %2, %2
	li8 %1, 0
	li %sr, __NR_PU32_switch_to
	syscall

GLOBAL(ret_from_fork)
	# %1 is expected to have struct task_struct *prev.
	rli %sr, schedule_tail
	rli %rp, ret_from_exception
	j %sr

GLOBAL(ret_from_kernel_thread)
	# %1 is expected to have struct task_struct *prev.
	rli %sr, schedule_tail
	jl %rp, %sr
	# Execute kernel-thread function after
	# retrieving its pointer and argument.
	CURRENT_THREAD_INFO(%1)
	cpy %sr, %1
	inc %sr, TASK_TI_KPC
	ld32 %sr, %sr
	inc %1, TASK_TI_KR1
	ld32 %1, %1
	rli %rp, ret_from_exception
	j %sr

GLOBAL(__switch_to)
	# Context switching from "prev" (current) task to "next" task.
	# %1 and %2 are expected to be holding the arguments; ie:
	# struct task_struct *__switch_to (
	# 	struct task_struct *prev, struct task_struct *next);
	li %sr, __NR_PU32_switch_to
	syscall
	j %rp
	# __NR_PU32_switch_to() takes care of setting the return value.
