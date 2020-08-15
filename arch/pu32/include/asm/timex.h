// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef __ASM_PU32_TIMEX_H
#define __ASM_PU32_TIMEX_H

typedef u64 cycles_t;

static inline cycles_t get_cycles (void) {
	inline unsigned long get_cycles_hi (void) {
		unsigned long hi;
		asm volatile ("getclkcyclecnth %0\n" : "=r"(hi) :: "memory");
		return hi;
	}
	unsigned long lo, hi;
	do {
		hi = get_cycles_hi();
		asm volatile ("getclkcyclecnt %0\n"  : "=r"(lo) :: "memory");
	} while (hi != get_cycles_hi());
	return (((u64)hi << 32) | lo);
}
#define get_cycles get_cycles

#define ARCH_HAS_READ_CURRENT_TIMER
static inline int read_current_timer (unsigned long *timer_val) {
	*timer_val = get_cycles();
	return 0;
}

static inline unsigned long random_get_entropy (void) {
	static unsigned long o = 0x7584486f /* Just a random constant */ ^
		__COUNTER__ /* ### Find a more random preprocessor macro */;
	return (o ^= get_cycles());
}
#define random_get_entropy random_get_entropy

#endif /* __ASM_PU32_TIMEX_H */
