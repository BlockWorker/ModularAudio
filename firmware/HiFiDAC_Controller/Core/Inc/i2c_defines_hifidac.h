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
 *
 *  * Misc registers
 *    - 0xFF: MODULE_ID: Module ID (1B, hex, r)
 *
 *  Bit field definitions:
 *  * STATUS (0x01, 2B):
 *    - 8: I2CERR: I2C communication error detected since last STATUS read
 *    - 7: SWARN: safety warning active (any)
 *    - 6: PVDD_OLIM: PVDD offset at limit
 *    - 5: PVDD_ONZ: PVDD offset nonzero
 *    - 4: PVDD_RED: PVDD voltage being reduced
 *    - 3: PVDD_VALID: PVDD voltage valid
 *    - 2: AMP_SD: amplifier shutdown active (set by this controller, external shutdown is possible through header and will not be detected)
 *    - 1: AMP_CLIPOTW: amplifier clip/otw active
 *    - 0: AMP_FAULT: amplifier fault active
 *  * CONTROL (0x08, 1B):
 *    - 4-7: RESET: controller reset (write 0xA to trigger software reset)
 *    - 1: INT_EN: enable interrupts
 *    - 0: AMP_MAN_SD: activate manual amp shutdown
 *  * INT_MASK (0x10, 1B):
 *    - 6: INT_PVDD_OLIM: PVDD offset at limit
 *    - 5: INT_PVDD_REDDONE: PVDD voltage reduction completed
 *    - 4: INT_PVDD_ERR: PVDD voltage error
 *    - 3: INT_SWARN: safety warning
 *    - 2: INT_SERR: safety error shutdown
 *    - 1: INT_AMP_CLIPOTW: amplifier clip/otw
 *    - 0: INT_AMP_FAULT: amplifier fault
 *  * INT_FLAGS (0x11, 1B):
 *    same layout as INT_MASK
 */

#ifndef INC_I2C_DEFINES_HIFIDAC_H_
#define INC_I2C_DEFINES_HIFIDAC_H_


//virtual register sizes in bytes - 0 means register is invalid
#define I2CDEF_HIFIDAC_REG_SIZES {\
  0, 2, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,\
  1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,\
  4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,\
  4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0,\
  4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0,\
  4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0,\
  4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0,\
  4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0,\
  4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0,\
  1, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 }


//General registers
#define I2CDEF_HIFIDAC_STATUS 0x01

#define I2CDEF_HIFIDAC_STATUS_AMP_FAULT_Pos 0
#define I2CDEF_HIFIDAC_STATUS_AMP_FAULT_Msk (0x1 << I2CDEF_HIFIDAC_STATUS_AMP_FAULT_Pos)
#define I2CDEF_HIFIDAC_STATUS_AMP_CLIPOTW_Pos 1
#define I2CDEF_HIFIDAC_STATUS_AMP_CLIPOTW_Msk (0x1 << I2CDEF_HIFIDAC_STATUS_AMP_CLIPOTW_Pos)
#define I2CDEF_HIFIDAC_STATUS_AMP_SD_Pos 2
#define I2CDEF_HIFIDAC_STATUS_AMP_SD_Msk (0x1 << I2CDEF_HIFIDAC_STATUS_AMP_SD_Pos)
#define I2CDEF_HIFIDAC_STATUS_PVDD_VALID_Pos 3
#define I2CDEF_HIFIDAC_STATUS_PVDD_VALID_Msk (0x1 << I2CDEF_HIFIDAC_STATUS_PVDD_VALID_Pos)
#define I2CDEF_HIFIDAC_STATUS_PVDD_RED_Pos 4
#define I2CDEF_HIFIDAC_STATUS_PVDD_RED_Msk (0x1 << I2CDEF_HIFIDAC_STATUS_PVDD_RED_Pos)
#define I2CDEF_HIFIDAC_STATUS_PVDD_ONZ_Pos 5
#define I2CDEF_HIFIDAC_STATUS_PVDD_ONZ_Msk (0x1 << I2CDEF_HIFIDAC_STATUS_PVDD_ONZ_Pos)
#define I2CDEF_HIFIDAC_STATUS_PVDD_OLIM_Pos 6
#define I2CDEF_HIFIDAC_STATUS_PVDD_OLIM_Msk (0x1 << I2CDEF_HIFIDAC_STATUS_PVDD_OLIM_Pos)
#define I2CDEF_HIFIDAC_STATUS_SWARN_Pos 7
#define I2CDEF_HIFIDAC_STATUS_SWARN_Msk (0x1 << I2CDEF_HIFIDAC_STATUS_SWARN_Pos)
#define I2CDEF_HIFIDAC_STATUS_I2CERR_Pos 8
#define I2CDEF_HIFIDAC_STATUS_I2CERR_Msk (0x1 << I2CDEF_HIFIDAC_STATUS_I2CERR_Pos)

#define I2CDEF_HIFIDAC_CONTROL 0x08

#define I2CDEF_HIFIDAC_CONTROL_AMP_MAN_SD_Pos 0
#define I2CDEF_HIFIDAC_CONTROL_AMP_MAN_SD_Msk (0x1 << I2CDEF_HIFIDAC_CONTROL_AMP_MAN_SD_Pos)
#define I2CDEF_HIFIDAC_CONTROL_INT_EN_Pos 1
#define I2CDEF_HIFIDAC_CONTROL_INT_EN_Msk (0x1 << I2CDEF_HIFIDAC_CONTROL_INT_EN_Pos)
#define I2CDEF_HIFIDAC_CONTROL_RESET_Pos 4
#define I2CDEF_HIFIDAC_CONTROL_RESET_Msk (0xF << I2CDEF_HIFIDAC_CONTROL_RESET_Pos)
#define I2CDEF_HIFIDAC_CONTROL_RESET_VALUE 0xA


//Interrupt registers
#define I2CDEF_HIFIDAC_INT_MASK 0x10

#define I2CDEF_HIFIDAC_INT_MASK_INT_AMP_FAULT_Pos 0
#define I2CDEF_HIFIDAC_INT_MASK_INT_AMP_FAULT_Msk (0x1 << I2CDEF_HIFIDAC_INT_MASK_INT_AMP_FAULT_Pos)
#define I2CDEF_HIFIDAC_INT_MASK_INT_AMP_CLIPOTW_Pos 1
#define I2CDEF_HIFIDAC_INT_MASK_INT_AMP_CLIPOTW_Msk (0x1 << I2CDEF_HIFIDAC_INT_MASK_INT_AMP_CLIPOTW_Pos)
#define I2CDEF_HIFIDAC_INT_MASK_INT_SERR_Pos 2
#define I2CDEF_HIFIDAC_INT_MASK_INT_SERR_Msk (0x1 << I2CDEF_HIFIDAC_INT_MASK_INT_SERR_Pos)
#define I2CDEF_HIFIDAC_INT_MASK_INT_SWARN_Pos 3
#define I2CDEF_HIFIDAC_INT_MASK_INT_SWARN_Msk (0x1 << I2CDEF_HIFIDAC_INT_MASK_INT_SWARN_Pos)
#define I2CDEF_HIFIDAC_INT_MASK_INT_PVDD_ERR_Pos 4
#define I2CDEF_HIFIDAC_INT_MASK_INT_PVDD_ERR_Msk (0x1 << I2CDEF_HIFIDAC_INT_MASK_INT_PVDD_ERR_Pos)
#define I2CDEF_HIFIDAC_INT_MASK_INT_PVDD_REDDONE_Pos 5
#define I2CDEF_HIFIDAC_INT_MASK_INT_PVDD_REDDONE_Msk (0x1 << I2CDEF_HIFIDAC_INT_MASK_INT_PVDD_REDDONE_Pos)
#define I2CDEF_HIFIDAC_INT_MASK_INT_PVDD_OLIM_Pos 6
#define I2CDEF_HIFIDAC_INT_MASK_INT_PVDD_OLIM_Msk (0x1 << I2CDEF_HIFIDAC_INT_MASK_INT_PVDD_OLIM_Pos)

#define I2CDEF_HIFIDAC_INT_FLAGS 0x11

#define I2CDEF_HIFIDAC_INT_FLAGS_INT_AMP_FAULT_Pos I2CDEF_HIFIDAC_INT_MASK_INT_AMP_FAULT_Pos
#define I2CDEF_HIFIDAC_INT_FLAGS_INT_AMP_FAULT_Msk I2CDEF_HIFIDAC_INT_MASK_INT_AMP_FAULT_Msk
#define I2CDEF_HIFIDAC_INT_FLAGS_INT_AMP_CLIPOTW_Pos I2CDEF_HIFIDAC_INT_MASK_INT_AMP_CLIPOTW_Pos
#define I2CDEF_HIFIDAC_INT_FLAGS_INT_AMP_CLIPOTW_Msk I2CDEF_HIFIDAC_INT_MASK_INT_AMP_CLIPOTW_Msk
#define I2CDEF_HIFIDAC_INT_FLAGS_INT_SERR_Pos I2CDEF_HIFIDAC_INT_MASK_INT_SERR_Pos
#define I2CDEF_HIFIDAC_INT_FLAGS_INT_SERR_Msk I2CDEF_HIFIDAC_INT_MASK_INT_SERR_Msk
#define I2CDEF_HIFIDAC_INT_FLAGS_INT_SWARN_Pos I2CDEF_HIFIDAC_INT_MASK_INT_SWARN_Pos
#define I2CDEF_HIFIDAC_INT_FLAGS_INT_SWARN_Msk I2CDEF_HIFIDAC_INT_MASK_INT_SWARN_Msk
#define I2CDEF_HIFIDAC_INT_FLAGS_INT_PVDD_ERR_Pos I2CDEF_HIFIDAC_INT_MASK_INT_PVDD_ERR_Pos
#define I2CDEF_HIFIDAC_INT_FLAGS_INT_PVDD_ERR_Msk I2CDEF_HIFIDAC_INT_MASK_INT_PVDD_ERR_Msk
#define I2CDEF_HIFIDAC_INT_FLAGS_INT_PVDD_REDDONE_Pos I2CDEF_HIFIDAC_INT_MASK_INT_PVDD_REDDONE_Pos
#define I2CDEF_HIFIDAC_INT_FLAGS_INT_PVDD_REDDONE_Msk I2CDEF_HIFIDAC_INT_MASK_INT_PVDD_REDDONE_Msk
#define I2CDEF_HIFIDAC_INT_FLAGS_INT_PVDD_OLIM_Pos I2CDEF_HIFIDAC_INT_MASK_INT_PVDD_OLIM_Pos
#define I2CDEF_HIFIDAC_INT_FLAGS_INT_PVDD_OLIM_Msk I2CDEF_HIFIDAC_INT_MASK_INT_PVDD_OLIM_Msk


//Misc registers
#define I2CDEF_HIFIDAC_MODULE_ID 0xFF
#define I2CDEF_HIFIDAC_MODULE_ID_VALUE 0x8D


#endif /* INC_I2C_DEFINES_HIFIDAC_H_ */
