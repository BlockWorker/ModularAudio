/*
 * spdif.h
 *
 *  Created on: Apr 11, 2025
 *      Author: Alex
 *
 *  Handles SPDIF data reception.
 */

#ifndef INC_SPDIF_H_
#define INC_SPDIF_H_

#include "main.h"
#include "inputs.h"

//SPDIFRX peripheral kernel clock frequency, in Hz
#define SPDIF_KERNEL_CLK_FREQ 240000000

//sample rate smoothing exponential moving average coefficients
#define SPDIF_SAMPLE_RATE_EMA_ALPHA 0.0625f
#define SPDIF_SAMPLE_RATE_EMA_1MALPHA (1.0f - SPDIF_SAMPLE_RATE_EMA_ALPHA)

//maximum acceptable relative sample rate error (from 44.1K/48K/96K) for activation
#define SPDIF_MAX_SAMPLE_RATE_ERROR_ON 0.01f
//maximum acceptable relative sample rate error before deactivation (hysteresis)
#define SPDIF_MAX_SAMPLE_RATE_ERROR_OFF 0.015f

//number of samples per channel per SPDIF receive batch
#define SPDIF_RX_BATCH_CHANNEL_SAMPLES 96

//number of samples per SPDIF receive batch in total (over both channels)
#define SPDIF_RX_BATCH_TOTAL_SAMPLES (2 * SPDIF_RX_BATCH_CHANNEL_SAMPLES)
//size of SPDIF receive buffer in samples - 2 batches to match DMA half-transfer callbacks
#define SPDIF_RX_BUF_SAMPLES (2 * SPDIF_RX_BATCH_TOTAL_SAMPLES)

//number of control bytes per channel per audio block (192 samples per block per channel, 1 bit per sample)
#define SPDIF_RX_CTLBLOCK_CHANNEL_BYTES 24
//size of SPDIF control receive buffer in control words - 2 blocks to match DMA half-transfer callbacks
#define SPDIF_RX_CTL_BUF_WORDS (2 * SPDIF_RX_CTLBLOCK_CHANNEL_BYTES)


//last detected reception sample rate (enum value); unknown if invalid or unsupported rate or too far away from nominal rate
extern SRC_SampleRate spdif_sample_rate_enum;


//initialise SPDIF reception - only needs to be called once
HAL_StatusTypeDef SPDIF_Init();

//SPDIFRX sync done interrupt handler
void SPDIF_HandleSyncDoneIRQ();
//SPDIFRX interface error interrupt handler
void SPDIF_HandleInterfaceErrorIRQ();



#endif /* INC_SPDIF_H_ */
