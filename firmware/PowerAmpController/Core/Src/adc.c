/*
 * adc.c
 *
 *  Created on: Oct 7, 2024
 *      Author: Alex
 */

#include "adc.h"
#include <stdio.h>
#include "arm_math.h"

//raw DMA buffers - with the given DMA configuration, samples end up arranged as {A1, B1, A2, B2, ...} and {C1, D1, C2, D2, ...}, respectively
q15_t dma_current_AB[P_ADC_DMA_BUFFER_SIZE];
q15_t dma_voltage_AB[P_ADC_DMA_BUFFER_SIZE];
q15_t dma_current_CD[P_ADC_DMA_BUFFER_SIZE];
q15_t dma_voltage_CD[P_ADC_DMA_BUFFER_SIZE];

//matrix representations of the above buffers (two columns, one per channel) - separate matrix for each buffer half, as processing is separate
arm_matrix_instance_q15 mat_current_AB_first = {P_ADC_SAMPLE_BATCH_SIZE, 2, dma_current_AB};
arm_matrix_instance_q15 mat_current_AB_second = {P_ADC_SAMPLE_BATCH_SIZE, 2, dma_current_AB + (P_ADC_DMA_BUFFER_SIZE / 2)};
arm_matrix_instance_q15 mat_voltage_AB_first = {P_ADC_SAMPLE_BATCH_SIZE, 2, dma_voltage_AB};
arm_matrix_instance_q15 mat_voltage_AB_second = {P_ADC_SAMPLE_BATCH_SIZE, 2, dma_voltage_AB + (P_ADC_DMA_BUFFER_SIZE / 2)};
arm_matrix_instance_q15 mat_current_CD_first = {P_ADC_SAMPLE_BATCH_SIZE, 2, dma_current_CD};
arm_matrix_instance_q15 mat_current_CD_second = {P_ADC_SAMPLE_BATCH_SIZE, 2, dma_current_CD + (P_ADC_DMA_BUFFER_SIZE / 2)};
arm_matrix_instance_q15 mat_voltage_CD_first = {P_ADC_SAMPLE_BATCH_SIZE, 2, dma_voltage_CD};
arm_matrix_instance_q15 mat_voltage_CD_second = {P_ADC_SAMPLE_BATCH_SIZE, 2, dma_voltage_CD + (P_ADC_DMA_BUFFER_SIZE / 2)};

#ifdef P_ADC_FOUR_CHANNEL
//processing vectors (for channel extraction)
const q15_t processing_vector_AC[] = P_ADC_PROCESSING_VECTOR_AC;
const q15_t processing_vector_BD[] = P_ADC_PROCESSING_VECTOR_BD;
#else
//processing vectors (for channel subtraction/extraction)
const q15_t processing_vector_current[] = P_ADC_CURRENT_PROCESSING_VECTOR;
const q15_t processing_vector_voltage[] = P_ADC_VOLTAGE_PROCESSING_VECTOR;
#endif

//raw rms voltage and current, average real power, and average apparent power of all channels (A, B, C, D) - all rescaled to factor 1
//last batch values ("instantaneous")
float raw_rms_voltage_inst[4] = { 0 };
float raw_rms_voltage_inst[4] = { 0 };
float raw_rms_current_inst[4] = { 0 };
float raw_rms_current_inst[4] = { 0 };
//averaged using EMA: 0s1 = 0.1s time constant, 1s0 = 1.0s time constant, sos = sum of squares (for calculating rms)
float raw_sos_voltage_0s1[4] = { 0 };
float raw_sos_voltage_1s0[4] = { 0 };
float raw_sos_current_0s1[4] = { 0 };
float raw_sos_current_1s0[4] = { 0 };
float raw_rms_voltage_0s1[4] = { 0 };
float raw_rms_voltage_1s0[4] = { 0 };
float raw_rms_current_0s1[4] = { 0 };
float raw_rms_current_1s0[4] = { 0 };
float raw_avg_real_power_0s1[4] = { 0 };
float raw_avg_real_power_1s0[4] = { 0 };
float raw_avg_apparent_power_0s1[4] = { 0 };
float raw_avg_apparent_power_1s0[4] = { 0 };


HAL_StatusTypeDef ADC_Init() {
  return HAL_OK;
}

HAL_StatusTypeDef ADC_Calibrate() {
  ReturnOnError(HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED));
  ReturnOnError(HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED));
  ReturnOnError(HAL_ADCEx_Calibration_Start(&hadc3, ADC_SINGLE_ENDED));
  return HAL_ADCEx_Calibration_Start(&hadc4, ADC_SINGLE_ENDED);
}

HAL_StatusTypeDef ADC_StartMonitoring() {
  //enable dual mode: ADC1, ADC2 (channel A+B current and voltage)
  ReturnOnError(HAL_ADC_Start(&hadc2)); //start slave (ADC2)
  SET_BIT(hadc2.Instance->CFGR, ADC_CFGR_DMAEN); //enable DMA transfer for slave (ADC2)
  ReturnOnError(HAL_DMA_Start(hadc2.DMA_Handle, (uint32_t)&hadc2.Instance->DR, (uint32_t)dma_voltage_AB, P_ADC_DMA_BUFFER_SIZE)); //start slave DMA (ADC2)
  SET_BIT(hadc1.Instance->CFGR, ADC_CFGR_DMAEN); //enable DMA transfer for master (ADC1)
  ReturnOnError(HAL_ADCEx_MultiModeStart_DMA(&hadc1, (uint32_t*)dma_current_AB, P_ADC_DMA_BUFFER_SIZE)); //start master DMA (ADC1) and simultaneous conversions

  //enable dual mode: ADC3, ADC4 (channel C+D current and voltage)
  ReturnOnError(HAL_ADC_Start(&hadc4)); //start slave (ADC4)
  SET_BIT(hadc4.Instance->CFGR, ADC_CFGR_DMAEN); //enable DMA transfer for slave (ADC4)
  ReturnOnError(HAL_DMA_Start(hadc4.DMA_Handle, (uint32_t)&hadc4.Instance->DR, (uint32_t)dma_voltage_CD, P_ADC_DMA_BUFFER_SIZE)); //start slave DMA (ADC4)
  SET_BIT(hadc3.Instance->CFGR, ADC_CFGR_DMAEN); //enable DMA transfer for master (ADC3)
  return HAL_ADCEx_MultiModeStart_DMA(&hadc3, (uint32_t*)dma_current_CD, P_ADC_DMA_BUFFER_SIZE); //start master DMA (ADC3) and simultaneous conversions
}

/**
 * data processing function (DMA to raw data)
 * index: 0 = channels A+B, 1 = channels C+D
 * half: 0 = first half of buffer, 1 = second half of buffer
 */
void _ADC_ProcessCallback(uint8_t index, uint8_t half) {
  arm_matrix_instance_q15* current_matrix;
  arm_matrix_instance_q15* voltage_matrix;

  //intermediate buffers for raw values
  q15_t raw_current[P_ADC_SAMPLE_BATCH_SIZE];
  q15_t raw_voltage[P_ADC_SAMPLE_BATCH_SIZE];
  q15_t raw_power[P_ADC_SAMPLE_BATCH_SIZE];
#ifdef P_ADC_FOUR_CHANNEL
  q15_t raw_current_second[P_ADC_SAMPLE_BATCH_SIZE];
  q15_t raw_voltage_second[P_ADC_SAMPLE_BATCH_SIZE];
  q15_t raw_power_second[P_ADC_SAMPLE_BATCH_SIZE];
#endif

  int i;

  //select matrices based on index and half
  if (index == 0) {
    if (half == 0) {
      current_matrix = &mat_current_AB_first;
      voltage_matrix = &mat_voltage_AB_first;
    } else {
      current_matrix = &mat_current_AB_second;
      voltage_matrix = &mat_voltage_AB_second;
    }
  } else {
    if (half == 0) {
      current_matrix = &mat_current_CD_first;
      voltage_matrix = &mat_voltage_CD_first;
    } else {
      current_matrix = &mat_current_CD_second;
      voltage_matrix = &mat_voltage_CD_second;
    }
  }

#ifdef DEBUG
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

  uint32_t start_cycle = DWT->CYCCNT;
#endif

#ifdef P_ADC_FOUR_CHANNEL
  //extract currents from matrices
  arm_mat_vec_mult_q15(current_matrix, processing_vector_AC, raw_current);
  arm_mat_vec_mult_q15(current_matrix, processing_vector_BD, raw_current_second);
  //extract voltages from matrices
  arm_mat_vec_mult_q15(voltage_matrix, processing_vector_AC, raw_voltage);
  arm_mat_vec_mult_q15(voltage_matrix, processing_vector_BD, raw_voltage_second);
  //calculate instantaneous power
  arm_mult_q15(raw_voltage, raw_current, raw_power, P_ADC_SAMPLE_BATCH_SIZE);
  arm_mult_q15(raw_voltage_2, raw_current_2, raw_power_2, P_ADC_SAMPLE_BATCH_SIZE);
  //calculate voltage and current sums of squares and average power
  q63_t raw_sos[4]; //sums of squares: I1, V1, I2, V2; in 34.30 format (see CMSIS-DSP documentation)
  q15_t raw_avg_powers[2]; //average real powers: 1, 2
  arm_power_q15(raw_current, P_ADC_SAMPLE_BATCH_SIZE, raw_sos);
  arm_power_q15(raw_voltage, P_ADC_SAMPLE_BATCH_SIZE, raw_sos + 1);
  arm_power_q15(raw_current_second, P_ADC_SAMPLE_BATCH_SIZE, raw_sos + 2);
  arm_power_q15(raw_voltage_second, P_ADC_SAMPLE_BATCH_SIZE, raw_sos + 3);
  arm_mean_q15(raw_power, P_ADC_SAMPLE_BATCH_SIZE, raw_avg_powers);
  arm_mean_q15(raw_power_second, P_ADC_SAMPLE_BATCH_SIZE, raw_avg_powers + 1);
  //convert to floats, sums of squares also get averaged at the same time
  float f_results[6]; //I1, V1, I2, V2, P1, P2
  for (i = 0; i < 4; i++) {
      f_results[i] = (float)raw_sos[i] / P_ADC_SOS_AVG_DIVISOR;
  }
  arm_q15_to_float(raw_avg_powers, f_results + 4, 2);
  //perform batch result calculations
  raw_rms_current_inst[2 * index] = sqrtf(f_results[0]) * 2.0f;
  raw_rms_voltage_inst[2 * index] = sqrtf(f_results[1]) * 2.0f;
  raw_avg_apparent_power_inst[2 * index] = sqrtf(f_results[0] * f_results[1]) * 4.0f;
  raw_rms_current[2 * index + 1] = sqrtf(f_results[2]) * 2.0f;
  raw_rms_voltage[2 * index + 1] = sqrtf(f_results[3]) * 2.0f;
  raw_avg_apparent_power[2 * index + 1] = sqrtf(f_results[2] * f_results[3]) * 4.0f;
  raw_avg_real_power[2 * index] = fabsf(f_results[4]) * 4.0f;
  raw_avg_real_power[2 * index + 1] = fabsf(f_results[5]) * 4.0f;
  //update EMA results
  raw_sos_current_0s1[2 * index] = raw_sos_current_0s1[2 * index] * P_ADC_EMA_0S1_1MALPHA + f_results[0] * P_ADC_EMA_0S1_ALPHA;
  raw_sos_current_1s0[2 * index] = raw_sos_current_1s0[2 * index] * P_ADC_EMA_1S0_1MALPHA + f_results[0] * P_ADC_EMA_1S0_ALPHA;
  raw_sos_voltage_0s1[2 * index] = raw_sos_voltage_0s1[2 * index] * P_ADC_EMA_0S1_1MALPHA + f_results[1] * P_ADC_EMA_0S1_ALPHA;
  raw_sos_voltage_1s0[2 * index] = raw_sos_voltage_1s0[2 * index] * P_ADC_EMA_1S0_1MALPHA + f_results[1] * P_ADC_EMA_1S0_ALPHA;
#else
  //extract current and differential voltage based on index
  arm_mat_vec_mult_q15(current_matrix, processing_vector_current, raw_current);
  arm_mat_vec_mult_q15(voltage_matrix, processing_vector_voltage, raw_voltage);
  //calculate instantaneous power
  arm_mult_q15(raw_voltage, raw_current, raw_power, P_ADC_SAMPLE_BATCH_SIZE);
  //calculate rms voltage and current and average power
  q15_t raw_results[3]; //Irms, Vrms, Pavg
  arm_rms_q15(raw_current, P_ADC_SAMPLE_BATCH_SIZE, raw_results);
  arm_rms_q15(raw_voltage, P_ADC_SAMPLE_BATCH_SIZE, raw_results + 1);
  arm_mean_q15(raw_power, P_ADC_SAMPLE_BATCH_SIZE, raw_results + 2);
  //convert to floats and save
  float f_results[3];
  arm_q15_to_float(raw_results, f_results, 3);
  raw_rms_current[2 * index] = raw_rms_current[2 * index + 1] = f_results[0] * 2.0f;
  raw_rms_voltage[2 * index] = raw_rms_voltage[2 * index + 1] = f_results[1] * 2.0f;
  raw_avg_real_power[2 * index] = raw_avg_real_power[2 * index + 1] = fabsf(f_results[2]) * 4.0f;
  raw_avg_apparent_power[2 * index] = raw_avg_apparent_power[2 * index + 1] = f_results[0] * f_results[1] * 4.0f;
#endif

#ifdef DEBUG
  uint32_t cycles = DWT->CYCCNT - start_cycle;

  DWT->CTRL &= ~DWT_CTRL_CYCCNTENA_Msk;

  //printf("Processed index %u half %u, processing took %lu cycles\n", index, half, cycles);
#endif
}

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef* hadc) {
  if (hadc == &hadc1) {
    _ADC_ProcessCallback(0, 1);
  } else if (hadc == &hadc3) {
    _ADC_ProcessCallback(1, 1);
  }
}

void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef* hadc) {
  if (hadc == &hadc1) {
    _ADC_ProcessCallback(0, 0);
  } else if (hadc == &hadc3) {
    _ADC_ProcessCallback(1, 0);
  }
}


