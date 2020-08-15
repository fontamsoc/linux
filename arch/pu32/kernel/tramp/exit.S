// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#include <linux/linkage.h>

#include <uapi/asm/unistd.h>

	.section .pu32tramp.text, "ax"
ENTRY(__tramp_exit)
	li %sr, __NR_exit
	syscall
ENDPROC(__tramp_exit)
