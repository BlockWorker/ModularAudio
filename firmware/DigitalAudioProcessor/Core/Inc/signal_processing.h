/*
 * signal_processing.h
 *
 *  Created on: Apr 5, 2025
 *      Author: Alex
 *
 *  Handles audio signal processing (after sample rate conversion), including mixing, effects, and filters.
 */

#ifndef INC_SIGNAL_PROCESSING_H_
#define INC_SIGNAL_PROCESSING_H_

#include "main.h"
#include "arm_math.h"


//maximum channels of the signal processing pipeline (i.e. maximum number of mixer output channels)
#define SP_MAX_CHANNELS 2

//output samples per batch per channel
#define SP_BATCH_CHANNEL_SAMPLES 96


//initialise the internal signal processing variables - only needs to be called once
HAL_StatusTypeDef SP_Init();
//resets the internal state of the signal processor
void SP_Reset();

//produce `out_channels` output channels with `SP_BATCH_CHANNEL_SAMPLES` samples per channel
//output buffer(s) must have enough space for a full batch of samples!
//channels may be in separate buffers or interleaved, starting at `out_bufs[channel]`, each with step size `out_step`
//will return HAL_BUSY if the preceding SRC is not ready to produce an output batch (will not process anything then)
HAL_StatusTypeDef SP_ProduceOutputBatch(q31_t** out_bufs, uint16_t out_step, uint16_t out_channels);


#endif /* INC_SIGNAL_PROCESSING_H_ */
