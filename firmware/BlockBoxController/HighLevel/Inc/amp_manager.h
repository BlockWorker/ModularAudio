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
#define AMP_EVENT_XYZ (1u << 8)

//amp setting lock timeout, in main loop cycles
#define AMP_LOCK_TIMEOUT_CYCLES (200 / MAIN_LOOP_PERIOD_MS)

//PVDD target lowering lock timeout, in main loop cycles - to avoid spam-lowering the PVDD target
#define AMP_PVDD_LOCK_TIMEOUT_CYCLES (500 / MAIN_LOOP_PERIOD_MS)

//clipping response lock timeout, in main loop cycles - to avoid responding multiple times to the "same" clipping event
#define AMP_CLIP_LOCK_TIMEOUT_CYCLES (500 / MAIN_LOOP_PERIOD_MS)

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


  //PVDD error/change handling? idk if needed,
  //detection, propagation and handling of safety events + reset if needed (lower volume on warning), amp fault detection and propagation + reset if needed
  //amp clipping detection (increase voltage or lower volume), amp overtemp detection (lower volume)

protected:
  bool initialised;
  uint32_t lock_timer;
  std::deque<QueuedOperation> queued_operations;

  uint32_t pvdd_lock_timer;
  uint32_t clip_lock_timer;

  float warning_limit_factor;
  PowerAmpThresholdSet warning_limits_irms;
  PowerAmpThresholdSet warning_limits_pavg;


  void ApplyNewPVDDTarget(float pvdd_volts, SuccessCallback&& callback);
  void UpdatePVDDForVolume(float volume_dB, SuccessCallback&& callback);

  void UpdateWarningLimits(float factor, SuccessCallback&& callback);

  void HandleEvent(EventSource* source, uint32_t event);
};


#endif


#endif /* INC_AMP_MANAGER_H_ */
