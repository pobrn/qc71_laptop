// SPDX-License-Identifier: GPL-2.0
#include "pr.h"

#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <uapi/asm-generic/errno-base.h>

#include "hwmon_fan.h"
#include "hwmon_pwm.h"

/* ========================================================================== */

static bool nohwmon;
module_param(nohwmon, bool, 0444);
MODULE_PARM_DESC(nohwmon, "do not report to the hardware monitoring subsystem (default=false)");

/* ========================================================================== */

int __init qc71_hwmon_setup(void)
{
	if (nohwmon)
		return -ENODEV;

	(void) qc71_hwmon_fan_setup();
	(void) qc71_hwmon_pwm_setup();

	return 0;
}

void qc71_hwmon_cleanup(void)
{
	(void) qc71_hwmon_fan_cleanup();
	(void) qc71_hwmon_pwm_cleanup();
}
