// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/console.h>
#include <linux/memblock.h>
#include <linux/initrd.h>
#include <linux/start_kernel.h>

#include <asm/sections.h>
#include <asm/pgalloc.h>
#include <asm/tlbflush.h>
#include <asm/setup.h>

#include <pu32.h>

unsigned long pu32_mem_start;
unsigned long pu32_mem_end;
unsigned long pu32_mem_end_high;
unsigned long pu32_TASK_UNMAPPED_BASE;

unsigned long pu32_ishw = 0;

void __init setup_arch (char **cmdline_p) {

	init_mm.start_code = (unsigned long)_stext;
	init_mm.end_code = (unsigned long)_etext;
	init_mm.end_data = (unsigned long)_edata;
	init_mm.brk = (unsigned long)_end;

	*cmdline_p = boot_command_line;

	parse_early_param();

	/* memblock_init(void) */ {
		memblock_add(pu32_mem_start, pu32_mem_end - pu32_mem_start);
		if ((unsigned long)_stext > (unsigned long)PAGE_SIZE) /* Reserved PU32_BIOS_RESERVED_MEM */
			memblock_reserve((unsigned long)PAGE_SIZE, (unsigned long)_stext - (unsigned long)PAGE_SIZE);
		memblock_reserve((unsigned long)_stext, (unsigned long)_end - (unsigned long)_stext);
		min_low_pfn = PFN_UP(pu32_mem_start);
		max_low_pfn = PFN_DOWN(pu32_mem_end);
		max_pfn = max_low_pfn;
		max_mapnr = max_low_pfn;
		memblock_set_current_limit(PFN_PHYS(max_low_pfn));

		unsigned long max_zone_pfn[MAX_NR_ZONES];
		memset(max_zone_pfn, 0, sizeof(max_zone_pfn));
		max_zone_pfn[ZONE_NORMAL] = max_pfn;
		free_area_init(max_zone_pfn);
	}

	early_memtest(min_low_pfn << PAGE_SHIFT, max_low_pfn << PAGE_SHIFT);

	pr_info("Phys. mem: %ldMB\n",
		(unsigned long)memblock_phys_mem_size()/1024/1024);

	if (TASK_UNMAPPED_BASE >= TASK_SIZE)
		panic("no space for virtual memory; TASK_UNMAPPED_BASE == 0x%x; TASK_SIZE == 0x%x\n",
			(unsigned)TASK_UNMAPPED_BASE, (unsigned)TASK_SIZE);

	pr_info("Virt. mem: %ldMB\n", (TASK_SIZE - TASK_UNMAPPED_BASE)/1024/1024);
}

void __init trap_init (void) {}

#include <hwdrvdevtbl.h>

__attribute__((noreturn)) void __init pu32_start (char **argv, char **envp) {
	// Adjust %ksl to enable caching throughout the kernel image memory range.
	asm volatile ("setksl %0\n" :: "r"((void *)_end));

	// Zero section .bss .
	memset32((uint32_t *)__bss_start, 0, ((__bss_stop - __bss_start) / sizeof(uint32_t)));

	char *getenv (char *name, char **envp) {
		if (!envp)
			return NULL;
		unsigned len = strlen(name);
		for (; *envp; ++envp)
			if (strncmp(*envp, name, len) == 0) {
				char *c = *envp + len;
				if (*c == '=')
					return (char *)++c;
			}
		return NULL;
	}

	if (getenv("ISHW", envp)) {

		pu32_ishw = 1;

		hwdrvdevtbl hwdrvdevtbl_dev = {.e = (devtblentry *)0, .id = 1 /* RAM device */};

		hwdrvdevtbl_find (&hwdrvdevtbl_dev);
		if (!hwdrvdevtbl_dev.mapsz) {
			pu32stdout("memory not found\n");
			while(1); }
		pu32_mem_start = (unsigned long)hwdrvdevtbl_dev.addr;
		pu32_mem_end_high = pu32_mem_start + (hwdrvdevtbl_dev.mapsz * sizeof(unsigned long));

	} else {
		char *e = getenv("MEMSTARTADDR", envp); // Memory start.
		if (e && kstrtoul(e, 0, &pu32_mem_start) == 0)
			/*pu32stdout("MEMSTARTADDR\t:0x%x\n", pu32_mem_start)*/;
		else {
			pu32stdout("memory not found\n");
			while(1);
		}

		e = getenv("MEMENDADDR", envp); // First byte after last memory byte.
		if (e && kstrtoul(e, 0, &pu32_mem_end_high) == 0)
			/*pu32stdout("MEMENDADDR\t:0x%x\n", pu32_mem_end_high)*/;
		else {
			pu32stdout("memory not found\n");
			while(1);
		}
	}

	// By convention, the first physical page cannot be RAM.
	pu32_mem_start = ((pu32_mem_start > PAGE_SIZE) ? pu32_mem_start : PAGE_SIZE);
	// We max low memory to (TASK_SIZE/3), which must be
	// less than 0x50000000 (Binutils.TEXT_START_ADDR).
	// TASK_UNMAPPED_BASE is defined to pu32_TASK_UNMAPPED_BASE, hence
	// the lower the physical memory, the higher the virtual memory.
	pu32_mem_end = ((pu32_mem_end_high > (TASK_SIZE/3)) ? (TASK_SIZE/3) : pu32_mem_end_high);
	pu32_TASK_UNMAPPED_BASE = ROUNDUPTOPOWEROFTWO(pu32_mem_end, PGDIR_SIZE); // RoundUp as it is used as FIRST_USER_ADDRESS value.

	char *p = argv[1]; if (p) {
		strlcpy(boot_command_line, p, COMMAND_LINE_SIZE);
		int i = 2; for (; (p = argv[i]); ++i) {
			strlcat(boot_command_line, " ", COMMAND_LINE_SIZE);
			strlcat(boot_command_line, p, COMMAND_LINE_SIZE);
		}
	} else
		strlcpy(
			boot_command_line,
			#if defined(CONFIG_PU32_DEBUG)
			"earlyprintk=keep loglevel=15 initcall_debug=1",
			#else
			"earlyprintk=keep",
			#endif
			COMMAND_LINE_SIZE);

	start_kernel();

	while(1);
}
