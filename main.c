// SPDX-License-Identifier: GPL-2.0
/* ========================================================================== */
/* https://www.intel.com/content/dam/support/us/en/documents/laptops/whitebook/QC71_PROD_SPEC.pdf
 *
 *
 * based on the following resources:
 *  - https://lwn.net/Articles/391230/
 *  - http://blog.nietrzeba.pl/2011/12/mof-decompilation.html
 *  - https://github.com/tuxedocomputers/tuxedo-cc-wmi/
 *  - https://github.com/tuxedocomputers/tuxedo-keyboard/
 *  - Control Center for Microsoft Windows
 *  - http://forum.notebookreview.com/threads/tongfang-gk7cn6s-gk7cp0s-gk7cp7s.825461/page-54
 */
/* ========================================================================== */
#include "pr.h"

#include <linux/dmi.h>
#include <linux/errname.h>
#include <linux/init.h>
#include <linux/kconfig.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/wmi.h>

#include "features.h"
#include "wmi.h"

/* submodules */
#include "pdev.h"
#include "wmi_events.h"
#include "hwmon.h"
#include "battery.h"
#include "led_lightbar.h"
#include "debugfs.h"

/* ========================================================================== */
#if 0
static const struct dmi_system_id qc71_dmi_table[] __initconst = {
	{
		.matches = {
			DMI_MATCH(DMI_BOARD_NAME, "LAPQC71"),
			{ }
		}
	},
	{
		/* https://avell.com.br/avell-a60-muv-295765 */
		.matches = {
			DMI_EXACT_MATCH(DMI_CHASSIS_VENDOR, "Avell High Performance"),
			DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "A60 MUV"),
			{ }
		}
	},
	{ }
};
MODULE_DEVICE_TABLE(dmi, qc71_dmi_table);
#endif
/* ========================================================================== */

#define SUBMODULE_ENTRY(name) { #name, qc71_ ## name ## _setup, qc71_ ## name ## _cleanup }

static struct {
	const char *name;
	int (*init)(void);
	void (*cleanup)(void);
} qc71_submodules[] = {
	SUBMODULE_ENTRY(pdev), /* must be first */
	SUBMODULE_ENTRY(wmi_events),
	SUBMODULE_ENTRY(hwmon),
	SUBMODULE_ENTRY(battery),
	SUBMODULE_ENTRY(led_lightbar),
	SUBMODULE_ENTRY(debugfs),
};

#undef SUBMODULE_ENTRY

static void do_cleanup(void)
{
	int i;

	for (i = ARRAY_SIZE(qc71_submodules) - 1; i >= 0; i--)
		qc71_submodules[i].cleanup();
}

static int __init qc71_laptop_module_init(void)
{
	int err = 0, i;

#if 0
	if (!dmi_check_system(qc71_dmi_table)) {
		pr_err("no DMI match\n");
		err = -ENODEV; goto out;
	}
#endif

	if (!wmi_has_guid(QC71_WMI_WMBC_GUID)) {
		pr_err("WMI GUID not found\n");
		err = -ENODEV; goto out;
	}

	err = qc71_check_features();
	if (err) {
		pr_err("cannot check system features: %d\n", err);
		goto out;
	}

	pr_info("features: %s%s%s%s%s%s\n",
		qc71_features.super_key_lock ? "super-key-lock " : "",
		qc71_features.lightbar ? "lightbar " : "",
		qc71_features.fan_boost ? "fan-boost " : "",
		qc71_features.fn_lock  ? "fn-lock " : "",
		qc71_features.batt_charge_limit ? "charge-limit " : "",
		qc71_features.fan_extras ? "fan-extras " : "");

	for (i = 0; i < ARRAY_SIZE(qc71_submodules); i++) {
		const char *name = qc71_submodules[i].name;
		
		err = qc71_submodules[i].init();
		if (err) {
			pr_err("failed to initialize %s submodule: %d\n", name, err);
			goto out;
		}
	}

out:
	if (err)
		do_cleanup();
	else
		pr_info("module loaded\n");

	return err;
}

static void __exit qc71_laptop_module_cleanup(void)
{
	do_cleanup();
	pr_info("module unloaded\n");
}

/* ========================================================================== */

module_init(qc71_laptop_module_init);
module_exit(qc71_laptop_module_cleanup);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Barnabás Pőcze <pobrn@protonmail.com>");
MODULE_DESCRIPTION("QC71 laptop platform driver");
MODULE_VERSION("0.1");
MODULE_ALIAS("wmi:" QC71_WMI_WMBC_GUID);
