// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef __ASM_PU32_PROCESSOR_H
#define __ASM_PU32_PROCESSOR_H

#include <asm/ptrace.h>

// Higher boundary of a userspace task virtual-memory.
#define TASK_SIZE	(0xE0000000 /* 3.5GB */)

#define STACK_TOP_MAX	TASK_SIZE
#define STACK_TOP	STACK_TOP_MAX

// Lower boundary of the mmap virtual-memory area.
extern unsigned long pu32_TASK_UNMAPPED_BASE;
#define TASK_UNMAPPED_BASE pu32_TASK_UNMAPPED_BASE

#define cpu_relax() asm volatile("preemptctx\n" ::: "memory")

// thread_info is used instead.
struct thread_struct {};
#define INIT_THREAD {}

#define KSTK_ESP(tsk)	(task_pt_regs(tsk)->sp)
#define KSTK_EIP(tsk)	(task_pt_regs(tsk)->pc)

// Free all resources held by a thread.
static inline void release_thread (struct task_struct *dead_task) {}

// Do necessary setup to start up a newly executed thread.
extern void start_thread (struct pt_regs *regs, unsigned long pc, unsigned long sp);

unsigned long __get_wchan (struct task_struct *p);

#endif /* __ASM_PU32_PROCESSOR_H */
