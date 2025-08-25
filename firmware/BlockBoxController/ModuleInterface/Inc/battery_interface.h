/*
 * battery_interface.h
 *
 *  Created on: Aug 24, 2025
 *      Author: Alex
 */

#ifndef INC_BATTERY_INTERFACE_H_
#define INC_BATTERY_INTERFACE_H_


#include "cpp_main.h"
#include "module_interface_uart.h"
#include "../../../BatteryMonitor_Controller/Core/Inc/uart_defines_bms.h"


//whether the battery interface uses CRC for communication
#define IF_BMS_USE_CRC true

//battery interface events
#define MODIF_BMS_EVENT_PRESENCE_UPDATE (1u << 8)
#define MODIF_BMS_EVENT_STATUS_UPDATE (1u << 9)
#define MODIF_BMS_EVENT_VOLTAGE_UPDATE (1u << 10)
#define MODIF_BMS_EVENT_CURRENT_UPDATE (1u << 11)
#define MODIF_BMS_EVENT_SOC_UPDATE (1u << 12)
#define MODIF_BMS_EVENT_HEALTH_UPDATE (1u << 13)
#define MODIF_BMS_EVENT_TEMP_UPDATE (1u << 14)
#define MODIF_BMS_EVENT_SAFETY_UPDATE (1u << 15)
#define MODIF_BMS_EVENT_SHUTDOWN_UPDATE (1u << 16)

//reset timeout, in main loop cycles
#define IF_BMS_RESET_TIMEOUT (500 / MAIN_LOOP_PERIOD_MS)


//BMS specific error types
typedef enum {
  IF_BMS_ERROR_UNKNOWN = IF_UART_ERROR_UNKNOWN,
  IF_BMS_ERROR_BMS_I2C_ERROR = UARTDEF_BMS_ERROR_BMS_I2C_ERROR,
  IF_BMS_ERROR_FLASH_ERROR = UARTDEF_BMS_ERROR_FLASH_ERROR,
  IF_BMS_ERROR_UART_FORMAT_ERROR_REMOTE = IF_UART_ERROR_UART_FORMAT_ERROR_REMOTE,
  IF_BMS_ERROR_INTERNAL_UART_ERROR_REMOTE = IF_UART_ERROR_INTERNAL_UART_ERROR_REMOTE,
  IF_BMS_ERROR_UART_COMMAND_NOT_ALLOWED = IF_UART_ERROR_UART_COMMAND_NOT_ALLOWED,
  IF_BMS_ERROR_UART_CRC_ERROR_REMOTE = IF_UART_ERROR_UART_CRC_ERROR_REMOTE,
  IF_BMS_ERROR_UART_FORMAT_ERROR_LOCAL = IF_UART_ERROR_UART_FORMAT_ERROR_LOCAL,
  IF_BMS_ERROR_UART_CRC_ERROR_LOCAL = IF_UART_ERROR_UART_CRC_ERROR_LOCAL
} BatteryInterfaceErrorType;

//BMS status
typedef union {
  struct {
    bool fault : 1;
    bool alert : 1;
    bool chg_fault : 1;
    bool shutdown : 1;
    int : 1;
    bool bms_i2c_error : 1;
    bool flash_error : 1;
    bool uart_error : 1;
  };
  uint8_t value;
} BatteryStatus;

//BMS state-of-charge confidence level
typedef enum {
  IF_BMS_SOCLVL_INVALID = UARTDEF_BMS_SOC_CONFIDENCE_INVALID,
  IF_BMS_SOCLVL_VOLTAGE_ONLY = UARTDEF_BMS_SOC_CONFIDENCE_VOLTAGE_ONLY,
  IF_BMS_SOCLVL_ESTIMATED_REF = UARTDEF_BMS_SOC_CONFIDENCE_ESTIMATED_REF,
  IF_BMS_SOCLVL_MEASURED_REF = UARTDEF_BMS_SOC_CONFIDENCE_MEASURED_REF
} BatterySoCLevel;

//BMS safety alerts/faults status
typedef union {
  struct {
    bool bms_regulator_error : 1;
    bool latched_current_fault : 1;
    bool overcurrent_charge : 1;
    bool overcurrent_discharge_slow : 1;
    bool overcurrent_discharge_fast : 1;
    bool short_circuit : 1;
    bool cell_undervoltage : 1;
    bool cell_overvoltage : 1;
    bool measurement_test_error : 1;
    bool voltage_reference_error : 1;
    bool watchdog_timeout : 1;
    bool bms_overtemperature : 1;
    bool undertemperature_charge : 1;
    bool undertemperature_discharge : 1;
    bool overtemperature_charge : 1;
    bool overtemperature_discharge : 1;
  };
  uint16_t value;
} BatterySafetyStatus;

//BMS scheduled shutdown type
typedef enum {
  IF_BMS_SHDN_NONE = UARTDEF_BMS_SHUTDOWN_TYPE_NONE,
  IF_BMS_SHDN_FULL_SHUTDOWN = UARTDEF_BMS_SHUTDOWN_TYPE_FULL_SHUTDOWN,
  IF_BMS_SHDN_END_OF_DISCHARGE = UARTDEF_BMS_SHUTDOWN_TYPE_END_OF_DISCHARGE,
  IF_BMS_SHDN_HOST_REQUEST = UARTDEF_BMS_SHUTDOWN_TYPE_HOST_REQUEST
} BatteryShutdownType;


#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
}


class BatteryInterface : public RegUARTModuleInterface {
public:
  bool IsBatteryPresent() const;

  BatteryStatus GetStatus() const;

  uint16_t GetStackVoltageMV() const;

  int16_t GetCellVoltageMV(uint8_t cell) const;
  const int16_t* GetCellVoltagesMV() const;

  int32_t GetCurrentMA() const;

  BatterySoCLevel GetSoCConfidenceLevel() const;
  float GetSoCFraction() const;
  float GetSoCEnergyWh() const;

  float GetBatteryHealth() const;

  int16_t GetBatteryTemperatureC() const;
  int16_t GetBMSTemperatureC() const;

  BatterySafetyStatus GetSafetyAlerts() const;
  BatterySafetyStatus GetSafetyFaults() const;

  BatteryShutdownType GetScheduledShutdown() const;
  uint16_t GetShutdownCountdownMS() const;

  uint8_t GetSeriesCellCount() const;
  uint8_t GetParallelCellCount() const;
  uint8_t GetTotalCellCount() const;

  uint16_t GetMinCellVoltageMV() const;
  uint16_t GetMaxCellVoltageMV() const;

  uint32_t GetMaxDischargeCurrentMA() const;
  uint32_t GetPeakDischargeCurrentMA() const;
  uint32_t GetMaxChargeCurrentMA() const;


  void SetBatteryHealth(float health, SuccessCallback&& callback);

  //void ResetModule(SuccessCallback&& callback);

  void ClearChargingFaults(SuccessCallback&& callback);

  void SetShutdownRequest(bool shutdown, SuccessCallback&& callback);
  void TriggerFullShutdown(SuccessCallback&& callback);


  void InitModule(SuccessCallback&& callback);
  void LoopTasks() override;


  BatteryInterface(UART_HandleTypeDef* uart_handle);

protected:
  bool initialised;
  bool present;

  uint32_t reset_wait_timer;
  SuccessCallback reset_callback;

  void HandleNotificationData(bool error, bool unsolicited) override;
  void OnRegisterUpdate(uint8_t address) override;
};


#endif


#endif /* INC_BATTERY_INTERFACE_H_ */
