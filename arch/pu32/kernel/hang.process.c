// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

// Function that must be used instead of panic() within pu32ctxswitchhdlr().
void pu32hang (const char *fmt, ...) {
	va_list args;
	va_start (args, fmt);
	pu32printf (fmt, args);
	void dump_stack_print_info (const char *log_lvl);
	dump_stack_print_info (KERN_DEFAULT);
	unsigned long *sp; asm volatile ("setkgpr %0, %%sp" : "=r"(sp));
	extern void show_stack (struct task_struct *tsk, unsigned long *sp, const char *loglvl);
	show_stack (NULL, sp, KERN_DEFAULT);
	asm volatile ("rli %%sr; j %%sr":);
}
