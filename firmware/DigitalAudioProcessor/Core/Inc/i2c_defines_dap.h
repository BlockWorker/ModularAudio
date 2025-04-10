/*
 * i2c_defines_dap.h
 *
 *  Created on: Apr 9, 2025
 *      Author: Alex
 *
 *  Digital Audio Processor I2C register definitions
 *
 *  Register map:
 *  * General registers
 *    - 0x01: STATUS: General status (1B, bit field, r)
 *    - 0x08: CONTROL: General control (1B, bit field, rw)
 *  * Interrupt registers
 *    - 0x10: INT_MASK: Interrupt mask (1B, bit field, rw, high = interrupt enable)
 *    - 0x11: INT_FLAGS: Interrupt flags (1B, bit field, rc, high = interrupt occurred)
 *  * Input registers
 *    - 0x20: INPUT_ACTIVE: Active input (1B, enum, rw)
 *    - 0x21: INPUTS_AVAILABLE: Available/connected/playing inputs (1B, bit field, r)
 *    - 0x28: I2S1_SAMPLE_RATE: I2S1 input sample rate (4B, unsigned 44.1K/48K/96K, rw)
 *    - 0x29: I2S2_SAMPLE_RATE: I2S2 input sample rate (4B, unsigned 44.1K/48K/96K, rw)
 *    - 0x2A: I2S3_SAMPLE_RATE: I2S3 input sample rate (4B, unsigned 44.1K/48K/96K, rw)
 *  * Sample rate converter registers
 *    - 0x30: SRC_INPUT_RATE: Currently configured input sample rate (4B, unsigned 44.1K/48K/96K, r)
 *    - 0x31: SRC_RATE_ERROR: Average relative input sample rate error (4B, float, r)
 *    - 0x32: SRC_BUFFER_ERROR: Average buffer fill level error in samples (4B, float, r)
 *  * Signal processor registers - filter setups and coefficients are only writable when signal processor is disabled
 *    - 0x40: MIXER_GAINS: Mixer gain matrix: out1in1, out1in2, out2in1, out2in2, each half of true gain (16B, 4 * 4B fixed point Q31, rw)
 *    - 0x41: VOLUME_GAINS: Volume gains per output channel in dB, in range [-120, 20] if positive gains are allowed, otherwise [-120, 0] (8B, 2 * 4B float, rw)
 *    - 0x42: LOUDNESS_GAINS: Loudness compensation gain per output channel in dB - active in range [-30, 0], lower to disable (8B, 2 * 4B float, rw)
 *    - 0x43: BIQUAD_SETUP: Number of active biquad filters per channel and their post-shift values (4B, 2 * 1B unsigned count + 2 * 1B unsigned shift, rw)
 *    - 0x44: FIR_SETUP: Active length of FIR filter per channel (4B, 2 * 2B unsigned length, rw)
 *    - 0x50-0x51: BIQUAD_COEFFS_CH?: Biquad filter coefficients: each b0 b1 b2 a1 a2, consecutive filters, a1+a2 negated vs. MATLAB (320B, 16 * 5 * 4B fixed point Q31, rw)
 *    - 0x58-0x59: FIR_COEFFS_CH?: FIR filter coefficients, in reverse-time order (coefficient 0 is last) (1200B, 300 * 4B fixed point Q31, rw)
 *  * Misc registers
 *    - 0xFF: MODULE_ID: Module ID (1B, hex, r)
 *
 *  Bit field and enum definitions:
 *  * STATUS (0x01, bit field, 1B):
 *    - 7: I2CERR: I2C communication error detected since last STATUS read
 *    - 1: SRC_READY: Sample rate converter is ready to provide output audio data
 *    - 0: STREAMING: Audio data is being streamed to the output
 *  * CONTROL (0x08, bit field, 1B):
 *    - 4-7: RESET: controller reset (write 0xA to trigger software reset)
 *    - 2: ALLOW_POS_GAIN: Allow positive dB volume gains (above-unity gain, may cause clipping)
 *    - 1: SP_EN: Enable signal processor
 *    - 0: INT_EN: Enable I2C interrupts
 *  * INT_MASK (0x10, bit field, 1B):
 *    - 3: INT_INPUT_RATE: Input sample rate changed
 *    - 2: INT_INPUT_AVAILABLE: Availability of inputs changed
 *    - 1: INT_ACTIVE_INPUT: Active input changed
 *    - 0: INT_SRC_READY: Sample rate converter ready state changed
 *  * INT_FLAGS (0x11, bit field, 1B):
 *    same layout as INT_MASK
 *  * INPUT_ACTIVE (0x20, enum, 1B):
 *    - 0x00: NONE: No input
 *    - 0x01: I2S1
 *    - 0x02: I2S2
 *    - 0x03: I2S3
 *    - 0x04: USB
 *    - 0x05: SPDIF
 *  * INPUTS_AVAILABLE (0x21, bit field, 1B):
 *    - 4: SPDIF
 *    - 3: USB
 *    - 2: I2S3
 *    - 1: I2S2
 *    - 0: I2S1
 *
 */

#ifndef INC_I2C_DEFINES_DAP_H_
#define INC_I2C_DEFINES_DAP_H_


//sizes of signal processor related registers in bytes
#define I2CDEF_DAP_REG_SIZE_SP_FIR (300 * 4)
#define I2CDEF_DAP_REG_SIZE_SP_BIQUAD (16 * 5 * 4)

//virtual register sizes in bytes - 0 means register is invalid
#define I2CDEF_DAP_REG_SIZES {\
  0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,\
  1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  1, 1, 0, 0, 0, 0, 0, 0, 4, 4, 4, 0, 0, 0, 0, 0,\
  4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  16, 8, 8, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  I2CDEF_DAP_REG_SIZE_SP_BIQUAD, I2CDEF_DAP_REG_SIZE_SP_BIQUAD, 0, 0, 0, 0, 0, 0, I2CDEF_DAP_REG_SIZE_SP_FIR, I2CDEF_DAP_REG_SIZE_SP_FIR, 0, 0, 0, 0, 0, 0,\
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 }


//General registers
#define I2CDEF_DAP_STATUS 0x01

#define I2CDEF_DAP_STATUS_STREAMING_Pos 0
#define I2CDEF_DAP_STATUS_STREAMING_Msk (0x1 << I2CDEF_DAP_STATUS_STREAMING_Pos)
#define I2CDEF_DAP_STATUS_SRC_READY_Pos 1
#define I2CDEF_DAP_STATUS_SRC_READY_Msk (0x1 << I2CDEF_DAP_STATUS_SRC_READY_Pos)
#define I2CDEF_DAP_STATUS_I2CERR_Pos 7
#define I2CDEF_DAP_STATUS_I2CERR_Msk (0x1 << I2CDEF_DAP_STATUS_I2CERR_Pos)

#define I2CDEF_DAP_CONTROL 0x08

#define I2CDEF_DAP_CONTROL_INT_EN_Pos 0
#define I2CDEF_DAP_CONTROL_INT_EN_Msk (0x1 << I2CDEF_DAP_CONTROL_INT_EN_Pos)
#define I2CDEF_DAP_CONTROL_SP_EN_Pos 1
#define I2CDEF_DAP_CONTROL_SP_EN_Msk (0x1 << I2CDEF_DAP_CONTROL_SP_EN_Pos)
#define I2CDEF_DAP_CONTROL_ALLOW_POS_GAIN_Pos 2
#define I2CDEF_DAP_CONTROL_ALLOW_POS_GAIN_Msk (0x1 << I2CDEF_DAP_CONTROL_ALLOW_POS_GAIN_Pos)
#define I2CDEF_DAP_CONTROL_RESET_Pos 4
#define I2CDEF_DAP_CONTROL_RESET_Msk (0xF << I2CDEF_DAP_CONTROL_RESET_Pos)
#define I2CDEF_DAP_CONTROL_RESET_VALUE 0xA


//Interrupt registers
#define I2CDEF_DAP_INT_MASK 0x10

#define I2CDEF_DAP_INT_MASK_INT_SRC_READY_Pos 0
#define I2CDEF_DAP_INT_MASK_INT_SRC_READY_Msk (0x1 << I2CDEF_DAP_INT_MASK_INT_SRC_READY_Pos)
#define I2CDEF_DAP_INT_MASK_INT_ACTIVE_INPUT_Pos 1
#define I2CDEF_DAP_INT_MASK_INT_ACTIVE_INPUT_Msk (0x1 << I2CDEF_DAP_INT_MASK_INT_ACTIVE_INPUT_Pos)
#define I2CDEF_DAP_INT_MASK_INT_INPUT_AVAILABLE_Pos 2
#define I2CDEF_DAP_INT_MASK_INT_INPUT_AVAILABLE_Msk (0x1 << I2CDEF_DAP_INT_MASK_INT_INPUT_AVAILABLE_Pos)
#define I2CDEF_DAP_INT_MASK_INT_INPUT_RATE_Pos 3
#define I2CDEF_DAP_INT_MASK_INT_INPUT_RATE_Msk (0x1 << I2CDEF_DAP_INT_MASK_INT_INPUT_RATE_Pos)

#define I2CDEF_DAP_INT_FLAGS 0x11

#define I2CDEF_DAP_INT_FLAGS_INT_SRC_READY_Pos I2CDEF_DAP_INT_MASK_INT_SRC_READY_Pos
#define I2CDEF_DAP_INT_FLAGS_INT_SRC_READY_Msk I2CDEF_DAP_INT_MASK_INT_SRC_READY_Msk
#define I2CDEF_DAP_INT_FLAGS_INT_ACTIVE_INPUT_Pos I2CDEF_DAP_INT_MASK_INT_ACTIVE_INPUT_Pos
#define I2CDEF_DAP_INT_FLAGS_INT_ACTIVE_INPUT_Msk I2CDEF_DAP_INT_MASK_INT_ACTIVE_INPUT_Msk
#define I2CDEF_DAP_INT_FLAGS_INT_INPUT_AVAILABLE_Pos I2CDEF_DAP_INT_MASK_INT_INPUT_AVAILABLE_Pos
#define I2CDEF_DAP_INT_FLAGS_INT_INPUT_AVAILABLE_Msk I2CDEF_DAP_INT_MASK_INT_INPUT_AVAILABLE_Msk
#define I2CDEF_DAP_INT_FLAGS_INT_INPUT_RATE_Pos I2CDEF_DAP_INT_MASK_INT_INPUT_RATE_Pos
#define I2CDEF_DAP_INT_FLAGS_INT_INPUT_RATE_Msk I2CDEF_DAP_INT_MASK_INT_INPUT_RATE_Msk


//Input registers
#define I2CDEF_DAP_INPUT_ACTIVE 0x20

#define I2CDEF_DAP_INPUT_ACTIVE_NONE 0x00
#define I2CDEF_DAP_INPUT_ACTIVE_I2S1 0x01
#define I2CDEF_DAP_INPUT_ACTIVE_I2S2 0x02
#define I2CDEF_DAP_INPUT_ACTIVE_I2S3 0x03
#define I2CDEF_DAP_INPUT_ACTIVE_USB 0x04
#define I2CDEF_DAP_INPUT_ACTIVE_SPDIF 0x05

#define I2CDEF_DAP_INPUTS_AVAILABLE 0x21

#define I2CDEF_DAP_INPUTS_AVAILABLE_I2S1_Pos 0
#define I2CDEF_DAP_INPUTS_AVAILABLE_I2S1_Msk (0x1 << I2CDEF_DAP_INPUTS_AVAILABLE_I2S1_Pos)
#define I2CDEF_DAP_INPUTS_AVAILABLE_I2S2_Pos 1
#define I2CDEF_DAP_INPUTS_AVAILABLE_I2S2_Msk (0x1 << I2CDEF_DAP_INPUTS_AVAILABLE_I2S2_Pos)
#define I2CDEF_DAP_INPUTS_AVAILABLE_I2S3_Pos 2
#define I2CDEF_DAP_INPUTS_AVAILABLE_I2S3_Msk (0x1 << I2CDEF_DAP_INPUTS_AVAILABLE_I2S3_Pos)
#define I2CDEF_DAP_INPUTS_AVAILABLE_USB_Pos 3
#define I2CDEF_DAP_INPUTS_AVAILABLE_USB_Msk (0x1 << I2CDEF_DAP_INPUTS_AVAILABLE_USB_Pos)
#define I2CDEF_DAP_INPUTS_AVAILABLE_SPDIF_Pos 4
#define I2CDEF_DAP_INPUTS_AVAILABLE_SPDIF_Msk (0x1 << I2CDEF_DAP_INPUTS_AVAILABLE_SPDIF_Pos)

#define I2CDEF_DAP_I2S1_SAMPLE_RATE 0x28

#define I2CDEF_DAP_I2S2_SAMPLE_RATE 0x29

#define I2CDEF_DAP_I2S3_SAMPLE_RATE 0x2A


//Sample rate converter registers
#define I2CDEF_DAP_SRC_INPUT_RATE 0x30

#define I2CDEF_DAP_SRC_RATE_ERROR 0x31

#define I2CDEF_DAP_SRC_BUFFER_ERROR 0x32


//Signal processor registers
#define I2CDEF_DAP_MIXER_GAINS 0x40

#define I2CDEF_DAP_VOLUME_GAINS 0x41

#define I2CDEF_DAP_LOUDNESS_GAINS 0x42

#define I2CDEF_DAP_BIQUAD_SETUP 0x43

#define I2CDEF_DAP_FIR_SETUP 0x44

#define I2CDEF_DAP_BIQUAD_COEFFS_CH1 0x50
#define I2CDEF_DAP_BIQUAD_COEFFS_CH2 0x51

#define I2CDEF_DAP_FIR_COEFFS_CH1 0x58
#define I2CDEF_DAP_FIR_COEFFS_CH2 0x59


//Misc registers
#define I2CDEF_DAP_MODULE_ID 0xFF
#define I2CDEF_DAP_MODULE_ID_VALUE 0xD4


#endif /* INC_I2C_DEFINES_DAP_H_ */
