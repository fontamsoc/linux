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

#define PU32_OPNOTAVAIL (8<<3)

char *pu32sysopcodestr (unsigned long sysopcode);

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
	asm volatile ("getclkfreq %0\n" : "=r" (n) :: "memory");
	return n;
}

static inline ssize_t pu32sysread (int fd, void *buf, size_t count) {

	register uintptr_t a1 asm("%1") = fd;
	register uintptr_t a2 asm("%2") = (uintptr_t)buf;
	register uintptr_t a3 asm("%3") = count;

	asm volatile (
		"li %%sr, %1\n"
		"syscall\n" :
		"=r"(a1) /* Output constraints */ :
		"i"(__NR_read), "r"(a1), "r"(a2), "r"(a3) /* Input constraints */ :
		"memory");

	return ((a1 != -1) ? a1 : 0);
}

static inline ssize_t pu32syswrite (int fd, void *buf, size_t count) {

	register uintptr_t a1 asm("%1") = fd;
	register uintptr_t a2 asm("%2") = (uintptr_t)buf;
	register uintptr_t a3 asm("%3") = count;

	asm volatile (
		"li %%sr, %1\n"
		"syscall\n" :
		"=r"(a1) /* Output constraints */ :
		"i"(__NR_write), "r"(a1), "r"(a2), "r"(a3) /* Input constraints */ :
		"memory");

	return ((a1 != -1) ? a1 : 0);
}

void pu32printf (const char *fmt, ...);

static inline off_t pu32syslseek (int fd, off_t offset, int whence) {

	register uintptr_t a1 asm("%1") = fd;
	register uintptr_t a2 asm("%2") = offset;
	register uintptr_t a3 asm("%3") = whence;

	asm volatile (
		"li %%sr, %1\n"
		"syscall\n" :
		"=r"(a1) /* Output constraints */ :
		"i"(__NR_lseek), "r"(a1), "r"(a2), "r"(a3) /* Input constraints */ :
		"memory");

	return ((a1 != -1) ? a1 : 0);
}

#endif /* !__ASSEMBLY__ */

#endif /* __PU32_ARCH_H */
