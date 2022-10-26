// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#include <linux/string.h>

#include <pu32.h>

#define PU32PRINTFBUFSZ 1024
static char pu32printfbuf[PU32PRINTFBUFSZ];

void pu32printf (const char *fmt, ...) {

	va_list args;

	va_start(args, fmt);
	int vsnprintf (char *buf, size_t size, const char *fmt, va_list args);
	vsnprintf (pu32printfbuf, PU32PRINTFBUFSZ, fmt, args);
	va_end(args);

	char *s = pu32printfbuf; unsigned n = strlen(pu32printfbuf);
	unsigned i; for (i = 0; i < n;)
		i += pu32syswrite (PU32_BIOS_FD_STDOUT, s+i, n-i);
}

#define PU32SYSOPCODESTRBUFSZ 64
static char pu32sysopcodestrbuf[PU32SYSOPCODESTRBUFSZ];

char *pu32sysopcodestr (unsigned long sysopcode) {
	unsigned long o = 0;
	void decode1gpr (void) {
		o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "%%%ld", (((sysopcode>>8)&0xf0)>>4));
	}
	void decode2gpr (void) {
		o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "%%%ld", (((sysopcode>>8)&0xf0)>>4));
		o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, " ");
		o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "%%%ld", ((sysopcode>>8)&0xf));
	}
	o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "%02lx ", (sysopcode&0xff));
	o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "%02lx ", ((sysopcode>>8)&0xff));
	switch (sysopcode&0xff) {
		case PU32_OPNOTAVAIL:
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "OPNOTAVAIL");
			break;
		case 0xb8:
			// Specification from the instruction set manual:
			// add %gpr1, %gpr2 |23|000|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "add ");
			decode2gpr();
			break;
		case 0xb9:
			// Specification from the instruction set manual:
			// sub %gpr1, %gpr2 |23|001|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "sub ");
			decode2gpr();
			break;
		case 0xca:
			// Specification from the instruction set manual:
			// mul %gpr1, %gpr2 |25|010|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "mul ");
			decode2gpr();
			break;
		case 0xcb:
			// Specification from the instruction set manual:
			// mulh %gpr1, %gpr2 |25|011|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "mulh ");
			decode2gpr();
			break;
		case 0xce:
			// Specification from the instruction set manual:
			// div %gpr1, %gpr2 |25|110|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "div ");
			decode2gpr();
			break;
		case 0xcf:
			// Specification from the instruction set manual:
			// mod %gpr1, %gpr2 |25|111|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "mod ");
			decode2gpr();
			break;
		case 0xc8:
			// Specification from the instruction set manual:
			// mulu %gpr1, %gpr2 |25|000|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "mulu ");
			decode2gpr();
			break;
		case 0xc9:
			// Specification from the instruction set manual:
			// mulhu %gpr1, %gpr2 |25|001|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "mulhu ");
			decode2gpr();
			break;
		case 0xcc:
			// Specification from the instruction set manual:
			// divu %gpr1, %gpr2 |25|100|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "divu ");
			decode2gpr();
			break;
		case 0xcd:
			// Specification from the instruction set manual:
			// modu %gpr1, %gpr2 |25|101|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "modu ");
			decode2gpr();
			break;
		case 0xd8:
			// Specification from the instruction set manual:
			// fadd %gpr1, %gpr2 |22|100|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "fadd ");
			decode2gpr();
			break;
		case 0xd9:
			// Specification from the instruction set manual:
			// fsub %gpr1, %gpr2 |22|101|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "fsub ");
			decode2gpr();
			break;
		case 0xda:
			// Specification from the instruction set manual:
			// fmul %gpr1, %gpr2 |22|110|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "fmul ");
			decode2gpr();
			break;
		case 0xdb:
			// Specification from the instruction set manual:
			// fdiv %gpr1, %gpr2 |22|111|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "fdiv ");
			decode2gpr();
			break;
		case 0xc3:
			// Specification from the instruction set manual:
			// and %gpr1, %gpr2 |24|011|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "and ");
			decode2gpr();
			break;
		case 0xc4:
			// Specification from the instruction set manual:
			// or %gpr1, %gpr2 |24|100|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "or ");
			decode2gpr();
			break;
		case 0xc5:
			// Specification from the instruction set manual:
			// xor %gpr1, %gpr2 |24|101|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "xor ");
			decode2gpr();
			break;
		case 0xc6:
			// Specification from the instruction set manual:
			// not %gpr1, %gpr2 |24|110|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "not ");
			decode2gpr();
			break;
		case 0xc7:
			// Specification from the instruction set manual:
			// cpy %gpr1, %gpr2 |24|111|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "cpy ");
			decode2gpr();
			break;
		case 0xc0:
			// Specification from the instruction set manual:
			// sll %gpr1, %gpr2 |24|000|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "sll ");
			decode2gpr();
			break;
		case 0xc1:
			// Specification from the instruction set manual:
			// srl %gpr1, %gpr2 |24|001|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "srl ");
			decode2gpr();
			break;
		case 0xc2:
			// Specification from the instruction set manual:
			// sra %gpr1, %gpr2 |24|010|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "sra ");
			decode2gpr();
			break;
		case 0xba:
			// Specification from the instruction set manual:
			// seq %gpr1, %gpr2 |23|010|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "seq ");
			decode2gpr();
			break;
		case 0xbb:
			// Specification from the instruction set manual:
			// sne %gpr1, %gpr2 |23|011|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "sne ");
			decode2gpr();
			break;
		case 0xbc:
			// Specification from the instruction set manual:
			// slt %gpr1, %gpr2 |23|100|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "slt ");
			decode2gpr();
			break;
		case 0xbd:
			// Specification from the instruction set manual:
			// slte %gpr1, %gpr2 |23|101|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "slte ");
			decode2gpr();
			break;
		case 0xbe:
			// Specification from the instruction set manual:
			// sltu %gpr1, %gpr2 |23|110|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "sltu ");
			decode2gpr();
			break;
		case 0xbf:
			// Specification from the instruction set manual:
			// slteu %gpr1, %gpr2 |23|111|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "slteu ");
			decode2gpr();
			break;
		case 0xb0:
			// Specification from the instruction set manual:
			// sgt %gpr1, %gpr2 |22|000|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "sgt ");
			decode2gpr();
			break;
		case 0xb1:
			// Specification from the instruction set manual:
			// sgte %gpr1, %gpr2 |22|001|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "sgte ");
			decode2gpr();
			break;
		case 0xb2:
			// Specification from the instruction set manual:
			// sgtu %gpr1, %gpr2 |22|010|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "sgtu ");
			decode2gpr();
			break;
		case 0xb3:
			// Specification from the instruction set manual:
			// sgteu %gpr1, %gpr2 |22|011|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "sgteu ");
			decode2gpr();
			break;
		case 0xd0:
			// Specification from the instruction set manual:
			// jz %gpr1 %gpr2 |26|000|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "jz ");
			decode2gpr();
			break;
		case 0xd1:
			// Specification from the instruction set manual:
			// jnz %gpr1 %gpr2 |26|001|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "jnz ");
			decode2gpr();
			break;
		case 0xd2:
			// Specification from the instruction set manual:
			// jl %gpr1 %gpr2 |26|010|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "jl ");
			decode2gpr();
			break;
		case 0xad:
			// Specification from the instruction set manual:
			// rli16 %gpr, imm |21|101|rrrr|0000|
			//                 |iiiiiiiiiiiiiiii|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "rli16 ");
			decode1gpr();
			break;
		case 0xae:
			// Specification from the instruction set manual:
			// rli32 %gpr, imm |21|110|rrrr|0000|
			//                 |iiiiiiiiiiiiiiii| 16 msb.
			//                 |iiiiiiiiiiiiiiii| 16 lsb.
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "rli32 ");
			decode1gpr();
			break;
		case 0xac:
			// Specification from the instruction set manual:
			// drli %gpr, imm |21|100|rrrr|0000|
			//                |iiiiiiiiiiiiiiii| 16 msb.
			//                |iiiiiiiiiiiiiiii| 16 lsb.
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "drli ");
			decode1gpr();
			break;
		case 0xa1:
			// Specification from the instruction set manual:
			// inc16 %gpr, imm	|20|001|rrrr|0000|
			// 			|iiiiiiiiiiiiiiii|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "inc16 ");
			decode1gpr();
			break;
		case 0xa2:
			// Specification from the instruction set manual:
			// inc32 %gpr, imm	|20|010|rrrr|0000|
			// 			|iiiiiiiiiiiiiiii| 16 msb.
			// 			|iiiiiiiiiiiiiiii| 16 lsb.
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "inc32 ");
			decode1gpr();
			break;
		case 0xa9:
			// Specification from the instruction set manual:
			// li16 %gpr, imm	|21|001|rrrr|0000|
			// 			|iiiiiiiiiiiiiiii| 16 msb.
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "li16 ");
			decode1gpr();
			break;
		case 0xaa:
			// Specification from the instruction set manual:
			// li32 %gpr, imm	|21|010|rrrr|0000|
			// 			|iiiiiiiiiiiiiiii| 16 msb.
			// 			|iiiiiiiiiiiiiiii| 16 lsb.
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "li32 ");
			decode1gpr();
			break;
		case 0xf4:
			// Specification from the instruction set manual:
			// ld8 %gpr1, %gpr2 |30|100|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "ld8 ");
			decode2gpr();
			break;
		case 0xf5:
			// Specification from the instruction set manual:
			// ld16 %gpr1, %gpr2 |30|101|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "ld16 ");
			decode2gpr();
			break;
		case 0xf6:
			// Specification from the instruction set manual:
			// ld32 %gpr1, %gpr2 |30|110|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "ld32 ");
			decode2gpr();
			break;
		case 0xf0:
			// Specification from the instruction set manual:
			// st8 %gpr1, %gpr2 |30|000|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "st8 ");
			decode2gpr();
			break;
		case 0xf1:
			// Specification from the instruction set manual:
			// st16 %gpr1, %gpr2 |30|001|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "st16 ");
			decode2gpr();
			break;
		case 0xf2:
			// Specification from the instruction set manual:
			// st32 %gpr1, %gpr2 |30|010|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "st32 ");
			decode2gpr();
			break;
		case 0x74:
			// Specification from the instruction set manual:
			// ld8v %gpr1, %gpr2 |14|100|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "ld8v ");
			decode2gpr();
			break;
		case 0x75:
			// Specification from the instruction set manual:
			// ld16v %gpr1, %gpr2 |14|101|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "ld16v ");
			decode2gpr();
			break;
		case 0x76:
			// Specification from the instruction set manual:
			// ld32v %gpr1, %gpr2 |14|110|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "ld32v ");
			decode2gpr();
			break;
		case 0x70:
			// Specification from the instruction set manual:
			// st8v %gpr1, %gpr2 |14|000|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "st8v ");
			decode2gpr();
			break;
		case 0x71:
			// Specification from the instruction set manual:
			// st16v %gpr1, %gpr2 |14|001|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "st16v ");
			decode2gpr();
			break;
		case 0x72:
			// Specification from the instruction set manual:
			// st32v %gpr1, %gpr2 |14|010|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "st32v ");
			decode2gpr();
			break;
		case 0xf8:
			// Specification from the instruction set manual:
			// ldst8 %gpr1, %gpr2 |31|000|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "ldst8 ");
			decode2gpr();
			break;
		case 0xf9:
			// Specification from the instruction set manual:
			// ldst16 %gpr1, %gpr2 |31|001|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "ldst16 ");
			decode2gpr();
			break;
		case 0xfa:
			// Specification from the instruction set manual:
			// ldst32 %gpr1, %gpr2 |31|010|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "ldst32 ");
			decode2gpr();
			break;
		case 0xfc:
			// Specification from the instruction set manual:
			// cldst8 %gpr1, %gpr2 |31|100|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "cldst8 ");
			decode2gpr();
			break;
		case 0xfd:
			// Specification from the instruction set manual:
			// cldst16 %gpr1, %gpr2 |31|101|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "cldst16 ");
			decode2gpr();
			break;
		case 0xfe:
			// Specification from the instruction set manual:
			// cldst32 %gpr1, %gpr2 |31|110|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "cldst32 ");
			decode2gpr();
			break;
		case 0x00:
			// Specification from the instruction set manual:
			// sysret |0|000|0000|0000|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "sysret ");
			break;
		case 0x01:
			// Specification from the instruction set manual:
			// syscall |0|001|0000|0000|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "syscall ");
			break;
		case 0x02:
			// Specification from the instruction set manual:
			// brk |0|010|0000|0000|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "brk ");
			break;
		case 0x03:
			// Specification from the instruction set manual:
			// halt |0|011|0000|0000|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "halt ");
			break;
		case 0x04:
			// Specification from the instruction set manual:
			// icacherst |0|100|0000|0000|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "icacherst ");
			break;
		case 0x05:
			// Specification from the instruction set manual:
			// dcacherst |0|101|0000|0000|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "dcacherst ");
			break;
		case 0x07:
			// Specification from the instruction set manual:
			// ksysret |0|111|0000|0000|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "ksysret ");
			break;
		case 0x39:
			// Specification from the instruction set manual:
			// setksl %gpr |7|001|rrrr|0000|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "setksl ");
			decode1gpr();
			break;
		case 0x3c:
			// Specification from the instruction set manual:
			// setasid %gpr |7|100|rrrr|0000|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "setasid ");
			decode1gpr();
			break;
		case 0x3d:
			// Specification from the instruction set manual:
			// setuip %gpr |7|101|rrrr|0000|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "setuip ");
			decode1gpr();
			break;
		case 0x3e:
			// Specification from the instruction set manual:
			// setflags %gpr |7|110|rrrr|0000|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "setflags ");
			decode1gpr();
			break;
		case 0x3f:
			// Specification from the instruction set manual:
			// settimer %gpr |7|111|rrrr|0000|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "settimer ");
			decode1gpr();
			break;
		case 0x3a:
			// Specification from the instruction set manual:
			// settlb %gpr1, %gpr2 |7|010|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "settlb ");
			decode2gpr();
			break;
		case 0x3b:
			// Specification from the instruction set manual:
			// clrtlb %gpr1, %gpr2 |7|011|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "clrtlb ");
			decode2gpr();
			break;
		case 0x79:
			// Specification from the instruction set manual:
			// setkgpr %gpr1 %gpr2 |15|001|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "setkgpr ");
			decode2gpr();
			break;
		case 0x7a:
			// Specification from the instruction set manual:
			// setugpr %gpr1 %gpr2 |15|010|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "setugpr ");
			decode2gpr();
			break;
		case 0x7b:
			// Specification from the instruction set manual:
			// setgpr %gpr1 %gpr2 |15|011|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "setgpr ");
			decode2gpr();
			break;
		case 0x28:
			// Specification from the instruction set manual:
			// getsysopcode %gpr |5|000|rrrr|0000|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "getsysopcode ");
			decode1gpr();
			break;
		case 0x29:
			// Specification from the instruction set manual:
			// getuip %gpr |5|001|rrrr|0000|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "getuip ");
			decode1gpr();
			break;
		case 0x2a:
			// Specification from the instruction set manual:
			// getfaultaddr %gpr |5|010|rrrr|0000|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "getfaultaddr ");
			decode1gpr();
			break;
		case 0x2b:
			// Specification from the instruction set manual:
			// getfaultreason %gpr |5|011|rrrr|0000|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "getfaultreason ");
			decode1gpr();
			break;
		case 0x2c:
			// Specification from the instruction set manual:
			// getclkcyclecnt %gpr |5|100|rrrr|0000|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "getclkcyclecnt ");
			decode1gpr();
			break;
		case 0x2d:
			// Specification from the instruction set manual:
			// getclkcyclecnth %gpr |5|101|rrrr|0000|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "getclkcyclecnth ");
			decode1gpr();
			break;
		case 0x2e:
			// Specification from the instruction set manual:
			// gettlbsize %gpr |5|110|rrrr|0000|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "gettlbsize ");
			decode1gpr();
			break;
		case 0x2f:
			// Specification from the instruction set manual:
			// geticachesize %gpr |5|111|rrrr|0000|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "geticachesize ");
			decode1gpr();
			break;
		case 0x10:
			// Specification from the instruction set manual:
			// getcoreid %gpr |2|000|rrrr|0000|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "getcoreid ");
			decode1gpr();
			break;
		case 0x11:
			// Specification from the instruction set manual:
			// getclkfreq %gpr |2|001|rrrr|0000|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "getclkfreq ");
			decode1gpr();
			break;
		case 0x12:
			// Specification from the instruction set manual:
			// getdcachesize %gpr |2|010|rrrr|0000|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "getdcachesize ");
			decode1gpr();
			break;
		case 0x13:
			// Specification from the instruction set manual:
			// gettlb %gpr1, %gpr2 |2|011|rrrr|rrrr|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "gettlb ");
			decode2gpr();
			break;
		case 0x14:
			// Specification from the instruction set manual:
			// getcap %gpr |2|100|rrrr|0000|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "getcap ");
			decode1gpr();
			break;
		case 0x15:
			// Specification from the instruction set manual:
			// getver %gpr |2|101|rrrr|0000|
			o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "getver ");
			decode1gpr();
			break;
		default: {
			if (sysopcode == 0b10010000)
				o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "preempt");
			else {
				do {
					unsigned long x = ((sysopcode>>4)&0xf);
					if (x == 0b1000) {
						// Specification from the instruction set manual:
						// li8 %gpr, imm |1000|iiii|rrrr|iiii|
						o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "li8 ");
					} else if (x == 0b1001) {
						// Specification from the instruction set manual:
						// inc8 %gpr, imm |1001|iiii|rrrr|iiii|
						o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "inc8 ");
					} else if (x == 0b1110) {
						// Specification from the instruction set manual:
						// rli8 %gpr, imm |1110|iiii|rrrr|iiii|
						o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "rli8 ");
					} else {
						o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "????");
						break;
					}
					decode1gpr();
					o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, " ");
					int8_t n = ((sysopcode>>8) + (((sysopcode&0xf)&0xf)<<4));
					if (n < 0) {
						n = -n;
						o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "-");
					}
					o += snprintf (&pu32sysopcodestrbuf[o], PU32SYSOPCODESTRBUFSZ-o, "%d", n);
				} while (0);
			}
			break;
		}
	}
	return pu32sysopcodestrbuf;
}
