/*
 * audio_path_manager.cpp
 *
 *  Created on: Jul 22, 2025
 *      Author: Alex
 */


#include "audio_path_manager.h"
#include "system.h"


//non-volatile storage layout
//initial loudness gain in dB, float (4B)
#define AUDIO_NVM_INIT_LOUDNESS_GAIN 0
//whether the loudness target tracks the maximum volume, bool (1B)
#define AUDIO_NVM_LOUDNESS_TRACK_MAX_VOL 4
//total storage size in bytes
#define AUDIO_NVM_TOTAL_BYTES 32

//default values for audio path settings (volatile and non-volatile)
#define AUDIO_DEFAULT_VOLUME_DB -30.0f
#define AUDIO_DEFAULT_MIN_VOLUME_DB -50.0f
#define AUDIO_DEFAULT_MAX_VOLUME_DB -20.0f
#define AUDIO_DEFAULT_VOLUME_STEP_DB 2.0f
#define AUDIO_DEFAULT_POS_GAIN_ALLOWED false
#define AUDIO_DEFAULT_LOUDNESS_GAIN_DB -INFINITY
#define AUDIO_DEFAULT_LOUDNESS_TRACK_MAX_VOL false

//TODO DAP mixer setup, DAP filter constants

//calibrated volume gain offsets, per channel, for DAC and DAP, in dB
#define AUDIO_GAIN_OFFSET_DAC_CH1 -6.0f
#define AUDIO_GAIN_OFFSET_DAC_CH2 -6.0f
#define AUDIO_GAIN_OFFSET_DAP_CH1 0.0f
#define AUDIO_GAIN_OFFSET_DAP_CH2 0.0f


AudioPathManager::AudioPathManager(BlockBoxV2System& system) :
    system(system), non_volatile_config(system.eeprom_if, AUDIO_NVM_TOTAL_BYTES, AudioPathManager::LoadNonVolatileConfigDefaults), initialised(false), lock_timer(0),
    persistent_active_input(AUDIO_INPUT_NONE), current_volume_dB(AUDIO_DEFAULT_VOLUME_DB), min_volume_dB(AUDIO_DEFAULT_MIN_VOLUME_DB),
    max_volume_dB(AUDIO_DEFAULT_MAX_VOLUME_DB), volume_step_dB(AUDIO_DEFAULT_VOLUME_STEP_DB) {}


//TODO: handling of power state switching (need to mute and restore volatile defaults for every(?) power state switch, and configure bluetooth connectivity accordingly)

void AudioPathManager::Init(SuccessCallback&& callback) {
  //TODO: register all relevant event handlers, handle initial off-state, set volatile defaults, write DAP I2S, mixer, filter, and loudness configs, write DAC configs
}

void AudioPathManager::LoopTasks() {
  if (!this->initialised) {
    return;
  }

  //decrement lock timer, under disabled interrupts
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  if (this->lock_timer > 0) {
    this->lock_timer--;
  }
  __set_PRIMASK(primask);
}


//input functions
AudioPathInput AudioPathManager::DetermineActiveInput() const {
  //if DAP has an active input, return that
  DAPInput dap_input = this->system.dap_if.GetActiveInput();
  switch (dap_input) {
    case IF_DAP_INPUT_I2S1:
    case IF_DAP_INPUT_USB:
    case IF_DAP_INPUT_SPDIF:
      return (AudioPathInput)dap_input;
    default:
      break;
  }

  //use persistent active input if it's still available
  if (this->IsInputAvailable(this->persistent_active_input)) {
    return this->persistent_active_input;
  }

  //persistent active input no longer available: check for any inputs that may be available but not playing
  if (this->IsInputAvailable(AUDIO_INPUT_BLUETOOTH)) {
    return AUDIO_INPUT_BLUETOOTH;
  } else if (this->IsInputAvailable(AUDIO_INPUT_USB)) {
    return AUDIO_INPUT_USB;
  }

  return AUDIO_INPUT_NONE;
}

AudioPathInput AudioPathManager::GetActiveInput() {
  AudioPathInput determined_input = this->DetermineActiveInput();

  //on change: notify event handlers
  if (determined_input != this->persistent_active_input) {
    this->persistent_active_input = determined_input;
    this->ExecuteCallbacks(AUDIO_EVENT_INPUT_UPDATE);
  }

  return this->persistent_active_input;
}

bool AudioPathManager::IsInputAvailable(AudioPathInput input) const {
  //if available in DAP, definitely true
  if (this->system.dap_if.IsInputAvailable((DAPInput)input)) {
    return true;
  }

  //check for inputs that may be passively available but not streaming audio
  switch (input) {
    case AUDIO_INPUT_BLUETOOTH:
    {
      BluetoothReceiverStatus bt_status = this->system.btrx_if.GetStatus();
      return bt_status.connected && bt_status.a2dp_link;
    }
    case AUDIO_INPUT_USB:
      return this->system.dap_if.GetStatus().usb_connected;
    default:
      return false;
  }
}


void AudioPathManager::SetActiveInput(AudioPathInput input, SuccessCallback&& callback) {
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  if (!this->initialised || this->lock_timer > 0 || !this->IsInputAvailable(input)) {
    //uninitialised, locked out, or unavailable input: failure, propagate to callback
    __set_PRIMASK(primask);
    if (callback) {
      callback(false);
    }
    return;
  }

  //lock out further operations
  this->lock_timer = AUDIO_LOCK_TIMEOUT_CYCLES;
  __set_PRIMASK(primask);

  auto update_cb = [&, callback = std::move(callback), input](bool success) {
    //if successful and input is different from saved: update and notify event handlers
    if (success && input != this->persistent_active_input) {
      this->persistent_active_input = input;
      this->ExecuteCallbacks(AUDIO_EVENT_INPUT_UPDATE);
    }

    //unlock operations and propagate success to external callback
    this->lock_timer = 0;
    if (callback) {
      callback(success);
    }
  };

  DAPInput dap_active_input = this->system.dap_if.GetActiveInput();
  if (this->system.dap_if.IsInputAvailable((DAPInput)input)) {
    //available in DAP: check if already active in DAP
    if (dap_active_input == (DAPInput)input) {
      //already active in DAP: just perform update of persistent input (if needed)
      update_cb(true);
    } else {
      //different from already active DAP input: set DAP input
      this->system.dap_if.SetActiveInput((DAPInput)input, std::move(update_cb));
    }
  } else if (dap_active_input == IF_DAP_INPUT_NONE) {
    //not available in DAP, and DAP is not receiving any audio: just perform update of persistent input
    update_cb(true);
  } else {
    //not available in DAP, and DAP is receiving audio from another input: don't switch, active audio overrides user's chosen passive input
    update_cb(false);
  }
}


//current absolute volume functions
float AudioPathManager::GetCurrentVolumeDB() const {
  return this->current_volume_dB;
}


void AudioPathManager::CheckAndFixVolumeLimits() {
  if (isnanf(this->min_volume_dB) || this->min_volume_dB < AUDIO_LIMIT_MIN_VOLUME_MIN || this->min_volume_dB > AUDIO_LIMIT_MIN_VOLUME_MAX ||
      isnanf(this->max_volume_dB) || this->max_volume_dB < AUDIO_LIMIT_MAX_VOLUME_MIN || this->max_volume_dB > AUDIO_LIMIT_MAX_VOLUME_MAX) {
    //invalid minimum and/or maximum: reset to defaults
    DEBUG_PRINTF("* AudioPathManager CheckAndFixVolumeLimits found invalid volume limits %.1f %.1f\n", this->min_volume_dB, this->max_volume_dB);
    this->min_volume_dB = AUDIO_DEFAULT_MIN_VOLUME_DB;
    this->max_volume_dB = AUDIO_DEFAULT_MAX_VOLUME_DB;
  }

  if (this->max_volume_dB > 0.0f && !this->IsPositiveGainAllowed()) {
    //positive gain when not allowed: clamp to zero
    DEBUG_PRINTF("* AudioPathManager CheckAndFixVolumeLimits found disallowed positive gain limit %.1f\n", this->max_volume_dB);
    this->max_volume_dB = 0.0f;
  }

  if (this->max_volume_dB - this->min_volume_dB < AUDIO_LIMIT_VOLUME_RANGE_MIN) {
    //range between minimum and maximum volume is too small: decrease minimum to extend range
    DEBUG_PRINTF("* AudioPathManager CheckAndFixVolumeLimits found volume limits %.1f %.1f with insufficient range\n", this->min_volume_dB, this->max_volume_dB);
    this->min_volume_dB = this->max_volume_dB - AUDIO_LIMIT_VOLUME_RANGE_MIN;
  }
}

void AudioPathManager::CheckAndFixVolumeStep() {
  if (isnanf(this->volume_step_dB) || this->volume_step_dB < AUDIO_LIMIT_VOLUME_STEP_MIN || this->volume_step_dB > AUDIO_LIMIT_VOLUME_STEP_MAX) {
    //invalid step: reset to default
    DEBUG_PRINTF("* AudioPathManager CheckAndFixVolumeStep found invalid volume step %.1f\n", this->volume_step_dB);
    this->volume_step_dB = AUDIO_DEFAULT_VOLUME_STEP_DB;
  }

  //ensure the step is an integer dB multiple
  this->volume_step_dB = roundf(this->volume_step_dB);
}

void AudioPathManager::ClampAndApplyVolumeGain(float desired_gain_dB, SuccessCallback&& callback) {
  this->CheckAndFixVolumeLimits();

  //clamp to min/max limits
  float clamped_gain;
  if (isnanf(desired_gain_dB) || desired_gain_dB < this->min_volume_dB) {
    clamped_gain = this->min_volume_dB;
  } else if (desired_gain_dB > this->max_volume_dB) {
    clamped_gain = this->max_volume_dB;
  } else {
    clamped_gain = desired_gain_dB;
  }

  //split gain into DAC and DAP gains
  float dac_gain, dap_gain;
  if (this->IsLoudnessTrackingMaxVolume()) {
    //loudness tracks max volume: apply max volume limit (if <0dB) in DAC, remaining gain in DAP
    dac_gain = MIN(this->max_volume_dB, 0.0f);
    dap_gain = clamped_gain - dac_gain;
  } else {
    //loudness doesn't track max volume: set DAC gain to 0dB, apply all gain in DAP
    dac_gain = 0.0f;
    dap_gain = clamped_gain;
  }

  //calculate target gains (per channel)
  float new_dac_gain_ch1 = dac_gain + AUDIO_GAIN_OFFSET_DAC_CH1;
  float new_dac_gain_ch2 = dac_gain + AUDIO_GAIN_OFFSET_DAC_CH2;
  DAPGains new_dap_gains;
  new_dap_gains.ch1 = dap_gain + AUDIO_GAIN_OFFSET_DAP_CH1;
  new_dap_gains.ch2 = dap_gain + AUDIO_GAIN_OFFSET_DAP_CH2;

  //get current gains (per channel)
  float current_dac_gain_ch1, current_dac_gain_ch2;
  this->system.dac_if.GetVolumesAsGains(current_dac_gain_ch1, current_dac_gain_ch2);
  DAPGains current_dap_gains = this->system.dap_if.GetVolumeGains();

  //completion callback (for after adjustments)
  auto completion_cb = [&, callback = std::move(callback), clamped_gain](bool success) {
    //if successful and gain is different from saved: update and notify event handlers
    if (success && clamped_gain != this->current_volume_dB) {
      this->current_volume_dB = clamped_gain;
      //TODO update bluetooth volume
      this->ExecuteCallbacks(AUDIO_EVENT_VOLUME_UPDATE);
    }

    //propagate success to external callback
    if (callback) {
      callback(success);
    }
  };

  //check if DAC or DAP already have the right gains
  if (new_dac_gain_ch1 == current_dac_gain_ch1 && new_dac_gain_ch2 == current_dac_gain_ch2) {
    //DAC is already correct: check DAP too, maybe we don't need to do anything
    if (new_dap_gains.ch1 == current_dap_gains.ch1 && new_dap_gains.ch2 == current_dap_gains.ch2) {
      //both DAC and DAP already correct: nothing to do, go to completion callback
      completion_cb(true);
      return;
    }

    //DAP needs adjustment: send new gains, then go to completion callback
    this->system.dap_if.SetVolumeGains(new_dap_gains, std::move(completion_cb));
  } else if (new_dap_gains.ch1 == current_dap_gains.ch1 && new_dap_gains.ch2 == current_dap_gains.ch2) {
    //DAP is already correct: only adjust DAC, then go to completion callback
    this->system.dac_if.SetVolumesFromGains(new_dac_gain_ch1, new_dac_gain_ch2, std::move(completion_cb));
  } else {
    //we need to adjust both DAC and DAP: calculate max gain differences for each, to apply smallest ("least positive") change first
    float dac_gain_diff = MAX(new_dac_gain_ch1 - current_dac_gain_ch1, new_dac_gain_ch2 - current_dac_gain_ch2);
    float dap_gain_diff = MAX(new_dap_gains.ch1 - current_dap_gains.ch1, new_dap_gains.ch2 - current_dap_gains.ch2);

    if (dac_gain_diff > dap_gain_diff) {
      //DAC difference is larger, so apply DAP first
      this->system.dap_if.SetVolumeGains(new_dap_gains, [&, completion_cb = std::move(completion_cb), new_dac_gain_ch1, new_dac_gain_ch2, current_dap_gains](bool success) {
        if (!success) {
          //propagate failure to completion callback
          DEBUG_PRINTF("* AudioPathManager ClampAndApplyVolumeGain DAP-DAC write failed at DAP\n");
          completion_cb(false);
          return;
        }

        //apply DAC afterwards
        this->system.dac_if.SetVolumesFromGains(new_dac_gain_ch1, new_dac_gain_ch2, [&, completion_cb = std::move(completion_cb), current_dap_gains](bool success) {
          if (success) {
            //all good: propagate success to completion callback
            completion_cb(true);
          } else {
            //DAP gain application succeeded, but DAC application failed: attempt to reset DAP gains to what they were
            DEBUG_PRINTF("* AudioPathManager ClampAndApplyVolumeGain DAP-DAC write failed at DAC, attempting DAP restore\n");
            this->system.dap_if.SetVolumeGains(current_dap_gains, [&, completion_cb = std::move(completion_cb)](bool success) {
              if (!success) {
                DEBUG_PRINTF("*** AudioPathManager ClampAndApplyVolumeGain DAP-DAC write failed to restore DAP after DAC write failure!\n");
              }

              //report failure to completion callback (regardless of restore success)
              completion_cb(false);
            });
          }
        });
      });
    } else {
      //DAP difference is larger (or equal), so apply DAC first
      this->system.dac_if.SetVolumesFromGains(new_dac_gain_ch1, new_dac_gain_ch2, [&, completion_cb = std::move(completion_cb), new_dap_gains, current_dac_gain_ch1, current_dac_gain_ch2](bool success) {
        if (!success) {
          //propagate failure to completion callback
          DEBUG_PRINTF("* AudioPathManager ClampAndApplyVolumeGain DAC-DAP write failed at DAC\n");
          completion_cb(false);
          return;
        }

        //apply DAP afterwards
        this->system.dap_if.SetVolumeGains(new_dap_gains, [&, completion_cb = std::move(completion_cb), current_dac_gain_ch1, current_dac_gain_ch2](bool success) {
          if (success) {
            //all good: propagate success to completion callback
            completion_cb(true);
          } else {
            //DAC gain application succeeded, but DAP application failed: attempt to reset DAC gains to what they were
            DEBUG_PRINTF("* AudioPathManager ClampAndApplyVolumeGain DAC-DAP write failed at DAP, attempting DAC restore\n");
            this->system.dac_if.SetVolumesFromGains(current_dac_gain_ch1, current_dac_gain_ch2, [&, completion_cb = std::move(completion_cb)](bool success) {
              if (!success) {
                DEBUG_PRINTF("*** AudioPathManager ClampAndApplyVolumeGain DAC-DAP write failed to restore DAC after DAP write failure!\n");
              }

              //report failure to completion callback (regardless of restore success)
              completion_cb(false);
            });
          }
        });
      });
    }
  }
}

void AudioPathManager::SetCurrentVolumeDB(float volume_dB, SuccessCallback&& callback) {
  if (isnanf(volume_dB)) {
    throw std::invalid_argument("AudioPathManager SetCurrentVolumeDB given invalid volume");
  }

  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  if (!this->initialised || this->lock_timer > 0) {
    //uninitialised, locked out, or unavailable input: failure, propagate to callback
    __set_PRIMASK(primask);
    if (callback) {
      callback(false);
    }
    return;
  }

  //lock out further operations
  this->lock_timer = AUDIO_LOCK_TIMEOUT_CYCLES;
  __set_PRIMASK(primask);

  //round volume to nearest step
  this->CheckAndFixVolumeStep();
  float rounded_volume = roundf(volume_dB / this->volume_step_dB) * this->volume_step_dB;

  //clamp and apply volume gain to DAC and DAP
  this->ClampAndApplyVolumeGain(rounded_volume, [&, callback = std::move(callback)](bool success) {
    //once done: unlock operations and propagate success to external callback
    this->lock_timer = 0;
    if (callback) {
      callback(success);
    }
  });
}

void AudioPathManager::ChangeCurrentVolumeDB(float volume_change_dB, SuccessCallback&& callback) {
  if (isnanf(volume_change_dB)) {
    throw std::invalid_argument("AudioPathManager ChangeCurrentVolumeDB given NaN volume change");
  }

  this->SetCurrentVolumeDB(this->current_volume_dB + volume_change_dB, std::move(callback));
}

void AudioPathManager::StepCurrentVolumeDB(bool up, SuccessCallback&& callback) {
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  this->CheckAndFixVolumeStep();

  float vol_change = up ? this->volume_step_dB : -this->volume_step_dB;
  __set_PRIMASK(primask);

  this->ChangeCurrentVolumeDB(vol_change, std::move(callback));
}


//current relative volume functions (0 to 1, relative to current min-max range)
float AudioPathManager::GetCurrentRelativeVolume() const {
  if (isnanf(this->current_volume_dB) || isnanf(this->min_volume_dB) || isnanf(this->max_volume_dB) ||
      this->current_volume_dB < this->min_volume_dB || this->current_volume_dB > this->max_volume_dB) {
    //something's wrong, return 0
    DEBUG_PRINTF("* AudioPathManager GetCurrentRelativeVolume found invalid volume %.1f or invalid limits\n", this->current_volume_dB);
    return 0.0f;
  }

  return (this->current_volume_dB - this->min_volume_dB) / (this->max_volume_dB - this->min_volume_dB);
}


void AudioPathManager::SetCurrentRelativeVolume(float relative_volume, SuccessCallback&& callback) {
  if (isnanf(relative_volume) || relative_volume < 0.0f || relative_volume > 1.0f) {
    throw std::invalid_argument("AudioPathManager SetCurrentRelativeVolume given invalid relative volume, must be in [0, 1]");
  }

  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  this->CheckAndFixVolumeLimits();

  float vol_dB = this->min_volume_dB + relative_volume * (this->max_volume_dB - this->min_volume_dB);
  __set_PRIMASK(primask);

  this->SetCurrentVolumeDB(vol_dB, std::move(callback));
}


//volume min/max/step functions
float AudioPathManager::GetMinVolumeDB() const {
  return this->min_volume_dB;
}

float AudioPathManager::GetMaxVolumeDB() const {
  return this->max_volume_dB;
}

float AudioPathManager::GetVolumeStepDB() const {
  return this->volume_step_dB;
}

bool AudioPathManager::IsPositiveGainAllowed() const {
  bool sp_en, pos_gain;
  this->system.dap_if.GetConfig(sp_en, pos_gain);
  return pos_gain;
}


void AudioPathManager::SetMinVolumeDB(float min_volume_dB, SuccessCallback&& callback) {
  if (isnanf(min_volume_dB) || min_volume_dB < AUDIO_LIMIT_MIN_VOLUME_MIN || min_volume_dB > AUDIO_LIMIT_MIN_VOLUME_MAX) {
    throw std::invalid_argument("AudioPathManager SetMinVolumeDB given invalid min volume");
  }

  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  if (!this->initialised || this->lock_timer > 0) {
    //uninitialised, locked out, or unavailable input: failure, propagate to callback
    __set_PRIMASK(primask);
    if (callback) {
      callback(false);
    }
    return;
  }

  //lock out further operations
  this->lock_timer = AUDIO_LOCK_TIMEOUT_CYCLES;
  __set_PRIMASK(primask);

  this->CheckAndFixVolumeLimits();

  //clamp to ensure sufficient min-max volume range
  float clamped_min_volume;
  if (this->max_volume_dB - min_volume_dB < AUDIO_LIMIT_VOLUME_RANGE_MIN) {
    clamped_min_volume = this->max_volume_dB - AUDIO_LIMIT_VOLUME_RANGE_MIN;
  } else {
    clamped_min_volume = min_volume_dB;
  }

  if (clamped_min_volume == this->min_volume_dB) {
    //already at desired min volume: nothing to do, unlock operations and report to callback
    this->lock_timer = 0;
    if (callback) {
      callback(true);
    }
  } else {
    //apply new min volume and refresh current volume gain (to ensure it's within the new range)
    this->min_volume_dB = clamped_min_volume;
    this->ClampAndApplyVolumeGain(this->current_volume_dB, [&, callback = std::move(callback)](bool success) {
      if (!success) {
        DEBUG_PRINTF("* AudioPathManager SetMinVolumeDB failed to re-apply current volume\n");
      }

      //once done: unlock operations and report to external callback (min volume application is successful, regardless of current volume re-application success)
      this->lock_timer = 0;
      if (callback) {
        callback(true);
      }
    });
  }
}

void AudioPathManager::SetMaxVolumeDB(float max_volume_dB, SuccessCallback&& callback) {
  if (isnanf(max_volume_dB) || max_volume_dB < AUDIO_LIMIT_MAX_VOLUME_MIN || max_volume_dB > AUDIO_LIMIT_MAX_VOLUME_MAX) {
    throw std::invalid_argument("AudioPathManager SetMaxVolumeDB given invalid max volume");
  }

  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  if (!this->initialised || this->lock_timer > 0) {
    //uninitialised, locked out, or unavailable input: failure, propagate to callback
    __set_PRIMASK(primask);
    if (callback) {
      callback(false);
    }
    return;
  }

  //lock out further operations
  this->lock_timer = AUDIO_LOCK_TIMEOUT_CYCLES;
  __set_PRIMASK(primask);

  this->CheckAndFixVolumeLimits();

  //clamp to 0 or less if positive gain is disallowed
  float clamped_max_volume;
  if (max_volume_dB > 0.0f && !this->IsPositiveGainAllowed()) {
    clamped_max_volume = 0.0f;
  } else {
    clamped_max_volume = max_volume_dB;
  }

  if (clamped_max_volume == this->max_volume_dB) {
    //already at desired max volume: nothing to do, unlock operations and report to callback
    this->lock_timer = 0;
    if (callback) {
      callback(true);
    }
  } else {
    //decrease min volume if the new max volume would result in an insufficient min-max range
    if (clamped_max_volume - this->min_volume_dB < AUDIO_LIMIT_VOLUME_RANGE_MIN) {
      this->min_volume_dB = clamped_max_volume - AUDIO_LIMIT_VOLUME_RANGE_MIN;
    }

    //apply new max volume and refresh current volume gain (to ensure it's within the new range, and distributed between DAC and DAP correctly)
    this->max_volume_dB = clamped_max_volume;
    this->ClampAndApplyVolumeGain(this->current_volume_dB, [&, callback = std::move(callback)](bool success) {
      if (!success) {
        DEBUG_PRINTF("* AudioPathManager SetMaxVolumeDB failed to re-apply current volume\n");
      }

      //once done: unlock operations and report to external callback (max volume application is successful, regardless of current volume re-application success)
      this->lock_timer = 0;
      if (callback) {
        callback(true);
      }
    });
  }
}

void AudioPathManager::SetVolumeStepDB(float volume_step_dB, SuccessCallback&& callback) {
  if (isnanf(volume_step_dB) || volume_step_dB < AUDIO_LIMIT_VOLUME_STEP_MIN || volume_step_dB > AUDIO_LIMIT_VOLUME_STEP_MAX) {
    throw std::invalid_argument("AudioPathManager SetVolumeStepDB given invalid volume step");
  }

  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  if (!this->initialised || this->lock_timer > 0) {
    //uninitialised, locked out, or unavailable input: failure, propagate to callback
    __set_PRIMASK(primask);
    if (callback) {
      callback(false);
    }
    return;
  }

  //lock out further operations
  this->lock_timer = AUDIO_LOCK_TIMEOUT_CYCLES;
  __set_PRIMASK(primask);

  this->CheckAndFixVolumeStep();

  //round step to next integer dB multiple
  float rounded_step = roundf(volume_step_dB);

  if (rounded_step == this->volume_step_dB) {
    //already at desired volume step: nothing to do, unlock operations and report to callback
    this->lock_timer = 0;
    if (callback) {
      callback(true);
    }
  } else {
    //apply new volume step
    this->volume_step_dB = rounded_step;

    //align current volume gain with new step size
    float aligned_volume = roundf(this->current_volume_dB / this->volume_step_dB) * this->volume_step_dB;

    this->CheckAndFixVolumeLimits();
    if (this->current_volume_dB == aligned_volume || this->current_volume_dB == this->min_volume_dB || this->current_volume_dB == this->max_volume_dB) {
      //already aligned, or at either volume limit: nothing more to do, unlock operations and report to callback
      this->lock_timer = 0;
      if (callback) {
        callback(true);
      }
    } else {
      //not aligned and not at limit: apply new aligned volume
      this->ClampAndApplyVolumeGain(aligned_volume, [&, callback = std::move(callback)](bool success) {
        if (!success) {
          DEBUG_PRINTF("* AudioPathManager SetVolumeStepDB failed to apply re-aligned volume\n");
        }

        //once done: unlock operations and report to external callback (volume step application is successful, regardless of current volume re-application success)
        this->lock_timer = 0;
        if (callback) {
          callback(true);
        }
      });
    }
  }
}

void AudioPathManager::SetPositiveGainAllowed(bool pos_gain_allowed, SuccessCallback&& callback) {
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  if (!this->initialised || this->lock_timer > 0) {
    //uninitialised, locked out, or unavailable input: failure, propagate to callback
    __set_PRIMASK(primask);
    if (callback) {
      callback(false);
    }
    return;
  }

  //lock out further operations
  this->lock_timer = AUDIO_LOCK_TIMEOUT_CYCLES;
  __set_PRIMASK(primask);

  //get current DAP config (mainly: whether positive gain is currently allowed)
  bool sp_enabled, pos_gain_currently_allowed;
  this->system.dap_if.GetConfig(sp_enabled, pos_gain_currently_allowed);

  if (pos_gain_allowed == pos_gain_currently_allowed) {
    //already in desired state: nothing to do, unlock operations and report to callback
    this->lock_timer = 0;
    if (callback) {
      callback(true);
    }
  } else {
    this->CheckAndFixVolumeLimits();
    if (pos_gain_allowed || this->max_volume_dB <= 0.0f) {
      //want to allow, or disallow with the maximum volume already non-positive: just set the corresponding DAP config
      this->system.dap_if.SetConfig(sp_enabled, pos_gain_allowed, [&, callback = std::move(callback)](bool success) {
        //once done: unlock operations and report success to callback
        this->lock_timer = 0;
        if (callback) {
          callback(success);
        }
      });
    } else {
      //want to disallow, but maximum volume is positive: lower maximum volume to 0dB and re-apply volume gain
      //decrease min volume if the new max volume would result in an insufficient min-max range
      if (this->min_volume_dB > -AUDIO_LIMIT_VOLUME_RANGE_MIN) {
        this->min_volume_dB = -AUDIO_LIMIT_VOLUME_RANGE_MIN;
      }
      this->max_volume_dB = 0.0f;
      this->ClampAndApplyVolumeGain(this->current_volume_dB, [&, callback = std::move(callback), sp_enabled, pos_gain_allowed](bool success) {
        if (!success) {
          DEBUG_PRINTF("* AudioPathManager SetPositiveGainAllowed failed to re-apply current volume after clamping max volume to 0\n");
        }

        //afterwards, set the corresponding DAP config (regardless of current volume re-application success)
        this->system.dap_if.SetConfig(sp_enabled, pos_gain_allowed, [&, callback = std::move(callback)](bool success) {
          //once done: unlock operations and report success to callback
          this->lock_timer = 0;
          if (callback) {
            callback(success);
          }
        });
      });
    }
  }
}


//mute functions
bool AudioPathManager::IsMute() const {
  bool mute_ch1, mute_ch2;
  this->system.dac_if.GetMutes(mute_ch1, mute_ch2);
  return mute_ch1 && mute_ch2;
}


void AudioPathManager::SetMute(bool mute, SuccessCallback&& callback) {
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  if (!this->initialised || this->lock_timer > 0) {
    //uninitialised, locked out, or unavailable input: failure, propagate to callback
    __set_PRIMASK(primask);
    if (callback) {
      callback(false);
    }
    return;
  }

  //lock out further operations
  this->lock_timer = AUDIO_LOCK_TIMEOUT_CYCLES;
  __set_PRIMASK(primask);

  if (mute == this->IsMute()) {
    //already in desired state: nothing to do, unlock operations and report to callback
    this->lock_timer = 0;
    if (callback) {
      callback(true);
    }
  } else {
    //write new mute state to DAC
    this->system.dac_if.SetMutes(mute, mute, [&, callback = std::move(callback)](bool success) {
      //notify event listeners if successful
      if (success) {
        //TODO update bluetooth volume (as it includes mute state)
        this->ExecuteCallbacks(AUDIO_EVENT_MUTE_UPDATE);
      }

      //unlock operations and report success to callback
      this->lock_timer = 0;
      if (callback) {
        callback(success);
      }
    });
  }
}


//loudness functions
float AudioPathManager::GetLoudnessGainDB() const {
  DAPGains loudness_gains = this->system.dap_if.GetLoudnessGains();
  //report maximum loudness gain - these should really be the same at all times, but it's better to over-report than under-report
  return MAX(loudness_gains.ch1, loudness_gains.ch2);
}

bool AudioPathManager::IsLoudnessTrackingMaxVolume() const {
  return this->non_volatile_config.GetValue8(AUDIO_NVM_LOUDNESS_TRACK_MAX_VOL) != 0;
}


void AudioPathManager::SetLoudnessGainDB(float loudness_gain_dB, SuccessCallback&& callback) {
  if (isnanf(loudness_gain_dB) || loudness_gain_dB > AUDIO_LIMIT_LOUDNESS_GAIN_MAX) {
    throw std::invalid_argument("AudioPathManager SetLoudnessGainDB given invalid loudness gain");
  }

  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  if (!this->initialised || this->lock_timer > 0) {
    //uninitialised, locked out, or unavailable input: failure, propagate to callback
    __set_PRIMASK(primask);
    if (callback) {
      callback(false);
    }
    return;
  }

  //lock out further operations
  this->lock_timer = AUDIO_LOCK_TIMEOUT_CYCLES;
  __set_PRIMASK(primask);

  DAPGains new_loudness_gains;
  if (loudness_gain_dB < AUDIO_LIMIT_LOUDNESS_GAIN_MIN_ACTIVE) {
    //desired loudness gain is below the minimum active gain: clamp it down to negative infinity (loudness disabled)
    new_loudness_gains.ch1 = new_loudness_gains.ch2 = -INFINITY;
  } else {
    //use desired loudness gain directly (loudness enabled)
    new_loudness_gains.ch1 = new_loudness_gains.ch2 = loudness_gain_dB;
  }

  //get the current loudness gains
  DAPGains current_loudness_gains = this->system.dap_if.GetLoudnessGains();

  if (new_loudness_gains.ch1 == current_loudness_gains.ch1 && new_loudness_gains.ch2 == current_loudness_gains.ch2) {
    //already have the desired loudness settings: nothing to do, unlock operations and report to callback
    this->lock_timer = 0;
    if (callback) {
      callback(true);
    }
  } else {
    //write new loudness gains to the DAP
    this->system.dap_if.SetLoudnessGains(new_loudness_gains, [&, callback = std::move(callback), new_gain = new_loudness_gains.ch1](bool success) {
      //if successful, save to non-volatile config as new initial loudness gain
      if (success) {
        this->non_volatile_config.SetValue32(AUDIO_NVM_INIT_LOUDNESS_GAIN, *(uint32_t*)&new_gain);
      }

      //once done: unlock operations and propagate success to external callback
      this->lock_timer = 0;
      if (callback) {
        callback(success);
      }
    });
  }
}

void AudioPathManager::SetLoudnessTrackingMaxVolume(bool track_max_volume, SuccessCallback&& callback) {
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  if (!this->initialised || this->lock_timer > 0) {
    //uninitialised, locked out, or unavailable input: failure, propagate to callback
    __set_PRIMASK(primask);
    if (callback) {
      callback(false);
    }
    return;
  }

  //lock out further operations (even if we don't do any async operations, still want to avoid concurrent modifications)
  this->lock_timer = AUDIO_LOCK_TIMEOUT_CYCLES;
  __set_PRIMASK(primask);

  this->non_volatile_config.SetValue8(AUDIO_NVM_LOUDNESS_TRACK_MAX_VOL, track_max_volume ? 1 : 0);

  //unlock operations and report to external callback
  this->lock_timer = 0;
  if (callback) {
    callback(true);
  }
}



void AudioPathManager::LoadNonVolatileConfigDefaults(StorageSection& section) {
  //write default initial loudness gain
  float default_loudness = AUDIO_DEFAULT_LOUDNESS_GAIN_DB;
  section.SetValue32(AUDIO_NVM_INIT_LOUDNESS_GAIN, *(uint32_t*)&default_loudness);

  //write default loudness tracking behaviour
  section.SetValue8(AUDIO_NVM_LOUDNESS_TRACK_MAX_VOL, AUDIO_DEFAULT_LOUDNESS_TRACK_MAX_VOL ? 1 : 0);
}

