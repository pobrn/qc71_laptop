// SPDX-License-Identifier: GPL-2.0
#include "pr.h"

#include <linux/bug.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>

#include "util.h"
#include "ec.h"
#include "features.h"
#include "misc.h"
#include "pdev.h"

/* ========================================================================== */

struct platform_device *qc71_platform_dev;

/* ========================================================================== */

static ssize_t fan_reduced_duty_cycle_show(struct device *dev,
					   struct device_attribute *attr, char *buf)
{
	int status = ec_read_byte(BIOS_CTRL_3_ADDR);

	if (status < 0)
		return status;

	return sprintf(buf, "%d\n", (int) !!(status & BIOS_CTRL_3_FAN_REDUCED_DUTY_CYCLE));
}

static ssize_t fan_reduced_duty_cycle_store(struct device *dev, struct device_attribute *attr,
					    const char *buf, size_t count)
{
	int status;
	bool value;

	if (kstrtobool(buf, &value))
		return -EINVAL;

	status = ec_read_byte(BIOS_CTRL_3_ADDR);
	if (status < 0)
		return status;

	status = SET_BIT(status, BIOS_CTRL_3_FAN_REDUCED_DUTY_CYCLE, value);

	status = ec_write_byte(BIOS_CTRL_3_ADDR, status);

	if (status < 0)
		return status;

	return count;
}

static ssize_t fan_always_on_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	int status = ec_read_byte(BIOS_CTRL_3_ADDR);

	if (status < 0)
		return status;

	return sprintf(buf, "%d\n", (int) !!(status & BIOS_CTRL_3_FAN_ALWAYS_ON));
}

static ssize_t fan_always_on_store(struct device *dev, struct device_attribute *attr,
				   const char *buf, size_t count)
{
	int status;
	bool value;

	if (kstrtobool(buf, &value))
		return -EINVAL;

	status = ec_read_byte(BIOS_CTRL_3_ADDR);
	if (status < 0)
		return status;

	status = SET_BIT(status, BIOS_CTRL_3_FAN_ALWAYS_ON, value);

	status = ec_write_byte(BIOS_CTRL_3_ADDR, status);

	if (status < 0)
		return status;

	return count;
}

static ssize_t fn_lock_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	int status = qc71_fn_lock_get_state();

	if (status < 0)
		return status;

	return sprintf(buf, "%d\n", status);
}

static ssize_t fn_lock_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	int status;
	bool value;

	if (kstrtobool(buf, &value))
		return -EINVAL;

	status = qc71_fn_lock_set_state(value);
	if (status < 0)
		return status;

	return count;
}

static ssize_t fn_lock_switch_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int status = ec_read_byte(AP_BIOS_BYTE_ADDR);

	if (status < 0)
		return status;

	return sprintf(buf, "%d\n", (int) !!(status & AP_BIOS_BYTE_FN_LOCK_SWITCH));
}

static ssize_t fn_lock_switch_store(struct device *dev, struct device_attribute *attr,
				    const char *buf, size_t count)
{
	int status;
	bool value;

	if (kstrtobool(buf, &value))
		return -EINVAL;

	status = ec_read_byte(AP_BIOS_BYTE_ADDR);
	if (status < 0)
		return status;

	status = SET_BIT(status, AP_BIOS_BYTE_FN_LOCK_SWITCH, value);

	status = ec_write_byte(AP_BIOS_BYTE_ADDR, status);

	if (status < 0)
		return status;

	return count;
}

static ssize_t manual_control_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int status = ec_read_byte(CTRL_1_ADDR);

	if (status < 0)
		return status;

	return sprintf(buf, "%d\n", (int) !!(status & CTRL_1_MANUAL_MODE));
}

static ssize_t manual_control_store(struct device *dev, struct device_attribute *attr,
				    const char *buf, size_t count)
{
	int status;
	bool value;

	if (kstrtobool(buf, &value))
		return -EINVAL;

	status = ec_read_byte(CTRL_1_ADDR);
	if (status < 0)
		return status;

	status = SET_BIT(status, CTRL_1_MANUAL_MODE, value);

	status = ec_write_byte(CTRL_1_ADDR, status);

	if (status < 0)
		return status;

	return count;
}

static ssize_t super_key_lock_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	int status = ec_read_byte(STATUS_1_ADDR);

	if (status < 0)
		return status;

	return sprintf(buf, "%d\n", (int) !!(status & STATUS_1_SUPER_KEY_LOCK));
}

static ssize_t super_key_lock_store(struct device *dev, struct device_attribute *attr,
				    const char *buf, size_t count)
{
	int status;
	bool value;

	if (kstrtobool(buf, &value))
		return -EINVAL;

	status = ec_read_byte(STATUS_1_ADDR);
	if (status < 0)
		return status;

	if (value != !!(status & STATUS_1_SUPER_KEY_LOCK)) {
		status = ec_write_byte(TRIGGER_1_ADDR, TRIGGER_1_SUPER_KEY_LOCK);

		if (status < 0)
			return status;
	}

	return count;
}

/* ========================================================================== */

enum { DEV_ATTRS_START_LINE = __LINE__ };
static DEVICE_ATTR_RW(fn_lock);
static DEVICE_ATTR_RW(fn_lock_switch);
static DEVICE_ATTR_RW(fan_always_on);
static DEVICE_ATTR_RW(fan_reduced_duty_cycle);
static DEVICE_ATTR_RW(manual_control);
static DEVICE_ATTR_RW(super_key_lock);
enum { DEV_ATTRS_END_LINE = __LINE__ };

/* -1 is not needed because of the NULL terminator */
static struct attribute *qc71_laptop_attrs[DEV_ATTRS_END_LINE - DEV_ATTRS_START_LINE];
ATTRIBUTE_GROUPS(qc71_laptop);

/* ========================================================================== */

static int __init setup_sysfs_attrs(void)
{
	size_t idx = 0;

	qc71_laptop_attrs[idx++] = &dev_attr_manual_control.attr;

	if (qc71_features.super_key_lock)
		qc71_laptop_attrs[idx++] = &dev_attr_super_key_lock.attr;

	if (qc71_features.fn_lock) {
		qc71_laptop_attrs[idx++] = &dev_attr_fn_lock.attr;
		qc71_laptop_attrs[idx++] = &dev_attr_fn_lock_switch.attr;
	}

	if (qc71_features.fan_extras) {
		qc71_laptop_attrs[idx++] = &dev_attr_fan_reduced_duty_cycle.attr;
		qc71_laptop_attrs[idx++] = &dev_attr_fan_always_on.attr;
	}

	qc71_laptop_attrs[idx] = NULL;

	if (WARN(idx >= ARRAY_SIZE(qc71_laptop_attrs), "sysfs attribute array overflow\n"))
		return -EOVERFLOW;

	return 0;
}

/* ========================================================================== */

int __init qc71_pdev_setup(void)
{
	int err = 0;

	err = setup_sysfs_attrs();
	if (err)
		goto out;

	qc71_platform_dev = platform_device_alloc(KBUILD_MODNAME, PLATFORM_DEVID_NONE);
	if (!qc71_platform_dev) {
		err = -ENOMEM;
		goto out;
	}

	qc71_platform_dev->dev.groups = qc71_laptop_groups;

	err = platform_device_add(qc71_platform_dev);
	if (err) {
		platform_device_put(qc71_platform_dev);
		qc71_platform_dev = NULL;
	}

out:
	return err;
}

void qc71_pdev_cleanup(void)
{
	/* checks for IS_ERR_OR_NULL() */
	platform_device_unregister(qc71_platform_dev);
}
