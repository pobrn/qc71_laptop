// SPDX-License-Identifier: GPL-2.0
#include "pr.h"

#include <linux/version.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 12, 0)
static inline int fixp_linear_interpolate(int x0, int y0, int x1, int y1, int x)
{
	if (y0 == y1 || x == x0)
		return y0;
	if (x1 == x0 || x == x1)
		return y1;

	return y0 + ((y1 - y0) * (x - x0) / (x1 - x0));
}
#else
#include <linux/bug.h> /* fixp-arith.h needs it, but doesn't include it */
#include <linux/fixp-arith.h>
#endif

#include <linux/lockdep.h>
#include <linux/mutex.h>
#include <linux/types.h>

#include "ec.h"
#include "fan.h"
#include "util.h"

/* ========================================================================== */

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

static DEFINE_MUTEX(fan_lock);

/* ========================================================================== */

static int qc71_fan_get_status(void)
{
	return ec_read_byte(FAN_CTRL_ADDR);
}

/* 'fan_lock' must be held */
static int qc71_fan_get_mode_unlocked(void)
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

/* ========================================================================== */

int qc71_fan_get_rpm(uint8_t fan_index)
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

int qc71_fan_query_abnorm(void)
{
	int res = ec_read_byte(CTRL_1_ADDR);

	if (res < 0)
		return res;

	return !!(res & CTRL_1_FAN_ABNORMAL);
}

int qc71_fan_get_pwm(uint8_t fan_index)
{
	int err;

	if (fan_index >= ARRAY_SIZE(qc71_fan_pwm_addrs))
		return -EINVAL;

	err = ec_read_byte(qc71_fan_pwm_addrs[fan_index]);
	if (err < 0)
		return err;

	return fixp_linear_interpolate(0, 0, FAN_MAX_PWM, U8_MAX, err);
}

int qc71_fan_set_pwm(uint8_t fan_index, uint8_t pwm)
{
	if (fan_index >= ARRAY_SIZE(qc71_fan_pwm_addrs))
		return -EINVAL;

	return ec_write_byte(qc71_fan_pwm_addrs[fan_index],
			     fixp_linear_interpolate(0, 0,
						     U8_MAX, FAN_MAX_PWM,
						     pwm));
}

int qc71_fan_get_temp(uint8_t fan_index)
{
	if (fan_index >= ARRAY_SIZE(qc71_fan_temp_addrs))
		return -EINVAL;

	return ec_read_byte(qc71_fan_temp_addrs[fan_index]);
}

int qc71_fan_get_mode(void)
{
	int err = mutex_lock_interruptible(&fan_lock);

	if (err)
		return err;

	err = qc71_fan_get_mode_unlocked();

	mutex_unlock(&fan_lock);
	return err;
}

int qc71_fan_set_mode(uint8_t mode)
{
	int err, oldpwm;

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
