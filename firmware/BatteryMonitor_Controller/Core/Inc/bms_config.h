/*
 * bms_config.h
 *
 *  Created on: Oct 28, 2024
 *      Author: Alex
 */

#ifndef INC_BMS_CONFIG_H_
#define INC_BMS_CONFIG_H_

#include <stdint.h>


//macro to split a two-byte value into individual bytes (little endian) for array definitions
#define TwoBytes(x) ((uint8_t)((x) & 0xFF)), ((uint8_t)(((x) >> 8) & 0xFF))


/**************************************/
/* ------ CALIBRATION SETTINGS ------ */
/**************************************/

//CC2 current gain - default (in prototype sample) 241 targetting 1mA/userA, here 105 targetting 2mA/userA (after calibration)
#define BMS_CAL_CURR_GAIN 105

#ifdef MAIN_CURRENT_CALIBRATION
//CC1 current gain - here 32 for calibration mode
#define BMS_CAL_CC1_GAIN 32
#else
//CC1 current gain - default (in prototype sample) 241 targetting 1mA/userA, here 210 targetting 1mA/userA (after calibration)
#define BMS_CAL_CC1_GAIN 210
#endif


/**********************************/
/* ------ GENERAL SETTINGS ------ */
/**********************************/

//power config (bitfield, see TRM) - default 0x01, here 0x0E (full loop speeds, shutdown on HW OT, shutdown on LFO fault, deepsleep LFO on, sleep mode disabled)
#define BMS_SET_POWER_CONFIG 0x0E

//REGOUT config (bitfield, see TRM) - default 0x0E (or 0x08?), here 0x0E (enabled, 3.3V)
#define BMS_SET_REGOUT_CONFIG 0x0E

//I2C address - default 0x08
#define BMS_SET_I2C_ADDRESS 0x08
//I2C config (bitfield, see TRM) - default 0x3400, here 0x3403 (no short timeouts, 2s low timeout, 2s busy timeout, CRC mode)
#define BMS_SET_I2C_CONFIG 0x3403

//data acquisition config (bitfield, see TRM) - default 0x0000 (TS thermistor mode, continuous CC measurements, all ADCs at highest resolution)
#define BMS_SET_DA_CONFIG 0x0000

//Vcell mode (number of cells present, see TRM for values) - default 0x00, here 0x04 (four cells)
#define BMS_SET_VCELL_MODE 0x04

//default alarm mask (bitfield, see TRM) - default 0xC200, here 0xF204 (safety faults and alerts, shutdown voltage detected, init complete)
#define BMS_SET_DEFAULT_ALARM_MASK 0xF204

//FET options (bitfield, see TRM) - default 0x18, here 0x59 (no CHG detect, FETOFF allowed, FETON disallowed, sleep charge [irrelevant], series FETs, FETs not autonomous initially, no PWM, allow protection recovery command)
#define BMS_SET_FET_OPTIONS 0x59
//charge detector time in 100ms steps (irrelevant here since CHG detect is disabled) - default 1 = 100ms
#define BMS_SET_CHGDET_TIME 1

//cell balancing config (bitfield, see TRM) - default 0x02 (measurement delay 1ms, balancing command allowed)
#define BMS_SET_BAL_CONFIG 0x02
//cell balancing min TS temp threshold in 23.44mV steps - default 255 = 5.977V(?), here 44 = 1.031V (103AT temp ~0°C)
#define BMS_SET_BAL_MINTEMP_TS 44
//cell balancing max TS temp threshold in 23.44mV steps - default 0 = 0V, here 15 = 0.352V (103AT temp ~45°C)
#define BMS_SET_BAL_MAXTEMP_TS 15
//cell balancing max internal temp threshold in °C - default 85°C
#define BMS_SET_BAL_MAXTEMP_INT 85


/*************************************/
/* ------ PROTECTION SETTINGS ------ */
/*************************************/
//enabled protections A (COV, CUV, SCD, OCD1, OCD2, OCC, CURLATCH, REGOUT) - default 0xA1, here 0xFF (all protections)
#define BMS_PROT_ENABLE_A 0xFF
//enabled protections B ([upper 2 reserved], OTD, OTC, UTD, UTC, OTINT, HWD) - default 0x00, here 0x3F (all protections)
#define BMS_PROT_ENABLE_B 0x3F

//discharge FET protection triggers A (CUV, SCD, OCD1, OCD2, HWD, OTD, UTD, OTINT) - default 0xFF (all discharge-related protections)
#define BMS_PROT_DSGFET_A 0xFF
//charge FET protection triggers A (COV, SCD, OCC, [reserved], HWD, OTC, UTC, OTINT) - default 0xEF (all charge-related protections)
#define BMS_PROT_CHGFET_A 0xEF
//both FET protection triggers B ([upper 5 reserved], VREF, VSS, REGOUT) - default 0x06, here 0x07 (all diagnostic protections)
#define BMS_PROT_BOTHFET_B 0x07

//body diode protection threshold in userA - default 64 userA, here 32 userA (intent: keep threshold the same considering userA = 2mA configured above)
#define BMS_PROT_BODYDIODE_THRESHOLD 32

//cell open wire normal-mode check time in FULLSCAN cycles - default 0 (no open wire check), here 32
#define BMS_PROT_COW_NORMAL_TIME 32
//cell open wire sleep-mode config (bitfield, see TRM) - default 0x10 (irrelevant here because no sleep)
#define BMS_PROT_COW_SLEEP_CONFIG 0x10

//host watchdog timeout in varying steps (see TRM) - default 0 = 1s, here 16 = 20s (results in wakeup alert every ~10s)
#define BMS_PROT_HWD_TIMEOUT 16

//cell undervoltage threshold in mV - default 2500mV, here 2550mV (a little extra safety headroom)
#define BMS_PROT_CUV_THRESHOLD 2550
//cell undervoltage delay in ADSCAN cycles - default 10
#define BMS_PROT_CUV_DELAY 10
//cell undervoltage recovery hysteresis (disabled, 50mV, 100mV, 200mV) - default 0x2 = 100mV
#define BMS_PROT_CUV_HYSTERESIS 0x2

//cell overvoltage threshold in mV - default 4200mV, here 4230mV (allow small transients above 4.2V target)
#define BMS_PROT_COV_THRESHOLD 4230
//cell overvoltage delay in ADSCAN cycles - default 10
#define BMS_PROT_COV_DELAY 10
//cell overvoltage recovery hysteresis (disabled, 50mV, 100mV, 200mV) - default 0x2 = 100mV
#define BMS_PROT_COV_HYSTERESIS 0x2

//overcurrent in charge threshold in 2A steps - default 2 = 4A, here 5 = 10A
#define BMS_PROT_OCC_THRESHOLD 5
//overcurrent in charge delay in varying steps (see TRM) - default 58 = 18.605ms
#define BMS_PROT_OCC_DELAY 58

//overcurrent in discharge 1 threshold in 2A steps - default 4 = 8A, here 25 = 50A
#define BMS_PROT_OCD1_THRESHOLD 25
//overcurrent in discharge 1 delay in varying steps (see TRM) - default 6 = 2.745ms
#define BMS_PROT_OCD1_DELAY 6
//overcurrent in discharge 2 threshold in 2A steps - default 3 = 6A, here 16 = 32A
#define BMS_PROT_OCD2_THRESHOLD 16
//overcurrent in discharge 2 delay in varying steps (see TRM) - default 19 = 6.71ms, here 133 = 201ms
#define BMS_PROT_OCD2_DELAY 133

//short circuit in discharge threshold (values see TRM) - default 0 = 10A, here 5 = 100A
#define BMS_PROT_SCD_THRESHOLD 5
//short circuit in discharge delay (values see TRM) - default 1 = 15us, here 2 = 31us
#define BMS_PROT_SCD_DELAY 2

//current latch limit (max number of consecutive auto-recoveries) (values see TRM) - default 0 = no latching, here 2 = 4 retries
#define BMS_PROT_CURR_LATCH_LIMIT 2
//current fault auto-recovery (retry) time in seconds - default 5s
#define BMS_PROT_CURR_RECOVERY_TIME 5

//overtemperature in charge threshold in 4.944mV steps (with 20k pull-up) - default 55 = 0.272V, here 72 = 0.356V (103AT temp ~45°C)
#define BMS_PROT_OTC_THRESHOLD 72
//overtemperature in charge delay in FULLSCAN cycles - default 15
#define BMS_PROT_OTC_DELAY 15
//overtemperature in charge recovery threshold in 4.944mV steps (with 20k pull-up) - default 63 = 0.311V, here 82 = 0.405V (103AT temp ~40°C)
#define BMS_PROT_OTC_RECOVERY 82

//undertemperature in charge threshold in 7.050mV steps (with 20k pull-up) - default 147 = 1.036V (default works for 103AT temp ~0°C)
#define BMS_PROT_UTC_THRESHOLD 147
//undertemperature in charge delay in FULLSCAN cycles - default 15
#define BMS_PROT_UTC_DELAY 15
//undertemperature in charge recovery threshold in 7.050mV steps (with 20k pull-up) - default 134 = 0.945V (default works for 103AT temp ~5°C)
#define BMS_PROT_UTC_RECOVERY 134

//overtemperature in discharge threshold in 4.944mV steps (with 20k pull-up) - default 48 = 0.237V (default works for 103AT temp ~60°C)
#define BMS_PROT_OTD_THRESHOLD 48
//overtemperature in discharge delay in FULLSCAN cycles - default 15
#define BMS_PROT_OTD_DELAY 15
//overtemperature in discharge recovery threshold in 4.944mV steps (with 20k pull-up) - default 55 = 0.272V (default works for 103AT temp ~55°C)
#define BMS_PROT_OTD_RECOVERY 55

//undertemperature in discharge threshold in 7.050mV steps (with 20k pull-up) - default 147 = 1.036V, here 197 = 1.389V (103AT temp ~-20°C)
#define BMS_PROT_UTD_THRESHOLD 197
//undertemperature in discharge delay in FULLSCAN cycles - default 15
#define BMS_PROT_UTD_DELAY 15
//undertemperature in discharge recovery threshold in 7.050mV steps (with 20k pull-up) - default 134 = 0.945V, here 186 = 1.311V (103AT temp ~-15°C)
#define BMS_PROT_UTD_RECOVERY 186

//internal overtemperature threshold in °C - default 105°C
#define BMS_PROT_OTINT_THRESHOLD 105
//internal overtemperature delay in FULLSCAN cycles - default 15
#define BMS_PROT_OTINT_DELAY 15
//internal overtemperature recovery threshold in °C - default 100°C
#define BMS_PROT_OTINT_RECOVERY 100


/********************************/
/* ------ POWER SETTINGS ------ */
/********************************/

//sleep current in userA - default 64 userA (irrelevant here because no sleep)
#define BMS_PWR_SLEEP_CURRENT 64
//time between voltage measurements in sleep mode in seconds - default 5s (irrelevant here because no sleep)
#define BMS_PWR_SLEEP_VOLTAGE_TIME 5
//sleep wake comparator current in 500mA steps - default 1 = 500mA (irrelevant here because no sleep)
#define BMS_PWR_WAKEUP_CURRENT 1

//shutdown cell voltage threshold in mV - default 0mV (disabled), here 2490mV (small transient headroom below discharge limit)
#define BMS_PWR_SHUTDOWN_CELL_VOLTAGE 2490
//shutdown stack voltage threshold in mV - default 0mV (disabled), here 10000mV (discharge limit in 4s configuration)
#define BMS_PWR_SHUTDOWN_STACK_VOLTAGE 10000
//shutdown internal temperature threshold in °C - default 0°C (disabled), here 130°C (backup for hardware overtemperature detection)
#define BMS_PWR_SHUTDOWN_INT_TEMP 130
//auto-shutdown time in minutes - default 0min (disabled)
#define BMS_PWR_SHUTDOWN_AUTO_TIME 0


/***********************************/
/* ------ SECURITY SETTINGS ------ */
/***********************************/

//security config ([upper 5 reserved], SEAL, LOCK_CFG, PERM_SEAL) - default 0x00
#define BMS_SEC_CONFIG 0x00
//full access key step 1 - default 0x0414
#define BMS_SEC_FULLACCESS_KEY_1 0x0414
//full access key step 2 - default 0x3672
#define BMS_SEC_FULLACCESS_KEY_2 0x3672


/***************************************/
/* ------ EXPECTED CONFIG ARRAY ------ */
/***************************************/

//offset of the below "bms_config_main_array" array indices, relative to the "bms_i2c_data_reg_sizes" array indices from bms_i2c.h
#define BMS_CONFIG_MAIN_ARRAY_OFFSET 0x14

//expected state of the config data memory, excluding calibration (0x9014 up to and including 0x905D)
//calibration values (0x9000-0x9013 and 0x9071-0x9072) are preprogrammed by TI and omitted from here, do potential adjustments separately based on factory calibration
extern const uint8_t bms_config_main_array[74];
/*const uint8_t bms_config_main_array[74] = {
    BMS_SET_POWER_CONFIG,
    BMS_SET_REGOUT_CONFIG,
    BMS_SET_I2C_ADDRESS,
    TwoBytes(BMS_SET_I2C_CONFIG),
    TwoBytes(BMS_SET_DA_CONFIG),
    BMS_SET_VCELL_MODE,
    TwoBytes(BMS_SET_DEFAULT_ALARM_MASK),
    BMS_SET_FET_OPTIONS,
    BMS_SET_CHGDET_TIME,
    BMS_SET_BAL_CONFIG,
    BMS_SET_BAL_MINTEMP_TS,
    BMS_SET_BAL_MAXTEMP_TS,
    BMS_SET_BAL_MAXTEMP_INT,
    BMS_PROT_ENABLE_A,
    BMS_PROT_ENABLE_B,
    BMS_PROT_DSGFET_A,
    BMS_PROT_CHGFET_A,
    BMS_PROT_BOTHFET_B,
    TwoBytes(BMS_PROT_BODYDIODE_THRESHOLD),
    BMS_PROT_COW_NORMAL_TIME,
    BMS_PROT_COW_SLEEP_CONFIG,
    BMS_PROT_HWD_TIMEOUT,
    TwoBytes(BMS_PROT_CUV_THRESHOLD),
    BMS_PROT_CUV_DELAY,
    BMS_PROT_CUV_HYSTERESIS,
    TwoBytes(BMS_PROT_COV_THRESHOLD),
    BMS_PROT_COV_DELAY,
    BMS_PROT_COV_HYSTERESIS,
    BMS_PROT_OCC_THRESHOLD,
    BMS_PROT_OCC_DELAY,
    BMS_PROT_OCD1_THRESHOLD,
    BMS_PROT_OCD1_DELAY,
    BMS_PROT_OCD2_THRESHOLD,
    BMS_PROT_OCD2_DELAY,
    BMS_PROT_SCD_THRESHOLD,
    BMS_PROT_SCD_DELAY,
    BMS_PROT_CURR_LATCH_LIMIT,
    BMS_PROT_CURR_RECOVERY_TIME,
    BMS_PROT_OTC_THRESHOLD,
    BMS_PROT_OTC_DELAY,
    BMS_PROT_OTC_RECOVERY,
    BMS_PROT_UTC_THRESHOLD,
    BMS_PROT_UTC_DELAY,
    BMS_PROT_UTC_RECOVERY,
    BMS_PROT_OTD_THRESHOLD,
    BMS_PROT_OTD_DELAY,
    BMS_PROT_OTD_RECOVERY,
    BMS_PROT_UTD_THRESHOLD,
    BMS_PROT_UTD_DELAY,
    BMS_PROT_UTD_RECOVERY,
    BMS_PROT_OTINT_THRESHOLD,
    BMS_PROT_OTINT_DELAY,
    BMS_PROT_OTINT_RECOVERY,
    TwoBytes(BMS_PWR_SLEEP_CURRENT),
    BMS_PWR_SLEEP_VOLTAGE_TIME,
    BMS_PWR_WAKEUP_CURRENT,
    TwoBytes(BMS_PWR_SHUTDOWN_CELL_VOLTAGE),
    TwoBytes(BMS_PWR_SHUTDOWN_STACK_VOLTAGE),
    BMS_PWR_SHUTDOWN_INT_TEMP,
    BMS_PWR_SHUTDOWN_AUTO_TIME,
    BMS_SEC_CONFIG,
    TwoBytes(BMS_SEC_FULLACCESS_KEY_1),
    TwoBytes(BMS_SEC_FULLACCESS_KEY_2)
};*/


#endif /* INC_BMS_CONFIG_H_ */
