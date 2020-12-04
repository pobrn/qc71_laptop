// SPDX-License-Identifier: GPL-2.0
#include "pr.h"

#include <linux/device.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/init.h>
#include <linux/lockdep.h>
#include <linux/moduleparam.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>
#include <linux/types.h>

#include "util.h"
#include "ec.h"
#include "features.h"
#include "pdev.h"

/* ========================================================================== */

#if IS_ENABLED(CONFIG_HWMON)

#define HWMON_NAME KBUILD_MODNAME "_hwmon"

static const uint16_t qc71_fan_rpm_addrs[] = {
	FAN_RPM_1_ADDR,
	FAN_RPM_2_ADDR,
};

static const uint16_t qc71_fan_pwm_addrs[] = {
	FAN_PWM_1_ADDR,
	FAN_PWM_2_ADDR,
};

static const uint16_t qc71_fan_temp_addrs[] = {
	FAN_TEMP_1_ADDR,
	FAN_TEMP_2_ADDR,
};

/* ========================================================================== */

static bool nohwmon;
module_param(nohwmon, bool, 0444);
MODULE_PARM_DESC(nohwmon, "do not report to the hardware monitoring subsystem (default=false)");

static struct device *qc71_hwmon_dev;

static DEFINE_MUTEX(fan_lock);

/* ========================================================================== */

static int qc71_fan_get_rpm(uint8_t fan_index)
{
	union qc71_ec_result res;
	int err;

	if (fan_index >= ARRAY_SIZE(qc71_fan_rpm_addrs))
		return -EINVAL;

	err = qc71_ec_read(qc71_fan_rpm_addrs[fan_index], &res);

	if (err)
		return err;

	return res.bytes.b1 << 8 | res.bytes.b2;
}

static int qc71_fan_query_abnorm(void)
{
	int res = ec_read_byte(CTRL_1_ADDR);

	if (res < 0)
		return res;

	return !!(res & CTRL_1_FAN_ABNORMAL);
}

static int qc71_fan_get_status(void)
{
	return ec_read_byte(FAN_CTRL_ADDR);
}

static int qc71_fan_get_pwm(uint8_t fan_index)
{
	if (fan_index >= ARRAY_SIZE(qc71_fan_pwm_addrs))
		return -EINVAL;

	return ec_read_byte(qc71_fan_pwm_addrs[fan_index]);
}

static int qc71_fan_set_pwm(uint8_t fan_index, uint8_t pwm)
{
	if (!qc71_features.fan_boost)
		return -EOPNOTSUPP;

	if (fan_index >= ARRAY_SIZE(qc71_fan_pwm_addrs))
		return -EINVAL;

	if (pwm > FAN_MAX_PWM)
		return -EINVAL;

	return ec_write_byte(qc71_fan_pwm_addrs[fan_index], pwm);
}

static int qc71_fan_get_temp(uint8_t fan_index)
{
	if (fan_index >= ARRAY_SIZE(qc71_fan_temp_addrs))
		return -EINVAL;

	return ec_read_byte(qc71_fan_temp_addrs[fan_index]);
}

/* 'fan_lock' must be held */
static int qc71_fan_get_mode_core(void)
{
	int err;

	lockdep_assert_held(&fan_lock);

	err = ec_read_byte(CTRL_1_ADDR);
	if (err < 0)
		return err;

	if (err & CTRL_1_MANUAL_MODE) {
		err = qc71_fan_get_status();
		if (err < 0)
			return err;

		if (err & FAN_CTRL_FAN_BOOST) {
			err = qc71_fan_get_pwm(0);

			if (err < 0)
				return err;

			if (err == FAN_MAX_PWM)
				err = 0; /* disengaged */
			else
				err = 1; /* manual */

		} else if (err & FAN_CTRL_AUTO) {
			err = 2; /* automatic fan control */
		} else {
			err = 1; /* manual */
		}
	} else {
		err = 2; /* automatic fan control */
	}

	return err;
}

static int qc71_fan_get_mode(void)
{
	int err = mutex_lock_interruptible(&fan_lock);

	if (err)
		return err;

	err = qc71_fan_get_mode_core();

	mutex_unlock(&fan_lock);
	return err;
}

static int qc71_fan_set_mode(uint8_t mode)
{
	int err, oldpwm;

	if ((mode == 0 || mode == 1) && !qc71_features.fan_boost)
		return -EOPNOTSUPP;

	err = mutex_lock_interruptible(&fan_lock);
	if (err)
		return err;

	switch (mode) {
	case 0:
		err = ec_write_byte(FAN_CTRL_ADDR, FAN_CTRL_FAN_BOOST);
		if (err)
			goto out;

		err = qc71_fan_set_pwm(0, FAN_MAX_PWM);
		break;
	case 1:
		oldpwm = err = qc71_fan_get_pwm(0);
		if (err < 0)
			goto out;

		err = ec_write_byte(FAN_CTRL_ADDR, FAN_CTRL_FAN_BOOST);
		if (err < 0)
			goto out;

		err = qc71_fan_set_pwm(0, oldpwm);
		if (err < 0)
			(void) ec_write_byte(FAN_CTRL_ADDR, 0x80 | FAN_CTRL_AUTO);
			/* try to restore automatic fan control */

		break;
	case 2:
		err = ec_write_byte(FAN_CTRL_ADDR, 0x80 | FAN_CTRL_AUTO);
		break;
	default:
		err = -EINVAL;
		break;
	}

out:
	mutex_unlock(&fan_lock);
	return err;
}

/* ========================================================================== */

static umode_t qc71_hwmon_is_visible(const void *data, enum hwmon_sensor_types type,
				     u32 attr, int channel)
{
	switch (type) {
	case hwmon_fan:
		switch (attr) {
		case hwmon_fan_input:
		case hwmon_fan_fault:
			return 0444;
		default:
			break;
		}
		break;
	case hwmon_pwm:
		switch (attr) {
		case hwmon_pwm_enable:
			return 0644;
		default:
			break;
		}
		break;
	case hwmon_temp:
		switch (attr) {
		case hwmon_temp_input:
		case hwmon_temp_label:
			return 0444;
		default:
			break;
		}
	default:
		break;
	}

	return 0;
}

static int qc71_hwmon_read(struct device *device, enum hwmon_sensor_types type,
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
		case hwmon_fan_fault:
			err = qc71_fan_query_abnorm();
			if (err < 0)
				return err;

			*value = err;
			break;
		default:
			return -EOPNOTSUPP;
		}
		break;
	case hwmon_pwm:
		switch (attr) {
		case hwmon_pwm_enable:
			err = qc71_fan_get_mode();
			if (err < 0)
				return err;

			*value = err;
			break;
		}
		break;
	case hwmon_temp:
		switch (attr) {
		case hwmon_temp_input:
			switch (channel) {
			case 0:
			case 1:
				err = qc71_fan_get_temp(channel);
				if (err < 0)
					return err;
				*value = err * 1000;
				break;
			case 2:
				err = ec_read_byte(BATT_TEMP_ADDR);
				if (err < 0)
					return err;
				*value = err * 100;
				break;
			default:
				return -EOPNOTSUPP;
			}

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

static int qc71_hwmon_write(struct device *device, enum hwmon_sensor_types type,
			    u32 attr, int channel, long value)
{
	if (type != hwmon_pwm && attr != hwmon_pwm_enable)
		return -EOPNOTSUPP;

	return qc71_fan_set_mode(value);
}

static ssize_t fan_pwm_show(uint8_t fan_index, char *buf)
{
	int res = qc71_fan_get_pwm(fan_index);

	if (res < 0)
		return res;

	return sprintf(buf, "%d\n", linear_mapping(0, FAN_MAX_PWM, res, 0, U8_MAX));
}

static ssize_t fan_pwm_store(uint8_t fan_index, const char *buf, size_t count)
{
	unsigned int value;
	int err;

	if (kstrtouint(buf, 10, &value))
		return -EINVAL;

	if (value > U8_MAX)
		return -EINVAL;

	err = qc71_fan_set_pwm(fan_index, linear_mapping(0, U8_MAX, value, 0, FAN_MAX_PWM));
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

static int qc71_hwmon_read_string(struct device *dev, enum hwmon_sensor_types type,
				  u32 attr, int channel, const char **str)
{
	switch (type) {
	case hwmon_temp:
		switch (attr) {
		case hwmon_temp_label:
			switch (channel) {
			case 0:
				*str = "fan1_temp";
				break;
			case 1:
				*str = "fan2_temp";
				break;
			case 2:
				*str = "battery";
				break;
			default:
				return -EOPNOTSUPP;
			}
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

static const struct hwmon_channel_info *qc71_hwmon_ch_info[] = {
	HWMON_CHANNEL_INFO(fan,
			   HWMON_F_INPUT | HWMON_F_FAULT,
			   HWMON_F_INPUT | HWMON_F_FAULT),
	HWMON_CHANNEL_INFO(pwm,
			   HWMON_PWM_ENABLE),
	HWMON_CHANNEL_INFO(temp,
			   HWMON_T_INPUT | HWMON_T_LABEL,
			   HWMON_T_INPUT | HWMON_T_LABEL,
			   HWMON_T_INPUT | HWMON_T_LABEL),
	NULL
};

static struct hwmon_ops qc71_hwmon_ops = {
	.is_visible  = qc71_hwmon_is_visible,
	.read        = qc71_hwmon_read,
	.write       = qc71_hwmon_write,
	.read_string = qc71_hwmon_read_string,
};

static struct hwmon_chip_info qc71_hwmon_chip_info = {
	.ops  = &qc71_hwmon_ops,
	.info =  qc71_hwmon_ch_info,
};

static DEVICE_ATTR(pwm1, 0644, fan_pwm0_show, fan_pwm0_store);
static DEVICE_ATTR(pwm2, 0644, fan_pwm1_show, fan_pwm1_store);

static struct attribute *qc71_hwmon_attrs[] = {
	&dev_attr_pwm1.attr,
	&dev_attr_pwm2.attr,
	NULL
};

ATTRIBUTE_GROUPS(qc71_hwmon);

/* ========================================================================== */

int __init qc71_hwmon_setup(void)
{
	int err = 0;

	if (nohwmon)
		return 0;

	qc71_hwmon_dev = hwmon_device_register_with_info(&qc71_platform_dev->dev,
							 HWMON_NAME, NULL,
							 &qc71_hwmon_chip_info, qc71_hwmon_groups);

	if (IS_ERR(qc71_hwmon_dev))
		err = PTR_ERR(qc71_hwmon_dev);

	return err;
}

void qc71_hwmon_cleanup(void)
{
	if (!IS_ERR_OR_NULL(qc71_hwmon_dev))
		hwmon_device_unregister(qc71_hwmon_dev);
}

#endif
