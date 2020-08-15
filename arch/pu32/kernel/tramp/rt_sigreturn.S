// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#include <linux/linkage.h>

#include <uapi/asm/unistd.h>

	.section .pu32tramp.text, "ax"
ENTRY(__tramp_rt_sigreturn)
	li %sr, __NR_rt_sigreturn
	syscall
END(__tramp_rt_sigreturn)
