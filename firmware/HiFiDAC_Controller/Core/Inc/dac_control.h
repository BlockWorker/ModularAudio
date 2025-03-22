/*
 * dac_control.h
 *
 *  Created on: Sep 22, 2024
 *      Author: Alex
 */

#ifndef INC_DAC_CONTROL_H_
#define INC_DAC_CONTROL_H_

#include "main.h"

#define DAC_EXPECTED_CHIP_ID 0x63

/**
 * Check that the DAC chip ID is correct, confirming that the chip functions and can communicate
 */
HAL_StatusTypeDef DAC_CheckChipID();

/**
 * Initialize the DAC chip with this application's basic settings
 */
HAL_StatusTypeDef DAC_Init();


#endif /* INC_DAC_CONTROL_H_ */
