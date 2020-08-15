// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef __ASM_PU32_FUTEX_H
#define __ASM_PU32_FUTEX_H

#include <linux/futex.h>
#include <linux/uaccess.h>

#include <asm/errno.h>

#define __futex_atomic_op(insn, ret, oldval, uaddr, oparg)	\
	__asm__ __volatile__ (					\
		"1:\n"						\
		"ld32 %1, %2\n"					\
		insn"\n"					\
		"cpy %%sr, %1\n"				\
		"cldst32 %3, %2\n"				\
		"sne %3, %%sr\n"				\
		"rli %%sr, 3f; jnz %3, %%sr\n"			\
		"rli %%sr, 4f; j %%sr\n"			\
		"2:\n"						\
		"li %0, %4\n"					\
		"rli %%sr, 4f; j %%sr\n"			\
		"3:\n" 						\
		"li %0, %5\n"					\
		"4:\n"						\
		".section __ex_table,\"a\"\n"			\
		".p2align 1\n"					\
		".long 1b, 2b\n"				\
		".previous\n"					\
		: "=&r" (ret),					\
		  "=&r" (oldval)				\
		: "r"   (uaddr),				\
		  "r"   (oparg),				\
		  "i"   (-EFAULT),				\
		  "i"   (-EAGAIN)				\
		: "%sr")					\

static inline int arch_futex_atomic_op_inuser (
	int op, u32 oparg, int *oval, u32 __user *uaddr) {

	if (!access_ok(uaddr, sizeof(u32)))
		return -EFAULT;

	int ret = 0; // Modified by __futex_atomic_op() on error.

	u32 oldval; // Modified by __futex_atomic_op().

	switch (op) {
		case FUTEX_OP_SET:
			__futex_atomic_op ("", ret, oldval, uaddr, oparg);
			break;
		case FUTEX_OP_ADD:
			__futex_atomic_op ("add %3, %1", ret, oldval, uaddr, oparg);
			break;
		case FUTEX_OP_OR:
			__futex_atomic_op ("or %3, %1", ret, oldval, uaddr, oparg);
			break;
		case FUTEX_OP_ANDN:
			__futex_atomic_op ("not %3, %3; and %3, %1", ret, oldval, uaddr, oparg);
			break;
		case FUTEX_OP_XOR:
			__futex_atomic_op ("xor %3, %1", ret, oldval, uaddr, oparg);
			break;
		default:
			ret = -ENOSYS;
	}

	if (ret == 0)
		*oval = oldval;

	return ret;
}

static inline int futex_atomic_cmpxchg_inatomic (
	u32 *uval, u32 __user *uaddr, u32 oldval, u32 newval) {
	return arch_futex_atomic_op_inuser (FUTEX_OP_SET, newval, &oldval, uaddr);
}

#endif /* __ASM_PU32_FUTEX_H */
