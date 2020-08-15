// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#include <linux/stringify.h>
#include <linux/export.h>
#include <linux/types.h>

#if __SIZEOF_POINTER__ == 8
#define __SIZEOF_POINTER_ORDER__ 3
#elif __SIZEOF_POINTER__== 4
#define __SIZEOF_POINTER_ORDER__ 2
#else
#error unsupported __SIZEOF_POINTER__ value
#endif

// Similar to uintcpy(), but copies from bottom to top.
// Returns dst.
void *uintcpy2 (void *dst, const void *src, unsigned long cnt); __asm__ (
	".text\n"
	".global  uintcpy2\n"
	".type    uintcpy2, @function\n"
	".p2align 1\n"
	"uintcpy2:\n"

	"rli %sr, 1f\n"
	"jz %3, %sr\n"
	"li8 %sr, "__stringify(__SIZEOF_POINTER_ORDER__)"\n"
	"sll %3, %sr\n"
	"add %1, %3\n"
	"add %2, %3\n"
	"rli %sr, 0f; 0:\n"
	"inc8 %1, -"__stringify(__SIZEOF_POINTER__)"\n"
	"inc8 %2, -"__stringify(__SIZEOF_POINTER__)"\n"
	"inc8 %3, -"__stringify(__SIZEOF_POINTER__)"\n"
	"ld %4, %2; st %4, %1\n"
	"jnz %3, %sr\n"
	"1: j %rp\n"

	".size    uintcpy2, (. - uintcpy2)\n");

// Similar to u8cpy(), but copies from bottom to top.
// Returns dst.
void *u8cpy2 (void *dst, const void *src, unsigned long cnt); __asm__ (
	".text\n"
	".global  u8cpy2\n"
	".type    u8cpy2, @function\n"
	".p2align 1\n"
	"u8cpy2:\n"

	"rli %sr, 1f\n"
	"jz %3, %sr\n"
	"add %1, %3\n"
	"add %2, %3\n"
	"rli %sr, 0f; 0:\n"
	"inc8 %1, -1\n"
	"inc8 %2, -1\n"
	"inc8 %3, -1\n"
	"ld8 %4, %2; st8 %4, %1\n"
	"jnz %3, %sr\n"
	"1: j %rp\n"

	".size    u8cpy2, (. - u8cpy2)\n");

void *memcpy (void *dest, const void *src, size_t count);

void *memmove (void *dest, const void *src, size_t count) {
	if (dest <= src) {
		memcpy (dest, src, count);
		return dest;
	} else {
		if (((unsigned long)dest|(unsigned long)src|(unsigned long)count)%sizeof(unsigned long))
			return u8cpy2 (dest, src, count);
		else
			return uintcpy2 (dest, src, count/sizeof(unsigned long));
	}
}
EXPORT_SYMBOL(memmove);
