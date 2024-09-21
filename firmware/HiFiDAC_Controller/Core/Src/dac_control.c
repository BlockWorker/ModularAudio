/*
 * dac_control.c
 *
 *  Created on: Sep 22, 2024
 *      Author: Alex
 */

#include "dac_control.h"
#include "dac_spi.h"

/**
 * Check that the DAC chip ID is correct, confirming that the chip functions and can communicate
 */
HAL_StatusTypeDef DAC_CheckChipID() {
  uint8_t chip_id = 0;

  HAL_StatusTypeDef res = DAC_SPI_Read8(REG_CHIP_ID_READ, &chip_id);

#ifdef DEBUG
  printf("Chip ID: 0x%x\n", chip_id);
#endif

  if (res != HAL_OK) return res;

  if (chip_id != DAC_EXPECTED_CHIP_ID) {
#ifdef DEBUG
      printf("ERROR: INCORRECT DAC CHIP ID\n");
#endif

      return HAL_ERROR;
  }

  return HAL_OK;
}

/**
 * Initialize the DAC chip with this application's basic settings
 */
HAL_StatusTypeDef DAC_Initialize() {

}
