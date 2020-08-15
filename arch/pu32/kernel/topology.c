// SPDX-License-Identifier: GPL-2.0-only
// (c) William Fonkou Tambe

#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/init.h>
#include <linux/node.h>
#include <linux/nodemask.h>

/* Register cpus in sysfs.
   Ideally we could rely on CONFIG_GENERIC_CPU_DEVICES to register cpus.
   Unfortunately this option registers cpus before topology_init() is called,
   and crashes the system. Nodes have to be register before cpus. */

static struct cpu cpu_devices[NR_CPUS];

static int __init topology_init (void) {

	int i, err = 0;

	for_each_present_cpu(i) {
		struct cpu *cpu = &cpu_devices[i];
		#ifdef CONFIG_HOTPLUG_CPU
		cpu->hotpluggable = 1;
		#endif
		if ((err = register_cpu(cpu, i))) {
			pr_warn ("%s: register_cpu(%d) == %d\n",
				__FUNCTION__, i, err);
			goto out;
		}
	}

out:
	return err;
}

subsys_initcall(topology_init);
