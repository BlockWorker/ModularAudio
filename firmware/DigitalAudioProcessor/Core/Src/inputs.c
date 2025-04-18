/*
 * inputs.c
 *
 *  Created on: Apr 7, 2025
 *      Author: Alex
 *
 *  Handles input logic and input selection.
 */

#include "inputs.h"
#include "signal_processing.h"
#include "usbd_audio.h"
#include "i2c.h"
#include "spdif.h"


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

//I2S DMA receive buffers
static q31_t _i2s1_rx_buffer[INPUT_I2S_RX_BUF_SAMPLES];
static q31_t _i2s2_rx_buffer[INPUT_I2S_RX_BUF_SAMPLES];
static q31_t _i2s3_rx_buffer[INPUT_I2S_RX_BUF_SAMPLES];

//I2S input sample rates
SRC_SampleRate input_i2s_sample_rates[3];


//handle completion of MDMA transfer of active input samples
static void _INPUT_MDMA_CompleteCallback(MDMA_HandleTypeDef* mdma) {
  int i;

  if (mdma == &hmdma_mdma_channel0_sw_0) {
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

//handle reception of I2S data (buffer half at given offset)
static void _INPUT_HandleI2SRx(I2S_HandleTypeDef* hi2s, uint32_t buf_offset) {
  //select correct input and buffer
  INPUT_Source input;
  q31_t* buf;
  if (hi2s == &hi2s1) {
    input = INPUT_I2S1;
    buf = _i2s1_rx_buffer;
  } else if (hi2s == &hi2s2) {
    input = INPUT_I2S2;
    buf = _i2s2_rx_buffer;
  } else if (hi2s == &hi2s3) {
    input = INPUT_I2S3;
    buf = _i2s3_rx_buffer;
  } else {
    return;
  }

  //pass received samples to the input processing function - interleaved, 2 channels, one batch of samples, no shift
  INPUT_ProcessSamples(input, buf + buf_offset, 2, 2, INPUT_I2S_RX_BATCH_CHANNEL_SAMPLES, INPUT_I2S_RX_BATCH_CHANNEL_SAMPLES, 0);
}

//reset the given I2S input for new reception
static void _INPUT_ResetI2S(INPUT_Source input) {
  //select correct I2S instance and buffer
  I2S_HandleTypeDef* hi2s;
  q31_t* buf;
  switch (input) {
    case INPUT_I2S1:
      hi2s = &hi2s1;
      buf = _i2s1_rx_buffer;
      break;
    case INPUT_I2S2:
      hi2s = &hi2s2;
      buf = _i2s2_rx_buffer;
      break;
    case INPUT_I2S3:
      hi2s = &hi2s3;
      buf = _i2s3_rx_buffer;
      break;
    default:
      return;
  }

  HAL_I2S_DMAStop(hi2s);
  HAL_I2S_Receive_DMA(hi2s, (uint16_t*)buf, INPUT_I2S_RX_BUF_SAMPLES);
}


//I2S receive first buffer half callback
void HAL_I2S_RxHalfCpltCallback(I2S_HandleTypeDef *hi2s) {
  _INPUT_HandleI2SRx(hi2s, 0);
}

//I2S receive second buffer half callback
void HAL_I2S_RxCpltCallback(I2S_HandleTypeDef *hi2s) {
  _INPUT_HandleI2SRx(hi2s, INPUT_I2S_RX_BATCH_TOTAL_SAMPLES);
}

//handle I2S reception errors
void HAL_I2S_ErrorCallback(I2S_HandleTypeDef *hi2s) {
  //select correct input and buffer
  INPUT_Source input;
  q31_t* buf;
  if (hi2s == &hi2s1) {
    input = INPUT_I2S1;
    buf = _i2s1_rx_buffer;
  } else if (hi2s == &hi2s2) {
    input = INPUT_I2S2;
    buf = _i2s2_rx_buffer;
  } else if (hi2s == &hi2s3) {
    input = INPUT_I2S3;
    buf = _i2s3_rx_buffer;
  } else {
    return;
  }

  DEBUG_PRINTF("*** I2S %u Error: %lu", input, HAL_I2S_GetError(hi2s));

  //stop and restart reception
  HAL_I2S_DMAStop(hi2s);
  HAL_I2S_Receive_DMA(hi2s, (uint16_t*)buf, INPUT_I2S_RX_BUF_SAMPLES);
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

  //initialise I2S sample rates with defaults specified in hardware config
  input_i2s_sample_rates[0] = hi2s1.Init.AudioFreq;
  input_i2s_sample_rates[1] = hi2s2.Init.AudioFreq;
  input_i2s_sample_rates[2] = hi2s3.Init.AudioFreq;

  //abort any potentially ongoing MDMA transfer and register the MDMA transfer complete callback for sample handling
  HAL_MDMA_Abort(&hmdma_mdma_channel0_sw_0);
  ReturnOnError(HAL_MDMA_RegisterCallback(&hmdma_mdma_channel0_sw_0, HAL_MDMA_XFER_CPLT_CB_ID, &_INPUT_MDMA_CompleteCallback));

  //stop any potentially ongoing I2S reception and start receiving on all I2S interfaces
  HAL_I2S_DMAStop(&hi2s1);
  HAL_I2S_DMAStop(&hi2s2);
  HAL_I2S_DMAStop(&hi2s3);
  HAL_I2S_Receive_DMA(&hi2s1, (uint16_t*)_i2s1_rx_buffer, INPUT_I2S_RX_BUF_SAMPLES);
  HAL_I2S_Receive_DMA(&hi2s2, (uint16_t*)_i2s2_rx_buffer, INPUT_I2S_RX_BUF_SAMPLES);
  HAL_I2S_Receive_DMA(&hi2s3, (uint16_t*)_i2s3_rx_buffer, INPUT_I2S_RX_BUF_SAMPLES);

  //initialise SPDIF
  return SPDIF_Init();
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

  if (!inputs_available[input] && input_active != input) {
    //nothing to do if input is already unavailable
    return;
  }

  //make unavailable and not silent
  inputs_available[input] = false;
  _inputs_silent[input] = false;

  I2C_TriggerInterrupt(I2CDEF_DAP_INT_FLAGS_INT_INPUT_AVAILABLE_Msk);

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
    I2C_TriggerInterrupt(I2CDEF_DAP_INT_FLAGS_INT_ACTIVE_INPUT_Msk);
  }

  //for I2S inputs: reset for new reception, to avoid data misalignment next time
  if (input == INPUT_I2S1 || input == INPUT_I2S2 || input == INPUT_I2S3) {
    _INPUT_ResetI2S(input);
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

  //for I2S inputs: reset for new reception, to avoid data misalignment - unnecessary in most cases, but mightt help some edge cases
  if (input == INPUT_I2S1 || input == INPUT_I2S2 || input == INPUT_I2S3) {
    _INPUT_ResetI2S(input);
  }

  I2C_TriggerInterrupt(I2CDEF_DAP_INT_FLAGS_INT_ACTIVE_INPUT_Msk);

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
      rate = input_i2s_sample_rates[0];
      break;
    case INPUT_I2S2:
      rate = input_i2s_sample_rates[1];
      break;
    case INPUT_I2S3:
      rate = input_i2s_sample_rates[2];
      break;
    case INPUT_USB:
      haudio = (USBD_AUDIO_HandleTypeDef*)hUsbDeviceHS.pClassDataCmsit[hUsbDeviceHS.classId];
      rate = haudio->freq;
      break;
    case INPUT_SPDIF:
      rate = spdif_sample_rate_enum;
      break;
    default:
      //invalid input
      DEBUG_PRINTF("* Attempted to update sample rate of invalid input %u\n", input);
      return HAL_ERROR;
  }

  if (!SRC_IsValidSampleRate(rate)) {
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

    I2C_TriggerInterrupt(I2CDEF_DAP_INT_FLAGS_INT_INPUT_RATE_Msk);
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
    I2C_TriggerInterrupt(I2CDEF_DAP_INT_FLAGS_INT_INPUT_AVAILABLE_Msk);
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
    HAL_MDMA_Start_IT(&hmdma_mdma_channel0_sw_0, (uint32_t)in_buf, (uint32_t)_input_dma_sample_buf, transfer_bytes, 1);
  }

  return HAL_OK;
}
