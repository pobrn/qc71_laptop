// SPDX-License-Identifier: GPL-2.0
#ifndef QC71_LED_LIGHTBAR_H
#define QC71_LED_LIGHTBAR_H

#if IS_ENABLED(CONFIG_LEDS_CLASS)

#include <linux/init.h>

int  __init qc71_led_lightbar_setup(void);
void        qc71_led_lightbar_cleanup(void);

#else

static inline int qc71_led_lightbar_setup(void)
{
	return 0;
}

static inline void qc71_led_lightbar_cleanup(void)
{

}

#endif

#endif /* QC71_LED_LIGHTBAR_H */
