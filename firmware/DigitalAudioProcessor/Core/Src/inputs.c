/*
 * inputs.c
 *
 *  Created on: Apr 7, 2025
 *      Author: Alex
 *
 *  Handles input logic and input selection.
 */

#include "inputs.h"
#include "sample_rate_conv.h"
#include "signal_processing.h"
#include "usbd_audio.h"


//currently active input source
INPUT_Source input_active = INPUT_NONE;

//available/connected input sources
bool inputs_available[_INPUT_COUNT];
//sources that are available but have been silent for at least one main loop cycle
static bool _inputs_silent[_INPUT_COUNT];


//initialise inputs - only needs to be called once
HAL_StatusTypeDef INPUT_Init() {
  int i;

  //initialise to no active input and all inputs unavailable
  input_active = INPUT_NONE;
  for (i = 0; i < _INPUT_COUNT; i++) {
    inputs_available[i] = false;
    _inputs_silent[i] = false;
  }

  return HAL_OK;
}

//perform main loop updates
void INPUT_LoopUpdate() {
  int i;

  //check all inputs for silence
  for (i = 0; i < _INPUT_COUNT; i++) {
    //skip unavailable inputs
    if (!inputs_available[i]) {
      continue;
    }

    if (_inputs_silent[i]) {
      //input has been silent for a full cycle: mark it as unavailable
      DEBUG_PRINTF("Input %d is being disabled due to silence\n", i);
      INPUT_Stop((INPUT_Source)i);
    } else {
      //input hasn't been silent: mark it as silent to check the next cycle for silence
      _inputs_silent[i] = true;
    }
  }
}

//marks the given input as unavailable immediately - if it was active, switches to the next available input (if there is one)
void INPUT_Stop(INPUT_Source input) {
  int i;

  if (input <= INPUT_NONE || input >= _INPUT_COUNT) {
    //invalid input
    DEBUG_PRINTF("* Attempted to stop invalid input %u\n", input);
    return;
  }

  //make unavailable and not silent
  inputs_available[input] = false;
  _inputs_silent[input] = false;

  DEBUG_PRINTF("Input %u has been stopped\n", input);

  //handle input switching if the input was active
  if (input_active == input) {
    //find another available input to switch to
    for (i = 0; i < _INPUT_COUNT; i++) {
      if (inputs_available[i]) {
        //found available input: switch to it and finish processing if successful
        if (INPUT_Activate((INPUT_Source)i) == HAL_OK) {
          return;
        }
      }
    }
    //no available input found: default to none
    input_active = INPUT_NONE;
  }
}

//make the given input active (it must be available)
HAL_StatusTypeDef INPUT_Activate(INPUT_Source input) {
  if (input <= INPUT_NONE || input >= _INPUT_COUNT || !inputs_available[input]) {
    //invalid or unavailable input
    DEBUG_PRINTF("* Attempted to activate invalid or unavailable input %u\n", input);
    return HAL_ERROR;
  }

  //mark input as active
  input_active = input;

  //update input's sample rate (including setting up the SRC for it)
  INPUT_UpdateSampleRate(input_active);

  return HAL_OK;
}

//update the sample rate of the given input (takes sample rate information directly from the corresponding source), applies it to the SRC if the input is active
HAL_StatusTypeDef INPUT_UpdateSampleRate(INPUT_Source input) {
  extern USBD_HandleTypeDef hUsbDeviceHS;
  USBD_AUDIO_HandleTypeDef* haudio;

  //check if the input is active - nothing to do for inactive inputs right now
  if (input != input_active) {
    return HAL_OK;
  }

  //get sample rate from config/info depending on the source
  SRC_SampleRate rate;
  switch (input) {
    case INPUT_I2S1:
      rate = hi2s1.Init.AudioFreq;
      break;
    case INPUT_I2S2:
      rate = hi2s2.Init.AudioFreq;
      break;
    case INPUT_I2S3:
      rate = hi2s3.Init.AudioFreq;
      break;
    case INPUT_USB:
      haudio = (USBD_AUDIO_HandleTypeDef*)hUsbDeviceHS.pClassDataCmsit[hUsbDeviceHS.classId];
      rate = haudio->freq;
      break;
    case INPUT_SPDIF:
      //TODO implement
      rate = SR_UNKNOWN;
      break;
    default:
      //invalid input
      DEBUG_PRINTF("* Attempted to update sample rate of invalid input %u\n", input);
      return HAL_ERROR;
  }

  if (rate != SR_44K && rate != SR_48K && rate != SR_96K) {
    //invalid sample rate
    DEBUG_PRINTF("* Input %u reported invalid sample rate %u\n", input, rate);
    return HAL_ERROR;
  }

  //check if the reported sample rate is different from the currently configured one
  if (rate != SRC_GetCurrentInputRate()) {
    //different rate: configure SRC for the new rate
    ReturnOnError(SRC_Configure(rate));
    //reset signal processor due to the SRC data stream reset
    SP_Reset();
  }

  return HAL_OK;
}

//handle reception of the given samples on the given input - also marks the input as available, and activates it if there isn't already an active input
HAL_StatusTypeDef INPUT_ProcessSamples(INPUT_Source input, const q31_t** in_bufs, uint16_t in_step, uint16_t in_channels, uint16_t in_samples, int8_t in_shift) {


  return HAL_OK;
}
