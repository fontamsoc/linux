// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/console.h>
#include <linux/memblock.h>
#include <linux/initrd.h>
#include <linux/start_kernel.h>
#include <linux/ioport.h>

#include <asm/sections.h>
#include <asm/pgalloc.h>
#include <asm/tlbflush.h>
#include <asm/setup.h>
#include <asm/cpuinfo.h>

#include <pu32.h>
#include <hwdrvdevtbl.h>

#if defined(CONFIG_NODES_SHIFT) && (CONFIG_NR_CPUS != (1<<CONFIG_NODES_SHIFT))
#error NODES_SHIFT is invalid
#endif

unsigned long pu32_mem_start;
unsigned long pu32_mem_end;
unsigned long pu32_mem_end_high;
unsigned long pu32_TASK_UNMAPPED_BASE;

unsigned long pu32_ishw = 0;
unsigned long pu32_bios_end = PAGE_SIZE;

static struct resource bios_res = { .name = "BIOS", };
static struct resource kimage_res = { .name = "Kernel image", };
static struct resource code_res = { .name = "Kernel code", };
static struct resource data_res = { .name = "Kernel data", };
static struct resource rodata_res = { .name = "Kernel rodata", };
static struct resource rwdata_res = { .name = "Kernel rwdata", };
static struct resource bss_res = { .name = "Kernel bss", };

static int __init add_resource (struct resource *parent, struct resource *res) {
	int ret = 0;
	ret = insert_resource (parent, res);
	if (ret < 0) {
		pr_err("Failed to add a %s resource at %x\n", res->name, res->start);
		return ret;
	}
	return 1;
}

static int __init add_kernel_resources (void) {

	int ret;

	if (pu32_bios_end > pu32_mem_start) {
		bios_res.start = (unsigned long)pu32_mem_start;
		bios_res.end = (unsigned long)pu32_bios_end - 1;
		bios_res.flags = IORESOURCE_SYSTEM_RAM | IORESOURCE_BUSY;

		ret = add_resource(&iomem_resource, &bios_res);
		if (ret < 0)
			return ret;
	}

	kimage_res.start = (unsigned long)_stext;
	kimage_res.end = (unsigned long)_end;
	kimage_res.flags = IORESOURCE_SYSTEM_RAM | IORESOURCE_BUSY;

	code_res.start = (unsigned long)_text;
	code_res.end = (unsigned long)_etext - 1;
	code_res.flags = IORESOURCE_SYSTEM_RAM | IORESOURCE_BUSY;

	data_res.start = (unsigned long)_sdata;
	data_res.end = (unsigned long)_edata - 1;
	data_res.flags = IORESOURCE_SYSTEM_RAM | IORESOURCE_BUSY;

	rodata_res.start = (unsigned long)__start_rodata;
	rodata_res.end = (unsigned long)__end_rodata - 1;
	rodata_res.flags = IORESOURCE_SYSTEM_RAM | IORESOURCE_BUSY;

	extern char __start_rwdata[], __end_rwdata[];
	rwdata_res.start = (unsigned long)__start_rwdata;
	rwdata_res.end = (unsigned long)__end_rwdata - 1;
	rwdata_res.flags = IORESOURCE_SYSTEM_RAM | IORESOURCE_BUSY;

	bss_res.start = (unsigned long)__bss_start;
	bss_res.end = (unsigned long)__bss_stop - 1;
	bss_res.flags = IORESOURCE_SYSTEM_RAM | IORESOURCE_BUSY;

	ret = add_resource(&iomem_resource, &kimage_res);
	if (ret < 0)
		return ret;

	ret = add_resource(&kimage_res, &code_res);
	if (ret < 0)
		return ret;

	ret = add_resource(&kimage_res, &data_res);
	if (ret < 0)
		return ret;

	ret = add_resource(&data_res, &rodata_res);
	if (ret < 0)
		return ret;

	ret = add_resource(&data_res, &rwdata_res);
	if (ret < 0)
		return ret;

	ret = add_resource(&rwdata_res, &bss_res);

	return ret;
}

static char *pu32devidtoname[] = {
	 [0] = "Reserved"
	,[1] = "RAM"
	,[2] = "DMA"
	,[3] = "IntCtrl"
	,[4] = "BlkDev"
	,[5] = "CharDev"
	,[6] = "GPIO"
	,[7] = "DevTbl"
	,[8] = "SpiMaster"
	,[9] = "PWM"
	,[10] = "FrameBuffer"
};

static void __init init_resources (void) {
	// Count devices mapped in physical address space.
	unsigned long devcnt = 0;
	if (pu32_ishw) {
		hwdrvdevtbl hwdrvdevtbl_dev = {.e = (devtblentry *)0, .id = -1};
		while (hwdrvdevtbl_find (&hwdrvdevtbl_dev, (void *)-1), hwdrvdevtbl_dev.mapsz)
			devcnt += 1;
	} else
		devcnt += 1;

	// We should use +1 as memblock_alloc() might increase memblock.reserved.cnt,
	// but since devcnt needs to be decremented for RAM device at 0x1000, it is omitted.
	unsigned long num_resources = memblock.memory.cnt + memblock.reserved.cnt + devcnt;
	unsigned long res_idx = num_resources -1;

	struct resource *mem_res;
	size_t mem_res_sz = num_resources * sizeof(*mem_res);
	if (!(mem_res = memblock_alloc(mem_res_sz, L1_CACHE_BYTES)))
		panic("%s: Failed to allocate %u bytes\n", __func__, mem_res_sz);

	// Start by adding the reserved regions, if they overlap with
	// memory regions, insert_resource() will later on take care of it.
	if (add_kernel_resources() < 0)
		goto err;

	struct resource *res = NULL;

	struct memblock_region *region = NULL;
	for_each_reserved_mem_region(region) {
		res = &mem_res[res_idx--];

		res->name = "Reserved";
		res->flags = IORESOURCE_MEM | IORESOURCE_BUSY;
		res->start = __pfn_to_phys(memblock_region_reserved_base_pfn(region));
		res->end = __pfn_to_phys(memblock_region_reserved_end_pfn(region)) - 1;

		// Ignore any other reserved regions within system memory.
		if (memblock_is_memory(res->start)) {
			// Re-use this pre-allocated resource.
			++res_idx;
			continue;
		}

		if (add_resource(&iomem_resource, res) < 0)
			goto err;
	}

	// Add memory regions to the resource tree.
	for_each_mem_region(region) {
		res = &mem_res[res_idx--];

		if (memblock_is_nomap(region)) {
			res->name = "Reserved";
			res->flags = IORESOURCE_MEM | IORESOURCE_BUSY;
		} else {
			res->name = "System RAM";
			res->flags = IORESOURCE_SYSTEM_RAM | IORESOURCE_BUSY;
		}

		res->start = __pfn_to_phys(memblock_region_memory_base_pfn(region));
		res->end = __pfn_to_phys(memblock_region_memory_end_pfn(region)) - 1;

		if (add_resource(&iomem_resource, res) < 0)
			goto err;
	}

	if (pu32_ishw) {
		hwdrvdevtbl hwdrvdevtbl_dev = {.e = (devtblentry *)0, .id = -1};
		while (hwdrvdevtbl_find (&hwdrvdevtbl_dev, (void *)-1), hwdrvdevtbl_dev.mapsz) {

			if (hwdrvdevtbl_dev.addr == (void *)0x1000)
				continue; // Skip RAM Device at 0x1000 which is "System RAM",
				          // and which was not counted in num_resources.

			res = &mem_res[res_idx--];

			res->name = (hwdrvdevtbl_dev.id < (sizeof(pu32devidtoname)/sizeof(pu32devidtoname[0]))) ? pu32devidtoname[hwdrvdevtbl_dev.id]: "UnkownDevID";
			res->flags = ((hwdrvdevtbl_dev.id == 1) ? IORESOURCE_MEM : IORESOURCE_IO) | IORESOURCE_BUSY;
			res->start = (unsigned long)hwdrvdevtbl_dev.addr;
			res->end = (unsigned long)hwdrvdevtbl_dev.addr + ((hwdrvdevtbl_dev.mapsz * sizeof(unsigned long)) - 1);

			if (add_resource(&iomem_resource, res) < 0)
				goto err;
		}
	}

	// Clean-up any unused pre-allocated resources.
	mem_res_sz = (res_idx + 1);
	if (mem_res_sz)
		memblock_free(mem_res, mem_res_sz * sizeof(*mem_res));
	return;

 err:
	// Better an empty resource tree than an inconsistent one.
	release_child_resources(&iomem_resource);
	memblock_free(mem_res, mem_res_sz);
}

#ifdef CONFIG_SMP
void __init setup_smp (void);
#endif

void __init setup_arch (char **cmdline_p) {

	init_mm.start_code = (unsigned long)_stext;
	init_mm.end_code = (unsigned long)_etext;
	init_mm.end_data = (unsigned long)_edata;
	init_mm.brk = (unsigned long)_end;

	*cmdline_p = boot_command_line;

	parse_early_param();

	/* memblock_init(void) */ {
		memblock_add(pu32_mem_start, pu32_mem_end - pu32_mem_start);
		if (pu32_bios_end > pu32_mem_start)
			memblock_reserve(pu32_mem_start, pu32_bios_end - pu32_mem_start);
		if (pu32_bios_end < (KERNELADDR - PAGE_SIZE)) // Reserve page holding parkpu().
			memblock_reserve((KERNELADDR - PAGE_SIZE), PAGE_SIZE);
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

	init_resources();

	#ifdef CONFIG_SMP
	setup_smp();
	#endif
}

void __init trap_init (void) {}

#define DEVTBLADDR (0x200 /* By convention, the device table is located at 0x200 */)

__attribute__((noreturn)) void __init pu32_start (char **argv, char **envp) {
	// Adjust %ksl to enable caching throughout the kernel image memory range.
	asm volatile ("setksl %0\n" :: "r"((void *)_end) : "memory");

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

	void *___ishw;
	if ((___ishw = getenv("___ISHW", envp))) {

		pu32_ishw = *(unsigned long *)___ishw;

		void *___biosend;
		if ((___biosend = getenv("BIOSend", envp))) {
			// Update where BIOS ends in memory.
			pu32_bios_end = PFN_ALIGN(*(unsigned long *)___biosend);
		}

		hwdrvdevtbl hwdrvdevtbl_dev = {.e = (devtblentry *)0, .id = 1 /* RAM device */};
		hwdrvdevtbl_find (&hwdrvdevtbl_dev, NULL);
		if (!hwdrvdevtbl_dev.mapsz) {
			pu32printf("memory not found\n");
			while(1); }
		pu32_mem_start = (unsigned long)hwdrvdevtbl_dev.addr;
		pu32_mem_end_high = pu32_mem_start + (hwdrvdevtbl_dev.mapsz * sizeof(unsigned long));

		#ifdef CONFIG_PROC_FS
		c_info_rcache = 1 /* RAMCACHESZ */;
		asm volatile ("ldst %0, %1\n" : "+r"(c_info_rcache) : "r"(DEVTBLADDR) : "memory");
		c_info_rcache *= sizeof(unsigned long);
		c_info_rcache /= 1024;
		#endif

	} else {
		char *e = getenv("MEMSTARTADDR", envp); // Memory start.
		if (e && kstrtoul(e, 0, &pu32_mem_start) == 0)
			/*pu32printf("MEMSTARTADDR\t:0x%x\n", pu32_mem_start)*/;
		else {
			pu32printf("memory not found\n");
			while(1);
		}

		e = getenv("MEMENDADDR", envp); // First byte after last memory byte.
		if (e && kstrtoul(e, 0, &pu32_mem_end_high) == 0)
			/*pu32printf("MEMENDADDR\t:0x%x\n", pu32_mem_end_high)*/;
		else {
			pu32printf("memory not found\n");
			while(1);
		}

		// Update where BIOS ends in memory.
		pu32_bios_end = pu32_mem_start;

		#ifdef CONFIG_PROC_FS
		c_info_rcache = 0;
		#endif
	}

	#ifdef CONFIG_PROC_FS
	c_setup();
	#endif

	// By convention, the first physical page cannot be RAM.
	pu32_mem_start = ((pu32_mem_start > PAGE_SIZE) ? pu32_mem_start : PAGE_SIZE);
	// We max low memory to (TASK_SIZE/3), which must be
	// less than 0x50000000 (Binutils.TEXT_START_ADDR).
	// TASK_UNMAPPED_BASE is defined to pu32_TASK_UNMAPPED_BASE, hence
	// the lower the physical memory, the higher the virtual memory.
	pu32_mem_end = ((pu32_mem_end_high > (TASK_SIZE/3)) ? (TASK_SIZE/3) : pu32_mem_end_high);
	pu32_TASK_UNMAPPED_BASE = ROUNDUPTOPOWEROFTWO(pu32_mem_end, PGDIR_SIZE); // RoundUp as it is used as FIRST_USER_ADDRESS value.
	asm volatile ("setksl %0\n" :: "r"(pu32_TASK_UNMAPPED_BASE) : "memory");

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

	void init_mmu_context (void);
	init_mmu_context();

	start_kernel();

	while(1);
}
