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
#include "sample_rate_conv.h"


//maximum channels of the signal processing pipeline (i.e. maximum number of mixer output channels)
#define SP_MAX_CHANNELS 2

//output samples per batch per channel
#define SP_BATCH_CHANNEL_SAMPLES SRC_BATCH_CHANNEL_SAMPLES

//maximum number of cascaded biquad filters per channel
#define SP_MAX_BIQUADS 16


//mixer gain matrix - rows = output channels (for further processing), columns = input/SRC channels, effective gains are matrix values * 2 (to allow for bigger range)
extern q31_t sp_mixer_gains[SP_MAX_CHANNELS][SRC_MAX_CHANNELS];


//initialise the internal signal processing variables - only needs to be called once
HAL_StatusTypeDef SP_Init();
//resets the internal state of the signal processor
void SP_Reset();

//setup the biquad filter parameters: number of filters and post-shift for each channel
//post-shift n effectively scales the biquad coefficients for that channel by 2^n (for all cascaded filters in that channel!)
//should always be followed by a reset for correct filter behaviour
HAL_StatusTypeDef SP_SetupBiquads(uint8_t* filter_counts, uint8_t* post_shifts);

//produce `out_channels` output channels with `SP_BATCH_CHANNEL_SAMPLES` samples per channel
//output buffer(s) must have enough space for a full batch of samples!
//channels may be in separate buffers or interleaved, starting at `out_bufs[channel]`, each with step size `out_step`
//will return HAL_BUSY if the preceding SRC is not ready to produce an output batch (will not process anything then)
HAL_StatusTypeDef SP_ProduceOutputBatch(q31_t** out_bufs, uint16_t out_step, uint16_t out_channels);


#endif /* INC_SIGNAL_PROCESSING_H_ */
