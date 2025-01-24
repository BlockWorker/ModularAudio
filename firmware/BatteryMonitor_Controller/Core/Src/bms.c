/*
 * bms.c
 *
 *  Created on: Jan 19, 2025
 *      Author: Alex
 */

#include "bms.h"
#include <math.h>
#include <string.h>


BMS_Status bms_status;
BMS_Measurements bms_measurements;

bool bms_should_deepsleep = false;
bool bms_should_disable_fets = false;

//set to true when an init-complete alert is detected
static bool _bms_detected_initcomp = false;
//set to true when a shutdown voltage fault is detected (cell or stack voltage below shutdown threshold)
static bool _bms_detected_shutdown_voltage = false;
//set to true when a safety fault or alert is detected
static bool _bms_detected_safety_event = false;


//wait for init-complete alert to occur, up to configured maximum wait time
static void _BMS_WaitForInitComp() {
  if (_bms_detected_initcomp) return;

  int i;
  for (i = 0; i < BMS_INITCOMP_WAIT_MAX; i++) {
    //re-run alert check if alert pin is low
    if (HAL_GPIO_ReadPin(BMS_ALERT_N_GPIO_Port, BMS_ALERT_N_Pin) == GPIO_PIN_RESET) {
      BMS_AlertInterrupt();
    }

    if (_bms_detected_initcomp) return;

    HAL_Delay(1);
  }
}

//detect whether the BMS IC is configured to use CRC for communication or not, and configure the driver accordingly - we need to know this for all of our further communication
static HAL_StatusTypeDef _BMS_DetectCRCMode() {
  //assume no-CRC mode at first
  bms_i2c_crc_active = false;

  uint16_t dev_number_read;
  if (BMS_I2C_SubcommandRead(SUBCMD_DEVICE_NUMBER, (uint8_t*)&dev_number_read, 2, BMS_COMM_MAX_TRIES) == HAL_OK) {
    //successfully read something without CRC: check correctness
    if (dev_number_read == BMS_EXPECTED_DEVICE_NUMBER) {
      //correct number: everything is okay, we're in no-CRC mode
      DEBUG_PRINTF("INFO: Detected no-CRC mode\n");
      return HAL_OK;
    } else {
      //incorrect number: shouldn't really happen
      DEBUG_PRINTF("WARNING: BMS CRC mode detection read successfully without CRC but returned wrong value (0x%04X instead of 0x%04X)\n", dev_number_read, BMS_EXPECTED_DEVICE_NUMBER);
    }
  }

  //didn't succeed in no-CRC mode: assume CRC mode now
  bms_i2c_crc_active = true;

  if (BMS_I2C_SubcommandRead(SUBCMD_DEVICE_NUMBER, (uint8_t*)&dev_number_read, 2, BMS_COMM_MAX_TRIES) == HAL_OK) {
    //successfully read something with CRC: check correctness
    if (dev_number_read == BMS_EXPECTED_DEVICE_NUMBER) {
      //correct number: everything is okay, we're in CRC mode
      DEBUG_PRINTF("INFO: Detected CRC mode\n");
      return HAL_OK;
    } else {
      //incorrect number: shouldn't really happen
      DEBUG_PRINTF("WARNING: BMS CRC mode detection read successfully with CRC but returned wrong value (0x%04X instead of 0x%04X)\n", dev_number_read, BMS_EXPECTED_DEVICE_NUMBER);
    }
  }

  //if we're here, both reads failed and/or we got incorrect values
  return HAL_ERROR;
}

//calculate the temperature in °C from the raw thermistor ADC measurement
static int16_t _BMS_GetThermistorTemp(int16_t adc_value) {
  //reading 0 or below means a short to ground or a measurement error
  if (adc_value <= 0) {
    DEBUG_PRINTF("ERROR: BMS Thermistor conversion got invalid ADC reading %d\n", adc_value);
    return BMS_ERROR_TEMPERATURE;
  }

  //measured voltage across thermistor, as a fraction of the IC's pull-up reference voltage
  float measured_ratio = BMS_THERM_LSB_VALUE * (float)adc_value;

  //calculate thermistor resistance (bottom resistor of the measurement voltage divider)
  float therm_res = BMS_THERM_PULLUP / (1.0f / measured_ratio - 1.0f);

  //take logarithm of resistance for next step
  float therm_res_ln = logf(therm_res);

  //calculate temperature using Steinhart-Hart approximation
  float temp = 1.0f / (BMS_THERM_COEFF_A + therm_res_ln * (BMS_THERM_COEFF_B + therm_res_ln * (BMS_THERM_COEFF_C + therm_res_ln * BMS_THERM_COEFF_D))) - 273.15f;

  //return rounded result
  return (int16_t)roundf(temp);
}

//enter CFGUPDATE mode
static HAL_StatusTypeDef _BMS_EnterCFGUPDATE() {
  ReturnOnError(BMS_I2C_SubcommandOnly(SUBCMD_SET_CFGUPDATE, BMS_COMM_MAX_TRIES));

  //update status repeatedly with small delays until we see the mode update
  int try;
  for (try = 0; try < BMS_COMM_MAX_TRIES; try++) {
    HAL_Delay(1);

    ReturnOnError(BMS_UpdateStatus());

    if (bms_status._cfgupdate_bit) {
      //mode change successful
      return HAL_OK;
    }
  }

  DEBUG_PRINTF("ERROR: CFGUPDATE status bit not correct after enter attempt\n");
  return HAL_ERROR;
}

//exit CFGUPDATE mode
static HAL_StatusTypeDef _BMS_ExitCFGUPDATE() {
  _bms_detected_initcomp = false;

  ReturnOnError(BMS_I2C_SubcommandOnly(SUBCMD_EXIT_CFGUPDATE, BMS_COMM_MAX_TRIES));

  //update status repeatedly with small delays until we see the mode update
  int try;
  for (try = 0; try < BMS_COMM_MAX_TRIES; try++) {
    HAL_Delay(1);

    if (BMS_UpdateStatus() != HAL_OK) {
      //CRC mode might have changed: detect and try again
      ReturnOnError(_BMS_DetectCRCMode());
      continue;
    }

    if (!bms_status._cfgupdate_bit) {
      //mode change successful: run CRC detection to ensure correct mode
      ReturnOnError(_BMS_DetectCRCMode());
      //wait for initcomp and finish
      _BMS_WaitForInitComp();
      return HAL_OK;
    }
  }

  DEBUG_PRINTF("ERROR: CFGUPDATE status bit not correct after exit attempt\n");
  return HAL_ERROR;
}

//check the data memory configuration, overwrite all incorrect values, and check again
static HAL_StatusTypeDef _BMS_CheckAndApplyConfig() {
  //buffer sized for the contiguous section of data memory - we don't care about the separately located cell delta calibration values
  uint8_t read_buffer[0x5E] __attribute__((aligned (2))) = { 0 };

  //read contiguous data memory
  ReturnOnError(BMS_I2C_DataMemoryRead(MEMADDR_CAL_CELL1_GAIN, read_buffer, 0x5E, BMS_COMM_MAX_TRIES));

  //check if everything is already correct - in that case, we can abort early
  bool current_gain_correct = (*(uint16_t*)(read_buffer + 0x06) == BMS_CAL_CURR_GAIN);
  if (memcmp(read_buffer + BMS_CONFIG_MAIN_ARRAY_OFFSET, bms_config_main_array, sizeof(bms_config_main_array)) == 0 && current_gain_correct) {
    DEBUG_PRINTF("INFO: BMS Config: Already correct at init\n");
    return HAL_OK;
  }

  //enter config update mode and start looking for differences that need to be overwritten
  ReturnOnError(_BMS_EnterCFGUPDATE());
  HAL_StatusTypeDef res = HAL_OK;

  //start with current gain (separate)
  if (!current_gain_correct) {
    //write configured current gain
    uint8_t curr_gain_config[2] = { TwoBytes(BMS_CAL_CURR_GAIN) };
    if (BMS_I2C_DataMemoryWrite(MEMADDR_CAL_CURR_GAIN, curr_gain_config, 2, BMS_COMM_MAX_TRIES) != HAL_OK) {
      DEBUG_PRINTF("ERROR: BMS Config: Current gain write failed\n");
      res = HAL_ERROR;
    }
  }

  //handle main config array
  int i;
  for (i = 0; i < sizeof(bms_config_main_array); i++) {
    if (res != HAL_OK) break;

    uint8_t config_index = i; //index of current byte in main config array
    uint8_t mem_index = BMS_CONFIG_MAIN_ARRAY_OFFSET + i; //index of current byte in data memory

    if (read_buffer[mem_index] != bms_config_main_array[config_index]) {
      //difference detected: get size of corresponding register
      uint8_t reg_size = bms_i2c_data_reg_sizes[mem_index];
      if (reg_size == 0) {
        //size zero means we're on the second byte of two-byte register: decrement indices and read new size
        --config_index;
        reg_size = bms_i2c_data_reg_sizes[--mem_index];
        if (reg_size != 2) {
          //new size is not the expected 2 bytes: something went wrong
          DEBUG_PRINTF("ERROR: BMS Config: Mem index 0x%02X reduced to 0x%02X, expected size 2 but got size %u\n", mem_index + 1, mem_index, reg_size);
          res = HAL_ERROR;
          continue;
        }
      }
      //perform actual write
      if (BMS_I2C_DataMemoryWrite((BMS_I2C_DataMemAddress)((uint16_t)MEMADDR_CAL_CELL1_GAIN + mem_index), bms_config_main_array + config_index, reg_size, BMS_COMM_MAX_TRIES) != HAL_OK) {
        DEBUG_PRINTF("ERROR: BMS Config: Failed to write mem index 0x%02X (size %u)\n", mem_index, reg_size);
        res = HAL_ERROR;
      }
    }
  }

  //exit config update mode
  ReturnOnError(_BMS_ExitCFGUPDATE());

  if (res == HAL_OK) {
    //re-read data memory to check the written config
    if (BMS_I2C_DataMemoryRead(MEMADDR_CAL_CELL1_GAIN, read_buffer, 0x5E, BMS_COMM_MAX_TRIES) == HAL_OK) {
      current_gain_correct = (*(uint16_t*)(read_buffer + 0x06) == BMS_CAL_CURR_GAIN);
      if (memcmp(read_buffer + BMS_CONFIG_MAIN_ARRAY_OFFSET, bms_config_main_array, sizeof(bms_config_main_array)) == 0 && current_gain_correct) {
        //we're good: finish
        return HAL_OK;
      } else {
        //read successful but wrong config
        DEBUG_PRINTF("ERROR: BMS Config: Memory readback returned incorrect data\n");
        res = HAL_ERROR;
      }
    } else {
      //read failed: error and re-check CRC mode just in case
      res = HAL_ERROR;
      DEBUG_PRINTF("ERROR: BMS Config: Memory readback failed\n");
      _BMS_DetectCRCMode();
    }
  } else {
    //write failed: re-check CRC mode just in case
    _BMS_DetectCRCMode();
  }

  return res;
}


HAL_StatusTypeDef BMS_UpdateStatus() {
  uint16_t status_read;
  ReturnOnError(BMS_I2C_DirectCommandRead(DIRCMD_BATTERY_STATUS, (uint8_t*)&status_read, 2, BMS_COMM_MAX_TRIES));

  HAL_StatusTypeDef res = HAL_OK;

  //read raw mode bits
  bms_status._sleep_bit = ((status_read & 0x8000) != 0);
  bms_status._deepsleep_bit = ((status_read & 0x4000) != 0);
  bms_status._cfgupdate_bit = ((status_read & 0x0020) != 0);
  bms_status._sleep_en_bit = ((status_read & 0x0040) != 0);

  //extract security state bits to determine mode and sealed status
  uint16_t sec_val = (status_read >> 10) & 0x3;
  if (sec_val == 1 || sec_val == 3) {
    //valid security state: decode sealed status and determine mode based on other bits
    bms_status.sealed = (sec_val == 3);
    //mode precedence based on bits: cfgupdate > deepsleep > sleep > normal
    if (bms_status._cfgupdate_bit) {
      bms_status.mode = BMSMODE_CFGUPDATE;
    } else if (bms_status._deepsleep_bit) {
      bms_status.mode = BMSMODE_DEEPSLEEP;
    } else if (bms_status._sleep_bit) {
      bms_status.mode = BMSMODE_SLEEP;
    } else {
      bms_status.mode = BMSMODE_NORMAL;
    }
  } else {
    //invalid security state: default to unknown mode
    bms_status.sealed = false;
    bms_status.mode = BMSMODE_UNKNOWN;
  }

  bms_status.fets_enabled = ((status_read & 0x0100) != 0);
  bms_status.dsg_state = ((status_read & 0x0004) != 0);
  bms_status.chg_state = ((status_read & 0x0008) != 0);

  //check whether we have any safety alerts - if yes, read which ones, otherwise set alert status to zero
  uint8_t safety_read;
  if ((status_read & 0x2000) > 0) {
    //read and populate actual status
    if (BMS_I2C_DirectCommandRead(DIRCMD_SAFETY_ALERT_A, &safety_read, 1, BMS_COMM_MAX_TRIES) == HAL_OK) {
      bms_status.safety_alerts._a = safety_read;
    } else {
      //read failed: assume "all alerts"
      DEBUG_PRINTF("ERROR: BMS Status update: Safety alerts A read failed\n");
      bms_status.safety_alerts._a = 0xFF;
      res = HAL_ERROR;
    }
    if (BMS_I2C_DirectCommandRead(DIRCMD_SAFETY_ALERT_B, &safety_read, 1, BMS_COMM_MAX_TRIES) == HAL_OK) {
      bms_status.safety_alerts._b = safety_read;
    } else {
      //read failed: assume "all alerts"
      DEBUG_PRINTF("ERROR: BMS Status update: Safety alerts B read failed\n");
      bms_status.safety_alerts._b = 0xFF;
      res = HAL_ERROR;
    }
  } else {
    //no alerts
    bms_status.safety_alerts._a = bms_status.safety_alerts._b = 0;
  }

  //handling of faults equivalent to alerts above
  if ((status_read & 0x1000) > 0) {
    //read and populate actual faults
    if (BMS_I2C_DirectCommandRead(DIRCMD_SAFETY_STATUS_A, &safety_read, 1, BMS_COMM_MAX_TRIES) == HAL_OK) {
      bms_status.safety_faults._a = safety_read;
    } else {
      //read failed: assume "all faults"
      DEBUG_PRINTF("ERROR: BMS Status update: Safety faults A read failed\n");
      bms_status.safety_faults._a = 0xFF;
      res = HAL_ERROR;
    }
    if (BMS_I2C_DirectCommandRead(DIRCMD_SAFETY_STATUS_B, &safety_read, 1, BMS_COMM_MAX_TRIES) == HAL_OK) {
      bms_status.safety_faults._b = safety_read;
    } else {
      //read failed: assume "all faults"
      DEBUG_PRINTF("ERROR: BMS Status update: Safety faults B read failed\n");
      bms_status.safety_faults._b = 0xFF;
      res = HAL_ERROR;
    }
  } else {
    //no faults
    bms_status.safety_faults._a = bms_status.safety_faults._b = 0;
  }

  return res;
}

HAL_StatusTypeDef BMS_UpdateMeasurements(bool include_temps) {
  //byte read buffer, aligned for up to int64 reads
  uint8_t read_buffer_u8[12] __attribute__((aligned (8))) = { 0 };
  //pointer for easy signed 16-bit reads
  int16_t* read_buffer_s16 = (int16_t*)read_buffer_u8;

  HAL_StatusTypeDef res = HAL_OK;

  //read cell voltages
  if (BMS_I2C_DirectCommandRead(DIRCMD_CELL1_VOLTAGE, read_buffer_u8, 10, BMS_COMM_MAX_TRIES) == HAL_OK) {
    bms_measurements.voltage_cell_1 = read_buffer_s16[0];
    bms_measurements.voltage_cell_2 = read_buffer_s16[1];
    bms_measurements.voltage_cell_3 = read_buffer_s16[2];
    bms_measurements.voltage_cell_4 = read_buffer_s16[3];
    bms_measurements.voltage_cell_5 = read_buffer_s16[4];
  } else {
    //read failed: default to error voltage
    DEBUG_PRINTF("ERROR: BMS measurement update: Cell voltage read failed\n");
    bms_measurements.voltage_cell_1 = BMS_ERROR_VOLTAGE;
    bms_measurements.voltage_cell_2 = BMS_ERROR_VOLTAGE;
    bms_measurements.voltage_cell_3 = BMS_ERROR_VOLTAGE;
    bms_measurements.voltage_cell_4 = BMS_ERROR_VOLTAGE;
    bms_measurements.voltage_cell_5 = BMS_ERROR_VOLTAGE;
    res = HAL_ERROR;
  }

  //read stack voltage
  if (BMS_I2C_DirectCommandRead(DIRCMD_STACK_VOLTAGE, read_buffer_u8, 2, BMS_COMM_MAX_TRIES) == HAL_OK) {
    bms_measurements.voltage_stack = *(uint16_t*)read_buffer_u8;
  } else {
    //read failed: default to error stack voltage
    DEBUG_PRINTF("ERROR: BMS measurement update: Stack voltage read failed\n");
    bms_measurements.voltage_stack = BMS_ERROR_STACKVOLTAGE;
    res = HAL_ERROR;
  }

  //read current - unnecessary if power off
  if (pwrsw_status) {
    if (BMS_I2C_DirectCommandRead(DIRCMD_CURRENT, read_buffer_u8, 2, BMS_COMM_MAX_TRIES) == HAL_OK) {
      bms_measurements.current = BMS_CONV_MA_PER_CUR_LSB * (int32_t)read_buffer_s16[0];
    } else {
      //read failed: default to error current
      DEBUG_PRINTF("ERROR: BMS measurement update: Current read failed\n");
      bms_measurements.current = BMS_ERROR_CURRENT;
      res = HAL_ERROR;
    }
  }

  //read charge and integration time - unnecessary if power off
  if (pwrsw_status) {
    if (BMS_I2C_SubcommandRead(SUBCMD_PASSQ, read_buffer_u8, 12, BMS_COMM_MAX_TRIES) == HAL_OK) {
      bms_measurements.accumulated_charge = BMS_CONV_MAS_PER_CHG_LSB * (*(int64_t*)read_buffer_u8);
      bms_measurements.charge_accumulation_time = *(uint32_t*)(read_buffer_u8 + 8);
    } else {
      //read failed: default to error charge and time
      DEBUG_PRINTF("ERROR: BMS measurement update: Charge and time read failed\n");
      bms_measurements.accumulated_charge = BMS_ERROR_CHARGE;
      bms_measurements.charge_accumulation_time = BMS_ERROR_TIME;
      res = HAL_ERROR;
    }
  }

  if (include_temps) {
    //read battery temp
    if (BMS_I2C_DirectCommandRead(DIRCMD_TS_MEASUREMENT, read_buffer_u8, 2, BMS_COMM_MAX_TRIES) == HAL_OK) {
      bms_measurements.bat_temp = _BMS_GetThermistorTemp(read_buffer_s16[0]);
    } else {
      //read failed: default to error temp
      DEBUG_PRINTF("ERROR: BMS measurement update: Thermistor read failed\n");
      bms_measurements.bat_temp = BMS_ERROR_TEMPERATURE;
      res = HAL_ERROR;
    }

    //read internal temp
    if (BMS_I2C_DirectCommandRead(DIRCMD_INT_TEMP, read_buffer_u8, 2, BMS_COMM_MAX_TRIES) == HAL_OK) {
      bms_measurements.internal_temp = read_buffer_s16[0];
    } else {
      //read failed: default to error temp
      DEBUG_PRINTF("ERROR: BMS measurement update: Internal temp read failed\n");
      bms_measurements.internal_temp = BMS_ERROR_TEMPERATURE;
      res = HAL_ERROR;
    }
  }

  return res;
}


HAL_StatusTypeDef BMS_SetFETControl(bool fets_enabled) {
  //make sure we have the latest state of the FET control bit
  ReturnOnError(BMS_UpdateStatus());

  //if we already have the desired state, we don't need to do anything
  if (bms_status.fets_enabled == fets_enabled) {
    return HAL_OK;
  }

  //send toggle command and read status again
  ReturnOnError(BMS_I2C_SubcommandOnly(SUBCMD_FET_ENABLE, BMS_COMM_MAX_TRIES));
  ReturnOnError(BMS_UpdateStatus());

  //check if we have the right status and return
  if (bms_status.fets_enabled != fets_enabled) {
    DEBUG_PRINTF("ERROR: BMS FET enable readback incorrect (%u instead of %u)\n", bms_status.fets_enabled, fets_enabled);
    return HAL_ERROR;
  }
  return HAL_OK;
}

HAL_StatusTypeDef BMS_SetFETForceOff(bool force_off_dsg, bool force_off_chg) {
  //calculate desired state of the FET control register
  uint8_t desired_state = (force_off_dsg ? 0x04 : 0x00) | (force_off_chg ? 0x08 : 0x00);

  //read initial state
  uint8_t read_state;
  ReturnOnError(BMS_I2C_DirectCommandRead(DIRCMD_FET_CONTROL, &read_state, 1, BMS_COMM_MAX_TRIES));
  if (read_state == desired_state) {
    //already correct state: return
    return HAL_OK;
  }

  //write FET control
  ReturnOnError(BMS_I2C_DirectCommandWrite(DIRCMD_FET_CONTROL, &desired_state, 1, BMS_COMM_MAX_TRIES));

  //read back FET control and check for correctness
  ReturnOnError(BMS_I2C_DirectCommandRead(DIRCMD_FET_CONTROL, &read_state, 1, BMS_COMM_MAX_TRIES));
  if (read_state != desired_state) {
    DEBUG_PRINTF("ERROR: BMS FET control readback incorrect (0x%02X instead of 0x%02X)\n", read_state, desired_state);
    return HAL_ERROR;
  }

  //do a final status update to reflect new FET states
  return BMS_UpdateStatus();
}


HAL_StatusTypeDef BMS_EnterDeepSleep() {
  //make sure we have the latest state of the deepsleep bit
  ReturnOnError(BMS_UpdateStatus());

  //if we're already in deepsleep, we don't need to do anything
  if (bms_status._deepsleep_bit) {
    return HAL_OK;
  }

  //send enter command twice and read status again
  ReturnOnError(BMS_I2C_SubcommandOnly(SUBCMD_DEEPSLEEP, BMS_COMM_MAX_TRIES));
  ReturnOnError(BMS_I2C_SubcommandOnly(SUBCMD_DEEPSLEEP, BMS_COMM_MAX_TRIES));
  ReturnOnError(BMS_UpdateStatus());

  //check if we have the right status and return
  if (!bms_status._deepsleep_bit) {
    DEBUG_PRINTF("ERROR: BMS failed to enter deepsleep mode\n");
    return HAL_ERROR;
  }
  return HAL_OK;
}

HAL_StatusTypeDef BMS_ExitDeepSleep() {
  //make sure we have the latest state of the deepsleep bit
  ReturnOnError(BMS_UpdateStatus());

  //if we're already not in deepsleep, we don't need to do anything
  if (!bms_status._deepsleep_bit) {
    return HAL_OK;
  }

  //send exit command
  _bms_detected_initcomp = false;
  ReturnOnError(BMS_I2C_SubcommandOnly(SUBCMD_EXIT_DEEPSLEEP, BMS_COMM_MAX_TRIES));

  //wait for initcomp, then read status again
  _BMS_WaitForInitComp();
  ReturnOnError(BMS_UpdateStatus());

  //check if we have the right status and return
  if (bms_status._deepsleep_bit) {
    DEBUG_PRINTF("ERROR: BMS failed to exit deepsleep mode\n");
    return HAL_ERROR;
  }
  return HAL_OK;
}


HAL_StatusTypeDef BMS_EnterShutdown(bool instant) {
  //send first shutdown command, trying both CRC modes if necessary
  if (BMS_I2C_SubcommandOnly(SUBCMD_SHUTDOWN, BMS_COMM_MAX_TRIES) != HAL_OK) {
    //failed: try other CRC mode
    DEBUG_PRINTF("WARNING: BMS shutdown command failed, trying other CRC mode...\n");
    bms_i2c_crc_active = !bms_i2c_crc_active;
    if (BMS_I2C_SubcommandOnly(SUBCMD_SHUTDOWN, BMS_COMM_MAX_TRIES) != HAL_OK) {
      //failed again: return to previous mode, abort
      DEBUG_PRINTF("ERROR: BMS shutdown command failed in both CRC modes\n");
      bms_i2c_crc_active = !bms_i2c_crc_active;
      return HAL_ERROR;
    }
  }

  //send one or two more shutdown commands (depending on instant or not)
  ReturnOnError(BMS_I2C_SubcommandOnly(SUBCMD_SHUTDOWN, BMS_COMM_MAX_TRIES));
  if (instant) {
    ReturnOnError(BMS_I2C_SubcommandOnly(SUBCMD_SHUTDOWN, BMS_COMM_MAX_TRIES));
  }

  if (instant) {
    //instant shutdown: stop everything and wait for shutdown to happen
    Error_Handler();
  }

  return HAL_OK;
}


void BMS_AlertInterrupt() {
  uint16_t alarm_status;
  if (BMS_I2C_DirectCommandRead(DIRCMD_ALARM_STATUS, (uint8_t*)&alarm_status, 2, BMS_COMM_MAX_TRIES) != HAL_OK) return;

  //INITCOMP bit (init complete after powerup, deepsleep exit, or cfgupdate exit)
  if ((alarm_status & 0x0004) != 0) {
    _bms_detected_initcomp = true;
  }

  //SHUTV bit (shutdown voltage threshold crossed (cell or stack)
  if ((alarm_status & 0x0200) != 0) {
    _bms_detected_shutdown_voltage = true;
  }

  //safety status or alert bits
  if ((alarm_status & 0xF000) != 0) {
    _bms_detected_safety_event = true;
  }

  //clear alarm flags
  BMS_I2C_DirectCommandWrite(DIRCMD_ALARM_STATUS, (uint8_t*)&alarm_status, 2, BMS_COMM_MAX_TRIES);
}


//actual initialisation attempt, may be retried multiple times by main init function
static HAL_StatusTypeDef _BMS_InitAttempt() {
  _bms_detected_initcomp = false;
  _bms_detected_safety_event = false;
  _bms_detected_shutdown_voltage = false;

  //get current CRC mode and initialise config
  ReturnOnError(_BMS_DetectCRCMode());
  ReturnOnError(_BMS_CheckAndApplyConfig());

  //get first measurement data
  ReturnOnError(BMS_UpdateMeasurements(true));

  //apply desired force-off status and enable FET control - also updates status
  ReturnOnError(BMS_SetFETForceOff(bms_should_disable_fets, bms_should_disable_fets));
  ReturnOnError(BMS_SetFETControl(true));

  //enter/exit deepsleep depending on desired state
  if (bms_should_deepsleep && !bms_status._deepsleep_bit) {
    ReturnOnError(BMS_EnterDeepSleep());
  } else if (!bms_should_deepsleep && bms_status._deepsleep_bit) {
    ReturnOnError(BMS_ExitDeepSleep());
  }

  return HAL_OK;
}

HAL_StatusTypeDef BMS_Init() {
  int try;
  for (try = 0; try < BMS_INIT_MAX_TRIES; try++) {
    //make attempt to initialise, return if successful
    if (_BMS_InitAttempt() == HAL_OK) {
      return HAL_OK;
    }

    DEBUG_PRINTF("WARNING: BMS Init attempt %u/%u failed\n", try + 1, BMS_INIT_MAX_TRIES);

    //reset BMS on failure
    if (BMS_I2C_SubcommandOnly(SUBCMD_RESET, BMS_COMM_MAX_TRIES) != HAL_OK) {
      //reset failed: retry using other CRC mode, return if fail again
      bms_i2c_crc_active = !bms_i2c_crc_active;
      ReturnOnError(BMS_I2C_SubcommandOnly(SUBCMD_RESET, BMS_COMM_MAX_TRIES));
    }
  }

  return HAL_ERROR;
}

void BMS_LoopUpdate(uint32_t loop_count) {
  //update alerts if interrupt was somehow missed
  if (HAL_GPIO_ReadPin(BMS_ALERT_N_GPIO_Port, BMS_ALERT_N_Pin) == GPIO_PIN_RESET) {
    BMS_AlertInterrupt();
  }

  //alert/fault handling
  if (_bms_detected_safety_event) {
    _bms_detected_safety_event = false;
    //TODO
  }
  if (_bms_detected_shutdown_voltage) {
    _bms_detected_shutdown_voltage = false;
    //TODO
  }

  //measurement update
  if (loop_count % BMS_LOOP_PERIOD_MEASUREMENTS == 0) {
    bool temps = (loop_count % BMS_LOOP_PERIOD_TEMPERATURES == 0);
    BMS_UpdateMeasurements(temps);
  }

  //status update and handling
  if (loop_count % BMS_LOOP_PERIOD_STATUS == 0) {
    BMS_UpdateStatus();

    //if we're somehow in CFGUPDATE: exit it
    if (bms_status._cfgupdate_bit) {
      //TODO note: resets PASSQ, may need to handle that stuff later
      _BMS_ExitCFGUPDATE();
    }

    //if FETs should be forced off, do that before mode changes
    if (bms_should_disable_fets) {
      BMS_SetFETForceOff(true, true);
    }

    //do mode change into or out of deepsleep depending on requirement
    if (bms_should_deepsleep && !bms_status._deepsleep_bit) {
      BMS_EnterDeepSleep();
    } else if (!bms_should_deepsleep && bms_status._deepsleep_bit) {
      BMS_ExitDeepSleep();
    }

    //if at least one FET is off and they should not be forced off, unforce them after mode changes
    if (!bms_should_disable_fets && (!bms_status.dsg_state || !bms_status.chg_state)) {
      BMS_SetFETForceOff(false, false);
    }

    //if FET control is disabled, enable it (should always be enabled)
    if (!bms_status.fets_enabled) {
      BMS_SetFETControl(true);
    }

    //if sleep is enabled, disable it (should always be disabled)
    if (bms_status._sleep_en_bit) {
      BMS_I2C_SubcommandOnly(SUBCMD_SLEEP_DISABLE, BMS_COMM_MAX_TRIES);
    }
  }

  //debug status printout - TODO remove later?
  if (loop_count % 200 == 1) {
    char* mode_str =
        (bms_status.mode == BMSMODE_NORMAL) ? "Normal" :
        (bms_status.mode == BMSMODE_SLEEP) ? "Sleep" :
        (bms_status.mode == BMSMODE_DEEPSLEEP) ? "DeepSleep" :
        (bms_status.mode == BMSMODE_CFGUPDATE) ? "ConfigUpdate" :
        "Unknown";
    DEBUG_PRINTF("------------------------------\n");
    DEBUG_PRINTF("Mode: %s, Sealed: %u, Alerts: 0x%04X, Faults: 0x%04X\n", mode_str, bms_status.sealed, bms_status.safety_alerts._all, bms_status.safety_faults._all);
    DEBUG_PRINTF("FETs enabled: %u, FET state: DSG %u CHG %u\n", bms_status.fets_enabled, bms_status.dsg_state, bms_status.chg_state);
    DEBUG_PRINTF("Cell voltages: %d %d %d %d %d, Stack voltage: %u, Current: %ld\n", bms_measurements.voltage_cell_1, bms_measurements.voltage_cell_2,
                 bms_measurements.voltage_cell_3, bms_measurements.voltage_cell_4, bms_measurements.voltage_cell_5, bms_measurements.voltage_stack, bms_measurements.current);
    DEBUG_PRINTF("Temps: Bat %d Int %d, Charge: %lld, Time: %lu\n", bms_measurements.bat_temp, bms_measurements.internal_temp, bms_measurements.accumulated_charge,
                 bms_measurements.charge_accumulation_time);
  }
}

