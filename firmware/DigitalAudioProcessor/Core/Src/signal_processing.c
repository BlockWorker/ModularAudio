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


#define SP_LOUDNESS_BIQUAD_STAGES 3
#define SP_LOUDNESS_BIQUAD_COEFF_POST_SHIFT 1
#define SP_LOUDNESS_BIQUAD_POST_GAIN 2317.7073f


#if SP_MAX_CHANNELS < SRC_MAX_CHANNELS
#error "Signal processor must be able to handle at least as many channels as the SRC"
#endif

#if SP_BATCH_CHANNEL_SAMPLES != SRC_BATCH_CHANNEL_SAMPLES
#error "Signal processor batch size must match SRC batch size"
#endif


//coefficients for loudness compensation biquads
static const q31_t __ITCM_DATA _sp_loudness_coeffs[5 * SP_LOUDNESS_BIQUAD_STAGES] = {
#include "../Data/biquad_loudness_coeffs.txt"
};


//whether the signal processor is enabled (ready to provide output data)
        bool sp_enabled = false;

//mixer gain matrix - rows = output channels (for further processing), columns = input/SRC channels, effective gains are matrix values * 2 (to allow for bigger range)
        q31_t                   __DTCM_BSS  sp_mixer_gains          [SP_MAX_CHANNELS][SRC_MAX_CHANNELS];
static  arm_matrix_instance_q31 __DTCM_DATA _sp_mixer_gain_matrix = { SP_MAX_CHANNELS, SRC_MAX_CHANNELS, sp_mixer_gains[0] };

//biquad filter cascades
        q31_t                         __DTCM_BSS  sp_biquad_coeffs      [SP_MAX_CHANNELS][5 * SP_MAX_BIQUADS];
static  q31_t                         __DTCM_BSS  _sp_biquad_states     [SP_MAX_CHANNELS][4 * SP_MAX_BIQUADS];
static  arm_biquad_casd_df1_inst_q31  __DTCM_BSS  _sp_biquad_instances  [SP_MAX_CHANNELS];

//FIR filters
        q31_t                 __DTCM_BSS  sp_fir_coeffs    [SP_MAX_CHANNELS][SP_MAX_FIR_LENGTH];
static  q31_t                 __DTCM_BSS  _sp_fir_states    [SP_MAX_CHANNELS][SP_BATCH_CHANNEL_SAMPLES + SP_MAX_FIR_LENGTH - 1];
static  arm_fir_instance_q31  __DTCM_BSS  _sp_fir_instances [SP_MAX_CHANNELS];

//volume gains in dB - range between `SP_MIN_VOL_GAIN` and `SP_MAX_VOL_GAIN`
        float __DTCM_BSS  sp_volume_gains_dB[SP_MAX_CHANNELS];
//whether positive dB volume gains are allowed
        bool              sp_volume_allow_positive_dB = false;

//loudness compensation gains in dB - max `SP_MAX_LOUDNESS_GAIN`, less than `SP_MIN_LOUDNESS_ENABLED_GAIN` means loudness compensation is disabled
        float                         __DTCM_BSS  sp_loudness_gains_dB    [SP_MAX_CHANNELS];
//biquad filter cascades for loudness compensation
static  q31_t                         __DTCM_BSS  _sp_loudness_states     [SP_MAX_CHANNELS][4 * SP_LOUDNESS_BIQUAD_STAGES];
static  arm_biquad_casd_df1_inst_q31  __DTCM_BSS  _sp_loudness_instances  [SP_MAX_CHANNELS];

//temporary scratch buffers for processing
static  q31_t __DTCM_BSS  _sp_scratch_a [SP_MAX_CHANNELS][SP_BATCH_CHANNEL_SAMPLES];
static  q31_t __DTCM_BSS  _sp_scratch_b [SP_MAX_CHANNELS][SP_BATCH_CHANNEL_SAMPLES];
//matrix representations of the above for initial mixer processing (scratch A matrix restricted to input channels from SRC)
static  arm_matrix_instance_q31 __DTCM_DATA _sp_scratch_a_mixer_matrix = { SRC_MAX_CHANNELS, SP_BATCH_CHANNEL_SAMPLES, _sp_scratch_a[0] };
static  arm_matrix_instance_q31 __DTCM_DATA _sp_scratch_b_mixer_matrix = { SP_MAX_CHANNELS, SP_BATCH_CHANNEL_SAMPLES, _sp_scratch_b[0] };


//applies given gains (linear and shift) to the given buffers (entire batch), can work in-place - assumes valid inputs!
static inline void _SP_ApplyGain(q31_t* in_buf, q31_t* out_buf, float gain_linear, int8_t gain_shift) {
  //split gain into fraction and exponent (shift)
  int shift_linear;
  float fraction = frexpf(gain_linear, &shift_linear);
  //get resulting shift from both gains combined
  int8_t shift = (int8_t)(shift_linear + gain_shift);
  //convert fraction into fixed-point format
  q31_t fraction_q31;
  arm_float_to_q31(&fraction, &fraction_q31, 1);
  //apply gain to vector
  arm_scale_q31(in_buf, fraction_q31, shift, out_buf, SP_BATCH_CHANNEL_SAMPLES);
}


//initialise the internal signal processing variables - only needs to be called once
HAL_StatusTypeDef SP_Init() {
  int i, j;

  //disable signal processor for now
  sp_enabled = false;
  //disallow positive volume gains by default
  sp_volume_allow_positive_dB = false;

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

  //initialise FIR filters
  memset(sp_fir_coeffs, 0, sizeof(sp_fir_coeffs));
  for (i = 0; i < SP_MAX_CHANNELS; i++) {
    arm_fir_instance_q31* inst = _sp_fir_instances + i;
    inst->numTaps = 0; //not active by default
    inst->pCoeffs = sp_fir_coeffs[i];
    inst->pState = _sp_fir_states[i];
    //for testing: set coefficient 0 (last) to approx 1 (passthrough)
    //sp_fir_coeffs[i][SP_MAX_FIR_LENGTH - 1] = INT32_MAX;
  }

  //initialise volume gains to 0dB
  memset(sp_volume_gains_dB, 0, sizeof(sp_volume_gains_dB));

  //initialise loudness biquad cascades
  for (i = 0; i < SP_MAX_CHANNELS; i++) {
    arm_biquad_casd_df1_inst_q31* inst = _sp_loudness_instances + i;
    inst->numStages = SP_LOUDNESS_BIQUAD_STAGES;
    inst->pState = _sp_loudness_states[i];
    inst->pCoeffs = _sp_loudness_coeffs;
    inst->postShift = SP_LOUDNESS_BIQUAD_COEFF_POST_SHIFT;
    //set loudness gain to negative infinity (disabled) by default
    sp_loudness_gains_dB[i] = -INFINITY;
  }

  SP_Reset();

  //enable signal processor at the end of init
  sp_enabled = true;

  return HAL_OK;
}

//resets the internal state of the signal processor
void SP_Reset() {
  memset(_sp_biquad_states, 0, sizeof(_sp_biquad_states));
  memset(_sp_fir_states, 0, sizeof(_sp_fir_states));
  memset(_sp_loudness_states, 0, sizeof(_sp_loudness_states));
}

//setup the biquad filter parameters: number of filters and post-shift for each channel
//post-shift n effectively scales the biquad coefficients for that channel by 2^n (for all cascaded filters in that channel!)
//should always be followed by a reset for correct filter behaviour
HAL_StatusTypeDef SP_SetupBiquads(const uint8_t* filter_counts, const uint8_t* post_shifts) {
  int i;

  //check parameters for validity
  if (filter_counts == NULL || post_shifts == NULL) {
    DEBUG_PRINTF("* SP biquad setup got null pointer as a parameter\n");
    return HAL_ERROR;
  }
  for (i = 0; i < SP_MAX_CHANNELS; i++) {
    if (filter_counts[i] > SP_MAX_BIQUADS || post_shifts[i] > 31) {
      DEBUG_PRINTF("* SP biquad setup got invalid count %u or shift %u on filter %d\n", filter_counts[i], post_shifts[i], i);
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

//setup the FIR filter lengths for each channel
//should always be followed by a reset for correct filter behaviour
HAL_StatusTypeDef SP_SetupFIRs(const uint16_t* filter_lengths) {
  int i;

  //check parameters for validity
  if (filter_lengths == NULL) {
    DEBUG_PRINTF("* SP FIR setup got null pointer as a parameter\n");
    return HAL_ERROR;
  }
  for (i = 0; i < SP_MAX_CHANNELS; i++) {
    if (filter_lengths[i] > SP_MAX_FIR_LENGTH) {
      DEBUG_PRINTF("* SP FIR setup got invalid length %u on filter %d\n", filter_lengths[i], i);
      return HAL_ERROR;
    }
  }

  //set up lengths
  for (i = 0; i < SP_MAX_CHANNELS; i++) {
    arm_fir_instance_q31* inst = _sp_fir_instances + i;
    inst->numTaps = filter_lengths[i];
  }

  return HAL_OK;
}

//get current biquad setup
void SP_GetBiquadSetup(uint8_t* filter_counts, uint8_t* post_shifts) {
  int i;
  for (i = 0; i < SP_MAX_CHANNELS; i++) {
    arm_biquad_casd_df1_inst_q31* inst = _sp_biquad_instances + i;
    filter_counts[i] = inst->numStages;
    post_shifts[i] = inst->postShift;
  }
}

//get current FIR setup
void SP_GetFIRSetup(uint16_t* filter_lengths) {
  int i;
  for (i = 0; i < SP_MAX_CHANNELS; i++) {
    arm_fir_instance_q31* inst = _sp_fir_instances + i;
    filter_lengths[i] = inst->numTaps;
  }
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

  //stop at this point if the SP is disabled - samples are still consumed but not processed
  if (!sp_enabled) {
    return HAL_BUSY;
  }

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

  //process FIR filters
  for (i = 0; i < out_channels; i++) {
    arm_fir_instance_q31* inst = _sp_fir_instances + i;
    if (inst->numTaps == 0) {
      //FIR inactive: just copy the existing data
      arm_copy_q31(scratch_in[i], scratch_out[i], SP_BATCH_CHANNEL_SAMPLES);
    } else {
      //FIR active: apply it
      arm_fir_fast_q31(inst, scratch_in[i], scratch_out[i], SP_BATCH_CHANNEL_SAMPLES);
    }
  }
  _SwapScratchPointers();

  //process volume gains and loudness compensation
  for (i = 0; i < out_channels; i++) {
    //get gain and clamp it to the valid range
    float* gain_p = sp_volume_gains_dB + i;
    if (isnanf(*gain_p)) {
      *gain_p = 0.0f;
    } else if (*gain_p < SP_MIN_VOL_GAIN) {
      *gain_p = SP_MIN_VOL_GAIN;
    } else if (*gain_p > 0.0f && !sp_volume_allow_positive_dB) {
      *gain_p = 0.0f;
    } else if (*gain_p > SP_MAX_VOL_GAIN) {
      *gain_p = SP_MAX_VOL_GAIN;
    }
    float vol_gain_dB = *gain_p;

    //check for unity gain (special case allowing shortcut)
    if (vol_gain_dB == 0.0f) {
      //unity gain: no processing needed and loudness compensation doesn't apply either
      //just copy the existing data, taking into account the SRC output shift and desired SP output shift
      arm_shift_q31(scratch_in[i], SP_OUTPUT_SHIFT - SRC_OUTPUT_SHIFT, scratch_out[i], SP_BATCH_CHANNEL_SAMPLES);
    } else {
      //non-unity gain: get linear volume gain
      float vol_gain_linear = powf(10.0f, vol_gain_dB / 20.0f);
      //get loudness compensation gain and clamp it to the valid range
      float* loudness_gain_p = sp_loudness_gains_dB + i;
      if (isnanf(*loudness_gain_p)) {
        *loudness_gain_p = -INFINITY;
      } else if (*loudness_gain_p > SP_MAX_LOUDNESS_GAIN) {
        *loudness_gain_p = SP_MAX_LOUDNESS_GAIN;
      }
      float loudness_gain_dB = *loudness_gain_p;

      //check if we need to do loudness compensation or not (also disabled for above-unity volume gain)
      if (vol_gain_dB > 0.0f || loudness_gain_dB < SP_MIN_LOUDNESS_ENABLED_GAIN) {
        //no loudness compensation: just apply volume gain, taking into account the SRC output shift and desired SP output shift
        _SP_ApplyGain(scratch_in[i], scratch_out[i], vol_gain_linear, SP_OUTPUT_SHIFT - SRC_OUTPUT_SHIFT);
      } else {
        //loudness compensation necessary: split into two paths: filtered signal, original signal
        //start by undoing SRC output shift to give the biquads maximum dynamic range to work with (these biquads scale the signal down a lot)
        arm_shift_q31(scratch_in[i], -SRC_OUTPUT_SHIFT, scratch_in[i], SP_BATCH_CHANNEL_SAMPLES);

        //perform biquad filtering
        arm_biquad_cascade_df1_fast_q31(_sp_loudness_instances + i, scratch_in[i], scratch_out[i], SP_BATCH_CHANNEL_SAMPLES);

        //calculate true loudness compensation gain: given gain + volume gain / 2 (in dB), converted to linear
        float loudness_gain_linear = powf(10.0f, (loudness_gain_dB + vol_gain_dB / 2.0f) / 20.0f);
        //clamp loudness compensation gain, to ensure total gain (both paths combined) is at most 1
        float max_loudness_gain_linear = 1.0f - vol_gain_linear;
        if (loudness_gain_linear > max_loudness_gain_linear) {
          loudness_gain_linear = max_loudness_gain_linear;
        }
        //multiply loudness compensation gain by biquad post gain to cancel out the signal reduction caused by the biquads themselves
        loudness_gain_linear *= SP_LOUDNESS_BIQUAD_POST_GAIN;
        //apply resulting loudness compensation gain, taking into account the desired SP output shift
        _SP_ApplyGain(scratch_out[i], scratch_out[i], loudness_gain_linear, SP_OUTPUT_SHIFT);

        //apply volume gain to the original signal, taking into account the desired SP output shift
        _SP_ApplyGain(scratch_in[i], scratch_in[i], vol_gain_linear, SP_OUTPUT_SHIFT);

        //sum the two paths (original + filtered) to get the resulting output signal
        arm_add_q31(scratch_in[i], scratch_out[i], scratch_out[i], SP_BATCH_CHANNEL_SAMPLES);
      }
    }
  }
  _SwapScratchPointers();

  //final output
  if (out_step == 1) {
    //non-interleaved output: can just directly copy out
    for (i = 0; i < out_channels; i++) {
      arm_copy_q31(scratch_in[i], out_bufs[i], SP_BATCH_CHANNEL_SAMPLES);
    }
  } else {
    //interleaved output: interleave
    for (i = 0; i < out_channels; i++) {
      q31_t* scratch = scratch_in[i];
      q31_t* out_ptr = out_bufs[i];
      for (j = 0; j < SP_BATCH_CHANNEL_SAMPLES; j++) {
        *out_ptr = scratch[j];
        out_ptr += out_step;
      }
    }
  }

  return HAL_OK;
}

