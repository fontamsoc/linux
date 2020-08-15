# SPDX-License-Identifier: GPL-2.0-only
# (c) William Fonkou Tambe

MODNAME := pu32fbdev

# If KERNELRELEASE is defined, we've been invoked from
# the kernel build system and can use its language.
ifneq (${KERNELRELEASE},)
	obj-${CONFIG_PU32FBDEV} += ${MODNAME}.o
# Otherwise we were called directly from the
# command line; invoke the kernel build system.
else
	KDIR ?= /lib/modules/$(shell uname -r)/build

${MODNAME}.ko:
	${MAKE} -C ${KDIR} M=$(shell pwd) CONFIG_PU32FBDEV=m modules

modules_install: ${MODNAME}.ko
	${MAKE} -C ${KDIR} M=$(shell pwd) CONFIG_PU32FBDEV=m modules_install
	depmod -A

clean:
	${MAKE} -C ${KDIR} M=$(shell pwd) clean

endif
