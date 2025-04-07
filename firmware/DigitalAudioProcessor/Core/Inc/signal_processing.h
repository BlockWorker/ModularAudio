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
//maximum FIR filter length per channel
#define SP_MAX_FIR_LENGTH 300
//minimum volume gain in dB
#define SP_MIN_VOL_GAIN -120.0f
//maximum volume gain in dB
#define SP_MAX_VOL_GAIN 0.0f
//minimum loudness compensation gain in dB (anything less than this means loudness compensation is disabled)
#define SP_MIN_LOUDNESS_ENABLED_GAIN -30.0f
//maximum loudness compensation gain in dB
#define SP_MAX_LOUDNESS_GAIN 0.0f

//bit shift of output samples - negative means shifted right
#define SP_OUTPUT_SHIFT 0


//mixer gain matrix - rows = output channels (for further processing), columns = input/SRC channels, effective gains are matrix values * 2 (to allow for bigger range)
extern q31_t sp_mixer_gains[SP_MAX_CHANNELS][SRC_MAX_CHANNELS];

//biquad filter coefficients - consecutive stages, each made of b0 b1 b2 a1 a2, negate a1 and a2 (compared to MATLAB coefficients)
extern q31_t sp_biquad_coeffs[SP_MAX_CHANNELS][5 * SP_MAX_BIQUADS];

//FIR filter coefficients - single stage (if need multiple, combine into one stage first), reversed coefficient order (index length-1 is coefficient 0)
extern q31_t sp_fir_coeffs[SP_MAX_CHANNELS][SP_MAX_FIR_LENGTH];

//volume gains in dB - range between `SP_MIN_VOL_GAIN` and `SP_MAX_VOL_GAIN`
extern float sp_volume_gains_dB[SP_MAX_CHANNELS];

//loudness compensation gains in dB - max `SP_MAX_LOUDNESS_GAIN`, less than `SP_MIN_LOUDNESS_ENABLED_GAIN` means loudness compensation is disabled
extern float sp_loudness_gains_dB[SP_MAX_CHANNELS];


//initialise the internal signal processing variables - only needs to be called once
HAL_StatusTypeDef SP_Init();
//resets the internal state of the signal processor
void SP_Reset();

//setup the biquad filter parameters: number of filters and post-shift for each channel
//post-shift n effectively scales the biquad coefficients for that channel by 2^n (for all cascaded filters in that channel!)
//should always be followed by a reset for correct filter behaviour
HAL_StatusTypeDef SP_SetupBiquads(uint8_t* filter_counts, uint8_t* post_shifts);
//setup the FIR filter lengths for each channel
//should always be followed by a reset for correct filter behaviour
HAL_StatusTypeDef SP_SetupFIRs(uint16_t* filter_lengths);

//produce `out_channels` output channels with `SP_BATCH_CHANNEL_SAMPLES` samples per channel
//output buffer(s) must have enough space for a full batch of samples!
//channels may be in separate buffers or interleaved, starting at `out_bufs[channel]`, each with step size `out_step`
//will return HAL_BUSY if the preceding SRC is not ready to produce an output batch (will not process anything then)
HAL_StatusTypeDef SP_ProduceOutputBatch(q31_t** out_bufs, uint16_t out_step, uint16_t out_channels);


#endif /* INC_SIGNAL_PROCESSING_H_ */
