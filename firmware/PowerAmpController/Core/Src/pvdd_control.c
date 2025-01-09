/*
 * pvdd_control.c
 *
 *  Created on: Oct 7, 2024
 *      Author: Alex
 */

#include "pvdd_control.h"
#include <stdio.h>
#include "arm_math.h"
#include "i2c.h"


float pvdd_voltage_target = 0.0f;
float pvdd_voltage_requested = 0.0f;
float pvdd_voltage_measured = 0.0f;
uint8_t pvdd_valid_voltage = 0;

//offset added to requested voltage, to adjust for errors
float pvdd_voltage_request_offset = 0.0f;
//if >0, offset will not update in main loop - decrements every loop
static uint16_t pvdd_remaining_lockout_cycles = 0;

//whether we are currently reducing the voltage (1) or not (0)
uint8_t pvdd_reduction_ongoing = 0;
//how many cycles the reduction has been going for
uint32_t pvdd_reduction_cycles = 0;
//measurement buffer for reduction measurements to determine whether voltage has settled
static float pvdd_reduction_measurements[PVDD_VOLTAGE_REDUCTION_WINDOW] = { 0.0f };
//current position in reduction measurement buffer
static uint8_t pvdd_reduction_measurement_index = 0;


//get the total spread (maximum - minimum) of the measured voltages in the reduction measurement buffer
static float _PVDD_GetReductionMeasurementSpread() {
  //initialise min and max values to the first entry
  float minVal = pvdd_reduction_measurements[0];
  float maxVal = minVal;

  //iterate over remaining buffer to find true min and max values
  int i;
  for (i = 1; i < PVDD_VOLTAGE_REDUCTION_WINDOW; i++) {
    float val = pvdd_reduction_measurements[i];
    if (val < minVal) minVal = val;
    if (val > maxVal) maxVal = val;
  }

  //calculate spread
  return maxVal - minVal;
}

static HAL_StatusTypeDef _PVDD_WriteDACVoltage() {
  if (!pvdd_reduction_ongoing) { //automatically calculate requested voltage unless reduction is currently ongoing
    pvdd_voltage_requested = pvdd_voltage_target + pvdd_voltage_request_offset;
  }

  //absolute minimum voltage that can be requested is the intercept
  if (pvdd_voltage_requested < PVDD_TRK_INTERCEPT) pvdd_voltage_requested = PVDD_TRK_INTERCEPT;
  //absolute maximum voltage that can be requested is one volt above maximum target (for safety)
  if (pvdd_voltage_requested > (PVDD_MAX_VOLTAGE + 1.0f)) pvdd_voltage_requested = (PVDD_MAX_VOLTAGE + 1.0f);

  float dac_value_f = PVDD_DAC_FACTOR * (pvdd_voltage_requested - PVDD_TRK_INTERCEPT);

  if (dac_value_f < 0.0f || dac_value_f >= 4096.0f) return HAL_ERROR;

  uint32_t dac_value_i = (uint32_t)floorf(dac_value_f);

  return HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, dac_value_i);
}

//reset the PVDD control logic after some kind of failure
static void _PVDD_ResetAfterFail() {
  //mark voltage as invalid, cancel potentially ongoing reduction
  pvdd_valid_voltage = 0;
  pvdd_reduction_ongoing = 0;

  //request currently measured voltage (for safety), clamped to allowed limits
  if (pvdd_voltage_measured < PVDD_MIN_VOLTAGE) PVDD_SetTargetVoltage(PVDD_MIN_VOLTAGE);
  else if (pvdd_voltage_measured > PVDD_MAX_VOLTAGE) PVDD_SetTargetVoltage(PVDD_MAX_VOLTAGE);
  else PVDD_SetTargetVoltage(pvdd_voltage_measured);
}

//check for (and handle) potential voltage fail compared to the chosen reference voltage and margin
//returns 1 in case of fail
static uint8_t _PVDD_CheckForVoltageFail(float ref_voltage, float fail_margin) {
  //check voltage fail conditions and reset if met
  if (fabsf(pvdd_voltage_measured - ref_voltage) > fail_margin) {
    //check for "expected overvoltage" condition (requested voltage is at/below supply input voltage) - only reset in other cases
    if (pvdd_voltage_measured < ref_voltage || pvdd_voltage_measured > PVDD_VOLTAGE_MAX_NOERROR) {
      if (pvdd_voltage_measured > 6.0f) DEBUG_PRINTF("ERROR: PVDD voltage fail, measured %f V, reference %f V\n", pvdd_voltage_measured, ref_voltage);
      _PVDD_ResetAfterFail();
      I2C_TriggerInterrupt(I2CDEF_POWERAMP_INT_FLAGS_INT_PVDD_ERR_Msk);
      return 1;
    }
  }
  return 0;
}

HAL_StatusTypeDef PVDD_Init() {
  pvdd_voltage_measured = 0.0f;
  pvdd_valid_voltage = 0;
  pvdd_reduction_ongoing = 0;

  //start DAC function
  ReturnOnError(HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, PVDD_INIT_DAC_VALUE));
  ReturnOnError(HAL_DAC_Start(&hdac1, DAC_CHANNEL_1));

  //initially request max voltage for safety (in case of spurious reset, going up is always safe)
  return PVDD_SetTargetVoltage(PVDD_MAX_VOLTAGE);
}

HAL_StatusTypeDef PVDD_SetTargetVoltage(float voltage) {
  if (voltage < PVDD_MIN_VOLTAGE || voltage > PVDD_MAX_VOLTAGE || isnanf(voltage)) return HAL_ERROR;

  pvdd_voltage_target = voltage;
  pvdd_voltage_request_offset = 0.0f;

  if (voltage >= (pvdd_voltage_measured * PVDD_VOLTAGE_REDUCTION_FACTOR)) {
    //increase or very small decrease: can safely do in a single step
    pvdd_remaining_lockout_cycles = PVDD_OFFSET_LOCKOUT_CYCLES;
    pvdd_reduction_ongoing = 0; //cancel any potentially ongoing reduction

    DEBUG_PRINTF("New PVDD target %f V, measured %f V, applying directly\n", pvdd_voltage_target, pvdd_voltage_measured);
  } else {
    //larger decrease: start gradual reduction process
    pvdd_reduction_ongoing = 1;
    pvdd_reduction_cycles = 0;

    //clear measurement buffer and index
    memset(pvdd_reduction_measurements, 0, sizeof(pvdd_reduction_measurements));
    pvdd_reduction_measurement_index = 0;

    //request first reduction step immediately, based on measured voltage (for safety)
    pvdd_voltage_requested = MAX(pvdd_voltage_measured * PVDD_VOLTAGE_REDUCTION_FACTOR, pvdd_voltage_target);
    pvdd_remaining_lockout_cycles = PVDD_VOLTAGE_REDUCTION_LOCKOUT_CYCLES;

    DEBUG_PRINTF("New PVDD target %f V, measured %f V, starting reduction: first step %f V\n", pvdd_voltage_target, pvdd_voltage_measured, pvdd_voltage_requested);
  }

  return _PVDD_WriteDACVoltage();
}

void PVDD_LoopUpdate() {
  //sanity check of requested voltage - reset if out of range
  if (pvdd_voltage_target < PVDD_MIN_VOLTAGE || pvdd_voltage_target > PVDD_MAX_VOLTAGE) {
    DEBUG_PRINTF("ERROR: PVDD invalid target voltage %f V, resetting PVDD control\n", pvdd_voltage_target);
    _PVDD_ResetAfterFail();
    I2C_TriggerInterrupt(I2CDEF_POWERAMP_INT_FLAGS_INT_PVDD_ERR_Msk);
    return;
  }

  //measure PVDD voltage
  if (HAL_ADCEx_InjectedStart(&hadc1) != HAL_OK) return;

  if (HAL_ADCEx_InjectedPollForConversion(&hadc1, 2) != HAL_OK) return;

  q15_t raw_measurement = HAL_ADCEx_InjectedGetValue(&hadc1, ADC_INJECTED_RANK_1);

  float raw_measurement_f;
  arm_q15_to_float(&raw_measurement, &raw_measurement_f, 1);

  float direct_measured_voltage = PVDD_ADC_FACTOR * raw_measurement_f;

  //check if we're locked out
  uint8_t locked_out = 0;
  __disable_irq(); //ensure atomicity of lockout check and modification
  if (pvdd_remaining_lockout_cycles > 0) {
    pvdd_remaining_lockout_cycles -= 1;
    locked_out = 1;
  }
  __enable_irq();

  //apply voltage updates and corresponding actions, depending on state (reduction or lockout or normal)
  if (pvdd_reduction_ongoing) { //in reduction process
    //update voltage measurement - faster smoothing due to expected change
    pvdd_voltage_measured = 0.8f * pvdd_voltage_measured + 0.2f * direct_measured_voltage;

    pvdd_reduction_cycles++;

    if (!locked_out) {
      //check for voltage fail in relation to request - end processing if there is one
      if (_PVDD_CheckForVoltageFail(pvdd_voltage_requested, PVDD_VOLTAGE_ERROR_FAIL_REDUCTION)) {
        return;
      }

      //not in fail condition: voltage is valid
      pvdd_valid_voltage = 1;
    }

    //store current measurement in reduction buffer and advance its index
    pvdd_reduction_measurements[pvdd_reduction_measurement_index] = direct_measured_voltage;
    pvdd_reduction_measurement_index = (pvdd_reduction_measurement_index + 1) % PVDD_VOLTAGE_REDUCTION_WINDOW;

    float measurement_spread = _PVDD_GetReductionMeasurementSpread();

    //check whether the voltage has stabilised enough yet (if not locked out)
    if (!locked_out && (measurement_spread < PVDD_VOLTAGE_REDUCTION_MARGIN)) {
      //step is stable: check if we're at our target or not
      if (pvdd_voltage_requested == pvdd_voltage_target) {
        //target reached: end reduction process
        pvdd_reduction_ongoing = 0;
        pvdd_remaining_lockout_cycles = PVDD_OFFSET_LOCKOUT_SHORT_CYCLES; //small lockout equivalent to offset change
        I2C_TriggerInterrupt(I2CDEF_POWERAMP_INT_MASK_INT_PVDD_REDDONE_Msk);
        DEBUG_PRINTF("PVDD reduction completed at %f V (cycle %lu)\n", pvdd_voltage_requested, pvdd_reduction_cycles);
        //DEBUG_PRINTF("PVDD reduction step %f V is stable (spread %f V), reduction completed (cycle %lu)\n", pvdd_voltage_requested, measurement_spread, pvdd_reduction_cycles);
      } else {
        //target not reached: take another step down, based on measured voltage (for safety)
        //DEBUG_PRINTF("PVDD reduction step %f V is stable (spread %f V), taking next step (cycle %lu)\n", pvdd_voltage_requested, measurement_spread, pvdd_reduction_cycles);
        pvdd_voltage_requested = MAX(pvdd_voltage_measured * PVDD_VOLTAGE_REDUCTION_FACTOR, pvdd_voltage_target);
        pvdd_remaining_lockout_cycles = PVDD_VOLTAGE_REDUCTION_LOCKOUT_CYCLES;
        _PVDD_WriteDACVoltage();
        //DEBUG_PRINTF("New PVDD reduction step: %f V\n", pvdd_voltage_requested);
      }
      return; //don't check reduction timeout if we just made a step (or if we're done)
    } /*else if (!locked_out) {
      DEBUG_PRINTF("PVDD reduction step %f V, not stable yet (spread %f V) (cycle %lu)\n", pvdd_voltage_requested, measurement_spread, pvdd_reduction_cycles);
    }*/

    //check for reduction timeout
    if (pvdd_reduction_cycles > PVDD_VOLTAGE_REDUCTION_TIMEOUT) {
      //timeout occurred: end reduction process, set target to the reached step
      DEBUG_PRINTF("WARNING: PVDD reduction timed out at %f V (target was %f V), cancelling (cycle %lu)\n", pvdd_voltage_requested, pvdd_voltage_target, pvdd_reduction_cycles);
      pvdd_voltage_target = pvdd_voltage_requested;
      pvdd_reduction_ongoing = 0;
      pvdd_remaining_lockout_cycles = PVDD_OFFSET_LOCKOUT_SHORT_CYCLES; //small lockout equivalent to offset change
      I2C_TriggerInterrupt(I2CDEF_POWERAMP_INT_MASK_INT_PVDD_REDDONE_Msk);
    }
  } else if (locked_out) { //not reducing, but in lockout
    //fast voltage update because we expect rapid changes during lockout time
    if (pvdd_remaining_lockout_cycles > 10) { //still in lockout for a while: instant update
	pvdd_voltage_measured = direct_measured_voltage;
    } else if (pvdd_remaining_lockout_cycles > 5) { //nearing end: apply light smoothing
	pvdd_voltage_measured = 0.5f * pvdd_voltage_measured + 0.5f * direct_measured_voltage;
    } else { //almost done: stronger smoothing
	pvdd_voltage_measured = 0.9f * pvdd_voltage_measured + 0.1f * direct_measured_voltage;
    }
  } else { //normal operation
    //use (slow) EMA for voltage measurement smoothing
    pvdd_voltage_measured = PVDD_EMA_1MALPHA * pvdd_voltage_measured + PVDD_EMA_ALPHA * direct_measured_voltage;

    //check for voltage fail in relation to target - end processing if there is one
    if (_PVDD_CheckForVoltageFail(pvdd_voltage_target, PVDD_VOLTAGE_ERROR_FAIL)) {
      return;
    }

    //not in fail condition: voltage is valid
    pvdd_valid_voltage = 1;

    //check voltage error and apply correction if needed
    if (pvdd_voltage_measured - pvdd_voltage_target < -PVDD_VOLTAGE_ERROR_CORRECT && pvdd_voltage_request_offset < PVDD_VOLTAGE_OFFSET_MAX) { //voltage lower than requested: correct upwards, if we have headroom
      if (pvdd_voltage_request_offset + PVDD_VOLTAGE_OFFSET_STEP >= PVDD_VOLTAGE_OFFSET_MAX) { //hit maximum: set to maximum correction and warn
	pvdd_voltage_request_offset = PVDD_VOLTAGE_OFFSET_MAX;
	DEBUG_PRINTF("WARNING: PVDD voltage offset hit maximum %f V (measured %f V, target %f V)\n", pvdd_voltage_request_offset, pvdd_voltage_measured, pvdd_voltage_target);
	I2C_TriggerInterrupt(I2CDEF_POWERAMP_INT_FLAGS_INT_PVDD_OLIM_Msk);
      } else { //not at maximum yet: increment offset by one step
	pvdd_voltage_request_offset += PVDD_VOLTAGE_OFFSET_STEP;
      }

      //lock out of further changes for a short time
      pvdd_remaining_lockout_cycles = PVDD_OFFSET_LOCKOUT_SHORT_CYCLES;

      _PVDD_WriteDACVoltage();
    } else if (pvdd_voltage_measured - pvdd_voltage_target > PVDD_VOLTAGE_ERROR_CORRECT && pvdd_voltage_request_offset > -PVDD_VOLTAGE_OFFSET_MAX) { //voltage higher than requested: correct downwards, if we have headroom
      if (pvdd_voltage_request_offset - PVDD_VOLTAGE_OFFSET_STEP <= -PVDD_VOLTAGE_OFFSET_MAX) { //hit maximum: set to maximum correction and warn
	pvdd_voltage_request_offset = -PVDD_VOLTAGE_OFFSET_MAX;
	DEBUG_PRINTF("WARNING: PVDD voltage offset hit maximum %f V (measured %f V, target %f V)\n", pvdd_voltage_request_offset, pvdd_voltage_measured, pvdd_voltage_target);
	I2C_TriggerInterrupt(I2CDEF_POWERAMP_INT_FLAGS_INT_PVDD_OLIM_Msk);
      } else { //not at maximum yet: decrement offset by one step
	pvdd_voltage_request_offset -= PVDD_VOLTAGE_OFFSET_STEP;
      }

      //lock out of further changes for a short time
      pvdd_remaining_lockout_cycles = PVDD_OFFSET_LOCKOUT_SHORT_CYCLES;

      _PVDD_WriteDACVoltage();
    }
  }
}
