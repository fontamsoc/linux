extra-y := head.o vmlinux.lds

obj-y += console.o entry.o irq.o process.o ptrace.o reboot.o \
	setup.o signal.o syscall_table.o time.o topology.o
obj-y += tramp/

obj-$(CONFIG_PROC_FS) += proc.o
obj-$(CONFIG_SMP) += smp.o

obj-$(CONFIG_STACKTRACE) += stacktrace.o

#obj-y += entry.o atomic.o signal.o traps.o irq.o time.o vdso.o
#obj-y += power.o syscall.o syscall_table.o setup.o
#obj-y += process.o cpu-probe.o ptrace.o dumpstack.o

#obj-$(CONFIG_MODULES)			+= module.o
