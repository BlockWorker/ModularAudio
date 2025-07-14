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

#include "dap_interface.h"
#include "hifidac_interface.h"
#include "bluetooth_receiver_interface.h"

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

  DAPInterface dap_if;
  HiFiDACInterface dac_if;

  BluetoothReceiverInterface btrx_if;

  void Init() override;
  void LoopTasks() override;

  BlockBoxV2System();
};


extern BlockBoxV2System bbv2_system;
extern System& main_system;


#endif


#endif /* INC_SYSTEM_H_ */
