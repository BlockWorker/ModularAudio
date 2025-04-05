/*
 * signal_processing.c
 *
 *  Created on: Apr 5, 2025
 *      Author: Alex
 *
 *  Handles audio signal processing (after sample rate conversion), including mixing, effects, and filters.
 */

#include "signal_processing.h"
#include "sample_rate_conv.h"


#define SP_FIR_TEST_LENGTH 128
static q31_t __DTCM_BSS _sp_fir_test_coeff[SP_FIR_TEST_LENGTH];

//2x FIR interpolator instances (with state of length blockSize + phaseLength - 1 per channel, as required by CMSIS-DSP)
static q31_t                __DTCM_BSS  _sp_fir_test_states    [SP_MAX_CHANNELS][SP_BATCH_CHANNEL_SAMPLES + SP_FIR_TEST_LENGTH - 1];
static arm_fir_instance_q31 __DTCM_BSS  _sp_fir_test_instances [SP_MAX_CHANNELS];


//temporary scratch buffers for processing
static q31_t __DTCM_BSS _sp_scratch_a[SP_MAX_CHANNELS][SP_BATCH_CHANNEL_SAMPLES];
static q31_t __DTCM_BSS _sp_scratch_b[SP_MAX_CHANNELS][SP_BATCH_CHANNEL_SAMPLES];


//initialise the internal signal processing variables - only needs to be called once
HAL_StatusTypeDef SP_Init() {
  int i;

  memset(_sp_fir_test_coeff, 0, sizeof(_sp_fir_test_coeff));
  _sp_fir_test_coeff[SP_FIR_TEST_LENGTH - 1] = INT32_MAX;

  for (i = 0; i < SP_MAX_CHANNELS; i++) {
    arm_fir_instance_q31* inst = _sp_fir_test_instances + i;
    inst->numTaps = SP_FIR_TEST_LENGTH;
    inst->pCoeffs = _sp_fir_test_coeff;
    inst->pState = _sp_fir_test_states[i];
  }

  SP_Reset();

  return HAL_OK;
}

//resets the internal state of the signal processor
void SP_Reset() {
  memset(_sp_fir_test_states, 0, sizeof(_sp_fir_test_states));
}

//produce `out_channels` output channels with `SP_BATCH_CHANNEL_SAMPLES` samples per channel
//output buffer(s) must have enough space for a full batch of samples!
//channels may be in separate buffers or interleaved, starting at `out_bufs[channel]`, each with step size `out_step`
//will return HAL_BUSY if the preceding SRC is not ready to produce an output batch (will not process anything then)
HAL_StatusTypeDef __RAM_FUNC SP_ProduceOutputBatch(q31_t** out_bufs, uint16_t out_step, uint16_t out_channels) {
  int i, j;

  //check parameters for validity
  if (out_bufs == NULL || out_step < 1 || out_channels < 1 || out_channels > SP_MAX_CHANNELS) {
    DEBUG_PRINTF("* Attempted SP output processing with invalid parameters %p %u %u\n", out_bufs, out_step, out_channels);
    return HAL_ERROR;
  }

  //process SRC output batch into scratch A
  q31_t* src_output_bufs[2] = { _sp_scratch_a[0], _sp_scratch_a[1] };
  ReturnOnError(SRC_ProduceOutputBatch(src_output_bufs, 1, SRC_MAX_CHANNELS));

  //do test processing from scratch A to scratch B, followed by shift back to full scale
  for (i = 0; i < out_channels; i++) {
    arm_fir_fast_q31(_sp_fir_test_instances + i, _sp_scratch_a[i], _sp_scratch_b[i], SP_BATCH_CHANNEL_SAMPLES);
  }

  //process output
  if (out_step == 1) {
    //non-interleaved output: can just directly copy out, while shifting back to full scale
    for (i = 0; i < out_channels; i++) {
      arm_shift_q31(_sp_scratch_b[i], -SRC_OUTPUT_SHIFT, out_bufs[i], SP_BATCH_CHANNEL_SAMPLES);
    }
  } else {
    //interleaved output: perform shift in-place first, then interleave
    for (i = 0; i < out_channels; i++) {
      q31_t* scratch = _sp_scratch_b[i];
      arm_shift_q31(scratch, -SRC_OUTPUT_SHIFT, scratch, SP_BATCH_CHANNEL_SAMPLES);

      q31_t* out_ptr = out_bufs[i];
      for (j = 0; j < SP_BATCH_CHANNEL_SAMPLES; j++) {
        *out_ptr = scratch[j];
        out_ptr += out_step;
      }
    }
  }

  return HAL_OK;
}

