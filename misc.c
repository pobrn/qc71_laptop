// SPDX-License-Identifier: GPL-2.0
#include "pr.h"

#include <linux/bug.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>

#include "ec.h"
#include "misc.h"
#include "util.h"

/* ========================================================================== */

int qc71_rfkill_get_wifi_state(void)
{
	int err = ec_read_byte(DEVICE_STATUS_ADDR);

	if (err < 0)
		return err;

	return !!(err & DEVICE_STATUS_WIFI_ON);
}

/* ========================================================================== */

int qc71_fn_lock_get_state(void)
{
	int status = ec_read_byte(BIOS_CTRL_1_ADDR);

	if (status < 0)
		return status;

	return !!(status & BIOS_CTRL_1_FN_LOCK_STATUS);
}

int qc71_fn_lock_set_state(bool state)
{
	int status = ec_read_byte(BIOS_CTRL_1_ADDR);

	if (status < 0)
		return status;

	status = SET_BIT(status, BIOS_CTRL_1_FN_LOCK_STATUS, state);

	return ec_write_byte(BIOS_CTRL_1_ADDR, status);
}
