/*
 * arm_math_ext.h
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

#ifndef INC_ARM_MATH_EXT_H_
#define INC_ARM_MATH_EXT_H_

#include "arm_math.h"


//enable this define if (manual) loop unrolling is desired
#undef ARM_MATH_EXT_LOOPUNROLL
//#define ARM_MATH_EXT_LOOPUNROLL


//instance structure for single-sample-optimised algorithms below; like arm_fir_instance_q31 but with an extra state buffer offset field
typedef struct {
  uint16_t numTaps;         /**< number of filter coefficients in the filter. */
  q31_t *pState;            /**< points to the state variable array. The array is of length 2*(numTaps-1). */
  const q31_t *pCoeffs;     /**< points to the coefficient array. The array is of length numTaps. */
  uint32_t stateOffset;     /**< offset of the current state in the state variable array. Initialise to 0. */
} armext_fir_single_instance_q31;


//like arm_fir_fast_q31, but only processes one sample and doesn't shift it into the state buffer, requires numTaps > 1
//implements single-sample-optimised state buffer; for this, the state buffer must be 2*(numTaps-1) long
void armext_fir_fast_single_noshift_q31(const armext_fir_single_instance_q31* S, const q31_t* pSrc, q31_t* pDst);

//shifts one sample into the filter's state buffer (without doing any actual filtering), requires numTaps > 1
//implements single-sample-optimised state buffer; for this, the state buffer must be 2*(numTaps-1) long
void armext_fir_single_shiftonly_q31(armext_fir_single_instance_q31* S, const q31_t* pSrc);


#endif /* INC_ARM_MATH_EXT_H_ */
