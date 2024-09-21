/*
 * dac_spi.c
 *
 *  Created on: Sep 21, 2024
 *      Author: Alex
 */

#include "dac_spi.h"
#include <string.h>

#define DAC_SPI_IS_READWRITE(x) (x >= 0 && x <= 0x8E)
#define DAC_SPI_IS_READONLY(x) (x >= 0xE0 && x <= 0xFB)

/**
 * Write data to DAC register
 */
HAL_StatusTypeDef DAC_SPI_Write(DAC_SPI_Register reg, uint8_t* data, uint8_t size) {
  uint8_t spi_data[66] = { 0 };

  uint8_t address = (uint8_t)reg;

  if (!DAC_SPI_IS_READWRITE(address) || size > 64) return HAL_ERROR;

  spi_data[0] = 0x03; //write command
  spi_data[1] = address;
  memcpy(spi_data + 2, data, size);

  return HAL_SPI_Transmit(&hspi1, spi_data, size + 2, 100);
}

/**
 * Write data to DAC register and confirm using read-back
 */
HAL_StatusTypeDef DAC_SPI_WriteConfirm(DAC_SPI_Register reg, uint8_t* data, uint8_t size) {
  uint8_t spi_data[66] = { 0 };
  uint8_t readback_data[66] = { 0 };

  uint8_t address = (uint8_t)reg;

  if (!DAC_SPI_IS_READWRITE(address) || size > 64) return HAL_ERROR;

  spi_data[0] = 0x03; //write command
  spi_data[1] = address;
  memcpy(spi_data + 2, data, size);

  HAL_StatusTypeDef result = HAL_SPI_Transmit(&hspi1, spi_data, size + 2, 100);

  if (result != HAL_OK) return result;

  spi_data[0] = 0x01; //read command for confirmation
  memset(spi_data + 2, 0, 64);

  result = HAL_SPI_TransmitReceive(&hspi1, spi_data, readback_data, size + 2, 100);

  if (result == HAL_OK) { //readback successful: check that we received correct data
      if (memcmp(data, readback_data + 2, size) != 0) {
#ifdef DEBUG
	  printf("Readback error on address 0x%x\n", address);
#endif
	  return HAL_ERROR;
      }
  }

  return result;
}

/**
 * Read data from DAC register
 */
HAL_StatusTypeDef DAC_SPI_Read(DAC_SPI_Register reg, uint8_t* data, uint8_t size) {
  uint8_t spi_send_data[66] = { 0 };
  uint8_t spi_receive_data[66] = { 0 };

  uint8_t address = (uint8_t)reg;

  if (!(DAC_SPI_IS_READWRITE(address) || DAC_SPI_IS_READONLY(address)) || size > 64) return HAL_ERROR;

  spi_send_data[0] = 0x01; //read command
  spi_send_data[1] = address;

  HAL_StatusTypeDef result = HAL_SPI_TransmitReceive(&hspi1, spi_send_data, spi_receive_data, size + 2, 100);

  if (result == HAL_OK) memcpy(data, spi_receive_data + 2, size);
  return result;
}

HAL_StatusTypeDef DAC_SPI_Write8(DAC_SPI_Register reg, uint8_t data) {
  uint8_t write8data[1] = { data };

  return DAC_SPI_Write(reg, write8data, 1);
}

HAL_StatusTypeDef DAC_SPI_Write16(DAC_SPI_Register reg, uint16_t data) {
  uint8_t write16data[2] = { data & 0xFF, (data >> 8) & 0xFF };

  return DAC_SPI_Write(reg, write16data, 2);
}

HAL_StatusTypeDef DAC_SPI_Write24(DAC_SPI_Register reg, uint32_t data) {
  uint8_t write24data[3] = { data & 0xFF, (data >> 8) & 0xFF, (data >> 16) & 0xFF };

  return DAC_SPI_Write(reg, write24data, 3);
}

HAL_StatusTypeDef DAC_SPI_WriteConfirm8(DAC_SPI_Register reg, uint8_t data) {
  uint8_t write8data[1] = { data };

  return DAC_SPI_WriteConfirm(reg, write8data, 1);
}

HAL_StatusTypeDef DAC_SPI_WriteConfirm16(DAC_SPI_Register reg, uint16_t data) {
  uint8_t write16data[2] = { data & 0xFF, (data >> 8) & 0xFF };

  return DAC_SPI_WriteConfirm(reg, write16data, 2);
}

HAL_StatusTypeDef DAC_SPI_WriteConfirm24(DAC_SPI_Register reg, uint32_t data) {
  uint8_t write24data[3] = { data & 0xFF, (data >> 8) & 0xFF, (data >> 16) & 0xFF };

  return DAC_SPI_Write(reg, write24data, 3);
}

HAL_StatusTypeDef DAC_SPI_Read8(DAC_SPI_Register reg, uint8_t* data) {
  uint8_t read8data[1] = { 0 };

  HAL_StatusTypeDef result = DAC_SPI_Read(reg, read8data, 1);

  if (result == HAL_OK) *data = read8data[0];
  return result;
}

HAL_StatusTypeDef DAC_SPI_Read16(DAC_SPI_Register reg, uint16_t* data) {
  uint8_t read16data[2] = { 0 };

  HAL_StatusTypeDef result = DAC_SPI_Read(reg, read16data, 2);

  if (result == HAL_OK) *data = (uint16_t)read16data[0] | ((uint16_t)read16data[1] << 8);
  return result;
}

HAL_StatusTypeDef DAC_SPI_Read24(DAC_SPI_Register reg, uint32_t* data) {
  uint8_t read24data[3] = { 0 };

  HAL_StatusTypeDef result = DAC_SPI_Read(reg, read24data, 3);

  if (result == HAL_OK) *data = (uint32_t)read24data[0] | ((uint32_t)read24data[1] << 8) | ((uint32_t)read24data[2] << 16);
  return result;
}
