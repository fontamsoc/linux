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
		void context_tracking_user_enter(void);
		void context_tracking_user_exit(void);
		unsigned long ti_in_userspace;
		if ((ti_in_userspace = pu32_in_userspace(current_thread_info())))
			context_tracking_user_enter();
		#endif
		asm volatile ("sysret\n" ::: "memory");
		asm volatile (
			"cpy %%tp, %0\n" ::
			"r"(task_thread_info(pu32_cpu_curr[raw_smp_processor_id()])) :
			"memory");
		#ifdef CONFIG_CONTEXT_TRACKING
		if (ti_in_userspace)
			context_tracking_user_exit();
		#endif
		pu32sysrethdlr();
	}
}
