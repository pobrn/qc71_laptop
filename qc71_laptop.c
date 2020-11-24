// SPDX-License-Identifier: GPL-2.0
/* ========================================================================== */
/* https://www.intel.com/content/dam/support/us/en/documents/laptops/whitebook/QC71_PROD_SPEC.pdf
 *
 *
 * based on the following resources:
 *  - https://lwn.net/Articles/391230/
 *  - http://blog.nietrzeba.pl/2011/12/mof-decompilation.html
 *  - https://github.com/tuxedocomputers/tuxedo-cc-wmi/
 *  - https://github.com/tuxedocomputers/tuxedo-keyboard/
 *  - Control Center for Microsoft Windows
 *  - http://forum.notebookreview.com/threads/tongfang-gk7cn6s-gk7cp0s-gk7cp7s.825461/page-54
 */
/* ========================================================================== */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <dt-bindings/leds/common.h>
#include <linux/power_supply.h>
#include <acpi/battery.h>
#include <linux/acpi.h>
#include <linux/bits.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dmi.h>
#include <linux/error-injection.h>
#include <linux/hwmon.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/input/sparse-keymap.h>
#include <linux/kconfig.h>
#include <linux/leds.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/rwsem.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/wmi.h>

ACPI_MODULE_NAME(KBUILD_MODNAME)

#define KBD_BL_LED_SUFFIX ":" LED_FUNCTION_KBD_BACKLIGHT

/* ========================================================================== */

#define DRIVERNAME KBUILD_MODNAME
#define HWMON_NAME DRIVERNAME "_hwmon"
#define SET_BIT(value, bit, on) ((on) ? ((value) | (bit)) : ((value) & ~(bit)))

/* ========================================================================== */
/* WMI methods */

/* AcpiTest_MULong */
#define QC71_WMI_WMBC_GUID       "ABBC0F6F-8EA1-11D1-00A0-C90629100000"
#define QC71_WMBC_GETSETULONG_ID 4

/* ========================================================================== */
/* WMI events */

/* AcpiTest_EventULong */
#define QC71_WMI_EVENT0_GUID     "ABBC0F72-8EA1-11D1-00A0-C90629100000"

/* AcpiTest_EventString */
#define QC71_WMI_EVENT1_GUID     "ABBC0F71-8EA1-11D1-00A0-C90629100000"

/* AcpiTest_EventPackage */
#define QC71_WMI_EVENT2_GUID     "ABBC0F70-8EA1-11D1-00A0-C90629100000"

static const char * const qc71_wmi_event_guids[] = {
	QC71_WMI_EVENT0_GUID,
	QC71_WMI_EVENT1_GUID,
	QC71_WMI_EVENT2_GUID,
};

/* ========================================================================== */

#define ADDR(page, offset)       (((uint16_t)(page) << 8) | ((uint16_t)(offset)))

enum qc71_lightbar_color {
	LIGHTBAR_RED   = 0,
	LIGHTBAR_GREEN = 1,
	LIGHTBAR_BLUE  = 2,
};

/* lightbar control register */
#define LIGHTBAR_CONTROL_ADDR   ADDR(0x07, 0x48)
#define LIGHTBAR_CTRL_S0_OFF    BIT(2)
#define LIGHTBAR_CTRL_S3_OFF    BIT(3)
#define LIGHTBAR_CTRL_RAINBOW   BIT(7)

#if defined(CONFIG_LEDS_CLASS) || defined(CONFIG_DEBUG_FS)
static const uint16_t lightbar_color_addrs[] = {
	[LIGHTBAR_RED]   = ADDR(0x07, 0x49),
	[LIGHTBAR_GREEN] = ADDR(0x07, 0x4A),
	[LIGHTBAR_BLUE]  = ADDR(0x07, 0x4B),
};
#endif

#if IS_ENABLED(CONFIG_LEDS_CLASS)

static const uint8_t lightbar_colors[] = {
	LIGHTBAR_RED,
	LIGHTBAR_GREEN,
	LIGHTBAR_BLUE,
};

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

#endif

#define HWMON_EXTRA

/*
 * EC register addresses and bitmasks,
 * some of them are not used,
 * only for documentation
 */

#define PROJ_ID_ADDR ADDR(0x07, 0x40)

/* fan control register */
#define FAN_CTRL_ADDR         ADDR(0x07, 0x51)
#define FAN_CTRL_LEVEL_MASK   GENMASK(2, 0)
#define FAN_CTRL_MAX_LEVEL    7
#define FAN_CTRL_TURBO        BIT(4)
#define FAN_CTRL_AUTO         BIT(5)
#define FAN_CTRL_FAN_BOOST    BIT(6)
#define FAN_CTRL_LEVEL(level) (128 + (level))

static const uint16_t qc71_fan_rpm_addrs[] = {
	ADDR(0x04, 0x64),
	ADDR(0x04, 0x6C),
};

#define FAN_PWM_1_ADDR ADDR(0x18, 0x04)
#define FAN_PWM_2_ADDR ADDR(0x18, 0x09)

static const uint16_t qc71_fan_pwm_addrs[] = {
	FAN_PWM_1_ADDR,
	FAN_PWM_2_ADDR,
};

#define FAN_MAX_PWM 200

static const uint16_t qc71_fan_temp_addrs[] = {
	ADDR(0x04, 0x3e),
	ADDR(0x04, 0x4f),
};

#define FAN_MODE_INDEX_ADDR ADDR(0x07, 0xAB)
#define FAN_MODE_INDEX_LOW_MASK GENMASK(3, 0)
#define FAN_MODE_INDEX_HIGH_MASK GENMASK(7, 4)

/* 1st control register */
#define CTRL_1_ADDR         ADDR(0x07, 0x41)
#define CTRL_1_MANUAL_MODE  BIT(0)
#define CTRL_1_FAN_ABNORMAL BIT(5)

/* 2nd control register */
#define CTRL_2_ADDR ADDR(0x07, 0x8C)

/* 3rd control register */
#define CTRL_3_ADDR         ADDR(0x07, 0xA5)
#define CTRL_3_PWR_LED_MASK GENMASK(1, 0)
#define CTRL_3_PWR_LED_NONE BIT(1)
#define CTRL_3_PWR_LED_BOTH BIT(0)
#define CTRL_3_PWR_LED_LEFT 0x00
#define CTRL_3_FAN_QUIET    BIT(2)
#define CTRL_3_OVERBOOST    BIT(4)
#define CTRL_3_HIGH_PWR     BIT(7)

/* 4th control register*/
#define CTRL_4_ADDR                   ADDR(0x07, 0xA6)
#define CTRL_4_OVERBOOST_DYN_TEMP_OFF BIT(1)

#define SUPPORT_1_ADDR           ADDR(0x07, 0x65)
#define SUPPORT_1_SUPER_KEY_LOCK BIT(5)
#define SUPPORT_1_LIGHTBAR       BIT(6)
#define SUPPORT_1_FAN_BOOST      BIT(7)

#define SUPPORT_2_ADDR      ADDR(0x07, 0x66)

#define SUPPORT_5_ADDR      ADDR(0x07, 0x42)
#define SUPPORT_5_FAN       BIT(5)

#define STATUS_1_ADDR           ADDR(0x07, 0x68)
#define STATUS_1_SUPER_KEY_LOCK BIT(0)

#define TRIGGER_1_ADDR           ADDR(0x07, 0x67)
#define TRIGGER_1_SUPER_KEY_LOCK BIT(0)

#define TRIGGER_2_ADDR ADDR(0x07, 0x5D)

#define BIOS_INFO_1_ADDR ADDR(0x04, 0x9F)
#define BIOS_INFO_5_ADDR ADDR(0x04, 0x66)

#define POWER_STATUS_ADDR ADDR(0x04, 0x5E)
#define POWER_SOURCE_ADDR ADDR(0x04, 0x90)
#define PLATFORM_ID_ADDR  ADDR(0x04, 0x56)

#define AP_BIOS_BYTE_ADDR ADDR(0x07, 0xA4)
#define AP_BIOS_BYTE_FN_LOCK_SWITCH BIT(3)

/* battery charger control register */
#define BATT_CHARGE_CTRL_ADDR ADDR(0x07, 0xB9)
#define BATT_CHARGE_CTRL_VALUE_MASK GENMASK(6, 0)
#define BATT_CHARGE_CTRL_REACHED BIT(7)

#define BATT_STATUS_ADDR        ADDR(0x04, 0x32)
#define BATT_STATUS_DISCHARDING BIT(0)

/* possibly temp (in C) = value / 10 + X */
#define BATT_TEMP_ADDR ADDR(0x04, 0xA2)

#define BATT_ALERT_ADDR ADDR(0x04, 0x94)

#define BIOS_CTRL_1_ADDR           ADDR(0x07, 0x4E)
#define BIOS_CTRL_1_FN_LOCK_STATUS BIT(4)

#define BIOS_CTRL_2_ADDR       ADDR(0x07, 0x82)
#define BIOS_CTRL_2_FAN_V2_NEW BIT(0)
#define BIOS_CTRL_2_FAN_QKEY   BIT(1)
#define BIOS_CTRL_2_FAN_V3     BIT(3)

/* 3rd control register of a different kind */
#define BIOS_CTRL_3_ADDR                   ADDR(0x7, 0xA3)
#define BIOS_CTRL_3_FAN_REDUCED_DUTY_CYCLE BIT(5)
#define BIOS_CTRL_3_FAN_ALWAYS_ON          BIT(6)

#define PL1_ADDR ADDR(0x07, 0x83)
#define PL2_ADDR ADDR(0x07, 0x84)
#define PL4_ADDR ADDR(0x07, 0x85)

#define DEVICE_STATUS_ADDR  ADDR(0x04, 0x7B)
#define DEVICE_STATUS_WIFI_ON BIT(7)
/*
 * BIT(5) is also set/unset depending on the rfkill state,
 * that's possibly for Bluetooth
 */

union qc71_ec_result {
	uint32_t dword;
	struct {
		uint16_t w1;
		uint16_t w2;
	} words;
	struct {
		uint8_t b1;
		uint8_t b2;
		uint8_t b3;
		uint8_t b4;
	} bytes;
};

static struct platform_device *qc71_platform_dev;

static struct input_dev *qc71_input_dev;

#if IS_ENABLED(CONFIG_HWMON)
static struct device *qc71_hwmon_dev;
#endif

struct {
	bool super_key_lock;
	bool lightbar;
	bool fan_boost;
	bool fn_lock;
	bool batt_charge_limit;
	bool fan_extras; /* duty cycle reduction, always on mode */
} features;

static DEFINE_MUTEX(ec_lock);
static DEFINE_MUTEX(fan_lock);

/* data for 'do_cleanup()' */
#if IS_ENABLED(CONFIG_ACPI_BATTERY)
static bool battery_hook_registered;
#endif

#if IS_ENABLED(CONFIG_LEDS_CLASS)
static bool lightbar_led_registered;
#endif

static int wmi_handlers_installed;

#if IS_ENABLED(CONFIG_DEBUG_FS)
static struct dentry *qc71_debugfs_dir;
#endif

/* ========================================================================== */
/* module parameters */

#if IS_ENABLED(CONFIG_LEDS_CLASS)
static bool nolightbar;
module_param(nolightbar, bool, 0444);
MODULE_PARM_DESC(nolightbar, "do not register the lightbar to the leds subsystem (default=false)");
#endif

#if IS_ENABLED(CONFIG_HWMON)
static bool nohwmon;
module_param(nohwmon, bool, 0444);
MODULE_PARM_DESC(nohwmon, "do not report to the hardware monitoring subsystem (default=false)");
#endif

#if IS_ENABLED(CONFIG_ACPI_BATTERY)
static bool nobattery;
module_param(nobattery, bool, 0444);
MODULE_PARM_DESC(nobattery, "do not expose battery related controls (default=false)");
#endif

#if IS_ENABLED(CONFIG_DEBUG_FS)
static bool debugregs;
module_param(debugregs, bool, 0444);
MODULE_PARM_DESC(debugregs, "expose various EC registers in debugfs (default=false)");
#endif

/* ========================================================================== */
/* EC access */

static int __must_check qc71_ec_transaction(uint16_t addr, uint16_t data,
					    union qc71_ec_result *result, bool read)
{
	uint8_t buf[] = {
		addr & 0xFF,
		addr >> 8,
		data & 0xFF,
		data >> 8,
		0,
		read ? 1 : 0,
		0,
		0,
	};
	static_assert(ARRAY_SIZE(buf) == 8);

	/* the returned ACPI_TYPE_BUFFER is 40 bytes long for some reason ... */
	uint8_t outbuf_buf[sizeof(union acpi_object) + 40];

	struct acpi_buffer input = { sizeof(buf), buf },
			   output = { sizeof(outbuf_buf), outbuf_buf };
	union acpi_object *obj;
	acpi_status status = AE_OK;

	int err = mutex_lock_interruptible(&ec_lock);

	if (err)
		goto out;

	status = wmi_evaluate_method(QC71_WMI_WMBC_GUID, 0,
				     QC71_WMBC_GETSETULONG_ID, &input, &output);

	mutex_unlock(&ec_lock);

	if (ACPI_FAILURE(status)) {
		err = -EIO;
		goto out;
	}

	obj = output.pointer;

	if (result) {
		if (obj && obj->type == ACPI_TYPE_BUFFER && obj->buffer.length >= sizeof(*result)) {
			memcpy(result, obj->buffer.pointer, sizeof(*result));
		} else {
			err = -ENODATA;
			goto out;
		}
	}

out:
	pr_debug("%s(addr=%#06x, data=%#06x, result=%s, read=%s): (%d) [%#010lx] %s\n",
		 __func__, (unsigned int) addr, (unsigned int) data,
		 result ? "wants" : "NULL", read ? "yes" : "no", err,
		 (unsigned long) status, acpi_format_exception(status));

	return err;
}
ALLOW_ERROR_INJECTION(qc71_ec_transaction, ERRNO);

static inline int __must_check qc71_ec_read(uint16_t addr, union qc71_ec_result *result)
{
	return qc71_ec_transaction(addr, 0, result, true);
}

static inline int __must_check qc71_ec_write(uint16_t addr, uint16_t data)
{
	return qc71_ec_transaction(addr, data, NULL, false);
}

static inline int __must_check ec_write_byte(uint16_t addr, uint8_t data)
{
	return qc71_ec_write(addr, data);
}

static int __must_check ec_read_byte(uint16_t addr)
{
	union qc71_ec_result result;
	int err;

	err = qc71_ec_read(addr, &result);

	if (err)
		return err;

	pr_debug("%s(addr=%#06x): %#04x %#04x %#04x %#04x\n",
		 __func__, (unsigned int) addr, result.bytes.b1,
		 result.bytes.b2, result.bytes.b3, result.bytes.b4);

	return result.bytes.b1;
}

/* ========================================================================== */
/* rfkill */

static int qc71_rfkill_get_state(void)
{
	int err = ec_read_byte(DEVICE_STATUS_ADDR);

	if (err < 0)
		return err;

	return !!(err & DEVICE_STATUS_WIFI_ON);
}

/* ========================================================================== */
/* fan */

#if IS_ENABLED(CONFIG_HWMON)

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
	int err = ec_read_byte(CTRL_1_ADDR);

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
	int err = mutex_lock_interruptible(&fan_lock), oldpwm;

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

#endif

/* ========================================================================== */
/* lightbar */

#if IS_ENABLED(CONFIG_LEDS_CLASS)

static int qc71_lightbar_get_status(void)
{
	return ec_read_byte(LIGHTBAR_CONTROL_ADDR);
}

static int qc71_lightbar_write_ctrl(uint8_t ctrl)
{
	return ec_write_byte(LIGHTBAR_CONTROL_ADDR, ctrl);
}

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

	for (i = ARRAY_SIZE(lightbar_colors) - 1; i >= 0 && !err; i--, color /= 10)
		err = qc71_lightbar_set_color_level(lightbar_colors[i], color % 10);

	return err;
}

#endif

static int qc71_fn_lock_get_state(void)
{
	int status = ec_read_byte(BIOS_CTRL_1_ADDR);

	if (status < 0)
		return status;

	return !!(status & BIOS_CTRL_1_FN_LOCK_STATUS);
}

static int qc71_fn_lock_set_state(bool state)
{
	int status = ec_read_byte(BIOS_CTRL_1_ADDR);

	if (status < 0)
		return status;

	status = SET_BIT(status, BIOS_CTRL_1_FN_LOCK_STATUS, state);

	return ec_write_byte(BIOS_CTRL_1_ADDR, status);
}

/* ========================================================================== */
/* platform device attrs */

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
/* debugfs interface */

#if IS_ENABLED(CONFIG_DEBUG_FS)

static const struct qc71_debugfs_attr {
	const char *name;
	uint16_t addr;
} qc71_debugfs_attrs[] = {
	{"ap_bios_byte",     AP_BIOS_BYTE_ADDR},

	{"batt_alert",       BATT_ALERT_ADDR},
	{"batt_charge_ctrl", BATT_CHARGE_CTRL_ADDR},
	{"batt_status",      BATT_STATUS_ADDR},
	{"battt_temp",       BATT_TEMP_ADDR},
	{"bios_ctrl_1",      BIOS_CTRL_1_ADDR},
	{"bios_ctrl_2",      BIOS_CTRL_2_ADDR},
	{"bios_ctrl_3",      BIOS_CTRL_3_ADDR},
	{"bios_info_1",      BIOS_INFO_1_ADDR},
	{"bios_info_5",      BIOS_INFO_5_ADDR},

	{"ctrl_1",           CTRL_1_ADDR},
	{"ctrl_2",           CTRL_2_ADDR},
	{"ctrl_3",           CTRL_3_ADDR},
	{"ctrl_4",           CTRL_4_ADDR},

	{"device_status",    DEVICE_STATUS_ADDR},

	{"fan_ctrl",         FAN_CTRL_ADDR},
	{"fan_mode_index",   FAN_MODE_INDEX_ADDR},
	{"fan_temp_1",       qc71_fan_temp_addrs[0]},
	{"fan_temp_2",       qc71_fan_temp_addrs[1]},
	{"fan_pwm_1",        qc71_fan_pwm_addrs[0]},
	{"fan_pwm_2",        qc71_fan_pwm_addrs[1]},

	/* setting these don't seem to work */
	{"fan_l1_pwm",       ADDR(0x07, 0x43)},
	{"fan_l2_pwm",       ADDR(0x07, 0x44)},
	{"fan_l3_pwm",       ADDR(0x07, 0x45)},
	/* seemingly there is another level here, fan_ctrl=0x84, pwm=0x5a */
	{"fan_l4_pwm",       ADDR(0x07, 0x46)},
	{"fan_l5_pwm",       ADDR(0x07, 0x47)}, /* this is seemingly ignored, fan_ctrl=0x86, pwm=0xb4 */

	{"fan_l1_pwm_default", ADDR(0x07, 0x86)},
	{"fan_l2_pwm_default", ADDR(0x07, 0x87)},
	{"fan_l3_pwm_default", ADDR(0x07, 0x88)},
	{"fan_l4_pwm_default", ADDR(0x07, 0x89)},
	{"fan_l5_pwm_default", ADDR(0x07, 0x8A)},

	/* these don't seem to work */
	{"fan_min_speed",   1950},
	{"fan_min_temp",    1951},
	{"fan_extra_speed", 1952},

	{"lightbar_ctrl",    LIGHTBAR_CONTROL_ADDR},
	{"lightbar_red",     lightbar_color_addrs[LIGHTBAR_RED]},
	{"lightbar_green",   lightbar_color_addrs[LIGHTBAR_GREEN]},
	{"lightbar_blue",    lightbar_color_addrs[LIGHTBAR_BLUE]},

	{"support_1",        SUPPORT_1_ADDR},
	{"support_2",        SUPPORT_2_ADDR},
	{"support_5",        SUPPORT_5_ADDR},
	{"status_1",         STATUS_1_ADDR},

	{"platform_id",      PLATFORM_ID_ADDR},
	{"power_source",     POWER_SOURCE_ADDR},
	{"project_id",       PROJ_ID_ADDR},
	{"power_status",     POWER_STATUS_ADDR},
	{"pl_1",             PL1_ADDR},
	{"pl_2",             PL2_ADDR},
	{"pl_4",             PL4_ADDR},

	{"trigger_1",        TRIGGER_1_ADDR},
	{"trigger_2",        TRIGGER_2_ADDR},
};

static int get_debugfs_byte(void *data, u64 *value)
{
	const struct qc71_debugfs_attr *attr = data;
	int status = ec_read_byte(attr->addr);

	if (status < 0)
		return status;

	*value = status;

	return 0;
}

static int set_debugfs_byte(void *data, u64 value)
{
	const struct qc71_debugfs_attr *attr = data;
	int status;

	if (value > U8_MAX)
		return -EINVAL;

	status = ec_write_byte(attr->addr, (uint8_t) value);

	if (status < 0)
		return status;

	return status;
}

DEFINE_DEBUGFS_ATTRIBUTE(qc71_debugfs_fops, get_debugfs_byte, set_debugfs_byte, "%#04llx\n");

#endif

/* ========================================================================== */
/* battery */

#if IS_ENABLED(CONFIG_ACPI_BATTERY)

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

#endif

/* ========================================================================== */
/* hwmon */

#if IS_ENABLED(CONFIG_HWMON)

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
#ifdef HWMON_EXTRA
	case hwmon_temp:
		switch (attr) {
		case hwmon_temp_input:
		case hwmon_temp_label:
			return 0444;
		default:
			break;
		}
#endif
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
#ifdef HWMON_EXTRA
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
#endif
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

	return sprintf(buf, "%d\n", res * U8_MAX / FAN_MAX_PWM);
}

static ssize_t fan_pwm_store(uint8_t fan_index, const char *buf, size_t count)
{
	unsigned int value;
	int err;

	if (kstrtouint(buf, 10, &value))
		return -EINVAL;

	if (value > U8_MAX)
		return -EINVAL;

	err = qc71_fan_set_pwm(fan_index, value * FAN_MAX_PWM / U8_MAX);
	if (err)
		return err;

	return count;
}

static ssize_t fan_pwm1_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	return fan_pwm_show(0, buf);
}

static ssize_t fan_pwm1_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	return fan_pwm_store(0, buf, count);
}

static ssize_t fan_pwm2_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	return fan_pwm_show(1, buf);
}

#ifdef HWMON_EXTRA
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
#endif

static const struct hwmon_channel_info *qc71_hwmon_ch_info[] = {
	HWMON_CHANNEL_INFO(fan,
			   HWMON_F_INPUT | HWMON_F_FAULT,
			   HWMON_F_INPUT | HWMON_F_FAULT),
	HWMON_CHANNEL_INFO(pwm,
			   HWMON_PWM_ENABLE),
#ifdef HWMON_EXTRA
	HWMON_CHANNEL_INFO(temp,
			   HWMON_T_INPUT | HWMON_T_LABEL,
			   HWMON_T_INPUT | HWMON_T_LABEL,
			   HWMON_T_INPUT | HWMON_T_LABEL),
#endif
	NULL
};

static struct hwmon_ops qc71_hwmon_ops = {
	.is_visible  = qc71_hwmon_is_visible,
	.read        = qc71_hwmon_read,
	.write       = qc71_hwmon_write,
#ifdef HWMON_EXTRA
	.read_string = qc71_hwmon_read_string,
#endif
};

static struct hwmon_chip_info qc71_hwmon_chip_info = {
	.ops  = &qc71_hwmon_ops,
	.info =  qc71_hwmon_ch_info,
};

static DEVICE_ATTR(pwm1, 0644, fan_pwm1_show, fan_pwm1_store);
static DEVICE_ATTR(pwm2, 0444, fan_pwm2_show, NULL);
/* the two fans seem to be linked to pwm1, changing pwm2 has no effect */

static struct attribute *qc71_hwmon_attrs[] = {
	&dev_attr_pwm1.attr,
	&dev_attr_pwm2.attr,
	NULL
};

ATTRIBUTE_GROUPS(qc71_hwmon);

#endif

/* ========================================================================== */
/* lightbar attrs */

#if IS_ENABLED(CONFIG_LEDS_CLASS)

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
	.name                    = DRIVERNAME "::lightbar",
	.max_brightness          = 1,
	.brightness_get          = qc71_lightbar_led_get_brightness,
	.brightness_set_blocking = qc71_lightbar_led_set_brightness,
	.groups                  = qc71_lightbar_led_groups,
};

#endif

/* ========================================================================== */

static void toggle_fn_lock_from_event_handler(void)
{
	int status = qc71_fn_lock_get_state();

	if (status < 0)
		return;

	/* seemingly the returned status in the WMI event handler is not the current */
	pr_info("setting Fn lock state from %d to %d\n", !status, status);
	qc71_fn_lock_set_state(status);
}

#if IS_ENABLED(CONFIG_LEDS_BRIGHTNESS_HW_CHANGED)
extern struct rw_semaphore leds_list_lock;
extern struct list_head leds_list;

static void emit_keyboard_led_hw_changed(void)
{
	struct list_head *node;

	if (down_read_killable(&leds_list_lock))
		return;

	list_for_each(node, &leds_list) {
		struct led_classdev *led = container_of(node, struct led_classdev, node);
		size_t name_length;
		const char *suffix;

		if (!(led->flags & LED_BRIGHT_HW_CHANGED))
			continue;

		name_length = strlen(led->name);

		if (name_length < strlen(KBD_BL_LED_SUFFIX))
			continue;

		suffix = led->name + name_length - strlen(KBD_BL_LED_SUFFIX);

		if (strcmp(suffix, KBD_BL_LED_SUFFIX) == 0) {
			int s;

			if (mutex_lock_interruptible(&led->led_access))
				break;

			s = led_update_brightness(led);
			if (s >= 0)
				led_classdev_notify_brightness_hw_changed(led, led->brightness);

			mutex_unlock(&led->led_access);
			break;
		}
	}

	up_read(&leds_list_lock);
}
#else
static inline void emit_keyboard_led_hw_changed()
{

}
#endif

/* ========================================================================== */
/* WMI events */
/* for debugging */

static const struct key_entry qc71_wmi_hotkeys[] = {

	/* reported via keyboard controller */
	{ KE_IGNORE, 0x01, { KEY_CAPSLOCK }},
	{ KE_IGNORE, 0x02, { KEY_NUMLOCK }},
	{ KE_IGNORE, 0x03, { KEY_SCROLLLOCK }},

	/* reported via "video bus" */
	{ KE_IGNORE, 0x14, { KEY_BRIGHTNESSUP }},
	{ KE_IGNORE, 0x15, { KEY_BRIGHTNESSDOWN }},

	/* reported in automatic mode when rfkill state changes */
	{ KE_SW,     0x1a, {.sw = { SW_RFKILL_ALL, 1 }}},
	{ KE_SW,     0x1b, {.sw = { SW_RFKILL_ALL, 0 }}},

	/* reported via keyboard controller */
	{ KE_IGNORE, 0x35, { KEY_MUTE }},
	{ KE_IGNORE, 0x36, { KEY_VOLUMEDOWN }},
	{ KE_IGNORE, 0x37, { KEY_VOLUMEUP }},

	/*
	 * not reported by other means when in manual mode,
	 * handled automatically when it automatic mode
	 */
	{ KE_KEY,    0xa4, { KEY_RFKILL }},
	{ KE_KEY,    0xb1, { KEY_KBDILLUMDOWN }},
	{ KE_KEY,    0xb2, { KEY_KBDILLUMUP }},

	{ KE_END }

};

static void qc71_wmi_event_d2_handler(union acpi_object *obj)
{
	if (!obj || obj->type != ACPI_TYPE_INTEGER)
		return;

	sparse_keymap_report_event(qc71_input_dev, obj->integer.value, 1, true);

	switch (obj->integer.value) {
	/* caps lock */
	case 1:
		pr_info("caps lock\n");
		break;

	/* num lock */
	case 2:
		pr_info("num lock\n");
		break;

	/* scroll lock */
	case 3:
		pr_info("scroll lock\n");
		break;

	/* touchpad on */
	case 4:
		pr_info("touchpad on\n");
		break;

	/* touchpad off */
	case 5:
		pr_info("touchpad off\n");
		break;

	/* increase screen brightness */
	case 20:
		pr_info("increase screen brightness\n");
		break;

	/* decrease screen brightness */
	case 21:
		pr_info("decrease screen brightness\n");
		break;

	/* radio on */
	case 26:
		/* triggered in automatic mode when the rfkill hotkey is pressed */
		pr_info("radio on\n");
		break;

	/* radio off */
	case 27:
		/* triggered in automatic mode when the rfkill hotkey is pressed */
		pr_info("radio off\n");
		break;

	/* mute/unmute */
	case 53:
		pr_info("change mute state\n");
		break;

	/* decrease volume */
	case 54:
		pr_info("decrease volume\n");
		break;

	/* increase volume */
	case 55:
		pr_info("increase volume\n");
		break;

	case 57:
		pr_info("lightbar on\n");
		break;

	case 58:
		pr_info("lightbar off\n");
		break;

	/* enable super key (win key) lock */
	case 64:
		pr_info("enable super key lock\n");
		break;

	/* decrease volume */
	case 65:
		pr_info("disable super key lock\n");
		break;

	/* enable/disable airplane mode */
	case 164:
		pr_info("airplane mode\n");
		break;

	/* super key (win key) state changed */
	case 165:
		pr_info("super key lock state changed\n");
		sysfs_notify(&qc71_platform_dev->dev.kobj, NULL, "super_key_lock");
		break;

	case 166:
		pr_info("lightbar state changed\n");
		break;

	/* fan boost state changed */
	case 167:
		pr_info("fan boost state changed\n");
		break;

	/* charger unplugged/plugged in */
	case 171:
		pr_info("AC plugged/unplugged\n");
		break;

	/* perf mode button pressed */
	case 176:
		pr_info("change perf mode\n");
		/* TODO: should it be handled here? */
		break;

	/* increase keyboard backlight */
	case 177:
		pr_info("keyboard backlight decrease\n");
		/* TODO: should it be handled here? */
		break;

	/* decrease keyboard backlight */
	case 178:
		pr_info("keyboard backlight increase\n");
		/* TODO: should it be handled here? */
		break;

	/* toggle Fn lock (Fn+ESC)*/
	case 184:
		pr_info("toggle Fn lock\n");
		toggle_fn_lock_from_event_handler();
		break;

	/* keyboard backlight brightness changed */
	case 240:
		pr_info("keyboard backlight changed\n");
		emit_keyboard_led_hw_changed();
		break;

	default:
		pr_warn("unknown code: %u\n", (unsigned int) obj->integer.value);
		break;
	}
}

static void qc71_wmi_event_handler(u32 value, void *context)
{
	struct acpi_buffer response = { ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object *obj;
	acpi_status status;

	pr_info("%s(value=%#04x) called\n", __func__, (unsigned int) value);
	status = wmi_get_event_data(value, &response);

	if (status != AE_OK) {
		pr_err("bad WMI event status %#010x\n", (unsigned int) status);
		return;
	}

	obj = response.pointer;

	if (obj) {
		pr_info("obj->type = %d\n", (int) obj->type);
		if (obj->type == ACPI_TYPE_INTEGER) {
			pr_info("int = %u\n", (unsigned int) obj->integer.value);
		} else if (obj->type == ACPI_TYPE_STRING) {
			pr_info("string = '%s'\n", obj->string.pointer);
		} else if (obj->type == ACPI_TYPE_BUFFER) {
			uint32_t i;

			for (i = 0; i < obj->buffer.length; i++)
				pr_info("buf[%u] = %#04x\n",
					(unsigned int) i,
					(unsigned int) obj->buffer.pointer[i]);
		}
	}

	switch (value) {
	case 0xd2:
		qc71_wmi_event_d2_handler(obj);
		break;
	case 0xd1:
	case 0xd0:
		break;
	}

	kfree(obj);
}

/* ========================================================================== */
/* DMI */

static const struct dmi_system_id qc71_dmi_table[] __initconst = {
	{
		.matches = {
			DMI_MATCH(DMI_BOARD_NAME, "LAPQC71"),
			{ }
		}
	},
	{
		/* https://avell.com.br/avell-a60-muv-295765 */
		.matches = {
			DMI_EXACT_MATCH(DMI_CHASSIS_VENDOR, "Avell High Performance"),
			DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "A60 MUV"),
			{ }
		}
	},
	{ }
};
MODULE_DEVICE_TABLE(dmi, qc71_dmi_table);

struct oem_string_walker_data {
	char *value;
	int index;
};

static void __init oem_string_walker(const struct dmi_header *dm, void *ptr)
{
	int i, count;
	const uint8_t *s;
	struct oem_string_walker_data *data = ptr;

	if (dm->type != 11 || dm->length < 5 || !IS_ERR_OR_NULL(data->value))
		return;

	count = *(uint8_t *)(dm + 1);

	if (data->index >= count)
		return;

	i = 0;
	s = ((uint8_t *)dm) + dm->length;

	while (i++ < data->index && *s)
		s += strlen(s) + 1;

	data->value = kstrdup(s, GFP_KERNEL);

	if (!data->value)
		data->value = ERR_PTR(-ENOMEM);
}

static char * __init read_oem_string(int index)
{
	struct oem_string_walker_data d = {.value = ERR_PTR(-ENOENT),
					   .index = index};
	int err = dmi_walk(oem_string_walker, &d);

	if (err) {
		if (!IS_ERR_OR_NULL(d.value))
			kfree(d.value);
		return ERR_PTR(err);
	}

	return d.value;
}

/* ========================================================================== */
/* setup */

/* QCCFL357.0062.2020.0313.1530 -> 62 */
static int __pure __init parse_bios_version(const char *str)
{
	const char *p = strchr(str, '.'), *p2;
	int bios_version;

	if (!p)
		return -EINVAL;

	p2 = strchr(p + 1, '.');

	if (!p2)
		return -EINVAL;

	p += 1;

	bios_version = 0;

	while (p != p2) {
		if (!isdigit(*p))
			return -EINVAL;

		bios_version = 10 * bios_version + *p - '0';
		p += 1;
	}

	return bios_version;
}

static int __init check_features_ec(void)
{
	int err = ec_read_byte(SUPPORT_1_ADDR);

	if (err >= 0) {
		features.super_key_lock = !!(err & SUPPORT_1_SUPER_KEY_LOCK);
		features.lightbar       = !!(err & SUPPORT_1_LIGHTBAR);
		features.fan_boost      = !!(err & SUPPORT_1_FAN_BOOST);
	} else {
		pr_warn("failed to query support_1 byte: %d\n", err);
	}

	return err;
}

static int __init check_features_bios(void)
{
	const char *bios_version_str;
	int bios_version;

	bios_version_str = dmi_get_system_info(DMI_BIOS_VERSION);

	if (!bios_version_str) {
		pr_warn("failed to get BIOS version DMI string\n");
		return -ENOENT;
	}

	pr_info("BIOS version string: '%s'\n", bios_version_str);

	bios_version = parse_bios_version(bios_version_str);

	if (bios_version < 0) {
		pr_warn("cannot parse BIOS version\n");
		return -EINVAL;
	}

	pr_info("BIOS version: %04d\n", bios_version);

	if (bios_version >= 114) {
		const char *s = read_oem_string(18);
		size_t s_len;

		if (IS_ERR(s))
			return PTR_ERR(s);

		s_len = strlen(s);

		pr_info("OEM_STRING(18) = '%s'\n", s);

		/* if it is entirely spaces */
		if (strspn(s, " ") == s_len) {
			features.fn_lock           = true;
			features.batt_charge_limit = true;
			features.fan_extras        = true;
		} else if (s_len > 0) {
			/* TODO */
			pr_warn("cannot extract supported features");
		}

		kfree(s);
	}

	return 0;
}

static int __init check_features(void)
{
	(void) check_features_ec();
	(void) check_features_bios();

	return 0;
}

static int __init setup_platform_dev(void)
{
	int err = 0;

	qc71_platform_dev = platform_device_alloc(DRIVERNAME, -1);
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

static int __init setup_sysfs_attrs(void)
{
	size_t idx = 0;

	qc71_laptop_attrs[idx++] = &dev_attr_manual_control.attr;

	if (features.super_key_lock)
		qc71_laptop_attrs[idx++] = &dev_attr_super_key_lock.attr;

	if (features.fn_lock) {
		qc71_laptop_attrs[idx++] = &dev_attr_fn_lock.attr;
		qc71_laptop_attrs[idx++] = &dev_attr_fn_lock_switch.attr;
	}

	if (features.fan_extras) {
		qc71_laptop_attrs[idx++] = &dev_attr_fan_reduced_duty_cycle.attr;
		qc71_laptop_attrs[idx++] = &dev_attr_fan_always_on.attr;
	}

	qc71_laptop_attrs[idx] = NULL;

	WARN_ON(idx >= ARRAY_SIZE(qc71_laptop_attrs));

	return 0;
}

static int __init setup_input_dev(void)
{
	int err = 0;

	qc71_input_dev = input_allocate_device();
	if (!qc71_input_dev)
		return -ENOMEM;

	qc71_input_dev->name = "QC71 laptop input device";
	qc71_input_dev->phys = "qc71_laptop/input0";
	qc71_input_dev->id.bustype = BUS_HOST;
	qc71_input_dev->dev.parent = &qc71_platform_dev->dev;

	err = sparse_keymap_setup(qc71_input_dev, qc71_wmi_hotkeys, NULL);
	if (err)
		goto err_free_device;

	err = qc71_rfkill_get_state();
	if (err >= 0)
		input_report_switch(qc71_input_dev, SW_RFKILL_ALL, err);

	err = input_register_device(qc71_input_dev);
	if (err)
		goto err_free_device;

	return err;

err_free_device:
	input_free_device(qc71_input_dev);
	qc71_input_dev = NULL;

	return err;
}

static int __init setup_wmi_handlers(void)
{
	int err = 0, i;

	for (i = 0; i < ARRAY_SIZE(qc71_wmi_event_guids); i++) {
		const char *guid = qc71_wmi_event_guids[i];
		acpi_status status =
			wmi_install_notify_handler(guid, qc71_wmi_event_handler, NULL);

		if (ACPI_FAILURE(status)) {
			pr_err("could not install WMI notify handler for '%s': [%#010lx] %s\n",
			       guid, (unsigned long) status, acpi_format_exception(status));

			break;
		}

		wmi_handlers_installed += 1;
	}

	return err;
}

#if IS_ENABLED(CONFIG_HWMON)
/* platform device must be set up when this is called */
static int __init setup_hwmon(void)
{
	int err = 0;

	qc71_hwmon_dev = hwmon_device_register_with_info(&qc71_platform_dev->dev,
							 HWMON_NAME, NULL,
							 &qc71_hwmon_chip_info, qc71_hwmon_groups);

	if (IS_ERR(qc71_hwmon_dev))
		err = PTR_ERR(qc71_hwmon_dev);

	return err;
}
#endif

#if IS_ENABLED(CONFIG_ACPI_BATTERY)
static int __init setup_battery_hook(void)
{
	battery_hook_register(&qc71_laptop_batt_hook);
	battery_hook_registered = true;

	return 0;
}
#endif

#if IS_ENABLED(CONFIG_LEDS_CLASS)
/* platform device must be set up when this is called */
static int __init setup_lightbar_led(void)
{
	int err = led_classdev_register(&qc71_platform_dev->dev, &qc71_lightbar_led);

	if (!err)
		lightbar_led_registered = true;

	return err;
}
#endif

#if IS_ENABLED(CONFIG_DEBUG_FS)
static int __init setup_debugfs(void)
{
	int err = 0;
	size_t i;

	qc71_debugfs_dir = debugfs_create_dir(DRIVERNAME, NULL);

	if (IS_ERR(qc71_debugfs_dir)) {
		err = PTR_ERR(qc71_debugfs_dir);
		goto out;
	}

	for (i = 0; i < ARRAY_SIZE(qc71_debugfs_attrs); i++) {
		const struct qc71_debugfs_attr *attr = &qc71_debugfs_attrs[i];
		struct dentry *d = debugfs_create_file(attr->name, 0600, qc71_debugfs_dir,
						       (void *) attr, &qc71_debugfs_fops);

		if (IS_ERR(d)) {
			err = PTR_ERR(d);
			goto out;
		}
	}

out:
	return err;
}
#endif

/* ========================================================================== */
/* cleanup */

static void do_cleanup(void)
{
#if IS_ENABLED(CONFIG_DEBUG_FS)
	debugfs_remove_recursive(qc71_debugfs_dir);
#endif

#if IS_ENABLED(CONFIG_LEDS_CLASS)
	if (lightbar_led_registered)
		led_classdev_unregister(&qc71_lightbar_led);
#endif

#if IS_ENABLED(CONFIG_ACPI_BATTERY)
	if (battery_hook_registered)
		battery_hook_unregister(&qc71_laptop_batt_hook);
#endif

#if IS_ENABLED(CONFIG_HWMON)
	if (!IS_ERR_OR_NULL(qc71_hwmon_dev))
		hwmon_device_unregister(qc71_hwmon_dev);
#endif

	while (wmi_handlers_installed--)
		wmi_remove_notify_handler(qc71_wmi_event_guids[wmi_handlers_installed]);

	if (qc71_input_dev)
		input_unregister_device(qc71_input_dev);

	platform_device_unregister(qc71_platform_dev);
}

/* ========================================================================== */
/* init, exit */

static int __init qc71_laptop_module_init(void)
{
	int err = 0;

	if (!dmi_check_system(qc71_dmi_table)) {
		pr_err("no DMI match\n");
		err = -ENODEV; goto out;
	}

	if (!wmi_has_guid(QC71_WMI_WMBC_GUID)) {
		pr_err("WMI GUID not found\n");
		err = -ENODEV; goto out;
	}

	err = check_features();
	if (err) {
		pr_err("cannot check system features: %d\n", err);
		goto out;
	}

	pr_info("features: %s%s%s%s%s%s\n",
		features.super_key_lock ? "super-key-lock " : "",
		features.lightbar ? "lightbar " : "",
		features.fan_boost ? "fan-boost " : "",
		features.fn_lock  ? "fn-lock " : "",
		features.batt_charge_limit ? "charge-limit " : "",
		features.fan_extras ? "fan-extras " : "");

	err = setup_sysfs_attrs();
	if (err)
		goto out;

	err = setup_platform_dev();
	if (err)
		goto out;

	err = setup_input_dev();
	if (err)
		goto out;

	err = setup_wmi_handlers();
	if (err)
		goto out;

#if IS_ENABLED(CONFIG_HWMON)
	if (!nohwmon) {
		err = setup_hwmon();
		if (err)
			goto out;
	}
#endif

#if IS_ENABLED(CONFIG_ACPI_BATTERY)
	if (!nobattery && features.batt_charge_limit) {
		err = setup_battery_hook();
		if (err)
			goto out;
	}
#endif

#if IS_ENABLED(CONFIG_LEDS_CLASS)
	if (!nolightbar && features.lightbar) {
		err = setup_lightbar_led();
		if (err)
			goto out;
	}
#endif

#if IS_ENABLED(CONFIG_DEBUG_FS)
	if (debugregs) {
		err = setup_debugfs();
		if (err)
			goto out;
	}
#endif

out:
	if (err)
		do_cleanup();
	else
		pr_info("module loaded\n");

	return err;
}

static void __exit qc71_laptop_module_cleanup(void)
{
	do_cleanup();
	pr_info("module unloaded\n");
}

/* ========================================================================== */

module_init(qc71_laptop_module_init);
module_exit(qc71_laptop_module_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Barnabás Pőcze <pobrn@protonmail.com>");
MODULE_DESCRIPTION("QC71 laptop platform driver");
MODULE_VERSION("0.1");
