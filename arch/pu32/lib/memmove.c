// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#include <linux/stringify.h>
#include <linux/export.h>
#include <linux/types.h>

// Similar to uintcpy(), but copies from bottom to top.
// Returns dst.
void *uintcpy2 (void *dst, const void *src, unsigned long cnt); __asm__ (
	".text\n"
	".type    uintcpy2, @function\n"
	".p2align 1\n"
	"uintcpy2:\n"

	"jz %3, %rp\n"
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

	".size    uintcpy2, (. - uintcpy2)\n");

// uintcpy8() -> uintcpy().
void *uintcpy2_8uint_1uint (void *dst, const void *src, unsigned long cnt8, unsigned long cnt); __asm__ (
	".text\n"
	".type    uintcpy2_8uint_1uint, @function\n"
	".p2align 1\n"
	"uintcpy2_8uint_1uint:\n"

	"jz %3, %rp\n"
	"_uintcpy28: rli %sr, 0f; 0:\n"
	"inc8 %1, -"__stringify(__SIZEOF_POINTER__)"\n"
	"inc8 %2, -"__stringify(__SIZEOF_POINTER__)"\n"
	"ld %5, %2; st %5, %1\n"
	"inc8 %1, -"__stringify(__SIZEOF_POINTER__)"\n"
	"inc8 %2, -"__stringify(__SIZEOF_POINTER__)"\n"
	"ld %5, %2; st %5, %1\n"
	"inc8 %1, -"__stringify(__SIZEOF_POINTER__)"\n"
	"inc8 %2, -"__stringify(__SIZEOF_POINTER__)"\n"
	"ld %5, %2; st %5, %1\n"
	"inc8 %1, -"__stringify(__SIZEOF_POINTER__)"\n"
	"inc8 %2, -"__stringify(__SIZEOF_POINTER__)"\n"
	"ld %5, %2; st %5, %1\n"
	"inc8 %1, -"__stringify(__SIZEOF_POINTER__)"\n"
	"inc8 %2, -"__stringify(__SIZEOF_POINTER__)"\n"
	"ld %5, %2; st %5, %1\n"
	"inc8 %1, -"__stringify(__SIZEOF_POINTER__)"\n"
	"inc8 %2, -"__stringify(__SIZEOF_POINTER__)"\n"
	"ld %5, %2; st %5, %1\n"
	"inc8 %1, -"__stringify(__SIZEOF_POINTER__)"\n"
	"inc8 %2, -"__stringify(__SIZEOF_POINTER__)"\n"
	"ld %5, %2; st %5, %1\n"
	"inc8 %1, -"__stringify(__SIZEOF_POINTER__)"\n"
	"inc8 %2, -"__stringify(__SIZEOF_POINTER__)"\n"
	"ld %5, %2; st %5, %1\n"
	"inc8 %3, -8*"__stringify(__SIZEOF_POINTER__)"\n"
	"jnz %3, %sr\n"
	"jz %4, %rp\n"
	"cpy %3, %4\n"
	"rli %sr, _uintcpy2\n"
	"j %sr\n"

	".size    uintcpy2_8uint_1uint, (. - uintcpy2_8uint_1uint)\n");

// Similar to u8cpy(), but copies from bottom to top.
// Returns dst.
void *u8cpy2 (void *dst, const void *src, unsigned long cnt, unsigned long cnt_for_uintcpy); __asm__ (
	".text\n"
	".type    u8cpy2, @function\n"
	".p2align 1\n"
	"u8cpy2:\n"

	"jz %3, %rp\n"
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

	".size    u8cpy2, (. - u8cpy2)\n");

// u8cpy2() -> uintcpy8() -> uintcpy().
void *u8cpy2_1u8_8uint_1uint (void *dst, const void *src, unsigned long cnt, unsigned long cnt_for_uintcpy8, unsigned long cnt_for_uintcpy1); __asm__ (
	".text\n"
	".type    u8cpy2_1u8_8uint_1uint, @function\n"
	".p2align 1\n"
	"u8cpy2_1u8_8uint_1uint:\n"

	"jz %3, %rp\n"
	"rli %sr, 0f; 0:\n"
	"inc8 %1, -1\n"
	"inc8 %2, -1\n"
	"inc8 %3, -1\n"
	"ld8 %6, %2; st8 %6, %1\n"
	"jnz %3, %sr\n"
	"jz %4, %rp\n"
	"cpy %3, %4\n"
	"cpy %4, %5\n"
	"rli %sr, _uintcpy28\n"
	"j %sr\n"

	".size    u8cpy2_1u8_8uint_1uint, (. - u8cpy2_1u8_8uint_1uint)\n");

// u8cpy8() -> u8cpy().
void *u8cpy2_8u8_1u8 (void *dst, const void *src, unsigned long cnt8, unsigned long cnt); __asm__ (
	".text\n"
	".type    u8cpy2_8u8_1u8, @function\n"
	".p2align 1\n"
	"u8cpy2_8u8_1u8:\n"

	"jz %3, %rp\n"
	"rli %sr, 0f; 0:\n"
	"inc8 %1, -1\n"
	"inc8 %2, -1\n"
	"ld8 %5, %2; st8 %5, %1\n"
	"inc8 %1, -1\n"
	"inc8 %2, -1\n"
	"ld8 %5, %2; st8 %5, %1\n"
	"inc8 %1, -1\n"
	"inc8 %2, -1\n"
	"ld8 %5, %2; st8 %5, %1\n"
	"inc8 %1, -1\n"
	"inc8 %2, -1\n"
	"ld8 %5, %2; st8 %5, %1\n"
	"inc8 %1, -1\n"
	"inc8 %2, -1\n"
	"ld8 %5, %2; st8 %5, %1\n"
	"inc8 %1, -1\n"
	"inc8 %2, -1\n"
	"ld8 %5, %2; st8 %5, %1\n"
	"inc8 %1, -1\n"
	"inc8 %2, -1\n"
	"ld8 %5, %2; st8 %5, %1\n"
	"inc8 %1, -1\n"
	"inc8 %2, -1\n"
	"ld8 %5, %2; st8 %5, %1\n"
	"inc8 %3, -8*1\n"
	"jnz %3, %sr\n"
	"jz %4, %rp\n"
	"cpy %3, %4\n"
	"li8 %4, 0\n"
	"rli %sr, _u8cpy2\n"
	"j %sr\n"

	".size    u8cpy2_8u8_1u8, (. - u8cpy2_8u8_1u8)\n");

void *memcpy (void *dest, const void *src, size_t count);

void *memmove (void *dest, const void *src, size_t count) {
	if (((unsigned long)dest - (unsigned long)src) >= (unsigned long)count)
		return memcpy (dest, src, count);
	else {
		dest += count;
		src += count;
		if (((unsigned long)dest|(unsigned long)src)%sizeof(unsigned long)) {
			if (((unsigned long)dest^(unsigned long)src)%sizeof(unsigned long))
				if (count >= (8*sizeof(unsigned char))) {
				unsigned long cnt_for_u8cpy1 = (count % (8*sizeof(unsigned char)));
				unsigned long cnt_for_u8cpy8 = (count - cnt_for_u8cpy1);
				return u8cpy2_8u8_1u8 (dest, src, cnt_for_u8cpy8, cnt_for_u8cpy1); // u8cpy8() -> u8cpy().
			} else
				return u8cpy2 (dest, src, count, 0);
			else {
				unsigned long n = ((unsigned long)dest%sizeof(unsigned long));
				if (n > count)
					return u8cpy2 (dest, src, count, 0);
				else {
					unsigned long cnt_for_uintcpy = (count-n);
					if (cnt_for_uintcpy >= (8*sizeof(unsigned long))) {
						unsigned long cnt_for_uintcpy1 = (cnt_for_uintcpy % (8*sizeof(unsigned long)));
						unsigned long cnt_for_uintcpy8 = (cnt_for_uintcpy - cnt_for_uintcpy1);
						return u8cpy2_1u8_8uint_1uint (dest, src, n, cnt_for_uintcpy8, cnt_for_uintcpy1); // u8cpy() -> uintcpy8() -> uintcpy().
					} else
						return u8cpy2 (dest, src, n, cnt_for_uintcpy);
				}
			}
		} else if (count >= (8*sizeof(unsigned long))) {
			unsigned long cnt_for_uintcpy1 = (count % (8*sizeof(unsigned long)));
			unsigned long cnt_for_uintcpy8 = (count - cnt_for_uintcpy1);
			return uintcpy2_8uint_1uint (dest, src, cnt_for_uintcpy8, cnt_for_uintcpy1); // uintcpy8() -> uintcpy().
		} else
			return uintcpy2 (dest, src, count);
	}
}
EXPORT_SYMBOL(memmove);
