// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#include <linux/stringify.h>
#include <linux/export.h>
#include <linux/types.h>

// Copies cnt bytes in uints from memory area src to memory area dst.
// The memory areas must not overlap.
// Returns (dst+(cnt*sizeof(unsigned long))).
void *uintcpy (void *dst, const void *src, unsigned long cnt); __asm__ (
	".text\n"
	".type    uintcpy, @function\n"
	".p2align 1\n"
	"uintcpy:\n"

	"rli %sr, 2f\n"
	"jz %3, %sr\n"
	"_uintcpy: cpy %4, %3\n"
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
	"1: jz %4, %rp\n"
	"cpy %3, %4; li8 %4, 0\n"
	"rli %sr, _u8cpy\n"
	"j %sr\n"
	"2: j %rp\n"

	".size    uintcpy, (. - uintcpy)\n");

// Copies cnt bytes from memory area src to memory area dst.
// If argument cnt_for_uintcpy is non-null, it is the number of bytes
// to copy using uintcpy() from (dst+cnt) which must be aligned to uint.
// The memory areas must not overlap.
// Returns (dst+cnt+cnt_for_uintcpy).
void *u8cpy (void *dst, const void *src, unsigned long cnt, unsigned long cnt_for_uintcpy); __asm__ (
	".text\n"
	".type    u8cpy, @function\n"
	".p2align 1\n"
	"u8cpy:\n"

	"rli %sr, 1f\n"
	"jz %3, %sr\n"
	"_u8cpy: rli %sr, 0f\n"
	"0: ld8 %5, %2; st8 %5, %1\n"
	"inc8 %1, 1\n"
	"inc8 %2, 1\n"
	"inc8 %3, -1\n"
	"jnz %3, %sr\n"
	"jz %4, %rp\n"
	"cpy %3, %4\n"
	"rli %sr, _uintcpy\n"
	"j %sr\n"
	"1: j %rp\n"

	".size    u8cpy, (. - u8cpy)\n");

void *memcpy (void *dest, const void *src, size_t count) {
	if (((unsigned long)dest|(unsigned long)src)%sizeof(unsigned long)) {
		if (((unsigned long)dest^(unsigned long)src)%sizeof(unsigned long))
			u8cpy (dest, src, count, 0);
		else {
			unsigned long n = (sizeof(unsigned long)-((unsigned long)dest%sizeof(unsigned long)));
			if (n > count)
				u8cpy (dest, src, count, 0);
			else
				u8cpy (dest, src, n, count-n);
		}
	} else
		uintcpy (dest, src, count);
	return dest;
}
EXPORT_SYMBOL(memcpy);
