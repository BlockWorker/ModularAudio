/*
 * spdif.c
 *
 *  Created on: Apr 11, 2025
 *      Author: Alex
 *
 *  Handles SPDIF data reception.
 */

#include "spdif.h"


//sample DMA receive buffer
static q31_t _spdif_sample_rx_buffer[SPDIF_RX_BUF_SAMPLES];
//control DMA receive buffer
static uint32_t _spdif_control_rx_buffer[SPDIF_RX_CTL_BUF_WORDS];

//control block data buffers
static uint8_t _spdif_control_data[SPDIF_RX_CTLBLOCK_CHANNEL_BYTES];
static uint16_t _spdif_control_user_data[SPDIF_RX_CTLBLOCK_CHANNEL_BYTES];
//control block write pointer
static uint16_t _spdif_control_buffer_pointer = 0;
//whether the control block data is valid
static bool _spdif_control_valid = false;

//whether the last control block is acceptable for data reception
static bool _spdif_control_acceptable = false;

//last detected reception sample rate (approximate)
/*static*/ float _spdif_sample_rate_approx = 0.0f;
//last detected reception sample rate (enum value); unknown if invalid or unsupported rate or too far away from nominal rate
SRC_SampleRate spdif_sample_rate_enum = SR_UNKNOWN;

//temporary sample buffers for sample batch processing
static q31_t _spdif_sample_temp_bufs[2][SPDIF_RX_BATCH_CHANNEL_SAMPLES];


//reset the SPDIF reception logic
static void _SPDIF_Reset() {
  //stop current receptions
  HAL_SPDIFRX_DMAStop(&hspdif1);

  //reset internal flags/info
  _spdif_control_buffer_pointer = 0;
  _spdif_control_valid = false;
  _spdif_control_acceptable = false;
  _spdif_sample_rate_approx = 0.0f;
  spdif_sample_rate_enum = SR_UNKNOWN;

  //enable sync complete and interface error interrupts and start sync
  __HAL_SPDIFRX_IDLE(&hspdif1);
  __HAL_SPDIFRX_ENABLE_IT(&hspdif1, SPDIFRX_IMR_SYNCDIE);
  __HAL_SPDIFRX_ENABLE_IT(&hspdif1, SPDIFRX_IMR_IFEIE);
  __HAL_SPDIFRX_SYNC(&hspdif1);
}

//complete SPDIF reception logic reset, once sync is established
static void _SPDIF_CompleteResetAfterSync() {
  //disable sync complete interrupt again, interface error interrupt stays enabled
  __HAL_SPDIFRX_DISABLE_IT(&hspdif1, SPDIFRX_IT_SYNCDIE);

  //start new receptions
  if (HAL_SPDIFRX_ReceiveCtrlFlow_DMA(&hspdif1, _spdif_control_rx_buffer, SPDIF_RX_CTL_BUF_WORDS) != HAL_OK) {
    DEBUG_PRINTF("* SPDIF control receive failed!\n");
    _SPDIF_Reset();
    return;
  }
  if (HAL_SPDIFRX_ReceiveDataFlow_DMA(&hspdif1, (uint32_t*)_spdif_sample_rx_buffer, SPDIF_RX_BUF_SAMPLES) != HAL_OK) {
    DEBUG_PRINTF("* SPDIF data receive failed!\n");
    _SPDIF_Reset();
    return;
  }
}

//interpret a completed control block
static void _SPDIF_InterpretControl() {
  //check for S/PDIF protocol, digital audio, no preemphasis, 2 channels
  bool new_acceptable = (_spdif_control_data[0] & 0x3B) == 0;

  //apply new acceptability value
  if (new_acceptable != _spdif_control_acceptable) {
    _spdif_control_acceptable = new_acceptable;

    if (!new_acceptable) {
      //no longer acceptable: stop input
      INPUT_Stop(INPUT_SPDIF);
    }
  }
}

//process control data from the reception buffer at the given offset
static void _SPDIF_ProcessControl(uint32_t buffer_offset) {
  int i;

  //process received control data, one word at a time
  for (i = 0; i < SPDIF_RX_CTLBLOCK_CHANNEL_BYTES; i++) {
    uint32_t control_word = _spdif_control_rx_buffer[buffer_offset + i];

    //check for start of block
    if ((control_word & SPDIFRX_CSR_SOB_Msk) != 0) {
      if (_spdif_control_valid) {
        //previously received block is valid: interpret it
        _SPDIF_InterpretControl();
      } else {
        //no valid received block: mark the next block as valid
        _spdif_control_valid = true;
      }
      //reset the control buffer pointer
      _spdif_control_buffer_pointer = 0;
    }

    //extract data if we're within a valid block
    if (_spdif_control_valid) {
      _spdif_control_data[_spdif_control_buffer_pointer] = (uint8_t)((control_word & SPDIFRX_CSR_CS_Msk) >> SPDIFRX_CSR_CS_Pos);
      _spdif_control_user_data[_spdif_control_buffer_pointer] = (uint16_t)((control_word & SPDIFRX_CSR_USR_Msk) >> SPDIFRX_CSR_USR_Pos);
    }
  }
}

//process sample data from the reception buffer at the given offset
static void _SPDIF_ProcessSamples(uint32_t buffer_offset) {
  int i, j;

  //calculate approximate sample rate - TODO: ideally this should be based on the true I2C_CKIN frequency (measured somehow?) instead of just the kernel frequency
  uint32_t width5 = (hspdif1.Instance->SR & SPDIFRX_SR_WIDTH5_Msk) >> SPDIFRX_SR_WIDTH5_Pos;
  float raw_approx_rate = (5.0f * (float)SPDIF_KERNEL_CLK_FREQ) / ((float)width5 * 64.0f);
  //apply exponential smoothing
  _spdif_sample_rate_approx = SPDIF_SAMPLE_RATE_EMA_ALPHA * raw_approx_rate + SPDIF_SAMPLE_RATE_EMA_1MALPHA * _spdif_sample_rate_approx;

  //sample rate error margin, depending on whether we already have a valid sample rate or not
  float error_margin = SRC_IsValidSampleRate(spdif_sample_rate_enum) ? SPDIF_MAX_SAMPLE_RATE_ERROR_OFF : SPDIF_MAX_SAMPLE_RATE_ERROR_ON;

  //find corresponding sample rate enum value, if there is one
  SRC_SampleRate detected_rate = SR_UNKNOWN;
  const SRC_SampleRate possible_rates[] = { SR_44K, SR_48K, SR_96K };
  for (i = 0; i < (sizeof(possible_rates) / sizeof(SRC_SampleRate)); i++) {
    SRC_SampleRate rate = possible_rates[i];
    //absolute relative error to the given rate
    float error = fabsf((_spdif_sample_rate_approx / (float)rate) - 1.0f);
    if (error < error_margin) {
      //within error margin: detect this rate
      detected_rate = rate;
      break;
    }
  }

  //update sample rate setting if it changed
  if (detected_rate != spdif_sample_rate_enum) {
    spdif_sample_rate_enum = detected_rate;

    //apply corresponding action to the input management logic
    if (detected_rate == SR_UNKNOWN) {
      //unknown/invalid rate: disable input, and return as we're done processing this batch for sure
      INPUT_Stop(INPUT_SPDIF);
      return;
    } else {
      //valid rate: update rate
      INPUT_UpdateSampleRate(INPUT_SPDIF);
    }
  }

  if (!_spdif_control_valid || !_spdif_control_acceptable || !SRC_IsValidSampleRate(spdif_sample_rate_enum)) {
    //invalid conditions for reception: ignore sample batch
    return;
  }

  //process samples in pairs
  for (i = 0; i < SPDIF_RX_BATCH_CHANNEL_SAMPLES; i++) {
    uint32_t rx_buf_offset = buffer_offset + 2 * i;

    for (j = 0; j < 2; j++) {
      uint32_t rx_sample = _spdif_sample_rx_buffer[rx_buf_offset + j];

      //detect channel by preamble type
      uint32_t preamble_type = (rx_sample & SPDIFRX_DR1_PT_Msk) >> SPDIFRX_DR1_PT_Pos;
      uint32_t channel = (preamble_type == 0x3) ? 1 : 0;

      //copy data or zero, based on validity
      if ((rx_sample & SPDIFRX_DR1_V_Msk) == 0) {
        //valid data: copy
        _spdif_sample_temp_bufs[channel][i] = (rx_sample & SPDIFRX_DR1_DR_Msk);
      } else {
        //invalid data: write zero
        _spdif_sample_temp_bufs[channel][i] = 0;
      }
    }
  }

  //pass samples to input
  INPUT_ProcessSamples(INPUT_SPDIF, _spdif_sample_temp_bufs[0], 1, 2, SPDIF_RX_BATCH_CHANNEL_SAMPLES, SPDIF_RX_BATCH_CHANNEL_SAMPLES, 0);
}


void HAL_SPDIFRX_RxHalfCpltCallback(SPDIFRX_HandleTypeDef *hspdif) {
  _SPDIF_ProcessSamples(0);
}

void HAL_SPDIFRX_RxCpltCallback(SPDIFRX_HandleTypeDef *hspdif) {
  _SPDIF_ProcessSamples(SPDIF_RX_BATCH_TOTAL_SAMPLES);
}

void HAL_SPDIFRX_ErrorCallback(SPDIFRX_HandleTypeDef *hspdif) {
  DEBUG_PRINTF("*** SPDIF Error: %lu", HAL_SPDIFRX_GetError(hspdif));
  _SPDIF_Reset();
}

void HAL_SPDIFRX_CxHalfCpltCallback(SPDIFRX_HandleTypeDef *hspdif) {
  _SPDIF_ProcessControl(0);
}

void HAL_SPDIFRX_CxCpltCallback(SPDIFRX_HandleTypeDef *hspdif) {
  _SPDIF_ProcessControl(SPDIF_RX_CTLBLOCK_CHANNEL_BYTES);
}


//initialise SPDIF reception - only needs to be called once
HAL_StatusTypeDef SPDIF_Init() {
  _SPDIF_Reset();

  return HAL_OK;
}


//SPDIFRX sync done interrupt handler
void SPDIF_HandleSyncDoneIRQ() {
  //DEBUG_PRINTF("SPDIF sync established\n");
  _SPDIF_CompleteResetAfterSync();
}

//SPDIFRX interface error interrupt handler
void SPDIF_HandleInterfaceErrorIRQ() {
  //DEBUG_PRINTF("SPDIF detected frame/sync error\n");
  _SPDIF_Reset();
}
