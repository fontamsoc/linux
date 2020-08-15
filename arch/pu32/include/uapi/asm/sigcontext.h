// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
// (c) William Fonkou Tambe

#ifndef __UAPI_ASM_PU32_SIGCONTEXT_H
#define __UAPI_ASM_PU32_SIGCONTEXT_H

#include <asm/ptrace.h>

struct sigcontext {
	struct pt_regs regs;
};

#endif /* __UAPI_ASM_PU32_SIGCONTEXT_H */
