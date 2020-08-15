// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef __ASM_PU32_SYSCALL_H
#define __ASM_PU32_SYSCALL_H

#include <uapi/asm/unistd.h>

#if (__NR_syscalls != 440)
#error __NR_syscalls has changed; update binutils-sim glibc newlib
#endif

#ifndef __ASSEMBLY__
extern void *syscall_table[];
#endif /* !__ASSEMBLY__ */

#define __NR_PU32_syscalls_start (__NR_syscalls+2)
#ifndef __NR_lseek
#define __NR_lseek (__NR_PU32_syscalls_start+0)
#endif
#define __NR_PU32_switch_to (__NR_PU32_syscalls_start+1)
#define __NR_PU32_syscalls_end (__NR_PU32_syscalls_start+2)

#endif /* __ASM_PU32_SYSCALL_H */
