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
#include "usbd_def.h"


#if I2C_VIRT_BUFFER_SIZE < I2CDEF_DAP_REG_SIZE_SP_FIR || I2C_VIRT_BUFFER_SIZE < I2CDEF_DAP_REG_SIZE_SP_BIQUAD
#error "I2C virtual buffer size is too small to fit FIR or biquad coefficient registers"
#endif

#if I2CDEF_DAP_REG_SIZE_SP_FIR != SP_MAX_FIR_LENGTH * 4 || I2CDEF_DAP_REG_SIZE_SP_BIQUAD != SP_MAX_BIQUADS * 5 * 4
#error "Mismatch between signal processor coefficient lengths and corresponding I2C register sizes"
#endif

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

//whether a comm error has been detected since the last status read
static uint8_t i2c_err_detected = 0;

//timeout for non-idle states in main loop cycles
static uint8_t non_idle_timeout = 0;
//sightings of busy peripheral in idle state
static uint8_t idle_busy_count = 0;

//register size map
static const uint16_t reg_size_map[] = I2CDEF_DAP_REG_SIZES;

//configured state
static uint8_t interrupts_enabled = 0;
static uint8_t interrupt_mask = 0;
static uint8_t interrupt_flags = I2CDEF_DAP_INT_FLAGS_INT_RESET_Msk;

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
  if (((interrupt_flags & I2CDEF_DAP_INT_FLAGS_INT_RESET_Msk) != 0) || (interrupts_enabled && (interrupt_flags & interrupt_mask) != 0)) {
    HAL_GPIO_WritePin(I2C_INT_PORT, I2C_INT_PIN, GPIO_PIN_RESET);
  } else {
    HAL_GPIO_WritePin(I2C_INT_PORT, I2C_INT_PIN, GPIO_PIN_SET);
  }
}

/**
 * called at the end of a write transaction, processes the received data to update amp state
 */
void _I2C_ProcessWriteData() {
  int i;
  uint8_t temp8;
  uint32_t temp32;
  float tempF;

  //DEBUG_PRINTF("I2C write trigger: address 0x%02X; size %u; value 0x%08lX (%f)\n", reg_addr, reg_size, *(uint32_t*)write_buf, *(float*)write_buf);

  //disallow filter setup or coefficient writes when signal processor is enabled
  if (reg_addr >= I2CDEF_DAP_BIQUAD_SETUP && reg_addr <= I2CDEF_DAP_FIR_COEFFS_CH2 && sp_enabled) {
    DEBUG_PRINTF("I2C write error: attempted write to signal processor filter setup or coefficients while SP is enabled\n");
    i2c_err_detected = 1;
    return;
  }

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

      //signal processor enabled state
      bool sp_enabled_new = (write_buf[0] & I2CDEF_DAP_CONTROL_SP_EN_Msk) != 0;
      if (!sp_enabled && sp_enabled_new) {
        //was disabled, enabling now: reset internal state
        SP_Reset();
      }
      sp_enabled = sp_enabled_new;

      //allow/disallow positive gains
      sp_volume_allow_positive_dB = (write_buf[0] & I2CDEF_DAP_CONTROL_ALLOW_POS_GAIN_Msk) != 0;

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
    case I2CDEF_DAP_INPUT_ACTIVE:
      //attempt to activate given input, does its own internal checks for input validity
      if (INPUT_Activate((INPUT_Source)write_buf[0]) != HAL_OK) {
        //failed (due to invalid given input): report error
        i2c_err_detected = 1;
      }
      break;
    case I2CDEF_DAP_I2S1_SAMPLE_RATE:
    case I2CDEF_DAP_I2S2_SAMPLE_RATE:
    case I2CDEF_DAP_I2S3_SAMPLE_RATE:
      //get sample rate value and check for validity
      temp32 = *(uint32_t*)write_buf;
      if (SRC_IsValidSampleRate(temp32)) {
        //sample rate good: get index of I2S interface
        temp8 = reg_addr - I2CDEF_DAP_I2S1_SAMPLE_RATE;
        //store sample rate and update it
        input_i2s_sample_rates[temp8] = (SRC_SampleRate)temp32;
        INPUT_UpdateSampleRate((INPUT_Source)(INPUT_I2S1 + temp8));
      } else {
        //bad sample rate: report error
        DEBUG_PRINTF("I2C write error: attempted to write bad sample rate %lu\n", temp32);
        i2c_err_detected = 1;
      }
      break;
    case I2CDEF_DAP_MIXER_GAINS:
      //copy directly to SP mixer gain array
      memcpy(sp_mixer_gains, write_buf, sizeof(sp_mixer_gains));
      break;
    case I2CDEF_DAP_VOLUME_GAINS:
      for (i = 0; i < SP_MAX_CHANNELS; i++) {
        //get float value
        tempF = ((float*)write_buf)[i];
        if (!isnanf(tempF) && tempF >= SP_MIN_VOL_GAIN && (tempF <= 0.0f || (sp_volume_allow_positive_dB && tempF <= SP_MAX_VOL_GAIN))) {
          //valid gain: write
          sp_volume_gains_dB[i] = tempF;
        } else {
          //invalid gain: report error
          i2c_err_detected = 1;
        }
      }
      break;
    case I2CDEF_DAP_LOUDNESS_GAINS:
      for (i = 0; i < SP_MAX_CHANNELS; i++) {
        //get float value
        tempF = ((float*)write_buf)[i];
        if (!isnanf(tempF) && tempF <= SP_MAX_LOUDNESS_GAIN) {
          //valid gain: write
          sp_loudness_gains_dB[i] = tempF;
        } else {
          //invalid gain: report error
          i2c_err_detected = 1;
        }
      }
      break;
    case I2CDEF_DAP_BIQUAD_SETUP:
      //attempt to perform biquad setup, does its own internal checks for validity
      if (SP_SetupBiquads(write_buf, write_buf + SP_MAX_CHANNELS) != HAL_OK) {
        //failed (due to invalid parameters): report error
        i2c_err_detected = 1;
      }
      break;
    case I2CDEF_DAP_FIR_SETUP:
      //attempt to perform FIR setup, does its own internal checks for validity
      if (SP_SetupFIRs((uint16_t*)write_buf) != HAL_OK) {
        //failed (due to invalid parameters): report error
        i2c_err_detected = 1;
      }
      break;
    case I2CDEF_DAP_BIQUAD_COEFFS_CH1:
    case I2CDEF_DAP_BIQUAD_COEFFS_CH2:
      //get channel
      temp8 = reg_addr - I2CDEF_DAP_BIQUAD_COEFFS_CH1;
      //copy to corresponding buffer
      memcpy(sp_biquad_coeffs[temp8], write_buf, SP_MAX_BIQUADS * 5 * sizeof(q31_t)); //TODO: actual written size?
      break;
    case I2CDEF_DAP_FIR_COEFFS_CH1:
    case I2CDEF_DAP_FIR_COEFFS_CH2:
      //get channel
      temp8 = reg_addr - I2CDEF_DAP_FIR_COEFFS_CH1;
      //copy to corresponding buffer
      memcpy(sp_fir_coeffs[temp8], write_buf, SP_MAX_FIR_LENGTH * sizeof(q31_t)); //TODO: actual written size?
      break;
    default:
      DEBUG_PRINTF("I2C write error: attempted write to non-writable register 0x%02X\n", reg_addr);
      i2c_err_detected = 1; //attempting to write to read-only register - report error
      break;
  }
}

/**
 * called at the start of a read transaction, prepares the data to be sent
 */
void _I2C_PrepareReadData() {
  bool tempB;
  uint8_t temp8;
  extern USBD_HandleTypeDef hUsbDeviceHS;

  memset(read_buf, 0, I2C_VIRT_BUFFER_SIZE);

  switch (reg_addr) {
    case I2CDEF_DAP_STATUS:
      tempB = SRC_IsReady();
      read_buf[0] =
          (tempB && sp_enabled ? I2CDEF_DAP_STATUS_STREAMING_Msk : 0) |
          (tempB ? I2CDEF_DAP_STATUS_SRC_READY_Msk : 0) |
          (hUsbDeviceHS.dev_state == USBD_STATE_CONFIGURED ? I2CDEF_DAP_STATUS_USB_CONN_Msk : 0) |
          (i2c_err_detected != 0 ? I2CDEF_DAP_STATUS_I2CERR_Msk : 0);
      i2c_err_detected = 0; //reset comm error detection after read
      break;
    case I2CDEF_DAP_CONTROL:
      read_buf[0] =
          (interrupts_enabled != 0 ? I2CDEF_DAP_CONTROL_INT_EN_Msk : 0) |
          (sp_enabled ? I2CDEF_DAP_CONTROL_SP_EN_Msk : 0) |
          (sp_volume_allow_positive_dB ? I2CDEF_DAP_CONTROL_ALLOW_POS_GAIN_Msk : 0);
      break;
    case I2CDEF_DAP_INT_MASK:
      read_buf[0] = interrupt_mask;
      break;
    case I2CDEF_DAP_INT_FLAGS:
      read_buf[0] = interrupt_flags;
      break;
    case I2CDEF_DAP_INPUT_ACTIVE:
      read_buf[0] = (uint8_t)input_active;
      break;
    case I2CDEF_DAP_INPUTS_AVAILABLE:
      read_buf[0] =
          (inputs_available[INPUT_I2S1] ? I2CDEF_DAP_INPUTS_AVAILABLE_I2S1_Msk : 0) |
          (inputs_available[INPUT_I2S2] ? I2CDEF_DAP_INPUTS_AVAILABLE_I2S2_Msk : 0) |
          (inputs_available[INPUT_I2S3] ? I2CDEF_DAP_INPUTS_AVAILABLE_I2S3_Msk : 0) |
          (inputs_available[INPUT_USB] ? I2CDEF_DAP_INPUTS_AVAILABLE_USB_Msk : 0) |
          (inputs_available[INPUT_SPDIF] ? I2CDEF_DAP_INPUTS_AVAILABLE_SPDIF_Msk : 0);
      break;
    case I2CDEF_DAP_I2S1_SAMPLE_RATE:
    case I2CDEF_DAP_I2S2_SAMPLE_RATE:
    case I2CDEF_DAP_I2S3_SAMPLE_RATE:
      //get index of I2S interface
      temp8 = reg_addr - I2CDEF_DAP_I2S1_SAMPLE_RATE;
      //get sample rate
      ((uint32_t*)read_buf)[0] = (uint32_t)input_i2s_sample_rates[temp8];
      break;
    case I2CDEF_DAP_SRC_INPUT_RATE:
      ((uint32_t*)read_buf)[0] = (uint32_t)SRC_GetCurrentInputRate();
      break;
    case I2CDEF_DAP_SRC_RATE_ERROR:
      ((float*)read_buf)[0] = SRC_GetAverageRateError();
      break;
    case I2CDEF_DAP_SRC_BUFFER_ERROR:
      ((float*)read_buf)[0] = SRC_GetAverageBufferFillError();
      break;
    case I2CDEF_DAP_MIXER_GAINS:
      //copy directly from SP mixer gain array
      memcpy(read_buf, sp_mixer_gains, sizeof(sp_mixer_gains));
      break;
    case I2CDEF_DAP_VOLUME_GAINS:
      //copy directly from SP volume gain array
      memcpy(read_buf, sp_volume_gains_dB, sizeof(sp_volume_gains_dB));
      break;
      break;
    case I2CDEF_DAP_LOUDNESS_GAINS:
      //copy directly from SP loudness gain array
      memcpy(read_buf, sp_loudness_gains_dB, sizeof(sp_loudness_gains_dB));
      break;
      break;
    case I2CDEF_DAP_BIQUAD_SETUP:
      SP_GetBiquadSetup(read_buf, read_buf + SP_MAX_CHANNELS);
      break;
    case I2CDEF_DAP_FIR_SETUP:
      SP_GetFIRSetup((uint16_t*)read_buf);
      break;
    case I2CDEF_DAP_BIQUAD_COEFFS_CH1:
    case I2CDEF_DAP_BIQUAD_COEFFS_CH2:
      //get channel
      temp8 = reg_addr - I2CDEF_DAP_BIQUAD_COEFFS_CH1;
      //copy from corresponding buffer
      memcpy(read_buf, sp_biquad_coeffs[temp8], SP_MAX_BIQUADS * 5 * sizeof(q31_t));
      break;
    case I2CDEF_DAP_FIR_COEFFS_CH1:
    case I2CDEF_DAP_FIR_COEFFS_CH2:
      //get channel
      temp8 = reg_addr - I2CDEF_DAP_FIR_COEFFS_CH1;
      //copy from corresponding buffer
      memcpy(read_buf, sp_fir_coeffs[temp8], SP_MAX_FIR_LENGTH * sizeof(q31_t));
      break;
    case I2CDEF_DAP_MODULE_ID:
      read_buf[0] = I2CDEF_DAP_MODULE_ID_VALUE;
      break;
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
  interrupt_flags = I2CDEF_DAP_INT_FLAGS_INT_RESET_Msk;
  i2c_err_detected = 0;
  non_idle_timeout = 0;
  idle_busy_count = 0;

  //perform hardware reset
  _I2C_HardwareReset();

  //enable I2C listening
  ReturnOnError(HAL_I2C_EnableListen_IT(&I2C_INSTANCE));

  //reset internal state
  state = I2C_IDLE;

  //update interrupt pin for init interrupt
  _I2C_UpdateInterruptPin();

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
