// SPDX-License-Identifier: GPL-2.0
#include "pr.h"

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/power_supply.h>
#include <acpi/battery.h>
#include <linux/sysfs.h>
#include <linux/types.h>

#include "ec.h"
#include "features.h"

/* ========================================================================== */

#if IS_ENABLED(CONFIG_ACPI_BATTERY)

static bool battery_hook_registered;

static bool nobattery;
module_param(nobattery, bool, 0444);
MODULE_PARM_DESC(nobattery, "do not expose battery related controls (default=false)");

/* ========================================================================== */

static ssize_t charge_control_end_threshold_show(struct device *dev,
						 struct device_attribute *attr, char *buf)
{
	int status = ec_read_byte(BATT_CHARGE_CTRL_ADDR);

	if (status < 0)
		return status;

	status &= BATT_CHARGE_CTRL_VALUE_MASK;

	if (status == 0)
		status = 100;

	return sprintf(buf, "%d\n", status);
}

static ssize_t charge_control_end_threshold_store(struct device *dev, struct device_attribute *attr,
						  const char *buf, size_t count)
{
	int status, value;

	if (kstrtoint(buf, 10, &value) || !(1 <= value && value <= 100))
		return -EINVAL;

	status = ec_read_byte(BATT_CHARGE_CTRL_ADDR);
	if (status < 0)
		return status;

	if (value == 100)
		value = 0;

	status = (status & ~BATT_CHARGE_CTRL_VALUE_MASK) | value;

	status = ec_write_byte(BATT_CHARGE_CTRL_ADDR, status);

	if (status < 0)
		return status;

	return count;
}

static DEVICE_ATTR_RW(charge_control_end_threshold);
static struct attribute *qc71_laptop_batt_attrs[] = {
	&dev_attr_charge_control_end_threshold.attr,
	NULL
};
ATTRIBUTE_GROUPS(qc71_laptop_batt);

static int qc71_laptop_batt_add(struct power_supply *battery)
{
	if (strcmp(battery->desc->name, "BAT0") != 0)
		return 0;

	return device_add_groups(&battery->dev, qc71_laptop_batt_groups);
}

static int qc71_laptop_batt_remove(struct power_supply *battery)
{
	if (strcmp(battery->desc->name, "BAT0") != 0)
		return 0;

	device_remove_groups(&battery->dev, qc71_laptop_batt_groups);
	return 0;
}

static struct acpi_battery_hook qc71_laptop_batt_hook = {
	.add_battery    = qc71_laptop_batt_add,
	.remove_battery = qc71_laptop_batt_remove,
	.name           = "QC71 laptop battery extension",
};

int __init qc71_battery_setup(void)
{
	if (nobattery || !qc71_features.batt_charge_limit)
		return -ENODEV;

	battery_hook_register(&qc71_laptop_batt_hook);
	battery_hook_registered = true;

	return 0;
}

void qc71_battery_cleanup(void)
{
	if (battery_hook_registered)
		battery_hook_unregister(&qc71_laptop_batt_hook);
}

#endif
