// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#include <linux/reboot.h>
#include <linux/smp.h>

#include <asm/syscall.h>

void machine_restart (char *cmd) {
	raw_local_irq_disable();
	#ifdef CONFIG_SMP
	smp_send_stop();
	#endif
	do_kernel_restart(cmd);
	asm volatile ("li8 %%1, 1; li %%sr, %0; syscall\n" :: "i"(__NR_exit) : "memory");
	pr_emerg("machine_restart() failed -- halting system\n");
	while(1);
}

void machine_power_off (void) {
	raw_local_irq_disable();
	#ifdef CONFIG_SMP
	smp_send_stop();
	#endif
	do_kernel_power_off();
	asm volatile ("li8 %%1, 0; li %%sr, %0; syscall\n" :: "i"(__NR_exit) : "memory");
	pr_emerg("machine_power_off() failed -- halting system\n");
	while(1);
}

void machine_halt (void) {
	machine_power_off();
}

void (*pm_power_off)(void) = NULL;
EXPORT_SYMBOL(pm_power_off);
