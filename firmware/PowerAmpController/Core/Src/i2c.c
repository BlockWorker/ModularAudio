/*
 * i2c.c
 *
 *  Created on: Oct 15, 2024
 *      Author: Alex
 */

#include "i2c.h"
#include <stdio.h>
#include "adc.h"
#include "pvdd_control.h"
#include "safety.h"


typedef enum {
  I2C_UNINIT,
  I2C_IDLE,
  I2C_ADDR_RECEIVED,
  I2C_WRITE,
  I2C_READ
} _I2C_State;


//internal state of the I2C system
static _I2C_State state = I2C_UNINIT;
//virtual register address to be written/read
static uint8_t reg_addr = 0;
//size of selected virtual register (for reads/writes)
static uint8_t reg_size = 0;
//virtual data buffers (read and write)
static uint8_t read_buf[I2C_VIRT_BUFFER_SIZE] = { 0 };
static uint8_t write_buf[I2C_VIRT_BUFFER_SIZE] = { 0 };

//register size map
static const uint8_t reg_size_map[] = I2CDEF_POWERAMP_REG_SIZES;


/**
 * called at the end of a write transaction, processes the received data to update amp state
 */
void _I2C_ProcessWriteData() {

}

/**
 * called at the start of a read transaction, prepares the data to be sent
 */
void _I2C_PrepareReadData() {

}


void HAL_I2C_ListenCpltCallback(I2C_HandleTypeDef *hi2c) {
  HAL_I2C_EnableListen_IT(hi2c);
}

void HAL_I2C_AddrCallback(I2C_HandleTypeDef *hi2c, uint8_t TransferDirection, uint16_t AddrMatchCode) {
  if (TransferDirection == I2C_DIRECTION_TRANSMIT) {
    if (state == I2C_IDLE) { //idle transmission: register address
      HAL_I2C_Slave_Seq_Receive_IT(hi2c, &reg_addr, 1, I2C_FIRST_FRAME);
    } else if (state == I2C_ADDR_RECEIVED) { //write transmission
      if (HAL_I2C_Slave_Seq_Receive_IT(hi2c, write_buf, reg_size, I2C_NEXT_FRAME) != HAL_OK) {
	//TODO: indicate error
	state = I2C_IDLE;
      }
      state = I2C_WRITE;
    } else {
      //TODO: indicate error
      state = I2C_IDLE;
    }
  } else {
    if (state == I2C_ADDR_RECEIVED) { //read transmission
      if (HAL_I2C_Slave_Seq_Transmit_IT(hi2c, read_buf, reg_size, I2C_NEXT_FRAME) != HAL_OK) {
	//TODO: indicate error
	state = I2C_IDLE;
      }
      state = I2C_READ;
    } else {
      //TODO: indicate error
      state = I2C_IDLE;
    }
  }
}

void HAL_I2C_SlaveTxCpltCallback(I2C_HandleTypeDef *hi2c) {
  if (reg_addr == 0) { //address 0 is invalid - always go back to idle after it
    state = I2C_IDLE;
    return;
  }

  //look at next register to potentially continue the read operation
  uint8_t new_size = reg_size_map[++reg_addr];

  if (new_size > 0) { //next register valid: prepare and send its data next, stay in read state
    reg_size = new_size;
    _I2C_PrepareReadData();
    if (HAL_I2C_Slave_Seq_Transmit_IT(hi2c, read_buf, reg_size, I2C_NEXT_FRAME) != HAL_OK) {
      //TODO: indicate error
      state = I2C_IDLE;
    }
  } else { //otherwise just go back to idle
    state = I2C_IDLE;
  }
}

void HAL_I2C_SlaveRxCpltCallback(I2C_HandleTypeDef *hi2c) {
  if (state == I2C_IDLE) { //received data is the address
    uint8_t size = reg_size_map[reg_addr];
    if (size == 0) { //invalid register address: accept/send one dummy byte
      read_buf[0] = 0;
      reg_size = 1;
      reg_addr = 0;
      //TODO: indicate error
    } else { //valid register: set size and prepare potential read data
      reg_size = size;
      _I2C_PrepareReadData();
    }
    state = I2C_ADDR_RECEIVED;
  } else if (state == I2C_WRITE) { //received data is the data to be written: process it and prepare to accept more data or go back to idle
    if (reg_addr == 0) { //address 0 is invalid - always go back to idle after it
      state = I2C_IDLE;
      return;
    }

    _I2C_ProcessWriteData();

    uint8_t new_size = reg_size_map[++reg_addr];

    if (new_size > 0) { //next register valid: accept its data next, stay in write state
      reg_size = new_size;
      if (HAL_I2C_Slave_Seq_Receive_IT(hi2c, write_buf, reg_size, I2C_NEXT_FRAME) != HAL_OK) {
      	//TODO: indicate error
      	state = I2C_IDLE;
      }
    } else { //otherwise just go back to idle
      state = I2C_IDLE;
    }
  } else { //shouldn't happen, but go back to idle just in case
    state = I2C_IDLE;
  }
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c) {
  //TODO: indicate error, unless it's just an expected AF
  //go back to idle state
  state = I2C_IDLE;
}


HAL_StatusTypeDef I2C_Init() {
  ReturnOnError(HAL_I2C_EnableListen_IT(&hi2c3));
  state = I2C_IDLE;
  return HAL_OK;
}

void I2C_UpdateStatus() {

}
