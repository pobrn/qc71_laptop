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

/* lightbar control register */
#define LIGHTBAR_CTRL_ADDR       ADDR(0x07, 0x48)
#define LIGHTBAR_CTRL_POWER_SAVE BIT(1)
#define LIGHTBAR_CTRL_S0_OFF     BIT(2)
#define LIGHTBAR_CTRL_S3_OFF     BIT(3)
#define LIGHTBAR_CTRL_RAINBOW    BIT(7)

#define LIGHTBAR_RED_ADDR   ADDR(0x07, 0x49)
#define LIGHTBAR_GREEN_ADDR ADDR(0x07, 0x4A)
#define LIGHTBAR_BLUE_ADDR  ADDR(0x07, 0x4B)

#define PROJ_ID_ADDR ADDR(0x07, 0x40)

/* fan control register */
#define FAN_CTRL_ADDR         ADDR(0x07, 0x51)
#define FAN_CTRL_LEVEL_MASK   GENMASK(2, 0)
#define FAN_CTRL_MAX_LEVEL    7
#define FAN_CTRL_TURBO        BIT(4)
#define FAN_CTRL_AUTO         BIT(5)
#define FAN_CTRL_FAN_BOOST    BIT(6)
#define FAN_CTRL_LEVEL(level) (128 + (level))

#define FAN_RPM_1_ADDR ADDR(0x04, 0x64)
#define FAN_RPM_2_ADDR ADDR(0x04, 0x6C)

#define FAN_PWM_1_ADDR ADDR(0x18, 0x04)
#define FAN_PWM_2_ADDR ADDR(0x18, 0x09)

#define FAN_MAX_PWM 200

#define FAN_TEMP_1_ADDR ADDR(0x04, 0x3e)
#define FAN_TEMP_2_ADDR ADDR(0x04, 0x4f)

#define FAN_MODE_INDEX_ADDR ADDR(0x07, 0xAB)
#define FAN_MODE_INDEX_LOW_MASK GENMASK(3, 0)
#define FAN_MODE_INDEX_HIGH_MASK GENMASK(7, 4)

/* 1st control register */
#define CTRL_1_ADDR                    ADDR(0x07, 0x41)
#define CTRL_1_MANUAL_MODE             BIT(0)
#define CTRL_1_ITE_KBD_EFFECT_REACTIVE BIT(3)
#define CTRL_1_FAN_ABNORMAL            BIT(5)

/* 2nd control register */
#define CTRL_2_ADDR ADDR(0x07, 0x8C)

/* 3rd control register */
#define CTRL_3_ADDR         ADDR(0x07, 0xA5)
#define CTRL_3_PWR_LED_MASK GENMASK(1, 0)
#define CTRL_3_PWR_LED_NONE BIT(1)
#define CTRL_3_PWR_LED_BOTH BIT(0)
#define CTRL_3_PWR_LED_LEFT 0x00
#define CTRL_3_FAN_QUIET    BIT(2)
#define CTRL_3_OVERBOOST    BIT(4)
#define CTRL_3_HIGH_PWR     BIT(7)

/* 4th control register*/
#define CTRL_4_ADDR                   ADDR(0x07, 0xA6)
#define CTRL_4_OVERBOOST_DYN_TEMP_OFF BIT(1)

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
#define SUPPORT_2_MY_BATTERY      BIT(6)

#define SUPPORT_5_ADDR      ADDR(0x07, 0x42)
#define SUPPORT_5_FAN       BIT(5)

#define TRIGGER_2_ADDR ADDR(0x07, 0x5D)

#define TRIGGER_1_ADDR           ADDR(0x07, 0x67)
#define TRIGGER_1_SUPER_KEY_LOCK BIT(0)
#define TRIGGER_1_LIGHTBAR       BIT(1)
#define TRIGGER_1_FAN_BOOST      BIT(2)
#define TRIGGER_1_SILENT_MODE    BIT(3)
#define TRIGGER_1_USB_CHARGING   BIT(4)

#define STATUS_1_ADDR           ADDR(0x07, 0x68)
#define STATUS_1_SUPER_KEY_LOCK BIT(0)
#define STATUS_1_LIGHTBAR       BIT(1)

#define BIOS_INFO_1_ADDR ADDR(0x04, 0x9F)
#define BIOS_INFO_5_ADDR ADDR(0x04, 0x66)

#define POWER_STATUS_ADDR ADDR(0x04, 0x5E)
#define POWER_SOURCE_ADDR ADDR(0x04, 0x90)
#define PLATFORM_ID_ADDR  ADDR(0x04, 0x56)

#define AP_BIOS_BYTE_ADDR ADDR(0x07, 0xA4)
#define AP_BIOS_BYTE_FN_LOCK_SWITCH BIT(3)

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

#define BIOS_CTRL_2_ADDR       ADDR(0x07, 0x82)
#define BIOS_CTRL_2_FAN_V2_NEW BIT(0)
#define BIOS_CTRL_2_FAN_QKEY   BIT(1)
#define BIOS_CTRL_2_FAN_V3     BIT(3)

/* 3rd control register of a different kind */
#define BIOS_CTRL_3_ADDR                   ADDR(0x7, 0xA3)
#define BIOS_CTRL_3_FAN_REDUCED_DUTY_CYCLE BIT(5)
#define BIOS_CTRL_3_FAN_ALWAYS_ON          BIT(6)

#define PL1_ADDR ADDR(0x07, 0x83)
#define PL2_ADDR ADDR(0x07, 0x84)
#define PL4_ADDR ADDR(0x07, 0x85)

#define DEVICE_STATUS_ADDR  ADDR(0x04, 0x7B)
#define DEVICE_STATUS_WIFI_ON BIT(7)
/*
 * BIT(5) is also set/unset depending on the rfkill state,
 * that's possibly for Bluetooth
 */

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

	pr_debug("%s(addr=%#06x): %#04x %#04x %#04x %#04x\n",
		 __func__, (unsigned int) addr, result.bytes.b1,
		 result.bytes.b2, result.bytes.b3, result.bytes.b4);

	return result.bytes.b1;
}

#endif /* QC71_LAPTOP_EC_H */
