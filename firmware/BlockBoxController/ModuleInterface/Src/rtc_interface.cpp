/*
 * rtc_interface.cpp
 *
 *  Created on: Aug 9, 2025
 *      Author: Alex
 */


#include "rtc_interface.h"
#include "system.h"


static const char* const rtc_month_names[12] = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };
static const char* const rtc_weekday_names[7] = { "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday" };

static uint8_t rtc_scratch[19];


uint8_t RTCInterface::GetMonthLength(uint8_t month, uint16_t year) {
  switch (month) {
    case 1:
    case 3:
    case 5:
    case 7:
    case 8:
    case 10:
    case 12:
      return 31;
    case 4:
    case 6:
    case 9:
    case 11:
      return 30;
    case 2:
      return (year % 400 == 0 || (year % 4 == 0 && year % 100 != 0)) ? 29 : 28;
    default:
      throw std::invalid_argument("RTCInterface GetMonthLength given invalid month, must be 1-12");
  }
}

uint8_t RTCInterface::GetWeekday(uint8_t day, uint8_t month, uint16_t year) {
  //Gauss's algorithm for weekday calculation - https://en.wikipedia.org/wiki/Determination_of_the_day_of_the_week#Gauss's_algorithm
  //Output modified for range 1-7, with 1 being Monday

  const uint8_t lut_common[12] = { 0, 3, 3, 6, 1, 4, 6, 2, 5, 0, 3, 5 };
  const uint8_t lut_leap[12] = { 0, 3, 4, 0, 2, 5, 0, 3, 6, 1, 4, 6 };

  //check for validity of inputs
  if (year < 1 || month < 1 || month > 12 || day < 1 || day > RTCInterface::GetMonthLength(month, year)) {
    throw std::invalid_argument("RTCInterface GetWeekday given invalid date");
  }

  uint8_t month_offset = (year % 400 == 0 || (year % 4 == 0 && year % 100 != 0)) ? lut_leap[month - 1] : lut_common[month - 1];
  uint16_t yearm1 = year - 1;

  return (day + month_offset + 5 * (yearm1 % 4) + 4 * (yearm1 % 100) + 6 * (yearm1 % 400) - 1) % 7 + 1;
}


const char* RTCInterface::GetMonthName(uint8_t month) {
  if (month < 1 || month > 12) {
    throw std::invalid_argument("RTCInterface GetMonthName given invalid month, must be 1-12");
  }

  return rtc_month_names[month - 1];
}

const char* RTCInterface::GetWeekdayName(uint8_t weekday) {
  if (weekday < 1 || weekday > 7) {
    throw std::invalid_argument("RTCInterface GetWeekdayName given invalid weekday, must be 1-7");
  }

  return rtc_weekday_names[weekday - 1];
}



RTCStatus RTCInterface::GetStatus() const {
  RTCStatus status;
  status.value = this->registers.Reg8(I2CDEF_RTC_STATUS_CTL);
  return status;
}


RTCDateTime RTCInterface::GetDateTime() const {
  return this->current_date_time;
}



void RTCInterface::SetDateTime(RTCDateTime datetime, SuccessCallback&& callback) {
  //check for write lock
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  if (this->write_lock_timer > 0) {
    //locked: failure, report to callback and return
    if (callback) {
      callback(false);
    }
    return;
  }

  //lock write
  this->write_lock_timer = IF_RTC_WRITE_LOCK_TIMEOUT_CYCLES;
  __set_PRIMASK(primask);

  //check for validity of given date and min+sec - weekday ignored, we recalculate that here anyway
  if (datetime.year < 2000 || datetime.year > 2199 || datetime.month < 1 || datetime.month > 12 ||
      datetime.day < 1 || datetime.day > RTCInterface::GetMonthLength(datetime.month, datetime.year) ||
      datetime.minutes > 59 || datetime.seconds > 59) {
    throw std::invalid_argument("RTCInterface SetDateTime given invalid date, minutes, or seconds");
  }

  //check for validity of hour mode and hours
  switch (datetime.hour_mode) {
    case IF_RTC_24H:
      if (datetime.hours > 23) {
        throw std::invalid_argument("RTCInterface SetDateTime given 24h time with invalid hour");
      }
      break;
    case IF_RTC_12H_AM:
    case IF_RTC_12H_PM:
      if (datetime.hours < 1 || datetime.hours > 12) {
        throw std::invalid_argument("RTCInterface SetDateTime given 12h time with invalid hour");
      }
      break;
    default:
      throw std::invalid_argument("RTCInterface SetDateTime given invalid hour mode");
  }

  //all valid: calculate weekday
  datetime.weekday = RTCInterface::GetWeekday(datetime.day, datetime.month, datetime.year);

  //prepare write buffer
  this->write_buffer[0] = ((datetime.seconds % 10) << I2CDEF_RTC_SECONDS_ONES_Pos) | ((datetime.seconds / 10) << I2CDEF_RTC_SECONDS_TENS_Pos);
  this->write_buffer[1] = ((datetime.minutes % 10) << I2CDEF_RTC_MINUTES_ONES_Pos) | ((datetime.minutes / 10) << I2CDEF_RTC_MINUTES_TENS_Pos);
  this->write_buffer[2] = ((datetime.hours % 10) << I2CDEF_RTC_HOURS_ONES_Pos) | ((datetime.hours / 10) << I2CDEF_RTC_HOURS_TENS_Pos);
  if (datetime.hour_mode != IF_RTC_24H) {
    this->write_buffer[2] |= I2CDEF_RTC_HOURS_12H_MODE_Msk | ((datetime.hour_mode == IF_RTC_12H_PM) ? I2CDEF_RTC_HOURS_20H_PM_Msk : 0);
  }
  this->write_buffer[3] = datetime.weekday;
  this->write_buffer[4] = ((datetime.day % 10) << I2CDEF_RTC_DAY_ONES_Pos) | ((datetime.day / 10) << I2CDEF_RTC_DAY_TENS_Pos);
  this->write_buffer[5] = ((datetime.month % 10) << I2CDEF_RTC_MONTH_ONES_Pos) | ((datetime.month / 10) << I2CDEF_RTC_MONTH_TENS_Pos) |
                          ((datetime.year >= 2100) ? I2CDEF_RTC_MONTH_CENTURY_Msk : 0);
  this->write_buffer[6] = ((datetime.year % 10) << I2CDEF_RTC_YEAR_ONES_Pos) | (((datetime.year % 100) / 10) << I2CDEF_RTC_YEAR_TENS_Pos);

  //write data to RTC chip
  this->WriteMultiRegisterAsync(I2CDEF_RTC_SECONDS, this->write_buffer, 7, [this, callback = std::move(callback)](bool success, uint32_t, uint16_t) {
    //once done: unlock write, read back registers (without waiting), and report success to external callback
    this->write_lock_timer = 0;
    this->ReadMultiRegisterAsync(I2CDEF_RTC_SECONDS, rtc_scratch, 7, ModuleTransferCallback());
    if (callback) {
      callback(success);
    }
  });
}



void RTCInterface::InitModule(SuccessCallback&& callback) {
  this->initialised = false;

  //initialise time+date to a reasonable fallback in case of read failure
  this->current_date_time.year = 2000;
  this->current_date_time.month = 1;
  this->current_date_time.day = 1;
  this->current_date_time.weekday = 6;
  this->current_date_time.hour_mode = IF_RTC_24H;
  this->current_date_time.hours = 0;
  this->current_date_time.minutes = 0;
  this->current_date_time.seconds = 0;

  //read all registers once to update registers to their initial values
  this->ReadMultiRegisterAsync(I2CDEF_RTC_SECONDS, rtc_scratch, 19, [this, callback = std::move(callback)](bool, uint32_t, uint16_t) {
    //after last read is done: init completed successfully (even if read failed - that's non-critical)
    this->initialised = true;
    this->write_lock_timer = 0;
    if (callback) {
      callback(true);
    }
  });
}

void RTCInterface::LoopTasks() {
  static uint32_t loop_count = 0;

  if (this->initialised && loop_count++ % 100 == 0) {
    //every 10 cycles (1000ms), read status - no callback needed, we're only reading to update the register
    this->ReadRegister8Async(I2CDEF_RTC_STATUS_CTL, ModuleTransferCallback());

    //save current date+time, read new one, check for second/minute changes once done (for notifications)
    RTCDateTime prev_dt = this->current_date_time;
    this->ReadMultiRegisterAsync(I2CDEF_RTC_SECONDS, rtc_scratch, 7, [this, prev_dt](bool, uint32_t, uint16_t) {
      if (this->current_date_time.seconds != prev_dt.seconds) {
        this->ExecuteCallbacks(MODIF_RTC_EVENT_SECOND_UPDATE);
      }
      if (this->current_date_time.minutes != prev_dt.minutes) {
        this->ExecuteCallbacks(MODIF_RTC_EVENT_MINUTE_UPDATE);
      }
    });
  }

  //allow base handling
  this->RegI2CModuleInterface::LoopTasks();

  //decrement lock timer, under disabled interrupts
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  if (this->write_lock_timer > 0) {
    if (--this->write_lock_timer == 0) {
      //shouldn't really ever happen, it means an unlock somewhere was missed or delayed excessively
      DEBUG_LOG(DEBUG_WARNING, "RTCInterface write lock timed out!");
    }
  }
  __set_PRIMASK(primask);
}



RTCInterface::RTCInterface(I2CHardwareInterface& hw_interface, uint8_t i2c_address) :
    RegI2CModuleInterface(hw_interface, i2c_address, I2CDEF_RTC_REG_SIZES, IF_RTC_USE_CRC), initialised(false), write_lock_timer(0) {}



void RTCInterface::OnRegisterUpdate(uint8_t address) {
  //allow base handling
  this->RegI2CModuleInterface::OnRegisterUpdate(address);

  if (!this->initialised) {
    return;
  }

  switch (address) {
    case I2CDEF_RTC_SECONDS:
    {
      //update seconds
      uint8_t raw_val = this->registers.Reg8(I2CDEF_RTC_SECONDS);
      uint8_t new_val = ((raw_val & I2CDEF_RTC_SECONDS_ONES_Msk) >> I2CDEF_RTC_SECONDS_ONES_Pos) + 10 * ((raw_val & I2CDEF_RTC_SECONDS_TENS_Msk) >> I2CDEF_RTC_SECONDS_TENS_Pos);
      if (new_val < 60) {
        //only update if value is valid
        this->current_date_time.seconds = new_val;
      }
      break;
    }
    case I2CDEF_RTC_MINUTES:
    {
      //update minutes
      uint8_t raw_val = this->registers.Reg8(I2CDEF_RTC_MINUTES);
      uint8_t new_val = ((raw_val & I2CDEF_RTC_MINUTES_ONES_Msk) >> I2CDEF_RTC_MINUTES_ONES_Pos) + 10 * ((raw_val & I2CDEF_RTC_MINUTES_TENS_Msk) >> I2CDEF_RTC_MINUTES_TENS_Pos);
      if (new_val < 60) {
        //only update if value is valid
        this->current_date_time.minutes = new_val;
      }
      break;
    }
    case I2CDEF_RTC_HOURS:
    {
      //update hours
      uint8_t raw_val = this->registers.Reg8(I2CDEF_RTC_HOURS);
      uint8_t new_val = ((raw_val & I2CDEF_RTC_HOURS_ONES_Msk) >> I2CDEF_RTC_HOURS_ONES_Pos) + 10 * ((raw_val & I2CDEF_RTC_HOURS_TENS_Msk) >> I2CDEF_RTC_HOURS_TENS_Pos);
      if ((raw_val & I2CDEF_RTC_HOURS_12H_MODE_Msk) != 0) {
        //12h mode
        if (new_val >= 1 && new_val <= 12) {
          //only update if value is valid
          this->current_date_time.hours = new_val;
          this->current_date_time.hour_mode = ((raw_val & I2CDEF_RTC_HOURS_20H_PM_Msk) == 0) ? IF_RTC_12H_AM : IF_RTC_12H_PM;
        }
      } else {
        //24h mode
        if ((raw_val & I2CDEF_RTC_HOURS_20H_PM_Msk) != 0) {
          new_val += 20;
        }
        if (new_val < 24) {
          //only update if value is valid
          this->current_date_time.hours = new_val;
          this->current_date_time.hour_mode = IF_RTC_24H;
        }
      }
      break;
    }
    case I2CDEF_RTC_WEEKDAY:
    {
      //update weekday
      uint8_t raw_val = this->registers.Reg8(I2CDEF_RTC_WEEKDAY);
      if (raw_val >= 1 && raw_val <= 7) {
        //only update if value is valid
        this->current_date_time.weekday = raw_val;
      }
      break;
    }
    case I2CDEF_RTC_DAY:
    {
      //update date day
      uint8_t raw_val = this->registers.Reg8(I2CDEF_RTC_DAY);
      uint8_t new_val = ((raw_val & I2CDEF_RTC_DAY_ONES_Msk) >> I2CDEF_RTC_DAY_ONES_Pos) + 10 * ((raw_val & I2CDEF_RTC_DAY_TENS_Msk) >> I2CDEF_RTC_DAY_TENS_Pos);
      if (new_val >= 1 && new_val <= 31) {
        //only update if value is valid
        this->current_date_time.day = new_val;
      }
      break;
    }
    case I2CDEF_RTC_MONTH:
    {
      //update month + century
      uint8_t raw_val = this->registers.Reg8(I2CDEF_RTC_MONTH);
      uint8_t new_val = ((raw_val & I2CDEF_RTC_MONTH_ONES_Msk) >> I2CDEF_RTC_MONTH_ONES_Pos) + 10 * ((raw_val & I2CDEF_RTC_MONTH_TENS_Msk) >> I2CDEF_RTC_MONTH_TENS_Pos);
      if (new_val >= 1 && new_val <= 12) {
        //only update if value is valid
        this->current_date_time.month = new_val;
        this->current_date_time.year = (this->current_date_time.year % 100) + ((raw_val & I2CDEF_RTC_MONTH_CENTURY_Msk) == 0) ? 2000 : 2100;
      }
      break;
    }
    case I2CDEF_RTC_YEAR:
    {
      //update year
      uint8_t raw_val = this->registers.Reg8(I2CDEF_RTC_YEAR);
      uint8_t new_val = ((raw_val & I2CDEF_RTC_YEAR_ONES_Msk) >> I2CDEF_RTC_YEAR_ONES_Pos) + 10 * ((raw_val & I2CDEF_RTC_YEAR_TENS_Msk) >> I2CDEF_RTC_YEAR_TENS_Pos);
      if (new_val < 100) {
        //only update if value is valid
        this->current_date_time.year = (this->current_date_time.year / 100) * 100 + new_val;
      }
      break;
    }
    case I2CDEF_RTC_STATUS_CTL:
      if (this->GetStatus().oscillator_stopped) {
        DEBUG_LOG(DEBUG_WARNING, "RTC detected oscillator stop!");
        //clear stopped flag
        this->WriteRegister8Async(I2CDEF_RTC_STATUS_CTL, 0, ModuleTransferCallback());
      }
      this->ExecuteCallbacks(MODIF_RTC_EVENT_STATUS_UPDATE);
      break;
    default:
      break;
  }
}

