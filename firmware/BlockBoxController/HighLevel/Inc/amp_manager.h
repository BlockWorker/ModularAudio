/*
 * amp_manager.h
 *
 *  Created on: Aug 10, 2025
 *      Author: Alex
 */

#ifndef INC_AMP_MANAGER_H_
#define INC_AMP_MANAGER_H_


#include "cpp_main.h"
#include "event_source.h"
#include "power_amp_interface.h"


//amplifier events
#define AMP_EVENT_SAFETY_WARNING (1u << 8)
#define AMP_EVENT_SAFETY_ERROR (1u << 9)

//amp setting lock timeout, in main loop cycles
#define AMP_LOCK_TIMEOUT_CYCLES (200 / MAIN_LOOP_PERIOD_MS)

//PVDD target lowering lock timeout, in main loop cycles - to avoid spam-lowering the PVDD target
#define AMP_PVDD_LOCK_TIMEOUT_CYCLES (500 / MAIN_LOOP_PERIOD_MS)

//clipping response lock timeout, in main loop cycles - to avoid responding multiple times to the "same" clipping event
#define AMP_CLIP_LOCK_TIMEOUT_CYCLES (1000 / MAIN_LOOP_PERIOD_MS)

//overtemperature warning response lock timeout, in main loop cycles - to avoid responding multiple times to the "same" warning event
#define AMP_OTW_LOCK_TIMEOUT_CYCLES (1000 / MAIN_LOOP_PERIOD_MS)

//warning response lock timeout, in main loop cycles - to avoid responding multiple times to the "same" warning event
#define AMP_WARN_LOCK_TIMEOUT_CYCLES (300 / MAIN_LOOP_PERIOD_MS)

//min and max warning limit factor
#define AMP_WARNING_FACTOR_MIN 0.5f
#define AMP_WARNING_FACTOR_MAX 1.0f


#ifdef __cplusplus


#include <deque>


class BlockBoxV2System;


class AmpManager : public EventSource {
public:
  BlockBoxV2System& system;

  AmpManager(BlockBoxV2System& system);

  void Init(SuccessCallback&& callback);
  void LoopTasks();

  void HandlePowerStateChange(bool on, SuccessCallback&& callback);

  //warning limit factor functions
  float GetWarningLimitFactor() const;

  void SetWarningLimitFactor(float limit_factor, SuccessCallback&& callback, bool queue_if_busy = false);

protected:
  bool initialised;
  bool callbacks_registered;
  uint32_t lock_timer;
  std::deque<QueuedOperation> queued_operations;

  uint32_t pvdd_lock_timer;
  uint32_t clip_lock_timer;
  uint32_t otw_lock_timer;
  uint32_t warn_lock_timer;

  float warning_limit_factor;
  PowerAmpThresholdSet warning_limits_irms;
  PowerAmpThresholdSet warning_limits_pavg;

  bool prev_amp_fault;
  bool prev_pvdd_fault;
  uint16_t prev_safety_error;


  void ApplyNewPVDDTarget(float pvdd_volts, SuccessCallback&& callback);
  void UpdatePVDDForVolume(float volume_dB, SuccessCallback&& callback);

  void UpdateWarningLimits(float factor, SuccessCallback&& callback);

  void HandleClipping(SuccessCallback&& callback);
  void ReduceVolume(SuccessCallback&& callback);

  void HandleEvent(EventSource* source, uint32_t event);
};


#endif


#endif /* INC_AMP_MANAGER_H_ */
