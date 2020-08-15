// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef __ASM_PU32_SYSCALL_H
#define __ASM_PU32_SYSCALL_H

#include <uapi/asm/unistd.h>

#if (__NR_arch_specific_syscall != 244)
#error __NR_arch_specific_syscall has changed; update binutils-sim fontamsoc-sw/bios; rebuild glibc
#endif

#ifndef __ASSEMBLY__
extern void *syscall_table[];
#endif /* !__ASSEMBLY__ */

#ifndef __NR_lseek
#define __NR_lseek (__NR_arch_specific_syscall+0)
#endif

#define __NR_PU32_syscalls_start (__NR_arch_specific_syscall+1)

#define __NR_PU32_switch_to (__NR_PU32_syscalls_start+0)

#define __NR_PU32_syscalls_end (__NR_PU32_syscalls_start+0)

#endif /* __ASM_PU32_SYSCALL_H */
