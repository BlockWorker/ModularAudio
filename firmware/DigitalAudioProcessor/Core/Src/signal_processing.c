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


#undef SP_ENABLE_TEST_FIR
#define SP_ENABLE_TEST_FIR

#if SP_MAX_CHANNELS < SRC_MAX_CHANNELS
#error "Signal processor must be able to handle at least as many channels as the SRC"
#endif

#if SP_BATCH_CHANNEL_SAMPLES != SRC_BATCH_CHANNEL_SAMPLES
#error "Signal processor batch size must match SRC batch size"
#endif


//test FIR filter instance - for determining the remaining available computation power
#ifdef SP_ENABLE_TEST_FIR
#define SP_FIR_TEST_LENGTH 320
static q31_t __DTCM_BSS _sp_fir_test_coeff[SP_FIR_TEST_LENGTH];
static q31_t                __DTCM_BSS  _sp_fir_test_states    [SP_MAX_CHANNELS][SP_BATCH_CHANNEL_SAMPLES + SP_FIR_TEST_LENGTH - 1];
static arm_fir_instance_q31 __DTCM_BSS  _sp_fir_test_instances [SP_MAX_CHANNELS];
#endif

//mixer gain matrix - rows = output channels (for further processing), columns = input/SRC channels, effective gains are matrix values * 2 (to allow for bigger range)
q31_t __DTCM_BSS sp_mixer_gains[SP_MAX_CHANNELS][SRC_MAX_CHANNELS];
static arm_matrix_instance_q31 __DTCM_DATA _sp_mixer_gain_matrix = { SP_MAX_CHANNELS, SRC_MAX_CHANNELS, sp_mixer_gains[0] };

//biquad filter cascades
q31_t __DTCM_BSS sp_biquad_coeffs[SP_MAX_CHANNELS][5 * SP_MAX_BIQUADS];
static q31_t __DTCM_BSS _sp_biquad_states[SP_MAX_CHANNELS][4 * SP_MAX_BIQUADS];
static arm_biquad_casd_df1_inst_q31 _sp_biquad_instances[SP_MAX_CHANNELS];

//temporary scratch buffers for processing
static q31_t __DTCM_BSS _sp_scratch_a[SP_MAX_CHANNELS][SP_BATCH_CHANNEL_SAMPLES];
static q31_t __DTCM_BSS _sp_scratch_b[SP_MAX_CHANNELS][SP_BATCH_CHANNEL_SAMPLES];
//matrix representations of the above for initial mixer processing (scratch A matrix restricted to input channels from SRC)
static arm_matrix_instance_q31 __DTCM_DATA _sp_scratch_a_mixer_matrix = { SRC_MAX_CHANNELS, SP_BATCH_CHANNEL_SAMPLES, _sp_scratch_a[0] };
static arm_matrix_instance_q31 __DTCM_DATA _sp_scratch_b_mixer_matrix = { SP_MAX_CHANNELS, SP_BATCH_CHANNEL_SAMPLES, _sp_scratch_b[0] };


//initialise the internal signal processing variables - only needs to be called once
HAL_StatusTypeDef SP_Init() {
  int i, j;

#ifdef SP_ENABLE_TEST_FIR
  //create test FIR filter with (almost) no actual effect on the signal
  memset(_sp_fir_test_coeff, 0, sizeof(_sp_fir_test_coeff));
  _sp_fir_test_coeff[SP_FIR_TEST_LENGTH - 1] = INT32_MAX;

  for (i = 0; i < SP_MAX_CHANNELS; i++) {
    arm_fir_instance_q31* inst = _sp_fir_test_instances + i;
    inst->numTaps = SP_FIR_TEST_LENGTH;
    inst->pCoeffs = _sp_fir_test_coeff;
    inst->pState = _sp_fir_test_states[i];
  }
#endif

  //initialise mixer gains to keep SRC channels as they are (identity matrix, taking 2x factor into account)
  memset(sp_mixer_gains, 0, sizeof(sp_mixer_gains));
  for (i = 0; i < SRC_MAX_CHANNELS; i++) {
    sp_mixer_gains[i][i] = 0x40000000;
  }

  //initialise biquad cascades
  memset(sp_biquad_coeffs, 0, sizeof(sp_biquad_coeffs));
  for (i = 0; i < SP_MAX_CHANNELS; i++) {
    arm_biquad_casd_df1_inst_q31* inst = _sp_biquad_instances + i;
    inst->numStages = 0; //no active stages by default
    inst->pState = _sp_biquad_states[i];
    inst->pCoeffs = sp_biquad_coeffs[i];
    inst->postShift = 1; //needed for defaulting to no effect on the signal
    //setup coefficients for no effect on the signal
    for (j = 0; j < SP_MAX_BIQUADS; j++) {
      sp_biquad_coeffs[i][5 * j] = 0x40000000;
    }
  }

  SP_Reset();

  return HAL_OK;
}

//resets the internal state of the signal processor
void SP_Reset() {
  memset(_sp_fir_test_states, 0, sizeof(_sp_fir_test_states));
  memset(_sp_biquad_states, 0, sizeof(_sp_biquad_states));
}

//setup the biquad filter parameters: number of filters and post-shift for each channel
//post-shift n effectively scales the biquad coefficients for that channel by 2^n (for all cascaded filters in that channel!)
//should always be followed by a reset for correct filter behaviour
HAL_StatusTypeDef SP_SetupBiquads(uint8_t* filter_counts, uint8_t* post_shifts) {
  int i;

  //check parameters for validity
  if (filter_counts == NULL || post_shifts == NULL) {
    return HAL_ERROR;
  }
  for (i = 0; i < SP_MAX_CHANNELS; i++) {
    if (filter_counts[i] > SP_MAX_BIQUADS || post_shifts[i] > 31) {
      return HAL_ERROR;
    }
  }

  //set up parameters
  for (i = 0; i < SP_MAX_CHANNELS; i++) {
    arm_biquad_casd_df1_inst_q31* inst = _sp_biquad_instances + i;
    inst->numStages = filter_counts[i];
    inst->postShift = post_shifts[i];
  }

  return HAL_OK;
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
  q31_t* src_output_bufs[SRC_MAX_CHANNELS];
  for (i = 0; i < SRC_MAX_CHANNELS; i++) {
    src_output_bufs[i] = _sp_scratch_a[i];
  }
  ReturnOnError(SRC_ProduceOutputBatch(src_output_bufs, 1, SRC_MAX_CHANNELS));

  //perform mixer calculations to map SRC channels to SP channels
  arm_mat_mult_fast_q31(&_sp_mixer_gain_matrix, &_sp_scratch_a_mixer_matrix, &_sp_scratch_b_mixer_matrix);
  //shift result left by one bit to effectively double the mixer gains
  arm_shift_q31(_sp_scratch_b[0], 1, _sp_scratch_b[0], SP_MAX_CHANNELS * SP_BATCH_CHANNEL_SAMPLES);

  //pointers to "input" and "output" scratch buffers to be used in the next processing step - along with a function to swap them after each operation
  q31_t (*scratch_in)[SP_BATCH_CHANNEL_SAMPLES] = _sp_scratch_b;
  q31_t (*scratch_out)[SP_BATCH_CHANNEL_SAMPLES] = _sp_scratch_a;
  inline void _SwapScratchPointers() {
    q31_t (*temp)[SP_BATCH_CHANNEL_SAMPLES] = scratch_in;
    scratch_in = scratch_out;
    scratch_out = temp;
  }

  //process biquad cascades
  for (i = 0; i < out_channels; i++) {
    arm_biquad_casd_df1_inst_q31* inst = _sp_biquad_instances + i;
    if (inst->numStages == 0) {
      //no biquads to process: just copy the existing data
      arm_copy_q31(scratch_in[i], scratch_out[i], SP_BATCH_CHANNEL_SAMPLES);
    } else {
      //at least one biquad active: apply cascade
      arm_biquad_cascade_df1_fast_q31(inst, scratch_in[i], scratch_out[i], SP_BATCH_CHANNEL_SAMPLES);
    }
  }
  _SwapScratchPointers();

#ifdef SP_ENABLE_TEST_FIR
  //do test processing
  for (i = 0; i < out_channels; i++) {
    arm_fir_fast_q31(_sp_fir_test_instances + i, scratch_in[i], scratch_out[i], SP_BATCH_CHANNEL_SAMPLES);
  }
  _SwapScratchPointers();
#endif

  //final output
  if (out_step == 1) {
    //non-interleaved output: can just directly copy out, while shifting back to full scale
    for (i = 0; i < out_channels; i++) {
      arm_shift_q31(scratch_in[i], -SRC_OUTPUT_SHIFT, out_bufs[i], SP_BATCH_CHANNEL_SAMPLES);
    }
  } else {
    //interleaved output: perform shift in-place first, then interleave
    for (i = 0; i < out_channels; i++) {
      q31_t* scratch = scratch_in[i];
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

