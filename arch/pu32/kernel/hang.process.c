// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

// Function that must be used instead of panic() within pu32ctxswitchhdlr().
__attribute__((__noinline__)) // used to force registers flushing.
void pu32hang (const char *fmt, ...) {
	va_list args;
	va_start (args, fmt);
	pu32printf (fmt, args);
	void dump_stack(void);
	dump_stack();
	asm volatile ("rli %%sr; j %%sr\n" ::: "memory");
}
