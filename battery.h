// SPDX-License-Identifier: GPL-2.0
#ifndef QC71_BATTERY_H
#define QC71_BATTERY_H

#if IS_ENABLED(CONFIG_ACPI_BATTERY)

#include <linux/init.h>

int  __init qc71_battery_setup(void);
void        qc71_battery_cleanup(void);

#else

static inline int qc71_battery_setup(void)
{
	return 0;
}

static inline void qc71_battery_cleanup(void)
{

}

#endif

#endif /* QC71_BATTERY_H */
