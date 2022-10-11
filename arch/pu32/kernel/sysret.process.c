// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#include "sysrethdlr.process.c"

__attribute__((__noinline__)) // used to force registers flushing.
static void pu32sysret (unsigned long _do) {
	if (!_do)
		return;
	for (;;) {
		#ifdef HAVE_CONTEXT_TRACKING
		#ifndef CONFIG_CONTEXT_TRACKING
		#error CONFIG_CONTEXT_TRACKING not defined
		#endif
		#endif
		#ifdef CONFIG_CONTEXT_TRACKING
		void user_enter_callable(void);
		void user_exit_callable(void);
		unsigned long ti_in_userspace;
		if ((ti_in_userspace = pu32_in_userspace(current_thread_info())))
			user_enter_callable();
		#endif
		asm volatile ("sysret\n" ::: "memory");
		asm volatile (
			"cpy %%tp, %0\n" ::
			"r"(task_thread_info(pu32_cpu_curr[raw_smp_processor_id()])) :
			"memory");
		#ifdef CONFIG_CONTEXT_TRACKING
		if (ti_in_userspace)
			user_exit_callable();
		#endif
		pu32sysrethdlr();
	}
}
