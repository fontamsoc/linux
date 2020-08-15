// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#include <asm/vmlinux.lds.h>
#include <asm/page.h>
#include <asm/thread_info.h>
#include <asm/cache.h>

#include <generated/asm-offsets.h>

#include <pu32.h>

OUTPUT_ARCH(pu32)
ENTRY(_start)

jiffies = jiffies_64;

SECTIONS {

	. = KERNELADDR;

	_stext = .;
	HEAD_TEXT_SECTION
	INIT_TEXT_SECTION(PAGE_SIZE)
	INIT_DATA_SECTION(PAGE_SIZE)
	/* For SMP support _inc_cpu_up_arg() _start_smp(),
	   for now we always want to keep __init __INIT code. */
	__init_begin = .;
	/* . = ALIGN(PAGE_SIZE); */
	__init_end = .;

	_text = .;
	.text : {
		TEXT_TEXT
		SCHED_TEXT
		CPUIDLE_TEXT
		LOCK_TEXT
		IRQENTRY_TEXT
		SOFTIRQENTRY_TEXT
		KPROBES_TEXT
		*(.text.*)
		. = ALIGN(PAGE_SIZE);
		__pu32tramp_start = .;
		*(.pu32tramp.text)
		. = ALIGN(PAGE_SIZE);
		__pu32tramp_end = .;
	}
	_etext = .;

	_sdata = .;

	RO_DATA(PAGE_SIZE)

	__start_rwdata = .;

	.data : {
		. = ALIGN(THREAD_SIZE);
		/* Conveniently placed after the .rodata section
		   to possibly trigger a pagefault on stack-overflow. */
		init_stack = .;
		. += OFFSETOF_TI;
		KEEP(*(.data..init_thread_info))
		. = init_stack + THREAD_SIZE;
		__start_init_task = .;
		KEEP(*(.data..init_task))
		__end_init_task = .;
		NOSAVE_DATA
		PAGE_ALIGNED_DATA(PAGE_SIZE)
		CACHELINE_ALIGNED_DATA(L1_CACHE_BYTES)
		READ_MOSTLY_DATA(L1_CACHE_BYTES)
		DATA_DATA
		KERNEL_CTORS()
	}
	PERCPU_SECTION(L1_CACHE_BYTES)

	__bss_start = .;
	. = ALIGN(L1_CACHE_BYTES);
	SBSS(0)
	BSS(0)
	. = ALIGN(L1_CACHE_BYTES);
	__bss_stop = .;

	BUG_TABLE

	. = ALIGN(PAGE_SIZE);
	__end_rwdata = .;
	_edata = .;
	_end = .;

	STABS_DEBUG
	DWARF_DEBUG
	ELF_DETAILS
	DISCARDS
}
