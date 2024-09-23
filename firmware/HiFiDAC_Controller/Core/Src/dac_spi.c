/*
 * dac_spi.c
 *
 *  Created on: Sep 21, 2024
 *      Author: Alex
 */

#include "dac_spi.h"
#include <string.h>
#include <stdio.h>

#define DAC_SPI_IS_READWRITE(x) (x >= 0 && x <= 0x8E)
#define DAC_SPI_IS_READONLY(x) (x >= 0xE0 && x <= 0xFB)

/**
 * Write data to DAC register
 */
HAL_StatusTypeDef DAC_SPI_Write8(DAC_SPI_Register reg, uint8_t data) {
  uint8_t spi_data[3] = { 0 };

  uint8_t address = (uint8_t)reg;

  if (!DAC_SPI_IS_READWRITE(address)) return HAL_ERROR;

  spi_data[0] = 0x03; //write command
  spi_data[1] = address;
  spi_data[2] = data;

  HAL_GPIO_WritePin(SPI1_NSS_GPIO_Port, SPI1_NSS_Pin, GPIO_PIN_RESET);
  HAL_StatusTypeDef result = HAL_SPI_Transmit(&hspi1, spi_data, 3, 100);
  HAL_GPIO_WritePin(SPI1_NSS_GPIO_Port, SPI1_NSS_Pin, GPIO_PIN_SET);

  return result;
}

HAL_StatusTypeDef DAC_SPI_Write16(DAC_SPI_Register reg, uint16_t data) {
  ReturnOnError(DAC_SPI_Write8(reg, (uint8_t)(data & 0xFF)));
  return DAC_SPI_Write8(reg + 1, (uint8_t)((data >> 8) & 0xFF));
}

HAL_StatusTypeDef DAC_SPI_Write24(DAC_SPI_Register reg, uint32_t data) {
  ReturnOnError(DAC_SPI_Write8(reg, (uint8_t)(data & 0xFF)));
  ReturnOnError(DAC_SPI_Write8(reg + 1, (uint8_t)((data >> 8) & 0xFF)));
  return DAC_SPI_Write8(reg + 2, (uint8_t)((data >> 16) & 0xFF));
}

/**
 * Write data to DAC register and confirm using read-back
 */
HAL_StatusTypeDef DAC_SPI_WriteConfirm8(DAC_SPI_Register reg, uint8_t data) {
  uint8_t readback = 0;

  ReturnOnError(DAC_SPI_Write8(reg, data));

  ReturnOnError(DAC_SPI_Read8(reg, &readback));

  if (data == readback) return HAL_OK;
  else return HAL_ERROR;
}

HAL_StatusTypeDef DAC_SPI_WriteConfirm16(DAC_SPI_Register reg, uint16_t data) {
  uint16_t readback = 0;

  ReturnOnError(DAC_SPI_Write16(reg, data));

  ReturnOnError(DAC_SPI_Read16(reg, &readback));

  if (data == readback) return HAL_OK;
  else return HAL_ERROR;
}

HAL_StatusTypeDef DAC_SPI_WriteConfirm24(DAC_SPI_Register reg, uint32_t data) {
  uint32_t readback = 0;

  ReturnOnError(DAC_SPI_Write24(reg, data));

  ReturnOnError(DAC_SPI_Read24(reg, &readback));

  if (data == readback) return HAL_OK;
  else return HAL_ERROR;
}

/**
 * Read data from DAC register
 */
HAL_StatusTypeDef DAC_SPI_Read8(DAC_SPI_Register reg, uint8_t* data) {
  uint8_t spi_send_data[3] = { 0 };
  uint8_t spi_receive_data[3] = { 0 };

  uint8_t address = (uint8_t)reg;

  if (!(DAC_SPI_IS_READWRITE(address) || DAC_SPI_IS_READONLY(address))) return HAL_ERROR;

  spi_send_data[0] = 0x01; //read command
  spi_send_data[1] = address;

  HAL_GPIO_WritePin(SPI1_NSS_GPIO_Port, SPI1_NSS_Pin, GPIO_PIN_RESET);
  HAL_StatusTypeDef result = HAL_SPI_TransmitReceive(&hspi1, spi_send_data, spi_receive_data, 3, 100);
  HAL_GPIO_WritePin(SPI1_NSS_GPIO_Port, SPI1_NSS_Pin, GPIO_PIN_SET);

  if (result == HAL_OK) *data = spi_receive_data[2];
  return result;
}

HAL_StatusTypeDef DAC_SPI_Read16(DAC_SPI_Register reg, uint16_t* data) {
  uint8_t read16data[2] = { 0 };

  ReturnOnError(DAC_SPI_Read8(reg, read16data));
  HAL_StatusTypeDef result = DAC_SPI_Read8(reg + 1, read16data + 1);

  if (result == HAL_OK) *data = (uint16_t)read16data[0] | ((uint16_t)read16data[1] << 8);
  return result;
}

HAL_StatusTypeDef DAC_SPI_Read24(DAC_SPI_Register reg, uint32_t* data) {
  uint8_t read24data[3] = { 0 };

  ReturnOnError(DAC_SPI_Read8(reg, read24data));
  ReturnOnError(DAC_SPI_Read8(reg + 1, read24data + 1));
  HAL_StatusTypeDef result = DAC_SPI_Read8(reg + 2, read24data + 2);

  if (result == HAL_OK) *data = (uint32_t)read24data[0] | ((uint32_t)read24data[1] << 8) | ((uint32_t)read24data[2] << 16);
  return result;
}
