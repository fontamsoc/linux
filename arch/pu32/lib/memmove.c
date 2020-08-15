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
	".type    uintcpy2, @function\n"
	".p2align 1\n"
	"uintcpy2:\n"

	"rli %sr, 2f\n"
	"jz %3, %sr\n"
	"_uintcpy2: cpy %4, %3\n"
	"li8 %5, "__stringify(__SIZEOF_POINTER__)"\n"
	"modu %4, %5\n"
	"divu %3, %5\n"
	"rli %sr, 1f\n"
	"jz %3, %sr\n"
	"rli %sr, 0f; 0:\n"
	"inc8 %1, -"__stringify(__SIZEOF_POINTER__)"\n"
	"inc8 %2, -"__stringify(__SIZEOF_POINTER__)"\n"
	"inc8 %3, -1\n"
	"ld %5, %2; st %5, %1\n"
	"jnz %3, %sr\n"
	"1: jz %4, %rp\n"
	"cpy %3, %4; li8 %4, 0\n"
	"rli %sr, _u8cpy2\n"
	"j %sr\n"
	"2: j %rp\n"

	".size    uintcpy2, (. - uintcpy2)\n");

// Similar to u8cpy(), but copies from bottom to top.
// Returns dst.
void *u8cpy2 (void *dst, const void *src, unsigned long cnt, unsigned long cnt_for_uintcpy2); __asm__ (
	".text\n"
	".type    u8cpy2, @function\n"
	".p2align 1\n"
	"u8cpy2:\n"

	"rli %sr, 1f\n"
	"jz %3, %sr\n"
	"_u8cpy2: rli %sr, 0f; 0:\n"
	"inc8 %1, -1\n"
	"inc8 %2, -1\n"
	"inc8 %3, -1\n"
	"ld8 %5, %2; st8 %5, %1\n"
	"jnz %3, %sr\n"
	"jz %4, %rp\n"
	"cpy %3, %4\n"
	"rli %sr, _uintcpy2\n"
	"j %sr\n"
	"1: j %rp\n"

	".size    u8cpy2, (. - u8cpy2)\n");

void *memcpy (void *dest, const void *src, size_t count);

void *memmove (void *dest, const void *src, size_t count) {
	if (((unsigned long)dest - (unsigned long)src) >= (unsigned long)count)
		return memcpy (dest, src, count);
	else {
		dest += count;
		src += count;
		if (((unsigned long)dest|(unsigned long)src)%sizeof(unsigned long)) {
			if (((unsigned long)dest^(unsigned long)src)%sizeof(unsigned long))
				return u8cpy2 (dest, src, count, 0);
			else {
				unsigned long n = ((unsigned long)dest%sizeof(unsigned long));
				if (n > count)
					return u8cpy2 (dest, src, count, 0);
				else
					return u8cpy2 (dest, src, n, count-n);
			}
		} else
			return uintcpy2 (dest, src, count);
	}
}
EXPORT_SYMBOL(memmove);
