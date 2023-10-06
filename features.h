// SPDX-License-Identifier: GPL-2.0
#ifndef QC71_FEATURES_H
#define QC71_FEATURES_H

#include <linux/init.h>
#include <linux/types.h>

struct qc71_features_struct {
	bool super_key_lock    : 1;
	bool lightbar          : 1;
	bool fan_boost         : 1;
	bool fn_lock           : 1;
	bool batt_charge_limit : 1;
	bool fan_extras        : 1; /* duty cycle reduction, always on mode */
	bool silent_mode       : 1; /* Slimbook silent mode: decreases fan rpm limit and tdp */
	bool turbo_mode        : 1;
};

/* ========================================================================== */

extern struct qc71_features_struct qc71_features;

/* ========================================================================== */

int __init qc71_check_features(void);

#endif /* QC71_FEATURES_H */
