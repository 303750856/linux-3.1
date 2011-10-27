/*
 * processor_thermal.c - Passive cooling submodule of the ACPI processor driver
 *
 *  Copyright (C) 2001, 2002 Andy Grover <andrew.grover@intel.com>
 *  Copyright (C) 2001, 2002 Paul Diefenbaugh <paul.s.diefenbaugh@intel.com>
 *  Copyright (C) 2004       Dominik Brodowski <linux@brodo.de>
 *  Copyright (C) 2004  Anil S Keshavamurthy <anil.s.keshavamurthy@intel.com>
 *  			- Added processor hotplug support
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or (at
 *  your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/sysdev.h>

#include <asm/uaccess.h>

#include <acpi/acpi_bus.h>
#include <acpi/processor.h>
#include <acpi/acpi_drivers.h>

#define PREFIX "ACPI: "

#define ACPI_PROCESSOR_CLASS            "processor"
#define _COMPONENT              ACPI_PROCESSOR_COMPONENT
ACPI_MODULE_NAME("processor_thermal");

#ifdef CONFIG_CPU_FREQ

/* If a passive cooling situation is detected, primarily CPUfreq is used, as it
 * offers (in most cases) voltage scaling in addition to frequency scaling, and
 * thus a cubic (instead of linear) reduction of energy. Also, we allow for
 * _any_ cpufreq driver and not only the acpi-cpufreq driver.
 */


static DEFINE_PER_CPU(unsigned int, cpufreq_thermal_limit_state);
static unsigned int acpi_thermal_cpufreq_is_init = 0;

static int cpu_has_cpufreq(unsigned int cpu)
{
	struct cpufreq_policy policy;
	if (!acpi_thermal_cpufreq_is_init || cpufreq_get_policy(&policy, cpu))
		return 0;
	return 1;
}

static int acpi_thermal_cpufreq_notifier(struct notifier_block *nb,
					 unsigned long event, void *data)
{
	struct cpufreq_policy *policy = data;
	int state = per_cpu(cpufreq_thermal_limit_state, policy->cpu);
	struct cpufreq_frequency_table *table;

	if (event != CPUFREQ_ADJUST)
		return 0;

	table = cpufreq_frequency_get_table(policy->cpu);

	if (!table)
		return 0;

	cpufreq_verify_within_limits(policy, 0, table[state].frequency);

	return 0;
}

static struct notifier_block acpi_thermal_cpufreq_notifier_block = {
	.notifier_call = acpi_thermal_cpufreq_notifier,
};

static int cpufreq_get_max_state(unsigned int cpu)
{
	int count = 0;
	struct cpufreq_frequency_table *table;

	if (!cpu_has_cpufreq(cpu))
		return 0;

	table = cpufreq_frequency_get_table(cpu);

	if (!table)
		return 0;

	while (table[count].frequency != CPUFREQ_TABLE_END)
		count++;

	return count;
}

static int cpufreq_get_cur_state(unsigned int cpu)
{
	if (!cpu_has_cpufreq(cpu))
		return 0;

	return per_cpu(cpufreq_thermal_limit_state, cpu);
}

static int cpufreq_set_cur_state(unsigned int cpu, int state)
{
	if (!cpu_has_cpufreq(cpu))
		return 0;

	per_cpu(cpufreq_thermal_limit_state, cpu) = state;
	cpufreq_update_policy(cpu);
	return 0;
}

void acpi_thermal_cpufreq_init(void)
{
	int i;

	for (i = 0; i < nr_cpu_ids; i++)
		if (cpu_present(i))
			per_cpu(cpufreq_thermal_limit_state, i) = 0;

	i = cpufreq_register_notifier(&acpi_thermal_cpufreq_notifier_block,
				      CPUFREQ_POLICY_NOTIFIER);
	if (!i)
		acpi_thermal_cpufreq_is_init = 1;
}

void acpi_thermal_cpufreq_exit(void)
{
	if (acpi_thermal_cpufreq_is_init)
		cpufreq_unregister_notifier
		    (&acpi_thermal_cpufreq_notifier_block,
		     CPUFREQ_POLICY_NOTIFIER);

	acpi_thermal_cpufreq_is_init = 0;
}

#else				/* ! CONFIG_CPU_FREQ */
static int cpufreq_get_max_state(unsigned int cpu)
{
	return 0;
}

static int cpufreq_get_cur_state(unsigned int cpu)
{
	return 0;
}

static int cpufreq_set_cur_state(unsigned int cpu, int state)
{
	return 0;
}

#endif

int acpi_processor_get_limit_info(struct acpi_processor *pr)
{

	if (!pr)
		return -EINVAL;

	if (pr->flags.throttling)
		pr->flags.limit = 1;

	return 0;
}

/* thermal cooling device callbacks */
static int acpi_processor_max_state(struct acpi_processor *pr)
{
	int max_state = 0;

	max_state += cpufreq_get_max_state(pr->id);
	if (pr->flags.throttling)
		max_state += (pr->throttling.state_count -1);

	return max_state;
}
static int
processor_get_max_state(struct thermal_cooling_device *cdev,
			unsigned long *state)
{
	struct acpi_device *device = cdev->devdata;
	struct acpi_processor *pr = acpi_driver_data(device);

	if (!device || !pr)
		return -EINVAL;

	*state = acpi_processor_max_state(pr);
	return 0;
}

static int
processor_get_cur_state(struct thermal_cooling_device *cdev,
			unsigned long *cur_state)
{
	struct acpi_device *device = cdev->devdata;
	struct acpi_processor *pr = acpi_driver_data(device);

	if (!device || !pr)
		return -EINVAL;

	*cur_state = cpufreq_get_cur_state(pr->id);
	if (pr->flags.throttling)
		*cur_state += pr->throttling.state;
	return 0;
}

static int
processor_set_cur_state(struct thermal_cooling_device *cdev,
			unsigned long state)
{
	struct acpi_device *device = cdev->devdata;
	struct acpi_processor *pr = acpi_driver_data(device);
	int result = 0;
	int max_pstate;

	if (!device || !pr)
		return -EINVAL;

	max_pstate = cpufreq_get_max_state(pr->id);

	if (state > acpi_processor_max_state(pr))
		return -EINVAL;

	if (state <= max_pstate) {
		if (pr->flags.throttling && pr->throttling.state)
			result = acpi_processor_set_throttling(pr, 0, false);
		cpufreq_set_cur_state(pr->id, state);
	} else {
		cpufreq_set_cur_state(pr->id, max_pstate);
		result = acpi_processor_set_throttling(pr,
				state - max_pstate, false);
	}
	return result;
}

const struct thermal_cooling_device_ops processor_cooling_ops = {
	.get_max_state = processor_get_max_state,
	.get_cur_state = processor_get_cur_state,
	.set_cur_state = processor_set_cur_state,
};
