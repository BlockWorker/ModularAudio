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
#include "inputs.h"


#undef SRC_DEBUG_ADAPTIVE
//#define SRC_DEBUG_ADAPTIVE
#undef SRC_DEBUG_TIMING
//#define SRC_DEBUG_TIMING

//filter parameter constants
#define SRC_FIR_INT2_PHASE_LENGTH 110
#define SRC_FIR_INT2_SHIFT -1

#define SRC_FFIR_160147_PHASE_COUNT 160
#define SRC_FFIR_160147_PHASE_LENGTH 20
#define SRC_FFIR_160147_PHASE_STEP 147
#define SRC_FFIR_160147_SHIFT -4

#define SRC_FFIR_ADAP_PHASE_COUNT 96
#define SRC_FFIR_ADAP_PHASE_LENGTH 50
#define SRC_FFIR_ADAP_SHIFT -4

#if SRC_FFIR_ADAP_PHASE_COUNT != SRC_BATCH_CHANNEL_SAMPLES
#error "SRC adaptive FFIR must have as many phases as there are samples per channel per output batch"
#endif

#if SRC_OUTPUT_SHIFT != MIN(MIN(SRC_FFIR_ADAP_SHIFT, SRC_FFIR_160147_SHIFT), SRC_FIR_INT2_SHIFT)
#error "SRC: Mismatch between sample shift required by filters and specified SRC output shift"
#endif


/********************************************************/
/*                  FILTER VARIABLES                    */
/********************************************************/

//filter coefficients for 2x FIR interpolator
static const q31_t __ITCM_DATA _src_fir_int2_coeffs[2 * SRC_FIR_INT2_PHASE_LENGTH] = {
#include "../Data/fir_interp2_coeffs.txt"
};

//filter coefficients for fixed 160/147 fractional FIR resampler
static const q31_t __ITCM_DATA _src_ffir_160147_coeffs[SRC_FFIR_160147_PHASE_COUNT * SRC_FFIR_160147_PHASE_LENGTH] = {
#include "../Data/ffir_160_147_coeffs.txt"
};

//filter coefficients for adaptive fractional FIR resampler
static const q31_t __ITCM_DATA _src_ffir_adap_coeffs[SRC_FFIR_ADAP_PHASE_COUNT * SRC_FFIR_ADAP_PHASE_LENGTH] = {
#include "../Data/ffir_adaptive_coeffs.txt"
};

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


/********************************************************/
/*                    SRC VARIABLES                     */
/********************************************************/

//currently configured input sample rate
static SRC_SampleRate _src_input_rate = SR_UNKNOWN;
//whether the SRC is ready to produce outputs (i.e. adaptive buffer has been pre-filled)
static bool _src_output_ready = false;

//semi-circular sample buffers for adaptive resampling, one per channel, read/write pointers are shared/synchronised
//each length (2 * buflength - 1), read/write pointers are always in [0, buflength), and the entries in [buflength, 2 * buflength - 1) are a copy of entries [0, buflength - 1)
//this allows circular buffer behaviour *and* contiguous reads/writes, at any point; just needs extra copies on write (_SRC_FinishBufferWrite function)
static q31_t __DTCM_BSS _src_buffers[SRC_MAX_CHANNELS][2 * SRC_BUF_TOTAL_CHANNEL_SAMPLES - 1];
//buffer read and write pointers
static volatile uint32_t __DTCM_BSS _src_buffer_read_ptr;
static volatile uint32_t __DTCM_BSS _src_buffer_write_ptr;
//macros for determining available data and free space in buffer
#define _src_buffer_available_data() ((_src_buffer_write_ptr + SRC_BUF_TOTAL_CHANNEL_SAMPLES - _src_buffer_read_ptr) % SRC_BUF_TOTAL_CHANNEL_SAMPLES)
#define _src_buffer_free_space() (SRC_BUF_TOTAL_CHANNEL_SAMPLES - _src_buffer_available_data() - 1)

//temporary scratch buffers for processing
static q31_t __DTCM_BSS _src_scratch_a[SRC_MAX_CHANNELS][SRC_SCRATCH_CHANNEL_SAMPLES];
static q31_t __DTCM_BSS _src_scratch_b[SRC_MAX_CHANNELS][SRC_SCRATCH_CHANNEL_SAMPLES];

//averaging histories, history positions, and sums for input rate error and buffer fill level error
static int16_t _src_input_rate_error_history[SRC_ADAPTIVE_RATE_ERROR_AVG_BATCHES];
static uint32_t _src_input_rate_error_history_position;
static uint32_t _src_input_rate_error_length;
static int32_t _src_input_rate_error_sum;
static int16_t _src_buffer_fill_error_history[SRC_ADAPTIVE_BUF_ERROR_AVG_BATCHES];
static uint32_t _src_buffer_fill_error_history_position;
static int32_t _src_buffer_fill_error_sum;
static float _src_last_buffer_fill_error_avg;
//counter of input samples since the last output batch
static volatile uint16_t _src_input_samples_since_last_output;

//debug access to internal state
#ifdef DEBUG
float* const src_debug_adaptive_decimation_ptr = &(_src_ffir_adap_instances[0].phase_step_fract);
#endif
#ifdef SRC_DEBUG_ADAPTIVE
static inline void _PrintLogData(float d1, float d2, float d3) {
  static uint32_t sampcounter = 0;
  if (sampcounter++ < 10) return;
  sampcounter = 0;
  int32_t d1i = (int32_t)(1000.0f * d1);
  int32_t d2i = (int32_t)(1000.0f * d2);
  int32_t d3i = (int32_t)(1000.0f * d3);
  printf("%ld %ld %ld\n", d1i, d2i, d3i);
}
#endif
#ifdef SRC_DEBUG_TIMING
volatile unsigned int *DWT_CYCCNT   = (volatile unsigned int *)0xE0001004; //address of the register
volatile unsigned int *DWT_CONTROL  = (volatile unsigned int *)0xE0001000; //address of the register
volatile unsigned int *DWT_LAR      = (volatile unsigned int *)0xE0001FB0; //address of the register
volatile unsigned int *SCB_DEMCR    = (volatile unsigned int *)0xE000EDFC; //address of the register
#define SRC_DEBUG_TIMING_EMA_ALPHA 1.0f
#define SRC_DEBUG_TIMING_EMA_1MALPHA (1.0f - SRC_DEBUG_TIMING_EMA_ALPHA)
static float _src_debug_time_in_avg = 0;
static float _src_debug_time_out_avg = 0;
static inline void _PrintLogData(float d1, float d2) {
  int32_t d1i = (int32_t)(d1);
  int32_t d2i = (int32_t)(d2);
  printf("%ld %ld\n", d1i, d2i);
}
#endif


/********************************************************/
/*                 INTERNAL FUNCTIONS                   */
/********************************************************/

//should be called after every (block) write to the semi-circular buffer, specifying written channels and number of written samples per channel
//handles copying newly written data (to maintain semi-circular buffer properties) and updating the write pointer accordingly
static void __RAM_FUNC _SRC_FinishBufferWrite(uint16_t active_channels, uint32_t written_samples) {
  int i;

  if (active_channels < 1 || active_channels > SRC_MAX_CHANNELS || written_samples < 1 || written_samples >= SRC_BUF_TOTAL_CHANNEL_SAMPLES) {
    //nothing to do or invalid parameters
    return;
  }

  //buffer offset of end of write (points to first unmodified byte)
  uint32_t write_end_offset = _src_buffer_write_ptr + written_samples;

  //copy from first half of buffer to second half, if necessary (which it usually is)
  uint32_t num_copy_to_second_half = (SRC_BUF_TOTAL_CHANNEL_SAMPLES - 1) - _src_buffer_write_ptr;
  if (num_copy_to_second_half > 0) {
    for (i = 0; i < active_channels; i++) {
      arm_copy_q31(_src_buffers[i] + _src_buffer_write_ptr,
                   _src_buffers[i] + _src_buffer_write_ptr + SRC_BUF_TOTAL_CHANNEL_SAMPLES,
                   num_copy_to_second_half);
      /*memcpy(_src_buffers[i] + _src_buffer_write_ptr + SRC_BUF_TOTAL_CHANNEL_SAMPLES,
             _src_buffers[i] + _src_buffer_write_ptr,
             num_copy_to_second_half * sizeof(q31_t));*/
    }
  }

  //copy from second half of buffer to first half, if necessary
  int32_t num_copy_to_first_half = (int32_t)write_end_offset - SRC_BUF_TOTAL_CHANNEL_SAMPLES;
  if (num_copy_to_first_half > 0) {
    for (i = 0; i < active_channels; i++) {
      arm_copy_q31(_src_buffers[i] + SRC_BUF_TOTAL_CHANNEL_SAMPLES,
                   _src_buffers[i],
                   num_copy_to_first_half);
      /*memcpy(_src_buffers[i],
             _src_buffers[i] + SRC_BUF_TOTAL_CHANNEL_SAMPLES,
             num_copy_to_first_half * sizeof(q31_t));*/
    }
  }

  //advance write pointer with wrap-around
  _src_buffer_write_ptr = write_end_offset % SRC_BUF_TOTAL_CHANNEL_SAMPLES;

  //mark SRC as ready to output if we have surpassed the ideal number of samples
  if (!_src_output_ready && _src_buffer_available_data() > SRC_BUF_IDEAL_CHANNEL_SAMPLES) {
    DEBUG_PRINTF("SRC ready: buffer filled to %lu samples\n", _src_buffer_available_data());

    //reset averaging data
    memset(_src_input_rate_error_history, 0, sizeof(_src_input_rate_error_history));
    _src_input_rate_error_history_position = 0;
    _src_input_rate_error_length = SRC_ADAPTIVE_RATE_ERROR_AVG_BATCHES_INITIAL;
    _src_input_rate_error_sum = 0;
    memset(_src_buffer_fill_error_history, 0, sizeof(_src_buffer_fill_error_history));
    _src_buffer_fill_error_history_position = 0;
    _src_buffer_fill_error_sum = 0;
    _src_last_buffer_fill_error_avg = 0.0f;

    _src_output_ready = true;
  }
}


/********************************************************/
/*                    API FUNCTIONS                     */
/********************************************************/

//initialise the SRC's internal filter variables - only needs to be called once
HAL_StatusTypeDef SRC_Init() {
  //reset SRC's own state - not configured for any sample rate at first
  _src_input_rate = SR_UNKNOWN;
  _src_output_ready = false;
  _src_buffer_read_ptr = 0;
  _src_buffer_write_ptr = 0;

  //reset averaging data
  memset(_src_input_rate_error_history, 0, sizeof(_src_input_rate_error_history));
  _src_input_rate_error_history_position = 0;
  _src_input_rate_error_length = SRC_ADAPTIVE_RATE_ERROR_AVG_BATCHES_INITIAL;
  _src_input_rate_error_sum = 0;
  memset(_src_buffer_fill_error_history, 0, sizeof(_src_buffer_fill_error_history));
  _src_buffer_fill_error_history_position = 0;
  _src_buffer_fill_error_sum = 0;
  _src_last_buffer_fill_error_avg = 0.0f;


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
    ffir_160147->coeff_array = _src_ffir_160147_coeffs;
    ffir_160147->phase_instances = _src_ffir_160147_phase_instances[i];
    ffir_160147->filter_state = _src_ffir_160147_states[i];
    ReturnOnError(FFIR_Init(ffir_160147));

    //init channel's adaptive fractional FIR resampler
    FFIR_Instance* ffir_adap = _src_ffir_adap_instances + i;
    ffir_adap->num_phases = SRC_FFIR_ADAP_PHASE_COUNT;
    ffir_adap->phase_length = SRC_FFIR_ADAP_PHASE_LENGTH;
    ffir_adap->phase_step_int = 0;
    ffir_adap->phase_step_fract = (float)SRC_FFIR_ADAP_PHASE_COUNT; //start with 1:1 resampling ratio initially
    ffir_adap->coeff_array = _src_ffir_adap_coeffs;
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

  //reset must happen atomically
  __disable_irq();

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

  //reset adaptive resampling buffer to empty
  _src_buffer_read_ptr = 0;
  _src_buffer_write_ptr = 0;

  __enable_irq();

  DEBUG_PRINTF("SRC configured to input sample rate %u\n", _src_input_rate);

#ifdef SRC_DEBUG_TIMING
  HAL_TIM_Base_Start(&htim2);
  HAL_TIM_Base_Start(&htim5);
#endif

  return HAL_OK;
}

//get the currently configured input sample rate
SRC_SampleRate SRC_GetCurrentInputRate() {
  return _src_input_rate;
}

//get the average relative input rate error
float SRC_GetAverageRateError() {
  //average error, in samples per batch
  float sample_error = (float)_src_input_rate_error_sum / (float)_src_input_rate_error_length;
  //convert to relative error and return
  return sample_error / (float)SRC_BATCH_CHANNEL_SAMPLES;
}

//get the average buffer fill error in samples
float SRC_GetAverageBufferFillError() {
  //calculate and return error in samples directly
  return (float)_src_buffer_fill_error_sum / (float)SRC_ADAPTIVE_BUF_ERROR_AVG_BATCHES;
}

//process `in_channels` input channels with `in_samples` samples per channel; where inputs were previously shifted to `in_shift` (negative = shifted right)
//must be at the currently configured input sample rate; to switch sample rate, a re-init is required
//channels may be in separate buffers or interleaved, starting at `in_bufs[channel]`, each with step size `in_step`
//if the SRC can't accept all given input samples, old samples will be silently discarded
HAL_StatusTypeDef __RAM_FUNC SRC_ProcessInputSamples(const q31_t** in_bufs, uint16_t in_step, uint16_t in_channels, uint16_t in_samples, int8_t in_shift) {
  int i, j;

#ifdef SRC_DEBUG_TIMING
  htim2.Instance->CNT = 0;
#endif

  //check parameters for validity
  if (in_bufs == NULL || in_step < 1 || in_channels < 1 || in_channels > SRC_MAX_CHANNELS || in_samples > SRC_INPUT_CHANNEL_SAMPLES_MAX) {
    DEBUG_PRINTF("* Attempted SRC input processing with invalid parameters %p %u %u %u\n", in_bufs, in_step, in_channels, in_samples);
    return HAL_ERROR;
  }

  //calculate required space for the given input samples
  uint32_t required_space;
  switch (_src_input_rate) {
    case SR_44K:
      required_space = (uint32_t)in_samples * 320 / 147 + 1;
      break;
    case SR_48K:
      required_space = (uint32_t)in_samples * 2;
      break;
    case SR_96K:
      required_space = (uint32_t)in_samples;
      break;
    default:
      DEBUG_PRINTF("* Attempted SRC input processing with invalid configured sample rate %lu\n", (uint32_t)_src_input_rate);
      return HAL_ERROR;
  }
  //check available space and potentially make space by discarding - must happen atomically
  __disable_irq();
  uint32_t available_space = _src_buffer_free_space();
  if (available_space < required_space) {
    //insufficient space for input samples: silently discard excess samples by advancing the read pointer (must happen atomically)
    uint32_t discard_count = required_space - available_space;
    _src_buffer_read_ptr = (_src_buffer_read_ptr + discard_count) % SRC_BUF_TOTAL_CHANNEL_SAMPLES;
  }
  __enable_irq();

  //pointers to buffers to be used as the actual inputs
  const q31_t* true_input_buffers[SRC_MAX_CHANNELS];
  //number of samples per channel written into the adaptive resampling buffer
  uint32_t written_samples;

  //further processing needs non-interleaved samples
  if (in_step == 1) {
    //inputs already not interleaved: use them directly
    for (i = 0; i < in_channels; i++) {
      true_input_buffers[i] = in_bufs[i];
    }
  } else {
    //inputs are interleaved: de-interleave into scratch A, then use that
    for (i = 0; i < in_channels; i++) {
      q31_t* scratch = _src_scratch_a[i];
      const q31_t* inp = in_bufs[i];
      for (j = 0; j < in_samples; j++) {
        scratch[j] = *inp;
        inp += in_step;
      }
      true_input_buffers[i] = scratch;
    }
  }

  //process any potential fixed-ratio resampling, according to input sample rate
  if (_src_input_rate == SR_96K) {
    //already at target 96k rate: only adaptive resampling needed, so just copy the inputs into the buffer while applying the necessary shift
    for (i = 0; i < in_channels; i++) {
      arm_shift_q31(true_input_buffers[i], SRC_OUTPUT_SHIFT - in_shift, _src_buffers[i] + _src_buffer_write_ptr, in_samples);
    }
    written_samples = in_samples;
  } else {
    //both 44.1k and 48k rates need 2x interpolation: perform 2x interpolation from input buffers
    for (i = 0; i < in_channels; i++) {
      //select output buffer depending on sample rate: for 44.1k, use scratch B for further processing; for 48k, use the adaptive resampling buffer directly
      q31_t* out = (_src_input_rate == SR_44K) ? (_src_scratch_b[i]) : (_src_buffers[i] + _src_buffer_write_ptr);

      //perform necessary shift first (into scratch A), then interpolate
      arm_shift_q31(true_input_buffers[i], SRC_OUTPUT_SHIFT - in_shift, _src_scratch_a[i], in_samples);
      arm_fir_interpolate_q31(_src_fir_int2_instances + i, _src_scratch_a[i], out, in_samples);
    }
    written_samples = 2 * in_samples;

    //fractional resampling step for 44.1k rate
    if (_src_input_rate == SR_44K) {
      //perform fractional resampling from scratch B into adaptive resampling buffer
      for (i = 0; i < in_channels; i++) {
        q31_t* inp = _src_scratch_b[i];
        q31_t* out = _src_buffers[i] + _src_buffer_write_ptr;

        uint32_t out_samples = FFIR_Process(_src_ffir_160147_instances + i, inp, inp + (2 * in_samples), 1, out, out + required_space, 1, NULL);

        //all channels should produce same number of samples from resampling, but take the maximum just in case
        if (out_samples > written_samples) {
          written_samples = out_samples;
        }
      }
    }
  }

  //perform final buffer processing for the written samples
  _SRC_FinishBufferWrite(in_channels, written_samples);

  //update number of samples since last output batch (atomically)
  __disable_irq();
  _src_input_samples_since_last_output += written_samples;
  __enable_irq();

#ifdef SRC_DEBUG_TIMING
  _src_debug_time_in_avg = SRC_DEBUG_TIMING_EMA_ALPHA * (float)htim2.Instance->CNT + SRC_DEBUG_TIMING_EMA_1MALPHA * _src_debug_time_in_avg;
#endif

  return HAL_OK;
}

//produce `out_channels` output channels with `SRC_CHANNEL_BATCH_SAMPLES` samples per channel
//output buffer(s) must have enough space for a full batch of samples!
//channels may be in separate buffers or interleaved, starting at `out_bufs[channel]`, each with step size `out_step`
//will return HAL_BUSY if the SRC is not ready to produce an output batch (will not process anything then)
HAL_StatusTypeDef __RAM_FUNC SRC_ProduceOutputBatch(q31_t** out_bufs, uint16_t out_step, uint16_t out_channels) {
  int i;

#ifdef SRC_DEBUG_TIMING
  htim5.Instance->CNT = 0;
#endif

  //check parameters for validity
  if (out_bufs == NULL || out_step < 1 || out_channels < 1 || out_channels > SRC_MAX_CHANNELS) {
    DEBUG_PRINTF("* Attempted SRC output processing with invalid parameters %p %u %u\n", out_bufs, out_step, out_channels);
    return HAL_ERROR;
  }

  //check if we're even ready to output
  if (!_src_output_ready) {
#ifdef SRC_DEBUG_ADAPTIVE
    _PrintLogData((float)_src_input_rate_error_sum / (float)_src_input_rate_error_length,
                  (float)_src_buffer_fill_error_sum / (float)SRC_ADAPTIVE_BUF_ERROR_AVG_BATCHES,
                  _src_ffir_adap_instances[0].phase_step_fract - (float)SRC_BATCH_CHANNEL_SAMPLES);
#endif

    _src_input_samples_since_last_output = 0;
    return HAL_BUSY;
  }

  //check how many input samples we have
  uint32_t available_input_samples = _src_buffer_available_data();
  if (available_input_samples < SRC_BUF_CRITICAL_CHANNEL_SAMPLES) {
    //buffer level is critically low: reset to "not ready" until buffer is refilled sufficiently
    DEBUG_PRINTF("SRC buffer critical (%lu samples), disabling until refilled\n", available_input_samples);

#ifdef SRC_DEBUG_ADAPTIVE
    _PrintLogData((float)_src_input_rate_error_sum / (float)_src_input_rate_error_length,
                  (float)_src_buffer_fill_error_sum / (float)SRC_ADAPTIVE_BUF_ERROR_AVG_BATCHES,
                  _src_ffir_adap_instances[0].phase_step_fract - (float)SRC_BATCH_CHANNEL_SAMPLES);
#endif
    _src_output_ready = false;
    _src_input_samples_since_last_output = 0;

    //stop currently active input (if valid)
    if (input_active > INPUT_NONE && input_active < _INPUT_COUNT) {
      INPUT_Stop(input_active);
    }

    return HAL_BUSY;
  }

  //update average input rate error - average length grows after startup
  int16_t input_rate_error = (int16_t)_src_input_samples_since_last_output - SRC_BATCH_CHANNEL_SAMPLES;
  _src_input_samples_since_last_output = 0;
  if (_src_input_rate_error_length >= SRC_ADAPTIVE_RATE_ERROR_AVG_BATCHES) {
    _src_input_rate_error_length = SRC_ADAPTIVE_RATE_ERROR_AVG_BATCHES;
    _src_input_rate_error_sum -= _src_input_rate_error_history[_src_input_rate_error_history_position];
  } else {
    _src_input_rate_error_length++;
  }
  _src_input_rate_error_sum += input_rate_error;
  _src_input_rate_error_history[_src_input_rate_error_history_position] = input_rate_error;
  //update average buffer fill error - average length is constant
  int16_t buffer_fill_error = (int16_t)available_input_samples - SRC_BUF_IDEAL_CHANNEL_SAMPLES;
  _src_buffer_fill_error_sum -= _src_buffer_fill_error_history[_src_buffer_fill_error_history_position];
  _src_buffer_fill_error_sum += buffer_fill_error;
  _src_buffer_fill_error_history[_src_buffer_fill_error_history_position] = buffer_fill_error;
  //update positions for averaging history
  _src_input_rate_error_history_position = (_src_input_rate_error_history_position + 1) % SRC_ADAPTIVE_RATE_ERROR_AVG_BATCHES;
  _src_buffer_fill_error_history_position = (_src_buffer_fill_error_history_position + 1) % SRC_ADAPTIVE_BUF_ERROR_AVG_BATCHES;


  //compute adaptive resampler's phase step (decimation factor) as the average fill margin (exponential moving average), starting with the first channel
  /*_src_ffir_adap_instances[0].phase_step_fract =
      (SRC_ADAPTIVE_EMA_ALPHA * (float)samples_above_ideal) + (SRC_ADAPTIVE_EMA_1MALPHA * _src_ffir_adap_instances[0].phase_step_fract);*/
  //compute averaged input rate error, averaged buffer fill error, and derivative of the latter (for predictive correction)
  float input_rate_error_avg = (float)_src_input_rate_error_sum / (float)_src_input_rate_error_length;
  float buffer_fill_error_avg = (float)_src_buffer_fill_error_sum / (float)SRC_ADAPTIVE_BUF_ERROR_AVG_BATCHES;
  float buffer_fill_error_d = buffer_fill_error_avg - _src_last_buffer_fill_error_avg;
  //update "last value" variable for buffer fill error, for next derivative calculation
  _src_last_buffer_fill_error_avg = buffer_fill_error_avg;

  //compute adaptive resampler's phase step (decimation factor) from the errors, starting with the first channel
  _src_ffir_adap_instances[0].phase_step_fract =
      (float)SRC_BATCH_CHANNEL_SAMPLES +
      input_rate_error_avg +
      SRC_ADAPTIVE_BUF_FILL_COEFF_P * buffer_fill_error_avg +
      SRC_ADAPTIVE_BUF_FILL_COEFF_D * buffer_fill_error_d;


  //clamp the phase step to the valid range
  if (_src_ffir_adap_instances[0].phase_step_fract < (float)SRC_BATCH_INPUT_SAMPLES_MIN) {
    _src_ffir_adap_instances[0].phase_step_fract = (float)SRC_BATCH_INPUT_SAMPLES_MIN;
  } else if (_src_ffir_adap_instances[0].phase_step_fract > (float)SRC_BATCH_INPUT_SAMPLES_MAX) {
    _src_ffir_adap_instances[0].phase_step_fract = (float)SRC_BATCH_INPUT_SAMPLES_MAX;
  }

  //copy same phase step to all other active channels - we want to keep all channels synchronised
  for (i = 1; i < out_channels; i++) {
    _src_ffir_adap_instances[i].phase_step_fract = _src_ffir_adap_instances[0].phase_step_fract;
  }

#ifdef SRC_DEBUG_ADAPTIVE
  _PrintLogData(input_rate_error_avg,
                buffer_fill_error_avg,
                _src_ffir_adap_instances[0].phase_step_fract - (float)SRC_BATCH_CHANNEL_SAMPLES);
#endif

  //perform adaptive resampling for all active channels, producing the desired output data
  uint32_t input_samples_consumed = 0;
  for (i = 0; i < out_channels; i++) {
    q31_t* in_start_ptr = _src_buffers[i] + _src_buffer_read_ptr;
    q31_t* in_end_ptr = in_start_ptr + SRC_BUF_TOTAL_CHANNEL_SAMPLES; //allow resampler to read as many samples as it needs
    q31_t* out_start_ptr = out_bufs[i];
    q31_t* out_end_ptr = out_start_ptr + (out_step * SRC_BATCH_CHANNEL_SAMPLES); //produce exactly one batch of output samples

    uint32_t in_samples;
    uint32_t out_samples = FFIR_Process(_src_ffir_adap_instances + i, in_start_ptr, in_end_ptr, 1, out_start_ptr, out_end_ptr, out_step, &in_samples);
    //make sure that we got the expected number of output samples
    if (out_samples != SRC_BATCH_CHANNEL_SAMPLES) {
      DEBUG_PRINTF("* SRC adaptive resampler channel %d produced %lu samples instead of the expected %d!\n", i, out_samples, SRC_BATCH_CHANNEL_SAMPLES);
      return HAL_ERROR;
    }
    //save the maximum number of consumed input samples - should all be the same; force maximum anyway for channel synchronisation
    if (in_samples > input_samples_consumed) {
      input_samples_consumed = in_samples;
    }
  }

  //update the buffer read pointer in accordance with the number of input samples we used
  _src_buffer_read_ptr = (_src_buffer_read_ptr + input_samples_consumed) % SRC_BUF_TOTAL_CHANNEL_SAMPLES;

#ifdef SRC_DEBUG_TIMING
  _src_debug_time_out_avg = SRC_DEBUG_TIMING_EMA_ALPHA * (float)htim5.Instance->CNT + SRC_DEBUG_TIMING_EMA_1MALPHA * _src_debug_time_out_avg;
  static uint32_t sampcounter = 0;
  if (sampcounter++ >= 10) {
    _PrintLogData(_src_debug_time_in_avg, _src_debug_time_out_avg);
    sampcounter = 0;
  }
#endif

  return HAL_OK;
}
