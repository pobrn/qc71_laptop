// SPDX-License-Identifier: GPL-2.0
#include "pr.h"

#include <linux/init.h>
#include <linux/leds.h>
#include <linux/moduleparam.h>
#include <linux/types.h>

#include "util.h"
#include "ec.h"
#include "features.h"
#include "led_lightbar.h"
#include "pdev.h"

/* ========================================================================== */

#if IS_ENABLED(CONFIG_LEDS_CLASS)

enum qc71_lightbar_color {
	LIGHTBAR_RED         = 0,
	LIGHTBAR_GREEN       = 1,
	LIGHTBAR_BLUE        = 2,
	LIGHTBAR_COLOR_COUNT
};

static const uint16_t lightbar_color_addrs[LIGHTBAR_COLOR_COUNT] = {
	[LIGHTBAR_RED]   = LIGHTBAR_RED_ADDR,
	[LIGHTBAR_GREEN] = LIGHTBAR_GREEN_ADDR,
	[LIGHTBAR_BLUE]  = LIGHTBAR_BLUE_ADDR,
};

static const uint8_t lightbar_colors[] = {
	LIGHTBAR_RED,
	LIGHTBAR_GREEN,
	LIGHTBAR_BLUE,
};

/* f(x) = 4x */
static const uint8_t lightbar_color_values[][10] = {
	[LIGHTBAR_RED]   = {0, 4, 8, 12, 16, 20, 24, 28, 32, 36},
	[LIGHTBAR_GREEN] = {0, 4, 8, 12, 16, 20, 24, 28, 32, 36},
	[LIGHTBAR_BLUE]  = {0, 4, 8, 12, 16, 20, 24, 28, 32, 36},
};

/* inverse of 'lightbar_color_values' */
static const uint8_t lightbar_pwm_to_level[][256] = {
	[LIGHTBAR_RED] = {
		 [0] = 0,
		 [4] = 1,
		 [8] = 2,
		[12] = 3,
		[16] = 4,
		[20] = 5,
		[24] = 6,
		[28] = 7,
		[32] = 8,
		[36] = 9,
	},

	[LIGHTBAR_GREEN] = {
		 [0] = 0,
		 [4] = 1,
		 [8] = 2,
		[12] = 3,
		[16] = 4,
		[20] = 5,
		[24] = 6,
		[28] = 7,
		[32] = 8,
		[36] = 9,
	},

	[LIGHTBAR_BLUE] = {
		 [0] = 0,
		 [4] = 1,
		 [8] = 2,
		[12] = 3,
		[16] = 4,
		[20] = 5,
		[24] = 6,
		[28] = 7,
		[32] = 8,
		[36] = 9,
	},
};


/* ========================================================================== */

static bool nolightbar;
module_param(nolightbar, bool, 0444);
MODULE_PARM_DESC(nolightbar, "do not register the lightbar to the leds subsystem (default=false)");

static bool lightbar_led_registered;

/* ========================================================================== */

static inline int qc71_lightbar_get_status(void)
{
	return ec_read_byte(LIGHTBAR_CTRL_ADDR);
}

static inline int qc71_lightbar_write_ctrl(uint8_t ctrl)
{
	return ec_write_byte(LIGHTBAR_CTRL_ADDR, ctrl);
}

/* ========================================================================== */

static int qc71_lightbar_switch(uint8_t mask, bool on)
{
	int status;

	if (mask != LIGHTBAR_CTRL_S0_OFF && mask != LIGHTBAR_CTRL_S3_OFF)
		return -EINVAL;

	status = qc71_lightbar_get_status();

	if (status < 0)
		return status;

	return qc71_lightbar_write_ctrl(SET_BIT(status, mask, !on));
}

static int qc71_lightbar_set_color_level(uint8_t color, uint8_t level)
{
	if (color >= ARRAY_SIZE(lightbar_color_addrs))
		return -EINVAL;

	if (level >= ARRAY_SIZE(lightbar_color_values[color]))
		return -EINVAL;

	return ec_write_byte(lightbar_color_addrs[color], lightbar_color_values[color][level]);
}

static int qc71_lightbar_get_color_level(uint8_t color)
{
	int err;

	if (color >= ARRAY_SIZE(lightbar_color_addrs))
		return -EINVAL;

	err = ec_read_byte(lightbar_color_addrs[color]);
	if (err < 0)
		return err;

	return lightbar_pwm_to_level[color][err];
}

static int qc71_lightbar_set_rainbow_mode(bool on)
{
	int status = qc71_lightbar_get_status();

	if (status < 0)
		return status;

	return qc71_lightbar_write_ctrl(SET_BIT(status, LIGHTBAR_CTRL_RAINBOW, on));
}

static int qc71_lightbar_set_color(unsigned int color)
{
	int err = 0, i;

	if (color > 999) /* color must lie in [0, 999] */
		return -EINVAL;

	for (i = ARRAY_SIZE(lightbar_colors) - 1; i >= 0 && !err; i--)
		err = qc71_lightbar_set_color_level(lightbar_colors[i],
						    do_div(color, 10));

	return err;
}

/* ========================================================================== */
/* lightbar attrs */

static ssize_t lightbar_s3_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int value = qc71_lightbar_get_status();

	if (value < 0)
		return value;

	return sprintf(buf, "%d\n", (int) !(value & LIGHTBAR_CTRL_S3_OFF));
}

static ssize_t lightbar_s3_store(struct device *dev, struct device_attribute *attr,
				 const char *buf, size_t count)
{
	int err;
	bool value;

	if (kstrtobool(buf, &value))
		return -EINVAL;

	err = qc71_lightbar_switch(LIGHTBAR_CTRL_S3_OFF, value);

	if (err)
		return err;

	return count;
}

static ssize_t lightbar_color_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	unsigned int color = 0;
	size_t i;

	for (i = 0; i < ARRAY_SIZE(lightbar_colors); i++) {
		int level = qc71_lightbar_get_color_level(lightbar_colors[i]);

		if (level < 0)
			return level;

		color *= 10;

		if (0 <= level && level <= 9)
			color += level;
	}

	return sprintf(buf, "%03u\n", color);
}

static ssize_t lightbar_color_store(struct device *dev, struct device_attribute *attr,
				    const char *buf, size_t count)
{
	unsigned int value;
	int err;

	if (kstrtouint(buf, 10, &value))
		return -EINVAL;

	err = qc71_lightbar_set_color(value);
	if (err)
		return err;

	return count;
}

static ssize_t lightbar_rainbow_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	int status = qc71_lightbar_get_status();

	if (status < 0)
		return status;

	return sprintf(buf, "%d\n", (int) !!(status & LIGHTBAR_CTRL_RAINBOW));
}

static ssize_t lightbar_rainbow_store(struct device *dev, struct device_attribute *attr,
				      const char *buf, size_t count)
{
	int err;
	bool value;

	if (kstrtobool(buf, &value))
		return -EINVAL;

	err = qc71_lightbar_set_rainbow_mode(value);

	if (err)
		return err;

	return count;
}

static enum led_brightness qc71_lightbar_led_get_brightness(struct led_classdev *led_cdev)
{
	int err = qc71_lightbar_get_status();

	if (err)
		return 0;

	return !(err & LIGHTBAR_CTRL_S0_OFF);
}

static int qc71_lightbar_led_set_brightness(struct led_classdev *led_cdev,
					    enum led_brightness value)
{
	return qc71_lightbar_switch(LIGHTBAR_CTRL_S0_OFF, !!value);
}

/* ========================================================================== */

static DEVICE_ATTR(brightness_s3, 0644, lightbar_s3_show,      lightbar_s3_store);
static DEVICE_ATTR(color,         0644, lightbar_color_show,   lightbar_color_store);
static DEVICE_ATTR(rainbow_mode,  0644, lightbar_rainbow_show, lightbar_rainbow_store);

static struct attribute *qc71_lightbar_led_attrs[] = {
	&dev_attr_brightness_s3.attr,
	&dev_attr_color.attr,
	&dev_attr_rainbow_mode.attr,
	NULL
};

ATTRIBUTE_GROUPS(qc71_lightbar_led);

static struct led_classdev qc71_lightbar_led = {
	.name                    = KBUILD_MODNAME "::lightbar",
	.max_brightness          = 1,
	.brightness_get          = qc71_lightbar_led_get_brightness,
	.brightness_set_blocking = qc71_lightbar_led_set_brightness,
	.groups                  = qc71_lightbar_led_groups,
};

/* ========================================================================== */

int __init qc71_led_lightbar_setup(void)
{
	int err;

	if (nolightbar)
		return -EPERM;

	if (!qc71_features.lightbar)
		return -ENOTSUPP;

	err = led_classdev_register(&qc71_platform_dev->dev, &qc71_lightbar_led);

	if (!err)
		lightbar_led_registered = true;

	return err;
}

void qc71_led_lightbar_cleanup(void)
{
	if (lightbar_led_registered)
		led_classdev_unregister(&qc71_lightbar_led);
}

#endif
