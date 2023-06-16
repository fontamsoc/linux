extra-y := vmlinux.lds

obj-y += head.o entry.o console.o irq.o process.o ptrace.o reboot.o \
	setup.o signal.o syscall_table.o time.o topology.o
obj-y += tramp/

obj-$(CONFIG_PROC_FS) += proc.o
obj-$(CONFIG_SMP) += smp.o

obj-$(CONFIG_STACKTRACE) += stacktrace.o
