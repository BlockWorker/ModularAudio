/*
 * fractional_fir.h
 *
 *  Created on: Mar 31, 2025
 *      Author: Alex
 *
 *  Implements fractional polyphase FIR filtering (sample rate conversion)
 */

#ifndef INC_FRACTIONAL_FIR_H_
#define INC_FRACTIONAL_FIR_H_

#include "main.h"
#include "arm_math_ext.h"


typedef struct {
  uint16_t num_phases;                              //number of phases in the filter (= interpolation factor)
  uint16_t phase_length;                            //length of phases (number of taps/coefficients per phase)
  uint16_t phase_step_int;                          //phase step per output sample (= decimation factor), integer - may be modified between processing calls
  float phase_step_fract;                           //phase step per output sample (= decimation factor), fractional - may be modified between processing calls

  const q31_t* coeff_array;                         //row-major array of `num_phases` x `phase_length` coefficient values as rows of phase coefficients - rows must be in reverse order

  armext_fir_single_instance_q31* phase_instances;  //array of `num_phases` instance structures of the individual phase FIR filters - contents will be initialised by the FFIR_Init function
  q31_t* filter_state;                              //filter state array of length 2*(`phase_length`-1), shared by all phase FIR filters - contents will be initialised by the FFIR_Init function

  uint32_t filter_state_offset;                     //filter state array offset - will be initialised by the FFIR_Init function, do not modify externally
  uint32_t _current_phase_int;                      //current phase index, integer part - will be initialised by the FFIR_Init function, do not modify externally
  float _current_phase_fract;                       //current phase index, fractional part in [0, 1) - will be initialised by the FFIR_Init function, do not modify externally
} FFIR_Instance;


//initialise the given FFIR filter instance - must have values/pointers assigned up to and including filter_state
HAL_StatusTypeDef FFIR_Init(FFIR_Instance* ffir);
//reset the given FFIR filter instance's internal state
void FFIR_Reset(FFIR_Instance* ffir);

//process samples through the given filter using the given input/output buffers and step sizes
//processing stops once the given end pointer of either the input or output is reached - whichever happens first
//returns the number of processed samples; as well as optionally the number of used input samples, in `in_count_p`
uint32_t FFIR_Process(FFIR_Instance* ffir, const q31_t* in_start, const q31_t* in_end, uint16_t in_step, q31_t* out_start, q31_t* out_end, uint16_t out_step, uint32_t* in_count_p);


#endif /* INC_FRACTIONAL_FIR_H_ */
