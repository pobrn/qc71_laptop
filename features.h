// SPDX-License-Identifier: GPL-2.0
#ifndef QC71_FEATURES_H
#define QC71_FEATURES_H

#include <linux/init.h>
#include <linux/types.h>

#define SLB_MODEL_EXECUTIVE             0x0100
#define SLB_MODEL_PROX                  0x0200
#define SLB_MODEL_TITAN                 0x0400
#define SLB_MODEL_HERO                  0x0800
#define SLB_MODEL_EVO                 0x010000
#define SLB_MODEL_CREATIVE            0x020000

struct qc71_features_struct {
	bool super_key_lock    : 1;
	bool lightbar          : 1;
	bool fan_boost         : 1;
	bool fn_lock           : 1;
	bool batt_charge_limit : 1;
	bool fan_extras        : 1; /* duty cycle reduction, always on mode */
	bool silent_mode       : 1; /* Slimbook silent mode: decreases fan rpm limit and tdp */
	bool turbo_mode        : 1; /* Slimbook Titan turbo mode */
	bool kbd_backlight_rgb : 1; /* Slimbook Hero backlight keyboard */
};

/* ========================================================================== */

extern struct qc71_features_struct qc71_features;
extern uint32_t qc71_model;

/* ========================================================================== */

int __init qc71_check_features(void);

#endif /* QC71_FEATURES_H */
