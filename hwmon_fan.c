// SPDX-License-Identifier: GPL-2.0
#include "pr.h"

#include <linux/device.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/init.h>
#include <linux/sysfs.h>
#include <linux/types.h>

#include "ec.h"
#include "fan.h"
#include "features.h"
#include "pdev.h"

/* ========================================================================== */

static struct device *qc71_hwmon_fan_dev;

/* ========================================================================== */

static umode_t qc71_hwmon_fan_is_visible(const void *data, enum hwmon_sensor_types type,
					 u32 attr, int channel)
{
	switch (type) {
	case hwmon_fan:
		switch (attr) {
		case hwmon_fan_input:
		case hwmon_fan_fault:
			return 0444;
		}
		break;
	case hwmon_temp:
		switch (attr) {
		case hwmon_temp_input:
		case hwmon_temp_label:
			return 0444;
		}
	default:
		break;
	}

	return 0;
}

static int qc71_hwmon_fan_read(struct device *device, enum hwmon_sensor_types type,
			       u32 attr, int channel, long *value)
{
	int err;

	switch (type) {
	case hwmon_fan:
		switch (attr) {
		case hwmon_fan_input:
			err = qc71_fan_get_rpm(channel);
			if (err < 0)
				return err;

			*value = err;
			break;
		default:
			return -EOPNOTSUPP;
		}
		break;
	case hwmon_temp:
		switch (attr) {
		case hwmon_temp_input:
			err = qc71_fan_get_temp(channel);
			if (err < 0)
				return err;

			*value = err * 1000;
			break;
		default:
			return -EOPNOTSUPP;
		}
		break;
	default:
		return -EOPNOTSUPP;
	}

	return 0;
}

static int qc71_hwmon_fan_read_string(struct device *dev, enum hwmon_sensor_types type,
				      u32 attr, int channel, const char **str)
{
	static const char * const temp_labels[] = {
		"fan1_temp",
		"fan2_temp",
	};

	switch (type) {
	case hwmon_temp:
		switch (attr) {
		case hwmon_temp_label:
			*str = temp_labels[channel];
			break;
		default:
			return -EOPNOTSUPP;
		}
		break;
	default:
		return -EOPNOTSUPP;
	}

	return 0;
}

/* ========================================================================== */

static const struct hwmon_channel_info *qc71_hwmon_fan_ch_info[] = {
	HWMON_CHANNEL_INFO(fan,
			   HWMON_F_INPUT,
			   HWMON_F_INPUT),
	HWMON_CHANNEL_INFO(temp,
			   HWMON_T_INPUT | HWMON_T_LABEL,
			   HWMON_T_INPUT | HWMON_T_LABEL),
	NULL
};

static const struct hwmon_ops qc71_hwmon_fan_ops = {
	.is_visible  = qc71_hwmon_fan_is_visible,
	.read        = qc71_hwmon_fan_read,
	.read_string = qc71_hwmon_fan_read_string,
};

static const struct hwmon_chip_info qc71_hwmon_fan_chip_info = {
	.ops  = &qc71_hwmon_fan_ops,
	.info =  qc71_hwmon_fan_ch_info,
};

/* ========================================================================== */

int __init qc71_hwmon_fan_setup(void)
{
	int err = 0;

	qc71_hwmon_fan_dev = hwmon_device_register_with_info(
		&qc71_platform_dev->dev, KBUILD_MODNAME ".hwmon.fan", NULL,
		&qc71_hwmon_fan_chip_info, NULL);

	if (IS_ERR(qc71_hwmon_fan_dev))
		err = PTR_ERR(qc71_hwmon_fan_dev);

	return err;
}

void qc71_hwmon_fan_cleanup(void)
{
	if (!IS_ERR_OR_NULL(qc71_hwmon_fan_dev))
		hwmon_device_unregister(qc71_hwmon_fan_dev);
}
