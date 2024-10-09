/*
 * adc.h
 *
 *  Created on: Oct 7, 2024
 *      Author: Alex
 */

#ifndef INC_ADC_H_
#define INC_ADC_H_

#include "main.h"

//allow definition to measure four channels (SE) instead of two (BTL)
#undef P_ADC_FOUR_CHANNEL
#define P_ADC_FOUR_CHANNEL

//how many samples per ADC are processed in each (half-buffer) processing batch
//to calculate processing batch length in real-time: t_batch = N_batch * N_scan_ch * (N_cycles_sample + N_cycles_convert) / f_ADC = N_batch * 2 * (61.5 + 12.5) / f_ADC
//batch size 512 at 18MHz corresponds to (512 * 2 * (61.5 + 12.5) / 18 MHz) ~= 4.21ms
#define P_ADC_SAMPLE_BATCH_SIZE 512

//size of the ADC DMA buffer in samples per dual channel
//4x batch size because the buffer holds two batches of two channels
#define P_ADC_DMA_BUFFER_SIZE (4 * P_ADC_SAMPLE_BATCH_SIZE)

#ifdef P_ADC_FOUR_CHANNEL
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


//raw rms voltage and current, average real power, and average apparent power of all channels (A, B, C, D) - all rescaled to factor 1
//averaged using EMA: 0s1 = 0.1s time constant, 1s0 = 1.0s time constant
extern float raw_rms_voltage_0s1[4];
extern float raw_rms_voltage_1s0[4];
extern float raw_rms_current_0s1[4];
extern float raw_rms_current_1s0[4];
extern float raw_avg_real_power_0s1[4];
extern float raw_avg_real_power_1s0[4];
extern float raw_avg_apparent_power_0s1[4];
extern float raw_avg_apparent_power_1s0[4];


HAL_StatusTypeDef ADC_Init();
HAL_StatusTypeDef ADC_Calibrate();

HAL_StatusTypeDef ADC_StartMonitoring();


#endif /* INC_ADC_H_ */
