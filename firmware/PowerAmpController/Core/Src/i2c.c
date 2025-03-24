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
static uint8_t reg_size = 0;
//virtual data buffers (read and write)
static uint8_t read_buf[I2C_VIRT_BUFFER_SIZE] = { 0 };
static uint8_t write_buf[I2C_VIRT_BUFFER_SIZE] = { 0 };

//dummy default limit buffer
static const float i2c_safety_no_warn[] = SAFETY_NO_WARN;

//whether a comm error has been detected since the last status read
static uint8_t i2c_err_detected = 0;

//timeout for non-idle states in main loop cycles
static uint8_t non_idle_timeout = 0;
//sightings of busy peripheral in idle state
static uint8_t idle_busy_count = 0;

//initial interrupt active
static uint8_t init_interrupt_active = 0;

//register size map
static const uint8_t reg_size_map[] = I2CDEF_POWERAMP_REG_SIZES;

//configured state
static uint8_t interrupts_enabled = 0;
static uint8_t interrupt_mask = 0;
static uint8_t interrupt_flags = 0;


void _I2C_UpdateInterruptPin() {
  if (interrupts_enabled && (interrupt_flags & interrupt_mask) != 0) {
    HAL_GPIO_WritePin(I2C3_INT_N_GPIO_Port, I2C3_INT_N_Pin, GPIO_PIN_RESET);
  } else {
    HAL_GPIO_WritePin(I2C3_INT_N_GPIO_Port, I2C3_INT_N_Pin, GPIO_PIN_SET);
  }
}

/**
 * called at the end of a write transaction, processes the received data to update amp state
 */
void _I2C_ProcessWriteData() {
  //temp holding variables
  uint8_t tb1, tb2, tb3;
  float tf1;

  //DEBUG_PRINTF("I2C write trigger: address 0x%02X; size %u; value 0x%08lX (%f)\n", reg_addr, reg_size, *(uint32_t*)write_buf, *(float*)write_buf);

  if (reg_addr < I2CDEF_POWERAMP_SERR_IRMS_INST_A || reg_addr > I2CDEF_POWERAMP_SWARN_PAPP_SLOW_SUM) { //basic registers
    switch (reg_addr) {
      case I2CDEF_POWERAMP_CONTROL:
        tb1 = (write_buf[0] & I2CDEF_POWERAMP_CONTROL_AMP_MAN_SD_Msk) > 0 ? 1 : 0;
        tb2 = (write_buf[0] & I2CDEF_POWERAMP_CONTROL_INT_EN_Msk) > 0 ? 1 : 0;
        tb3 = (write_buf[0] & I2CDEF_POWERAMP_CONTROL_RESET_Msk) >> I2CDEF_POWERAMP_CONTROL_RESET_Pos;

        if (tb3 != 0) { //check reset code
          if (tb3 == I2CDEF_POWERAMP_CONTROL_RESET_VALUE) { //correct: perform reset
            NVIC_SystemReset();
          } else { //incorrect: report error
            DEBUG_PRINTF("I2C write error: incorrect reset code\n");
            i2c_err_detected = 1;
          }
        }

        interrupts_enabled = tb2; //interrupt state
        _I2C_UpdateInterruptPin();

        SAFETY_SetManualShutdown(tb1); //amp shutdown
        break;
      case I2CDEF_POWERAMP_INT_MASK:
        interrupt_mask = write_buf[0];
        _I2C_UpdateInterruptPin();
        break;
      case I2CDEF_POWERAMP_INT_FLAGS:
        interrupt_flags &= write_buf[0]; //clear bits received as 0
        _I2C_UpdateInterruptPin();
        break;
      case I2CDEF_POWERAMP_PVDD_TARGET:
        tf1 = *(float*)write_buf;
        if (PVDD_SetTargetVoltage(tf1) != HAL_OK) { //voltage update fail is reported as an error (e.g., caused by invalid voltage value)
          DEBUG_PRINTF("I2C write error: PVDD voltage target change failed\n");
          i2c_err_detected = 1;
        }
        break;
      default:
        DEBUG_PRINTF("I2C write error: attempted write to non-writable register\n");
        i2c_err_detected = 1; //attempting to write to read-only register - report error
        break;
    }
  } else { //safety threshold registers, only writable during manual amp shutdown
    if (manual_shutdown != 1) { //attempting to write thresholds outside of amp shutdown - report error
      DEBUG_PRINTF("I2C write error: attempted safety threshold write while not in shutdown\n");
      i2c_err_detected = 1;
    } else { //we are in shutdown: proceed
      tf1 = *(float*)write_buf;

      if (tf1 <= 0.0f || isnanf(tf1)) { //threshold value too low or otherwise invalid - error
        DEBUG_PRINTF("I2C write error: invalid safety threshold value\n");
        i2c_err_detected = 1;
      } else { //proceed: get array index
        uint8_t index = (reg_addr & 0x0F) % 5;
        float* target_array;
        const float* limit_array = i2c_safety_no_warn; //maximum limits for target array: default to warning limits (i.e. no limits)

        //select arrays by removing index
        switch(reg_addr - index) {
          case I2CDEF_POWERAMP_SERR_IRMS_INST_A:
            target_array = safety_max_current_inst;
            limit_array = safety_limit_current_inst;
            break;
          case I2CDEF_POWERAMP_SERR_IRMS_FAST_A:
            target_array = safety_max_current_0s1;
            limit_array = safety_limit_current_0s1;
            break;
          case I2CDEF_POWERAMP_SERR_IRMS_SLOW_A:
            target_array = safety_max_current_1s0;
            limit_array = safety_limit_current_1s0;
            break;
          case I2CDEF_POWERAMP_SERR_PAVG_INST_A:
            target_array = safety_max_real_power_inst;
            limit_array = safety_limit_real_power_inst;
            break;
          case I2CDEF_POWERAMP_SERR_PAVG_FAST_A:
            target_array = safety_max_real_power_0s1;
            limit_array = safety_limit_real_power_0s1;
            break;
          case I2CDEF_POWERAMP_SERR_PAVG_SLOW_A:
            target_array = safety_max_real_power_1s0;
            limit_array = safety_limit_real_power_1s0;
            break;
          case I2CDEF_POWERAMP_SERR_PAPP_INST_A:
            target_array = safety_max_apparent_power_inst;
            limit_array = safety_limit_apparent_power_inst;
            break;
          case I2CDEF_POWERAMP_SERR_PAPP_FAST_A:
            target_array = safety_max_apparent_power_0s1;
            limit_array = safety_limit_apparent_power_0s1;
            break;
          case I2CDEF_POWERAMP_SERR_PAPP_SLOW_A:
            target_array = safety_max_apparent_power_1s0;
            limit_array = safety_limit_apparent_power_1s0;
            break;
          case I2CDEF_POWERAMP_SWARN_IRMS_INST_A:
            target_array = safety_warn_current_inst;
            break;
          case I2CDEF_POWERAMP_SWARN_IRMS_FAST_A:
            target_array = safety_warn_current_0s1;
            break;
          case I2CDEF_POWERAMP_SWARN_IRMS_SLOW_A:
            target_array = safety_warn_current_1s0;
            break;
          case I2CDEF_POWERAMP_SWARN_PAVG_INST_A:
            target_array = safety_warn_real_power_inst;
            break;
          case I2CDEF_POWERAMP_SWARN_PAVG_FAST_A:
            target_array = safety_warn_real_power_0s1;
            break;
          case I2CDEF_POWERAMP_SWARN_PAVG_SLOW_A:
            target_array = safety_warn_real_power_1s0;
            break;
          case I2CDEF_POWERAMP_SWARN_PAPP_INST_A:
            target_array = safety_warn_apparent_power_inst;
            break;
          case I2CDEF_POWERAMP_SWARN_PAPP_FAST_A:
            target_array = safety_warn_apparent_power_0s1;
            break;
          case I2CDEF_POWERAMP_SWARN_PAPP_SLOW_A:
            target_array = safety_warn_apparent_power_1s0;
            break;
          default:
            i2c_err_detected = 1; //should never happen
            return;
        }

        if (tf1 > limit_array[index]) { //limit exceeded: error
          DEBUG_PRINTF("I2C write error: safety threshold value not acceptable\n");
          i2c_err_detected = 1;
        } else { //acceptable value: write to target array
          target_array[index] = tf1;
        }
      }
    }
  }
}

/**
 * called at the start of a read transaction, prepares the data to be sent
 */
void _I2C_PrepareReadData() {
  memset(read_buf, 0, I2C_VIRT_BUFFER_SIZE);

  if (reg_addr < I2CDEF_POWERAMP_MON_VRMS_FAST_A || reg_addr > I2CDEF_POWERAMP_SWARN_PAPP_SLOW_SUM) { //basic registers
    switch (reg_addr) {
      case I2CDEF_POWERAMP_STATUS:
        ((uint16_t*)read_buf)[0] =
            ((HAL_GPIO_ReadPin(AMP_FAULT_N_GPIO_Port, AMP_FAULT_N_Pin) == GPIO_PIN_RESET ? 1 : 0) << I2CDEF_POWERAMP_STATUS_AMP_FAULT_Pos) |
            ((HAL_GPIO_ReadPin(AMP_CLIP_OTW_N_GPIO_Port, AMP_CLIP_OTW_N_Pin) == GPIO_PIN_RESET ? 1 : 0) << I2CDEF_POWERAMP_STATUS_AMP_CLIPOTW_Pos) |
            (is_shutdown << I2CDEF_POWERAMP_STATUS_AMP_SD_Pos) |
            (pvdd_valid_voltage << I2CDEF_POWERAMP_STATUS_PVDD_VALID_Pos) |
            (pvdd_reduction_ongoing << I2CDEF_POWERAMP_STATUS_PVDD_RED_Pos) |
            ((fabsf(pvdd_voltage_request_offset) > 1e-5f ? 1 : 0) << I2CDEF_POWERAMP_STATUS_PVDD_ONZ_Pos) |
            ((fabsf(pvdd_voltage_request_offset) >= PVDD_VOLTAGE_OFFSET_MAX ? 1 : 0) << I2CDEF_POWERAMP_STATUS_PVDD_OLIM_Pos) |
            (((safety_warn_status_inst | safety_warn_status_loop) != 0 ? 1 : 0) << I2CDEF_POWERAMP_STATUS_SWARN_Pos) |
            ((uint16_t)i2c_err_detected << I2CDEF_POWERAMP_STATUS_I2CERR_Pos);
        i2c_err_detected = 0; //reset comm error detection after read
        break;
      case I2CDEF_POWERAMP_CONTROL:
        read_buf[0] =
            (manual_shutdown << I2CDEF_POWERAMP_CONTROL_AMP_MAN_SD_Pos) |
            (interrupts_enabled << I2CDEF_POWERAMP_CONTROL_INT_EN_Pos);
            //RESET bit always reads as 0
        break;
      case I2CDEF_POWERAMP_INT_MASK:
        read_buf[0] = interrupt_mask;
        break;
      case I2CDEF_POWERAMP_INT_FLAGS:
        read_buf[0] = interrupt_flags;
        if (init_interrupt_active) { //clear init interrupt
          init_interrupt_active = 0;
          _I2C_UpdateInterruptPin();
        }
        break;
      case I2CDEF_POWERAMP_PVDD_TARGET:
        ((float*)read_buf)[0] = pvdd_voltage_target;
        break;
      case I2CDEF_POWERAMP_PVDD_REQ:
        ((float*)read_buf)[0] = pvdd_voltage_requested;
        break;
      case I2CDEF_POWERAMP_PVDD_MEASURED:
        ((float*)read_buf)[0] = pvdd_voltage_measured;
        break;
      case I2CDEF_POWERAMP_SAFETY_STATUS:
      	read_buf[0] =
      	    (safety_shutdown << I2CDEF_POWERAMP_SAFETY_STATUS_SAFETY_SERR_SD_Pos) |
      	    (manual_shutdown << I2CDEF_POWERAMP_SAFETY_STATUS_SAFETY_MAN_SD_Pos);
      	break;
      case I2CDEF_POWERAMP_SERR_SOURCE:
      	((uint16_t*)read_buf)[0] = safety_err_status;
      	break;
      case I2CDEF_POWERAMP_SWARN_SOURCE:
        ((uint16_t*)read_buf)[0] = safety_warn_status_inst | safety_warn_status_loop;
      	break;
      case I2CDEF_POWERAMP_MODULE_ID:
        read_buf[0] = I2CDEF_POWERAMP_MODULE_ID_VALUE;
        break;
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
        default:
          i2c_err_detected = 1; //should never happen
          return;
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
        default:
          i2c_err_detected = 1; //should never happen
          return;
      }

      //return array value
      ((float*)read_buf)[0] = safety_array[index];
    }
  }
}

static __always_inline void _I2C_ErrorReset() {
  i2c_err_detected = 1;
  state = I2C_IDLE;
  HAL_I2C_EnableListen_IT(&hi2c3);
}

static __always_inline void _I2C_HardwareReset() {
  __disable_irq();
  //reset peripheral
  HAL_I2C_DeInit(&hi2c3);
  __HAL_RCC_I2C3_FORCE_RESET();
  int i;
  for (i = 0; i < 50; i++) __NOP();
  __HAL_RCC_I2C3_RELEASE_RESET();
  HAL_I2C_Init(&hi2c3);
  //enable I2C timeouts
  WRITE_REG(hi2c3.Instance->TIMEOUTR, (I2C_SCL_STRETCH_TIMEOUT | I2C_SCL_LOW_TIMEOUT));
  SET_BIT(hi2c3.Instance->TIMEOUTR, (I2C_TIMEOUTR_TEXTEN | I2C_TIMEOUTR_TIMOUTEN));
  //clear interrupt pin
  HAL_GPIO_WritePin(I2C3_INT_N_GPIO_Port, I2C3_INT_N_Pin, GPIO_PIN_SET);
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

  uint8_t new_size;

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

    uint8_t size = reg_size_map[reg_addr];
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
    uint8_t new_size;

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
  ReturnOnError(HAL_I2C_EnableListen_IT(&hi2c3));

  //reset internal state
  state = I2C_IDLE;

  //assert init interrupt (notify master that the controller has been reset - cleared once INT_FLAGS register is accessed)
  init_interrupt_active = 1;
  HAL_GPIO_WritePin(I2C3_INT_N_GPIO_Port, I2C3_INT_N_Pin, GPIO_PIN_RESET);

  return HAL_OK;
}

void I2C_TriggerInterrupt(uint8_t interrupt_bit) {
  interrupt_flags |= interrupt_bit; //set flag
  _I2C_UpdateInterruptPin();
}

void I2C_LoopUpdate() {
  if (state == I2C_IDLE && __HAL_I2C_GET_FLAG(&hi2c3, I2C_FLAG_BUSY) == SET) { //driver idle but peripheral busy: should be impossible
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
