// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef __ASM_PU32_ELF_H
#define __ASM_PU32_ELF_H

#include <uapi/asm/ptrace.h>
#include <asm/page.h>

#define EM_PU32 0xdeed

// PU32 specific relocs.
#define R_PU32_NONE	0
#define R_PU32_8	1
#define R_PU32_16	2
#define R_PU32_32	3
#define R_PU32_8_PCREL	4
#define R_PU32_16_PCREL	5
#define R_PU32_32_PCREL	6

// Used to set parameters in the core dumps.
#define ELF_ARCH	EM_PU32
#define ELF_CLASS	ELFCLASS32
#define ELF_DATA	ELFDATA2LSB
#define ELF_OSABI 	ELFOSABI_NONE

// Used to ensure we don't load something for the wrong architecture.
#define elf_check_arch(x) ((x)->e_machine == EM_PU32)

#define ELF_EXEC_PAGESIZE PAGE_SIZE

// This is the location where an ET_DYN program is loaded if exec'ed.
// Typical use of this is to invoke "./ld.so someprog" to test out
// a new version of the loader. Need to make sure that it is out of
// the way of the program that it will "exec", and that there is
// sufficient room for the brk.
#define ELF_ET_DYN_BASE ((TASK_SIZE / 3) * 2)

#define ELF_HWCAP 0
#define ELF_PLATFORM  (NULL)

#define CORE_DUMP_USE_REGSET

typedef unsigned long elf_greg_t;
#define ELF_NGREG (sizeof(struct pt_regs) / sizeof(elf_greg_t))
typedef elf_greg_t elf_gregset_t[ELF_NGREG];
typedef float elf_fpreg_t;
#define ELF_NFPREG 1
typedef elf_fpreg_t elf_fpregset_t[ELF_NFPREG];

#endif /* __ASM_PU32_ELF_H */
