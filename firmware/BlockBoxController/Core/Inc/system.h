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
#include "bbv2_gui_manager.h"
#include "audio_path_manager.h"

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

  BluetoothReceiverInterface btrx_if;

  EVEDriver eve_drv;
  BlockBoxV2GUIManager gui_mgr;

  AudioPathManager audio_mgr;

  void Init() override;
  void LoopTasks() override;

  BlockBoxV2System();

private:
  void InitEEPROM(SuccessCallback&& callback);
  void InitDAP(SuccessCallback&& callback);
  void InitHiFiDAC(SuccessCallback&& callback);
  void InitPowerAmp(SuccessCallback&& callback);
  void InitBluetoothReceiver(SuccessCallback&& callback);

};


extern BlockBoxV2System bbv2_system;
extern System& main_system;


#endif


#endif /* INC_SYSTEM_H_ */
