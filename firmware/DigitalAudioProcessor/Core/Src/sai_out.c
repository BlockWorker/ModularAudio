/*
 * sai_out.c
 *
 *  Created on: Mar 31, 2025
 *      Author: Alex
 *
 *  Handles the buffer and HAL state of the SAI I2C output
 */

#include "sai_out.h"
#include "sample_rate_conv.h"


//output data DMA buffer
static q31_t __D3_BSS _sai_out_buffer[SAI_OUT_BUF_SAMPLES];


//calculates a batch of output samples into the buffer at the given offset; writes zeros if there are insufficient input samples to process
static void _SAI_OUT_CalculateBatch(uint32_t buffer_offset) {
  int i;

  //prepare buffer pointers for the sample batch
  q31_t* buf_pointers[SAI_OUT_CHANNELS];
  for (i = 0; i < SAI_OUT_CHANNELS; i++) {
    buf_pointers[i] = _sai_out_buffer + buffer_offset + i;
  }

  //try to process SRC output batch
  if (SRC_ProduceOutputBatch(buf_pointers, SAI_OUT_CHANNELS, SAI_OUT_CHANNELS) == HAL_OK) {
    //output produced successfully: unshift results to be at full scale again
    arm_shift_q31(_sai_out_buffer + buffer_offset, -SRC_OUTPUT_SHIFT, _sai_out_buffer + buffer_offset, SAI_OUT_TOTAL_BATCH_SAMPLES);
  } else {
    //failed to produce output: zero-fill batch
    memset(_sai_out_buffer + buffer_offset, 0, SAI_OUT_TOTAL_BATCH_SAMPLES * sizeof(q31_t));
  }
}


void HAL_SAI_TxCpltCallback(SAI_HandleTypeDef *hsai) {
  //transfer reached end point (second batch in buffer sent): calculate new second batch into buffer while first batch is transmitting
  _SAI_OUT_CalculateBatch(SAI_OUT_TOTAL_BATCH_SAMPLES);
}

void HAL_SAI_TxHalfCpltCallback(SAI_HandleTypeDef *hsai) {
  //transfer reached halfway point (first batch in buffer sent): calculate new first batch into buffer while second batch is transmitting
  _SAI_OUT_CalculateBatch(0);
}

void HAL_SAI_ErrorCallback(SAI_HandleTypeDef *hsai) {
  //error: abort and re-init
  DEBUG_PRINTF("*** SAI Error: %lu", HAL_SAI_GetError(hsai));
  SAI_OUT_Init();
}


HAL_StatusTypeDef SAI_OUT_Init() {
  //abort any potential ongoing transfer
  HAL_SAI_Abort(&hsai_BlockB4);

  //fill the buffer with initial data - using zeros if insufficient input data
  _SAI_OUT_CalculateBatch(0);
  _SAI_OUT_CalculateBatch(SAI_OUT_TOTAL_BATCH_SAMPLES);

  //start the circular DMA transfer
  return HAL_SAI_Transmit_DMA(&hsai_BlockB4, (uint8_t*)_sai_out_buffer, SAI_OUT_BUF_SAMPLES);
}
