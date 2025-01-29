/*
 * bms.c
 *
 *  Created on: Jan 19, 2025
 *      Author: Alex
 */

#include "bms.h"
#include "bat_calculations.h"
#include <math.h>
#include <string.h>


BMS_Status bms_status;
BMS_Measurements bms_measurements;

//state-of-charge calculation status and results
//battery health fraction (in [0, 1]) - may be modified externally, will apply on next update
float bms_soc_battery_health = 1.0f;
//precision level of current calculated values
BMS_SoC_PrecisionLevel bms_soc_precisionlevel = SOC_VOLTAGE_ONLY;
//calculated state-of-charge in terms of energy (in Wh) and fraction of full (in [0, 1])
float bms_soc_energy = NAN;
float bms_soc_fraction = NAN;

bool bms_should_deepsleep = false;
bool bms_should_disable_fets = false;

//set to true when an init-complete alert is detected
static bool _bms_detected_initcomp = false;
//set to true when a shutdown voltage fault is detected (cell or stack voltage below shutdown threshold)
static bool _bms_detected_shutdown_voltage = false;
//set to true when a safety fault or alert is detected
static bool _bms_detected_safety_event = false;

//charge reference point in Ah (actual charge of a single cell when BMS charge register is zero), for SOC_CHARGE_ESTIMATED and SOC_CHARGE_FULL modes
static float _bms_soc_charge_reference = 0.0f;

//bit mask of which cells are currently balancing
static uint8_t _bms_bal_active_cells = 0;


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
  //reset state-of-charge calculation to voltage-only mode, due to CFGUPDATE resetting integrated charge
  bms_soc_precisionlevel = SOC_VOLTAGE_ONLY;

  //disable all cell balancing
  _bms_bal_active_cells = 0;
  ReturnOnError(BMS_I2C_SubcommandWrite(SUBCMD_CB_ACTIVE_CELLS, &_bms_bal_active_cells, 1, BMS_COMM_MAX_TRIES));

  //send actual command
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
  //reset state-of-charge calculation to voltage-only mode, due to CFGUPDATE resetting integrated charge
  bms_soc_precisionlevel = SOC_VOLTAGE_ONLY;

  //send actual command
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
  bool cc1_gain_correct = (*(uint16_t*)(read_buffer + 0x0A) == BMS_CAL_CC1_GAIN);
  bool current_gain_correct = (*(uint16_t*)(read_buffer + 0x06) == BMS_CAL_CURR_GAIN);
  if (memcmp(read_buffer + BMS_CONFIG_MAIN_ARRAY_OFFSET, bms_config_main_array, sizeof(bms_config_main_array)) == 0 && cc1_gain_correct && current_gain_correct) {
    DEBUG_PRINTF("INFO: BMS Config: Already correct at init\n");
    return HAL_OK;
  }

  //enter config update mode and start looking for differences that need to be overwritten
  ReturnOnError(_BMS_EnterCFGUPDATE());
  HAL_StatusTypeDef res = HAL_OK;

  //start with current gains (separate)
  if (!cc1_gain_correct) {
    //write configured CC1 current gain
    uint8_t cc1_gain_config[2] = { TwoBytes(BMS_CAL_CC1_GAIN) };
    if (BMS_I2C_DataMemoryWrite(MEMADDR_CAL_CC1_GAIN, cc1_gain_config, 2, BMS_COMM_MAX_TRIES) != HAL_OK) {
      DEBUG_PRINTF("ERROR: BMS Config: CC1 gain write failed\n");
      res = HAL_ERROR;
    }
  }
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
      cc1_gain_correct = (*(uint16_t*)(read_buffer + 0x0A) == BMS_CAL_CC1_GAIN);
      current_gain_correct = (*(uint16_t*)(read_buffer + 0x06) == BMS_CAL_CURR_GAIN);
      if (memcmp(read_buffer + BMS_CONFIG_MAIN_ARRAY_OFFSET, bms_config_main_array, sizeof(bms_config_main_array)) == 0 && cc1_gain_correct && current_gain_correct) {
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

//state-of-charge calculation update - TODO: automatic battery health detection? here or separately?
static void _BMS_UpdateSoC() {
  //smoothed current (in A) and cell voltage (in V) values - initially NaN meaning "uninitialised"
  static float current_smoothed = NAN;
  static float voltages_smoothed[BMS_CELLS_SERIES];

  int i;
  float min_voltage;
  float charge;
  float cell_energy;

  //convert integer measurements to floats in corresponding units
  float current = (float)bms_measurements.current / 1000.0f / (float)BMS_CELLS_PARALLEL; //mA total -> A per cell
  float voltages[BMS_CELLS_SERIES];
  for (i = 0; i < BMS_CELLS_SERIES; i++) voltages[i] = (float)bms_measurements.voltage_cells[i] / 1000.0f; //mV -> V
  float charge_delta = (float)bms_measurements.accumulated_charge / 3600000.0f / (float)BMS_CELLS_PARALLEL; //mAs total -> Ah per cell

  //calculate smoothed current and cell voltages
  if (isnanf(current_smoothed)) {
    //initialise from first measurement, if valid
    if (bms_measurements.current != BMS_ERROR_CURRENT && bms_measurements.voltage_cells[0] > 0 && bms_measurements.voltage_cells[1] > 0 &&
        bms_measurements.voltage_cells[2] > 0 && bms_measurements.voltage_cells[3] > 0 && bms_measurements.voltage_cells[4] > 0) {
      //current initially set to "too much" for reference point setting, to ensure some settling time at the start
      current_smoothed = (bms_measurements.current > 0 ? 1.0f : -1.0f) * bat_calc_voltageToCharge_max_valid_current * BMS_SOC_CURRENT_INIT_FACTOR;
      //cell voltages initialised correctly
      for (i = 0; i < BMS_CELLS_SERIES; i++) voltages_smoothed[i] = voltages[i];
    }
  } else {
    //apply exponential smoothing
    current_smoothed = BMS_SOC_SMOOTHING_ALPHA * current + BMS_SOC_SMOOTHING_1MALPHA * current_smoothed;
    for (i = 0; i < BMS_CELLS_SERIES; i++) {
      voltages_smoothed[i] = BMS_SOC_SMOOTHING_ALPHA * voltages[i] + BMS_SOC_SMOOTHING_1MALPHA * voltages_smoothed[i];
    }
  }

  //if battery health is invalid, default to 100% health
  if (bms_soc_battery_health <= 0.0f || bms_soc_battery_health > 1.0f) bms_soc_battery_health = 1.0f;

  //run voltage-based charge estimation if smoothed current is small enough
  float estimated_charge = NAN;
  if (fabsf(current_smoothed) <= bat_calc_voltageToCharge_max_valid_current) {
    //determine lowest smoothed cell voltage
    min_voltage = voltages_smoothed[0];
    for (i = 1; i < BMS_CELLS_SERIES; i++) {
      if (voltages_smoothed[i] < min_voltage) min_voltage = voltages_smoothed[i];
    }

    //check if lowest smoothed voltage is acceptable
    if (min_voltage >= BMS_SOC_CELL_VOLTAGE_MIN && min_voltage <= BMS_SOC_CELL_VOLTAGE_MAX) {
      //check if the battery is fully charged, reset BMS charge measurement if so
      if (min_voltage >= BMS_SOC_CELL_FULL_CHARGE_VOLTAGE_MIN && BMS_I2C_SubcommandOnly(SUBCMD_RESET_PASSQ, BMS_COMM_MAX_TRIES) == HAL_OK) {
        //fully charged: set charge reference point to full and go to SOC_CHARGE_FULL mode
        _bms_soc_charge_reference = bat_calc_cellCharge_max * bms_soc_battery_health;
        charge_delta = 0.0f;
        bms_soc_precisionlevel = SOC_CHARGE_FULL;
      } else {
        //not full (normal operation): estimate charge of weakest cell
        estimated_charge = BAT_CALC_CellVoltageToCharge(min_voltage, bms_soc_battery_health);

        //save as new charge reference or check existing charge reference
        switch (bms_soc_precisionlevel) {
          case SOC_CHARGE_ESTIMATED:
          case SOC_CHARGE_FULL:
            //check integrated charge
            charge = _bms_soc_charge_reference + charge_delta;
            if (fabsf(charge - estimated_charge) <= BMS_SOC_CHARGE_DIFFERENCE_MAX) {
              //close enough to estimated charge: continue with existing reference
              break;
            }
            //too far away from estimated charge: fall through to next case, resetting it
          default:
            //reset BMS charge measurement, then set estimated charge reference point and go to SOC_CHARGE_ESTIMATED mode
            if (BMS_I2C_SubcommandOnly(SUBCMD_RESET_PASSQ, BMS_COMM_MAX_TRIES) == HAL_OK) {
              _bms_soc_charge_reference = estimated_charge;
              charge_delta = 0.0f;
              bms_soc_precisionlevel = SOC_CHARGE_ESTIMATED;
            } else {
              //fall back to voltage-only if reset command fails somehow
              bms_soc_precisionlevel = SOC_VOLTAGE_ONLY;
            }
            break;
        }
      }
    }
  }

  //calculate estimated cell energy, depending on the mode
  switch (bms_soc_precisionlevel) {
    default:
      //invalid mode: default to voltage only, fall through to that case
      DEBUG_PRINTF("WARNING: BMS SoC estimation encountered invalid mode %u\n", (uint8_t)bms_soc_precisionlevel);
      bms_soc_precisionlevel = SOC_VOLTAGE_ONLY;
    case SOC_VOLTAGE_ONLY:
      if (fabsf(current) > bat_calc_voltageToEnergy_max_valid_current) {
        //current too high for valid voltage-to-energy estimation: report unknown energy and fraction (NaN)
        bms_soc_energy = NAN;
        bms_soc_fraction = NAN;
        return;
      } else {
        //determine lowest cell voltage
        min_voltage = voltages[0];
        for (i = 1; i < BMS_CELLS_SERIES; i++) {
          if (voltages[i] < min_voltage) min_voltage = voltages[i];
        }

        if (min_voltage < BMS_SOC_CELL_VOLTAGE_MIN || min_voltage > BMS_SOC_CELL_VOLTAGE_MAX) {
          //unacceptable voltage: something went wrong, no estimation possible
          bms_soc_energy = NAN;
          bms_soc_fraction = NAN;
          return;
        }

        //estimate energy of weakest cell
        cell_energy = BAT_CALC_CellVoltageToEnergy(min_voltage, bms_soc_battery_health);
      }
      break;
    case SOC_CHARGE_ESTIMATED:
    case SOC_CHARGE_FULL:
      //calculate integrated charge per cell
      charge = _bms_soc_charge_reference + charge_delta;
      //estimate energy of single cell
      cell_energy = BAT_CALC_CellChargeToEnergy(charge, bms_soc_battery_health);
      break;
  }

  //multiply single cell energy by cell count to get total energy
  bms_soc_energy = BMS_CELLS_TOTAL * cell_energy;
  //calculate fraction from cell energy and battery health, and clamp to [0, 1]
  bms_soc_fraction = cell_energy / (bat_calc_cellEnergy_max * bms_soc_battery_health);
  if (bms_soc_fraction < 0.0f) bms_soc_fraction = 0.0f;
  else if (bms_soc_fraction > 1.0f) bms_soc_fraction = 1.0f;
}

//check if any cells need balancing and enable/disable balancing accordingly
static HAL_StatusTypeDef _BMS_HandleBalancing() {
  int i;

  //check whether we're even in a state to do cell balancing
  if (bms_status._deepsleep_bit || bms_status.safety_faults._all != 0 || !pwrsw_status) {
    //no: disable all cell balancing
    _bms_bal_active_cells = 0;
    return BMS_I2C_SubcommandWrite(SUBCMD_CB_ACTIVE_CELLS, &_bms_bal_active_cells, 1, BMS_COMM_MAX_TRIES);
  }

  //find lowest voltage (weakest) cell
  int16_t min_voltage = bms_measurements.voltage_cells[0];
  uint8_t min_index = 0;
  for (i = 1; i < BMS_CELLS_SERIES; i++) {
    if (bms_measurements.voltage_cells[i] < min_voltage) {
      min_voltage = bms_measurements.voltage_cells[i];
      min_index = i;
    }
  }

  if (min_voltage < BMS_PROT_CUV_THRESHOLD || min_voltage > BMS_PROT_COV_THRESHOLD) {
    //invalid weakest cell voltage, something is wrong: disable balancing and return
    _bms_bal_active_cells = 0;
    BMS_I2C_SubcommandWrite(SUBCMD_CB_ACTIVE_CELLS, &_bms_bal_active_cells, 1, BMS_COMM_MAX_TRIES);
    return HAL_ERROR;
  }

  //check other cells for whether balancing is needed
  uint8_t need_balancing = 0;
  for (i = 0; i < BMS_CELLS_SERIES; i++) {
    if (i == min_index) continue;

    uint8_t cell_bit = (2 << i); //cell 1 = bit 1 (not bit 0)

    //determine difference to weakest cell needed for balancing
    int16_t threshold;
    if ((_bms_bal_active_cells & cell_bit) != 0) {
      //cell already balancing: keep balancing enabled as long as above stop threshold
      threshold = BMS_BAL_DIFF_STOP;
    } else {
      //cell not balancing right now: only start balancing if below start threshold
      threshold = BMS_BAL_DIFF_START;
    }

    //compare difference to threshold to see whether balancing should be enabled
    if (bms_measurements.voltage_cells[i] - min_voltage > threshold) {
      need_balancing |= cell_bit;
    }
  }

  //enable balancing for non-adjacent set of cells that need it - prefer "uneven" cells 1,3,5 (arbitrary choice)
  if ((need_balancing & 0x2A) != 0) {
    //enable uneven cells only, to prevent adjacent cells from balancing simultaneously (might result in too much current otherwise)
    _bms_bal_active_cells = (need_balancing & 0x2A);
  } else {
    //uneven cells don't need balancing: can enable whatever others need balancing (adjacent cells impossible now anyway)
    _bms_bal_active_cells = need_balancing;
  }

  //send command
  return BMS_I2C_SubcommandWrite(SUBCMD_CB_ACTIVE_CELLS, &_bms_bal_active_cells, 1, BMS_COMM_MAX_TRIES);
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
    memcpy(bms_measurements.voltage_cells, read_buffer_s16, 10);
  } else {
    //read failed: default to error voltage
    DEBUG_PRINTF("ERROR: BMS measurement update: Cell voltage read failed\n");
    bms_measurements.voltage_cells[0] = BMS_ERROR_VOLTAGE;
    bms_measurements.voltage_cells[1] = BMS_ERROR_VOLTAGE;
    bms_measurements.voltage_cells[2] = BMS_ERROR_VOLTAGE;
    bms_measurements.voltage_cells[3] = BMS_ERROR_VOLTAGE;
    bms_measurements.voltage_cells[4] = BMS_ERROR_VOLTAGE;
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

  //current, charge, time, state-of-charge are unnecessary when power is off
  if (pwrsw_status) {
    //read current
    if (BMS_I2C_DirectCommandRead(DIRCMD_CURRENT, read_buffer_u8, 2, BMS_COMM_MAX_TRIES) == HAL_OK) {
      bms_measurements.current = BMS_CONV_CUR_TO_MA_MULT * (int32_t)read_buffer_s16[0];
    } else {
      //read failed: default to error current
      DEBUG_PRINTF("ERROR: BMS measurement update: Current read failed\n");
      bms_measurements.current = BMS_ERROR_CURRENT;
      res = HAL_ERROR;
    }

    //read charge and integration time
    if (BMS_I2C_SubcommandRead(SUBCMD_PASSQ, read_buffer_u8, 12, BMS_COMM_MAX_TRIES) == HAL_OK) {
      bms_measurements.accumulated_charge = ((int64_t)BMS_CONV_CHG_TO_MAS_MULT * (*(int64_t*)read_buffer_u8)) / (int64_t)BMS_CONV_CHG_TO_MAS_DIV;
      bms_measurements.charge_accumulation_time = *(uint32_t*)(read_buffer_u8 + 8);
    } else {
      //read failed: default to error charge and time
      DEBUG_PRINTF("ERROR: BMS measurement update: Charge and time read failed\n");
      bms_measurements.accumulated_charge = BMS_ERROR_CHARGE;
      bms_measurements.charge_accumulation_time = BMS_ERROR_TIME;
      res = HAL_ERROR;
    }

    //update state-of-charge calculations
    _BMS_UpdateSoC();
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

  //disable all cell balancing
  _bms_bal_active_cells = 0;
  ReturnOnError(BMS_I2C_SubcommandWrite(SUBCMD_CB_ACTIVE_CELLS, &_bms_bal_active_cells, 1, BMS_COMM_MAX_TRIES));

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

  bms_soc_battery_health = 1.0f;
  bms_soc_precisionlevel = SOC_VOLTAGE_ONLY;
  bms_soc_energy = NAN;
  bms_soc_fraction = NAN;

  //get current CRC mode and initialise config
  ReturnOnError(_BMS_DetectCRCMode());
  ReturnOnError(_BMS_CheckAndApplyConfig());

  //disable all cell balancing
  _bms_bal_active_cells = 0;
  ReturnOnError(BMS_I2C_SubcommandWrite(SUBCMD_CB_ACTIVE_CELLS, &_bms_bal_active_cells, 1, BMS_COMM_MAX_TRIES));

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

#ifdef MAIN_CURRENT_CALIBRATION
  static float cc1_mean = 0.0f;
  static float cc1_var = 0.0f;
  static float cc2_mean = 0.0f;
  static float cc2_var = 0.0f;
  const float alpha = 0.03125f;
  static float cc2_lt_mean = 0.0f;
  const float lt_alpha = 0.0005f;

  //calculate moving averages and standard deviations
  if (loop_count % 25 == 0) {
    int16_t cc1_read;
    int32_t cc2_read;
    BMS_I2C_DirectCommandRead(DIRCMD_CC1_CURRENT, (uint8_t*)&cc1_read, 2, BMS_COMM_MAX_TRIES);
    BMS_I2C_DirectCommandRead(DIRCMD_RAW_CURRENT, (uint8_t*)&cc2_read, 4, BMS_COMM_MAX_TRIES);

    float diff = (float)cc1_read - cc1_mean;
    float incr = alpha * diff;
    cc1_mean += incr;
    cc1_var = (1.0f - alpha) * (cc1_var + diff * incr);

    diff = (float)cc2_read - cc2_mean;
    incr = alpha * diff;
    cc2_mean += incr;
    cc2_var = (1.0f - alpha) * (cc2_var + diff * incr);

    cc2_lt_mean = lt_alpha * (float)cc2_read + (1.0f - lt_alpha) * cc2_lt_mean;
  }

  if (loop_count % 100 == 0) {
    DEBUG_PRINTF("CC1 CAL: mean = %.2f; stddev = %.2f\nCC2 CAL: mean = %.2f; stddev = %.2f; lt_mean = %.2f\n", cc1_mean, sqrtf(cc1_var), cc2_mean, sqrtf(cc2_var), cc2_lt_mean);
  }
#endif

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
      _BMS_ExitCFGUPDATE();
    }

    //if there are any safety faults, disable all cell balancing
    if (bms_status.safety_faults._all != 0) {
      _bms_bal_active_cells = 0;
      BMS_I2C_SubcommandWrite(SUBCMD_CB_ACTIVE_CELLS, &_bms_bal_active_cells, 1, BMS_COMM_MAX_TRIES);
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

  //cell balancing handling (in phase with measurement update)
  if (!bms_status._deepsleep_bit && loop_count % BMS_LOOP_PERIOD_MEASUREMENTS == 0) {
    _BMS_HandleBalancing();
  }

  //debug status printout - TODO remove later?
#ifdef DEBUG
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
    DEBUG_PRINTF("Cell voltages: %d %d %d %d %d, Stack voltage: %u, Current: %ld\n", bms_measurements.voltage_cells[0], bms_measurements.voltage_cells[1],
                 bms_measurements.voltage_cells[2], bms_measurements.voltage_cells[3], bms_measurements.voltage_cells[4], bms_measurements.voltage_stack, bms_measurements.current);
    DEBUG_PRINTF("SoC: Mode: %u, Energy: %.2f, Fraction: %.4f, Health: %.2f\n", (uint8_t)bms_soc_precisionlevel, bms_soc_energy, bms_soc_fraction, bms_soc_battery_health);
    DEBUG_PRINTF("Temps: Bat %d Int %d, Charge: %lld, Time: %lu, Balancing: 0x%02X\n", bms_measurements.bat_temp, bms_measurements.internal_temp, bms_measurements.accumulated_charge,
                 bms_measurements.charge_accumulation_time, _bms_bal_active_cells);
  }
#endif
}

