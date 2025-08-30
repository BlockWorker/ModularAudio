/*
 * power_manager.h
 *
 *  Created on: Aug 28, 2025
 *      Author: Alex
 */

#ifndef INC_POWER_MANAGER_H_
#define INC_POWER_MANAGER_H_


#include "cpp_main.h"
#include "event_source.h"
#include "storage.h"
#include "charger_interface.h"


//power system events
#define PWR_EVENT_CHARGING_ACTIVE_CHANGE (1u << 8)

//power setting lock timeout, in main loop cycles
#define PWR_LOCK_TIMEOUT_CYCLES (200 / MAIN_LOOP_PERIOD_MS)

//auto-shutdown lock timeout, in main loop cycles
#define PWR_ASD_LOCK_TIMEOUT_CYCLES (300 / MAIN_LOOP_PERIOD_MS)

//min and max values for maximum adapter current, in amps
#define PWR_ADAPTER_CURRENT_A_MIN IF_CHG_ADAPTER_CURRENT_A_MIN
#define PWR_ADAPTER_CURRENT_A_MAX IF_CHG_ADAPTER_CURRENT_A_MAX

//min and max charging target current, in amps
#define PWR_CHG_CURRENT_A_MIN 0.128f
#define PWR_CHG_CURRENT_A_MAX 8.128f

//min and max auto-shutdown delays, in milliseconds
#define PWR_ASD_DELAY_MS_MIN 60000u
#define PWR_ASD_DELAY_MS_MAX HAL_MAX_DELAY


//charging target state-of-charge steps (approximate percentages)
typedef enum {
  PWR_CHG_TARGET_70 = 0,
  PWR_CHG_TARGET_75,
  PWR_CHG_TARGET_80,
  PWR_CHG_TARGET_85,
  PWR_CHG_TARGET_90,
  PWR_CHG_TARGET_95,
  PWR_CHG_TARGET_100,
  _PWR_CHG_TARGET_COUNT
} PowerChargingTargetState;


#ifdef __cplusplus


#include <deque>


class BlockBoxV2System;


class PowerManager : public EventSource {
public:
  BlockBoxV2System& system;

  PowerManager(BlockBoxV2System& system);

  void Init(SuccessCallback&& callback);
  void LoopTasks();

  //void HandlePowerStateChange(bool on, SuccessCallback&& callback);

  //adapter config functions
  float GetAdapterMaxCurrentA() const;

  void SetAdapterMaxCurrentA(float current_A, SuccessCallback&& callback, bool queue_if_busy = false);

  //charging functions
  bool IsCharging() const;
  PowerChargingTargetState GetChargingTargetState() const;
  float GetChargingTargetCurrentA() const;

  void SetChargingTargetState(PowerChargingTargetState target, SuccessCallback&& callback, bool queue_if_busy = false);
  void SetChargingTargetCurrentA(float current_A, SuccessCallback&& callback, bool queue_if_busy = false);

  //auto-shutdown functions
  uint32_t GetAutoShutdownDelayMS() const;

  void SetAutoShutdownDelayMS(uint32_t delay_ms);
  void ResetAutoShutdownTimer();

  //charging: voltage target config, current target config, writing+rewriting to charger, reporting+display(LED) of charging state, charge cut-off monitoring (based on voltage and consistently low charge current),
  //no starting charge if SoC or voltage too close to target
  //time-based auto-shutdown of battery, learn mode handling for health measurement (?)(maybe not initially?)
  //TODO: NVM storage of config, probably including max input current

protected:
  StorageSection non_volatile_config;

  bool initialised;
  uint32_t lock_timer;
  std::deque<QueuedOperation> queued_operations;

  uint32_t asd_lock_timer;

  bool charging_active;
  uint32_t charging_end_condition_cycles;

  uint32_t auto_shutdown_last_reset;

  static void LoadNonVolatileConfigDefaults(StorageSection& section);

  uint16_t GetTargetStateVoltageMV(PowerChargingTargetState target);

  uint16_t GetChargingTargetCurrentMA() const;

  //void HandleEvent(EventSource* source, uint32_t event);
};


#endif


#endif /* INC_POWER_MANAGER_H_ */
