/*
 * amp_manager.cpp
 *
 *  Created on: Aug 10, 2025
 *      Author: Alex
 */


#include "amp_manager.h"
#include "system.h"


/******************************************************/
/*       Constant definitions and configuration       */
/******************************************************/

//PVDD configuration
//volume/gain in dB, at (and above) which PVDD should be at maximum
#define AMP_PVDD_MAX_VOLUME_DB -2.0f
//hysteresis for lowering PVDD: only lower if the current target is at least this factor above the desired target
#define AMP_PVDD_LOWERING_HYST 1.2f
//PVDD clipping increase factor: multiply PVDD target by this factor when detecting clipping
#define AMP_PVDD_CLIPPING_INCREASE 1.1f

//default warning limit factor
#define AMP_WARNING_FACTOR_DEFAULT 0.8f


//Safety limit configuration (error limits)
static const PowerAmpThresholdSet _amp_safety_limits_irms = { //current; calculated as sqrt(P_desired / R_dc) + a bit of margin (0.3A)
  { 7.8f, 7.8f, 6.1f, 6.1f, 13.0f }, //instantaneous
  { 5.8f, 5.8f, 4.4f, 4.4f, 10.0f }, //fast (0.1s)
  { 4.3f, 4.3f, 3.2f, 3.2f, 7.0f }, //slow (1s)
};
static const PowerAmpThresholdSet _amp_safety_limits_pavg = { //power; calculated as a P_desired + margin for the warnings to trigger first
  { 500.0f, 500.0f, 250.0f, 250.0f, 600.0f }, //instantaneous (woofer should never trigger)
  { 300.0f, 300.0f, 150.0f, 150.0f, 400.0f }, //fast (0.1s)
  { 120.0f, 120.0f, 60.0f, 60.0f, 200.0f }, //slow (1s)
};
static const PowerAmpThresholdSet _amp_safety_limits_papp = { //apparent power; disabled
  { INFINITY, INFINITY, INFINITY, INFINITY, INFINITY }, //instantaneous
  { INFINITY, INFINITY, INFINITY, INFINITY, INFINITY }, //fast (0.1s)
  { INFINITY, INFINITY, INFINITY, INFINITY, INFINITY }, //slow (1s)
};

//Base warning limit configuration (= 100% of power target); sum warnings not used
static const PowerAmpThresholdSet _amp_warning_base_limits_irms = { //current; calculated as sqrt(P_desired / Z_nom) + a bit of margin (0.3A)
  { 7.4f, 7.4f, 5.3f, 5.3f, INFINITY }, //instantaneous
  { 5.3f, 5.3f, 3.8f, 3.8f, INFINITY }, //fast (0.1s)
  { 3.8f, 3.8f, 2.8f, 2.8f, INFINITY }, //slow (1s)
};
static const PowerAmpThresholdSet _amp_warning_base_limits_pavg = { //power; P_desired levels: slow = RMS rating, fast = program/music rating (= 2x RMS rating), instantaneous = 2x program/music rating (= 4x RMS rating)
  { 400.0f, 400.0f, 200.0f, 200.0f, INFINITY }, //instantaneous (woofer should never trigger at 100% target)
  { 200.0f, 200.0f, 100.0f, 100.0f, INFINITY }, //fast (0.1s)
  { 100.0f, 100.0f, 50.0f, 50.0f, INFINITY }, //slow (1s)
};
static const PowerAmpThresholdSet _amp_warning_base_limits_papp = { //apparent power; disabled
  { INFINITY, INFINITY, INFINITY, INFINITY, INFINITY }, //instantaneous
  { INFINITY, INFINITY, INFINITY, INFINITY, INFINITY }, //fast (0.1s)
  { INFINITY, INFINITY, INFINITY, INFINITY, INFINITY }, //slow (1s)
};


static_assert(AMP_PVDD_LOWERING_HYST > 1.0f);
static_assert(AMP_PVDD_CLIPPING_INCREASE > 1.0f);
static_assert(AMP_WARNING_FACTOR_DEFAULT >= AMP_WARNING_FACTOR_MIN && AMP_WARNING_FACTOR_DEFAULT <= AMP_WARNING_FACTOR_MAX);


/******************************************************/
/*                  Initialisation                    */
/******************************************************/

AmpManager::AmpManager(BlockBoxV2System& system) :
    system(system), initialised(false), callbacks_registered(false), lock_timer(0), pvdd_lock_timer(0), clip_lock_timer(0), otw_lock_timer(0), warn_lock_timer(0),
    warning_limit_factor(AMP_WARNING_FACTOR_DEFAULT), prev_amp_fault(false), prev_pvdd_fault(false), prev_safety_error(0) {}


void AmpManager::Init(SuccessCallback&& callback) {
  this->initialised = false;
  this->lock_timer = 0;
  this->pvdd_lock_timer = 0;
  this->clip_lock_timer = 0;
  this->otw_lock_timer = 0;
  this->warn_lock_timer = 0;
  this->warning_limit_factor = AMP_WARNING_FACTOR_DEFAULT;
  this->prev_amp_fault = false;
  this->prev_pvdd_fault = false;
  this->prev_safety_error = 0;

  //enter power-off state
  this->HandlePowerStateChange(false, [this, callback = std::move(callback)](bool success) {
    if (!success) {
      //propagate failure to external callback
      DEBUG_LOG(DEBUG_ERROR, "AmpManager Init failed to enter power-off state");
      if (callback) {
        callback(false);
      }
      return;
    }

    //write safety error limits - RMS current
    this->system.amp_if.SetSafetyThresholds(IF_POWERAMP_THR_IRMS_ERROR, &_amp_safety_limits_irms, [this, callback = std::move(callback)](bool success) {
      if (!success) {
        //propagate failure to external callback
        DEBUG_LOG(DEBUG_ERROR, "AmpManager Init failed to set Irms error thresholds");
        if (callback) {
          callback(false);
        }
        return;
      }

      //write safety error limits - Average power
      this->system.amp_if.SetSafetyThresholds(IF_POWERAMP_THR_PAVG_ERROR, &_amp_safety_limits_pavg, [this, callback = std::move(callback)](bool success) {
        if (!success) {
          //propagate failure to external callback
          DEBUG_LOG(DEBUG_ERROR, "AmpManager Init failed to set Pavg error thresholds");
          if (callback) {
            callback(false);
          }
          return;
        }

        //write safety error limits - Apparent power (disabled)
        this->system.amp_if.SetSafetyThresholds(IF_POWERAMP_THR_PAPP_ERROR, &_amp_safety_limits_papp, [this, callback = std::move(callback)](bool success) {
          if (!success) {
            //propagate failure to external callback
            DEBUG_LOG(DEBUG_ERROR, "AmpManager Init failed to set Papp error thresholds");
            if (callback) {
              callback(false);
            }
            return;
          }

          //write safety warning limits - Apparent power (disabled)
          this->system.amp_if.SetSafetyThresholds(IF_POWERAMP_THR_PAPP_WARNING, &_amp_warning_base_limits_papp, [this, callback = std::move(callback)](bool success) {
            if (!success) {
              //propagate failure to external callback
              DEBUG_LOG(DEBUG_ERROR, "AmpManager Init failed to set Papp warning thresholds");
              if (callback) {
                callback(false);
              }
              return;
            }

            //write safety warning limits - RMS current and average power, scaled according to default factor
            this->UpdateWarningLimits(AMP_WARNING_FACTOR_DEFAULT, [this, callback = std::move(callback)](bool success) {
              if (success) {
                if (!this->callbacks_registered) {
                  //register event callbacks
                  this->system.amp_if.RegisterCallback(std::bind(&AmpManager::HandleEvent, this, std::placeholders::_1, std::placeholders::_2),
                                                       MODIF_EVENT_MODULE_RESET | MODIF_POWERAMP_EVENT_STATUS_UPDATE | MODIF_POWERAMP_EVENT_SAFETY_UPDATE);
                  this->system.audio_mgr.RegisterCallback(std::bind(&AmpManager::HandleEvent, this, std::placeholders::_1, std::placeholders::_2),
                                                          AUDIO_EVENT_VOLUME_UPDATE);
                  this->callbacks_registered = true;
                }

                //init done
                this->initialised = true;
              } else {
                DEBUG_LOG(DEBUG_ERROR, "AmpManager Init failed to set initial Irms and Pavg warning thresholds");
              }

              //propagate success to external callback
              if (callback) {
                callback(success);
              }
            });
          });
        });
      });
    });
  });
}


/******************************************************/
/*          Basic Tasks and Event Handling            */
/******************************************************/

void AmpManager::LoopTasks() {
  //static uint32_t loop_count = 0;

  if (!this->initialised) {
    return;
  }

  //decrement lock timers, under disabled interrupts
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  if (this->pvdd_lock_timer > 0) {
    if (--this->pvdd_lock_timer == 0) {
      //PVDD lowering lock timed out: trigger update to make sure we're aligned with the volume
      this->UpdatePVDDForVolume(this->system.audio_mgr.GetCurrentVolumeDB(), SuccessCallback());
    }
  }

  if (this->clip_lock_timer > 0) {
    this->clip_lock_timer--;
  }

  if (this->otw_lock_timer > 0) {
    this->otw_lock_timer--;
  }

  if (this->warn_lock_timer > 0) {
    this->warn_lock_timer--;
  }

  if (this->lock_timer > 0) {
    if (--this->lock_timer == 0) {
      //shouldn't really ever happen, it means an unlock somewhere was missed or delayed excessively
      DEBUG_LOG(DEBUG_WARNING, "AmpManager lock timed out!");
    }
  }

  //if not locked, check for queued operations
  if (this->lock_timer == 0 && !this->queued_operations.empty()) {
    //have queued operation: remove from queue, lock again
    QueuedOperation op = std::move(this->queued_operations.front());
    this->queued_operations.pop_front();
    //begin operation under disabled interrupts
    if (op) {
      op();
    }
  }
  __set_PRIMASK(primask);
}


void AmpManager::HandlePowerStateChange(bool on, SuccessCallback&& callback) {
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  if (this->lock_timer > 0) {
    //locked out: queue for later execution
    this->queued_operations.push_back([this, on, callback = std::move(callback)]() mutable {
      this->HandlePowerStateChange(on, std::move(callback));
    });
    __set_PRIMASK(primask);
    return;
  }

  //lock out further operations
  this->lock_timer = AMP_LOCK_TIMEOUT_CYCLES;
  __set_PRIMASK(primask);

  //set amp manual shutdown according to state
  this->system.amp_if.SetManualShutdownActive(!on, [this, on, callback = std::move(callback)](bool success) {
    if (!success) {
      DEBUG_LOG(DEBUG_ERROR, "AmpManager HandlePowerStateChange failed to set amp shutdown");
    }

    //callback for after PVDD adjustment
    auto post_pvdd_cb = [this, callback = std::move(callback), prev_success = success](bool success) {
      if (!success) {
        DEBUG_LOG(DEBUG_ERROR, "AmpManager HandlePowerStateChange failed to set PVDD target");
      }

      //once done: unlock operations and propagate success (true = all succeeded, false otherwise) to external callback
      this->lock_timer = 0;
      if (callback) {
        callback(prev_success && success);
      }
    };

    //continue regardless of success: set PVDD according to state
    if (on) {
      //turning on: configure based on volume
      this->UpdatePVDDForVolume(this->system.audio_mgr.GetCurrentVolumeDB(), std::move(post_pvdd_cb));
    } else {
      //turning off: lower to minimum, lock out other lowering
      this->pvdd_lock_timer = AMP_PVDD_LOCK_TIMEOUT_CYCLES;
      this->system.amp_if.SetPVDDTargetVoltage(IF_POWERAMP_PVDD_TARGET_MIN, std::move(post_pvdd_cb));
    }
  });
}


void AmpManager::HandleEvent(EventSource* source, uint32_t event) {
  if (source == &this->system.amp_if && event == MODIF_EVENT_MODULE_RESET) {
    //amp module reset: attempt re-init of this manager and return to correct state
    this->Init([this](bool success) {
      if (!success) {
        DEBUG_LOG(DEBUG_CRITICAL, "AmpManager failed to re-init after amp module reset!");
        return;
      }

      //if we're supposed to be powered on, attempt to adjust state correspondingly
      if (this->system.IsPoweredOn()) {
        this->HandlePowerStateChange(true, [](bool success) {
          if (!success) {
            DEBUG_LOG(DEBUG_ERROR, "AmpManager failed to turn on again after amp module reset");
          }
        });
      }
    });
    return;
  } else if (!this->initialised) {
    //don't handle other events until initialised
    return;
  }

  if (source == &this->system.audio_mgr) {
    //handle volume change event: update PVDD target
    this->UpdatePVDDForVolume(this->system.audio_mgr.GetCurrentVolumeDB(), SuccessCallback());
  } else {
    //handle amp module events
    switch (event) {
      case MODIF_POWERAMP_EVENT_STATUS_UPDATE:
      {
        //status update
        PowerAmpStatus status = this->system.amp_if.GetStatus();

        //check for faults first
        if (status.amp_fault) {
          if (!this->prev_amp_fault) {
            //log fault
            DEBUG_LOG(DEBUG_CRITICAL, "Amplifier fault");
            this->prev_amp_fault = true;
          }
          this->system.gui_mgr.ActivatePopups(GUI_POPUP_AMP_FAULT);
        } else {
          this->prev_amp_fault = false;
          this->system.gui_mgr.DeactivatePopups(GUI_POPUP_AMP_FAULT);
        }

        if (!status.pvdd_valid) {
          if (!this->prev_pvdd_fault) {
            //log PVDD fault
            DEBUG_LOG(DEBUG_CRITICAL, "Amp PVDD fault: %.2f V", this->system.amp_if.GetPVDDMeasuredVoltage());
            this->prev_pvdd_fault = true;
          }
        } else {
          this->prev_pvdd_fault = false;
        }

        //check for safety warning and respond
        if (status.safety_warning) {
          uint32_t primask = __get_PRIMASK();
          __disable_irq();
          if (this->warn_lock_timer == 0) {
            //warning detected and not locked out: lock out and respond by lowering volume
            this->warn_lock_timer = AMP_WARN_LOCK_TIMEOUT_CYCLES;
            __set_PRIMASK(primask);
            DEBUG_LOG(DEBUG_INFO, "AmpManager responding to safety warning");
            this->ReduceVolume([this](bool success) {
              if (!success) {
                //on failure, unlock immediately
                DEBUG_LOG(DEBUG_WARNING, "AmpManager failed to reduce volume in response to safety warning!");
                this->warn_lock_timer = 0;
              }
            });
            this->ExecuteCallbacks(AMP_EVENT_SAFETY_WARNING);
            //don't do anything else for now
            return;
          } else {
            //locked out: don't respond
            __set_PRIMASK(primask);
          }
        } else {
          //no safety warning: unlock for immediate handling of the next one
          this->warn_lock_timer = 0;
        }

        //check for overtemperature warning and respond
        if (status.otw_detected) {
          uint32_t primask = __get_PRIMASK();
          __disable_irq();
          if (this->otw_lock_timer == 0) {
            //otw detected and not locked out: lock out and respond by lowering volume
            this->otw_lock_timer = AMP_OTW_LOCK_TIMEOUT_CYCLES;
            __set_PRIMASK(primask);
            DEBUG_LOG(DEBUG_WARNING, "AmpManager responding to overtemperature warning");
            this->ReduceVolume([this](bool success) {
              if (!success) {
                //on failure, unlock immediately
                DEBUG_LOG(DEBUG_WARNING, "AmpManager failed to reduce volume in response to overtemperature warning!");
                this->otw_lock_timer = 0;
              }
            });
            //don't do anything else for now
            return;
          } else {
            //locked out: don't respond
            __set_PRIMASK(primask);
          }
        } else {
          //no overtemperature warning: unlock for immediate handling of the next one
          this->otw_lock_timer = 0;
        }

        //check for clipping and respond
        if (status.clip_detected) {
          this->HandleClipping([this](bool success) {
            if (!success) {
              //on failure, unlock immediately
              DEBUG_LOG(DEBUG_WARNING, "AmpManager failed to respond to clipping!");
              this->clip_lock_timer = 0;
            }
          });
        }
        break;
      }
      case MODIF_POWERAMP_EVENT_SAFETY_UPDATE:
      {
        //safety update: just check for safety error shutdown (warnings handled in status update above)
        if (this->system.amp_if.IsSafetyShutdownActive()) {
          uint16_t err = this->system.amp_if.GetSafetyErrorSource().value;
          if (this->prev_safety_error != err) {
            //log error
            DEBUG_LOG(DEBUG_CRITICAL, "Amp safety error: 0x%04X", err);
            this->prev_safety_error = err;
            this->ExecuteCallbacks(AMP_EVENT_SAFETY_ERROR);
          }
          this->system.gui_mgr.ActivatePopups(GUI_POPUP_AMP_SAFETY_ERR);
        } else {
          this->prev_safety_error = 0;
          this->system.gui_mgr.DeactivatePopups(GUI_POPUP_AMP_SAFETY_ERR);
        }
        break;
      }
      default:
        break;
    }
  }
}


/******************************************************/
/*                  PVDD Handling                     */
/******************************************************/

void AmpManager::ApplyNewPVDDTarget(float pvdd_volts, SuccessCallback&& callback) {
  float current_pvdd_target = this->system.amp_if.GetPVDDTargetVoltage();
  bool apply = false;

  //apply new PVDD target if it's higher than the existing target, or much lower than the existing target and not locked
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  if (isnanf(current_pvdd_target) || current_pvdd_target < pvdd_volts) {
    apply = true;
  } else if (current_pvdd_target / pvdd_volts > AMP_PVDD_LOWERING_HYST && this->pvdd_lock_timer == 0) {
    apply = true;
    //lock further lowering for a while
    this->pvdd_lock_timer = AMP_PVDD_LOCK_TIMEOUT_CYCLES;
  }
  __set_PRIMASK(primask);

  //apply if needed
  if (apply) {
    this->system.amp_if.SetPVDDTargetVoltage(pvdd_volts, std::move(callback));
  } else {
    //not applying because too close or lowered to recently: report success
    if (callback) {
      callback(true);
    }
  }
}

void AmpManager::UpdatePVDDForVolume(float volume_dB, SuccessCallback&& callback) {
  if (isnanf(volume_dB)) {
    throw std::invalid_argument("AmpManager UpdatePVDDForVolume given invalid volume");
  }

  //calculate desired PVDD voltage, based on difference between volume and max-pvdd threshold (accounting for dB scale)
  float desired_pvdd = IF_POWERAMP_PVDD_TARGET_MAX * powf(10.0f, (volume_dB - AMP_PVDD_MAX_VOLUME_DB) / 20.0f);
  //clamp to valid PVDD target range
  if (desired_pvdd < IF_POWERAMP_PVDD_TARGET_MIN) {
    desired_pvdd = IF_POWERAMP_PVDD_TARGET_MIN;
  } else if (desired_pvdd > IF_POWERAMP_PVDD_TARGET_MAX) {
    desired_pvdd = IF_POWERAMP_PVDD_TARGET_MAX;
  }

  //apply new PVDD voltage
  this->ApplyNewPVDDTarget(desired_pvdd, std::move(callback));
}


/******************************************************/
/*              Limit & Safety Handling               */
/******************************************************/

static void _AmpManager_ScaleThresholdSet(PowerAmpThresholdSet* set, float factor) {
  float* raw_ptr = (float*)set;
  int i;
  for (i = 0; i < 15; i++) {
    raw_ptr[i] *= factor;
  }
}

void AmpManager::UpdateWarningLimits(float factor, SuccessCallback&& callback) {
  //ensure validity of limit factor
  if (isnanf(factor) || factor < AMP_WARNING_FACTOR_MIN) {
    factor = AMP_WARNING_FACTOR_MIN;
  } else if (factor > AMP_WARNING_FACTOR_MAX) {
    factor = AMP_WARNING_FACTOR_MAX;
  }

  //copy base threshold sets
  memcpy(&this->warning_limits_irms, &_amp_warning_base_limits_irms, sizeof(PowerAmpThresholdSet));
  memcpy(&this->warning_limits_pavg, &_amp_warning_base_limits_pavg, sizeof(PowerAmpThresholdSet));

  //scale threshold sets with factor
  _AmpManager_ScaleThresholdSet(&this->warning_limits_irms, factor);
  _AmpManager_ScaleThresholdSet(&this->warning_limits_pavg, factor);

  //apply current set
  this->system.amp_if.SetSafetyThresholds(IF_POWERAMP_THR_IRMS_WARNING, &this->warning_limits_irms, [this, factor, callback = std::move(callback)](bool success) {
    if (!success) {
      DEBUG_LOG(DEBUG_ERROR, "AmpManager UpdateWarningLimits failed to set Irms warning thresholds");
    }

    //continue regardless of success: apply power set
    this->system.amp_if.SetSafetyThresholds(IF_POWERAMP_THR_PAVG_WARNING, &this->warning_limits_pavg, [this, factor, callback = std::move(callback), prev_success = success](bool success) {
      if (!success) {
        DEBUG_LOG(DEBUG_ERROR, "AmpManager UpdateWarningLimits failed to set Pavg warning thresholds");
      }

      if (success && prev_success) {
        //save factor if successful
        this->warning_limit_factor = factor;
      }

      //report overall success (both operations) to external callback
      if (callback) {
        callback(success && prev_success);
      }
    });
  });
}


float AmpManager::GetWarningLimitFactor() const {
  return this->warning_limit_factor;
}


void AmpManager::SetWarningLimitFactor(float limit_factor, SuccessCallback&& callback, bool queue_if_busy) {
  if (isnanf(limit_factor) || limit_factor < AMP_WARNING_FACTOR_MIN || limit_factor > AMP_WARNING_FACTOR_MAX) {
    throw std::invalid_argument("AmpManager SetWarningLimitFactor given invalid factor, must be in range [0.5, 1]");
  }

  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  if (!this->initialised) {
    //uninitialised: failure, propagate to callback
    __set_PRIMASK(primask);
    if (callback) {
      callback(false);
    }
    return;
  }

  if (this->lock_timer > 0) {
    //locked out: failure, queue or propagate to callback
    if (queue_if_busy) {
      this->queued_operations.push_back([this, limit_factor, callback = std::move(callback)]() mutable {
        this->SetWarningLimitFactor(limit_factor, std::move(callback), false);
      });
      __set_PRIMASK(primask);
    } else {
      __set_PRIMASK(primask);
      if (callback) {
        callback(false);
      }
    }
    return;
  }

  //lock out further operations
  this->lock_timer = AMP_LOCK_TIMEOUT_CYCLES;
  __set_PRIMASK(primask);

  if (this->warning_limit_factor == limit_factor) {
    //already in desired state: unlock operations and propagate success to external callback
    this->lock_timer = 0;
    if (callback) {
      callback(true);
    }
  } else {
    //shut down amplifier first
    bool amp_was_on = !this->system.amp_if.IsManualShutdownActive();
    this->system.amp_if.SetManualShutdownActive(true, [this, limit_factor, amp_was_on, callback = std::move(callback)](bool success) {
      if (!success) {
        //propagate failure to external callback
        DEBUG_LOG(DEBUG_ERROR, "AmpManager SetWarningLimitFactor failed to set manual amp shutdown");
        this->lock_timer = 0;
        if (callback) {
          callback(false);
        }
        return;
      }

      //apply new factor
      this->UpdateWarningLimits(limit_factor, [this, amp_was_on, callback = std::move(callback)](bool success) {
        if (!success) {
          //propagate failure to external callback
          DEBUG_LOG(DEBUG_ERROR, "AmpManager SetWarningLimitFactor failed to apply new limits");
          this->lock_timer = 0;
          if (callback) {
            callback(false);
          }
          return;
        }

        if (amp_was_on) {
          //restart amplifier
          this->system.amp_if.SetManualShutdownActive(false, [this, callback = std::move(callback)](bool success) {
            if (!success) {
              DEBUG_LOG(DEBUG_ERROR, "AmpManager SetWarningLimitFactor failed to reset manual amp shutdown");
            }

            //once done: unlock operations and propagate success to external callback
            this->lock_timer = 0;
            if (callback) {
              callback(success);
            }
          });
        } else {
          //no need to restart: done, unlock operations and propagate success to external callback
          this->lock_timer = 0;
          if (callback) {
            callback(success);
          }
        }
      });
    });
  }
}


void AmpManager::HandleClipping(SuccessCallback&& callback) {
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  if (this->clip_lock_timer > 0) {
    //locked out, return success (i.e. nothing to do right now)
    if (callback) {
      callback(true);
    }
    return;
  }

  //lock out further clipping responses for a while
  this->clip_lock_timer = AMP_CLIP_LOCK_TIMEOUT_CYCLES;
  __set_PRIMASK(primask);

  float current_pvdd_target = this->system.amp_if.GetPVDDTargetVoltage();
  if (current_pvdd_target - this->system.amp_if.GetPVDDMeasuredVoltage() > 5.0f) {
    //voltage is already trying to increase, but not done yet: nothing to do, report success to callback
    if (callback) {
      callback(true);
    }
  } else if (current_pvdd_target < IF_POWERAMP_PVDD_TARGET_MAX) {
    //voltage not maxed out yet: increase voltage, clamping to limit
    DEBUG_LOG(DEBUG_INFO, "AmpManager increasing voltage due to clipping");
    float new_target = current_pvdd_target * AMP_PVDD_CLIPPING_INCREASE;
    if (new_target > IF_POWERAMP_PVDD_TARGET_MAX) {
      new_target = IF_POWERAMP_PVDD_TARGET_MAX;
    }
    this->ApplyNewPVDDTarget(new_target, std::move(callback));
  }/* else {
    //voltage already at limit, still clipping: reduce volume - TODO disabled this for now, not quite desired - instead implement pre-amp clipping detection in DAP?
    this->ReduceVolume(std::move(callback));
  }*/
}

void AmpManager::ReduceVolume(SuccessCallback&& callback) {
  //go one volume step down, ensuring that it happens (queueing if busy)
  this->system.audio_mgr.StepCurrentVolumeDB(false, std::move(callback), true);
}

