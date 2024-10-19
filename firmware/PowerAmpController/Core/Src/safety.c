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
#include "i2c.h"


#define _SAFETY_CHECK_LIMITS(x,m,i) do { for (i = 0; i < 5; i++) { if (x[i] < 0.0f || x[i] > m[i]) return HAL_ERROR; } } while (0)


//safety thresholds for current, real power, and apparent power - shutdown if exceeded
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
//constant maximum limits for sanity checking and reset
const float safety_limit_current_inst[5] = SAFETY_LIMIT_MAX_CURRENT_INST;
const float safety_limit_current_0s1[5] = SAFETY_LIMIT_MAX_CURRENT_0S1;
const float safety_limit_current_1s0[5] = SAFETY_LIMIT_MAX_CURRENT_1S0;
const float safety_limit_real_power_inst[5] = SAFETY_LIMIT_MAX_REAL_POWER_INST;
const float safety_limit_real_power_0s1[5] = SAFETY_LIMIT_MAX_REAL_POWER_0S1;
const float safety_limit_real_power_1s0[5] = SAFETY_LIMIT_MAX_REAL_POWER_1S0;
const float safety_limit_apparent_power_inst[5] = SAFETY_LIMIT_MAX_APPARENT_POWER_INST;
const float safety_limit_apparent_power_0s1[5] = SAFETY_LIMIT_MAX_APPARENT_POWER_0S1;
const float safety_limit_apparent_power_1s0[5] = SAFETY_LIMIT_MAX_APPARENT_POWER_1S0;
//similar to above, but lower "warning" thresholds - notify I2C bus if exceeded
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
uint8_t safety_shutdown = 1;
//manual shutdown set
uint8_t manual_shutdown = 1;
//error status
uint16_t safety_err_status = 0;
uint16_t safety_warn_status_inst = 0;
uint16_t safety_warn_status_loop = 0;

//helper arrays to translate from channel index (0 = A, ..., 3 = D) to corresponding status register bits (as defined in I2C defines)
static const uint16_t safety_channel_err_bits[] = { I2CDEF_POWERAMP_SERR_SOURCE_CHAN_A, I2CDEF_POWERAMP_SERR_SOURCE_CHAN_B, I2CDEF_POWERAMP_SERR_SOURCE_CHAN_C, I2CDEF_POWERAMP_SERR_SOURCE_CHAN_D };
static const uint16_t safety_channel_warn_bits[] = { I2CDEF_POWERAMP_SWARN_SOURCE_CHAN_A, I2CDEF_POWERAMP_SWARN_SOURCE_CHAN_B, I2CDEF_POWERAMP_SWARN_SOURCE_CHAN_C, I2CDEF_POWERAMP_SWARN_SOURCE_CHAN_D };


//perform sanity check that configured fast thresholds are within the valid ranges
HAL_StatusTypeDef _SAFETY_SanityCheckFastLimits() {
  int i;
  _SAFETY_CHECK_LIMITS(safety_max_current_inst, safety_limit_current_inst, i);
  _SAFETY_CHECK_LIMITS(safety_max_real_power_inst, safety_limit_real_power_inst, i);
  _SAFETY_CHECK_LIMITS(safety_max_apparent_power_inst, safety_limit_apparent_power_inst, i);
  return HAL_OK;
}

//perform sanity check that all configured thresholds are within the valid ranges
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

//reset maximum thresholds to default maximum limits
void _SAFETY_ResetThresholds() {
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

  //reset loop warnings
  safety_warn_status_loop = 0;

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

    //check max
    _Bool imaxf = current_0s1 > safety_max_current_0s1[i];
    _Bool imaxl = current_1s0 > safety_max_current_1s0[i];
    _Bool pmaxf = real_power_0s1 > safety_max_real_power_0s1[i];
    _Bool pmaxl = real_power_1s0 > safety_max_real_power_1s0[i];
    _Bool amaxf = apparent_power_0s1 > safety_max_apparent_power_0s1[i];
    _Bool amaxl = apparent_power_1s0 > safety_max_apparent_power_1s0[i];
    if (imaxf || imaxl || pmaxf || pmaxl || amaxf || amaxl) {
      //DEBUG_PRINTF("ERROR: Safety loop - maximum exceeded, channel %d: %f A, %f W, %f VA\n", i, current_0s1, real_power_0s1, apparent_power_0s1);
      if (imaxf) {
	safety_err_status |= I2CDEF_POWERAMP_SERR_SOURCE_MTYPE_IRMS_FAST;
      }
      if (imaxl) {
	safety_err_status |= I2CDEF_POWERAMP_SERR_SOURCE_MTYPE_IRMS_SLOW;
      }
      if (pmaxf) {
	safety_err_status |= I2CDEF_POWERAMP_SERR_SOURCE_MTYPE_PAVG_FAST;
      }
      if (pmaxl) {
	safety_err_status |= I2CDEF_POWERAMP_SERR_SOURCE_MTYPE_PAVG_SLOW;
      }
      if (amaxf) {
	safety_err_status |= I2CDEF_POWERAMP_SERR_SOURCE_MTYPE_PAPP_FAST;
      }
      if (amaxl) {
	safety_err_status |= I2CDEF_POWERAMP_SERR_SOURCE_MTYPE_PAPP_SLOW;
      }
      safety_err_status |= safety_channel_err_bits[i];
      _SAFETY_TriggerSafetyShutdown();
      I2C_TriggerInterrupt(I2CDEF_POWERAMP_INT_FLAGS_INT_SERR_Msk);
      return;
    }

    //check warning
    _Bool iwarnf = current_0s1 > safety_warn_current_0s1[i];
    _Bool iwarnl = current_1s0 > safety_warn_current_1s0[i];
    _Bool pwarnf = real_power_0s1 > safety_warn_real_power_0s1[i];
    _Bool pwarnl = real_power_1s0 > safety_warn_real_power_1s0[i];
    _Bool awarnf = apparent_power_0s1 > safety_warn_apparent_power_0s1[i];
    _Bool awarnl = apparent_power_1s0 > safety_warn_apparent_power_1s0[i];
    if (iwarnf || iwarnl || pwarnf || pwarnl || awarnf || awarnl) {
      //DEBUG_PRINTF("WARNING: Safety loop - warn level exceeded, channel %d: %f A, %f W, %f VA\n", i, current_0s1, real_power_0s1, apparent_power_0s1);
      if (iwarnf) {
	safety_warn_status_loop |= I2CDEF_POWERAMP_SWARN_SOURCE_MTYPE_IRMS_FAST;
      }
      if (iwarnl) {
	safety_warn_status_loop |= I2CDEF_POWERAMP_SWARN_SOURCE_MTYPE_IRMS_SLOW;
      }
      if (pwarnf) {
	safety_warn_status_loop |= I2CDEF_POWERAMP_SWARN_SOURCE_MTYPE_PAVG_FAST;
      }
      if (pwarnl) {
	safety_warn_status_loop |= I2CDEF_POWERAMP_SWARN_SOURCE_MTYPE_PAVG_SLOW;
      }
      if (awarnf) {
	safety_warn_status_loop |= I2CDEF_POWERAMP_SWARN_SOURCE_MTYPE_PAPP_FAST;
      }
      if (awarnl) {
	safety_warn_status_loop |= I2CDEF_POWERAMP_SWARN_SOURCE_MTYPE_PAPP_SLOW;
      }
      safety_warn_status_loop |= safety_channel_warn_bits[i];
      I2C_TriggerInterrupt(I2CDEF_POWERAMP_INT_FLAGS_INT_SWARN_Msk);
    }

    current_sum_0s1 += current_0s1;
    current_sum_1s0 += current_1s0;
    real_power_sum_0s1 += real_power_0s1;
    real_power_sum_1s0 += real_power_1s0;
    apparent_power_sum_0s1 += apparent_power_0s1;
    apparent_power_sum_1s0 += apparent_power_1s0;
  }

  //check sum values (threshold index 4)
  //check max
  _Bool imaxfs = current_sum_0s1 > safety_max_current_0s1[4];
  _Bool imaxls = current_sum_1s0 > safety_max_current_1s0[4];
  _Bool pmaxfs = real_power_sum_0s1 > safety_max_real_power_0s1[4];
  _Bool pmaxls = real_power_sum_1s0 > safety_max_real_power_1s0[4];
  _Bool amaxfs = apparent_power_sum_0s1 > safety_max_apparent_power_0s1[4];
  _Bool amaxls = apparent_power_sum_1s0 > safety_max_apparent_power_1s0[4];
  if (imaxfs || imaxls || pmaxfs || pmaxls || amaxfs || amaxls) {
    //DEBUG_PRINTF("ERROR: Safety loop - maximum exceeded, channel sum: %f A, %f W, %f VA\n", current_sum_0s1, real_power_sum_0s1, apparent_power_sum_0s1);
    if (imaxfs) {
      safety_err_status |= I2CDEF_POWERAMP_SERR_SOURCE_MTYPE_IRMS_FAST;
    }
    if (imaxls) {
      safety_err_status |= I2CDEF_POWERAMP_SERR_SOURCE_MTYPE_IRMS_SLOW;
    }
    if (pmaxfs) {
      safety_err_status |= I2CDEF_POWERAMP_SERR_SOURCE_MTYPE_PAVG_FAST;
    }
    if (pmaxls) {
      safety_err_status |= I2CDEF_POWERAMP_SERR_SOURCE_MTYPE_PAVG_SLOW;
    }
    if (amaxfs) {
      safety_err_status |= I2CDEF_POWERAMP_SERR_SOURCE_MTYPE_PAPP_FAST;
    }
    if (amaxls) {
      safety_err_status |= I2CDEF_POWERAMP_SERR_SOURCE_MTYPE_PAPP_SLOW;
    }
    safety_err_status |= I2CDEF_POWERAMP_SERR_SOURCE_CHAN_SUM;
    _SAFETY_TriggerSafetyShutdown();
    I2C_TriggerInterrupt(I2CDEF_POWERAMP_INT_FLAGS_INT_SERR_Msk);
    return;
  }

  //check warning
  _Bool iwarnfs = current_sum_0s1 > safety_warn_current_0s1[4];
  _Bool iwarnls = current_sum_1s0 > safety_warn_current_1s0[4];
  _Bool pwarnfs = real_power_sum_0s1 > safety_warn_real_power_0s1[4];
  _Bool pwarnls = real_power_sum_1s0 > safety_warn_real_power_1s0[4];
  _Bool awarnfs = apparent_power_sum_0s1 > safety_warn_apparent_power_0s1[4];
  _Bool awarnls = apparent_power_sum_1s0 > safety_warn_apparent_power_1s0[4];
  if (iwarnfs || iwarnls || pwarnfs || pwarnls || awarnfs || awarnls) {
    //DEBUG_PRINTF("WARNING: Safety loop - warn level exceeded, channel sum: %f A, %f W, %f VA\n", current_sum_0s1, real_power_sum_0s1, apparent_power_sum_0s1);
    if (iwarnfs) {
      safety_warn_status_loop |= I2CDEF_POWERAMP_SWARN_SOURCE_MTYPE_IRMS_FAST;
    }
    if (iwarnls) {
      safety_warn_status_loop |= I2CDEF_POWERAMP_SWARN_SOURCE_MTYPE_IRMS_SLOW;
    }
    if (pwarnfs) {
      safety_warn_status_loop |= I2CDEF_POWERAMP_SWARN_SOURCE_MTYPE_PAVG_FAST;
    }
    if (pwarnls) {
      safety_warn_status_loop |= I2CDEF_POWERAMP_SWARN_SOURCE_MTYPE_PAVG_SLOW;
    }
    if (awarnfs) {
      safety_warn_status_loop |= I2CDEF_POWERAMP_SWARN_SOURCE_MTYPE_PAPP_FAST;
    }
    if (awarnls) {
      safety_warn_status_loop |= I2CDEF_POWERAMP_SWARN_SOURCE_MTYPE_PAPP_SLOW;
    }
    safety_warn_status_loop |= I2CDEF_POWERAMP_SWARN_SOURCE_CHAN_SUM;
    I2C_TriggerInterrupt(I2CDEF_POWERAMP_INT_FLAGS_INT_SWARN_Msk);
  }
}


/**
 * enable(1)/disable(0) manual shutdown of amplifier.
 * enabling manual shutdown also resets prior safety shutdown, allowing amp operation once manual shutdown is disabled again
 */
void SAFETY_SetManualShutdown(uint8_t shutdown) {
  //just set shutdown variable, leave actual pin updating to main loop
  manual_shutdown = shutdown > 0 ? 1 : 0;

  //when enabling manual shutdown: reset safety shutdown and errors
  if (shutdown > 0) {
    safety_shutdown = 0;
    safety_err_status = 0;
  }
}

/**
 * check instantaneous (last batch) measurements for safety - called after every ADC batch
 */
void SAFETY_CheckADCInstValues() {
  if (_SAFETY_SanityCheckFastLimits() != HAL_OK) {
    DEBUG_PRINTF("ERROR: Safety inst value check - limit sanity check failed\n");
    safety_err_status = I2CDEF_POWERAMP_SERR_SOURCE_MTYPE_Msk; //all source types but no channels - special value for failed sanity check
    _SAFETY_TriggerSafetyShutdown();
    _SAFETY_ResetThresholds();
    I2C_TriggerInterrupt(I2CDEF_POWERAMP_INT_FLAGS_INT_SERR_Msk);
    return;
  }

  float current_sum = 0.0f, real_power_sum = 0.0f, apparent_power_sum = 0.0f;
  int i;

  //reset inst warnings
  safety_warn_status_inst = 0;

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
    _Bool imax = current > safety_max_current_inst[i];
    _Bool pmax = real_power > safety_max_real_power_inst[i];
    _Bool amax = apparent_power > safety_max_apparent_power_inst[i];
    if (imax || pmax || amax) {
      //DEBUG_PRINTF("ERROR: Safety inst value check - maximum exceeded, channel %d: %f A, %f W, %f VA\n", i, current, real_power, apparent_power);
      if (imax) {
	safety_err_status |= I2CDEF_POWERAMP_SERR_SOURCE_MTYPE_IRMS_INST;
      }
      if (pmax) {
	safety_err_status |= I2CDEF_POWERAMP_SERR_SOURCE_MTYPE_PAVG_INST;
      }
      if (amax) {
	safety_err_status |= I2CDEF_POWERAMP_SERR_SOURCE_MTYPE_PAPP_INST;
      }
      safety_err_status |= safety_channel_err_bits[i];
      _SAFETY_TriggerSafetyShutdown();
      I2C_TriggerInterrupt(I2CDEF_POWERAMP_INT_FLAGS_INT_SERR_Msk);
      return;
    }

    //check warning
    _Bool iwarn = current > safety_warn_current_inst[i];
    _Bool pwarn = real_power > safety_warn_real_power_inst[i];
    _Bool awarn = apparent_power > safety_warn_apparent_power_inst[i];
    if (iwarn || pwarn || awarn) {
      //DEBUG_PRINTF("WARNING: Safety inst value check - warn level exceeded, channel %d: %f A, %f W, %f VA\n", i, current, real_power, apparent_power);
      if (iwarn) {
	safety_warn_status_inst |= I2CDEF_POWERAMP_SWARN_SOURCE_MTYPE_IRMS_INST;
      }
      if (pwarn) {
	safety_warn_status_inst |= I2CDEF_POWERAMP_SWARN_SOURCE_MTYPE_PAVG_INST;
      }
      if (awarn) {
	safety_warn_status_inst |= I2CDEF_POWERAMP_SWARN_SOURCE_MTYPE_PAPP_INST;
      }
      safety_warn_status_inst |= safety_channel_warn_bits[i];
      I2C_TriggerInterrupt(I2CDEF_POWERAMP_INT_FLAGS_INT_SWARN_Msk);
    }

    current_sum += current;
    real_power_sum += real_power;
    apparent_power_sum += apparent_power;
  }

  //check sum values (threshold index 4)
  //check max
  _Bool imaxs = current_sum > safety_max_current_inst[4];
  _Bool pmaxs = real_power_sum > safety_max_real_power_inst[4];
  _Bool amaxs = apparent_power_sum > safety_max_apparent_power_inst[4];
  if (imaxs || pmaxs || amaxs) {
    //DEBUG_PRINTF("ERROR: Safety inst value check - maximum exceeded, channel sum: %f A, %f W, %f VA\n", current_sum, real_power_sum, apparent_power_sum);
    if (imaxs) {
      safety_err_status |= I2CDEF_POWERAMP_SERR_SOURCE_MTYPE_IRMS_INST;
    }
    if (pmaxs) {
      safety_err_status |= I2CDEF_POWERAMP_SERR_SOURCE_MTYPE_PAVG_INST;
    }
    if (amaxs) {
      safety_err_status |= I2CDEF_POWERAMP_SERR_SOURCE_MTYPE_PAPP_INST;
    }
    safety_err_status |= I2CDEF_POWERAMP_SERR_SOURCE_CHAN_SUM;
    _SAFETY_TriggerSafetyShutdown();
    I2C_TriggerInterrupt(I2CDEF_POWERAMP_INT_FLAGS_INT_SERR_Msk);
    return;
  }

  //check warning
  _Bool iwarns = current_sum > safety_warn_current_inst[4];
  _Bool pwarns = real_power_sum > safety_warn_real_power_inst[4];
  _Bool awarns = apparent_power_sum > safety_warn_apparent_power_inst[4];
  if (iwarns || pwarns || awarns) {
    //DEBUG_PRINTF("WARNING: Safety inst value check - warn level exceeded, channel sum: %f A, %f W, %f VA\n", current_sum, real_power_sum, apparent_power_sum);
    if (iwarns) {
      safety_warn_status_inst |= I2CDEF_POWERAMP_SWARN_SOURCE_MTYPE_IRMS_INST;
    }
    if (pwarns) {
      safety_warn_status_inst |= I2CDEF_POWERAMP_SWARN_SOURCE_MTYPE_PAVG_INST;
    }
    if (awarns) {
      safety_warn_status_inst |= I2CDEF_POWERAMP_SWARN_SOURCE_MTYPE_PAPP_INST;
    }
    safety_warn_status_inst |= I2CDEF_POWERAMP_SWARN_SOURCE_CHAN_SUM;
    I2C_TriggerInterrupt(I2CDEF_POWERAMP_INT_FLAGS_INT_SWARN_Msk);
  }
}

HAL_StatusTypeDef SAFETY_Init() {
  //enforce initial shutdown
  _SAFETY_TriggerSafetyShutdown();

  //start with reset of threshold values to maximum limits (defaults)
  _SAFETY_ResetThresholds();

  //delay to settle smoothed ADC measurements - 500ms for full settling of 0.1s time constant
  HAL_Delay(500);

  //check that initial voltage and current are within limits
  float max_current, max_voltage;
  arm_absmax_no_idx_f32(rms_current_0s1, 4, &max_current);
  arm_absmax_no_idx_f32(rms_voltage_0s1, 4, &max_voltage);
  if (max_current > SAFETY_MAX_INIT_CURRENT || max_voltage > SAFETY_MAX_INIT_VOLTAGE) {
    DEBUG_PRINTF("ERROR: Safety init - maximum initial measurements exceeded: %f A, %f V\n", max_current, max_voltage);
    I2C_TriggerInterrupt(I2CDEF_POWERAMP_INT_FLAGS_INT_SERR_Msk);
    return HAL_ERROR;
  }

  //reset error and warning status
  safety_err_status = 0;
  safety_warn_status_inst = 0;
  safety_warn_status_loop = 0;

  //init passed: remove safety shutdown, activation of amplifier is left up to main loop
  safety_shutdown = 0;

  return HAL_OK;
}

void SAFETY_LoopUpdate() {
  //start with sanity check before doing anything else
  if (_SAFETY_SanityCheckLimits() != HAL_OK) {
    DEBUG_PRINTF("ERROR: Safety loop - limit sanity check failed\n");
    safety_err_status = I2CDEF_POWERAMP_SERR_SOURCE_MTYPE_Msk; //all source types but no channels - special value for failed sanity check
    _SAFETY_TriggerSafetyShutdown();
    _SAFETY_ResetThresholds();
    I2C_TriggerInterrupt(I2CDEF_POWERAMP_INT_FLAGS_INT_SERR_Msk);
  } else { //if sanity check passes: do threshold checks
    _SAFETY_DoLoopChecks();
  }

  //process amp shutdown logic
  is_shutdown = safety_shutdown || manual_shutdown || !pvdd_valid_voltage;
  HAL_GPIO_WritePin(AMP_RESET_N_GPIO_Port, AMP_RESET_N_Pin, is_shutdown ? GPIO_PIN_RESET : GPIO_PIN_SET);
}
