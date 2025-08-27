/*
 * i2c_defines_charger.h
 *
 *  Created on: Aug 25, 2025
 *      Author: Alex
 *
 *  Battery charger I2C/SMBus register definitions (according to BQ24735 data sheet)
 *
 *  Register map:
 *  * Main registers
 *    - 0x12: CHG_OPTION: Charger options/config (2B, bit field, rw)
 *    - 0x14: CHG_CURRENT: Fast-charge current (2B, unsigned mA in bits 6-12, rw)
 *    - 0x15: CHG_VOLTAGE: Charge end voltage (2B, unsigned mV in bits 4-14, rw)
 *    - 0x3F: INPUT_CURRENT: Max adapter current target (2B, unsigned x 2.5mA in bits 7-12, rw)
 *  * Constant registers
 *    - 0xFE: MFR_ID: Manufacturer ID (2B, hex, r)
 *    - 0xFF: DEV_ID: Device ID (2B, hex, r)
 *
 *  Bit field and enum definitions:
 *  * CHG_OPTION (0x12, bit field, 2B):
 *    - 15: ACOK_DEG_T: ACOK signal deglitch time adjust (150ms or 1.3s)
 *    - 14-13: WDT: Watchdog timer setting
 *    - 12-11: BAT_DEPL: Depletion comparator threshold adjust
 *    - 10: EMI_FREQ_ADJ: Switching frequency adjustment direction
 *    - 9: EMI_FREQ_EN: Enable switching frequency adjustment
 *    - 8: IFAULT_HI_EN: Enable high-side FET short circuit protection
 *    - 7: IFAULT_LOW_THR: Low-side FET short circuit protection threshold
 *    - 6: LEARN: Enable battery learn cycle
 *    - 5: IOUT_SEL: Selection of current amplifier output (adapter/charge)
 *    - 4: AC_PRES: Adapter present (read-only)
 *    - 3: BOOST_EN: Allow boost mode
 *    - 2: BOOST_ACTIVE: Boost mode active (read-only)
 *    - 1: ACOC_EN: Enable adapter overcurrent protection (3.33x limit)
 *    - 0: CHG_INH: Inhibit charging
 */

#ifndef INC_I2C_DEFINES_CHARGER_H_
#define INC_I2C_DEFINES_CHARGER_H_


//virtual register sizes in bytes - 0 means register is invalid
#define I2CDEF_CHG_REG_SIZES {\
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  0, 0, 2, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2,\
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
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2 }


//Main registers
#define I2CDEF_CHG_CHG_OPTION 0x12

#define I2CDEF_CHG_CHG_OPTION_CHG_INH_Pos 0
#define I2CDEF_CHG_CHG_OPTION_CHG_INH_Msk (0x1 << I2CDEF_CHG_CHG_OPTION_CHG_INH_Pos)
#define I2CDEF_CHG_CHG_OPTION_ACOC_EN_Pos 1
#define I2CDEF_CHG_CHG_OPTION_ACOC_EN_Msk (0x1 << I2CDEF_CHG_CHG_OPTION_ACOC_EN_Pos)
#define I2CDEF_CHG_CHG_OPTION_BOOST_ACTIVE_Pos 2
#define I2CDEF_CHG_CHG_OPTION_BOOST_ACTIVE_Msk (0x1 << I2CDEF_CHG_CHG_OPTION_BOOST_ACTIVE_Pos)
#define I2CDEF_CHG_CHG_OPTION_BOOST_EN_Pos 3
#define I2CDEF_CHG_CHG_OPTION_BOOST_EN_Msk (0x1 << I2CDEF_CHG_CHG_OPTION_BOOST_EN_Pos)
#define I2CDEF_CHG_CHG_OPTION_AC_PRES_Pos 4
#define I2CDEF_CHG_CHG_OPTION_AC_PRES_Msk (0x1 << I2CDEF_CHG_CHG_OPTION_AC_PRES_Pos)
#define I2CDEF_CHG_CHG_OPTION_IOUT_SEL_Pos 5
#define I2CDEF_CHG_CHG_OPTION_IOUT_SEL_Msk (0x1 << I2CDEF_CHG_CHG_OPTION_IOUT_SEL_Pos)
#define I2CDEF_CHG_CHG_OPTION_LEARN_Pos 6
#define I2CDEF_CHG_CHG_OPTION_LEARN_Msk (0x1 << I2CDEF_CHG_CHG_OPTION_LEARN_Pos)
#define I2CDEF_CHG_CHG_OPTION_IFAULT_LOW_THR_Pos 7
#define I2CDEF_CHG_CHG_OPTION_IFAULT_LOW_THR_Msk (0x1 << I2CDEF_CHG_CHG_OPTION_IFAULT_LOW_THR_Pos)
#define I2CDEF_CHG_CHG_OPTION_IFAULT_HI_EN_Pos 8
#define I2CDEF_CHG_CHG_OPTION_IFAULT_HI_EN_Msk (0x1 << I2CDEF_CHG_CHG_OPTION_IFAULT_HI_EN_Pos)
#define I2CDEF_CHG_CHG_OPTION_EMI_FREQ_EN_Pos 9
#define I2CDEF_CHG_CHG_OPTION_EMI_FREQ_EN_Msk (0x1 << I2CDEF_CHG_CHG_OPTION_EMI_FREQ_EN_Pos)
#define I2CDEF_CHG_CHG_OPTION_EMI_FREQ_ADJ_Pos 10
#define I2CDEF_CHG_CHG_OPTION_EMI_FREQ_ADJ_Msk (0x1 << I2CDEF_CHG_CHG_OPTION_EMI_FREQ_ADJ_Pos)
#define I2CDEF_CHG_CHG_OPTION_BAT_DEPL_Pos 11
#define I2CDEF_CHG_CHG_OPTION_BAT_DEPL_Msk (0x3 << I2CDEF_CHG_CHG_OPTION_BAT_DEPL_Pos)
#define I2CDEF_CHG_CHG_OPTION_WDT_Pos 13
#define I2CDEF_CHG_CHG_OPTION_WDT_Msk (0x3 << I2CDEF_CHG_CHG_OPTION_WDT_Pos)
#define I2CDEF_CHG_CHG_OPTION_ACOK_DEG_T_Pos 15
#define I2CDEF_CHG_CHG_OPTION_ACOK_DEG_T_Msk (0x1 << I2CDEF_CHG_CHG_OPTION_ACOK_DEG_T_Pos)

#define I2CDEF_CHG_CHG_CURRENT 0x14

#define I2CDEF_CHG_CHG_CURRENT_MASK 0x1FC0

#define I2CDEF_CHG_CHG_VOLTAGE 0x15

#define I2CDEF_CHG_CHG_VOLTAGE_MASK 0x7FF0

#define I2CDEF_CHG_INPUT_CURRENT 0x3F

#define I2CDEF_CHG_INPUT_CURRENT_MASK 0x1F80

//Constant registers
#define I2CDEF_CHG_MFR_ID 0xFE

#define I2CDEF_CHG_DEV_ID 0xFF
#define I2CDEF_CHG_DEV_ID_VALUE 0x000B


#endif /* INC_I2C_DEFINES_CHARGER_H_ */
