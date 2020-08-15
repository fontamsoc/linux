// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note
// (c) William Fonkou Tambe

#ifndef __UAPI_ASM_PU32_PTRACE_H
#define __UAPI_ASM_PU32_PTRACE_H

struct pt_regs {
	unsigned long sp;
	unsigned long r1;
	unsigned long r2;
	unsigned long r3;
	unsigned long r4;
	unsigned long r5;
	unsigned long r6;
	unsigned long r7;
	unsigned long r8;
	unsigned long r9;
	unsigned long tp;
	unsigned long r11;
	unsigned long r12;
	unsigned long sr;
	unsigned long fp;
	unsigned long rp;
	unsigned long pc;
};

#define PTRACE_GET_THREAD_AREA 25

#endif /* __UAPI_ASM_PU32_PTRACE_H */
