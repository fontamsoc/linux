// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#ifndef __ASM_PU32_PCI_H
#define __ASM_PU32_PCI_H

#define	pcibios_assign_all_busses()	1

#define	PCIBIOS_MIN_IO		0
#define	PCIBIOS_MIN_MEM		0

#ifdef CONFIG_PCI
static inline int pci_get_legacy_ide_irq (
	struct pci_dev *dev, int channel) {
        // No legacy IRQ.
        return -ENODEV;
}
#define HAVE_ARCH_PCI_GET_LEGACY_IDE_IRQ

static inline int pci_proc_domain (struct pci_bus *bus) {
        // Always show the domain in /proc .
        return 1;
}
#endif  /* CONFIG_PCI */

#endif /* __ASM_PU32_PCI_H */
