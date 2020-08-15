// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef __ASM_PU32_PTRACE_H
#define __ASM_PU32_PTRACE_H

#include <asm/percpu.h>
#include <uapi/asm/ptrace.h>
#include <asm/thread_info.h>
#include <pu32.h>

extern unsigned long pu32_kernelmode_stack[NR_CPUS];

#define user_mode(regs) ({ \
	unsigned long sp; asm volatile ("cpy %0, %%sp\n" : "=r"(sp) :: "memory"); \
	(((sp & ~(THREAD_SIZE-1)) == pu32_kernelmode_stack[raw_smp_processor_id()]) ? \
		pu32_in_userspace(current_thread_info()) : \
		pu32_ret_to_userspace(current_thread_info()));})
#define user_stack_pointer(regs) ({ \
	unsigned long sp; asm volatile ("cpy %0, %%sp\n" : "=r"(sp) :: "memory"); \
	(((sp & ~(THREAD_SIZE-1)) == pu32_kernelmode_stack[raw_smp_processor_id()]) ? \
		sp : (regs)->sp);})
#define instruction_pointer(regs) ({ \
	unsigned long sp; asm volatile ("cpy %0, %%sp\n" : "=r"(sp) :: "memory"); \
	(((sp & ~(THREAD_SIZE-1)) == pu32_kernelmode_stack[raw_smp_processor_id()]) ? \
		_RET_IP_ : (regs)->pc);})

typedef enum {
	PU32_PT_REGS_WHICH_SPFPRP,
	PU32_PT_REGS_WHICH_ALL,
} pu32_pt_regs_which;

struct pu32_pt_regs {
	pu32FaultReason	faultreason; // Reason for this pu32_pt_regs.
	unsigned long	sysopcode; // instruction opcode at which this pu32_pt_regs was generated.
	unsigned long		prev_ksp_offset; // Offset to compute previous value of corresponding ti->ksp.
	pu32_pt_regs_which	which; // Determine which registers are valid.
	struct pt_regs		regs;
};

#define pu32_ti_pt_regs(ti)	((struct pu32_pt_regs *)ti->ksp)
#define pu32_tsk_pt_regs(tsk)	((struct pu32_pt_regs *)task_thread_info(tsk)->ksp)
#define task_pt_regs(tsk)	((struct pt_regs *)&pu32_tsk_pt_regs(tsk)->regs)

#endif	/* __ASM_PU32_PTRACE_H */
