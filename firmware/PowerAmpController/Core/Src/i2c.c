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


#define I2C_OWN_ADDRESS_WRITE ((uint8_t)I2C_GET_OWN_ADDRESS1(&I2C_INSTANCE))
#define I2C_OWN_ADDRESS_READ (I2C_OWN_ADDRESS_WRITE | 0x01)


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
static uint8_t __attribute__((aligned(4))) read_buf[I2C_VIRT_BUFFER_SIZE + 1] = { 0 };
static uint8_t __attribute__((aligned(4))) write_buf[I2C_VIRT_BUFFER_SIZE + 1] = { 0 };

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
static const uint16_t reg_size_map[] = I2CDEF_POWERAMP_REG_SIZES;

//configured state
static uint8_t interrupts_enabled = 0;
static uint8_t interrupt_mask = 0;
static uint8_t interrupt_flags = 0;

//I2C CRC data
static const uint8_t _i2c_crc_table[256] = {
  0x00, 0x7F, 0xFE, 0x81, 0x83, 0xFC, 0x7D, 0x02, 0x79, 0x06, 0x87, 0xF8, 0xFA, 0x85, 0x04, 0x7B,
  0xF2, 0x8D, 0x0C, 0x73, 0x71, 0x0E, 0x8F, 0xF0, 0x8B, 0xF4, 0x75, 0x0A, 0x08, 0x77, 0xF6, 0x89,
  0x9B, 0xE4, 0x65, 0x1A, 0x18, 0x67, 0xE6, 0x99, 0xE2, 0x9D, 0x1C, 0x63, 0x61, 0x1E, 0x9F, 0xE0,
  0x69, 0x16, 0x97, 0xE8, 0xEA, 0x95, 0x14, 0x6B, 0x10, 0x6F, 0xEE, 0x91, 0x93, 0xEC, 0x6D, 0x12,
  0x49, 0x36, 0xB7, 0xC8, 0xCA, 0xB5, 0x34, 0x4B, 0x30, 0x4F, 0xCE, 0xB1, 0xB3, 0xCC, 0x4D, 0x32,
  0xBB, 0xC4, 0x45, 0x3A, 0x38, 0x47, 0xC6, 0xB9, 0xC2, 0xBD, 0x3C, 0x43, 0x41, 0x3E, 0xBF, 0xC0,
  0xD2, 0xAD, 0x2C, 0x53, 0x51, 0x2E, 0xAF, 0xD0, 0xAB, 0xD4, 0x55, 0x2A, 0x28, 0x57, 0xD6, 0xA9,
  0x20, 0x5F, 0xDE, 0xA1, 0xA3, 0xDC, 0x5D, 0x22, 0x59, 0x26, 0xA7, 0xD8, 0xDA, 0xA5, 0x24, 0x5B,
  0x92, 0xED, 0x6C, 0x13, 0x11, 0x6E, 0xEF, 0x90, 0xEB, 0x94, 0x15, 0x6A, 0x68, 0x17, 0x96, 0xE9,
  0x60, 0x1F, 0x9E, 0xE1, 0xE3, 0x9C, 0x1D, 0x62, 0x19, 0x66, 0xE7, 0x98, 0x9A, 0xE5, 0x64, 0x1B,
  0x09, 0x76, 0xF7, 0x88, 0x8A, 0xF5, 0x74, 0x0B, 0x70, 0x0F, 0x8E, 0xF1, 0xF3, 0x8C, 0x0D, 0x72,
  0xFB, 0x84, 0x05, 0x7A, 0x78, 0x07, 0x86, 0xF9, 0x82, 0xFD, 0x7C, 0x03, 0x01, 0x7E, 0xFF, 0x80,
  0xDB, 0xA4, 0x25, 0x5A, 0x58, 0x27, 0xA6, 0xD9, 0xA2, 0xDD, 0x5C, 0x23, 0x21, 0x5E, 0xDF, 0xA0,
  0x29, 0x56, 0xD7, 0xA8, 0xAA, 0xD5, 0x54, 0x2B, 0x50, 0x2F, 0xAE, 0xD1, 0xD3, 0xAC, 0x2D, 0x52,
  0x40, 0x3F, 0xBE, 0xC1, 0xC3, 0xBC, 0x3D, 0x42, 0x39, 0x46, 0xC7, 0xB8, 0xBA, 0xC5, 0x44, 0x3B,
  0xB2, 0xCD, 0x4C, 0x33, 0x31, 0x4E, 0xCF, 0xB0, 0xCB, 0xB4, 0x35, 0x4A, 0x48, 0x37, 0xB6, 0xC9
};
static uint8_t _i2c_crc_state = 0;


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
        if (init_interrupt_active) {
          //init interrupt: report no flags (to signal init), then clear init interrupt
          read_buf[0] = 0;
          init_interrupt_active = 0;
          _I2C_UpdateInterruptPin();
        } else {
          read_buf[0] = interrupt_flags;
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

//calculate CRC, starting with existing _i2c_crc_state
static uint8_t _I2C_CRC_Accumulate(uint8_t* buf, uint16_t length) {
  int i;
  for (i = 0; i < length; i++) {
    _i2c_crc_state = _i2c_crc_table[buf[i] ^ _i2c_crc_state];
  }
  return _i2c_crc_state;
}

//calculates the CRC for the current read buffer and writes it to the end of the buffer
static inline void _I2C_CalculateReadCRC() {
  read_buf[reg_size] = _I2C_CRC_Accumulate(read_buf, reg_size);
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
  for (i = 0; i < 10; i++) __NOP();
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
    if (state != I2C_UNINIT) { //write transmission: start with register address, with potential write data afterwards
      state = I2C_WAITING_ADDR;
      non_idle_timeout = I2C_NONIDLE_TIMEOUT;

      HAL_I2C_Slave_Seq_Receive_IT(hi2c, &reg_addr, 1, I2C_FIRST_FRAME);
    } else {
      DEBUG_PRINTF("I2C Error: Transmit request in invalid state %u\n", state);
      _I2C_HardwareReset();
      _I2C_ErrorReset();
    }
  } else {
    if (state == I2C_ADDR_RECEIVED) { //read transmission: only supported after address write
      state = I2C_READ;
      non_idle_timeout = I2C_NONIDLE_TIMEOUT;

      if (HAL_I2C_Slave_Seq_Transmit_IT(hi2c, read_buf, reg_size + 1, I2C_NEXT_FRAME) != HAL_OK) {
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
    memset(read_buf, 0, I2C_VIRT_BUFFER_SIZE + 1);
    reg_size = 1;
    reg_addr = 0;
  }

  //subsequent chained register read: only include data itself in CRC
  _i2c_crc_state = 0;
  _I2C_CalculateReadCRC();

  if (HAL_I2C_Slave_Seq_Transmit_IT(hi2c, read_buf, reg_size + 1, I2C_NEXT_FRAME) != HAL_OK) {
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
      memset(read_buf, 0, I2C_VIRT_BUFFER_SIZE + 1);
      reg_size = 1;
      reg_addr = 0;
      i2c_err_detected = 1;
    } else { //valid register: set size and prepare potential read data
      reg_size = size;
      _I2C_PrepareReadData();
    }

    //first register read after address: include I2C address and register address bytes in CRC
    uint8_t read_crc_buf[3] = { I2C_OWN_ADDRESS_WRITE, reg_addr, I2C_OWN_ADDRESS_READ };
    _i2c_crc_state = 0;
    _I2C_CRC_Accumulate(read_crc_buf, 3);
    _I2C_CalculateReadCRC();

    if (HAL_I2C_Slave_Seq_Receive_IT(hi2c, write_buf, reg_size + 1, I2C_NEXT_FRAME) != HAL_OK) { //prepare to receive write data immediately
      DEBUG_PRINTF("I2C Error: Write start fail, address 0x%02X\n", reg_addr);
      _I2C_HardwareReset();
      _I2C_ErrorReset();
    }
  } else if (state == I2C_ADDR_RECEIVED || state == I2C_WRITE) { //received data is the data to be written: process it and prepare to accept more data (always to prevent freeze)
    uint16_t new_size;

    _I2C_State prev_state = state;
    state = I2C_WRITE;
    non_idle_timeout = I2C_NONIDLE_TIMEOUT;

    if (reg_addr == 0) { //register invalid - continue as invalid
      new_size = 0;
    } else { //register valid: calculate and check CRC before processing
      _i2c_crc_state = 0;
      if (prev_state == I2C_ADDR_RECEIVED) {
        //first register after address: include I2C address and register address in CRC
        uint8_t write_crc_init_buf[2] = { I2C_OWN_ADDRESS_WRITE, reg_addr };
        _I2C_CRC_Accumulate(write_crc_init_buf, 2);
      }

      uint8_t crc_check = _I2C_CRC_Accumulate(write_buf, reg_size + 1);
      if (crc_check == 0) {
        //good CRC: process data
        _I2C_ProcessWriteData();
      } else {
        //wrong CRC: report error and ignore data
        DEBUG_PRINTF("I2C CRC mismatch on write of register 0x%02X\n", reg_addr);
        i2c_err_detected = 1;
      }

      //get next register size (if valid)
      new_size = reg_size_map[++reg_addr];
    }

    if (new_size > 0) { //next register valid: accept its data next
      reg_size = new_size;
    } else { //next register invalid: accept dummy data and continue as invalid
      reg_size = 1;
      reg_addr = 0;
    }

    if (HAL_I2C_Slave_Seq_Receive_IT(hi2c, write_buf, reg_size + 1, I2C_NEXT_FRAME) != HAL_OK) {
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
  if (state == I2C_IDLE && __HAL_I2C_GET_FLAG(&I2C_INSTANCE, I2C_FLAG_BUSY) == SET) { //driver idle but peripheral busy: check timeout
    if (++idle_busy_count > I2C_PERIPHERAL_BUSY_TIMEOUT) { //peripheral busy for too long, reset
      DEBUG_PRINTF("I2C Error: Peripheral busy in idle state timeout\n");
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
