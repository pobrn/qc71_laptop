// SPDX-License-Identifier: GPL-2.0
#ifndef QC71_MISC_H
#define QC71_MISC_H

#include <linux/types.h>

/* ========================================================================== */

int qc71_rfkill_get_wifi_state(void);

int qc71_fn_lock_get_state(void);
int qc71_fn_lock_set_state(bool state);

#endif /* QC71_MISC_H */
