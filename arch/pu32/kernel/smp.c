// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

//#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/sched.h>
//#include <linux/kernel_stat.h>
#include <linux/notifier.h>
#include <linux/cpu.h>
#include <linux/interrupt.h>
#include <linux/percpu.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/irq.h>
#include <linux/sched/task_stack.h>
#include <linux/sched/mm.h>
#include <linux/sched/hotplug.h>

#include <asm/irq.h>
//#include <asm/traps.h>
#include <asm/sections.h>
//#include <asm/mmu_context.h>
//#include <asm/pgalloc.h>

#include <hwdrvintctrl.h>

void __init smp_prepare_boot_cpu (void) {}
void __init smp_prepare_cpus (unsigned int max_cpus) {}

volatile unsigned int cpu_up_arg;

extern unsigned long pu32_ishw;

void __init setup_smp (void) {

	cpu_up_arg = 1;

	extern char _inc_cpu_up_arg[];
	unsigned long _inc_cpu_up_arg_raddr = ((unsigned long)_inc_cpu_up_arg - (PARKPU_RLI16IMM_ADDR + 2));
	while (1) {
		unsigned long old_cpu_up_arg = cpu_up_arg;
		BUG_ON(_inc_cpu_up_arg_raddr>>16); // Insure address fit in 16bits.
		asm volatile ("ldst16 %0, %1" /* does memory fencing as well */
			: "+r" ((unsigned long){_inc_cpu_up_arg_raddr})
			: "r"  (PARKPU_RLI16IMM_ADDR));
		unsigned long irqdst;
		if (pu32_ishw)
			irqdst = hwdrvintctrl_int(cpu_up_arg);
		else {
			// Discard any data currently buffered in PU32_BIOS_FD_INTCTRLDEV.
			while (pu32sysread (PU32_BIOS_FD_INTCTRLDEV, &irqdst, sizeof(unsigned long)));
			// Send IPI to cpu_up_arg.
			pu32syswrite (PU32_BIOS_FD_INTCTRLDEV, (void *)&cpu_up_arg, sizeof(unsigned long));
			// Read IPI request response.
			pu32sysread (PU32_BIOS_FD_INTCTRLDEV, (void *)&irqdst, sizeof(unsigned long));
		}
		if (irqdst == -1)
			break;
		unsigned long timeout;
		for (	timeout = msecs_to_jiffies(10000);
			timeout && cpu_up_arg == old_cpu_up_arg;
			--timeout);
		if (cpu_up_arg != old_cpu_up_arg)
			continue;
		pr_crit("CPU%u failed to start\n", cpu_up_arg);
		break;
	}

	if (cpu_up_arg > nr_cpu_ids) {
		pr_warn("Total number of cpus [%d] > [%d]\n",
			cpu_up_arg, nr_cpu_ids);
		cpu_up_arg = nr_cpu_ids;
	}

	unsigned int cpu;
	for (cpu = 1; cpu < cpu_up_arg; ++cpu) {
		set_cpu_possible(cpu, true);
		set_cpu_present(cpu, true);
	}
}

static DECLARE_COMPLETION(cpu_up_flag);

int __cpu_up (unsigned int cpu, struct task_struct *tidle) {

	struct thread_info *ti = task_thread_info(tidle);
	ti->last_cpu = ti->cpu = cpu;

	cpu_up_arg = // Value to set in the cpu %sp.
		// sizeof(struct pu32_pt_regs) accounts for the space
		// used only if the thread ever become a user-thread.
		((unsigned long)end_of_stack(tidle) - sizeof(struct pu32_pt_regs));

	extern char _start_smp[];
	unsigned long _start_smp_raddr = ((unsigned long)_start_smp - (PARKPU_RLI16IMM_ADDR + 2));
	BUG_ON(_start_smp_raddr>>16); // Insure address fit in 16bits.
	asm volatile ("ldst16 %0, %1" /* does memory fencing as well */
		: "+r" ((unsigned long){_start_smp_raddr})
		: "r"  (PARKPU_RLI16IMM_ADDR));

	unsigned long irqdst;
	if (pu32_ishw)
		irqdst = hwdrvintctrl_int(cpu);
	else {
		// Discard any data currently buffered in PU32_BIOS_FD_INTCTRLDEV.
		while (pu32sysread (PU32_BIOS_FD_INTCTRLDEV, &irqdst, sizeof(unsigned long)));
		// Send IPI to cpu.
		pu32syswrite (PU32_BIOS_FD_INTCTRLDEV, (void *)&cpu, sizeof(unsigned long));
		// Read IPI request response.
		pu32sysread (PU32_BIOS_FD_INTCTRLDEV, (void *)&irqdst, sizeof(unsigned long));
	}
	if (irqdst == -1) {
		pr_crit("CPU%u failed to start\n", cpu);
		return -EOPNOTSUPP;
	}

	wait_for_completion_timeout(&cpu_up_flag, msecs_to_jiffies(10000));

	if (!cpu_online(cpu)) {
		pr_crit("CPU%u startup timeout\n", cpu);
		return -EIO;
	}

	return 0;
}

void __init smp_cpus_done (unsigned int max_cpus) {}

int setup_profiling_timer (unsigned int multiplier) {
	return -EINVAL;
}

extern unsigned long pu32_TASK_UNMAPPED_BASE;

void pu32ctxswitchhdlr (void);

void c_setup (void);

void pu32_start_smp (void) {

	asm volatile ("setksl %0\n" :: "r"(pu32_TASK_UNMAPPED_BASE));

	struct thread_info *ti = pu32_get_thread_info(cpu_up_arg);
	asm volatile ("cpy %%tp, %0" :: "r"(ti));

	struct mm_struct *mm = &init_mm;
	mmget(mm);
	mmgrab(mm);
	current->active_mm = mm;
	cpumask_set_cpu(raw_smp_processor_id(), mm_cpumask(mm));

	enable_percpu_irq(PU32_IPI_IRQ, 0);

	notify_cpu_starting(raw_smp_processor_id());
	c_setup();
	set_cpu_online(raw_smp_processor_id(), true);
	pr_info("CPU%u online\n", (unsigned int)raw_smp_processor_id());
	complete(&cpu_up_flag);

	// Disable preemption before enabling interrupts, so we don't
	// try to schedule a CPU that hasn't actually started yet.
	preempt_disable();
	void pu32_clockevent_init (void); pu32_clockevent_init();
	pu32ctxswitchhdlr();
	raw_local_irq_enable();
	cpu_startup_entry(CPUHP_AP_ONLINE_IDLE);
}

struct ipi_data_struct {
	unsigned long bits;
};
static struct ipi_data_struct ipi_data[NR_CPUS];

enum ipi_msg_type {
	IPI_EMPTY,
	IPI_RESCHEDULE,
	IPI_CALL_FUNC,
	IPI_CPU_STOP,
	IPI_MAX
};

extern unsigned long pu32_kernelmode_stack[NR_CPUS];

static irqreturn_t handle_ipi (int irq, void *dev) {
	while (true) {
		unsigned long ops = xchg(&ipi_data[raw_smp_processor_id()].bits, 0);
		if (ops == 0)
			return IRQ_HANDLED;
		if (ops & (1 << IPI_RESCHEDULE))
			scheduler_ipi();
		if (ops & (1 << IPI_CALL_FUNC))
			generic_smp_call_function_interrupt();
		if (ops & (1 << IPI_CPU_STOP)) {
			set_cpu_online(raw_smp_processor_id(), false);
			kfree((void *)pu32_kernelmode_stack[raw_smp_processor_id()]);
			asm volatile ("j %0" :: "r"(PARKPU_ADDR));
			// It should never reach here.
			BUG();
		}
		BUG_ON((ops >> IPI_MAX) != 0);
	}
	return IRQ_HANDLED;
}

static void ipi_msg (const struct cpumask *mask, enum ipi_msg_type msg_id) {
	unsigned int cpu;
	for_each_cpu(cpu, mask)
		__set_bit(msg_id, &ipi_data[raw_smp_processor_id()].bits);
	smp_mb();
	for_each_cpu(cpu, mask) {
		unsigned long irqdst;
		if (pu32_ishw)
			irqdst = hwdrvintctrl_int(cpu);
		else {
			// Discard any data currently buffered in PU32_BIOS_FD_INTCTRLDEV.
			while (pu32sysread (PU32_BIOS_FD_INTCTRLDEV, &irqdst, sizeof(unsigned long)));
			// Send IPI to cpu.
			pu32syswrite (PU32_BIOS_FD_INTCTRLDEV, (void *)&cpu, sizeof(unsigned long));
			// Read IPI request response.
			pu32sysread (PU32_BIOS_FD_INTCTRLDEV, (void *)&irqdst, sizeof(unsigned long));
		}
		if (irqdst == -1)
			pr_crit("CPU%u ipi_msg failed\n", cpu);
	}
}

void arch_send_call_function_ipi_mask (struct cpumask *mask) {
	ipi_msg(mask, IPI_CALL_FUNC);
}

void arch_send_call_function_single_ipi (int cpu) {
	ipi_msg(cpumask_of(cpu), IPI_CALL_FUNC);
}

void smp_send_reschedule (int cpu) {
	ipi_msg(cpumask_of(cpu), IPI_RESCHEDULE);
}

void smp_send_stop (void) {
	struct cpumask mask;
	cpumask_copy(&mask, cpu_online_mask);
	unsigned int cpu = raw_smp_processor_id();
	cpumask_clear_cpu(cpu, &mask);
	ipi_msg(&mask, IPI_CPU_STOP);
}

void ipi_init (void) {
	int rc = request_irq (
		PU32_IPI_IRQ, handle_ipi,
		IRQF_PERCPU|IRQF_NOBALANCING, "IPI", &((unsigned long){0}));
	if (rc)
		panic("IPI IRQ request failed\n");
	enable_percpu_irq(PU32_IPI_IRQ, 0);
}

#ifdef CONFIG_HOTPLUG_CPU

int __cpu_disable (void) {
	unsigned int cpu = raw_smp_processor_id();
	set_cpu_online(cpu, false);
	#ifdef CONFIG_GENERIC_IRQ_MIGRATION
	irq_migrate_all_off_this_cpu();
	#endif
	clear_tasks_mm_cpumask(cpu);
	return 0;
}

void __cpu_die (unsigned int cpu) {
	if (!cpu_wait_death(cpu, 5)) {
		pr_crit("CPU%u shutdown failed\n", cpu);
		return;
	}
	pr_notice("CPU%u offline\n", cpu);
}

void arch_cpu_idle_dead (void) {
	idle_task_exit();
	cpu_report_death();
	unsigned int cpu = raw_smp_processor_id();
	ipi_msg(cpumask_of(cpu), IPI_CPU_STOP);
	// It should never reach here.
	BUG();
}

#endif /* CONFIG_HOTPLUG_CPU */
