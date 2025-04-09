/*
 * sample_rate_conv.h
 *
 *  Created on: Apr 1, 2025
 *      Author: Alex
 *
 *  Handles audio sample rate conversion (fixed and adaptive)
 */

#ifndef INC_SAMPLE_RATE_CONV_H_
#define INC_SAMPLE_RATE_CONV_H_

#include "main.h"
#include "arm_math.h"


//maximum number of audio channels for sample rate conversion
#define SRC_MAX_CHANNELS 2

//output samples per batch per channel
#define SRC_BATCH_CHANNEL_SAMPLES 96
//minimum and maximum number of input samples per output batch
#define SRC_BATCH_INPUT_SAMPLES_MIN (SRC_BATCH_CHANNEL_SAMPLES - 2)
#define SRC_BATCH_INPUT_SAMPLES_MAX (SRC_BATCH_CHANNEL_SAMPLES + 2)
//critical fill level of adaptive resampling buffer, in samples - set to be definitely enough to produce one output batch
#define SRC_BUF_CRITICAL_CHANNEL_SAMPLES (SRC_BATCH_INPUT_SAMPLES_MAX + 1)
//ideal fill level of adaptive resampling buffer (after read), in output batches
#define SRC_BUF_IDEAL_BATCHES 4
//ideal fill level of adaptive resampling buffer (before read), in samples per channel
#define SRC_BUF_IDEAL_CHANNEL_SAMPLES ((SRC_BUF_IDEAL_BATCHES + 1) * SRC_BATCH_CHANNEL_SAMPLES)
//adative resampling buffer size, in samples per channel
#define SRC_BUF_TOTAL_CHANNEL_SAMPLES ((2 * SRC_BUF_IDEAL_BATCHES + 1) * SRC_BATCH_CHANNEL_SAMPLES)

//maximum input samples per channel that are supported per call
#define SRC_INPUT_CHANNEL_SAMPLES_MAX 128
//size of scratch buffers, in samples per channel - set here to be enough for maximum input samples after interpolation
#define SRC_SCRATCH_CHANNEL_SAMPLES (2 * SRC_INPUT_CHANNEL_SAMPLES_MAX)

//error averaging lengths (in batches) for adaptive resampling rate
#define SRC_ADAPTIVE_RATE_ERROR_AVG_BATCHES_INITIAL 8
#define SRC_ADAPTIVE_RATE_ERROR_AVG_BATCHES 6144
#define SRC_ADAPTIVE_BUF_ERROR_AVG_BATCHES 8192
//proportional and derivative influence coefficients of the buffer fill error on the adaptive resampling rate
#define SRC_ADAPTIVE_BUF_FILL_COEFF_P (1.0f / 4096.0f)
#define SRC_ADAPTIVE_BUF_FILL_COEFF_D (2.0f)

//bit shift of output samples - negative means shifted right
#define SRC_OUTPUT_SHIFT -4


typedef enum {
  SR_UNKNOWN = 0,
  SR_44K = 44100,
  SR_48K = 48000,
  SR_96K = 96000
} SRC_SampleRate;


//initialise the SRC's internal filter variables - only needs to be called once
HAL_StatusTypeDef SRC_Init();

//reset the SRC's internal state and prepare for the conversion of a new input signal at the given sample rate
HAL_StatusTypeDef SRC_Configure(SRC_SampleRate input_rate);
//get the currently configured input sample rate
SRC_SampleRate SRC_GetCurrentInputRate();

//get the average relative input rate error
float SRC_GetAverageRateError();
//get the average buffer fill error in samples
float SRC_GetAverageBufferFillError();

//process `in_channels` input channels with `in_samples` samples per channel; where inputs were previously shifted to `in_shift` (negative = shifted right)
//must be at the currently configured input sample rate; to switch sample rate, a re-init is required
//channels may be in separate buffers or interleaved, starting at `in_bufs[channel]`, each with step size `in_step`
HAL_StatusTypeDef SRC_ProcessInputSamples(const q31_t** in_bufs, uint16_t in_step, uint16_t in_channels, uint16_t in_samples, int8_t in_shift);

//produce `out_channels` output channels with `SRC_BATCH_CHANNEL_SAMPLES` samples per channel
//output buffer(s) must have enough space for a full batch of samples!
//channels may be in separate buffers or interleaved, starting at `out_bufs[channel]`, each with step size `out_step`
//will return HAL_BUSY if the SRC is not ready to produce an output batch (will not process anything then)
HAL_StatusTypeDef SRC_ProduceOutputBatch(q31_t** out_bufs, uint16_t out_step, uint16_t out_channels);


#endif /* INC_SAMPLE_RATE_CONV_H_ */
