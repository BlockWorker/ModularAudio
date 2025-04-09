/*
 * i2c.c
 *
 *  Created on: Oct 15, 2024
 *      Author: Alex
 */

#include "i2c.h"
#include <stdio.h>
#include <string.h>
#include "inputs.h"
#include "sample_rate_conv.h"
#include "signal_processing.h"


#if I2C_VIRT_BUFFER_SIZE < I2CDEF_DAP_REG_SIZE_SP_FIR || I2C_VIRT_BUFFER_SIZE < I2CDEF_DAP_REG_SIZE_SP_BIQUAD
#error "I2C virtual buffer size is too small to fit FIR or biquad coefficient registers"
#endif

#if I2CDEF_DAP_REG_SIZE_SP_FIR != SP_MAX_FIR_LENGTH * 4 || I2CDEF_DAP_REG_SIZE_SP_BIQUAD != SP_MAX_BIQUADS * 5 * 4
#error "Mismatch between signal processor coefficient lengths and corresponding I2C register sizes"
#endif


typedef enum {
  I2C_UNINIT,
  I2C_IDLE,
  I2C_WAITING_ADDR,
  I2C_ADDR_RECEIVED,
  I2C_WRITE,
  I2C_READ
} _I2C_State;


//internal state of the I2C system
static _I2C_State state = I2C_UNINIT;
//virtual register address to be written/read
static uint8_t reg_addr = 0;
//size of selected virtual register (for reads/writes)
static uint16_t reg_size = 0;
//virtual data buffers (read and write)
static uint8_t read_buf[I2C_VIRT_BUFFER_SIZE] = { 0 };
static uint8_t write_buf[I2C_VIRT_BUFFER_SIZE] = { 0 };

//whether a comm error has been detected since the last status read
static uint8_t i2c_err_detected = 0;

//timeout for non-idle states in main loop cycles
static uint8_t non_idle_timeout = 0;
//sightings of busy peripheral in idle state
static uint8_t idle_busy_count = 0;

//initial interrupt active
static uint8_t init_interrupt_active = 0;

//register size map
static const uint16_t reg_size_map[] = I2CDEF_DAP_REG_SIZES;

//configured state
static uint8_t interrupts_enabled = 0;
static uint8_t interrupt_mask = 0;
static uint8_t interrupt_flags = 0;


void _I2C_UpdateInterruptPin() {
  if (init_interrupt_active || (interrupts_enabled && (interrupt_flags & interrupt_mask) != 0)) {
    HAL_GPIO_WritePin(I2C_INT_PORT, I2C_INT_PIN, GPIO_PIN_RESET);
  } else {
    HAL_GPIO_WritePin(I2C_INT_PORT, I2C_INT_PIN, GPIO_PIN_SET);
  }
}

/**
 * called at the end of a write transaction, processes the received data to update amp state
 */
void _I2C_ProcessWriteData() {
  uint8_t temp8;

  //DEBUG_PRINTF("I2C write trigger: address 0x%02X; size %u; value 0x%08lX (%f)\n", reg_addr, reg_size, *(uint32_t*)write_buf, *(float*)write_buf);

  switch (reg_addr) {
    case I2CDEF_DAP_CONTROL:
      temp8 = (write_buf[0] & I2CDEF_DAP_CONTROL_RESET_Msk) >> I2CDEF_DAP_CONTROL_RESET_Pos;

      if (temp8 != 0) { //check reset code
        if (temp8 == I2CDEF_DAP_CONTROL_RESET_VALUE) { //correct: perform reset
          NVIC_SystemReset();
        } else { //incorrect: report error
          DEBUG_PRINTF("I2C write error: incorrect reset code\n");
          i2c_err_detected = 1;
        }
      }

      interrupts_enabled = (write_buf[0] & I2CDEF_DAP_CONTROL_INT_EN_Msk) != 0 ? 1 : 0; //interrupt state
      _I2C_UpdateInterruptPin();
      break;
    case I2CDEF_DAP_INT_MASK:
      interrupt_mask = write_buf[0];
      _I2C_UpdateInterruptPin();
      break;
    case I2CDEF_DAP_INT_FLAGS:
      interrupt_flags &= write_buf[0]; //clear bits received as 0
      _I2C_UpdateInterruptPin();
      break;
    default:
      DEBUG_PRINTF("I2C write error: attempted write to non-writable register\n");
      i2c_err_detected = 1; //attempting to write to read-only register - report error
      break;
  }
}

/**
 * called at the start of a read transaction, prepares the data to be sent
 */
void _I2C_PrepareReadData() {
  memset(read_buf, 0, I2C_VIRT_BUFFER_SIZE);

  switch (reg_addr) {
    case I2CDEF_DAP_STATUS:
      read_buf[0] =
          (i2c_err_detected != 0 ? I2CDEF_DAP_STATUS_I2CERR_Msk : 0);
      i2c_err_detected = 0; //reset comm error detection after read
      break;
    case I2CDEF_DAP_CONTROL:
      read_buf[0] =
          (interrupts_enabled != 0 ? I2CDEF_DAP_CONTROL_INT_EN_Msk : 0);
      break;
    case I2CDEF_DAP_INT_MASK:
      read_buf[0] = interrupt_mask;
      break;
    case I2CDEF_DAP_INT_FLAGS:
      if (init_interrupt_active) {
        //init interrupt: report no flags (to signal init), then clear init interrupt
        read_buf[0] = 0;
        init_interrupt_active = 0;
        _I2C_UpdateInterruptPin();
      } else {
        read_buf[0] = interrupt_flags;
      }
      break;

    case I2CDEF_DAP_MODULE_ID:
      read_buf[0] = I2CDEF_DAP_MODULE_ID_VALUE;
      break;
  }
}

static __always_inline void _I2C_ErrorReset() {
  i2c_err_detected = 1;
  state = I2C_IDLE;
  HAL_I2C_EnableListen_IT(&I2C_INSTANCE);
}

static __always_inline void _I2C_HardwareReset() {
  __disable_irq();
  //reset peripheral
  HAL_I2C_DeInit(&I2C_INSTANCE);
  I2C_FORCE_RESET();
  int i;
  for (i = 0; i < 50; i++) __NOP();
  I2C_RELEASE_RESET();
  HAL_I2C_Init(&I2C_INSTANCE);
  //enable I2C timeouts
  WRITE_REG(I2C_INSTANCE.Instance->TIMEOUTR, (I2C_SCL_STRETCH_TIMEOUT | I2C_SCL_LOW_TIMEOUT));
  SET_BIT(I2C_INSTANCE.Instance->TIMEOUTR, (I2C_TIMEOUTR_TEXTEN | I2C_TIMEOUTR_TIMOUTEN));
  //clear interrupt pin
  HAL_GPIO_WritePin(I2C_INT_PORT, I2C_INT_PIN, GPIO_PIN_SET);
  //reset loop counters
  idle_busy_count = 0;
  non_idle_timeout = 0;
  __enable_irq();
}


void HAL_I2C_ListenCpltCallback(I2C_HandleTypeDef *hi2c) {
  state = I2C_IDLE;
  HAL_I2C_EnableListen_IT(hi2c);
}

void HAL_I2C_AddrCallback(I2C_HandleTypeDef *hi2c, uint8_t TransferDirection, uint16_t AddrMatchCode) {
  if (TransferDirection == I2C_DIRECTION_TRANSMIT) {
    if (state == I2C_IDLE) { //idle transmission: register address
      state = I2C_WAITING_ADDR;
      non_idle_timeout = I2C_NONIDLE_TIMEOUT;

      HAL_I2C_Slave_Seq_Receive_IT(hi2c, &reg_addr, 1, I2C_FIRST_FRAME);
    } else if (state == I2C_ADDR_RECEIVED) { //write transmission
      state = I2C_WRITE;
      non_idle_timeout = I2C_NONIDLE_TIMEOUT;

      if (HAL_I2C_Slave_Seq_Receive_IT(hi2c, write_buf, reg_size, I2C_NEXT_FRAME) != HAL_OK) {
        DEBUG_PRINTF("I2C Error: Write start fail, address 0x%02X\n", reg_addr);
        _I2C_HardwareReset();
        _I2C_ErrorReset();
      }
    } else {
      DEBUG_PRINTF("I2C Error: Transmit request in invalid state %u\n", state);
      _I2C_HardwareReset();
      _I2C_ErrorReset();
    }
  } else {
    if (state == I2C_ADDR_RECEIVED) { //read transmission
      state = I2C_READ;
      non_idle_timeout = I2C_NONIDLE_TIMEOUT;

      if (HAL_I2C_Slave_Seq_Transmit_IT(hi2c, read_buf, reg_size, I2C_NEXT_FRAME) != HAL_OK) {
        DEBUG_PRINTF("I2C Error: Read start fail, address 0x%02X\n", reg_addr);
        _I2C_HardwareReset();
        _I2C_ErrorReset();
      }
    } else {
      DEBUG_PRINTF("I2C Error: Receive request in invalid state %u\n", state);
      _I2C_HardwareReset();
      _I2C_ErrorReset();
    }
  }
}

void HAL_I2C_SlaveTxCpltCallback(I2C_HandleTypeDef *hi2c) {
  //always allow more reads, to avoid I2C bus freezing

  uint16_t new_size;

  non_idle_timeout = I2C_NONIDLE_TIMEOUT;

  if (reg_addr == 0) { //we're at register 0 (invalid) - always continue as invalid
    new_size = 0;
  } else { //otherwise look at next register to potentially continue with valid read operations
    new_size = reg_size_map[++reg_addr];
  }

  if (new_size > 0) { //register valid: prepare and send its data next, stay in read state
    reg_size = new_size;
    _I2C_PrepareReadData();
  } else { //register invalid: send dummy data
    memset(read_buf, 0, I2C_VIRT_BUFFER_SIZE);
    reg_size = 1;
    reg_addr = 0;
  }

  if (HAL_I2C_Slave_Seq_Transmit_IT(hi2c, read_buf, reg_size, I2C_NEXT_FRAME) != HAL_OK) {
    DEBUG_PRINTF("I2C Error: Sequential read start fail, address 0x%02X\n", reg_addr);
    _I2C_HardwareReset();
    _I2C_ErrorReset();
  }
}

void HAL_I2C_SlaveRxCpltCallback(I2C_HandleTypeDef *hi2c) {
  if (state == I2C_WAITING_ADDR) { //received data is the address
    state = I2C_ADDR_RECEIVED;
    non_idle_timeout = I2C_NONIDLE_TIMEOUT;

    uint16_t size = reg_size_map[reg_addr];
    if (size == 0) { //invalid register address: accept/send dummy bytes
      DEBUG_PRINTF("I2C Error: Attempted access of invalid address 0x%02X\n", reg_addr);
      memset(read_buf, 0, I2C_VIRT_BUFFER_SIZE);
      reg_size = 1;
      reg_addr = 0;
      i2c_err_detected = 1;
    } else { //valid register: set size and prepare potential read data
      reg_size = size;
      _I2C_PrepareReadData();
    }

    if (HAL_I2C_Slave_Seq_Receive_IT(hi2c, write_buf, reg_size, I2C_NEXT_FRAME) != HAL_OK) { //prepare to receive write data immediately
      DEBUG_PRINTF("I2C Error: Write start fail, address 0x%02X\n", reg_addr);
      _I2C_HardwareReset();
      _I2C_ErrorReset();
    }
  } else if (state == I2C_ADDR_RECEIVED || state == I2C_WRITE) { //received data is the data to be written: process it and prepare to accept more data (always to prevent freeze)
    uint16_t new_size;

    state = I2C_WRITE;
    non_idle_timeout = I2C_NONIDLE_TIMEOUT;

    if (reg_addr == 0) { //register invalid - continue as invalid
      new_size = 0;
    } else { //register valid - process data, check if next is valid
      _I2C_ProcessWriteData();
      new_size = reg_size_map[++reg_addr];
    }

    if (new_size > 0) { //next register valid: accept its data next
      reg_size = new_size;
    } else { //next register invalid: accept dummy data and continue as invalid
      reg_size = 1;
      reg_addr = 0;
    }

    if (HAL_I2C_Slave_Seq_Receive_IT(hi2c, write_buf, reg_size, I2C_NEXT_FRAME) != HAL_OK) {
      DEBUG_PRINTF("I2C Error: Sequential write start fail, address 0x%02X\n", reg_addr);
      _I2C_HardwareReset();
      _I2C_ErrorReset();
    }
  } else { //ignore data received in idle or other states
    state = I2C_IDLE;
  }
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c) {
  uint32_t error_code = HAL_I2C_GetError(hi2c);

  //if error is not an expected AF: indicate error
  if (error_code != HAL_I2C_ERROR_AF) {
    DEBUG_PRINTF("I2C Error: HAL error 0x%02lX\n", error_code);
    _I2C_HardwareReset();
    _I2C_ErrorReset();
  }

  //go back to idle state
  state = I2C_IDLE;
}


void I2C_TimeoutISR() {
  DEBUG_PRINTF("I2C Error: Timeout\n");
  _I2C_HardwareReset();
  _I2C_ErrorReset();
}

HAL_StatusTypeDef I2C_Init() {
  //reset interrupt and error state
  interrupts_enabled = 0;
  interrupt_mask = 0;
  interrupt_flags = 0;
  i2c_err_detected = 0;
  non_idle_timeout = 0;
  idle_busy_count = 0;

  //perform hardware reset
  _I2C_HardwareReset();

  //enable I2C listening
  ReturnOnError(HAL_I2C_EnableListen_IT(&I2C_INSTANCE));

  //reset internal state
  state = I2C_IDLE;

  //assert init interrupt (notify master that the controller has been reset - cleared once INT_FLAGS register is accessed)
  init_interrupt_active = 1;
  HAL_GPIO_WritePin(I2C_INT_PORT, I2C_INT_PIN, GPIO_PIN_RESET);

  return HAL_OK;
}

void I2C_TriggerInterrupt(uint8_t interrupt_bit) {
  interrupt_flags |= interrupt_bit; //set flag
  _I2C_UpdateInterruptPin();
}

void I2C_LoopUpdate() {
  if (state == I2C_IDLE && __HAL_I2C_GET_FLAG(&I2C_INSTANCE, I2C_FLAG_BUSY) == SET) { //driver idle but peripheral busy: should be impossible
    if (++idle_busy_count > 1) { //second sighting in a row or more, reset
      DEBUG_PRINTF("I2C Error: Peripheral busy in idle state\n");
      _I2C_HardwareReset();
      _I2C_ErrorReset();
    }
  } else {
    idle_busy_count = 0;
  }

  __disable_irq();
  if (non_idle_timeout > 0) { //handle non-idle state timeouts
    if (state == I2C_IDLE || state == I2C_UNINIT) { //reset timeout if idle
      non_idle_timeout = 0;
    } else {
      if (--non_idle_timeout == 0) { //timed out: error
        __enable_irq();
        DEBUG_PRINTF("I2C Error: Non-idle state timed out\n");
        _I2C_HardwareReset();
        _I2C_ErrorReset();
      }
    }
  }
  __enable_irq();
}
