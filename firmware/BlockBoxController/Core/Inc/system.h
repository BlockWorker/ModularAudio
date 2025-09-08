/*
 * system.h
 *
 *  Created on: May 10, 2025
 *      Author: Alex
 */

#ifndef INC_SYSTEM_H_
#define INC_SYSTEM_H_


#include "cpp_main.h"


#ifdef __cplusplus

#include "eeprom_interface.h"
#include "dap_interface.h"
#include "hifidac_interface.h"
#include "power_amp_interface.h"
#include "rtc_interface.h"
#include "bluetooth_receiver_interface.h"
#include "battery_interface.h"
#include "charger_interface.h"
#include "bbv2_gui_manager.h"
#include "audio_path_manager.h"
#include "amp_manager.h"
#include "power_manager.h"
#include "debug_log.h"


#define DEBUG_LOG(level, ...) do { DebugLog::instance.LogEntry(level, bbv2_system.rtc_if.GetDateTime(), __VA_ARGS__); } while (0)


extern "C" {
#endif


#ifdef __cplusplus
}


class System {
public:
  virtual void Init() = 0;
  virtual void LoopTasks() = 0;
};


class BlockBoxV2System : public System {
public:
  I2CHardwareInterface main_i2c_hw;
  EEPROMInterface eeprom_if;
  DAPInterface dap_if;
  HiFiDACInterface dac_if;
  PowerAmpInterface amp_if;
  RTCInterface rtc_if;

  I2CHardwareInterface chg_i2c_hw;
  ChargerInterface chg_if;

  BluetoothReceiverInterface btrx_if;
  BatteryInterface bat_if;

  EVEDriver eve_drv;
  BlockBoxV2GUIManager gui_mgr;

  AudioPathManager audio_mgr;
  AmpManager amp_mgr;
  PowerManager power_mgr;

  void Init() override;
  void LoopTasks() override;

  bool IsPoweredOn() const;
  void SetPowerState(bool on, SuccessCallback&& callback);

  BlockBoxV2System();

private:
  bool powered_on;

  void InitEEPROM(SuccessCallback&& callback);
  void InitDAP(SuccessCallback&& callback);
  void InitHiFiDAC(SuccessCallback&& callback);
  void InitPowerAmp(SuccessCallback&& callback);
  void InitCharger(SuccessCallback&& callback);
  void InitBluetoothReceiver(SuccessCallback&& callback);
  void InitBattery(SuccessCallback&& callback);

};


extern BlockBoxV2System bbv2_system;
extern System& main_system;


#endif


#endif /* INC_SYSTEM_H_ */
