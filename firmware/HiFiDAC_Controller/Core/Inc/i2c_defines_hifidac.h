/*
 * i2c_defines_hifidac.h
 *
 *  Created on: Mar 21, 2025
 *      Author: Alex
 *
 *  HiFiDAC I2C register definitions
 *
 *  Register map:
 *  * General registers
 *    - 0x01: STATUS: General status (1B, bit field, r)
 *    - 0x08: CONTROL: General control (1B, bit field, rw)
 *  * Interrupt registers
 *    - 0x10: INT_MASK: Interrupt mask (1B, bit field, rw, high = interrupt enable)
 *    - 0x11: INT_FLAGS: Interrupt flags (1B, bit field, rc, high = interrupt occurred)
 *  * DAC control registers
 *    - 0x20: VOLUME: Channel volumes (2B (1 + 1) unsigned, low: CH1 * -.5dB, high: CH2 * -.5dB, rw)
 *    - 0x21: MUTE: Channel mute (1B, bit field, rw)
 *    - 0x22: PATH: DAC signal path control (1B, bit field, rw)
 *    - 0x23: CLK_CFG: DAC clock config (1B, bit field, rw)
 *    - 0x24: MASTER_DIV: I2C master mode BCK divider (1B, unsigned divider minus one, rw)
 *    - 0x25: TDM_SLOT_NUM: Number of TDM slots/channels (1B, unsigned slot count minus one, max 31, rw)
 *    - 0x26: CH_SLOTS: TDM slot for each channel (2B (1 + 1) unsigned zero based, low: CH1 slot, high: CH2 slot, each max 31, rw)
 *  * Calibration registers
 *    - 0x30: FILTER_SHAPE: FIR interpolation filter shape (1B, unsigned, max 7, see ES9039Q2M datasheet, rw)
 *    - 0x31: THD_C2: Second harmonic correction coefficients (4B (2 + 2) signed, low: CH1, high: CH2, see ES9039Q2M datasheet, rw)
 *    - 0x32: THD_C3: Third harmonic correction coefficients (4B (2 + 2) signed, low: CH1, high: CH2, see ES9039Q2M datasheet, rw)
 *  * Misc registers
 *    - 0xFF: MODULE_ID: Module ID (1B, hex, r)
 *
 *  Bit field definitions:
 *  * STATUS (0x01, 1B):
 *    - 8: I2CERR: I2C communication error detected since last STATUS read
 *    - 6: MONITOR_ERROR: BCK/WS monitor has detected an error
 *    - 5: RAMP_DONE_CH2: Channel 2 soft start ramp is done
 *    - 4: RAMP_DONE_CH1: Channel 1 soft start ramp is done
 *    - 3: AUTOMUTE_CH2: Channel 2 is automuted
 *    - 2: AUTOMUTE_CH1: Channel 1 is automuted
 *    - 1: LOCK: Async mode SRC is locked to the input
 *    - 0: INIT_DONE: System init complete
 *  * CONTROL (0x08, 1B):
 *    - 4-7: RESET: controller reset (write 0xA to trigger software reset)
 *    - 3: MASTER: I2C master mode (otherwise slave mode)
 *    - 2: SYNC: Synchronous DAC mode (otherwise asynchronous mode)
 *    - 1: DAC_EN: Enable analog DAC section
 *    - 0: INT_EN: Enable I2C interrupts
 *  * INT_MASK (0x10, 1B):
 *    - 3: INT_MONITOR: BCK/WS monitor error status changed
 *    - 2: INT_RAMP: Ramp done status changed
 *    - 1: INT_AUTOMUTE: Automute status changed
 *    - 0: INT_LOCK: Async lock status changed
 *  * INT_FLAGS (0x11, 1B):
 *    same layout as INT_MASK
 *  * MUTE (0x21, 1B):
 *    - 1: MUTE_CH2: Manual mute channel 2
 *    - 0: MUTE_CH1: Manual mute channel 1
 *  * PATH (0x22, 1B):
 *    - 6: MUTE_GND_RAMP: Soft ramp outputs to ground when muted
 *    - 5: 4XGAIN_CH2: Add a 4x gain to channel 2
 *    - 4: 4XGAIN_CH1: Add a 4x gain to channel 1
 *    - 3: INVERT_CH2: Invert channel 2 output
 *    - 2: INVERT_CH1: Invert channel 1 output
 *    - 1: AUTOMUTE_CH2: Enable channel 2 automute
 *    - 0: AUTOMUTE_CH1: Enable channel 1 automute
 *  * CLK_CFG (0x23, 1B):
 *    see ES9039Q2M datasheet for "DAC CLOCK CONFIG" register
 *
 */

#ifndef INC_I2C_DEFINES_HIFIDAC_H_
#define INC_I2C_DEFINES_HIFIDAC_H_


//virtual register sizes in bytes - 0 means register is invalid
#define I2CDEF_HIFIDAC_REG_SIZES {\
  0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,\
  1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  2, 1, 1, 1, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  1, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
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
#define I2CDEF_HIFIDAC_STATUS 0x01

#define I2CDEF_HIFIDAC_STATUS_INIT_DONE_Pos 0
#define I2CDEF_HIFIDAC_STATUS_INIT_DONE_Msk (0x1 << I2CDEF_HIFIDAC_STATUS_INIT_DONE_Pos)
#define I2CDEF_HIFIDAC_STATUS_LOCK_Pos 1
#define I2CDEF_HIFIDAC_STATUS_LOCK_Msk (0x1 << I2CDEF_HIFIDAC_STATUS_LOCK_Pos)
#define I2CDEF_HIFIDAC_STATUS_AUTOMUTE_CH1_Pos 2
#define I2CDEF_HIFIDAC_STATUS_AUTOMUTE_CH1_Msk (0x1 << I2CDEF_HIFIDAC_STATUS_AUTOMUTE_CH1_Pos)
#define I2CDEF_HIFIDAC_STATUS_AUTOMUTE_CH2_Pos 3
#define I2CDEF_HIFIDAC_STATUS_AUTOMUTE_CH2_Msk (0x1 << I2CDEF_HIFIDAC_STATUS_AUTOMUTE_CH2_Pos)
#define I2CDEF_HIFIDAC_STATUS_RAMP_DONE_CH1_Pos 4
#define I2CDEF_HIFIDAC_STATUS_RAMP_DONE_CH1_Msk (0x1 << I2CDEF_HIFIDAC_STATUS_RAMP_DONE_CH1_Pos)
#define I2CDEF_HIFIDAC_STATUS_RAMP_DONE_CH2_Pos 5
#define I2CDEF_HIFIDAC_STATUS_RAMP_DONE_CH2_Msk (0x1 << I2CDEF_HIFIDAC_STATUS_RAMP_DONE_CH2_Pos)
#define I2CDEF_HIFIDAC_STATUS_MONITOR_ERROR_Pos 6
#define I2CDEF_HIFIDAC_STATUS_MONITOR_ERROR_Msk (0x1 << I2CDEF_HIFIDAC_STATUS_MONITOR_ERROR_Pos)
#define I2CDEF_HIFIDAC_STATUS_I2CERR_Pos 8
#define I2CDEF_HIFIDAC_STATUS_I2CERR_Msk (0x1 << I2CDEF_HIFIDAC_STATUS_I2CERR_Pos)

#define I2CDEF_HIFIDAC_CONTROL 0x08

#define I2CDEF_HIFIDAC_CONTROL_INT_EN_Pos 0
#define I2CDEF_HIFIDAC_CONTROL_INT_EN_Msk (0x1 << I2CDEF_HIFIDAC_CONTROL_INT_EN_Pos)
#define I2CDEF_HIFIDAC_CONTROL_DAC_EN_Pos 1
#define I2CDEF_HIFIDAC_CONTROL_DAC_EN_Msk (0x1 << I2CDEF_HIFIDAC_CONTROL_DAC_EN_Pos)
#define I2CDEF_HIFIDAC_CONTROL_SYNC_Pos 2
#define I2CDEF_HIFIDAC_CONTROL_SYNC_Msk (0x1 << I2CDEF_HIFIDAC_CONTROL_SYNC_Pos)
#define I2CDEF_HIFIDAC_CONTROL_MASTER_Pos 3
#define I2CDEF_HIFIDAC_CONTROL_MASTER_Msk (0x1 << I2CDEF_HIFIDAC_CONTROL_MASTER_Pos)
#define I2CDEF_HIFIDAC_CONTROL_RESET_Pos 4
#define I2CDEF_HIFIDAC_CONTROL_RESET_Msk (0xF << I2CDEF_HIFIDAC_CONTROL_RESET_Pos)
#define I2CDEF_HIFIDAC_CONTROL_RESET_VALUE 0xA


//Interrupt registers
#define I2CDEF_HIFIDAC_INT_MASK 0x10

#define I2CDEF_HIFIDAC_INT_MASK_INT_LOCK_Pos 0
#define I2CDEF_HIFIDAC_INT_MASK_INT_LOCK_Msk (0x1 << I2CDEF_HIFIDAC_INT_MASK_INT_LOCK_Pos)
#define I2CDEF_HIFIDAC_INT_MASK_INT_AUTOMUTE_Pos 1
#define I2CDEF_HIFIDAC_INT_MASK_INT_AUTOMUTE_Msk (0x1 << I2CDEF_HIFIDAC_INT_MASK_INT_AUTOMUTE_Pos)
#define I2CDEF_HIFIDAC_INT_MASK_INT_RAMP_Pos 2
#define I2CDEF_HIFIDAC_INT_MASK_INT_RAMP_Msk (0x1 << I2CDEF_HIFIDAC_INT_MASK_INT_RAMP_Pos)
#define I2CDEF_HIFIDAC_INT_MASK_INT_MONITOR_Pos 3
#define I2CDEF_HIFIDAC_INT_MASK_INT_MONITOR_Msk (0x1 << I2CDEF_HIFIDAC_INT_MASK_INT_MONITOR_Pos)

#define I2CDEF_HIFIDAC_INT_FLAGS 0x11

#define I2CDEF_HIFIDAC_INT_FLAGS_INT_LOCK_Pos I2CDEF_HIFIDAC_INT_MASK_INT_LOCK_Pos
#define I2CDEF_HIFIDAC_INT_FLAGS_INT_LOCK_Msk I2CDEF_HIFIDAC_INT_MASK_INT_LOCK_Msk
#define I2CDEF_HIFIDAC_INT_FLAGS_INT_AUTOMUTE_Pos I2CDEF_HIFIDAC_INT_MASK_INT_AUTOMUTE_Pos
#define I2CDEF_HIFIDAC_INT_FLAGS_INT_AUTOMUTE_Msk I2CDEF_HIFIDAC_INT_MASK_INT_AUTOMUTE_Msk
#define I2CDEF_HIFIDAC_INT_FLAGS_INT_RAMP_Pos I2CDEF_HIFIDAC_INT_MASK_INT_RAMP_Pos
#define I2CDEF_HIFIDAC_INT_FLAGS_INT_RAMP_Msk I2CDEF_HIFIDAC_INT_MASK_INT_RAMP_Msk
#define I2CDEF_HIFIDAC_INT_FLAGS_INT_MONITOR_Pos I2CDEF_HIFIDAC_INT_MASK_INT_MONITOR_Pos
#define I2CDEF_HIFIDAC_INT_FLAGS_INT_MONITOR_Msk I2CDEF_HIFIDAC_INT_MASK_INT_MONITOR_Msk


//DAC control registers
#define I2CDEF_HIFIDAC_VOLUME 0x20

#define I2CDEF_HIFIDAC_MUTE 0x21

#define I2CDEF_HIFIDAC_MUTE_MUTE_CH1_Pos 0
#define I2CDEF_HIFIDAC_MUTE_MUTE_CH1_Msk (0x1 << I2CDEF_HIFIDAC_MUTE_MUTE_CH1_Pos)
#define I2CDEF_HIFIDAC_MUTE_MUTE_CH2_Pos 1
#define I2CDEF_HIFIDAC_MUTE_MUTE_CH2_Msk (0x1 << I2CDEF_HIFIDAC_MUTE_MUTE_CH2_Pos)

#define I2CDEF_HIFIDAC_PATH 0x22

#define I2CDEF_HIFIDAC_PATH_AUTOMUTE_CH1_Pos 0
#define I2CDEF_HIFIDAC_PATH_AUTOMUTE_CH1_Msk (0x1 << I2CDEF_HIFIDAC_PATH_AUTOMUTE_CH1_Pos)
#define I2CDEF_HIFIDAC_PATH_AUTOMUTE_CH2_Pos 1
#define I2CDEF_HIFIDAC_PATH_AUTOMUTE_CH2_Msk (0x1 << I2CDEF_HIFIDAC_PATH_AUTOMUTE_CH2_Pos)
#define I2CDEF_HIFIDAC_PATH_INVERT_CH1_Pos 2
#define I2CDEF_HIFIDAC_PATH_INVERT_CH1_Msk (0x1 << I2CDEF_HIFIDAC_PATH_INVERT_CH1_Pos)
#define I2CDEF_HIFIDAC_PATH_INVERT_CH2_Pos 3
#define I2CDEF_HIFIDAC_PATH_INVERT_CH2_Msk (0x1 << I2CDEF_HIFIDAC_PATH_INVERT_CH2_Pos)
#define I2CDEF_HIFIDAC_PATH_4XGAIN_CH1_Pos 4
#define I2CDEF_HIFIDAC_PATH_4XGAIN_CH1_Msk (0x1 << I2CDEF_HIFIDAC_PATH_4XGAIN_CH1_Pos)
#define I2CDEF_HIFIDAC_PATH_4XGAIN_CH2_Pos 5
#define I2CDEF_HIFIDAC_PATH_4XGAIN_CH2_Msk (0x1 << I2CDEF_HIFIDAC_PATH_4XGAIN_CH2_Pos)
#define I2CDEF_HIFIDAC_PATH_MUTE_GND_RAMP_Pos 6
#define I2CDEF_HIFIDAC_PATH_MUTE_GND_RAMP_Msk (0x1 << I2CDEF_HIFIDAC_PATH_MUTE_GND_RAMP_Pos)

#define I2CDEF_HIFIDAC_CLK_CFG 0x23

#define I2CDEF_HIFIDAC_CLK_CFG_SELECT_IDAC_NUM_Pos 0
#define I2CDEF_HIFIDAC_CLK_CFG_SELECT_IDAC_NUM_Msk (0x3F << I2CDEF_HIFIDAC_CLK_CFG_SELECT_IDAC_NUM_Pos)
#define I2CDEF_HIFIDAC_CLK_CFG_SELECT_IDAC_HALF_Pos 6
#define I2CDEF_HIFIDAC_CLK_CFG_SELECT_IDAC_HALF_Msk (0x1 << I2CDEF_HIFIDAC_CLK_CFG_SELECT_IDAC_HALF_Pos)
#define I2CDEF_HIFIDAC_CLK_CFG_AUTO_FS_DETECT_Pos 7
#define I2CDEF_HIFIDAC_CLK_CFG_AUTO_FS_DETECT_Msk (0x1 << I2CDEF_HIFIDAC_CLK_CFG_AUTO_FS_DETECT_Pos)

#define I2CDEF_HIFIDAC_MASTER_DIV 0x24

#define I2CDEF_HIFIDAC_TDM_SLOT_NUM 0x25

#define I2CDEF_HIFIDAC_CH_SLOTS 0x26


//Calibration registers
#define I2CDEF_HIFIDAC_FILTER_SHAPE 0x30

#define I2CDEF_HIFIDAC_THD_C2 0x31

#define I2CDEF_HIFIDAC_THD_C3 0x32


//Misc registers
#define I2CDEF_HIFIDAC_MODULE_ID 0xFF
#define I2CDEF_HIFIDAC_MODULE_ID_VALUE 0x8D


#endif /* INC_I2C_DEFINES_HIFIDAC_H_ */
