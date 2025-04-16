/*
 * spdif.h
 *
 *  Created on: Apr 11, 2025
 *      Author: Alex
 *
 *  Handles SPDIF data reception.
 */

#ifndef INC_SPDIF_H_
#define INC_SPDIF_H_

#include "main.h"


//maximum acceptable relative sample rate error (from 44.1K/48K/96K)
#define SPDIF_MAX_SAMPLE_RATE_ERROR 0.01f


//initialise SPDIF reception - only needs to be called once
HAL_StatusTypeDef SPDIF_Init();




#endif /* INC_SPDIF_H_ */
