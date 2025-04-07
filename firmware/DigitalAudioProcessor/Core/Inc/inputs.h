/*
 * inputs.h
 *
 *  Created on: Apr 7, 2025
 *      Author: Alex
 *
 *  Handles input logic and input selection.
 */

#ifndef INC_INPUTS_H_
#define INC_INPUTS_H_

#include "main.h"
#include "arm_math.h"


//maximum number of input channels
#define INPUT_MAX_CHANNELS 2
//maximum number of samples per channel in a single input batch
#define INPUT_MAX_BATCH_CHANNEL_SAMPLES 128


typedef enum {
  INPUT_NONE = 0,
  INPUT_I2S1 = 1,
  INPUT_I2S2 = 2,
  INPUT_I2S3 = 3,
  INPUT_USB = 4,
  INPUT_SPDIF = 5,
  _INPUT_COUNT
} INPUT_Source;


//currently active input source
extern INPUT_Source input_active;

//available/connected input sources
extern bool inputs_available[_INPUT_COUNT];


//initialise inputs - only needs to be called once
HAL_StatusTypeDef INPUT_Init();
//perform main loop updates
void INPUT_LoopUpdate();

//marks the given input as unavailable immediately - if it was active, switches to the next available input (if there is one)
void INPUT_Stop(INPUT_Source input);
//make the given input active (it must be available)
HAL_StatusTypeDef INPUT_Activate(INPUT_Source input);

//update the sample rate of the given input (takes sample rate information directly from the corresponding source), applies it to the SRC if the input is active
HAL_StatusTypeDef INPUT_UpdateSampleRate(INPUT_Source input);

//handle reception of the given samples on the given input - also marks the input as available, and activates it if there isn't already an active input
HAL_StatusTypeDef INPUT_ProcessSamples(INPUT_Source input, const q31_t** in_bufs, uint16_t in_step, uint16_t in_channels, uint16_t in_samples, int8_t in_shift);

#endif /* INC_INPUTS_H_ */
