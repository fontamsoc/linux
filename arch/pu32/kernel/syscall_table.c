// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#include <linux/syscalls.h>

long sys_rt_sigreturn (void);

#undef __SYSCALL
#define __SYSCALL(nr, call) [nr] = (call),

void *syscall_table[__NR_syscalls] = {
	[0 ... __NR_syscalls - 1] = sys_ni_syscall,
	#include <uapi/asm/unistd.h>
};
