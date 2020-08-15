// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef __PU32_ARCH_H
#define __PU32_ARCH_H

// Reserved region for BIOS at the start of memory.
// Must match corresponding constant in the bootloader source-code.
#define PU32_BIOS_RESERVED_MEM (7*PAGE_SIZE)

#define KERNELADDR (PAGE_SIZE + PU32_BIOS_RESERVED_MEM)

#define PARKPUSZ 24

#define PARKPU_ADDR          (KERNELADDR - PARKPUSZ)
#define PARKPU_RLI16IMM_ADDR (PARKPU_ADDR + 14)

#ifndef __ASSEMBLY__

#include <linux/kernel.h>
#include <linux/types.h>

#include <asm/timex.h>
#include <asm/syscall.h>

// RoundDown to power of two.
#define ROUNDDOWNTOPOWEROFTWO(VALUE,POWEROFTWO) \
	((VALUE) & -(POWEROFTWO))
// RoundUp to power of two.
#define ROUNDUPTOPOWEROFTWO(VALUE,POWEROFTWO) \
	ROUNDDOWNTOPOWEROFTWO(((VALUE) + ((POWEROFTWO)-1)), POWEROFTWO)

#define PU32_BIOS_FD_STDIN             4
#define PU32_BIOS_FD_STDOUT            1
#define PU32_BIOS_FD_STDERR            2
#define PU32_BIOS_FD_STORAGEDEV        5
#define PU32_BIOS_FD_NETWORKDEV        6
#define PU32_BIOS_FD_INTCTRLDEV        7

// IRQ ids when running in a VM.
#define PU32_VM_IRQ_TTYS0 0

#define PU32_FLAGS_setasid		0x1
#define PU32_FLAGS_settimer		0x2
#define PU32_FLAGS_settlb		0x4
#define PU32_FLAGS_clrtlb		0x8
#define PU32_FLAGS_getclkcyclecnt	0x10
#define PU32_FLAGS_getclkfreq		0x20
#define PU32_FLAGS_gettlbsize		0x40
#define PU32_FLAGS_getcachesize		0x80
#define PU32_FLAGS_getcoreid		0x100
#define PU32_FLAGS_cacherst		0x200
#define PU32_FLAGS_gettlb		0x400
#define PU32_FLAGS_setflags		0x800
#define PU32_FLAGS_disExtIntr		0x1000
#define PU32_FLAGS_disTimerIntr		0x2000
#define PU32_FLAGS_disPreemptIntr	0x4000
#define PU32_FLAGS_halt			0x8000

#define PU32_FLAGS_KERNELSPACE		(\
	PU32_FLAGS_setasid		|\
	PU32_FLAGS_settimer		|\
	PU32_FLAGS_settlb		|\
	PU32_FLAGS_clrtlb		|\
	PU32_FLAGS_getclkcyclecnt	|\
	PU32_FLAGS_getclkfreq		|\
	PU32_FLAGS_gettlbsize		|\
	PU32_FLAGS_getcachesize		|\
	PU32_FLAGS_getcoreid		|\
	PU32_FLAGS_cacherst		|\
	PU32_FLAGS_gettlb		|\
	PU32_FLAGS_setflags		|\
	PU32_FLAGS_halt			)

#define PU32_FLAGS_USERSPACE 0;

#define PU32_FLAGS_disIntr		(\
	PU32_FLAGS_disExtIntr		|\
	PU32_FLAGS_disTimerIntr		|\
	PU32_FLAGS_disPreemptIntr	)

#define PU32_CAP_mmu	0x1
#define PU32_CAP_hptw	0x2

typedef enum {
	pu32ReadFaultIntr,
	pu32WriteFaultIntr,
	pu32ExecFaultIntr,
	pu32AlignFaultIntr,
	pu32ExtIntr,
	pu32SysOpIntr,
	pu32TimerIntr,
	pu32PreemptIntr,
} pu32FaultReason;

static inline char *pu32faultreasonstr (
	pu32FaultReason faultreason, unsigned ispceqfaddr) {
	return
		(faultreason == pu32ReadFaultIntr) ? "ReadFault" :
		(faultreason == pu32WriteFaultIntr) ? "WriteFault" :
		(faultreason == pu32ExecFaultIntr) ? "ExecFault" :
		(faultreason == pu32AlignFaultIntr) ?
			(ispceqfaddr ? "I-AlignFault" : "D-AlignFault") :
		(faultreason == pu32ExtIntr) ? "ExtIntr" :
		(faultreason == pu32SysOpIntr) ? "SysOpIntr" :
		(faultreason == pu32TimerIntr) ? "TimerIntr" :
		(faultreason == pu32PreemptIntr) ? "PreemptIntr" :
		"UnknownIntr";
}

#define PU32STRBUFSZ 1024
// Buffer used in various locations with snprintf().
extern char pu32strbuf[];

#define PU32_OPNOTAVAIL (8<<3)

static inline char *pu32sysopcodestr (unsigned long sysopcode) {
	unsigned long o = 0;
	void decode1gpr (void) {
		o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "%%%ld", (((sysopcode>>8)&0xf0)>>4));
	}
	void decode2gpr (void) {
		o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "%%%ld", (((sysopcode>>8)&0xf0)>>4));
		o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, " ");
		o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "%%%ld", ((sysopcode>>8)&0xf));
	}
	o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "%02lx ", (sysopcode&0xff));
	o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "%02lx ", ((sysopcode>>8)&0xff));
	switch (sysopcode&0xff) {
		case PU32_OPNOTAVAIL:
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "OPNOTAVAIL");
			break;
		case 0xb8:
			// Specification from the instruction set manual:
			// add %gpr1, %gpr2 |23|000|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "add ");
			decode2gpr();
			break;
		case 0xb9:
			// Specification from the instruction set manual:
			// sub %gpr1, %gpr2 |23|001|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "sub ");
			decode2gpr();
			break;
		case 0xca:
			// Specification from the instruction set manual:
			// mul %gpr1, %gpr2 |25|010|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "mul ");
			decode2gpr();
			break;
		case 0xcb:
			// Specification from the instruction set manual:
			// mulh %gpr1, %gpr2 |25|011|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "mulh ");
			decode2gpr();
			break;
		case 0xce:
			// Specification from the instruction set manual:
			// div %gpr1, %gpr2 |25|110|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "div ");
			decode2gpr();
			break;
		case 0xcf:
			// Specification from the instruction set manual:
			// mod %gpr1, %gpr2 |25|111|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "mod ");
			decode2gpr();
			break;
		case 0xc8:
			// Specification from the instruction set manual:
			// mulu %gpr1, %gpr2 |25|000|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "mulu ");
			decode2gpr();
			break;
		case 0xc9:
			// Specification from the instruction set manual:
			// mulhu %gpr1, %gpr2 |25|001|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "mulhu ");
			decode2gpr();
			break;
		case 0xcc:
			// Specification from the instruction set manual:
			// divu %gpr1, %gpr2 |25|100|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "divu ");
			decode2gpr();
			break;
		case 0xcd:
			// Specification from the instruction set manual:
			// modu %gpr1, %gpr2 |25|101|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "modu ");
			decode2gpr();
			break;
		case 0xd8:
			// Specification from the instruction set manual:
			// fadd %gpr1, %gpr2 |22|100|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "fadd ");
			decode2gpr();
			break;
		case 0xd9:
			// Specification from the instruction set manual:
			// fsub %gpr1, %gpr2 |22|101|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "fsub ");
			decode2gpr();
			break;
		case 0xda:
			// Specification from the instruction set manual:
			// fmul %gpr1, %gpr2 |22|110|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "fmul ");
			decode2gpr();
			break;
		case 0xdb:
			// Specification from the instruction set manual:
			// fdiv %gpr1, %gpr2 |22|111|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "fdiv ");
			decode2gpr();
			break;
		case 0xc3:
			// Specification from the instruction set manual:
			// and %gpr1, %gpr2 |24|011|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "and ");
			decode2gpr();
			break;
		case 0xc4:
			// Specification from the instruction set manual:
			// or %gpr1, %gpr2 |24|100|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "or ");
			decode2gpr();
			break;
		case 0xc5:
			// Specification from the instruction set manual:
			// xor %gpr1, %gpr2 |24|101|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "xor ");
			decode2gpr();
			break;
		case 0xc6:
			// Specification from the instruction set manual:
			// not %gpr1, %gpr2 |24|110|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "not ");
			decode2gpr();
			break;
		case 0xc7:
			// Specification from the instruction set manual:
			// cpy %gpr1, %gpr2 |24|111|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "cpy ");
			decode2gpr();
			break;
		case 0xc0:
			// Specification from the instruction set manual:
			// sll %gpr1, %gpr2 |24|000|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "sll ");
			decode2gpr();
			break;
		case 0xc1:
			// Specification from the instruction set manual:
			// srl %gpr1, %gpr2 |24|001|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "srl ");
			decode2gpr();
			break;
		case 0xc2:
			// Specification from the instruction set manual:
			// sra %gpr1, %gpr2 |24|010|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "sra ");
			decode2gpr();
			break;
		case 0xba:
			// Specification from the instruction set manual:
			// seq %gpr1, %gpr2 |23|010|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "seq ");
			decode2gpr();
			break;
		case 0xbb:
			// Specification from the instruction set manual:
			// sne %gpr1, %gpr2 |23|011|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "sne ");
			decode2gpr();
			break;
		case 0xbc:
			// Specification from the instruction set manual:
			// slt %gpr1, %gpr2 |23|100|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "slt ");
			decode2gpr();
			break;
		case 0xbd:
			// Specification from the instruction set manual:
			// slte %gpr1, %gpr2 |23|101|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "slte ");
			decode2gpr();
			break;
		case 0xbe:
			// Specification from the instruction set manual:
			// sltu %gpr1, %gpr2 |23|110|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "sltu ");
			decode2gpr();
			break;
		case 0xbf:
			// Specification from the instruction set manual:
			// slteu %gpr1, %gpr2 |23|111|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "slteu ");
			decode2gpr();
			break;
		case 0xb0:
			// Specification from the instruction set manual:
			// sgt %gpr1, %gpr2 |22|000|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "sgt ");
			decode2gpr();
			break;
		case 0xb1:
			// Specification from the instruction set manual:
			// sgte %gpr1, %gpr2 |22|001|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "sgte ");
			decode2gpr();
			break;
		case 0xb2:
			// Specification from the instruction set manual:
			// sgtu %gpr1, %gpr2 |22|010|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "sgtu ");
			decode2gpr();
			break;
		case 0xb3:
			// Specification from the instruction set manual:
			// sgteu %gpr1, %gpr2 |22|011|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "sgteu ");
			decode2gpr();
			break;
		case 0xd0:
			// Specification from the instruction set manual:
			// jz %gpr1 %gpr2 |26|000|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "jz ");
			decode2gpr();
			break;
		case 0xd1:
			// Specification from the instruction set manual:
			// jnz %gpr1 %gpr2 |26|001|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "jnz ");
			decode2gpr();
			break;
		case 0xd2:
			// Specification from the instruction set manual:
			// jl %gpr1 %gpr2 |26|010|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "jl ");
			decode2gpr();
			break;
		case 0xad:
			// Specification from the instruction set manual:
			// rli16 %gpr, imm |21|101|rrrr|0000|
			//                 |iiiiiiiiiiiiiiii|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "rli16 ");
			decode1gpr();
			break;
		case 0xae:
			// Specification from the instruction set manual:
			// rli32 %gpr, imm |21|110|rrrr|0000|
			//                 |iiiiiiiiiiiiiiii| 16 msb.
			//                 |iiiiiiiiiiiiiiii| 16 lsb.
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "rli32 ");
			decode1gpr();
			break;
		case 0xac:
			// Specification from the instruction set manual:
			// drli %gpr, imm |21|100|rrrr|0000|
			//                |iiiiiiiiiiiiiiii| 16 msb.
			//                |iiiiiiiiiiiiiiii| 16 lsb.
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "drli ");
			decode1gpr();
			break;
		case 0xa1:
			// Specification from the instruction set manual:
			// inc16 %gpr, imm	|20|001|rrrr|0000|
			// 			|iiiiiiiiiiiiiiii|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "inc16 ");
			decode1gpr();
			break;
		case 0xa2:
			// Specification from the instruction set manual:
			// inc32 %gpr, imm	|20|010|rrrr|0000|
			// 			|iiiiiiiiiiiiiiii| 16 msb.
			// 			|iiiiiiiiiiiiiiii| 16 lsb.
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "inc32 ");
			decode1gpr();
			break;
		case 0xa9:
			// Specification from the instruction set manual:
			// li16 %gpr, imm	|21|001|rrrr|0000|
			// 			|iiiiiiiiiiiiiiii| 16 msb.
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "li16 ");
			decode1gpr();
			break;
		case 0xaa:
			// Specification from the instruction set manual:
			// li32 %gpr, imm	|21|010|rrrr|0000|
			// 			|iiiiiiiiiiiiiiii| 16 msb.
			// 			|iiiiiiiiiiiiiiii| 16 lsb.
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "li32 ");
			decode1gpr();
			break;
		case 0xf4:
			// Specification from the instruction set manual:
			// ld8 %gpr1, %gpr2 |30|100|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "ld8 ");
			decode2gpr();
			break;
		case 0xf5:
			// Specification from the instruction set manual:
			// ld16 %gpr1, %gpr2 |30|101|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "ld16 ");
			decode2gpr();
			break;
		case 0xf6:
			// Specification from the instruction set manual:
			// ld32 %gpr1, %gpr2 |30|110|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "ld32 ");
			decode2gpr();
			break;
		case 0xf0:
			// Specification from the instruction set manual:
			// st8 %gpr1, %gpr2 |30|000|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "st8 ");
			decode2gpr();
			break;
		case 0xf1:
			// Specification from the instruction set manual:
			// st16 %gpr1, %gpr2 |30|001|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "st16 ");
			decode2gpr();
			break;
		case 0xf2:
			// Specification from the instruction set manual:
			// st32 %gpr1, %gpr2 |30|010|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "st32 ");
			decode2gpr();
			break;
		case 0x74:
			// Specification from the instruction set manual:
			// vld8 %gpr1, %gpr2 |14|100|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "vld8 ");
			decode2gpr();
			break;
		case 0x75:
			// Specification from the instruction set manual:
			// vld16 %gpr1, %gpr2 |14|101|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "vld16 ");
			decode2gpr();
			break;
		case 0x76:
			// Specification from the instruction set manual:
			// vld32 %gpr1, %gpr2 |14|110|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "vld32 ");
			decode2gpr();
			break;
		case 0x70:
			// Specification from the instruction set manual:
			// vst8 %gpr1, %gpr2 |14|000|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "vst8 ");
			decode2gpr();
			break;
		case 0x71:
			// Specification from the instruction set manual:
			// vst16 %gpr1, %gpr2 |14|001|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "vst16 ");
			decode2gpr();
			break;
		case 0x72:
			// Specification from the instruction set manual:
			// vst32 %gpr1, %gpr2 |14|010|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "vst32 ");
			decode2gpr();
			break;
		case 0xf8:
			// Specification from the instruction set manual:
			// ldst8 %gpr1, %gpr2 |31|000|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "ldst8 ");
			decode2gpr();
			break;
		case 0xf9:
			// Specification from the instruction set manual:
			// ldst16 %gpr1, %gpr2 |31|001|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "ldst16 ");
			decode2gpr();
			break;
		case 0xfa:
			// Specification from the instruction set manual:
			// ldst32 %gpr1, %gpr2 |31|010|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "ldst32 ");
			decode2gpr();
			break;
		case 0xfc:
			// Specification from the instruction set manual:
			// cldst8 %gpr1, %gpr2 |31|100|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "cldst8 ");
			decode2gpr();
			break;
		case 0xfd:
			// Specification from the instruction set manual:
			// cldst16 %gpr1, %gpr2 |31|101|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "cldst16 ");
			decode2gpr();
			break;
		case 0xfe:
			// Specification from the instruction set manual:
			// cldst32 %gpr1, %gpr2 |31|110|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "cldst32 ");
			decode2gpr();
			break;
		case 0x00:
			// Specification from the instruction set manual:
			// sysret |0|000|0000|0000|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "sysret ");
			break;
		case 0x01:
			// Specification from the instruction set manual:
			// syscall |0|001|0000|0000|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "syscall ");
			break;
		case 0x02:
			// Specification from the instruction set manual:
			// brk |0|010|0000|0000|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "brk ");
			break;
		case 0x03:
			// Specification from the instruction set manual:
			// halt |0|011|0000|0000|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "halt ");
			break;
		case 0x04:
			// Specification from the instruction set manual:
			// icacherst |0|100|0000|0000|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "icacherst ");
			break;
		case 0x05:
			// Specification from the instruction set manual:
			// dcacherst |0|101|0000|0000|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "dcacherst ");
			break;
		case 0x07:
			// Specification from the instruction set manual:
			// ksysret |0|111|0000|0000|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "ksysret ");
			break;
		case 0x39:
			// Specification from the instruction set manual:
			// setksl %gpr |7|001|rrrr|0000|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "setksl ");
			decode1gpr();
			break;
		case 0x3c:
			// Specification from the instruction set manual:
			// setasid %gpr |7|100|rrrr|0000|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "setasid ");
			decode1gpr();
			break;
		case 0x3d:
			// Specification from the instruction set manual:
			// setuip %gpr |7|101|rrrr|0000|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "setuip ");
			decode1gpr();
			break;
		case 0x3e:
			// Specification from the instruction set manual:
			// setflags %gpr |7|110|rrrr|0000|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "setflags ");
			decode1gpr();
			break;
		case 0x3f:
			// Specification from the instruction set manual:
			// settimer %gpr |7|111|rrrr|0000|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "settimer ");
			decode1gpr();
			break;
		case 0x3a:
			// Specification from the instruction set manual:
			// settlb %gpr1, %gpr2 |7|010|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "settlb ");
			decode2gpr();
			break;
		case 0x3b:
			// Specification from the instruction set manual:
			// clrtlb %gpr1, %gpr2 |7|011|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "clrtlb ");
			decode2gpr();
			break;
		case 0x79:
			// Specification from the instruction set manual:
			// setkgpr %gpr1 %gpr2 |15|001|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "setkgpr ");
			decode2gpr();
			break;
		case 0x7a:
			// Specification from the instruction set manual:
			// setugpr %gpr1 %gpr2 |15|010|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "setugpr ");
			decode2gpr();
			break;
		case 0x7b:
			// Specification from the instruction set manual:
			// setgpr %gpr1 %gpr2 |15|011|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "setgpr ");
			decode2gpr();
			break;
		case 0x28:
			// Specification from the instruction set manual:
			// getsysopcode %gpr |5|000|rrrr|0000|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "getsysopcode ");
			decode1gpr();
			break;
		case 0x29:
			// Specification from the instruction set manual:
			// getuip %gpr |5|001|rrrr|0000|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "getuip ");
			decode1gpr();
			break;
		case 0x2a:
			// Specification from the instruction set manual:
			// getfaultaddr %gpr |5|010|rrrr|0000|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "getfaultaddr ");
			decode1gpr();
			break;
		case 0x2b:
			// Specification from the instruction set manual:
			// getfaultreason %gpr |5|011|rrrr|0000|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "getfaultreason ");
			decode1gpr();
			break;
		case 0x2c:
			// Specification from the instruction set manual:
			// getclkcyclecnt %gpr |5|100|rrrr|0000|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "getclkcyclecnt ");
			decode1gpr();
			break;
		case 0x2d:
			// Specification from the instruction set manual:
			// getclkcyclecnth %gpr |5|101|rrrr|0000|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "getclkcyclecnth ");
			decode1gpr();
			break;
		case 0x2e:
			// Specification from the instruction set manual:
			// gettlbsize %gpr |5|110|rrrr|0000|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "gettlbsize ");
			decode1gpr();
			break;
		case 0x2f:
			// Specification from the instruction set manual:
			// geticachesize %gpr |5|111|rrrr|0000|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "geticachesize ");
			decode1gpr();
			break;
		case 0x10:
			// Specification from the instruction set manual:
			// getcoreid %gpr |2|000|rrrr|0000|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "getcoreid ");
			decode1gpr();
			break;
		case 0x11:
			// Specification from the instruction set manual:
			// getclkfreq %gpr |2|001|rrrr|0000|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "getclkfreq ");
			decode1gpr();
			break;
		case 0x12:
			// Specification from the instruction set manual:
			// getdcachesize %gpr |2|010|rrrr|0000|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "getdcachesize ");
			decode1gpr();
			break;
		case 0x13:
			// Specification from the instruction set manual:
			// gettlb %gpr1, %gpr2 |2|011|rrrr|rrrr|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "gettlb ");
			decode2gpr();
			break;
		case 0x14:
			// Specification from the instruction set manual:
			// getcap %gpr |2|100|rrrr|0000|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "getcap ");
			decode1gpr();
			break;
		case 0x15:
			// Specification from the instruction set manual:
			// getver %gpr |2|101|rrrr|0000|
			o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "getver ");
			decode1gpr();
			break;
		default: {
			if (sysopcode == 0b10010000)
				o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "preempt");
			else {
				do {
					unsigned long x = ((sysopcode>>4)&0xf);
					if (x == 0b1000) {
						// Specification from the instruction set manual:
						// li8 %gpr, imm |1000|iiii|rrrr|iiii|
						o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "li8 ");
					} else if (x == 0b1001) {
						// Specification from the instruction set manual:
						// inc8 %gpr, imm |1001|iiii|rrrr|iiii|
						o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "inc8 ");
					} else if (x == 0b1110) {
						// Specification from the instruction set manual:
						// rli8 %gpr, imm |1110|iiii|rrrr|iiii|
						o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "rli8 ");
					} else {
						o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "????");
						break;
					}
					decode1gpr();
					o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, " ");
					int8_t n = ((sysopcode>>8) + (((sysopcode&0xf)&0xf)<<4));
					if (n < 0) {
						n = -n;
						o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "-");
					}
					o += snprintf (&pu32strbuf[o], PU32STRBUFSZ-o, "%d", n);
				} while (0);
			}
			break;
		}
	}
	return pu32strbuf;
}

struct pu32tlbentry {
	union {
		struct {
			unsigned long executable:1;
			unsigned long writable:1;
			unsigned long readable:1;
			unsigned long cached:1;
			unsigned long user: 1;
			unsigned long xxx:7;
			unsigned long ppn:((sizeof(unsigned long)*8)-12);
		};
		unsigned long d1;
	};
	union {
		struct {
			unsigned long asid:12;
			unsigned long vpn:((sizeof(unsigned long)*8)-12);
		};
		unsigned long d2;
	};
};

static inline unsigned long pu32clkfreq (void) {
	cycles_t n;
	asm volatile ("getclkfreq %0" : "=r" (n));
	return n;
}

static inline ssize_t pu32sysread (int fd, void *buf, size_t count) {

	register uintptr_t a1 asm("%1") = fd;
	register uintptr_t a2 asm("%2") = (uintptr_t)buf;
	register uintptr_t a3 asm("%3") = count;

	asm volatile (
		"li %%sr, %1\n"
		"syscall\n"
		: "=r"(a1) // Output constraints.
		: "i"(__NR_read), "r"(a1), "r"(a2), "r"(a3) // Input constraints.
	);

	return ((a1 != -1) ? a1 : 0);
}

static inline ssize_t pu32syswrite (int fd, void *buf, size_t count) {

	register uintptr_t a1 asm("%1") = fd;
	register uintptr_t a2 asm("%2") = (uintptr_t)buf;
	register uintptr_t a3 asm("%3") = count;

	asm volatile (
		"li %%sr, %1\n"
		"syscall\n"
		: "=r"(a1) // Output constraints.
		: "i"(__NR_write), "r"(a1), "r"(a2), "r"(a3) // Input constraints.
	);

	return ((a1 != -1) ? a1 : 0);
}

void pu32stdout (const char *fmt, ...);

static inline off_t pu32syslseek (int fd, off_t offset, int whence) {

	register uintptr_t a1 asm("%1") = fd;
	register uintptr_t a2 asm("%2") = offset;
	register uintptr_t a3 asm("%3") = whence;

	asm volatile (
		"li %%sr, %1\n"
		"syscall\n"
		: "=r"(a1) // Output constraints.
		: "i"(__NR_lseek), "r"(a1), "r"(a2), "r"(a3) // Input constraints.
	);

	return ((a1 != -1) ? a1 : 0);
}

#endif /* !__ASSEMBLY__ */

#endif /* __PU32_ARCH_H */
