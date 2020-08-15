// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#include <linux/clocksource.h>
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

static u64 tsc_read (struct clocksource *cs) {
	return get_cycles();
}

static struct clocksource clocksource_tsc = {
	.name		= "tsc",
	.rating		= 400,
	.read		= tsc_read,
	.mask		= CLOCKSOURCE_MASK(64),
	.flags		= CLOCK_SOURCE_IS_CONTINUOUS,
};

static struct clock_event_device *clkevtdevs[NR_CPUS];

void pu32_timer_intr (void) {
	raw_local_irq_disable();
	irq_enter();
	struct clock_event_device *e =
		clkevtdevs[raw_smp_processor_id()];
	e->event_handler(e);
	irq_exit();
	raw_local_irq_enable();
}

static int pu32_timer_set_next_event (
	unsigned long delta,
	struct clock_event_device *dev) {
	asm volatile ("settimer %0" :: "r"(delta));
	return 0;
}

static int pu32_timer_set_state_shutdown (
	struct clock_event_device *evt) {
	asm volatile ("settimer %0" :: "r"(-1));
	return 0;
}

void pu32_clockevent_init (void) {

	struct clock_event_device *e =
		kzalloc(sizeof(*e), GFP_ATOMIC);

	unsigned cpu = raw_smp_processor_id();

	clkevtdevs[cpu] = e;

	const unsigned strsz = 10;
	char *s = kzalloc(strsz, GFP_ATOMIC);
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
	clocksource_register_hz(&clocksource_tsc, pu32clkfreq());
	// Optimize clocksource_tsc.mult and clocksource_tsc.shift .
	while (!(clocksource_tsc.mult & 1) && clocksource_tsc.shift) {
		clocksource_tsc.mult >>= 1;
		--clocksource_tsc.shift;
	}
	u64 read_sched_clock (void) {
		return get_cycles();
	}
	sched_clock_register (read_sched_clock, 64, pu32clkfreq());
	pu32_clockevent_init();
}
