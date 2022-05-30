// SPDX-License-Identifier: GPL-2.0
#ifndef QC71_LAPTOP_EC_H
#define QC71_LAPTOP_EC_H

#include <linux/compiler_types.h>
#include <linux/types.h>

/* ========================================================================== */
/*
 * EC register addresses and bitmasks,
 * some of them are not used,
 * only for documentation
 */

#define ADDR(page, offset)       (((uint16_t)(page) << 8) | ((uint16_t)(offset)))

/* ========================================================================== */

#define AP_BIOS_BYTE_ADDR ADDR(0x07, 0xA4)
#define AP_BIOS_BYTE_FN_LOCK_SWITCH BIT(3)

/* ========================================================================== */

/* battery charger control register */
#define BATT_CHARGE_CTRL_ADDR ADDR(0x07, 0xB9)
#define BATT_CHARGE_CTRL_VALUE_MASK GENMASK(6, 0)
#define BATT_CHARGE_CTRL_REACHED BIT(7)

#define BATT_STATUS_ADDR        ADDR(0x04, 0x32)
#define BATT_STATUS_DISCHARGING BIT(0)

/* possibly temp (in C) = value / 10 + X */
#define BATT_TEMP_ADDR ADDR(0x04, 0xA2)

#define BATT_ALERT_ADDR ADDR(0x04, 0x94)

#define BIOS_CTRL_1_ADDR           ADDR(0x07, 0x4E)
#define BIOS_CTRL_1_FN_LOCK_STATUS BIT(4)

#define BIOS_CTRL_2_ADDR                       ADDR(0x07, 0x82)
#define BIOS_CTRL_2_FAN_V2_NEW                 BIT(0)
#define BIOS_CTRL_2_FAN_QKEY                   BIT(1)
#define BIOS_CTRL_2_OFFICE_MODE_FAN_TABLE_TYPE BIT(2)
#define BIOS_CTRL_2_FAN_V3                     BIT(3)
#define BIOS_CTRL_2_DEFAULT_MODE               BIT(4)

/* 3rd control register of a different kind */
#define BIOS_CTRL_3_ADDR                   ADDR(0x7, 0xA3)
#define BIOS_CTRL_3_FAN_REDUCED_DUTY_CYCLE BIT(5)
#define BIOS_CTRL_3_FAN_ALWAYS_ON          BIT(6)

#define BIOS_INFO_1_ADDR ADDR(0x04, 0x9F)
#define BIOS_INFO_5_ADDR ADDR(0x04, 0x66)

/* ========================================================================== */

#define CTRL_1_ADDR                    ADDR(0x07, 0x41)
#define CTRL_1_MANUAL_MODE             BIT(0)
#define CTRL_1_ITE_KBD_EFFECT_REACTIVE BIT(3)
#define CTRL_1_FAN_ABNORMAL            BIT(5)

#define CTRL_2_ADDR                        ADDR(0x07, 0x8C)
#define CTRL_2_SINGLE_COLOR_KEYBOARD       BIT(0)
#define CTRL_2_SINGLE_COLOR_KBD_BL_OFF     BIT(1)
#define CTRL_2_TURBO_LEVEL_MASK            GENMASK(3, 2)
#define CTRL_2_TURBO_LEVEL_0               0x00
#define CTRL_2_TURBO_LEVEL_1               BIT(2)
#define CTRL_2_TURBO_LEVEL_2               BIT(3)
#define CTRL_2_TURBO_LEVEL_3               (BIT(2) | BIT(3))
// #define CTRL_2_SINGLE_COLOR_KBD_?         BIT(4)
#define CTRL_2_SINGLE_COLOR_KBD_BRIGHTNESS GENMASK(7, 5)

#define CTRL_3_ADDR         ADDR(0x07, 0xA5)
#define CTRL_3_PWR_LED_MASK GENMASK(1, 0)
#define CTRL_3_PWR_LED_NONE BIT(1)
#define CTRL_3_PWR_LED_BOTH BIT(0)
#define CTRL_3_PWR_LED_LEFT 0x00
#define CTRL_3_FAN_QUIET    BIT(2)
#define CTRL_3_OVERBOOST    BIT(4)
#define CTRL_3_HIGH_PWR     BIT(7)

#define CTRL_4_ADDR                   ADDR(0x07, 0xA6)
#define CTRL_4_OVERBOOST_DYN_TEMP_OFF BIT(1)
#define CTRL_4_TOUCHPAD_TOGGLE_OFF    BIT(6)

#define CTRL_5_ADDR ADDR(0x07, 0xC5)

#define CTRL_6_ADDR ADDR(0x07, 0x8E)

/* ========================================================================== */

#define DEVICE_STATUS_ADDR  ADDR(0x04, 0x7B)
#define DEVICE_STATUS_WIFI_ON BIT(7)
/* BIT(5) is seemingly also (un)set depending on the rfkill state (bluetooth?) */

/* ========================================================================== */

/* fan control register */
#define FAN_CTRL_ADDR         ADDR(0x07, 0x51)
#define FAN_CTRL_LEVEL_MASK   GENMASK(2, 0)
#define FAN_CTRL_TURBO        BIT(4)
#define FAN_CTRL_AUTO         BIT(5)
#define FAN_CTRL_FAN_BOOST    BIT(6)
#define FAN_CTRL_SILENT_MODE  BIT(7)

#define FAN_RPM_1_ADDR ADDR(0x04, 0x64)
#define FAN_RPM_2_ADDR ADDR(0x04, 0x6C)

#define FAN_PWM_1_ADDR ADDR(0x18, 0x04)
#define FAN_PWM_2_ADDR ADDR(0x18, 0x09)

#define FAN_TEMP_1_ADDR ADDR(0x04, 0x3e)
#define FAN_TEMP_2_ADDR ADDR(0x04, 0x4f)

#define FAN_MODE_INDEX_ADDR ADDR(0x07, 0xAB)
#define FAN_MODE_INDEX_LOW_MASK GENMASK(3, 0)
#define FAN_MODE_INDEX_HIGH_MASK GENMASK(7, 4)

/* ========================================================================== */

/*
 * the actual keyboard type is seemingly determined from this number,
 * the project id, the controller firmware version,
 * and the HID usage page of the descriptor of the controller
 */
#define KEYBOARD_TYPE_ADDR  ADDR(0x07, 0x3C)
#define KEYBOARD_TYPE_101    25
#define KEYBOARD_TYPE_101M   41
#define KEYBOARD_TYPE_102    17
#define KEYBOARD_TYPE_102M   33
#define KEYBOARD_TYPE_85     25
#define KEYBOARD_TYPE_86     17
#define KEYBOARD_TYPE_87     73
#define KEYBOARD_TYPE_88     65
#define KEYBOARD_TYPE_97     57
#define KEYBOARD_TYPE_98     49
#define KEYBOARD_TYPE_99    121
#define KEYBOARD_TYPE_100   113

/* ========================================================================== */

/* lightbar control register */
#define LIGHTBAR_CTRL_ADDR       ADDR(0x07, 0x48)
#define LIGHTBAR_CTRL_POWER_SAVE BIT(1)
#define LIGHTBAR_CTRL_S0_OFF     BIT(2)
#define LIGHTBAR_CTRL_S3_OFF     BIT(3)
#define LIGHTBAR_CTRL_RAINBOW    BIT(7)

#define LIGHTBAR_RED_ADDR   ADDR(0x07, 0x49)
#define LIGHTBAR_GREEN_ADDR ADDR(0x07, 0x4A)
#define LIGHTBAR_BLUE_ADDR  ADDR(0x07, 0x4B)

/* ========================================================================== */

#define PROJ_ID_ADDR              ADDR(0x07, 0x40)
#define PROJ_ID_GIxKN              1
#define PROJ_ID_GJxKN              2
#define PROJ_ID_GKxCN              3
#define PROJ_ID_GIxCN              4
#define PROJ_ID_GJxCN              5
#define PROJ_ID_GK5CN_X            6
#define PROJ_ID_GK7CN_S            7
#define PROJ_ID_GK7CPCS_GK5CQ7Z    8
#define PROJ_ID_PF5NU1G_PF4LUXF    9
#define PROJ_ID_IDP               11
#define PROJ_ID_ID6Y              12
#define PROJ_ID_ID7Y              13
#define PROJ_ID_PF4MU_PF4MN_PF5MU 14
#define PROJ_ID_CML_GAMING        15
#define PROJ_ID_GK7NXXR           16
#define PROJ_ID_GM5MU1Y           17

/* ========================================================================== */

#define STATUS_1_ADDR           ADDR(0x07, 0x68)
#define STATUS_1_SUPER_KEY_LOCK BIT(0)
#define STATUS_1_LIGHTBAR       BIT(1)
#define STATUS_1_FAN_BOOST      BIT(2)

#define SUPPORT_1_ADDR           ADDR(0x07, 0x65)
#define SUPPORT_1_AIRPLANE_MODE  BIT(0)
#define SUPPORT_1_GPS_SWITCH     BIT(1)
#define SUPPORT_1_OVERCLOCK      BIT(2)
#define SUPPORT_1_MACRO_KEY      BIT(3)
#define SUPPORT_1_SHORTCUT_KEY   BIT(4)
#define SUPPORT_1_SUPER_KEY_LOCK BIT(5)
#define SUPPORT_1_LIGHTBAR       BIT(6)
#define SUPPORT_1_FAN_BOOST      BIT(7)

#define SUPPORT_2_ADDR            ADDR(0x07, 0x66)
#define SUPPORT_2_SILENT_MODE     BIT(0)
#define SUPPORT_2_USB_CHARGING    BIT(1)
#define SUPPORT_2_SINGLE_ZONE_KBD BIT(2)
#define SUPPORT_2_CHINA_MODE      BIT(5)
#define SUPPORT_2_MY_BATTERY      BIT(6)

#define SUPPORT_5_ADDR      ADDR(0x07, 0x42)
#define SUPPORT_5_FAN_TURBO BIT(4)
#define SUPPORT_5_FAN       BIT(5)

#define SUPPORT_6 ADDR(0x07, 0x8E)
#define SUPPORT_6_FAN3P5 BIT(1)

/* ========================================================================== */

#define TRIGGER_1_ADDR           ADDR(0x07, 0x67)
#define TRIGGER_1_SUPER_KEY_LOCK BIT(0)
#define TRIGGER_1_LIGHTBAR       BIT(1)
#define TRIGGER_1_FAN_BOOST      BIT(2)
#define TRIGGER_1_SILENT_MODE    BIT(3)
#define TRIGGER_1_USB_CHARGING   BIT(4)

#define TRIGGER_2_ADDR ADDR(0x07, 0x5D)

/* ========================================================================== */

#define PLATFORM_ID_ADDR  ADDR(0x04, 0x56)
#define POWER_STATUS_ADDR ADDR(0x04, 0x5E)
#define POWER_SOURCE_ADDR ADDR(0x04, 0x90)

#define PL1_ADDR ADDR(0x07, 0x83)
#define PL2_ADDR ADDR(0x07, 0x84)
#define PL4_ADDR ADDR(0x07, 0x85)

/* ========================================================================== */

union qc71_ec_result {
	uint32_t dword;
	struct {
		uint16_t w1;
		uint16_t w2;
	} words;
	struct {
		uint8_t b1;
		uint8_t b2;
		uint8_t b3;
		uint8_t b4;
	} bytes;
};

int __must_check qc71_ec_transaction(uint16_t addr, uint16_t data,
				     union qc71_ec_result *result, bool read);

static inline __must_check int qc71_ec_read(uint16_t addr, union qc71_ec_result *result)
{
	return qc71_ec_transaction(addr, 0, result, true);
}

static inline __must_check int qc71_ec_write(uint16_t addr, uint16_t data)
{
	return qc71_ec_transaction(addr, data, NULL, false);
}

static inline __must_check int ec_write_byte(uint16_t addr, uint8_t data)
{
	return qc71_ec_write(addr, data);
}

static inline __must_check int ec_read_byte(uint16_t addr)
{
	union qc71_ec_result result;
	int err;

	err = qc71_ec_read(addr, &result);

	if (err)
		return err;

	return result.bytes.b1;
}

#endif /* QC71_LAPTOP_EC_H */
