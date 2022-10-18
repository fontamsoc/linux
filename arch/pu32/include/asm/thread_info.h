// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef __ASM_PU32_THREAD_INFO_H
#define __ASM_PU32_THREAD_INFO_H

#include <asm/page.h>

#define THREAD_SIZE_ORDER CONFIG_KERNEL_STACK_ORDER
#define THREAD_SIZE ((1 << CONFIG_KERNEL_STACK_ORDER) * PAGE_SIZE)

#include <pu32.h>

#ifndef __ASSEMBLY__

#include <asm/processor.h>
#include <asm/ptrace.h>
#include <asm/mmu.h>

struct thread_info {
	struct task_struct *task;
	unsigned long      flags;
	unsigned int       cpu;
	unsigned int       last_cpu;
	int                preempt_count;
	unsigned long      in_fault; // Tell whether the thread is executing do_fault().
	pu32FaultReason    faultreason; // Used to save pu32FaultReason when in_fault is non-null.
	unsigned long      ksp, kr1, kpc;
};

#define __HAVE_THREAD_FUNCTIONS
#define task_stack_page(tsk)		((void *)(tsk)->stack)
#define PU32_TI_OFFSET			(THREAD_SIZE - sizeof(struct thread_info))
#define pu32_stack_top(sp)		((unsigned long)sp & ~(THREAD_SIZE - 1))
// pu32_stack_bottom() takes into account the location used by set_task_stack_end_magic().
#define pu32_stack_bottom(sp)		(pu32_stack_top(sp) + (PU32_TI_OFFSET - __SIZEOF_POINTER__))
#define end_of_stack(tsk)		((long unsigned *)pu32_stack_bottom(task_stack_page(tsk)))
#define pu32_get_thread_info(sp)	((struct thread_info *)(pu32_stack_top(sp) + PU32_TI_OFFSET))
#define task_thread_info(tsk)		pu32_get_thread_info(task_stack_page(tsk))
#define pu32_in_userspace(ti) ({ \
	/* To be used before save_pu32umode_regs() or after restore_pu32umode_regs() */ \
	(ti->ksp == pu32_stack_bottom(ti->ksp)); })
#define pu32_ret_to_userspace(ti) \
	/* To be used after save_pu32umode_regs() or before restore_pu32umode_regs().
	   This function must be used instead of (!pu32_ret_to_kernelspace()),
	   because it also checks whether the task is a kernel-thread. */ \
	(!((ti)->task->flags&(PF_KTHREAD | PF_IO_WORKER)) && \
		((ti)->ksp == (unsigned long)((struct pu32_pt_regs *)pu32_stack_bottom((ti)->ksp)-1)))
// setup_thread_stack() does not set ti->ksp (if kernel-thread),
// ti->kr1, ti->kpc, as they are set by copy_thread().
#define setup_thread_stack(tsk, orig)  {					\
	struct thread_info *tsk_ti = task_thread_info(tsk);			\
	tsk_ti->task = (tsk);							\
	struct thread_info *orig_ti = task_thread_info(orig);			\
	tsk_ti->flags = orig_ti->flags;						\
	tsk_ti->cpu = orig_ti->cpu;						\
	tsk_ti->last_cpu = orig_ti->cpu;					\
	tsk_ti->preempt_count = orig_ti->preempt_count;				\
	tsk_ti->in_fault = orig_ti->in_fault;					\
	if (!(orig->flags&(PF_KTHREAD | PF_IO_WORKER))) {			\
		unsigned long orig_ti_ksp = orig_ti->ksp;			\
		unsigned long tsk_ti_ksp = (					\
			tsk_ti->ksp =						\
				((unsigned long)tsk_ti +			\
				(orig_ti_ksp - (unsigned long)orig_ti)));	\
		memcpy(								\
			(void *)tsk_ti_ksp, (void *)orig_ti_ksp,		\
			(pu32_stack_bottom(orig_ti_ksp) - orig_ti_ksp));	\
	}									\
}

#define INIT_THREAD_INFO(tsk) {				\
	.task		= &tsk,				\
	.cpu		= 0,				\
	.last_cpu	= 0,				\
	.preempt_count	= INIT_PREEMPT_COUNT,		\
	.in_fault	= 0,				\
}

// Must match CURRENT_THREAD_INFO.
#define current_thread_info() ({ \
	struct thread_info *ti; \
	asm volatile ("cpy %0, %%tp\n" : "=r"(ti) :: "memory"); \
	ti; })

#endif /* !__ASSEMBLY__ */

// Compute current_thread_info() in R.
#define CURRENT_THREAD_INFO(R)	\
	cpy R, %tp

#define TIF_SIGPENDING		0	/* signal pending */
#define TIF_NOTIFY_RESUME	1	/* callback before returning to user */
#define TIF_NEED_RESCHED	2	/* rescheduling necessary */
#define TIF_SYSCALL_TRACE	3	/* syscall trace active */
#define TIF_SYSCALL_TRACEPOINT	4	/* syscall tracepoint instrumentation */
#define TIF_SYSCALL_AUDIT	5	/* syscall auditing */
#define TIF_NOTIFY_SIGNAL	7       /* signal notifications exist */
#define TIF_POLLING_NRFLAG	16	/* poll_idle() is TIF_NEED_RESCHED */
#define TIF_MEMDIE		18	/* is terminating due to OOM killer */

#define _TIF_SIGPENDING		(1 << TIF_SIGPENDING)
#define _TIF_NOTIFY_RESUME	(1 << TIF_NOTIFY_RESUME)
#define _TIF_NEED_RESCHED	(1 << TIF_NEED_RESCHED)
#define _TIF_SYSCALL_TRACE	(1 << TIF_SYSCALL_TRACE)
#define _TIF_SYSCALL_TRACEPOINT	(1 << TIF_SYSCALL_TRACEPOINT)
#define _TIF_SYSCALL_AUDIT	(1 << TIF_SYSCALL_AUDIT)
#define _TIF_NOTIFY_SIGNAL	(1 << TIF_NOTIFY_SIGNAL)
#define _TIF_POLLING_NRFLAG	(1 << TIF_POLLING_NRFLAG)
#define _TIF_MEMDIE		(1 << TIF_MEMDIE)

#define _TIF_WORK_MASK (_TIF_SIGPENDING | _TIF_NOTIFY_RESUME | _TIF_NEED_RESCHED | \
                        _TIF_NOTIFY_SIGNAL)

#endif	/* __ASM_PU32_THREAD_INFO_H */
