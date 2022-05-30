// SPDX-License-Identifier: GPL-2.0
#include "pr.h"

#include <linux/ctype.h>
#include <linux/dmi.h>
#include <linux/init.h>
#include <linux/mod_devicetable.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "ec.h"
#include "features.h"

/* ========================================================================== */

struct oem_string_walker_data {
	char *value;
	int index;
};

/* ========================================================================== */

static int __init slimbook_dmi_cb(const struct dmi_system_id *id)
{
	qc71_features.fn_lock      = true;
	qc71_features.silent_mode  = true;

	return 1;
}

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
	{
		/* Slimbook PROX AMD */
		.callback = slimbook_dmi_cb,
		.matches = {
			DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "PROX-AMD"),
			{ }
		}
	},
	{
		/* Slimbook PROX15 AMD */
		.callback = slimbook_dmi_cb,
		.matches = {
			DMI_EXACT_MATCH(DMI_PRODUCT_NAME, "PROX15-AMD"),
			{ }
		}
	},
	{
		/* Slimbook PROX AMD5 */
		.callback = slimbook_dmi_cb,
		.matches = {
			DMI_EXACT_MATCH(DMI_PRODUCT_NAME,"PROX-AMD5"),
			{ }
		}
	},
	{
		/* Slimbook PROX15 AMD5 */
		.callback = slimbook_dmi_cb,
		.matches = {
			DMI_EXACT_MATCH(DMI_PRODUCT_NAME,"PROX15-AMD5"),
			{ }
		}
	},
	{ }
};

/* ========================================================================== */

struct qc71_features_struct qc71_features;

/* ========================================================================== */

static void __init oem_string_walker(const struct dmi_header *dm, void *ptr)
{
	int i, count;
	const uint8_t *s;
	struct oem_string_walker_data *data = ptr;

	if (dm->type != 11 || dm->length < 5 || !IS_ERR_OR_NULL(data->value))
		return;

	count = *(uint8_t *)(dm + 1);

	if (data->index >= count)
		return;

	i = 0;
	s = ((uint8_t *)dm) + dm->length;

	while (i++ < data->index && *s)
		s += strlen(s) + 1;

	data->value = kstrdup(s, GFP_KERNEL);

	if (!data->value)
		data->value = ERR_PTR(-ENOMEM);
}

static char * __init read_oem_string(int index)
{
	struct oem_string_walker_data d = {.value = ERR_PTR(-ENOENT),
					   .index = index};
	int err = dmi_walk(oem_string_walker, &d);

	if (err) {
		if (!IS_ERR_OR_NULL(d.value))
			kfree(d.value);
		return ERR_PTR(err);
	}

	return d.value;
}

/* QCCFL357.0062.2020.0313.1530 -> 62 */
static int __pure __init parse_bios_version(const char *str)
{
	const char *p = strchr(str, '.'), *p2;
	int bios_version;

	if (!p)
		return -EINVAL;

	p2 = strchr(p + 1, '.');

	if (!p2)
		return -EINVAL;

	p += 1;

	bios_version = 0;

	while (p != p2) {
		if (!isdigit(*p))
			return -EINVAL;

		bios_version = 10 * bios_version + *p - '0';
		p += 1;
	}

	return bios_version;
}

static int __init check_features_ec(void)
{
	int err = ec_read_byte(SUPPORT_1_ADDR);

	if (err >= 0) {
		qc71_features.super_key_lock = !!(err & SUPPORT_1_SUPER_KEY_LOCK);
		qc71_features.lightbar       = !!(err & SUPPORT_1_LIGHTBAR);
		qc71_features.fan_boost      = !!(err & SUPPORT_1_FAN_BOOST);
	} else {
		pr_warn("failed to query support_1 byte: %d\n", err);
	}

	return err;
}

static int __init check_features_bios(void)
{
	const char *bios_version_str;
	int bios_version;

	if (!dmi_check_system(qc71_dmi_table)) {
		pr_warn("no DMI match\n");
		return -ENODEV;
	}

	bios_version_str = dmi_get_system_info(DMI_BIOS_VERSION);

	if (!bios_version_str) {
		pr_warn("failed to get BIOS version DMI string\n");
		return -ENOENT;
	}

	pr_info("BIOS version string: '%s'\n", bios_version_str);

	bios_version = parse_bios_version(bios_version_str);

	if (bios_version < 0) {
		pr_warn("cannot parse BIOS version\n");
		return -EINVAL;
	}

	pr_info("BIOS version: %04d\n", bios_version);

	if (bios_version >= 114) {
		const char *s = read_oem_string(18);
		size_t s_len;

		if (IS_ERR(s))
			return PTR_ERR(s);

		s_len = strlen(s);

		pr_info("OEM_STRING(18) = '%s'\n", s);

		/* if it is entirely spaces */
		if (strspn(s, " ") == s_len) {
			qc71_features.fn_lock           = true;
			qc71_features.batt_charge_limit = true;
			qc71_features.fan_extras        = true;
		} else if (s_len > 0) {
			/* TODO */
			pr_warn("cannot extract supported features");
		}

		kfree(s);
	}

	return 0;
}

int __init qc71_check_features(void)
{
	(void) check_features_ec();
	(void) check_features_bios();

	return 0;
}
