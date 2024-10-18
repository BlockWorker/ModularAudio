/*
 * adc.h
 *
 *  Created on: Oct 7, 2024
 *      Author: Alex
 */

#ifndef INC_ADC_H_
#define INC_ADC_H_

#include "main.h"

//how many samples per ADC are processed in each (half-buffer) processing batch
//to calculate processing batch length in real-time: t_batch = N_batch * N_scan_ch * (N_cycles_sample + N_cycles_convert) / f_ADC = N_batch * 2 * (61.5 + 12.5) / f_ADC
//batch size 512 at 18MHz corresponds to (512 * 2 * (61.5 + 12.5) / 18 MHz) ~= 4.21ms
#define P_ADC_SAMPLE_BATCH_SIZE 512

//size of the ADC DMA buffer in samples per dual channel
//4x batch size because the buffer holds two batches of two channels
#define P_ADC_DMA_BUFFER_SIZE (4 * P_ADC_SAMPLE_BATCH_SIZE)

#ifdef MAIN_FOUR_CHANNEL
//select first channel of each pair - scaled by 0.5 due to Q15 range limits
#define P_ADC_PROCESSING_VECTOR_AC { 0x4000, 0x0000 }
//select second channel of each pair - scaled by 0.5 due to Q15 range limits
#define P_ADC_PROCESSING_VECTOR_BD { 0x0000, 0x4000 }
#else
//process voltage by taking the channel difference - scaled by 0.5 due to Q15 range limits
#define P_ADC_VOLTAGE_PROCESSING_VECTOR { 0x4000, 0xC000 }
//process current by selecting the first channel (second channel is unpopulated in 2-channel configuration) - scaled by 0.5 due to Q15 range limits
#define P_ADC_CURRENT_PROCESSING_VECTOR { 0x4000, 0x0000 }
#endif

//conversion factor between float format and (integer form of) 34.30 fixed-point format
#define P_ADC_3430_CONV_FACTOR 1073741824.0f
//divisor for fixed-point to floating-point sum-of-squares averaging (given sum-of-squares in 34.30 format)
#define P_ADC_SOS_AVG_DIVISOR (P_ADC_3430_CONV_FACTOR * P_ADC_SAMPLE_BATCH_SIZE)

//EMA alpha and 1-alpha values for 0.1s and 1.0s time constants
//calculation: 1-alpha = e^(-t_batch / t_constant)
//given values here calculated for t_batch = 4.21ms (18 MHz f_ADC, 512 batch size)
#define P_ADC_EMA_0S1_1MALPHA 0.958776f
#define P_ADC_EMA_0S1_ALPHA (1.0f - P_ADC_EMA_0S1_1MALPHA)
#define P_ADC_EMA_1S0_1MALPHA 0.995799f
#define P_ADC_EMA_1S0_ALPHA (1.0f - P_ADC_EMA_1S0_1MALPHA)

//system constants that determine unit conversion factors - adjust these for calibration
//VREF = VDDA voltage - theoretical value ~3.555 V
#define P_ADC_VREF 3.555f
//channel current gains, BTL mode uses A and C only - theoretical value 0.1 V/A
#define P_ADC_IGAIN_A 0.1f
#define P_ADC_IGAIN_B 0.1f
#define P_ADC_IGAIN_C 0.1f
#define P_ADC_IGAIN_D 0.1f
//channel voltage gains, BTL mode uses A and C only - theoretical value 11/211 ~= 0.0521327 V/V
#define P_ADC_VGAIN_A 0.0521327f
#define P_ADC_VGAIN_B 0.0521327f
#define P_ADC_VGAIN_C 0.0521327f
#define P_ADC_VGAIN_D 0.0521327f

//unit conversion factors
//current: fractional scaled ADC value [0, 0.5] -> measured current [A]
#define P_ADC_FACTOR_IA (2.0f * P_ADC_VREF / P_ADC_IGAIN_A)
#define P_ADC_FACTOR_IB (2.0f * P_ADC_VREF / P_ADC_IGAIN_B)
#define P_ADC_FACTOR_IC (2.0f * P_ADC_VREF / P_ADC_IGAIN_C)
#define P_ADC_FACTOR_ID (2.0f * P_ADC_VREF / P_ADC_IGAIN_D)
//voltage: fractional scaled ADC value [0, 0.5] -> measured voltage [V]
#define P_ADC_FACTOR_VA (2.0f * P_ADC_VREF / P_ADC_VGAIN_A)
#define P_ADC_FACTOR_VB (2.0f * P_ADC_VREF / P_ADC_VGAIN_B)
#define P_ADC_FACTOR_VC (2.0f * P_ADC_VREF / P_ADC_VGAIN_C)
#define P_ADC_FACTOR_VD (2.0f * P_ADC_VREF / P_ADC_VGAIN_D)
//power: fractional scaled ADC value [0, 0.25] -> measured power [W]
#define P_ADC_FACTOR_PA (P_ADC_FACTOR_IA * P_ADC_FACTOR_VA)
#define P_ADC_FACTOR_PB (P_ADC_FACTOR_IB * P_ADC_FACTOR_VB)
#define P_ADC_FACTOR_PC (P_ADC_FACTOR_IC * P_ADC_FACTOR_VC)
#define P_ADC_FACTOR_PD (P_ADC_FACTOR_ID * P_ADC_FACTOR_VD)


//READ ONLY
//rms voltage and current, average real power, and average apparent power of all channels (A, B, C, D) - in V, A, and W, respectively
//last batch values ("instantaneous")
extern float rms_voltage_inst[4];
extern float rms_current_inst[4];
extern float avg_real_power_inst[4];
extern float avg_apparent_power_inst[4];
//averaged using EMA: 0s1 = 0.1s time constant, 1s0 = 1.0s time constant
extern float rms_voltage_0s1[4];
extern float rms_voltage_1s0[4];
extern float rms_current_0s1[4];
extern float rms_current_1s0[4];
extern float avg_real_power_0s1[4];
extern float avg_real_power_1s0[4];
extern float avg_apparent_power_0s1[4];
extern float avg_apparent_power_1s0[4];


HAL_StatusTypeDef ADC_Init();
HAL_StatusTypeDef ADC_Calibrate();

HAL_StatusTypeDef ADC_StartMonitoring();


#endif /* INC_ADC_H_ */
