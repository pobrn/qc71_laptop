// SPDX-License-Identifier: GPL-2.0
#ifndef QC71_HWMON_PWM_H
#define QC71_HWMON_PWM_H

#include <linux/init.h>

int  __init qc71_hwmon_pwm_setup(void);
void        qc71_hwmon_pwm_cleanup(void);

#endif /* QC71_HWMON_PWM_H */
