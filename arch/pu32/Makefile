
#KBUILD_CFLAGS += -pipe
KBUILD_CFLAGS += -O3 -g3 -mzpoprp
#KBUILD_CFLAGS += -save-temps -fverbose-asm
KBUILD_CFLAGS += -fno-builtin -Werror=implicit-function-declaration -fno-trampolines -Wno-declaration-after-statement
KBUILD_AFLAGS += $(KBUILD_CFLAGS)

head-y := arch/pu32/kernel/head.o

libs-y += arch/pu32/lib/

drivers-y += arch/pu32/drivers/

boot := arch/pu32/boot

vmlinux.bin: vmlinux
	$(Q)$(MAKE) $(build)=$(boot) $(boot)/$@

define archhelp
	echo '  vmlinux.bin	- Binary kernel image (arch/$(ARCH)/boot/vmlinux.bin)'
endef
