/*
 * sample_rate_conv.c
 *
 *  Created on: Apr 1, 2025
 *      Author: Alex
 *
 *  Handles audio sample rate conversion (fixed and adaptive)
 */

#include "sample_rate_conv.h"
#include "fractional_fir.h"


//filter parameter constants
#define SRC_FIR_INT2_PHASE_LENGTH 110
#define SRC_FFIR_160147_PHASE_COUNT 160
#define SRC_FFIR_160147_PHASE_LENGTH 20
#define SRC_FFIR_160147_PHASE_STEP 147
#define SRC_FFIR_ADAP_PHASE_COUNT 96
#define SRC_FFIR_ADAP_PHASE_LENGTH 50

#if SRC_FFIR_ADAP_PHASE_COUNT != SRC_CHANNEL_BATCH_SAMPLES
#error "SRC adaptive FFIR must have as many phases as there are samples per channel per output batch"
#endif


/********************************************************/
/*                  FILTER VARIABLES                    */
/********************************************************/

//filter coefficients for 2x FIR interpolator
static const q31_t __ITCM_DATA _src_fir_int2_coeffs[2 * SRC_FIR_INT2_PHASE_LENGTH] = {
#include "../Data/fir_interp2_coeffs.txt"
};

#pragma GCC diagnostic ignored "-Wmissing-braces" //disable missing braces warning - initialising 2D arrays from contiguous list works fine
//filter coefficients for fixed 160/147 fractional FIR resampler
static const q31_t __ITCM_DATA _src_ffir_160147_coeffs[SRC_FFIR_160147_PHASE_COUNT][SRC_FFIR_160147_PHASE_LENGTH] = {
#include "../Data/ffir_160_147_coeffs.txt"
};

//filter coefficients for adaptive fractional FIR resampler
static const q31_t __ITCM_DATA _src_ffir_adap_coeffs[SRC_FFIR_ADAP_PHASE_COUNT][SRC_FFIR_ADAP_PHASE_LENGTH] = {
#include "../Data/ffir_adaptive_coeffs.txt"
};
#pragma GCC diagnostic pop //re-enable missing braces warning

//2x FIR interpolator instances (with state of length blockSize + phaseLength - 1 per channel, as required by CMSIS-DSP)
static q31_t                            __DTCM_BSS  _src_fir_int2_states    [SRC_MAX_CHANNELS][SRC_INPUT_CHANNEL_SAMPLES_MAX + SRC_FIR_INT2_PHASE_LENGTH - 1];
static arm_fir_interpolate_instance_q31 __DTCM_BSS  _src_fir_int2_instances [SRC_MAX_CHANNELS];

//fixed 160/147 fractional FIR resampler instances (with state of length 2 * (phaseLength - 1) per channel, as required by fractional_fir specification)
static armext_fir_single_instance_q31   __DTCM_BSS  _src_ffir_160147_phase_instances  [SRC_MAX_CHANNELS][SRC_FFIR_160147_PHASE_COUNT];
static q31_t                            __DTCM_BSS  _src_ffir_160147_states           [SRC_MAX_CHANNELS][2 * (SRC_FFIR_160147_PHASE_LENGTH - 1)];
static FFIR_Instance                    __DTCM_BSS  _src_ffir_160147_instances        [SRC_MAX_CHANNELS];

//adaptive fractional FIR resampler instances (with state of length 2 * (phaseLength - 1) per channel, as required by fractional_fir specification)
static armext_fir_single_instance_q31   __DTCM_BSS  _src_ffir_adap_phase_instances  [SRC_MAX_CHANNELS][SRC_FFIR_ADAP_PHASE_COUNT];
static q31_t                            __DTCM_BSS  _src_ffir_adap_states           [SRC_MAX_CHANNELS][2 * (SRC_FFIR_ADAP_PHASE_LENGTH - 1)];
static FFIR_Instance                    __DTCM_BSS  _src_ffir_adap_instances        [SRC_MAX_CHANNELS];


//currently configured input sample rate
static SRC_SampleRate _src_input_rate = SR_UNKNOWN;
//whether the SRC is ready to produce outputs (i.e. adaptive buffer has been pre-filled)
static bool _src_output_ready = false;


//initialise the SRC's internal filter variables - only needs to be called once
HAL_StatusTypeDef SRC_Init() {
  //reset SRC's own state - not configured for any sample rate at first
  _src_input_rate = SR_UNKNOWN;
  _src_output_ready = false;
  //TODO: buffer state init etc

  //clear 2x interpolator states - doesn't happen automatically because we don't call any init function for them
  memset(_src_fir_int2_states, 0, sizeof(_src_fir_int2_states));

  //set up filter instances for each channel
  int i;
  for (i = 0; i < SRC_MAX_CHANNELS; i++) {
    //init channel's 2x FIR interpolator
    arm_fir_interpolate_instance_q31* int2 = _src_fir_int2_instances + i;
    int2->L = 2;
    int2->phaseLength = SRC_FIR_INT2_PHASE_LENGTH;
    int2->pCoeffs = _src_fir_int2_coeffs;
    int2->pState = _src_fir_int2_states[i];

    //init channel's fixed 160/147 fractional FIR resampler
    FFIR_Instance* ffir_160147 = _src_ffir_160147_instances + i;
    ffir_160147->num_phases = SRC_FFIR_160147_PHASE_COUNT;
    ffir_160147->phase_length = SRC_FFIR_160147_PHASE_LENGTH;
    ffir_160147->phase_step_int = SRC_FFIR_160147_PHASE_STEP;
    ffir_160147->phase_step_fract = 0.0f;
    ffir_160147->coeff_array = (const q31_t**)_src_ffir_160147_coeffs;
    ffir_160147->phase_instances = _src_ffir_160147_phase_instances[i];
    ffir_160147->filter_state = _src_ffir_160147_states[i];
    ReturnOnError(FFIR_Init(ffir_160147));

    //init channel's adaptive fractional FIR resampler
    FFIR_Instance* ffir_adap = _src_ffir_adap_instances + i;
    ffir_adap->num_phases = SRC_FFIR_ADAP_PHASE_COUNT;
    ffir_adap->phase_length = SRC_FFIR_ADAP_PHASE_LENGTH;
    ffir_adap->phase_step_int = 0;
    ffir_adap->phase_step_fract = (float)SRC_FFIR_ADAP_PHASE_COUNT; //start with 1:1 resampling ratio initially
    ffir_adap->coeff_array = (const q31_t**)_src_ffir_adap_coeffs;
    ffir_adap->phase_instances = _src_ffir_adap_phase_instances[i];
    ffir_adap->filter_state = _src_ffir_adap_states[i];
    ReturnOnError(FFIR_Init(ffir_adap));
  }

  return HAL_OK;
}

//reset the SRC's internal state and prepare for the conversion of a new input signal at the given sample rate
HAL_StatusTypeDef SRC_Configure(SRC_SampleRate input_rate) {
  int i;

  //check for valid input rate
  if (input_rate != SR_44K && input_rate != SR_48K && input_rate != SR_96K) {
    DEBUG_PRINTF("* Attempted SRC config with invalid sample rate %lu\n", (uint32_t)input_rate);
    return HAL_ERROR;
  }

  //disable output until buffer is refilled
  _src_output_ready = false;

  //switch to new input rate
  _src_input_rate = input_rate;

  //reset and clear filters/resamplers that are needed for the new input rate
  if (_src_input_rate != SR_96K) {
    //needs 2x interpolation: clear 2x interpolator states
    memset(_src_fir_int2_states, 0, sizeof(_src_fir_int2_states));

    if (_src_input_rate == SR_44K) {
      //needs 160/147 resampling: reset fixed FFIR resamplers
      for (i = 0; i < SRC_MAX_CHANNELS; i++) {
        FFIR_Reset(_src_ffir_160147_instances + i);
      }
    }
  }
  //reset adaptive FFIR resamplers and restore default phase steps
  for (i = 0; i < SRC_MAX_CHANNELS; i++) {
    FFIR_Instance* ffir_adap = _src_ffir_adap_instances + i;
    ffir_adap->phase_step_fract = (float)SRC_FFIR_ADAP_PHASE_COUNT;
    FFIR_Reset(ffir_adap);
  }

  //TODO: reset buffer states etc

  return HAL_OK;
}

//get the currently configured input sample rate
SRC_SampleRate SRC_GetCurrentInputRate() {
  return _src_input_rate;
}

//process `in_channels` input channels with `in_samples` samples per channel
//must be at the currently configured input sample rate; to switch sample rate, a re-init is required
//channels may be in separate buffers or interleaved, starting at `in_bufs[channel]`, each with step size `in_step`
HAL_StatusTypeDef __RAM_FUNC SRC_ProcessInputSamples(const q31_t** in_bufs, uint16_t in_step, uint16_t in_channels, uint16_t in_samples) {
  //check for valid input rate
  if (_src_input_rate != SR_44K && _src_input_rate != SR_48K && _src_input_rate != SR_96K) {
    DEBUG_PRINTF("* Attempted SRC input processing with invalid configured sample rate %lu\n", (uint32_t)_src_input_rate);
    return HAL_ERROR;
  }
  //check parameters for validity
  if (in_bufs == NULL || in_step < 1 || in_channels < 1 || in_channels > SRC_MAX_CHANNELS || in_samples > SRC_INPUT_CHANNEL_SAMPLES_MAX) {
    DEBUG_PRINTF("* Attempted SRC input processing with invalid parameters %p %u %u %u\n", in_bufs, in_step, in_channels, in_samples);
    return HAL_ERROR;
  }

  //TODO

  return HAL_OK;
}

//produce `out_channels` output channels with `SRC_CHANNEL_BATCH_SAMPLES` samples per channel
//output buffer(s) must have enough space for a full batch of samples!
//channels may be in separate buffers or interleaved, starting at `out_bufs[channel]`, each with step size `out_step`
//will return HAL_BUSY if the SRC is not ready to produce an output batch (will not process anything then)
HAL_StatusTypeDef __RAM_FUNC SRC_ProduceOutputBatch(q31_t** out_bufs, uint16_t out_step, uint16_t out_channels) {
  //check if we're even ready to output
  if (!_src_output_ready) {
    return HAL_BUSY;
  }

  //TODO

  return HAL_OK;
}
