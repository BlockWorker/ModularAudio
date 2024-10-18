/*
 * i2c.c
 *
 *  Created on: Oct 15, 2024
 *      Author: Alex
 */

#include "i2c.h"
#include <stdio.h>
#include <string.h>
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

//whether a comm error has been detected since the last status read
static uint8_t i2c_err_detected = 0;

//register size map
static const uint8_t reg_size_map[] = I2CDEF_POWERAMP_REG_SIZES;

//configured state
static uint8_t interrupts_enabled = 0;
static uint8_t interrupt_mask = 0;
static uint8_t interrupt_flags = 0;


/**
 * called at the end of a write transaction, processes the received data to update amp state
 */
void _I2C_ProcessWriteData() {
  DEBUG_PRINTF("I2C write trigger: address 0x%02X; size %u; value 0x%08X (%f)\n", reg_addr, reg_size, *(uint32_t*)write_buf, *(float*)write_buf);
}

/**
 * called at the start of a read transaction, prepares the data to be sent
 */
void _I2C_PrepareReadData() {
  memset(read_buf, 0, I2C_VIRT_BUFFER_SIZE);

  if (reg_addr < I2CDEF_POWERAMP_MON_VRMS_FAST_A || reg_addr > I2CDEF_POWERAMP_SWARN_PAPP_SLOW_SUM) { //basic registers
    switch (reg_addr) {
      case I2CDEF_POWERAMP_STATUS:
	read_buf[0] =
	  ((HAL_GPIO_ReadPin(AMP_FAULT_N_GPIO_Port, AMP_FAULT_N_Pin) == GPIO_PIN_RESET ? 1 : 0) << I2CDEF_POWERAMP_STATUS_AMP_FAULT_Pos) |
	  ((HAL_GPIO_ReadPin(AMP_CLIP_OTW_N_GPIO_Port, AMP_CLIP_OTW_N_Pin) == GPIO_PIN_RESET ? 1 : 0) << I2CDEF_POWERAMP_STATUS_AMP_CLIPOTW_Pos) |
	  (is_shutdown << I2CDEF_POWERAMP_STATUS_AMP_SD_Pos) |
	  (pvdd_valid_voltage << I2CDEF_POWERAMP_STATUS_PVDD_VALID_Pos) |
	  ((fabsf(pvdd_voltage_request_offset) > 1e-5f ? 1 : 0) << I2CDEF_POWERAMP_STATUS_PVDD_ONZ_Pos) |
	  ((fabsf(pvdd_voltage_request_offset) >= PVDD_VOLTAGE_OFFSET_MAX ? 1 : 0) << I2CDEF_POWERAMP_STATUS_PVDD_OLIM_Pos) |
	  (((safety_warn_status_inst | safety_warn_status_loop) != 0 ? 1 : 0) << I2CDEF_POWERAMP_STATUS_SWARN_Pos) |
	  (i2c_err_detected << I2CDEF_POWERAMP_STATUS_I2CERR_Pos);
	i2c_err_detected = 0; //reset comm error detection after read
	return;
      case I2CDEF_POWERAMP_CONTROL:
	read_buf[0] =
	  (manual_shutdown << I2CDEF_POWERAMP_CONTROL_AMP_MAN_SD_Pos) |
	  (interrupts_enabled << I2CDEF_POWERAMP_CONTROL_INT_EN_Pos);
	return;
      case I2CDEF_POWERAMP_INT_MASK:
	read_buf[0] = interrupt_mask;
	return;
      case I2CDEF_POWERAMP_INT_FLAGS:
	read_buf[0] = interrupt_flags;
	return;
      case I2CDEF_POWERAMP_PVDD_REQ:
	((float*)read_buf)[0] = pvdd_voltage_request;
	return;
      case I2CDEF_POWERAMP_PVDD_MEASURED:
	((float*)read_buf)[0] = pvdd_voltage_measured;
	return;
      case I2CDEF_POWERAMP_PVDD_OFFSET:
	((float*)read_buf)[0] = pvdd_voltage_request_offset;
	return;
      case I2CDEF_POWERAMP_SAFETY_STATUS:
      	read_buf[0] =
	  (safety_shutdown << I2CDEF_POWERAMP_SAFETY_STATUS_SAFETY_SERR_SD_Pos) |
	  (manual_shutdown << I2CDEF_POWERAMP_SAFETY_STATUS_SAFETY_MAN_SD_Pos);
      	return;
      case I2CDEF_POWERAMP_SERR_SOURCE:
      	read_buf[0] = safety_err_status;
      	return;
      case I2CDEF_POWERAMP_SWARN_SOURCE:
      	read_buf[0] = safety_warn_status_inst | safety_warn_status_loop;
      	return;
      case I2CDEF_POWERAMP_MODULE_ID:
	read_buf[0] = I2CDEF_POWERAMP_MODULE_ID_VALUE;
	return;
    }
  } else { //array registers
    if (reg_addr < I2CDEF_POWERAMP_SERR_IRMS_INST_A) { //ADC/monitor registers: split address into type and index
      uint8_t type = reg_addr & 0xFC;
      uint8_t index = reg_addr & 0x03;
      float* mon_array;

      //select array
      switch (type) {
	case I2CDEF_POWERAMP_MON_VRMS_FAST_A:
	  mon_array = rms_voltage_0s1;
	  break;
	case I2CDEF_POWERAMP_MON_IRMS_FAST_A:
	  mon_array = rms_current_0s1;
	  break;
	case I2CDEF_POWERAMP_MON_PAVG_FAST_A:
	  mon_array = avg_real_power_0s1;
	  break;
	case I2CDEF_POWERAMP_MON_PAPP_FAST_A:
	  mon_array = avg_apparent_power_0s1;
	  break;
	case I2CDEF_POWERAMP_MON_VRMS_SLOW_A:
	  mon_array = rms_voltage_1s0;
	  break;
	case I2CDEF_POWERAMP_MON_IRMS_SLOW_A:
	  mon_array = rms_current_1s0;
	  break;
	case I2CDEF_POWERAMP_MON_PAVG_SLOW_A:
	  mon_array = avg_real_power_1s0;
	  break;
	case I2CDEF_POWERAMP_MON_PAPP_SLOW_A:
	  mon_array = avg_apparent_power_1s0;
	  break;
      }

      //return array value
      ((float*)read_buf)[0] = mon_array[index];
    } else { //safety threshold registers: get array index
      uint8_t index = (reg_addr & 0x0F) % 5;
      float* safety_array;

      //select array by removing index
      switch(reg_addr - index) {
	case I2CDEF_POWERAMP_SERR_IRMS_INST_A:
	  safety_array = safety_max_current_inst;
	  break;
	case I2CDEF_POWERAMP_SERR_IRMS_FAST_A:
	  safety_array = safety_max_current_0s1;
	  break;
	case I2CDEF_POWERAMP_SERR_IRMS_SLOW_A:
	  safety_array = safety_max_current_1s0;
	  break;
	case I2CDEF_POWERAMP_SERR_PAVG_INST_A:
	  safety_array = safety_max_real_power_inst;
	  break;
	case I2CDEF_POWERAMP_SERR_PAVG_FAST_A:
	  safety_array = safety_max_real_power_0s1;
	  break;
	case I2CDEF_POWERAMP_SERR_PAVG_SLOW_A:
	  safety_array = safety_max_real_power_1s0;
	  break;
	case I2CDEF_POWERAMP_SERR_PAPP_INST_A:
	  safety_array = safety_max_apparent_power_inst;
	  break;
	case I2CDEF_POWERAMP_SERR_PAPP_FAST_A:
	  safety_array = safety_max_apparent_power_0s1;
	  break;
	case I2CDEF_POWERAMP_SERR_PAPP_SLOW_A:
	  safety_array = safety_max_apparent_power_1s0;
	  break;
	case I2CDEF_POWERAMP_SWARN_IRMS_INST_A:
	  safety_array = safety_warn_current_inst;
	  break;
	case I2CDEF_POWERAMP_SWARN_IRMS_FAST_A:
	  safety_array = safety_warn_current_0s1;
	  break;
	case I2CDEF_POWERAMP_SWARN_IRMS_SLOW_A:
	  safety_array = safety_warn_current_1s0;
	  break;
	case I2CDEF_POWERAMP_SWARN_PAVG_INST_A:
	  safety_array = safety_warn_real_power_inst;
	  break;
	case I2CDEF_POWERAMP_SWARN_PAVG_FAST_A:
	  safety_array = safety_warn_real_power_0s1;
	  break;
	case I2CDEF_POWERAMP_SWARN_PAVG_SLOW_A:
	  safety_array = safety_warn_real_power_1s0;
	  break;
	case I2CDEF_POWERAMP_SWARN_PAPP_INST_A:
	  safety_array = safety_warn_apparent_power_inst;
	  break;
	case I2CDEF_POWERAMP_SWARN_PAPP_FAST_A:
	  safety_array = safety_warn_apparent_power_0s1;
	  break;
	case I2CDEF_POWERAMP_SWARN_PAPP_SLOW_A:
	  safety_array = safety_warn_apparent_power_1s0;
	  break;
      }

      //return array value
      ((float*)read_buf)[0] = safety_array[index];
    }
  }

  DEBUG_PRINTF("I2C read trigger: address 0x%02X; size %u; value 0x%08X (%f)\n", reg_addr, reg_size, *(uint32_t*)read_buf, *(float*)read_buf);
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
	DEBUG_PRINTF("I2C Error: Write start fail, address 0x%02X\n", reg_addr);
	i2c_err_detected = 1;
	state = I2C_IDLE;
      }
      state = I2C_WRITE;
    } else {
      DEBUG_PRINTF("I2C Error: Transmit request in invalid state %u\n", state);
      i2c_err_detected = 1;
      state = I2C_IDLE;
    }
  } else {
    if (state == I2C_ADDR_RECEIVED) { //read transmission
      if (HAL_I2C_Slave_Seq_Transmit_IT(hi2c, read_buf, reg_size, I2C_NEXT_FRAME) != HAL_OK) {
	DEBUG_PRINTF("I2C Error: Read start fail, address 0x%02X\n", reg_addr);
	i2c_err_detected = 1;
	state = I2C_IDLE;
      }
      state = I2C_READ;
    } else {
      DEBUG_PRINTF("I2C Error: Receive request in invalid state %u\n", state);
      i2c_err_detected = 1;
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
      DEBUG_PRINTF("I2C Error: Sequential read start fail, address 0x%02X\n", reg_addr);
      i2c_err_detected = 1;
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
      DEBUG_PRINTF("I2C Error: Attempted access of invalid address 0x%02X\n", reg_addr);
      reg_addr = 0;
      i2c_err_detected = 1;
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
	DEBUG_PRINTF("I2C Error: Sequential write start fail, address 0x%02X\n", reg_addr);
	i2c_err_detected = 1;
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
  uint32_t error_code = HAL_I2C_GetError(hi2c);

  //if error is not an expected AF: indicate error
  if (error_code != HAL_I2C_ERROR_AF) {
    DEBUG_PRINTF("I2C Error: HAL error 0x%02X\n", error_code);
    i2c_err_detected = 1;
  }

  //go back to idle state
  state = I2C_IDLE;
}


HAL_StatusTypeDef I2C_Init() {
  //reset interrupt and error state
  interrupts_enabled = 0;
  interrupt_mask = 0;
  interrupt_flags = 0;
  HAL_GPIO_WritePin(I2C3_INT_N_GPIO_Port, I2C3_INT_N_Pin, GPIO_PIN_SET);
  i2c_err_detected = 0;

  //enable I2C listening
  ReturnOnError(HAL_I2C_EnableListen_IT(&hi2c3));

  //reset internal state
  state = I2C_IDLE;

  return HAL_OK;
}

void I2C_TriggerInterrupt(uint8_t interrupt_bit) {
  interrupt_flags |= interrupt_bit; //set flag
  if (interrupts_enabled && (interrupt_flags & interrupt_mask) != 0) { //interrupt is enabled: drive interrupt output
    HAL_GPIO_WritePin(I2C3_INT_N_GPIO_Port, I2C3_INT_N_Pin, GPIO_PIN_RESET);
  }
}
