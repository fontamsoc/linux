// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef HWDRVINTCTRL_H
#define HWDRVINTCTRL_H

// PIRWOP is the only memory operation handled with following
// expected from pi1_data_i |arg: (ARCHBITSZ-1) bits|cmd: 1 bit|
// where the field "cmd" values are CMDACKINT(0) or CMDINTDST(1):
// 	CMDACKINT: Acknowledges an interrupt source; field "arg" is expected
// 	to have following format |idx: (ARCHBITSZ-2) bits|en: 1 bit| where
// 	"idx" is the interrupt destination index and "en" enable/disable
// 	further interrupt delivery to the interrupt destination "idx".
// 	pi1_data_o get set to the interrupt source index, or -2 if there are
// 	no pending interrupt for the destination "idx", or -1 (for an interrupt
// 	triggered by CMDINTDST), while the field "arg" from pi1_data_i is ignored.
// 	CMDINTDST: Triggers an interrupt targeting a specific destination;
// 	the field "arg" from pi1_data_i is the index of the interrupt destination
// 	to target, while pi1_data_o get set to the interrupt destination index
// 	if valid, -2 if not ready due to an interrupt pending ack, or -1 if invalid.
// An interrupt must be acknowledged as soon as possible so
// that the intctrl can dispatch another interrupt request.

#define INTCTRLADDR ((void *)0x0ff0 /* By convention, the interrupt controller is located at 0x0ff0 */)

static inline unsigned long hwdrvintctrl_ack (unsigned long idx, unsigned long en) {
	unsigned long intsrc = ((idx<<2) | ((en&1)<<1) | 0);
	__asm__ __volatile__ (
		"ldst %0, %1"
		: "+r" (intsrc)
		: "r" (INTCTRLADDR));
	return intsrc;
}

static inline unsigned long hwdrvintctrl_int (unsigned long idx) {
	unsigned long intdst = ((idx<<1) | 1);
	__asm__ __volatile__ (
		"ldst %0, %1"
		: "+r" (intdst)
		: "r" (INTCTRLADDR));
	return intdst;
}

#endif /* HWDRVINTCTRL_H */
