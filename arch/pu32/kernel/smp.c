// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/notifier.h>
#include <linux/cpu.h>
#include <linux/interrupt.h>
#include <linux/percpu.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/irq.h>
#include <linux/irq_work.h>
#include <linux/seq_file.h>
#include <linux/sched/task_stack.h>
#include <linux/sched/mm.h>
#include <linux/sched/hotplug.h>

#include <asm/irq.h>
#include <asm/sections.h>

#include <hwdrvintctrl.h>

void __init smp_prepare_boot_cpu (void) {}
void __init smp_prepare_cpus (unsigned int max_cpus) {}

volatile unsigned int cpu_up_arg;

extern unsigned long pu32_ishw;

static unsigned long pu32_send_ipi (unsigned int cpu) {
		unsigned long irqdst;
		if (pu32_ishw) {
			irqdst = hwdrvintctrl_int(cpu);
		} else {
			// Discard any data currently buffered in PU32_BIOS_FD_INTCTRLDEV.
			while (pu32sysread (PU32_BIOS_FD_INTCTRLDEV, &irqdst, sizeof(unsigned long)));
			// Send IPI to cpu.
			pu32syswrite (PU32_BIOS_FD_INTCTRLDEV, (void *)&cpu, sizeof(unsigned long));
			// Read IPI request response.
			pu32sysread (PU32_BIOS_FD_INTCTRLDEV, (void *)&irqdst, sizeof(unsigned long));
		}
		return irqdst;
}

// Hold stack address to be used by _inc_cpu_up_arg().
void *_inc_cpu_up_arg_stack_ptr = 0;

void __init setup_smp (void) {

	char _inc_cpu_up_arg_stack[64];
	_inc_cpu_up_arg_stack_ptr = &_inc_cpu_up_arg_stack + 1;

	cpu_up_arg = 1;

	extern char _inc_cpu_up_arg[];
	unsigned long _inc_cpu_up_arg_raddr = ((unsigned long)_inc_cpu_up_arg - (PARKPU_RLI16IMM_ADDR + 2));
	while (1) {
		unsigned int old_cpu_up_arg = cpu_up_arg;
		BUG_ON(_inc_cpu_up_arg_raddr>>16); // Insure address fit in 16bits.
		(*(volatile uint16_t *)PARKPU_RLI16IMM_ADDR) = _inc_cpu_up_arg_raddr;
		unsigned long irqdst = pu32_send_ipi(old_cpu_up_arg);
		if (irqdst == -2) {
			pr_crit("CPU%u failed to start\n", old_cpu_up_arg);
			break;
		}
		if (irqdst == -1)
			break;
		unsigned long timeout = (jiffies + msecs_to_jiffies(10000));
		do {
			if (cpu_up_arg != old_cpu_up_arg)
				break;
		} while (time_before(jiffies, timeout));
		if (cpu_up_arg != old_cpu_up_arg) {
			if (cpu_up_arg > nr_cpu_ids) {
				pr_warn("Total number of cpus greater than %d\n", nr_cpu_ids);
				cpu_up_arg = nr_cpu_ids;
				break;
			}
			continue;
		}
		pr_crit("CPU%u failed to start\n", old_cpu_up_arg);
		break;
	}

	unsigned int cpu;
	for (cpu = 1; cpu < cpu_up_arg; ++cpu) {
		set_cpu_possible(cpu, true);
		set_cpu_present(cpu, true);
	}
}

int __cpu_up (unsigned int cpu, struct task_struct *tidle) {

	struct thread_info *ti = task_thread_info(tidle);
	ti->last_cpu = ti->cpu = cpu;

	cpu_up_arg = ti->ksp; // Value to set in the cpu %sp.

	extern char _start_smp[];
	unsigned long _start_smp_raddr = ((unsigned long)_start_smp - (PARKPU_RLI16IMM_ADDR + 2));
	BUG_ON(_start_smp_raddr>>16); // Insure address fit in 16bits.
	(*(volatile uint16_t *)PARKPU_RLI16IMM_ADDR) = _start_smp_raddr;

	int ret = 0;
	unsigned long irqdst;
	unsigned long timeout = (jiffies + msecs_to_jiffies(10000));
	do {
		irqdst = pu32_send_ipi(cpu);
		if (irqdst != -2)
			break;
	} while (time_before(jiffies, timeout));
	if (irqdst == -1) {
		pr_crit("CPU%u failed to start\n", cpu);
		ret = -EOPNOTSUPP;
		goto done;
	}
	timeout = (jiffies + msecs_to_jiffies(10000));
	do {
		if (cpu_online(cpu))
			goto done;
	} while (time_before(jiffies, timeout));

	pr_crit("CPU%u startup timeout\n", cpu);
	ret = -EIO;

	done:;
	return ret;
}

void __init smp_cpus_done (unsigned int max_cpus) {}

extern unsigned long pu32_TASK_UNMAPPED_BASE;

void pu32ctxswitchhdlr (void);

void c_setup (void);

void pu32_start_smp (void) {

	if (pu32_ishw)
		asm volatile ("setksysopfaulthdlr %0\n" :: "r"(pu32_ishw) : "memory");

	asm volatile ("setksl %0\n" :: "r"(pu32_TASK_UNMAPPED_BASE) : "memory");

	struct thread_info *ti = pu32_get_thread_info(cpu_up_arg);
	asm volatile ("cpy %%tp, %0\n" :: "r"(ti) : "memory");

	struct mm_struct *mm = &init_mm;
	mmget(mm);
	mmgrab(mm);
	current->active_mm = mm;
	cpumask_set_cpu(raw_smp_processor_id(), mm_cpumask(mm));

	c_setup();

	void pu32_clockevent_init (void); pu32_clockevent_init();

	enable_percpu_irq(PU32_IPI_IRQ, 0);

	notify_cpu_starting(raw_smp_processor_id());
	set_cpu_online(raw_smp_processor_id(), true);
	pr_info("CPU%u online\n", (unsigned int)raw_smp_processor_id());

	pu32ctxswitchhdlr();

	raw_local_irq_enable();
	cpu_startup_entry(CPUHP_AP_ONLINE_IDLE);
}

enum ipi_msg_type {
	IPI_EMPTY,
	IPI_RESCHEDULE,
	IPI_CALL_FUNC,
	#ifdef CONFIG_IRQ_WORK
	IPI_IRQ_WORK,
	#endif
	IPI_MAX
};

struct ipi_data_struct {
	unsigned long bits;
	unsigned long stats[IPI_MAX];
};
static struct ipi_data_struct ipi_data[NR_CPUS];

static irqreturn_t handle_ipi (int irq, void *dev) {
	unsigned long *stats = ipi_data[raw_smp_processor_id()].stats;
	unsigned long ops = xchg(&ipi_data[raw_smp_processor_id()].bits, 0);
	if (!ops) {
		++stats[IPI_EMPTY];
		return IRQ_HANDLED;
	}
	do {
		if (ops & (1 << IPI_EMPTY)) {
			BUG();
		}
		if (ops & (1 << IPI_RESCHEDULE)) {
			++stats[IPI_RESCHEDULE];
			scheduler_ipi();
		}
		if (ops & (1 << IPI_CALL_FUNC)) {
			++stats[IPI_CALL_FUNC];
			generic_smp_call_function_interrupt();
		}
		#ifdef CONFIG_IRQ_WORK
		if (ops & (1 << IPI_IRQ_WORK)) {
			++stats[IPI_IRQ_WORK];
			irq_work_run();
		}
		#endif
		BUG_ON((ops >> IPI_MAX) != 0);
		ops = xchg(&ipi_data[raw_smp_processor_id()].bits, 0);
	} while (ops);
	return IRQ_HANDLED;
}

static void ipi_msg (const struct cpumask *mask, enum ipi_msg_type msg_id) {
	unsigned int cpu;
	for_each_cpu(cpu, mask)
		__set_bit(msg_id, &ipi_data[cpu].bits);
	smp_mb();
	for_each_cpu(cpu, mask) {
		unsigned long irqdst;
		do {
			irqdst = pu32_send_ipi(cpu);
		} while (irqdst == -2);
		if (irqdst == -1)
			pr_crit("CPU%u ipi_msg failed\n", cpu);
	}
}

static const char * const ipi_names[] = {
	[IPI_EMPTY]		= "Empty",
	[IPI_RESCHEDULE]	= "Reschedule",
	[IPI_CALL_FUNC]		= "Function call",
	#ifdef CONFIG_IRQ_WORK
	[IPI_IRQ_WORK]		= "IRQ work",
	#endif
};

int arch_show_interrupts(struct seq_file *p, int prec) {
	unsigned int cpu, i;
	for (i = 0; i < IPI_MAX; i++) {
		if (!ipi_names[i])
			continue;
		seq_printf (p, "%*s:", prec, "IPI");
		for_each_online_cpu(cpu)
			seq_printf(p, " %10lu", ipi_data[cpu].stats[i]);
		seq_printf(p, "     %s\n", ipi_names[i]);
	}
	return 0;
}

#ifdef CONFIG_IRQ_WORK
void arch_irq_work_raise (void) {
	ipi_msg(cpumask_of(raw_smp_processor_id()), IPI_IRQ_WORK);
}
#endif

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
	unsigned int cpu;
	for_each_online_cpu(cpu) {
		if (cpu == raw_smp_processor_id())
			continue;
		set_cpu_online(cpu, false);
		unsigned long irqdst;
		do {
			irqdst = pu32_send_ipi(cpu);
		} while (irqdst == -2);
		if (irqdst == -1)
			pr_crit("smp_send_stop(): CPU%u failed\n", cpu);
	}
}

static int ipi_dummy_dev;
void ipi_init (void) {
	int rc = request_irq (
		PU32_IPI_IRQ, handle_ipi,
		IRQF_PERCPU|IRQF_NOBALANCING, "IPI", &ipi_dummy_dev);
	if (rc)
		panic("IPI IRQ request failed\n");
	enable_percpu_irq(PU32_IPI_IRQ, 0);
}

#ifdef CONFIG_HOTPLUG_CPU

int __cpu_disable (void) {
	if (pu32_ishw) // Disable core from receiving device interrupts.
		hwdrvintctrl_ack(raw_smp_processor_id(), 0);
	set_cpu_online(raw_smp_processor_id(), false);
	#ifdef CONFIG_GENERIC_IRQ_MIGRATION
	irq_migrate_all_off_this_cpu();
	#endif
	clear_tasks_mm_cpumask(raw_smp_processor_id());
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
	asm volatile (
		"setflags %0\n" ::
		"r"(PU32_FLAGS_KERNELSPACE | (PU32_FLAGS_disIntr & ~PU32_FLAGS_disExtIntr)) :
		"memory");
	unsigned int cpu = raw_smp_processor_id();
	unsigned long irqdst;
	do {
		irqdst = pu32_send_ipi(cpu);
	} while (irqdst == -2);
	if (irqdst == -1)
		pr_crit("arch_cpu_idle_dead(): CPU%u failed\n", cpu);
	while(1);
}

#endif /* CONFIG_HOTPLUG_CPU */
