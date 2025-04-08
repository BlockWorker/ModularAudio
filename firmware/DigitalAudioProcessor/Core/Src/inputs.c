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

//parameters of the sample batch currently being transferred over MDMA
static q31_t    __DTCM_BSS  _input_dma_sample_buf     [INPUT_MAX_CHANNELS * INPUT_MAX_BATCH_CHANNEL_SAMPLES];
static uint16_t             _input_dma_step           = 0;
static uint16_t             _input_dma_channels       = 0;
static uint16_t             _input_dma_samples        = 0;
static uint16_t             _input_dma_buf_sample_cap = 0;
static int8_t               _input_dma_shift          = 0;


//handle completion of MDMA transfer of active input samples
static void _INPUT_MDMA_CompleteCallback(MDMA_HandleTypeDef* mdma) {
  int i;

  if (mdma == &hmdma_mdma_channel40_sw_0) {
    //derive buffer pointers depending on the given step size
    const q31_t* buf_pointers[INPUT_MAX_CHANNELS];
    if (_input_dma_step == 1) {
      //step size 1: contiguous channel buffers
      for (i = 0; i < _input_dma_channels; i++) {
        buf_pointers[i] = _input_dma_sample_buf + i * _input_dma_buf_sample_cap;
      }
    } else {
      //step size >1: interleaved channels in a single buffer
      for (i = 0; i < _input_dma_channels; i++) {
        buf_pointers[i] = _input_dma_sample_buf + i;
      }
    }

    //pass the transferred sample batch to the SRC
    SRC_ProcessInputSamples(buf_pointers, _input_dma_step, _input_dma_channels, _input_dma_samples, _input_dma_shift);
  }
}


//initialise inputs - only needs to be called once
HAL_StatusTypeDef INPUT_Init() {
  int i;

  //initialise to no active input and all inputs unavailable
  input_active = INPUT_NONE;
  for (i = 0; i < _INPUT_COUNT; i++) {
    inputs_available[i] = false;
    _inputs_silent[i] = false;
  }

  //abort any potentially ongoing MDMA transfer and register the MDMA transfer complete callback for sample handling
  HAL_MDMA_Abort(&hmdma_mdma_channel40_sw_0);
  ReturnOnError(HAL_MDMA_RegisterCallback(&hmdma_mdma_channel40_sw_0, HAL_MDMA_XFER_CPLT_CB_ID, &_INPUT_MDMA_CompleteCallback));

  return HAL_OK;
}

//perform main loop updates
void INPUT_LoopUpdate() {
  int i;

  //check all inputs for silence
  for (i = 1; i < _INPUT_COUNT; i++) {
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
    for (i = 1; i < _INPUT_COUNT; i++) {
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

  DEBUG_PRINTF("Input %u has been activated\n", input);

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
//buffer may contain consecutive contiguous channels (in_step = 1) or be interleaved (in_step >= in_channels > 1), with given pre-shift (negative = data is shifted right)
HAL_StatusTypeDef INPUT_ProcessSamples(INPUT_Source input, const q31_t* in_buf, uint16_t in_step, uint16_t in_channels, uint16_t in_samples, uint16_t in_buf_sample_cap, int8_t in_shift) {
  //check parameters for validity
  if (input <= INPUT_NONE || input >= _INPUT_COUNT || in_buf == NULL || in_channels < 1 || in_channels > INPUT_MAX_CHANNELS ||
      (in_step < in_channels && in_step != 1) || in_step > INPUT_MAX_CHANNELS || in_samples < 1 || in_samples > INPUT_MAX_BATCH_CHANNEL_SAMPLES ||
      in_buf_sample_cap < 1 || in_buf_sample_cap > INPUT_MAX_BATCH_CHANNEL_SAMPLES || in_samples > in_buf_sample_cap) {
    DEBUG_PRINTF("* Input attempted to process samples with invalid parameters %u %p %u %u %u %u %d\n", input, in_buf, in_step, in_channels, in_samples, in_buf_sample_cap, in_shift);
    return HAL_ERROR;
  }

  //mark this input as available and not silent
  if (!inputs_available[input]) {
    inputs_available[input] = true;
    DEBUG_PRINTF("Input %u has become available\n", input);
  }
  _inputs_silent[input] = false;

  //if there's no valid active input, activate this input
  if (input_active <= INPUT_NONE || input_active >= _INPUT_COUNT) {
    ReturnOnError(INPUT_Activate(input));
  }

  //do actual sample processing only if this input is active
  if (input == input_active) {
    _input_dma_step = in_step;
    _input_dma_channels = in_channels;
    _input_dma_samples = in_samples;
    _input_dma_buf_sample_cap = in_buf_sample_cap;
    _input_dma_shift = in_shift;

    //calculate size of transfer in bytes and start MDMA transfer
    uint32_t transfer_bytes = (uint32_t)in_buf_sample_cap * (uint32_t)(MAX(in_step, in_channels)) * sizeof(q31_t);
    HAL_MDMA_Start_IT(&hmdma_mdma_channel40_sw_0, (uint32_t)in_buf, (uint32_t)_input_dma_sample_buf, transfer_bytes, 1);
  }

  return HAL_OK;
}
