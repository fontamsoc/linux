// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#include <linux/sched.h>
#include <linux/stacktrace.h>

#include <asm/thread_info.h>

static void dump_trace (
	struct task_struct *tsk, struct stack_trace *trace) {

	unsigned long *eos = (unsigned long *)end_of_stack(tsk);

	unsigned long ksp = task_thread_info(tsk)->ksp;
	unsigned long *start = (unsigned long *)(ksp + sizeof(struct pu32_pt_regs));
	unsigned long *end = (unsigned long *)((ksp & ~(THREAD_SIZE - 1)) +
		((struct pu32_pt_regs *)ksp)->prev_ksp_offset);

	while (1) {
		extern char _text[];
		unsigned long addr = ((struct pu32_pt_regs *)ksp)->regs.pc;
		if (addr != *start &&
			__kernel_text_address(addr) && (addr >= (unsigned long)_text)) {
			if (trace->nr_entries >= trace->max_entries)
				return;
			trace->entries[trace->nr_entries++] = addr;
		}
		if (start < end) {
			addr = ((struct pu32_pt_regs *)ksp)->regs.rp;
			if (addr != *start &&
				__kernel_text_address(addr) && (addr >= (unsigned long)_text)) {
				if (trace->nr_entries >= trace->max_entries)
					return;
				trace->entries[trace->nr_entries++] = addr;
			}
			do {
				addr = *start;
				if (__kernel_text_address(addr) && (addr >= (unsigned long)_text)) {
					if (trace->nr_entries >= trace->max_entries)
						return;
					trace->entries[trace->nr_entries++] = addr;
				}
				++start;
			} while (start < end);
		}
		if (end == eos)
			return;
		ksp = (unsigned long)end;
		start = (unsigned long *)(ksp + sizeof(struct pu32_pt_regs));
		end = (unsigned long *)((ksp & ~(THREAD_SIZE - 1)) +
			((struct pu32_pt_regs *)ksp)->prev_ksp_offset);
	}
}

void save_stack_trace (struct stack_trace *trace) {
	dump_trace (current, trace);
}
EXPORT_SYMBOL_GPL(save_stack_trace);

void save_stack_trace_tsk (
	struct task_struct *tsk, struct stack_trace *trace) {
	dump_trace (tsk, trace);
}
EXPORT_SYMBOL_GPL(save_stack_trace_tsk);
