// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

//#include <linux/module.h>
//#include <linux/sched.h>
//#include <linux/personality.h>
//#include <linux/binfmts.h>
#include <linux/elf.h>
#include <linux/mman.h>

#if ELF_EXEC_PAGESIZE > PAGE_SIZE
#define ELF_MIN_ALIGN	ELF_EXEC_PAGESIZE
#else
#define ELF_MIN_ALIGN	PAGE_SIZE
#endif

#define ELF_PAGESTART(_v) ((_v) & ~(unsigned long)(ELF_MIN_ALIGN-1))
#define ELF_PAGEOFFSET(_v) ((_v) & (ELF_MIN_ALIGN-1))
#define ELF_PAGEALIGN(_v) (((_v) + ELF_MIN_ALIGN - 1) & ~(ELF_MIN_ALIGN - 1))

#define BAD_ADDR(x) ((unsigned long)(x) >= TASK_SIZE)

unsigned long elf_map (
	struct file *filep, unsigned long addr,
	const struct elf32_phdr *eppnt,
	int prot, int type, unsigned long total_size) {

	unsigned long map_addr;
	unsigned long size = eppnt->p_filesz + ELF_PAGEOFFSET(eppnt->p_vaddr);
	unsigned long off = eppnt->p_offset - ELF_PAGEOFFSET(eppnt->p_vaddr);
	addr = ELF_PAGESTART(addr);
	size = ELF_PAGEALIGN(size);

	/* mmap() will return -EINVAL if given a zero size, but a
	 * segment with zero filesize is perfectly valid */
	if (!size)
		return addr;

	// Adjust addr only when the first elf program header is parsed;
	// subsequent mappings will follow the first mapping.
	if (!off) {
		if (addr < TASK_UNMAPPED_BASE)
			addr += (TASK_UNMAPPED_BASE - addr);
	}
	if (addr < TASK_UNMAPPED_BASE)
		return -EINVAL; // Would occur if first mapping was not adjusted.

	/*
	* total_size is the size of the ELF (interpreter) image.
	* The _first_ mmap needs to know the full size, otherwise
	* randomization might put this image into an overlapping
	* position with the ELF binary image. (since size < total_size)
	* So we first map the 'big' image - and unmap the remainder at
	* the end. (which unmap is needed for ELF images with holes.)
	*/
	if (total_size) {
		total_size = ELF_PAGEALIGN(total_size);
		map_addr = vm_mmap(filep, addr, total_size, prot, type, off);
		if (!BAD_ADDR(map_addr))
			vm_munmap(map_addr+size, total_size-size);
	} else
		map_addr = vm_mmap(filep, addr, size, prot, type, off);

	if ((type & MAP_FIXED_NOREPLACE) &&
	    PTR_ERR((void *)map_addr) == -EEXIST)
		pr_info("(%d:%s): elf segment at 0x%lx requested but the memory is mapped already\n",
			task_pid_nr(current), current->comm, addr);

	return(map_addr);
}
