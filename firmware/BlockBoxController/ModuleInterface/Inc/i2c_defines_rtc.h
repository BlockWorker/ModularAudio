/*
 * i2c_defines_rtc.h
 *
 *  Created on: Aug 9, 2025
 *      Author: Alex
 *
 *  Real-Time Clock I2C register definitions (according to DS3231 data sheet)
 *
 *  Register map:
 *  * Date/Time registers
 *    - 0x00: SECONDS: Time seconds (1B, bit field, rw)
 *    - 0x01: MINUTES: Time minutes (1B, bit field, rw)
 *    - 0x02: HOURS: Time hours (1B, bit field, rw)
 *    - 0x03: WEEKDAY: Weekday index (1B, unsigned 1-7, rw)
 *    - 0x04: DAY: Date day (1B, bit field, rw)
 *    - 0x05: MONTH: Date month+century (1B, bit field, rw)
 *    - 0x06: YEAR: Date year (1B, bit field, rw)
 *  * Alarm registers
 *    - 0x07: A1SEC: Alarm 1 seconds (1B, bit field, rw)
 *    - 0x08: A1MIN: Alarm 1 minutes (1B, bit field, rw)
 *    - 0x09: A1HOUR: Alarm 1 hours (1B, bit field, rw)
 *    - 0x0A: A1DAY: Alarm 1 days (1B, bit field, rw)
 *    - 0x0B: A2MIN: Alarm 2 minutes (1B, bit field, rw)
 *    - 0x0C: A2HOUR: Alarm 2 hours (1B, bit field, rw)
 *    - 0x0D: A2DAY: Alarm 2 days (1B, bit field, rw)
 *  * Control+Status registers
 *    - 0x0E: CONTROL: General control (1B, bit field, rw)
 *    - 0x0F: STATUS_CTL: Status + more control (1B, bit field, rw)
 *    - 0x10: AGING_OFFSET: Aging offset (1B, signed, rw)
 *    - 0x11: TEMP_MSB: Temperature whole degrees (1B, signed, r)
 *    - 0x12: TEMP_LSB: Temperature fractional degrees (1B, unsigned fractional 0.8, r)
 *
 *  Bit field and enum definitions:
 *  * SECONDS (0x00, bit field, 1B):
 *    - 6-4: TENS
 *    - 3-0: ONES
 *  * MINUTES (0x01, bit field, 1B):
 *    - 6-4: TENS
 *    - 3-0: ONES
 *  * HOURS (0x02, bit field, 1B):
 *    - 6: 12H_MODE
 *    - 5: 20H_PM: If 12Hr: PM; else +20 hours
 *    - 4: TENS
 *    - 3-0: ONES
 *  * DAY (0x04, bit field, 1B):
 *    - 5-4: TENS
 *    - 3-0: ONES
 *  * MONTH (0x05, bit field, 1B):
 *    - 7: CENTURY: 1 = 2100s, 0 = 2000s
 *    - 4: TENS
 *    - 3-0: ONES
 *  * YEAR (0x06, bit field, 1B):
 *    - 7-4: TENS
 *    - 3-0: ONES
 *  * A1SEC (0x07, bit field, 1B):
 *    - 7: M1: Mode bit 1
 *    - 6-4: TENS
 *    - 3-0: ONES
 *  * A1MIN/A2MIN (0x08/0x0B, bit field, 1B):
 *    - 7: M2: Mode bit 2
 *    - 6-4: TENS
 *    - 3-0: ONES
 *  * A1HOUR/A2HOUR (0x09/0x0C, bit field, 1B):
 *    - 7: M3: Mode bit 3
 *    - 6: 12H_MODE
 *    - 5: 20H_PM: If 12Hr: PM; else +20 hours
 *    - 4: TENS
 *    - 3-0: ONES
 *  * A1DAY/A2DAY (0x0A/0x0D, bit field, 1B):
 *    - 7: M4: Mode bit 4
 *    - 6: WEEKDAY: 1 = use weekday, 0 = use date day
 *    - 5-4: TENS: Only used for date day
 *    - 3-0: ONES
 *  * CONTROL (0x0E, bit field, 1B):
 *    - 7: EOSC_N: Oscillator disabled when 1
 *    - 6: BBSQW: Square wave keeps running on battery power
 *    - 5: CONV: Force temperature conversion
 *    - 4-3: RS: Square wave rate select
 *    - 2: INTCN: Enable interrupts (instead of square wave)
 *    - 1: A2IE: Alarm 2 interrupt enable
 *    - 0: A1IE: Alarm 1 interrupt enable
 *  * STATUS_CTL (0x0F, bit field, 1B):
 *    - 7: OSF: Oscillator has stopped at some point (read/clear bit)
 *    - 3: EN32KHZ: Enable 32kHz square wave output
 *    - 2: BSY: Busy with temperature conversion (read-only bit)
 *    - 1: A2F: Alarm 2 interrupt flag (read/clear bit)
 *    - 0: A1F: Alarm 1 interrupt flag (read/clear bit)
 *
 */

#ifndef INC_I2C_DEFINES_RTC_H_
#define INC_I2C_DEFINES_RTC_H_


//virtual register sizes in bytes - 0 means register is invalid
#define I2CDEF_RTC_REG_SIZES {\
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,\
  1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
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
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }


//Date/Time registers
#define I2CDEF_RTC_SECONDS 0x00

#define I2CDEF_RTC_SECONDS_ONES_Pos 0
#define I2CDEF_RTC_SECONDS_ONES_Msk (0xF << I2CDEF_RTC_SECONDS_ONES_Pos)
#define I2CDEF_RTC_SECONDS_TENS_Pos 4
#define I2CDEF_RTC_SECONDS_TENS_Msk (0x7 << I2CDEF_RTC_SECONDS_TENS_Pos)

#define I2CDEF_RTC_MINUTES 0x01

#define I2CDEF_RTC_MINUTES_ONES_Pos 0
#define I2CDEF_RTC_MINUTES_ONES_Msk (0xF << I2CDEF_RTC_MINUTES_ONES_Pos)
#define I2CDEF_RTC_MINUTES_TENS_Pos 4
#define I2CDEF_RTC_MINUTES_TENS_Msk (0x7 << I2CDEF_RTC_MINUTES_TENS_Pos)

#define I2CDEF_RTC_HOURS 0x02

#define I2CDEF_RTC_HOURS_ONES_Pos 0
#define I2CDEF_RTC_HOURS_ONES_Msk (0xF << I2CDEF_RTC_HOURS_ONES_Pos)
#define I2CDEF_RTC_HOURS_TENS_Pos 4
#define I2CDEF_RTC_HOURS_TENS_Msk (0x1 << I2CDEF_RTC_HOURS_TENS_Pos)
#define I2CDEF_RTC_HOURS_20H_PM_Pos 5
#define I2CDEF_RTC_HOURS_20H_PM_Msk (0x1 << I2CDEF_RTC_HOURS_20H_PM_Pos)
#define I2CDEF_RTC_HOURS_12H_MODE_Pos 6
#define I2CDEF_RTC_HOURS_12H_MODE_Msk (0x1 << I2CDEF_RTC_HOURS_12H_MODE_Pos)

#define I2CDEF_RTC_WEEKDAY 0x03

#define I2CDEF_RTC_DAY 0x04

#define I2CDEF_RTC_DAY_ONES_Pos 0
#define I2CDEF_RTC_DAY_ONES_Msk (0xF << I2CDEF_RTC_DAY_ONES_Pos)
#define I2CDEF_RTC_DAY_TENS_Pos 4
#define I2CDEF_RTC_DAY_TENS_Msk (0x3 << I2CDEF_RTC_DAY_TENS_Pos)

#define I2CDEF_RTC_MONTH 0x05

#define I2CDEF_RTC_MONTH_ONES_Pos 0
#define I2CDEF_RTC_MONTH_ONES_Msk (0xF << I2CDEF_RTC_MONTH_ONES_Pos)
#define I2CDEF_RTC_MONTH_TENS_Pos 4
#define I2CDEF_RTC_MONTH_TENS_Msk (0x1 << I2CDEF_RTC_MONTH_TENS_Pos)
#define I2CDEF_RTC_MONTH_CENTURY_Pos 7
#define I2CDEF_RTC_MONTH_CENTURY_Msk (0x1 << I2CDEF_RTC_MONTH_CENTURY_Pos)

#define I2CDEF_RTC_YEAR 0x06

#define I2CDEF_RTC_YEAR_ONES_Pos 0
#define I2CDEF_RTC_YEAR_ONES_Msk (0xF << I2CDEF_RTC_YEAR_ONES_Pos)
#define I2CDEF_RTC_YEAR_TENS_Pos 4
#define I2CDEF_RTC_YEAR_TENS_Msk (0xF << I2CDEF_RTC_YEAR_TENS_Pos)

#define I2CDEF_RTC_A1SEC 0x07

#define I2CDEF_RTC_A1SEC_ONES_Pos I2CDEF_RTC_SECONDS_ONES_Pos
#define I2CDEF_RTC_A1SEC_ONES_Msk I2CDEF_RTC_SECONDS_ONES_Msk
#define I2CDEF_RTC_A1SEC_TENS_Pos I2CDEF_RTC_SECONDS_TENS_Pos
#define I2CDEF_RTC_A1SEC_TENS_Msk I2CDEF_RTC_SECONDS_TENS_Msk
#define I2CDEF_RTC_A1SEC_M1_Pos 7
#define I2CDEF_RTC_A1SEC_M1_Msk (0x1 << I2CDEF_RTC_A1SEC_M1_Pos)

#define I2CDEF_RTC_A1MIN 0x08

#define I2CDEF_RTC_A1MIN_ONES_Pos I2CDEF_RTC_MINUTES_ONES_Pos
#define I2CDEF_RTC_A1MIN_ONES_Msk I2CDEF_RTC_MINUTES_ONES_Msk
#define I2CDEF_RTC_A1MIN_TENS_Pos I2CDEF_RTC_MINUTES_TENS_Pos
#define I2CDEF_RTC_A1MIN_TENS_Msk I2CDEF_RTC_MINUTES_TENS_Msk
#define I2CDEF_RTC_A1MIN_M2_Pos 7
#define I2CDEF_RTC_A1MIN_M2_Msk (0x1 << I2CDEF_RTC_A1MIN_M2_Pos)

#define I2CDEF_RTC_A1HOUR 0x09

#define I2CDEF_RTC_A1HOUR_ONES_Pos I2CDEF_RTC_HOURS_ONES_Pos
#define I2CDEF_RTC_A1HOUR_ONES_Msk I2CDEF_RTC_HOURS_ONES_Msk
#define I2CDEF_RTC_A1HOUR_TENS_Pos I2CDEF_RTC_HOURS_TENS_Pos
#define I2CDEF_RTC_A1HOUR_TENS_Msk I2CDEF_RTC_HOURS_TENS_Msk
#define I2CDEF_RTC_A1HOUR_20H_PM_Pos I2CDEF_RTC_HOURS_20H_PM_Pos
#define I2CDEF_RTC_A1HOUR_20H_PM_Msk I2CDEF_RTC_HOURS_20H_PM_Msk
#define I2CDEF_RTC_A1HOUR_12H_MODE_Pos I2CDEF_RTC_HOURS_12H_MODE_Pos
#define I2CDEF_RTC_A1HOUR_12H_MODE_Msk I2CDEF_RTC_HOURS_12H_MODE_Msk
#define I2CDEF_RTC_A1HOUR_M3_Pos 7
#define I2CDEF_RTC_A1HOUR_M3_Msk (0x1 << I2CDEF_RTC_A1HOUR_M3_Pos)

#define I2CDEF_RTC_A1DAY 0x0A

#define I2CDEF_RTC_A1DAY_ONES_Pos I2CDEF_RTC_DAY_ONES_Pos
#define I2CDEF_RTC_A1DAY_ONES_Msk I2CDEF_RTC_DAY_ONES_Msk
#define I2CDEF_RTC_A1DAY_TENS_Pos I2CDEF_RTC_DAY_TENS_Pos
#define I2CDEF_RTC_A1DAY_TENS_Msk I2CDEF_RTC_DAY_TENS_Msk
#define I2CDEF_RTC_A1DAY_WEEKDAY_Pos 6
#define I2CDEF_RTC_A1DAY_WEEKDAY_Msk (0x1 << I2CDEF_RTC_A1DAY_WEEKDAY_Pos)
#define I2CDEF_RTC_A1DAY_M4_Pos 7
#define I2CDEF_RTC_A1DAY_M4_Msk (0x1 << I2CDEF_RTC_A1DAY_M4_Pos)

#define I2CDEF_RTC_A2MIN 0x0B

#define I2CDEF_RTC_A2MIN_ONES_Pos I2CDEF_RTC_A1MIN_ONES_Pos
#define I2CDEF_RTC_A2MIN_ONES_Msk I2CDEF_RTC_A1MIN_ONES_Msk
#define I2CDEF_RTC_A2MIN_TENS_Pos I2CDEF_RTC_A1MIN_TENS_Pos
#define I2CDEF_RTC_A2MIN_TENS_Msk I2CDEF_RTC_A1MIN_TENS_Msk
#define I2CDEF_RTC_A2MIN_M2_Pos I2CDEF_RTC_A1MIN_M2_Pos
#define I2CDEF_RTC_A2MIN_M2_Msk I2CDEF_RTC_A1MIN_M2_Msk

#define I2CDEF_RTC_A2HOUR 0x0C

#define I2CDEF_RTC_A2HOUR_ONES_Pos I2CDEF_RTC_A1HOUR_ONES_Pos
#define I2CDEF_RTC_A2HOUR_ONES_Msk I2CDEF_RTC_A1HOUR_ONES_Msk
#define I2CDEF_RTC_A2HOUR_TENS_Pos I2CDEF_RTC_A1HOUR_TENS_Pos
#define I2CDEF_RTC_A2HOUR_TENS_Msk I2CDEF_RTC_A1HOUR_TENS_Msk
#define I2CDEF_RTC_A2HOUR_20H_PM_Pos I2CDEF_RTC_A1HOUR_20H_PM_Pos
#define I2CDEF_RTC_A2HOUR_20H_PM_Msk I2CDEF_RTC_A1HOUR_20H_PM_Msk
#define I2CDEF_RTC_A2HOUR_12H_MODE_Pos I2CDEF_RTC_A1HOUR_12H_MODE_Pos
#define I2CDEF_RTC_A2HOUR_12H_MODE_Msk I2CDEF_RTC_A1HOUR_12H_MODE_Msk
#define I2CDEF_RTC_A2HOUR_M3_Pos I2CDEF_RTC_A1HOUR_M3_Pos
#define I2CDEF_RTC_A2HOUR_M3_Msk I2CDEF_RTC_A1HOUR_M3_Msk

#define I2CDEF_RTC_A2DAY 0x0D

#define I2CDEF_RTC_A2DAY_ONES_Pos I2CDEF_RTC_A1DAY_ONES_Pos
#define I2CDEF_RTC_A2DAY_ONES_Msk I2CDEF_RTC_A1DAY_ONES_Msk
#define I2CDEF_RTC_A2DAY_TENS_Pos I2CDEF_RTC_A1DAY_TENS_Pos
#define I2CDEF_RTC_A2DAY_TENS_Msk I2CDEF_RTC_A1DAY_TENS_Msk
#define I2CDEF_RTC_A2DAY_WEEKDAY_Pos I2CDEF_RTC_A1DAY_WEEKDAY_Pos
#define I2CDEF_RTC_A2DAY_WEEKDAY_Msk I2CDEF_RTC_A1DAY_WEEKDAY_Msk
#define I2CDEF_RTC_A2DAY_M4_Pos I2CDEF_RTC_A1DAY_M4_Pos
#define I2CDEF_RTC_A2DAY_M4_Msk I2CDEF_RTC_A1DAY_M4_Msk

#define I2CDEF_RTC_CONTROL 0x0E

#define I2CDEF_RTC_CONTROL_A1IE_Pos 0
#define I2CDEF_RTC_CONTROL_A1IE_Msk (0x1 << I2CDEF_RTC_CONTROL_A1IE_Pos)
#define I2CDEF_RTC_CONTROL_A2IE_Pos 1
#define I2CDEF_RTC_CONTROL_A2IE_Msk (0x1 << I2CDEF_RTC_CONTROL_A2IE_Pos)
#define I2CDEF_RTC_CONTROL_INTCN_Pos 2
#define I2CDEF_RTC_CONTROL_INTCN_Msk (0x1 << I2CDEF_RTC_CONTROL_INTCN_Pos)
#define I2CDEF_RTC_CONTROL_RS_Pos 3
#define I2CDEF_RTC_CONTROL_RS_Msk (0x3 << I2CDEF_RTC_CONTROL_RS_Pos)
#define I2CDEF_RTC_CONTROL_CONV_Pos 5
#define I2CDEF_RTC_CONTROL_CONV_Msk (0x1 << I2CDEF_RTC_CONTROL_CONV_Pos)
#define I2CDEF_RTC_CONTROL_BBSQW_Pos 6
#define I2CDEF_RTC_CONTROL_BBSQW_Msk (0x1 << I2CDEF_RTC_CONTROL_BBSQW_Pos)
#define I2CDEF_RTC_CONTROL_EOSC_N_Pos 7
#define I2CDEF_RTC_CONTROL_EOSC_N_Msk (0x1 << I2CDEF_RTC_CONTROL_EOSC_N_Pos)

#define I2CDEF_RTC_STATUS_CTL 0x0F

#define I2CDEF_RTC_STATUS_CTL_A1F_Pos 0
#define I2CDEF_RTC_STATUS_CTL_A1F_Msk (0x1 << I2CDEF_RTC_STATUS_CTL_A1F_Pos)
#define I2CDEF_RTC_STATUS_CTL_A2F_Pos 1
#define I2CDEF_RTC_STATUS_CTL_A2F_Msk (0x1 << I2CDEF_RTC_STATUS_CTL_A2F_Pos)
#define I2CDEF_RTC_STATUS_CTL_BSY_Pos 2
#define I2CDEF_RTC_STATUS_CTL_BSY_Msk (0x1 << I2CDEF_RTC_STATUS_CTL_BSY_Pos)
#define I2CDEF_RTC_STATUS_CTL_EN32KHZ_Pos 3
#define I2CDEF_RTC_STATUS_CTL_EN32KHZ_Msk (0x1 << I2CDEF_RTC_STATUS_CTL_EN32KHZ_Pos)
#define I2CDEF_RTC_STATUS_CTL_OSF_Pos 7
#define I2CDEF_RTC_STATUS_CTL_OSF_Msk (0x1 << I2CDEF_RTC_STATUS_CTL_OSF_Pos)

#define I2CDEF_RTC_AGING_OFFSET 0x10

#define I2CDEF_RTC_TEMP_MSB 0x11

#define I2CDEF_RTC_TEMP_LSB 0x12




#endif /* INC_I2C_DEFINES_RTC_H_ */
