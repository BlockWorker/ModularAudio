/*
 * sai_out.h
 *
 *  Created on: Mar 31, 2025
 *      Author: Alex
 *
 *  Handles the buffer and HAL state of the SAI I2C output
 */

#ifndef INC_SAI_OUT_H_
#define INC_SAI_OUT_H_

#include "main.h"

//samples per batch per channel
#define SAI_OUT_CHANNEL_BATCH_SAMPLES 96
//output audio channels
#define SAI_OUT_CHANNELS 2

//number of samples per batch in total (over all channels)
#define SAI_OUT_TOTAL_BATCH_SAMPLES (SAI_OUT_CHANNEL_BATCH_SAMPLES * SAI_OUT_CHANNELS)
//total DMA buffer size in samples - 2 batches to match DMA half-transfer callbacks
#define SAI_OUT_BUF_SAMPLES (SAI_OUT_TOTAL_BATCH_SAMPLES * 2)


HAL_StatusTypeDef SAI_OUT_Init();

#endif /* INC_SAI_OUT_H_ */
