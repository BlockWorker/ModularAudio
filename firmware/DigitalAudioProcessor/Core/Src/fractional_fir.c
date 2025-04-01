/*
 * fractional_fir.c
 *
 *  Created on: Mar 31, 2025
 *      Author: Alex
 *
 *  Implements fractional polyphase FIR filtering (sample rate conversion)
 */

#include "fractional_fir.h"


//initialise the given FFIR filter instance - must have values/pointers assigned up to and including filter_state
HAL_StatusTypeDef FFIR_Init(FFIR_Instance* ffir) {
  //check for valid instance struct and parameters
  if (ffir == NULL || ffir->num_phases < 2 || ffir->phase_length < 2 ||
      ffir->phase_step_fract < 0.0f || ffir->phase_step_fract > (float)UINT16_MAX || isnanf(ffir->phase_step_fract) ||
      ffir->coeff_array == NULL || ffir->phase_instances == NULL || ffir->filter_state == NULL) {
    DEBUG_PRINTF("* Attempted to initialise FFIR without setting up the instance struct correctly!\n");
    return HAL_ERROR;
  }

  //set up phase FIR filter instance structs
  int i;
  for (i = 0; i < ffir->num_phases; i++) {
    armext_fir_single_instance_q31* phase = ffir->phase_instances + i;
    phase->numTaps = ffir->phase_length;
    phase->pState = ffir->filter_state;
    phase->pCoeffs = ffir->coeff_array[i];
    phase->pStateOffset = &ffir->filter_state_offset;
  }

  //Reset the filter states
  FFIR_Reset(ffir);

  return HAL_OK;
}

//reset the given FFIR filter instance's internal state
void FFIR_Reset(FFIR_Instance* ffir) {
  //check validity of relevant instance parameters just in case
  if (ffir == NULL || ffir->num_phases < 2 || ffir->phase_length < 2 ||
      ffir->phase_instances == NULL || ffir->filter_state == NULL) {
    DEBUG_PRINTF("* Attempted to reset an invalid FFIR instance!\n");
    return;
  }

  //zero-fill the shared FIR filter state
  memset(ffir->filter_state, 0, 2 * (ffir->phase_length - 1) * sizeof(q31_t));

  //reset phase FIR filter instance state offset
  ffir->filter_state_offset = 0;

  //reset the initial phase to zero
  ffir->_current_phase_int = 0;
  ffir->_current_phase_fract = 0.0f;
}

//process samples through the given filter using the given input/output buffers and step sizes
//processing stops once the given end pointer of either the input or output is reached - whichever happens first
//returns the number of processed samples
uint32_t __RAM_FUNC FFIR_Process(FFIR_Instance* ffir, const q31_t* in_start, const q31_t* in_end, uint16_t in_step, q31_t* out_start, q31_t* out_end, uint16_t out_step) {
  //check for valid struct and phase steps
  if (ffir == NULL || ffir->phase_step_fract < 0.0f || ffir->phase_step_fract > (float)UINT16_MAX || isnanf(ffir->phase_step_fract) ||
      in_start == NULL || in_step < 1 || out_start == NULL || out_step < 1) {
    DEBUG_PRINTF("* Attempted to process FFIR with null struct, invalid phase steps, or invalid parameters!\n");
    return 0;
  }
  //check for empty input or output range
  if (in_end <= in_start || out_end <= out_start) {
    return 0;
  }

  //counter for processed samples
  uint32_t sample_counter = 0;
  //pointers for sample processing
  const q31_t* in_ptr = in_start;
  q31_t* out_ptr;

  //nested function to read input samples and decrement the current phase until it's in [0, num_phases)
  //returns false if end of input is reached, otherwise true
  bool __RAM_FUNC __read_input_until_phase_valid() {
    //if our phase is outside [0, num_phases), "wrap around" to next input sample, repeat until we're in a valid range
    while (ffir->_current_phase_int >= ffir->num_phases) {
      //shift current input sample into the filter state buffer, as we're moving to the next sample
      //(doesn't matter which phase's FIR instance we use here, since the state is shared)
      armext_fir_single_shiftonly_q31(ffir->phase_instances, in_ptr);

      //phase "wraps around" to the start
      ffir->_current_phase_int -= ffir->num_phases;

      //increment input sample pointer
      in_ptr += in_step;
      if (in_ptr >= in_end) {
        //input end reached: we're done here
        return false;
      }
    }

    return true;
  }

  //start by correcting the phase (by reading input samples) if necessary
  if (!__read_input_until_phase_valid()) {
    //ran out of input: return no samples processed
    return 0;
  }

  //loop through samples to process - produce output samples one at a time
  for (out_ptr = out_start; out_ptr < out_end; out_ptr += out_step) {
    //produce one output sample using FIR filter corresponding to current (integer) phase - state buffer remains unmodified for now
    armext_fir_fast_single_noshift_q31(ffir->phase_instances + ffir->_current_phase_int, in_ptr, out_ptr);
    sample_counter++;

    //increment phase - integer step first
    ffir->_current_phase_int += ffir->phase_step_int;
    //check if we're doing fractional phase steps
    if (ffir->phase_step_fract > 0.0f) {
      //fractional step is needed: add step to fractional phase and check if it's still in [0, 1)
      ffir->_current_phase_fract += ffir->phase_step_fract;
      if (ffir->_current_phase_fract >= 1.0f) {
        //fractional phase is above 1: return it to [0, 1) and increase integer phase accordingly
        float int_part;
        ffir->_current_phase_fract = modff(ffir->_current_phase_fract, &int_part);
        ffir->_current_phase_int += (uint16_t)int_part;
      }
    }

    //correct the phase to range [0, num_phases) by reading input samples, if necessary
    if (!__read_input_until_phase_valid()) {
      //ran out of input: stop processing and return
      return sample_counter;
    }
  }

  return sample_counter;
}

