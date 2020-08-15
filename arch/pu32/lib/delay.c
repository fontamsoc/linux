// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#include <linux/kernel.h>
#include <linux/delay.h>

#include <asm/timex.h>
#include <asm/param.h>

void __delay(unsigned long loops) {
	cycles_t i = get_cycles();
	while ((get_cycles() - i) < loops)
		asm volatile("preemptctx\n" ::: "memory");
}
EXPORT_SYMBOL(__delay);

void __const_udelay(unsigned long xloops) {
	unsigned long long loops;
	loops = (unsigned long long)xloops * loops_per_jiffy * HZ;
	__delay(loops >> 32);
}
EXPORT_SYMBOL(__const_udelay);

void __udelay(unsigned long usecs) {
	__const_udelay(usecs * 0x10C7UL); /* 2**32 / 1000000 (rounded up) */
}
EXPORT_SYMBOL(__udelay);

void __ndelay(unsigned long nsecs) {
	__const_udelay(nsecs * 0x5UL); /* 2**32 / 1000000000 (rounded up) */
}
EXPORT_SYMBOL(__ndelay);
