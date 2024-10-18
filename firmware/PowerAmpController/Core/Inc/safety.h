/*
 * safety.h
 *
 *  Created on: Oct 11, 2024
 *      Author: Alex
 */

#ifndef INC_SAFETY_H_
#define INC_SAFETY_H_

#include "main.h"
#include <math.h>

//absolute maximum safety limits (defined by circuit design), see variables below
//explanation: circuit rated for 200W/channel max (inductor + trace thermal limitation) and 270W total max (IC thermal/cooling limitation)
//current limits given extra headroom for inductive effects; apparent power limits are not mandatory
#define SAFETY_LIMIT_MAX_CURRENT_INST { 12.0f, 12.0f, 12.0f, 12.0f, 15.0f }
#define SAFETY_LIMIT_MAX_CURRENT_0S1 { 9.5f, 9.5f, 9.5f, 9.5f, 11.0f }
#define SAFETY_LIMIT_MAX_CURRENT_1S0 { 7.5f, 7.5f, 7.5f, 7.5f, 9.0f }
#define SAFETY_LIMIT_MAX_REAL_POWER_INST { 300.0f, 300.0f, 300.0f, 300.0f, 500.0f }
#define SAFETY_LIMIT_MAX_REAL_POWER_0S1 { 230.0f, 230.0f, 230.0f, 230.0f, 350.0f }
#define SAFETY_LIMIT_MAX_REAL_POWER_1S0 { 200.0f, 200.0f, 200.0f, 200.0f, 270.0f }
#define SAFETY_LIMIT_MAX_APPARENT_POWER_INST { INFINITY, INFINITY, INFINITY, INFINITY, INFINITY }
#define SAFETY_LIMIT_MAX_APPARENT_POWER_0S1 { INFINITY, INFINITY, INFINITY, INFINITY, INFINITY }
#define SAFETY_LIMIT_MAX_APPARENT_POWER_1S0 { INFINITY, INFINITY, INFINITY, INFINITY, INFINITY }

//default value for warning limits - warnings disabled by default
#define SAFETY_NO_WARN { INFINITY, INFINITY, INFINITY, INFINITY, INFINITY }

//maximum current magnitude during init (amp shut down)
#define SAFETY_MAX_INIT_CURRENT 0.05f
//maximum voltage magnitude during init (amp shut down)
#define SAFETY_MAX_INIT_VOLTAGE 0.5f


//safety limits for current, real power, and apparent power - shutdown if exceeded
//inst = single ADC batch (fast), 0s1 = 0.1s time constant EMA, 1s0 = 1.0s time constant EMA
//entries 0-3 are channel-wise (A-D, BTL mode uses A and C only), entry 4 is sum of all channels
extern float safety_max_current_inst[5];
extern float safety_max_current_0s1[5];
extern float safety_max_current_1s0[5];
extern float safety_max_real_power_inst[5];
extern float safety_max_real_power_0s1[5];
extern float safety_max_real_power_1s0[5];
extern float safety_max_apparent_power_inst[5];
extern float safety_max_apparent_power_0s1[5];
extern float safety_max_apparent_power_1s0[5];
//constant limit maximums for sanity checking and reset
extern const float safety_limit_current_inst[5];
extern const float safety_limit_current_0s1[5];
extern const float safety_limit_current_1s0[5];
extern const float safety_limit_real_power_inst[5];
extern const float safety_limit_real_power_0s1[5];
extern const float safety_limit_real_power_1s0[5];
extern const float safety_limit_apparent_power_inst[5];
extern const float safety_limit_apparent_power_0s1[5];
extern const float safety_limit_apparent_power_1s0[5];
//similar to above, but lower "warning" limits - notify I2C bus if exceeded
extern float safety_warn_current_inst[5];
extern float safety_warn_current_0s1[5];
extern float safety_warn_current_1s0[5];
extern float safety_warn_real_power_inst[5];
extern float safety_warn_real_power_0s1[5];
extern float safety_warn_real_power_1s0[5];
extern float safety_warn_apparent_power_inst[5];
extern float safety_warn_apparent_power_0s1[5];
extern float safety_warn_apparent_power_1s0[5];

//DO NOT SET THESE DIRECTLY - READ ONLY
//1 = amp shut down (by force or error condition), 0 = amp allowed to run
extern uint8_t is_shutdown;
//safety shutdown set
extern uint8_t safety_shutdown;
//manual shutdown set
extern uint8_t manual_shutdown;
//error status: sources and channels, as defined in I2C defines file
extern uint16_t safety_err_status;
//warning status, like error status - separate ones for inst and loop measurement cycles
extern uint16_t safety_warn_status_inst;
extern uint16_t safety_warn_status_loop;


/**
 * enable(1)/disable(0) manual shutdown of amplifier.
 * enabling manual shutdown also resets prior safety shutdown, allowing amp operation once manual shutdown is disabled again
 */
void SAFETY_SetManualShutdown(uint8_t shutdown);

/**
 * check instantaneous (last batch) measurements for safety - called after every ADC batch
 */
void SAFETY_CheckADCInstValues();

HAL_StatusTypeDef SAFETY_Init();
void SAFETY_LoopUpdate();


#endif /* INC_SAFETY_H_ */
