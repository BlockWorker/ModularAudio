/*
 * rtc_interface.h
 *
 *  Created on: Aug 9, 2025
 *      Author: Alex
 */

#ifndef INC_RTC_INTERFACE_H_
#define INC_RTC_INTERFACE_H_


#include "cpp_main.h"
#include "module_interface_i2c.h"
#include "i2c_defines_rtc.h"


//whether the RTC interface uses CRC for communication
#define IF_RTC_USE_CRC false

//RTC interface events
#define MODIF_RTC_EVENT_STATUS_UPDATE (1u << 8)
#define MODIF_RTC_EVENT_SECOND_UPDATE (1u << 9)
#define MODIF_RTC_EVENT_MINUTE_UPDATE (1u << 10)

//RTC write lock timeout cycles
#define IF_RTC_WRITE_LOCK_TIMEOUT_CYCLES (200 / MAIN_LOOP_PERIOD_MS)


//RTC status
typedef union {
  struct {
    bool alarm1_flag : 1;
    bool alarm2_flag : 1;
    bool busy : 1;
    int : 4;
    bool oscillator_stopped : 1;
  };
  uint8_t value;
} RTCStatus;

//RTC hour mode
typedef enum {
  IF_RTC_24H,
  IF_RTC_12H_AM,
  IF_RTC_12H_PM
} RTCHourMode;

//RTC date+time
typedef struct {
  uint8_t seconds;
  uint8_t minutes;
  uint8_t hours;
  RTCHourMode hour_mode;

  uint8_t weekday;
  uint8_t day;
  uint8_t month;
  uint16_t year;
} RTCDateTime;


#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
}


class RTCInterface : public RegI2CModuleInterface {
public:
  static uint8_t GetMonthLength(uint8_t month, uint16_t year);
  static uint8_t GetWeekday(uint8_t day, uint8_t month, uint16_t year);

  static const char* GetMonthName(uint8_t month);
  static const char* GetWeekdayName(uint8_t weekday);


  RTCStatus GetStatus() const;

  RTCDateTime GetDateTime() const;


  void SetDateTime(RTCDateTime datetime, SuccessCallback&& callback);


  //TODO if needed, implement alarms + control + offset + temperature


  void InitModule(SuccessCallback&& callback);
  void LoopTasks() override;


  RTCInterface(I2CHardwareInterface& hw_interface, uint8_t i2c_address);

protected:
  bool initialised;

  RTCDateTime current_date_time;

  uint32_t write_lock_timer;
  uint8_t write_buffer[7];

  void OnRegisterUpdate(uint8_t address) override;
};


#endif


#endif /* INC_RTC_INTERFACE_H_ */
