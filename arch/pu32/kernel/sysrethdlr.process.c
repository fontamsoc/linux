// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#include "regs.process.c"

#include "sysOpIntr.sysrethdlr.process.c"
#include "faultIntr.sysrethdlr.process.c"
#include "extIntr.sysrethdlr.process.c"
#include "timerIntr.sysrethdlr.process.c"
#include "preemptIntr.sysrethdlr.process.c"

__attribute__((__noinline__)) // used to force registers flushing.
static void pu32sysrethdlr (void) {

	pu32FaultReason faultreason;
	unsigned long sysopcode;
	asm volatile (
		"getfaultreason %0\n"
		"getsysopcode %1\n" :
		"=r"(faultreason),
		"=r"(sysopcode) ::
		"memory");

	switch (faultreason) {

		case pu32SysOpIntr:
			pu32sysrethdlr_sysOpIntr (sysopcode);
			break;

		case pu32ReadFaultIntr:
		case pu32WriteFaultIntr:
		case pu32ExecFaultIntr:
		case pu32AlignFaultIntr:
			pu32sysrethdlr_faultIntr (faultreason, sysopcode);
			break;

		case pu32ExtIntr:
			pu32sysrethdlr_extIntr (sysopcode);
			break;

		case pu32TimerIntr:
			pu32sysrethdlr_timerIntr (sysopcode);
			break;

		case pu32PreemptIntr:
			pu32sysrethdlr_preemptIntr (sysopcode);
			break;

		default:
			pu32hang ("Unexpected pu32FaultReason 0x%x\n", faultreason);
			break;
			// pu32hang() will infinite-loop;
			// so `return` will not be executed, but
			// it is left in place for completeness.
	}
}
