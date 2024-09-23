/*
 * dac_control.c
 *
 *  Created on: Sep 22, 2024
 *      Author: Alex
 */

#include "dac_control.h"
#include "dac_spi.h"
#include <stdio.h>

/**
 * Check that the DAC chip ID is correct, confirming that the chip functions and can communicate
 */
HAL_StatusTypeDef DAC_CheckChipID() {
  uint8_t chip_id = 0;

  ReturnOnError(DAC_SPI_Read8(REG_CHIP_ID_READ, &chip_id));

#ifdef DEBUG
  printf("Chip ID: 0x%02X\n", chip_id);
#endif

  if (chip_id != DAC_EXPECTED_CHIP_ID) return HAL_ERROR;

  return HAL_OK;
}

#ifdef DEBUG
void PrintReg8(DAC_SPI_Register reg) {
  uint8_t i8 = 0;
  HAL_StatusTypeDef res = DAC_SPI_Read8(reg, &i8);
  if (res != HAL_OK) printf("0x%02X: Read error %u\n", (uint8_t)reg, (uint8_t)res);
  else printf("0x%02X: 0x%02X\n", (uint8_t)reg, i8);
}

void PrintReg16(DAC_SPI_Register reg) {
  uint16_t i16 = 0;
  HAL_StatusTypeDef res = DAC_SPI_Read16(reg, &i16);
  if (res != HAL_OK) printf("0x%02X: Read error %u\n", (uint8_t)reg, (uint8_t)res);
  else printf("0x%02X: 0x%04X\n", (uint8_t)reg, i16);
}

void PrintReg24(DAC_SPI_Register reg) {
  uint32_t i32 = 0;
  HAL_StatusTypeDef res = DAC_SPI_Read24(reg, &i32);
  if (res != HAL_OK) printf("0x%02X: Read error %u\n", (uint8_t)reg, (uint8_t)res);
  else printf("0x%02X: 0x%06lX\n", (uint8_t)reg, i32);
}

void PrintAllRegisters() {
  PrintReg8(REG_SYSTEM_CONFIG);
  PrintReg8(REG_SYS_MODE_CONFIG);
  PrintReg8(REG_DAC_CLOCK_CONFIG);
  PrintReg8(REG_CLOCK_CONFIG);
  PrintReg8(REG_CLK_GEAR_SELECT);
  PrintReg16(REG_INTERRUPT_MASKP);
  PrintReg16(REG_INTERRUPT_MASKN);
  PrintReg16(REG_INTERRUPT_CLEAR);
  PrintReg8(REG_DPLL_BW);
  PrintReg8(REG_DATA_PATH_CONFIG);
  PrintReg8(REG_PCM_4X_GAIN);
  PrintReg8(REG_GPIO12_CONFIG);
  PrintReg8(REG_GPIO34_CONFIG);
  PrintReg8(REG_GPIO56_CONFIG);
  PrintReg8(REG_GPIO78_CONFIG);
  PrintReg8(REG_GPIO_OUTPUT_ENABLE);
  PrintReg8(REG_GPIO_INPUT);
  PrintReg8(REG_GPIO_WK_EN);
  PrintReg8(REG_INVERT_GPIO);
  PrintReg8(REG_GPIO_READ_EN);
  PrintReg16(REG_GPIO_OUTPUT_LOGIC);
  PrintReg8(REG_PWM1_COUNT);
  PrintReg16(REG_PWM1_FREQUENCY);
  PrintReg8(REG_PWM2_COUNT);
  PrintReg16(REG_PWM2_FREQUENCY);
  PrintReg8(REG_PWM3_COUNT);
  PrintReg16(REG_PWM3_FREQUENCY);
  PrintReg8(REG_INPUT_SELECTION);
  PrintReg8(REG_MASTER_ENCODER_CONFIG);
  PrintReg8(REG_TDM_CONFIG);
  PrintReg8(REG_TDM_CONFIG1);
  PrintReg8(REG_TDM_CONFIG2);
  PrintReg8(REG_BCKWS_MONITOR_CONFIG);
  PrintReg8(REG_CH1_SLOT_CONFIG);
  PrintReg8(REG_CH2_SLOT_CONFIG);
  PrintReg8(REG_VOLUME_CH1);
  PrintReg8(REG_VOLUME_CH2);
  PrintReg8(REG_DAC_VOL_UP_RATE);
  PrintReg8(REG_DAC_VOL_DOWN_RATE);
  PrintReg8(REG_DAC_VOL_DOWN_RATE_FAST);
  PrintReg8(REG_DAC_MUTE);
  PrintReg8(REG_DAC_INVERT);
  PrintReg8(REG_FILTER_SHAPE);
  PrintReg8(REG_IIR_BANDWIDTH_SPDIF_SEL);
  PrintReg8(REG_DAC_PATH_CONFIG);
  PrintReg16(REG_THD_C2_CH1);
  PrintReg16(REG_THD_C2_CH2);
  PrintReg16(REG_THD_C3_CH1);
  PrintReg16(REG_THD_C3_CH2);
  PrintReg8(REG_AUTOMUTE_ENABLE);
  PrintReg16(REG_AUTOMUTE_TIME);
  PrintReg16(REG_AUTOMUTE_LEVEL);
  PrintReg16(REG_AUTOMUTE_OFF_LEVEL);
  PrintReg8(REG_SOFT_RAMP_CONFIG);
  PrintReg8(REG_PROGRAM_RAM_CONTROL);
  PrintReg8(REG_SPDIF_READ_CONTROL);
  PrintReg8(REG_PROGRAM_RAM_ADDRESS);
  PrintReg24(REG_PROGRAM_RAM_DATA);
  PrintReg8(REG_CHIP_ID_READ);
  PrintReg16(REG_INTERRUPT_STATES);
  PrintReg16(REG_INTERRUPT_SOURCES);
  PrintReg8(REG_RATIO_VALID_READ);
  PrintReg8(REG_GPIO_READ);
  PrintReg8(REG_VOL_MIN_READ);
  PrintReg8(REG_AUTOMUTE_READ);
  PrintReg8(REG_SOFT_RAMP_UP_READ);
  PrintReg8(REG_SOFT_RAMP_DOWN_READ);
  PrintReg8(REG_INPUT_STREAM_READBACK);
  PrintReg24(REG_PROG_COEFF_OUT_READ);
  PrintReg8(REG_SPDIF_DATA_READ);
}
#endif

/**
 * Initialize the DAC chip with this application's basic settings
 */
HAL_StatusTypeDef DAC_Initialize() {

  ReturnOnError(DAC_SPI_Write8(REG_SYSTEM_CONFIG, 0x80)); //perform soft reset

  HAL_Delay(10);

  //configuration for ASYNC mode I2C slave at 96kHz for testing
  ReturnOnError(DAC_SPI_WriteConfirm8(REG_DAC_CLOCK_CONFIG, 0x80)); //keep auto detection on (shouldn't matter), set CLK_IDAC = SYS_CLK
  ReturnOnError(DAC_SPI_WriteConfirm8(REG_VOLUME_CH1, 0x04)); //write initial channel volume - modify for calibration
  ReturnOnError(DAC_SPI_WriteConfirm8(REG_VOLUME_CH2, 0x04)); //testing revealed that the prototype needs -2dB to produce 9Vpeak

  HAL_Delay(10);

  ReturnOnError(DAC_SPI_WriteConfirm8(REG_SYSTEM_CONFIG, 0x02)); //enable DAC analog section

#ifdef DEBUG
  PrintAllRegisters();
#endif

  return HAL_OK;
}
