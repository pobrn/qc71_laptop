// SPDX-License-Identifier: GPL-2.0
#include "pr.h"

#include <linux/debugfs.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/uaccess.h>
#include <linux/sched/signal.h>
#include <linux/types.h>

#include "debugfs.h"
#include "ec.h"

#if IS_ENABLED(CONFIG_DEBUG_FS)

#define DEBUGFS_DIR_NAME KBUILD_MODNAME

static const struct qc71_debugfs_attr {
	const char *name;
	uint16_t addr;
} qc71_debugfs_attrs[] = {
	{"1108", 1108},

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
	{"ctrl_5",           CTRL_5_ADDR},
	{"ctrl_6",           CTRL_6_ADDR},

	{"device_status",    DEVICE_STATUS_ADDR},

	{"fan_ctrl",         FAN_CTRL_ADDR},
	{"fan_mode_index",   FAN_MODE_INDEX_ADDR},
	{"fan_temp_1",       FAN_TEMP_1_ADDR},
	{"fan_temp_2",       FAN_TEMP_2_ADDR},
	{"fan_pwm_1",        FAN_PWM_1_ADDR},
	{"fan_pwm_2",        FAN_PWM_2_ADDR},

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

	{"lightbar_ctrl",    LIGHTBAR_CTRL_ADDR},
	{"lightbar_red",     LIGHTBAR_RED_ADDR},
	{"lightbar_green",   LIGHTBAR_GREEN_ADDR},
	{"lightbar_blue",    LIGHTBAR_BLUE_ADDR},

	{"keyboard_type",    KEYBOARD_TYPE_ADDR},

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

/* ========================================================================== */

static bool debugregs;
module_param(debugregs, bool, 0444);
MODULE_PARM_DESC(debugregs, "expose various EC registers in debugfs (default=false)");

static struct dentry *qc71_debugfs_dir,
		     *qc71_debugfs_regs_dir;

/* ========================================================================== */

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

DEFINE_DEBUGFS_ATTRIBUTE(qc71_debugfs_fops, get_debugfs_byte, set_debugfs_byte, "0x%02llx\n");

/* ========================================================================== */

static ssize_t qc71_debugfs_ec_read(struct file *f, char __user *buf, size_t count, loff_t *offset)
{
	size_t i;

	for (i = 0; *offset + i < U16_MAX && i < count; i++) {
		int err = ec_read_byte(*offset + i);
		u8 byte;

		if (signal_pending(current))
			return -EINTR;

		if (err < 0) {
			if (i)
				break;

			return err;
		}

		byte = err;

		if (put_user(byte, buf + i))
			return -EFAULT;
	}

	*offset += i;

	return i;
}

static ssize_t qc71_debugfs_ec_write(struct file *f, const char __user *buf, size_t count, loff_t *offset)
{
	size_t i;

	for (i = 0; *offset + i < U16_MAX && i < count; i++) {
		int err;
		u8 byte;

		if (get_user(byte, buf + i))
			return -EFAULT;

		err = ec_write_byte(*offset + i, byte);
		if (err) {
			if (i)
				break;

			return err;
		}

		if (signal_pending(current))
			return -EINTR;
	}

	*offset += i;

	return i;
}

static const struct file_operations qc71_debugfs_ec_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.read = qc71_debugfs_ec_read,
	.write = qc71_debugfs_ec_write,
	.llseek = default_llseek,
};

/* ========================================================================== */

int __init qc71_debugfs_setup(void)
{
	struct dentry *d;
	int err = 0;
	size_t i;

	if (!debugregs)
		return -ENODEV;

	qc71_debugfs_dir = debugfs_create_dir(DEBUGFS_DIR_NAME, NULL);

	if (IS_ERR(qc71_debugfs_dir)) {
		err = PTR_ERR(qc71_debugfs_dir);
		goto out;
	}

	qc71_debugfs_regs_dir = debugfs_create_dir("regs", qc71_debugfs_dir);

	if (IS_ERR(qc71_debugfs_regs_dir)) {
		err = PTR_ERR(qc71_debugfs_regs_dir);
		debugfs_remove_recursive(qc71_debugfs_dir);
		goto out;
	}

	for (i = 0; i < ARRAY_SIZE(qc71_debugfs_attrs); i++) {
		const struct qc71_debugfs_attr *attr = &qc71_debugfs_attrs[i];

		d = debugfs_create_file(attr->name, 0600, qc71_debugfs_regs_dir,
					(void *) attr, &qc71_debugfs_fops);

		if (IS_ERR(d)) {
			err = PTR_ERR(d);
			debugfs_remove_recursive(qc71_debugfs_dir);
			goto out;
		}
	}

	d = debugfs_create_file("ec", 0600, qc71_debugfs_dir, NULL, &qc71_debugfs_ec_fops);
	if (IS_ERR(d)) {
		err = PTR_ERR(d);
		debugfs_remove_recursive(qc71_debugfs_dir);
		goto out;
	}

out:
	return err;
}

void qc71_debugfs_cleanup(void)
{
	/* checks if IS_ERR_OR_NULL() */
	debugfs_remove_recursive(qc71_debugfs_dir);
}

#endif
