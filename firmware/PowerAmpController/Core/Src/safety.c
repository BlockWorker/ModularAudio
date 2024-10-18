/*
 * safety.c
 *
 *  Created on: Oct 11, 2024
 *      Author: Alex
 */

#include "safety.h"
#include <stdio.h>
#include <string.h>
#include "adc.h"
#include "pvdd_control.h"
#include "arm_math.h"


#define _SAFETY_CHECK_LIMITS(x,m,i) do { for (i = 0; i < 5; i++) { if (x[i] < 0.0f || x[i] > m[i]) return HAL_ERROR; } } while (0)


//safety limits for current, real power, and apparent power - shutdown if exceeded
//inst = single ADC batch (fast), 0s1 = 0.1s time constant EMA, 1s0 = 1.0s time constant EMA
//entries 0-3 are channel-wise (A-D, BTL mode uses A and C only), entry 4 is sum of all channels
float safety_max_current_inst[5] = SAFETY_LIMIT_MAX_CURRENT_INST;
float safety_max_current_0s1[5] = SAFETY_LIMIT_MAX_CURRENT_0S1;
float safety_max_current_1s0[5] = SAFETY_LIMIT_MAX_CURRENT_1S0;
float safety_max_real_power_inst[5] = SAFETY_LIMIT_MAX_REAL_POWER_INST;
float safety_max_real_power_0s1[5] = SAFETY_LIMIT_MAX_REAL_POWER_0S1;
float safety_max_real_power_1s0[5] = SAFETY_LIMIT_MAX_REAL_POWER_1S0;
float safety_max_apparent_power_inst[5] = SAFETY_LIMIT_MAX_APPARENT_POWER_INST;
float safety_max_apparent_power_0s1[5] = SAFETY_LIMIT_MAX_APPARENT_POWER_0S1;
float safety_max_apparent_power_1s0[5] = SAFETY_LIMIT_MAX_APPARENT_POWER_1S0;
//constant limit maximums for sanity checking and reset
static const float safety_limit_current_inst[5] = SAFETY_LIMIT_MAX_CURRENT_INST;
static const float safety_limit_current_0s1[5] = SAFETY_LIMIT_MAX_CURRENT_0S1;
static const float safety_limit_current_1s0[5] = SAFETY_LIMIT_MAX_CURRENT_1S0;
static const float safety_limit_real_power_inst[5] = SAFETY_LIMIT_MAX_REAL_POWER_INST;
static const float safety_limit_real_power_0s1[5] = SAFETY_LIMIT_MAX_REAL_POWER_0S1;
static const float safety_limit_real_power_1s0[5] = SAFETY_LIMIT_MAX_REAL_POWER_1S0;
static const float safety_limit_apparent_power_inst[5] = SAFETY_LIMIT_MAX_APPARENT_POWER_INST;
static const float safety_limit_apparent_power_0s1[5] = SAFETY_LIMIT_MAX_APPARENT_POWER_0S1;
static const float safety_limit_apparent_power_1s0[5] = SAFETY_LIMIT_MAX_APPARENT_POWER_1S0;
//similar to above, but lower "warning" limits - notify I2C bus if exceeded
float safety_warn_current_inst[5] = SAFETY_NO_WARN;
float safety_warn_current_0s1[5] = SAFETY_NO_WARN;
float safety_warn_current_1s0[5] = SAFETY_NO_WARN;
float safety_warn_real_power_inst[5] = SAFETY_NO_WARN;
float safety_warn_real_power_0s1[5] = SAFETY_NO_WARN;
float safety_warn_real_power_1s0[5] = SAFETY_NO_WARN;
float safety_warn_apparent_power_inst[5] = SAFETY_NO_WARN;
float safety_warn_apparent_power_0s1[5] = SAFETY_NO_WARN;
float safety_warn_apparent_power_1s0[5] = SAFETY_NO_WARN;

//1 = amp shut down (by force or error condition), 0 = amp allowed to run
uint8_t is_shutdown = 1;
//safety shutdown set
static uint8_t safety_shutdown = 1;
//manual shutdown set
static uint8_t manual_shutdown = 0;


//perform sanity check that configured fast limits are within the valid ranges
HAL_StatusTypeDef _SAFETY_SanityCheckFastLimits() {
  int i;
  _SAFETY_CHECK_LIMITS(safety_max_current_inst, safety_limit_current_inst, i);
  _SAFETY_CHECK_LIMITS(safety_max_real_power_inst, safety_limit_real_power_inst, i);
  _SAFETY_CHECK_LIMITS(safety_max_apparent_power_inst, safety_limit_apparent_power_inst, i);
  return HAL_OK;
}

//perform sanity check that all configured limits are within the valid ranges
HAL_StatusTypeDef _SAFETY_SanityCheckLimits() {
  int i;
  ReturnOnError(_SAFETY_SanityCheckFastLimits());
  _SAFETY_CHECK_LIMITS(safety_max_current_0s1, safety_limit_current_0s1, i);
  _SAFETY_CHECK_LIMITS(safety_max_current_1s0, safety_limit_current_1s0, i);
  _SAFETY_CHECK_LIMITS(safety_max_real_power_0s1, safety_limit_real_power_0s1, i);
  _SAFETY_CHECK_LIMITS(safety_max_real_power_1s0, safety_limit_real_power_1s0, i);
  _SAFETY_CHECK_LIMITS(safety_max_apparent_power_0s1, safety_limit_apparent_power_0s1, i);
  _SAFETY_CHECK_LIMITS(safety_max_apparent_power_1s0, safety_limit_apparent_power_1s0, i);
  return HAL_OK;
}

//reset maximum thresholds to default absolute limits
void _SAFETY_ResetLimits() {
  memcpy(safety_max_current_inst, safety_limit_current_inst, sizeof(safety_max_current_inst));
  memcpy(safety_max_current_0s1, safety_limit_current_0s1, sizeof(safety_max_current_0s1));
  memcpy(safety_max_current_1s0, safety_limit_current_1s0, sizeof(safety_max_current_1s0));
  memcpy(safety_max_real_power_inst, safety_limit_real_power_inst, sizeof(safety_max_real_power_inst));
  memcpy(safety_max_real_power_0s1, safety_limit_real_power_0s1, sizeof(safety_max_real_power_0s1));
  memcpy(safety_max_real_power_1s0, safety_limit_real_power_1s0, sizeof(safety_max_real_power_1s0));
  memcpy(safety_max_apparent_power_inst, safety_limit_apparent_power_inst, sizeof(safety_max_apparent_power_inst));
  memcpy(safety_max_apparent_power_0s1, safety_limit_apparent_power_0s1, sizeof(safety_max_apparent_power_0s1));
  memcpy(safety_max_apparent_power_1s0, safety_limit_apparent_power_1s0, sizeof(safety_max_apparent_power_1s0));
}

//instant safety shutdown of the amp
__always_inline void _SAFETY_TriggerSafetyShutdown() {
  safety_shutdown = 1;
  is_shutdown = 1;
  HAL_GPIO_WritePin(AMP_RESET_N_GPIO_Port, AMP_RESET_N_Pin, GPIO_PIN_RESET);
}

//perform threshold checks for main loop
void _SAFETY_DoLoopChecks() {
  float current_sum_0s1 = 0.0f, real_power_sum_0s1 = 0.0f, apparent_power_sum_0s1 = 0.0f, current_sum_1s0 = 0.0f, real_power_sum_1s0 = 0.0f, apparent_power_sum_1s0 = 0.0f;
  int i;

//loop to get and check channel values
#ifdef MAIN_FOUR_CHANNEL
  for (i = 0; i < 4; i++) {
#else
  for (i = 0; i < 3; i += 2) {
#endif
    float current_0s1 = rms_current_0s1[i];
    float current_1s0 = rms_current_1s0[i];
    float real_power_0s1 = avg_real_power_0s1[i];
    float real_power_1s0 = avg_real_power_1s0[i];
    float apparent_power_0s1 = avg_apparent_power_0s1[i];
    float apparent_power_1s0 = avg_apparent_power_1s0[i];

    //check 0.1s max
    if (current_0s1 > safety_max_current_0s1[i] || real_power_0s1 > safety_max_real_power_0s1[i] || apparent_power_0s1 > safety_max_apparent_power_0s1[i]) {
      DEBUG_PRINTF("ERROR: Safety loop - fast maximum exceeded, channel %d: %f A, %f W, %f VA\n", i, current_0s1, real_power_0s1, apparent_power_0s1);
      //TODO: indicate error on I2C bus
      _SAFETY_TriggerSafetyShutdown();
      return;
    }

    //check 1.0s max
    if (current_1s0 > safety_max_current_1s0[i] || real_power_1s0 > safety_max_real_power_1s0[i] || apparent_power_1s0 > safety_max_apparent_power_1s0[i]) {
      DEBUG_PRINTF("ERROR: Safety loop - slow maximum exceeded, channel %d: %f A, %f W, %f VA\n", i, current_1s0, real_power_1s0, apparent_power_1s0);
      //TODO: indicate error on I2C bus
      _SAFETY_TriggerSafetyShutdown();
      return;
    }

    //check 0.1s warning
    if (current_0s1 > safety_warn_current_0s1[i] || real_power_0s1 > safety_warn_real_power_0s1[i] || apparent_power_0s1 > safety_warn_apparent_power_0s1[i]) {
      DEBUG_PRINTF("WARNING: Safety loop - fast warn level exceeded, channel %d: %f A, %f W, %f VA\n", i, current_0s1, real_power_0s1, apparent_power_0s1);
      //TODO: indicate warning on I2C bus
    }

    //check 1.0s warning
    if (current_1s0 > safety_warn_current_1s0[i] || real_power_1s0 > safety_warn_real_power_1s0[i] || apparent_power_1s0 > safety_warn_apparent_power_1s0[i]) {
      DEBUG_PRINTF("WARNING: Safety loop - slow warn level exceeded, channel %d: %f A, %f W, %f VA\n", i, current_1s0, real_power_1s0, apparent_power_1s0);
      //TODO: indicate warning on I2C bus
    }

    current_sum_0s1 += current_0s1;
    current_sum_1s0 += current_1s0;
    real_power_sum_0s1 += real_power_0s1;
    real_power_sum_1s0 += real_power_1s0;
    apparent_power_sum_0s1 += apparent_power_0s1;
    apparent_power_sum_1s0 += apparent_power_1s0;
  }

  //check sum values (threshold index 4)
  //check 0.1s max
  if (current_sum_0s1 > safety_max_current_0s1[4] || real_power_sum_0s1 > safety_max_real_power_0s1[4] || apparent_power_sum_0s1 > safety_max_apparent_power_0s1[4]) {
    DEBUG_PRINTF("ERROR: Safety loop - fast maximum exceeded, channel sum: %f A, %f W, %f VA\n", current_sum_0s1, real_power_sum_0s1, apparent_power_sum_0s1);
    //TODO: indicate error on I2C bus
    _SAFETY_TriggerSafetyShutdown();
    return;
  }

  //check 1.0s max
  if (current_sum_1s0 > safety_max_current_1s0[4] || real_power_sum_1s0 > safety_max_real_power_1s0[4] || apparent_power_sum_1s0 > safety_max_apparent_power_1s0[4]) {
    DEBUG_PRINTF("ERROR: Safety loop - slow maximum exceeded, channel sum: %f A, %f W, %f VA\n", current_sum_1s0, real_power_sum_1s0, apparent_power_sum_1s0);
    //TODO: indicate error on I2C bus
    _SAFETY_TriggerSafetyShutdown();
    return;
  }

  //check 0.1s warning
  if (current_sum_0s1 > safety_warn_current_0s1[4] || real_power_sum_0s1 > safety_warn_real_power_0s1[4] || apparent_power_sum_0s1 > safety_warn_apparent_power_0s1[4]) {
    DEBUG_PRINTF("WARNING: Safety loop - fast warn level exceeded, channel sum: %f A, %f W, %f VA\n", current_sum_0s1, real_power_sum_0s1, apparent_power_sum_0s1);
    //TODO: indicate warning on I2C bus
  }

  //check 1.0s warning
  if (current_sum_1s0 > safety_warn_current_1s0[4] || real_power_sum_1s0 > safety_warn_real_power_1s0[4] || apparent_power_sum_1s0 > safety_warn_apparent_power_1s0[4]) {
    DEBUG_PRINTF("WARNING: Safety loop - slow warn level exceeded, channel sum: %f A, %f W, %f VA\n", current_sum_1s0, real_power_sum_1s0, apparent_power_sum_1s0);
    //TODO: indicate warning on I2C bus
  }
}


/**
 * enable(1)/disable(0) manual shutdown of amplifier.
 * enabling manual shutdown also resets prior safety shutdown, allowing amp operation once manual shutdown is disabled again
 */
void SAFETY_SetManualShutdown(uint8_t shutdown) {
  //just set shutdown variable, leave actual pin updating to main loop
  manual_shutdown = shutdown > 0;

  //when enabling manual shutdown: reset safety shutdown
  if (shutdown > 0) safety_shutdown = 0;
}

/**
 * check instantaneous (last batch) measurements for safety - called after every ADC batch
 */
void SAFETY_CheckADCInstValues() {
  if (_SAFETY_SanityCheckFastLimits() != HAL_OK) {
    DEBUG_PRINTF("ERROR: Safety inst value check - limit sanity check failed\n");
    //TODO: indicate error on I2C bus
    _SAFETY_TriggerSafetyShutdown();
    _SAFETY_ResetLimits();
    return;
  }

  float current_sum = 0.0f, real_power_sum = 0.0f, apparent_power_sum = 0.0f;
  int i;

  //loop to get and check channel values
#ifdef MAIN_FOUR_CHANNEL
  for (i = 0; i < 4; i++) {
#else
  for (i = 0; i < 3; i += 2) {
#endif
    float current = rms_current_inst[i];
    float real_power = avg_real_power_inst[i];
    float apparent_power = avg_apparent_power_inst[i];

    //check max
    if (current > safety_max_current_inst[i] || real_power > safety_max_real_power_inst[i] || apparent_power > safety_max_apparent_power_inst[i]) {
      DEBUG_PRINTF("ERROR: Safety inst value check - maximum exceeded, channel %d: %f A, %f W, %f VA\n", i, current, real_power, apparent_power);
      //TODO: indicate error on I2C bus
      _SAFETY_TriggerSafetyShutdown();
      return;
    }

    //check warning
    if (current > safety_warn_current_inst[i] || real_power > safety_warn_real_power_inst[i] || apparent_power > safety_warn_apparent_power_inst[i]) {
      DEBUG_PRINTF("WARNING: Safety inst value check - warn level exceeded, channel %d: %f A, %f W, %f VA\n", i, current, real_power, apparent_power);
      //TODO: indicate warning on I2C bus
    }

    current_sum += current;
    real_power_sum += real_power;
    apparent_power_sum += apparent_power;
  }

  //check sum values (threshold index 4)
  //check max
  if (current_sum > safety_max_current_inst[4] || real_power_sum > safety_max_real_power_inst[4] || apparent_power_sum > safety_max_apparent_power_inst[4]) {
    DEBUG_PRINTF("ERROR: Safety inst value check - maximum exceeded, channel sum: %f A, %f W, %f VA\n", current_sum, real_power_sum, apparent_power_sum);
    //TODO: indicate error on I2C bus
    _SAFETY_TriggerSafetyShutdown();
    return;
  }

  //check warning
  if (current_sum > safety_warn_current_inst[4] || real_power_sum > safety_warn_real_power_inst[4] || apparent_power_sum > safety_warn_apparent_power_inst[4]) {
    DEBUG_PRINTF("WARNING: Safety inst value check - warn level exceeded, channel sum: %f A, %f W, %f VA\n", current_sum, real_power_sum, apparent_power_sum);
    //TODO: indicate warning on I2C bus
  }
}

HAL_StatusTypeDef SAFETY_Init() {
  //enforce initial shutdown
  _SAFETY_TriggerSafetyShutdown();

  //start with reset of threshold values to maximum limits (defaults)
  _SAFETY_ResetLimits();

  //delay to settle smoothed ADC measurements - 500ms for full settling of 0.1s time constant
  HAL_Delay(500);

  //check that initial voltage and current are within limits
  float max_current, max_voltage;
  arm_absmax_no_idx_f32(rms_current_0s1, 4, &max_current);
  arm_absmax_no_idx_f32(rms_voltage_0s1, 4, &max_voltage);
  if (max_current > SAFETY_MAX_INIT_CURRENT || max_voltage > SAFETY_MAX_INIT_VOLTAGE) {
    DEBUG_PRINTF("ERROR: Safety init - maximum initial measurements exceeded: %f A, %f V\n", max_current, max_voltage);
    //TODO: indicate error on I2C bus
    return HAL_ERROR;
  }

  //init passed: remove safety shutdown, activation of amplifier is left up to main loop
  safety_shutdown = 0;

  return HAL_OK;
}

void SAFETY_LoopUpdate() {
  //start with sanity check before doing anything else
  if (_SAFETY_SanityCheckLimits() != HAL_OK) {
    DEBUG_PRINTF("ERROR: Safety loop - limit sanity check failed\n");
    //TODO: indicate error on I2C bus
    _SAFETY_TriggerSafetyShutdown();
    _SAFETY_ResetLimits();
  } else { //if sanity check passes: do threshold checks
    _SAFETY_DoLoopChecks();
  }

  //process amp shutdown logic
  is_shutdown = safety_shutdown || manual_shutdown || !pvdd_valid_voltage;
  HAL_GPIO_WritePin(AMP_RESET_N_GPIO_Port, AMP_RESET_N_Pin, is_shutdown ? GPIO_PIN_RESET : GPIO_PIN_SET);
}
