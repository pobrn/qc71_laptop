// SPDX-License-Identifier: GPL-2.0
#include "pr.h"

#include <linux/device.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/init.h>
#include <linux/lockdep.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>
#include <linux/types.h>

#include "fan.h"
#include "features.h"
#include "pdev.h"
#include "util.h"

/* ========================================================================== */

static struct device *qc71_hwmon_pwm_dev;

/* ========================================================================== */

static umode_t qc71_hwmon_pwm_is_visible(const void *data, enum hwmon_sensor_types type,
					 u32 attr, int channel)
{
	if (type != hwmon_pwm && attr != hwmon_pwm_enable)
		return -EOPNOTSUPP;

	return 0644;
}

static int qc71_hwmon_pwm_read(struct device *device, enum hwmon_sensor_types type,
			       u32 attr, int channel, long *value)
{
	int err;

	switch (type) {
	case hwmon_pwm:
		switch (attr) {
		case hwmon_pwm_enable:
			err = qc71_fan_get_mode();
			if (err < 0)
				return err;

			*value = err;
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

static int qc71_hwmon_pwm_write(struct device *device, enum hwmon_sensor_types type,
			    u32 attr, int channel, long value)
{
	if (type != hwmon_pwm && attr != hwmon_pwm_enable)
		return -EOPNOTSUPP;

	return qc71_fan_set_mode(value);
}

static const struct hwmon_channel_info *qc71_hwmon_pwm_ch_info[] = {
	HWMON_CHANNEL_INFO(pwm,
			   HWMON_PWM_ENABLE),
	NULL
};

static struct hwmon_ops qc71_hwmon_pwm_ops = {
	.is_visible  = qc71_hwmon_pwm_is_visible,
	.read        = qc71_hwmon_pwm_read,
	.write       = qc71_hwmon_pwm_write,
};

static struct hwmon_chip_info qc71_hwmon_pwm_chip_info = {
	.ops  = &qc71_hwmon_pwm_ops,
	.info =  qc71_hwmon_pwm_ch_info,
};

/* ========================================================================== */

static ssize_t fan_pwm_show(uint8_t fan_index, char *buf)
{
	int res = qc71_fan_get_pwm(fan_index);

	if (res < 0)
		return res;

	return sprintf(buf, "%d\n", res);
}

static ssize_t fan_pwm_store(uint8_t fan_index, const char *buf, size_t count)
{
	unsigned int value;
	int err;

	if (kstrtouint(buf, 10, &value))
		return -EINVAL;

	if (value > U8_MAX)
		return -EINVAL;

	err = qc71_fan_set_pwm(fan_index, value);
	if (err)
		return err;

	return count;
}

#define FAN_PWM_SHOW_FUNC(fan_id) \
static ssize_t fan_pwm ## fan_id ## _show(struct device *dev, \
					  struct device_attribute *attr, char *buf) \
{ \
	return fan_pwm_show(fan_id, buf); \
}

#define FAN_PWM_STORE_FUNC(fan_id) \
static ssize_t fan_pwm ## fan_id ## _store(struct device *dev, struct device_attribute *attr, \
					   const char *buf, size_t count) \
{ \
	return fan_pwm_store(fan_id, buf, count); \
}

FAN_PWM_SHOW_FUNC(0)
FAN_PWM_SHOW_FUNC(1)
FAN_PWM_STORE_FUNC(0)
FAN_PWM_STORE_FUNC(1)

static DEVICE_ATTR(pwm1, 0644, fan_pwm0_show, fan_pwm0_store);
static DEVICE_ATTR(pwm2, 0644, fan_pwm1_show, fan_pwm1_store);

static struct attribute *qc71_hwmon_pwm_attrs[] = {
	&dev_attr_pwm1.attr,
	&dev_attr_pwm2.attr,
	NULL
};

ATTRIBUTE_GROUPS(qc71_hwmon_pwm);

/* ========================================================================== */

int __init qc71_hwmon_pwm_setup(void)
{
	int err = 0;

	if (!qc71_features.fan_boost)
		return -ENODEV;

	qc71_hwmon_pwm_dev = hwmon_device_register_with_info(
		&qc71_platform_dev->dev, KBUILD_MODNAME ".hwmon.pwm", NULL,
		&qc71_hwmon_pwm_chip_info, qc71_hwmon_pwm_groups);

	if (IS_ERR(qc71_hwmon_pwm_dev))
		err = PTR_ERR(qc71_hwmon_pwm_dev);

	return err;
}

void qc71_hwmon_pwm_cleanup(void)
{
	if (!IS_ERR_OR_NULL(qc71_hwmon_pwm_dev))
		hwmon_device_unregister(qc71_hwmon_pwm_dev);
}
