// SPDX-License-Identifier: GPL-2.0
#ifndef QC71_HWMON_H
#define QC71_HWMON_H

#if IS_ENABLED(CONFIG_HWMON)

#include <linux/init.h>

int  __init qc71_hwmon_setup(void);
void        qc71_hwmon_cleanup(void);

#else

static inline int qc71_hwmon_setup(void)
{
	return 0;
}

static inline void qc71_hwmon_cleanup(void)
{

}

#endif

#endif /* QC71_HWMON_H */
