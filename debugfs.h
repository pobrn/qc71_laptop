// SPDX-License-Identifier: GPL-2.0
#ifndef QC71_DEBUGFS_H
#define QC71_DEBUGFS_H

#if IS_ENABLED(CONFIG_DEBUG_FS)

#include <linux/init.h>

int  __init qc71_debugfs_setup(void);
void        qc71_debugfs_cleanup(void);

#else

static inline int qc71_debugfs_setup(void)
{
	return 0;
}

static inline void qc71_debugfs_cleanup(void)
{

}

#endif

#endif /* QC71_DEBUGFS_H */
