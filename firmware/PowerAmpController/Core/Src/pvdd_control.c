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


float pvdd_voltage_requested = 0.0f;
float pvdd_voltage_measured = 0.0f;
uint8_t pvdd_valid_voltage = 0;

//offset added to requested voltage, to adjust for errors
float pvdd_voltage_request_offset = 0.0f;
//if >0, offset will not update in main loop - decrements every loop
static uint16_t pvdd_remaining_lockout_cycles = 0;

HAL_StatusTypeDef PVDD_Init() {
  pvdd_voltage_measured = 0.0f;
  pvdd_valid_voltage = 0;

  //start DAC function
  ReturnOnError(HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 0));
  ReturnOnError(HAL_DAC_Start(&hdac1, DAC_CHANNEL_1));

  //request minimum voltage initially
  return PVDD_SetRequestedVoltage(PVDD_MIN_VOLTAGE);
}

HAL_StatusTypeDef _PVDD_WriteDACVoltage() {
  float adjusted_requested_voltage = pvdd_voltage_requested + pvdd_voltage_request_offset;

  //absolute minimum voltage that can be requested is the intercept
  if (adjusted_requested_voltage < PVDD_TRK_INTERCEPT) adjusted_requested_voltage = PVDD_TRK_INTERCEPT;

  float dac_value_f = PVDD_DAC_FACTOR * (adjusted_requested_voltage - PVDD_TRK_INTERCEPT);

  if (dac_value_f < 0.0f || dac_value_f >= 4096.0f) return HAL_ERROR;

  uint32_t dac_value_i = (uint32_t)floorf(dac_value_f);

  return HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, dac_value_i);
}

HAL_StatusTypeDef PVDD_SetRequestedVoltage(float voltage) {
  if (voltage < PVDD_MIN_VOLTAGE || voltage > PVDD_MAX_VOLTAGE || isnanf(voltage)) return HAL_ERROR;

  pvdd_voltage_requested = voltage;
  pvdd_voltage_request_offset = 0.0f;
  pvdd_remaining_lockout_cycles = PVDD_OFFSET_LOCKOUT_CYCLES;

  return _PVDD_WriteDACVoltage();
}

void PVDD_LoopUpdate() {
  //sanity check of requested voltage - reset if out of range
  if (pvdd_voltage_requested < PVDD_MIN_VOLTAGE || pvdd_voltage_requested > PVDD_MAX_VOLTAGE) {
    DEBUG_PRINTF("ERROR: PVDD invalid requested voltage %f V, resetting PVDD control\n", pvdd_voltage_requested);
    PVDD_Init();
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

  //apply voltage updates and corresponding actions (if not in lockout)
  __disable_irq(); //ensure atomicity of lockout check and modification
  if (pvdd_remaining_lockout_cycles > 0) { //in lockout
    pvdd_remaining_lockout_cycles -= 1; //count down lockout timer
    __enable_irq();

    //fast voltage update because we expect rapid changes during lockout time
    if (pvdd_remaining_lockout_cycles > 10) { //still in lockout for a while: instant update
	pvdd_voltage_measured = direct_measured_voltage;
    } else if (pvdd_remaining_lockout_cycles > 5) { //nearing end: apply light smoothing
	pvdd_voltage_measured = 0.5f * pvdd_voltage_measured + 0.5f * direct_measured_voltage;
    } else { //almost done: stronger smoothing
	pvdd_voltage_measured = 0.9f * pvdd_voltage_measured + 0.1f * direct_measured_voltage;
    }
  } else { //not in lockout
    __enable_irq();

    //use (slow) EMA for voltage measurement smoothing
    pvdd_voltage_measured = PVDD_EMA_1MALPHA * pvdd_voltage_measured + PVDD_EMA_ALPHA * direct_measured_voltage;

    //check voltage fail condition: reset if met
    if (fabsf(pvdd_voltage_measured - pvdd_voltage_requested) > PVDD_VOLTAGE_ERROR_FAIL) {
      DEBUG_PRINTF("ERROR: PVDD voltage fail, measured %f V, target %f V\n", pvdd_voltage_measured, pvdd_voltage_requested);
      PVDD_Init();
      I2C_TriggerInterrupt(I2CDEF_POWERAMP_INT_FLAGS_INT_PVDD_ERR_Msk);
      return;
    }

    //not in fail condition: voltage is valid
    pvdd_valid_voltage = 1;

    //check voltage error and apply correction if needed
    if (pvdd_voltage_measured - pvdd_voltage_requested < -PVDD_VOLTAGE_ERROR_CORRECT && pvdd_voltage_request_offset < PVDD_VOLTAGE_OFFSET_MAX) { //voltage lower than requested: correct upwards, if we have headroom
      if (pvdd_voltage_request_offset + PVDD_VOLTAGE_OFFSET_STEP >= PVDD_VOLTAGE_OFFSET_MAX) { //hit maximum: set to maximum correction and warn
	pvdd_voltage_request_offset = PVDD_VOLTAGE_OFFSET_MAX;
	DEBUG_PRINTF("WARNING: PVDD voltage offset hit maximum %f V (measured %f V, target %f V)\n", pvdd_voltage_request_offset, pvdd_voltage_measured, pvdd_voltage_requested);
	I2C_TriggerInterrupt(I2CDEF_POWERAMP_INT_FLAGS_INT_PVDD_OLIM_Msk);
      } else { //not at maximum yet: increment offset by one step
	pvdd_voltage_request_offset += PVDD_VOLTAGE_OFFSET_STEP;
      }

      //lock out of further changes for a short time
      pvdd_remaining_lockout_cycles = PVDD_OFFSET_LOCKOUT_SHORT_CYCLES;

      _PVDD_WriteDACVoltage();
    } else if (pvdd_voltage_measured - pvdd_voltage_requested > PVDD_VOLTAGE_ERROR_CORRECT && pvdd_voltage_request_offset > -PVDD_VOLTAGE_OFFSET_MAX) { //voltage higher than requested: correct downwards, if we have headroom
      if (pvdd_voltage_request_offset - PVDD_VOLTAGE_OFFSET_STEP <= -PVDD_VOLTAGE_OFFSET_MAX) { //hit maximum: set to maximum correction and warn
	pvdd_voltage_request_offset = -PVDD_VOLTAGE_OFFSET_MAX;
	DEBUG_PRINTF("WARNING: PVDD voltage offset hit maximum %f V (measured %f V, target %f V)\n", pvdd_voltage_request_offset, pvdd_voltage_measured, pvdd_voltage_requested);
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
