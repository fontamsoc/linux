// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#include <linux/stringify.h>
#include <linux/export.h>
#include <linux/types.h>

// Set cnt bytes in uints of memory area s using c.
// Returns (s+(cnt*sizeof(unsigned long))).
void *uintset (void *s, int c, unsigned long cnt); __asm__ (
	".text\n"
	".type    uintset, @function\n"
	".p2align 1\n"
	"uintset:\n"

	"rli %sr, 2f\n"
	"jz %3, %sr\n"
	"_uintset: cpy %4, %3\n"
	"li8 %5, "__stringify(__SIZEOF_POINTER__)"\n"
	"modu %4, %5\n"
	"divu %3, %5\n"
	"rli %sr, 1f\n"
	"jz %3, %sr\n"
	"rli %sr, 0f\n"
	"0: st %2, %1\n"
	"inc8 %1, "__stringify(__SIZEOF_POINTER__)"\n"
	"inc8 %3, -1\n"
	"jnz %3, %sr\n"
	"1: jz %4, %rp\n"
	"cpy %3, %4; li8 %4, 0\n"
	"rli %sr, _u8set\n"
	"j %sr\n"
	"2: j %rp\n"

	".size    uintset, (. - uintset)\n");

// Set cnt bytes of memory area s using c.
// If argument cnt_for_uintset is non-null, it is the number of bytes
// to set using uintset() from (s+cnt) which must be aligned to uint.
// Returns (s+cnt+cnt_for_uintset).
void *u8set (void *s, int c, unsigned long cnt, unsigned long cnt_for_uintset); __asm__ (
	".text\n"
	".type    u8set, @function\n"
	".p2align 1\n"
	"u8set:\n"

	"rli %sr, 1f\n"
	"jz %3, %sr\n"
	"_u8set: rli %sr, 0f\n"
	"0: st8 %2, %1\n"
	"inc8 %1, 1\n"
	"inc8 %3, -1\n"
	"jnz %3, %sr\n"
	"jz %4, %rp\n"
	"cpy %3, %4\n"
	"rli %sr, _uintset\n"
	"j %sr\n"
	"1: j %rp\n"

	".size    u8set, (. - u8set)\n");

void *memset (void *s, int c, size_t cnt) {
	inline unsigned int c8to32 (unsigned char c8) {
		#if __SIZEOF_POINTER__ != 4
		#error c8to32() invalid !!!
		#endif
		unsigned short c16 = (((unsigned short)c8<<8)|c8);
		return (((unsigned int)c16<<16)|c16);
	}
	unsigned long x = (((unsigned long)s)%sizeof(unsigned long));
	if (x) {
		unsigned long n = (sizeof(unsigned long)-x);
		if (n > cnt)
			u8set (s, c, cnt, 0);
		else
			u8set (s, c8to32(c), n, cnt-n);
	} else
		uintset (s, c8to32(c), cnt);
	return s;
}
EXPORT_SYMBOL(memset);
