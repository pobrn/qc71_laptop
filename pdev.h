// SPDX-License-Identifier: GPL-2.0
#ifndef QC71_PDEV_H
#define QC71_PDEV_H

#include <linux/init.h>
#include <linux/platform_device.h>

/* ========================================================================== */

extern struct platform_device *qc71_platform_dev;

/* ========================================================================== */

int  __init qc71_pdev_setup(void);
void        qc71_pdev_cleanup(void);

#endif /* QC71_PDEV_H */
