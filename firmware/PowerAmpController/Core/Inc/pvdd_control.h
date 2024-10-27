/*
 * pvdd_control.h
 *
 *  Created on: Oct 7, 2024
 *      Author: Alex
 */

#ifndef INC_PVDD_CONTROL_H_
#define INC_PVDD_CONTROL_H_

#include "main.h"
#include "adc.h"


//minimum and maximum allowable requested voltages
#define PVDD_MIN_VOLTAGE 18.0f
#define PVDD_MAX_VOLTAGE 53.5f

//system constants that determine unit conversion factors - adjust these for calibration
//VREF = VDDA voltage - theoretical value equal to ADC VREF
#define PVDD_DAC_VREF P_ADC_VREF
//PVDD measurement ADC gain - theoretical value 11/211 ~= 0.0521327 V/V
#define PVDD_ADC_GAIN 0.0521327f
//PVDD supply tracking slope - theoretical value 10.873 V/V (see PowerAmpSupply schematic)
#define PVDD_TRK_SLOPE 10.873f
//PVDD supply tracking intercept - theoretical value 17.918 V (see PowerAmpSupply schematic)
#define PVDD_TRK_INTERCEPT 17.918f

//unit conversion factors
//ADC: fractional ADC reading [0, 1] -> measured voltage [V]
#define PVDD_ADC_FACTOR (P_ADC_VREF / PVDD_ADC_GAIN)
//DAC: requested voltage above intercept [V] -> right-aligned DAC value [0, 4096)
#define PVDD_DAC_FACTOR (4095.99f / (PVDD_TRK_SLOPE * PVDD_DAC_VREF))

//voltage offset lock-out time after new voltage request, in milliseconds - for current PowerAmpSupply design, <200ms to get within 0.1V of track voltage
#define PVDD_OFFSET_LOCKOUT_MS 300
//lock-out time in main loop cycles - plus some cycles to allow for smoothing
#define PVDD_OFFSET_LOCKOUT_CYCLES (PVDD_OFFSET_LOCKOUT_MS / MAIN_LOOP_PERIOD_MS + 10)
//lock-out time after small offset adjustment, in main loop cycles
#define PVDD_OFFSET_LOCKOUT_SHORT_CYCLES 3

//EMA alpha and 1-alpha values for voltage measurement smoothing
//calculation: 1-alpha = e^(-t_loop_period / t_constant)
//given values here calculated for t_loop_period = 10ms and t_constant ~= 500ms
#define PVDD_EMA_1MALPHA 0.98f
#define PVDD_EMA_ALPHA (1.0f - PVDD_EMA_1MALPHA)

//maximum acceptable voltage that doesn't result in overvoltage error (expected overvoltage when supply input is at/above requested voltage)
#define PVDD_VOLTAGE_MAX_NOERROR 25.0f
//voltage error above which "fail" condition is triggered (causing PVDD control reset)
#define PVDD_VOLTAGE_ERROR_FAIL 2.0f
//voltage error which causes a correction attempt using offset
#define PVDD_VOLTAGE_ERROR_CORRECT 0.1f
//voltage offset step size for correction attempts
#define PVDD_VOLTAGE_OFFSET_STEP 0.1f
//voltage offset maximum magnitude
#define PVDD_VOLTAGE_OFFSET_MAX 1.0f

//DO NOT SET THESE DIRECTLY - READ ONLY
extern float pvdd_voltage_requested;
extern float pvdd_voltage_request_offset;

extern float pvdd_voltage_measured;
//1 = voltage is valid (measured ~= requested), 0 = voltage is invalid
extern uint8_t pvdd_valid_voltage;


HAL_StatusTypeDef PVDD_Init();

HAL_StatusTypeDef PVDD_SetRequestedVoltage(float voltage);

void PVDD_LoopUpdate();


#endif /* INC_PVDD_CONTROL_H_ */
