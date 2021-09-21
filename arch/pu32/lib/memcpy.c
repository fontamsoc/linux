// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#include <linux/stringify.h>
#include <linux/export.h>
#include <linux/types.h>

// Copies cnt u8s in uints from memory area src to memory area dst.
// The memory areas must not overlap.
// Returns (dst+(cnt*sizeof(unsigned long))).
void *uintcpy (void *dst, const void *src, unsigned long cnt); __asm__ (
	".text\n"
	".type    uintcpy, @function\n"
	".p2align 1\n"
	"uintcpy:\n"

	"rli %sr, 2f\n"
	"jz %3, %sr\n"
	"cpy %4, %3\n"
	"li8 %5, "__stringify(__SIZEOF_POINTER__)"\n"
	"modu %4, %5\n"
	"divu %3, %5\n"
	"rli %sr, 1f\n"
	"jz %3, %sr\n"
	"rli %sr, 0f\n"
	"0: ld %5, %2; st %5, %1\n"
	"inc8 %1, "__stringify(__SIZEOF_POINTER__)"\n"
	"inc8 %2, "__stringify(__SIZEOF_POINTER__)"\n"
	"inc8 %3, -1\n"
	"jnz %3, %sr\n"
	"1: cpy %3, %4\n"
	"rli %sr, u8cpy\n"
	"jnz %3, %sr\n"
	"2: j %rp\n"

	".size    uintcpy, (. - uintcpy)\n");

// Copies cnt u8s from memory area src to memory area dst.
// The memory areas must not overlap.
// Returns (dst+cnt).
void *u8cpy (void *dst, const void *src, unsigned long cnt); __asm__ (
	".text\n"
	".type    u8cpy, @function\n"
	".p2align 1\n"
	"u8cpy:\n"

	"rli %sr, 1f\n"
	"jz %3, %sr\n"
	"rli %sr, 0f\n"
	"0: ld8 %4, %2; st8 %4, %1\n"
	"inc8 %1, 1\n"
	"inc8 %2, 1\n"
	"inc8 %3, -1\n"
	"jnz %3, %sr\n"
	"1: j %rp\n"

	".size    u8cpy, (. - u8cpy)\n");

void *memcpy (void *dest, const void *src, size_t count) {
	if (((unsigned long)dest|(unsigned long)src)%sizeof(unsigned long))
		u8cpy (dest, src, count);
	else
		uintcpy (dest, src, count);
	return dest;
}
EXPORT_SYMBOL(memcpy);
