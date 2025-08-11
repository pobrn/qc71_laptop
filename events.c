// SPDX-License-Identifier: GPL-2.0
#include "pr.h"

#include <acpi/video.h>
#include <dt-bindings/leds/common.h>
#include <linux/acpi.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/input/sparse-keymap.h>
#include <linux/leds.h>
#include <linux/version.h>

#include "events.h"
#include "misc.h"
#include "pdev.h"
#include "wmi.h"
#include "features.h"
#include "ec.h"

/* ========================================================================== */

#define KBD_BL_LED_SUFFIX ":" LED_FUNCTION_KBD_BACKLIGHT

/* ========================================================================== */

static struct {
	const char *guid;
	bool handler_installed;
} qc71_wmi_event_guids[] = {
	{ .guid = QC71_WMI_EVENT_70_GUID },
	{ .guid = QC71_WMI_EVENT_71_GUID },
	{ .guid = QC71_WMI_EVENT_72_GUID },
};

static const struct key_entry qc71_wmi_hotkeys[] = {

	/* reported via keyboard controller */
	{ KE_IGNORE, 0x01, { KEY_CAPSLOCK }},
	{ KE_IGNORE, 0x02, { KEY_NUMLOCK }},
	{ KE_IGNORE, 0x03, { KEY_SCROLLLOCK }},

	/* reported via "video bus" */
	{ KE_IGNORE, 0x14, { KEY_BRIGHTNESSUP }},
	{ KE_IGNORE, 0x15, { KEY_BRIGHTNESSDOWN }},

	/* reported in automatic mode when rfkill state changes */
	{ KE_SW,     0x1a, {.sw = { SW_RFKILL_ALL, 1 }}},
	{ KE_SW,     0x1b, {.sw = { SW_RFKILL_ALL, 0 }}},

	/* reported via keyboard controller */
	{ KE_IGNORE, 0x35, { KEY_MUTE }},
	{ KE_IGNORE, 0x36, { KEY_VOLUMEDOWN }},
	{ KE_IGNORE, 0x37, { KEY_VOLUMEUP }},

	/*
	 * not reported by other means when in manual mode,
	 * handled automatically when it automatic mode
	 */
	{ KE_KEY,    0xa4, { KEY_RFKILL }},
	{ KE_KEY,    0xa5, { KEY_FN_F2 }},
	{ KE_KEY,    0xb0, { KEY_FN_F5 }},
	{ KE_KEY,    0xb1, { KEY_KBDILLUMDOWN }},
	{ KE_KEY,    0xb2, { KEY_KBDILLUMUP }},
	{ KE_KEY,    0xb3, { KEY_KBDILLUMTOGGLE }},
	{ KE_KEY,    0xb8, { KEY_FN_ESC }},
	{ KE_KEY,    0xbc, { KEY_FN_F5 }},
	{ KE_KEY,    0xcf, { KEY_FN_F12 }},
	{ KE_END }
};

/* ========================================================================== */

static struct input_dev *qc71_input_dev;

/* ========================================================================== */

static void toggle_fn_lock_from_event_handler(void)
{
	int status = qc71_fn_lock_get_state();

	if (status < 0)
		return;

	/* seemingly the returned status in the WMI event handler is not the current */
	pr_info("setting Fn lock state from %d to %d\n", !status, status);
	qc71_fn_lock_set_state(status);
}

#if IS_ENABLED(CONFIG_LEDS_BRIGHTNESS_HW_CHANGED)
extern struct rw_semaphore leds_list_lock;
extern struct list_head leds_list;

static void emit_keyboard_led_hw_changed(void)
{
	struct led_classdev *led;

	if (down_read_killable(&leds_list_lock))
		return;

	list_for_each_entry (led, &leds_list, node) {
		size_t name_length;
		const char *suffix;

		if (!(led->flags & LED_BRIGHT_HW_CHANGED))
			continue;

		name_length = strlen(led->name);

		if (name_length < strlen(KBD_BL_LED_SUFFIX))
			continue;

		suffix = led->name + name_length - strlen(KBD_BL_LED_SUFFIX);

		if (strcmp(suffix, KBD_BL_LED_SUFFIX) == 0) {
			if (mutex_lock_interruptible(&led->led_access))
				break;

			if (led_update_brightness(led) >= 0)
				led_classdev_notify_brightness_hw_changed(led, led->brightness);

			mutex_unlock(&led->led_access);
			break;
		}
	}

	up_read(&leds_list_lock);
}
#else
static inline void emit_keyboard_led_hw_changed(void)
{ }
#endif

static void change_performance(void)
{
	int status, performance_bits, current_value, next_value;
	char profile[32];

	status = ec_read_byte(FAN_CTRL_ADDR);

	if (status<0)
		return;

	pr_debug("current profile:%x\n",status);

	performance_bits = FAN_CTRL_SILENT_MODE | FAN_CTRL_TURBO;
	current_value = status & performance_bits;
	next_value = status & ~performance_bits;

	switch (current_value) {
		case 0:
			next_value = next_value | FAN_CTRL_AUTO | FAN_CTRL_TURBO;
			sprintf(profile,"perfomance");
			break;

		case FAN_CTRL_SILENT_MODE:
			next_value = next_value | FAN_CTRL_AUTO;
			sprintf(profile,"balanced");
			break;

		case FAN_CTRL_TURBO:
			next_value = next_value | FAN_CTRL_AUTO | FAN_CTRL_SILENT_MODE;
			sprintf(profile,"energy-saver");
			break;

		default:
			next_value = next_value | FAN_CTRL_AUTO;
			sprintf(profile,"balanced");
	}

	pr_info("Setting profile to: %s\n",profile);
	ec_write_byte(FAN_CTRL_ADDR, next_value);
}

static void process_event_72(const union acpi_object *obj)
{
	bool do_report = true;

	if (obj->type != ACPI_TYPE_INTEGER)
		return;

	switch (obj->integer.value) {
	/* caps lock */
	case 0x01:
		pr_debug("caps lock\n");
		break;

	/* num lock */
	case 0x02:
		pr_debug("num lock\n");
		break;

	/* scroll lock */
	case 0x03:
		pr_debug("scroll lock\n");
		break;

	/* touchpad on */
	case 0x04:
		do_report = false;
		pr_debug("touchpad on\n");
		break;

	/* touchpad off */
	case 0x05:
		do_report = false;
		pr_debug("touchpad off\n");
		break;

	/* increase screen brightness */
	case 0x14:
		pr_debug("increase screen brightness\n");
		/* do_report = !acpi_video_handles_brightness_key_presses() */
		break;

	/* decrease screen brightness */
	case 0x15:
		pr_debug("decrease screen brightness\n");
		/* do_report = !acpi_video_handles_brightness_key_presses() */
		break;

	/* radio on */
	case 0x1a:
		/* triggered in automatic mode when the rfkill hotkey is pressed */
		pr_debug("radio on\n");
		break;

	/* radio off */
	case 0x1b:
		/* triggered in automatic mode when the rfkill hotkey is pressed */
		pr_debug("radio off\n");
		break;

	/* mute/unmute */
	case 0x35:
		pr_debug("toggle mute\n");
		break;

	/* decrease volume */
	case 0x36:
		pr_debug("decrease volume\n");
		break;

	/* increase volume */
	case 0x37:
		pr_debug("increase volume\n");
		break;

	case 0x39:
		do_report = false;
		pr_debug("lightbar on\n");
		break;

	case 0x3a:
		do_report = false;
		pr_debug("lightbar off\n");
		break;

	case 0x3b:
		do_report = false;
		pr_debug("backlight off\n");
		break;

	case 0x3d:
		do_report = false;
		pr_debug("backlight half\n");
		break;

	case 0x3f:
		do_report = false;
		pr_debug("backlight full\n");
		break;

	/* enable super key (win key) lock */
	case 0x40:
		do_report = false;
		pr_debug("enable super key lock\n");
		break;

	/* decrease volume */
	case 0x41:
		do_report = false;
		pr_debug("disable super key lock\n");
		break;

	/* enable/disable airplane mode */
	case 0xa4:
		pr_debug("toggle airplane mode\n");
		break;

	/* super key (win key) lock state changed */
	case 0xa5:
		//do_report = false;
		pr_debug("super key lock state changed\n");
		sysfs_notify(&qc71_platform_dev->dev.kobj, NULL, "super_key_lock");
		break;

	case 0xa6:
		do_report = false;
		pr_debug("lightbar state changed\n");
		break;

	/* fan boost state changed */
	case 0xa7:
		do_report = false;
		pr_info("fan boost state changed\n");
		break;

	/* charger unplugged/plugged in */
	case 0xab:
		do_report = false;
		pr_info("AC plugged/unplugged\n");
		break;

	/* perf mode button pressed */
	case 0xb0:
		do_report = false;
		pr_info("change perf mode\n");

		if (qc71_model == SLB_MODEL_EVO ||
			qc71_model == SLB_MODEL_CREATIVE) {
			do_report = true;
			change_performance();
		}
		break;

	/* increase keyboard backlight */
	case 0xb1:
		pr_debug("keyboard backlight decrease\n");
		/* TODO: should it be handled here? */
		break;

	/* decrease keyboard backlight */
	case 0xb2:
		pr_debug("keyboard backlight increase\n");
		/* TODO: should it be handled here? */
		break;

	/* keyboard backlight cycle */
	case 0xb3:
		break;

	/* toggle Fn lock (Fn+ESC)*/
	case 0xb8:
		pr_debug("toggle Fn lock\n");
		toggle_fn_lock_from_event_handler();
		sysfs_notify(&qc71_platform_dev->dev.kobj, NULL, "fn_lock");
		break;

	/* perf mode button pressed */
	case 0xbc:
		do_report = false;
		pr_info("change perfomance mode\n");
		
		if (qc71_model == SLB_MODEL_EXECUTIVE) {
			do_report = true;
		}

		sysfs_notify(&qc71_platform_dev->dev.kobj, NULL, "silent_mode");

		if (qc71_model == SLB_MODEL_HERO || qc71_model == SLB_MODEL_TITAN) {
			sysfs_notify(&qc71_platform_dev->dev.kobj, NULL, "turbo_mode");
		}
		break;

	/* webcam toggle on/off */
	case 0xcf:

		break;

	/* keyboard backlight brightness changed */
	case 0xf0:
		do_report = false;
		pr_debug("keyboard backlight changed\n");
		emit_keyboard_led_hw_changed();
		break;

	default:
		pr_warn("unknown code: %u\n", (unsigned int) obj->integer.value);
		break;
	}

	if (do_report && qc71_input_dev)
		sparse_keymap_report_event(qc71_input_dev,
					   obj->integer.value, 1, true);

}

static void process_event(const union acpi_object *obj, const char *guid)
{
	pr_debug("guid=%s obj=%p\n", guid, obj);

	if (!obj)
		return;

	pr_debug("obj->type = %d\n", (int) obj->type);
	if (obj->type == ACPI_TYPE_INTEGER) {
		pr_debug("int = %u\n", (unsigned int) obj->integer.value);
	} else if (obj->type == ACPI_TYPE_STRING) {
		pr_debug("string = '%s'\n", obj->string.pointer);
	} else if (obj->type == ACPI_TYPE_BUFFER) {
		pr_debug("buffer = %u %*ph", obj->buffer.length,
			(int) obj->buffer.length, (void *) obj->buffer.pointer);
	}

	if (strcmp(guid, QC71_WMI_EVENT_72_GUID) == 0)
		process_event_72(obj);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
static void qc71_wmi_event_handler(union acpi_object *obj, void *context)
{
	process_event(obj, context);
}
#else
static void qc71_wmi_event_handler(u32 value, void *context)
{
	struct acpi_buffer response = { ACPI_ALLOCATE_BUFFER, NULL };
	acpi_status status;

	status = wmi_get_event_data(value, &response);
	if (ACPI_FAILURE(status)) {
		pr_err("bad WMI event status: %#010x\n", (unsigned int) status);
		return;
	}

	process_event(response.pointer, context);
	kfree(response.pointer);
}
#endif

static int __init setup_input_dev(void)
{
	int err = 0;

	qc71_input_dev = input_allocate_device();
	if (!qc71_input_dev)
		return -ENOMEM;

	qc71_input_dev->name = "QC71 laptop input device";
	qc71_input_dev->phys = "qc71_laptop/input0";
	qc71_input_dev->id.bustype = BUS_HOST;
	qc71_input_dev->dev.parent = &qc71_platform_dev->dev;

	err = sparse_keymap_setup(qc71_input_dev, qc71_wmi_hotkeys, NULL);
	if (err)
		goto err_free_device;

	err = qc71_rfkill_get_wifi_state();
	if (err >= 0)
		input_report_switch(qc71_input_dev, SW_RFKILL_ALL, err);
	else
		input_report_switch(qc71_input_dev, SW_RFKILL_ALL, 1);

	err = input_register_device(qc71_input_dev);
	if (err)
		goto err_free_device;

	return err;

err_free_device:
	input_free_device(qc71_input_dev);
	qc71_input_dev = NULL;

	return err;
}

/* ========================================================================== */

int __init qc71_wmi_events_setup(void)
{
	int err = 0, i;

	(void) setup_input_dev();

	for (i = 0; i < ARRAY_SIZE(qc71_wmi_event_guids); i++) {
		const char *guid = qc71_wmi_event_guids[i].guid;
		acpi_status status =
			wmi_install_notify_handler(guid, qc71_wmi_event_handler, (void *) guid);

		if (ACPI_FAILURE(status)) {
			pr_warn("could not install WMI notify handler for '%s': [%#010lx] %s\n",
				guid, (unsigned long) status, acpi_format_exception(status));
		} else {
			qc71_wmi_event_guids[i].handler_installed = true;
		}
	}

	return err;
}

void qc71_wmi_events_cleanup(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(qc71_wmi_event_guids); i++) {
		if (qc71_wmi_event_guids[i].handler_installed) {
			wmi_remove_notify_handler(qc71_wmi_event_guids[i].guid);
			qc71_wmi_event_guids[i].handler_installed = false;
		}
	}

	if (qc71_input_dev)
		input_unregister_device(qc71_input_dev);
}
