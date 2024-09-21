/*
 * dac_spi.h
 *
 *  Created on: Sep 21, 2024
 *      Author: Alex
 */

#ifndef INC_DAC_SPI_H_
#define INC_DAC_SPI_H_

#include "main.h"


typedef enum {
  REG_SYSTEM_CONFIG = 0x00,
  REG_SYS_MODE_CONFIG = 0x01,
  REG_DAC_CLOCK_CONFIG = 0x03,
  REG_CLOCK_CONFIG = 0x04,
  REG_CLK_GEAR_SELECT = 0x05,
  REG_INTERRUPT_MASKP = 0x0A,
  REG_INTERRUPT_MASKN = 0x0F,
  REG_INTERRUPT_CLEAR = 0x14,
  REG_DPLL_BW = 0x1D,
  REG_DATA_PATH_CONFIG = 0x22,
  REG_PCM_4X_GAIN = 0x23,
  REG_GPIO12_CONFIG = 0x25,
  REG_GPIO34_CONFIG = 0x26,
  REG_GPIO56_CONFIG = 0x27,
  REG_GPIO78_CONFIG = 0x28,
  REG_GPIO_OUTPUT_ENABLE = 0x29,
  REG_GPIO_INPUT = 0x2A,
  REG_GPIO_WK_EN = 0x2B,
  REG_INVERT_GPIO = 0x2C,
  REG_GPIO_READ_EN = 0x2D,
  REG_GPIO_OUTPUT_LOGIC = 0x2E,
  REG_PWM1_COUNT = 0x30,
  REG_PWM1_FREQUENCY = 0x31,
  REG_PWM2_COUNT = 0x33,
  REG_PWM2_FREQUENCY = 0x34,
  REG_PWM3_COUNT = 0x36,
  REG_PWM3_FREQUENCY = 0x37,
  REG_INPUT_SELECTION = 0x39,
  REG_MASTER_ENCODER_CONFIG = 0x3A,
  REG_TDM_CONFIG = 0x3B,
  REG_TDM_CONFIG1 = 0x3C,
  REG_TDM_CONFIG2 = 0x3D,
  REG_BCKWS_MONITOR_CONFIG = 0x3E,
  REG_CH1_SLOT_CONFIG = 0x40,
  REG_CH2_SLOT_CONFIG = 0x41,
  REG_VOLUME_CH1 = 0x4A,
  REG_VOLUME_CH2 = 0x4B,
  REG_DAC_VOL_UP_RATE = 0x52,
  REG_DAC_VOL_DOWN_RATE = 0x53,
  REG_DAC_VOL_DOWN_RATE_FAST = 0x54,
  REG_DAC_MUTE = 0x56,
  REG_DAC_INVERT = 0x57,
  REG_FILTER_SHAPE = 0x58,
  REG_IIR_BANDWIDTH_SPDIF_SEL = 0x59,
  REG_DAC_PATH_CONFIG = 0x5A,
  REG_THD_C2_CH1 = 0x5B,
  REG_THD_C2_CH2 = 0x5D,
  REG_THD_C3_CH1 = 0x6B,
  REG_THD_C3_CH2 = 0x6D,
  REG_AUTOMUTE_ENABLE = 0x7B,
  REG_AUTOMUTE_TIME = 0x7C,
  REG_AUTOMUTE_LEVEL = 0x7E,
  REG_AUTOMUTE_OFF_LEVEL = 0x80,
  REG_SOFT_RAMP_CONFIG = 0x82,
  REG_PROGRAM_RAM_CONTROL = 0x87,
  REG_SPDIF_READ_CONTROL = 0x88,
  REG_PROGRAM_RAM_ADDRESS = 0x89,
  REG_PROGRAM_RAM_DATA = 0x8A,

  REG_CHIP_ID_READ = 0xE1,
  REG_INTERRUPT_STATES = 0xE5,
  REG_INTERRUPT_SOURCES = 0xEA,
  REG_RATIO_VALID_READ = 0xEF,
  REG_GPIO_READ = 0xF0,
  REG_VOL_MIN_READ = 0xF1,
  REG_AUTOMUTE_READ = 0xF2,
  REG_SOFT_RAMP_UP_READ = 0xF3,
  REG_SOFT_RAMP_DOWN_READ = 0xF4,
  REG_INPUT_STREAM_READBACK = 0xF5,
  REG_PROG_COEFF_OUT_READ = 0xF6,
  REG_SPDIF_DATA_READ = 0xFB
} DAC_SPI_Register;

/**
 * Write data to DAC register
 */
HAL_StatusTypeDef DAC_SPI_Write(DAC_SPI_Register reg, uint8_t* data, uint8_t size);
/**
 * Write data to DAC register and confirm using read-back
 */
HAL_StatusTypeDef DAC_SPI_WriteConfirm(DAC_SPI_Register reg, uint8_t* data, uint8_t size);
/**
 * Read data from DAC register
 */
HAL_StatusTypeDef DAC_SPI_Read(DAC_SPI_Register reg, uint8_t* data, uint8_t size);

HAL_StatusTypeDef DAC_SPI_Write8(DAC_SPI_Register reg, uint8_t data);
HAL_StatusTypeDef DAC_SPI_Write16(DAC_SPI_Register reg, uint16_t data);
HAL_StatusTypeDef DAC_SPI_Write24(DAC_SPI_Register reg, uint32_t data);

HAL_StatusTypeDef DAC_SPI_WriteConfirm8(DAC_SPI_Register reg, uint8_t data);
HAL_StatusTypeDef DAC_SPI_WriteConfirm16(DAC_SPI_Register reg, uint16_t data);
HAL_StatusTypeDef DAC_SPI_WriteConfirm24(DAC_SPI_Register reg, uint32_t data);

HAL_StatusTypeDef DAC_SPI_Read8(DAC_SPI_Register reg, uint8_t* data);
HAL_StatusTypeDef DAC_SPI_Read16(DAC_SPI_Register reg, uint16_t* data);
HAL_StatusTypeDef DAC_SPI_Read24(DAC_SPI_Register reg, uint32_t* data);

#endif /* INC_DAC_SPI_H_ */
