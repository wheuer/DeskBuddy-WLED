#ifndef DESK_BUDDY_H
#define DESK_BUDDY_H

#define DESK_BUDDY_MODULE_ID 99 // No real definition for module ids so just a random number hopefully valid

#define STATUS_UPDATE_INTERVAL      2000 // ms
#define ABSOLUTE_MAX_CURRENT        2000 // Technically we could hanlde up to 4A, but thats quite bright and HOT 
#define TEMPERATURE_BEFORE_CUTOFF   50
#define TMEPERATURE_HYST            10
#define SAFE_DEFAULT_BRIGHTNESS     15

#define LED_TOP_PIN       26
#define LED_BOT_PIN       27
#define VLED_LOAD_SW_PIN  25

#define TOUCH_PANEL_DEFAULT_BRIGHTNESS  0x8
#define LED_DRIVER_I2C_ADDR             0x14
#define DEVICE_CONFIG0_REG              0x00 
#define LED_CONFIG_REG                  0x02
#define BANK_BRIGHTNESS_REG             0x03 

#define BUTTON_PRESS_HYST               1000 // minimum ms between acknowledged presses
#define BUTTON_BRIGHTNESS_STEP_SIZE     5
#define BUTTON_TOP                      0x1
#define BUTTON_MID_TOP                  0x2
#define BUTTON_MID_BOT                  0x4
#define BUTTON_BOT                      0x8

#define TOUCH_SENSE_I2C_ADDR  0x1B
#define KEY_STATUS_REG        0x03

#define POWER_METER_I2C_ADDR          0x40
#define REGISTER_CALIBRATION_ADDR     0x05
#define REGISTER_SHUNT_VOLTAGE_ADDR   0x01
#define REGISTER_CURRENT_ADDR         0x04 // Returned in A
#define REGISTER_BUS_VOLTAGE_ADDR     0x02

#define POWER_METER_SHUNT_RESISTANCE    (10/1000)
#define POWER_METER_MINIMUM_CURRENT_LSB 0.00016 // Based on a theoretical maximum current of ~5A, expected maximum is around 4.5A
#define POWER_METER_CALIBRATION_VALUE   3200 // (0.00512 / (POWER_METER_SHUNT_RESISTANCE * POWER_METER_MINIMUM_CURRENT_LSB))

#define USB_PD_I2C_ADDR             0x20
#define ACTIVE_CONTRACT_PDO_REG     0x34

#define TEMP_SENSOR_ONE_I2C_ADDR    0x48
#define TEMP_SENSOR_TWO_I2C_ADDR    0x49
#define TEMP_RESULT_REG             0x00

#endif // DESK_BUDDY_H