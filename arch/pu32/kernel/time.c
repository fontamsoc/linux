// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#include <linux/clockchips.h>
#include <linux/sched_clock.h>
#include <linux/hardirq.h>
#include <linux/timex.h>
#include <linux/string.h>
#include <pu32.h>

extern unsigned long loops_per_jiffy;
// Calculates loops_per_jiffy from clock-frequency.
void calibrate_delay (void) {
	loops_per_jiffy = (pu32clkfreq() / HZ);
	pr_info("%lu.%02lu BogoMIPS (lpj=%lu)\n",
		loops_per_jiffy / (500000 / HZ),
		(loops_per_jiffy / (5000 / HZ)) % 100,
		loops_per_jiffy);
}

static char clkevtdevs_names[NR_CPUS][9]; // Enough space for "timerXXX".
static struct clock_event_device clkevtdevs[NR_CPUS];

void pu32_timer_intr (void) {
	raw_local_irq_disable();
	irq_enter();
	struct clock_event_device *e = &clkevtdevs[raw_smp_processor_id()];
	e->event_handler(e);
	irq_exit();
	raw_local_irq_enable();
}

static int pu32_timer_set_next_event (
	unsigned long delta,
	struct clock_event_device *dev) {
	asm volatile ("settimer %0\n" :: "r"(delta) : "memory");
	return 0;
}

static int pu32_timer_set_state_shutdown (
	struct clock_event_device *evt) {
	asm volatile ("settimer %0\n" :: "r"(-1) : "memory");
	return 0;
}

void pu32_clockevent_init (void) {

	unsigned cpu = raw_smp_processor_id();

	struct clock_event_device *e = &clkevtdevs[cpu];

	const unsigned strsz = sizeof(clkevtdevs_names[0]);
	char *s = clkevtdevs_names[cpu];
	snprintf(s, strsz, "timer%u", cpu);

	e->name = s;
	e->features = CLOCK_EVT_FEAT_ONESHOT;
	e->rating = 400;
	e->set_next_event = pu32_timer_set_next_event;
	e->set_state_shutdown = pu32_timer_set_state_shutdown;
	e->cpumask = cpumask_of(cpu);

	clockevents_config_and_register(e, pu32clkfreq(), 1, -2);
}

void __init time_init (void) {
	u64 read_sched_clock (void) {
		return get_cycles();
	}
	sched_clock_register (read_sched_clock, 64, pu32clkfreq());
	pu32_clockevent_init();
}
