// SPDX-License-Identifier: GPL-2.0
#ifndef QC71_WMI_EVENTS_H
#define QC71_WMI_EVENTS_H

#if IS_ENABLED(CONFIG_LEDS_CLASS)

#include <linux/init.h>

int  __init qc71_wmi_events_setup(void);
void        qc71_wmi_events_cleanup(void);

#else

static inline int qc71_wmi_events_setup(void)
{
	return 0;
}

static inline void qc71_wmi_events_cleanup(void)
{

}

#endif

#endif /* QC71_WMI_EVENTS_H */
