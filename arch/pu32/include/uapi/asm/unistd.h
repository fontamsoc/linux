// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
// (c) William Fonkou Tambe

// NOTE: There is no #include guard on purpose.

#define sys_mmap2 sys_mmap_pgoff

#define __ARCH_WANT_SYS_CLONE
#define __ARCH_WANT_TIME32_SYSCALLS
#define __ARCH_WANT_STAT64
#define __ARCH_WANT_SET_GET_RLIMIT

#include <asm-generic/unistd.h>

// __NR_PU32_syscalls_start in <asm/syscall.h> must be adjusted for new __NR_ added here.
