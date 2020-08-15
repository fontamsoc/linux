// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#include <linux/extable.h>
#include <linux/uaccess.h>

int fixup_exception (struct pt_regs *regs) {
	const struct exception_table_entry *fixup =
		search_exception_tables(regs->pc);
	if (fixup) {
		regs->pc = fixup->fixup;
		return 1;
	}
	return 0;
}
