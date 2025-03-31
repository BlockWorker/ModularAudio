/*
 * arm_math_ext.c
 *
 *  Created on: Mar 31, 2025
 *      Author: Alex
 *
 *  CMSIS-DSP extensions
 *  Based on the original ARM CMSIS DSP code, copyright ARM Ltd, see license below.
 */
/*
 * Copyright (C) 2010-2021 ARM Limited or its affiliates. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "arm_math_ext.h"
#include "main.h"


//like arm_fir_fast_q31, but only processes one sample and doesn't shift it into the state buffer, requires numTaps > 1
//implements single-sample-optimised state buffer; for this, the state buffer must be 2*(numTaps-1) long
void __RAM_FUNC armext_fir_fast_single_noshift_q31(const armext_fir_single_instance_q31* S, const q31_t* pSrc, q31_t* pDst) {
  q31_t* pState = S->pState + S->stateOffset;     /* State pointer */
  const q31_t* pCoeffs = S->pCoeffs;              /* Coefficient pointer */
  q31_t acc0;                                     /* Accumulator */
  uint32_t numTaps = S->numTaps;                  /* Number of filter coefficients in the filter */

#if defined(ARM_MATH_EXT_LOOPUNROLL)
  uint32_t tapCnt;                                /* Auxiliary loop counter */
#endif

  /* Set the accumulator to zero */
  acc0 = 0;

#if defined(ARM_MATH_EXT_LOOPUNROLL)

  /* Loop unrolling. Process 4 taps at a time. */
  tapCnt = (numTaps - 1) >> 2U;

  /* Loop over the number of taps.  Unroll by a factor of 4.
     Repeat until we've computed at most numTaps-1 coefficients. */
  while (tapCnt > 0U) {
    multAcc_32x32_keep32_R(acc0, (*pState++), (*pCoeffs++));
    multAcc_32x32_keep32_R(acc0, (*pState++), (*pCoeffs++));
    multAcc_32x32_keep32_R(acc0, (*pState++), (*pCoeffs++));
    multAcc_32x32_keep32_R(acc0, (*pState++), (*pCoeffs++));
    tapCnt--;
  }

  /* Compute the remaining filter taps normally (up to 4: 3 past and 1 new) */
  numTaps = (numTaps - 1) % 0x4U + 1U;

#endif

  /* Perform the multiply-accumulates with past samples (i.e. all but last) */
  do {
    multAcc_32x32_keep32_R(acc0, (*pState++), (*pCoeffs++));
    numTaps--;
  } while (numTaps > 1U);

  /* Perform the last multiply-accumulate with the new sample */
  multAcc_32x32_keep32_R(acc0, (*pSrc), (*pCoeffs));

  /* The result is in 2.30 format. Convert to 1.31
     Then store the output in the destination buffer. */
  *pDst = (q31_t)(acc0 << 1);
}

//shifts one sample into the filter's state buffer (without doing any actual filtering), requires numTaps > 1
//implements single-sample-optimised state buffer; for this, the state buffer must be 2*(numTaps-1) long
void __RAM_FUNC armext_fir_single_shiftonly_q31(armext_fir_single_instance_q31* S, const q31_t* pSrc) {
  //check whether we've reached the end of the state buffer yet
  if (S->stateOffset >= (uint32_t)(S->numTaps - 1)) {
    //end reached: copy last numTaps-2 samples (located at end of buffer) to start of state buffer
    memcpy(S->pState, S->pState + S->numTaps, (S->numTaps - 2) * sizeof(q31_t));
    //reset the state offset
    S->stateOffset = 0;
  } else {
    //end not reached yet: just increment the state offset
    S->stateOffset++;
  }

  //insert new sample at the end of the new-offset state
  S->pState[S->stateOffset + S->numTaps - 2] = *pSrc;
}
