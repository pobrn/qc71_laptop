// SPDX-License-Identifier: GPL-2.0
#ifndef QC71_HWMON_FAN_H
#define QC71_HWMON_FAN_H

#include <linux/init.h>

int  __init qc71_hwmon_fan_setup(void);
void        qc71_hwmon_fan_cleanup(void);

#endif /* QC71_HWMON_FAN_H */
