// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef __ASM_PU32_SYSCALL_H
#define __ASM_PU32_SYSCALL_H

#include <uapi/asm/unistd.h>

#if (__NR_arch_specific_syscall != 244)
#error __NR_arch_specific_syscall has changed; update binutils-sim fontamsoc-sw/bios; rebuild glibc
#endif

#ifndef __NR_lseek
#define __NR_lseek (__NR_arch_specific_syscall+0)
#endif

#define __NR_PU32_syscalls_start (__NR_arch_specific_syscall+1)

#define __NR_PU32_switch_to (__NR_PU32_syscalls_start+0)

#define __NR_PU32_syscalls_end (__NR_PU32_syscalls_start+0)

#ifndef __ASSEMBLY__

#include <uapi/linux/audit.h>
#include <linux/err.h>
#include <uapi/asm/ptrace.h>

struct task_struct;

extern void *syscall_table[];

static inline long syscall_get_nr (struct task_struct *task, struct pt_regs *regs) {
	return regs->sr;
}

static inline long syscall_get_error (struct task_struct *task, struct pt_regs *regs) {
	return IS_ERR_VALUE(regs->r1) ? regs->r1 : 0;
}

static inline long syscall_get_return_value (struct task_struct *task, struct pt_regs *regs) {
	return regs->r1;
}

static inline void syscall_set_return_value (
	struct task_struct *task, struct pt_regs *regs,
	int error, long val) {
	regs->r1 = (error ?: val);
}

static inline void syscall_get_arguments (
	struct task_struct *task, struct pt_regs *regs, unsigned long *args) {
	*args++ = regs->r1;
	*args++ = regs->r2;
	*args++ = regs->r3;
	*args++ = regs->r4;
	*args++ = regs->r5;
	*args   = regs->r6;
}

static inline int syscall_get_arch (struct task_struct *task) {
	return AUDIT_ARCH_PU32;
}

#endif /* !__ASSEMBLY__ */

#endif /* __ASM_PU32_SYSCALL_H */
