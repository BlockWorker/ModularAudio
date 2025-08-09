/*
 * audio_path_manager.cpp
 *
 *  Created on: Jul 22, 2025
 *      Author: Alex
 */


#include "audio_path_manager.h"
#include "system.h"


/******************************************************/
/*       Constant definitions and configuration       */
/******************************************************/

//non-volatile storage layout
//initial loudness gain in dB, float (4B)
#define AUDIO_NVM_INIT_LOUDNESS_GAIN 0
//whether the loudness target tracks the maximum volume, bool (1B)
#define AUDIO_NVM_LOUDNESS_TRACK_MAX_VOL 4
//mixer config (1B)
#define AUDIO_NVM_MIXER_CONFIG 5
//volume step in dB, float (4B)
#define AUDIO_NVM_VOLUME_STEP 8
//total storage size in bytes
#define AUDIO_NVM_TOTAL_BYTES 12

//default values for audio path settings (volatile and non-volatile)
#define AUDIO_DEFAULT_VOLUME_DB -30.0f
#define AUDIO_DEFAULT_MIN_VOLUME_DB -50.0f
#define AUDIO_DEFAULT_MAX_VOLUME_DB -20.0f
#define AUDIO_DEFAULT_VOLUME_STEP_DB 2.0f
#define AUDIO_DEFAULT_POS_GAIN_ALLOWED false
#define AUDIO_DEFAULT_LOUDNESS_GAIN_DB -INFINITY
#define AUDIO_DEFAULT_LOUDNESS_TRACK_MAX_VOL false

//calibrated volume gain offsets, per channel, for DAC and DAP, in dB
#define AUDIO_GAIN_OFFSET_DAC_CH1 -10.0f
#define AUDIO_GAIN_OFFSET_DAC_CH2 -5.0f
#define AUDIO_GAIN_OFFSET_DAP_CH1 0.0f
#define AUDIO_GAIN_OFFSET_DAP_CH2 0.0f

//Bluetooth absolute volume offset (value) for non-muted min volume (in 0-127 range)
#define AUDIO_BLUETOOTH_VOL_OFFSET 7
//Bluetooth absolute volume margin (steps within 0-127 range) - volume changes of this size or more are sent to the device
#define AUDIO_BLUETOOTH_VOL_MARGIN 4

//DAC configuration
#define AUDIO_DAC_SYNC_MODE             true
#define AUDIO_DAC_MASTER_MODE           false
#define AUDIO_DAC_ENABLE_AUTOMUTE       true
#define AUDIO_DAC_INVERT_CH1            false
#define AUDIO_DAC_INVERT_CH2            false
#define AUDIO_DAC_4XGAIN_CH1            false
#define AUDIO_DAC_4XGAIN_CH2            false
#define AUDIO_DAC_ENABLE_MUTE_GND_RAMP  true
#define AUDIO_DAC_INTERNAL_CLOCK_CFG    0x80
#define AUDIO_DAC_TDM_SLOT_COUNT        2
#define AUDIO_DAC_TDM_SLOT_CH1          0
#define AUDIO_DAC_TDM_SLOT_CH2          1
#define AUDIO_DAC_FILTER_SHAPE          IF_HIFIDAC_FILTER_MIN_PHASE
#define AUDIO_DAC_THD_C2_CH1            0
#define AUDIO_DAC_THD_C2_CH2            0
#define AUDIO_DAC_THD_C3_CH1            0
#define AUDIO_DAC_THD_C3_CH2            0

//DAP I2S1 input sample rate (from Bluetooth Receiver)
#define AUDIO_DAP_I2S1_SAMPLE_RATE IF_DAP_SR_96K


//DAP mixer configs
//avg config: each output channel is average of input channels (mono downmix)
static const DAPMixerConfig _audio_dap_mixer_config_avg = {
//input ch1   input ch2
  0x20000000, 0x20000000, //output ch1
  0x20000000, 0x20000000  //output ch2
};
//left config: each output channel is copy of left channel (mono select)
static const DAPMixerConfig _audio_dap_mixer_config_left = {
//input ch1   input ch2
  0x40000000, 0x00000000, //output ch1
  0x40000000, 0x00000000  //output ch2
};

//DAP biquad configs, per channel
//hifi config
static const uint32_t _audio_dap_biquad_hifi_coeffs_ch1[I2CDEF_DAP_REG_SIZE_SP_BIQUAD / sizeof(uint32_t)] = {
#include "../Data/biquad_coeffs_hifi_tweeter.txt"
};
static const uint32_t _audio_dap_biquad_hifi_coeffs_ch2[I2CDEF_DAP_REG_SIZE_SP_BIQUAD / sizeof(uint32_t)] = {
#include "../Data/biquad_coeffs_hifi_woofer.txt"
};
static const DAPBiquadSetup _audio_dap_biquad_hifi_setup = { 11, 12, 1, 1 };

//power config
static const uint32_t _audio_dap_biquad_power_coeffs_ch1[I2CDEF_DAP_REG_SIZE_SP_BIQUAD / sizeof(uint32_t)] = {
#include "../Data/biquad_coeffs_power_tweeter.txt"
};
static const uint32_t _audio_dap_biquad_power_coeffs_ch2[I2CDEF_DAP_REG_SIZE_SP_BIQUAD / sizeof(uint32_t)] = {
#include "../Data/biquad_coeffs_power_woofer.txt"
};
static const DAPBiquadSetup _audio_dap_biquad_power_setup = { 0, 0, 1, 1 };

//DAP FIR configs, per channel
static const q31_t _audio_dap_fir_coeffs_ch1[I2CDEF_DAP_REG_SIZE_SP_FIR / sizeof(q31_t)] = {
//#include "..."
  0
};
static const q31_t _audio_dap_fir_coeffs_ch2[I2CDEF_DAP_REG_SIZE_SP_FIR / sizeof(q31_t)] = {
//#include "..."
  0
};
static const DAPFIRSetup _audio_dap_fir_setup = { 0, 0 };


//check validity of audio limits in accordance with DAP limits
static_assert(AUDIO_LIMIT_MIN_VOLUME_MIN + MIN(AUDIO_GAIN_OFFSET_DAP_CH1, AUDIO_GAIN_OFFSET_DAP_CH2) >= IF_DAP_VOLUME_GAIN_MIN);
static_assert(AUDIO_LIMIT_MAX_VOLUME_MAX + MAX(AUDIO_GAIN_OFFSET_DAP_CH1, AUDIO_GAIN_OFFSET_DAP_CH2) <= IF_DAP_VOLUME_GAIN_MAX);
//check that sufficient min-max range is always achievable with the given limits
static_assert(AUDIO_LIMIT_MAX_VOLUME_MIN - AUDIO_LIMIT_MIN_VOLUME_MIN >= AUDIO_LIMIT_VOLUME_RANGE_MIN);
//check that lowest maximum volume is within DAC limits
static_assert(AUDIO_LIMIT_MAX_VOLUME_MIN + MIN(AUDIO_GAIN_OFFSET_DAC_CH1, AUDIO_GAIN_OFFSET_DAC_CH2) >= -127.5f);
//check that default volume, min/max, and step are within the given limits
static_assert(AUDIO_DEFAULT_MIN_VOLUME_DB >= AUDIO_LIMIT_MIN_VOLUME_MIN && AUDIO_DEFAULT_MIN_VOLUME_DB <= AUDIO_LIMIT_MIN_VOLUME_MAX);
static_assert(AUDIO_DEFAULT_MAX_VOLUME_DB >= AUDIO_LIMIT_MAX_VOLUME_MIN && AUDIO_DEFAULT_MAX_VOLUME_DB <= AUDIO_LIMIT_MAX_VOLUME_MAX);
static_assert(AUDIO_DEFAULT_VOLUME_STEP_DB >= AUDIO_LIMIT_VOLUME_STEP_MIN && AUDIO_DEFAULT_VOLUME_STEP_DB <= AUDIO_LIMIT_VOLUME_STEP_MAX);
static_assert(AUDIO_DEFAULT_MAX_VOLUME_DB - AUDIO_DEFAULT_MIN_VOLUME_DB >= AUDIO_LIMIT_VOLUME_RANGE_MIN);
static_assert(AUDIO_DEFAULT_VOLUME_DB >= AUDIO_DEFAULT_MIN_VOLUME_DB && AUDIO_DEFAULT_VOLUME_DB <= AUDIO_DEFAULT_MAX_VOLUME_DB);
static_assert(roundf(AUDIO_DEFAULT_VOLUME_STEP_DB) == AUDIO_DEFAULT_VOLUME_STEP_DB);


/******************************************************/
/*                  Initialisation                    */
/******************************************************/

AudioPathManager::AudioPathManager(BlockBoxV2System& system) :
    system(system), non_volatile_config(system.eeprom_if, AUDIO_NVM_TOTAL_BYTES, AudioPathManager::LoadNonVolatileConfigDefaults), initialised(false), lock_timer(0),
    bluetooth_volume_lock_timer(0), bluetooth_previously_connected(false), persistent_active_input(AUDIO_INPUT_NONE), current_volume_dB(AUDIO_DEFAULT_VOLUME_DB),
    min_volume_dB(AUDIO_DEFAULT_MIN_VOLUME_DB), max_volume_dB(AUDIO_DEFAULT_MAX_VOLUME_DB), eq_mode(AUDIO_EQ_HIFI), calibration_mode(AUDIO_CAL_NONE) {}


void AudioPathManager::InitDACSetup(SuccessCallback&& callback) {
  //write basic config
  HiFiDACConfig cfg;
  cfg.dac_enable = true;
  cfg.sync_mode = AUDIO_DAC_SYNC_MODE;
  cfg.master_mode = AUDIO_DAC_MASTER_MODE;
  this->system.dac_if.SetConfig(cfg, [&, callback = std::move(callback)](bool success) {
    if (!success) {
      //propagate failure to external callback
      DEBUG_PRINTF("* AudioPathManager InitDACSetup failed to write basic config\n");
      if (callback) {
        callback(false);
      }
      return;
    }

    //set up signal path
    HiFiDACSignalPathSetup setup;
    setup.automute_ch1 = setup.automute_ch2 = AUDIO_DAC_ENABLE_AUTOMUTE;
    setup.invert_ch1 = AUDIO_DAC_INVERT_CH1;
    setup.invert_ch2 = AUDIO_DAC_INVERT_CH2;
    setup.gain4x_ch1 = AUDIO_DAC_4XGAIN_CH1;
    setup.gain4x_ch2 = AUDIO_DAC_4XGAIN_CH2;
    setup.mute_gnd_ramp = AUDIO_DAC_ENABLE_MUTE_GND_RAMP;
    this->system.dac_if.SetSignalPathSetup(setup, [&, callback = std::move(callback)](bool success) {
      if (!success) {
        //propagate failure to external callback
        DEBUG_PRINTF("* AudioPathManager InitDACSetup failed to set up signal path\n");
        if (callback) {
          callback(false);
        }
        return;
      }

      //configure internal clock
      HiFiDACInternalClockConfig clock_cfg;
      clock_cfg.value = AUDIO_DAC_INTERNAL_CLOCK_CFG;
      this->system.dac_if.SetInternalClockConfig(clock_cfg, [&, callback = std::move(callback)](bool success) {
        if (!success) {
          //propagate failure to external callback
          DEBUG_PRINTF("* AudioPathManager InitDACSetup failed to configure internal clock\n");
          if (callback) {
            callback(false);
          }
          return;
        }

        //configure TDM slot count
        this->system.dac_if.SetTDMSlotCount(AUDIO_DAC_TDM_SLOT_COUNT, [&, callback = std::move(callback)](bool success) {
          if (!success) {
            //propagate failure to external callback
            DEBUG_PRINTF("* AudioPathManager InitDACSetup failed to configure TDM slot count\n");
            if (callback) {
              callback(false);
            }
            return;
          }

          //configure channel TDM slots
          this->system.dac_if.SetChannelTDMSlots(AUDIO_DAC_TDM_SLOT_CH1, AUDIO_DAC_TDM_SLOT_CH2, [&, callback = std::move(callback)](bool success) {
            if (!success) {
              //propagate failure to external callback
              DEBUG_PRINTF("* AudioPathManager InitDACSetup failed to configure channel TDM slots\n");
              if (callback) {
                callback(false);
              }
              return;
            }

            //configure filter shape
            this->system.dac_if.SetInterpolationFilterShape(AUDIO_DAC_FILTER_SHAPE, [&, callback = std::move(callback)](bool success) {
              if (!success) {
                //propagate failure to external callback
                DEBUG_PRINTF("* AudioPathManager InitDACSetup failed to configure filter shape\n");
                if (callback) {
                  callback(false);
                }
                return;
              }

              //configure second harmonic correction
              this->system.dac_if.SetSecondHarmonicCorrectionCoefficients(AUDIO_DAC_THD_C2_CH1, AUDIO_DAC_THD_C2_CH2, [&, callback = std::move(callback)](bool success) {
                if (!success) {
                  //propagate failure to external callback
                  DEBUG_PRINTF("* AudioPathManager InitDACSetup failed to configure second harmonic correction\n");
                  if (callback) {
                    callback(false);
                  }
                  return;
                }

                //configure third harmonic correction
                this->system.dac_if.SetThirdHarmonicCorrectionCoefficients(AUDIO_DAC_THD_C3_CH1, AUDIO_DAC_THD_C3_CH2, [&, callback = std::move(callback)](bool success) {
                  if (!success) {
                    DEBUG_PRINTF("* AudioPathManager InitDACSetup failed to configure third harmonic correction\n");
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
    });
  });
}

void AudioPathManager::InitDAPSetup(SuccessCallback&& callback) {
  //start by disabling signal processor
  this->system.dap_if.SetConfig(false, false, [&, callback = std::move(callback)](bool success) {
    if (!success) {
      //propagate failure to external callback
      DEBUG_PRINTF("* AudioPathManager InitDAPSetup failed to disable signal processor\n");
      if (callback) {
        callback(false);
      }
      return;
    }

    //configure I2S1 (Bluetooth) input sample rate
    this->system.dap_if.SetI2SInputSampleRate(IF_DAP_INPUT_I2S1, AUDIO_DAP_I2S1_SAMPLE_RATE, [&, callback = std::move(callback)](bool success) {
      if (!success) {
        //propagate failure to external callback
        DEBUG_PRINTF("* AudioPathManager InitDAPSetup failed to configure I2S1 input sample rate\n");
        if (callback) {
          callback(false);
        }
        return;
      }

      //configure initial loudness gains
      //get value from non-volatile config and convert to float
      uint32_t loudness_gain_int = this->non_volatile_config.GetValue32(AUDIO_NVM_INIT_LOUDNESS_GAIN);
      float loudness_gain_float = *(float*)&loudness_gain_int;
      if (isnanf(loudness_gain_float) || loudness_gain_float > AUDIO_LIMIT_LOUDNESS_GAIN_MAX) {
        //value is bad somehow: reset to default
        loudness_gain_float = AUDIO_DEFAULT_LOUDNESS_GAIN_DB;
        this->non_volatile_config.SetValue32(AUDIO_NVM_INIT_LOUDNESS_GAIN, *(uint32_t*)&loudness_gain_float);
      }
      //put gain into structure and apply
      DAPGains loudness_gains;
      loudness_gains.ch1 = loudness_gains.ch2 = loudness_gain_float;
      this->system.dap_if.SetLoudnessGains(loudness_gains, [&, callback = std::move(callback)](bool success) {
        if (!success) {
          //propagate failure to external callback
          DEBUG_PRINTF("* AudioPathManager InitDAPSetup failed to configure initial loudness gains\n");
          if (callback) {
            callback(false);
          }
          return;
        }

        //write FIR coefficients for channel 1
        this->system.dap_if.SetFIRCoefficients(IF_DAP_CH1, _audio_dap_fir_coeffs_ch1, [&, callback = std::move(callback)](bool success) {
          if (!success) {
            //propagate failure to external callback
            DEBUG_PRINTF("* AudioPathManager InitDAPSetup failed to write FIR coefficients for channel 1\n");
            if (callback) {
              callback(false);
            }
            return;
          }

          //write FIR coefficients for channel 2
          this->system.dap_if.SetFIRCoefficients(IF_DAP_CH2, _audio_dap_fir_coeffs_ch2, [&, callback = std::move(callback)](bool success) {
            if (!success) {
              //propagate failure to external callback
              DEBUG_PRINTF("* AudioPathManager InitDAPSetup failed to write FIR coefficients for channel 2\n");
              if (callback) {
                callback(false);
              }
              return;
            }

            //set up FIRs (for both channels)
            this->system.dap_if.SetFIRSetup(_audio_dap_fir_setup, [&, callback = std::move(callback)](bool success) {
              if (!success) {
                //propagate failure to external callback
                DEBUG_PRINTF("* AudioPathManager InitDAPSetup failed to set up FIRs\n");
                if (callback) {
                  callback(false);
                }
                return;
              }

              //check validity of mixer mode
              AudioPathMixerMode mixer_mode = this->GetMixerMode();
              if (mixer_mode != AUDIO_MIXER_AVG && mixer_mode != AUDIO_MIXER_LEFT) {
                //invalid mode: reset
                mixer_mode = AUDIO_MIXER_AVG;
                this->non_volatile_config.SetValue8(AUDIO_NVM_MIXER_CONFIG, mixer_mode);
              }

              //set up mixer, EQ and calibration (no cal) - includes re-enabling the signal processor
              this->UpdateMixerAndEQParams(mixer_mode, AUDIO_EQ_HIFI, AUDIO_CAL_NONE, [&, callback = std::move(callback)](bool success) {
                if (!success) {
                  DEBUG_PRINTF("* AudioPathManager InitDAPSetup failed to set up mixer and EQ\n");
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
  });
}

void AudioPathManager::Init(SuccessCallback&& callback) {
  this->initialised = false;
  this->lock_timer = 0;
  this->bluetooth_volume_lock_timer = 0;

  this->eq_mode = AUDIO_EQ_HIFI;
  this->calibration_mode = AUDIO_CAL_NONE;

  //enter power-off state, including volatile config resets
  this->HandlePowerStateChange(false, [&, callback = std::move(callback)](bool success) {
    if (!success) {
      //propagate failure to external callback
      DEBUG_PRINTF("* AudioPathManager Init failed to enter power-off state\n");
      if (callback) {
        callback(false);
      }
      return;
    }

    //set up DAC
    this->InitDACSetup([&, callback = std::move(callback)](bool success) {
      if (!success) {
        //propagate failure to external callback
        if (callback) {
          callback(false);
        }
        return;
      }

      //set up DAP
      this->InitDAPSetup([&, callback = std::move(callback)](bool success) {
        if (success) {
          //register event callbacks
          this->system.dap_if.RegisterCallback(std::bind(&AudioPathManager::HandleEvent, this, std::placeholders::_1, std::placeholders::_2),
                                               MODIF_DAP_EVENT_STATUS_UPDATE | MODIF_DAP_EVENT_INPUTS_UPDATE);
          this->system.btrx_if.RegisterCallback(std::bind(&AudioPathManager::HandleEvent, this, std::placeholders::_1, std::placeholders::_2),
                                                MODIF_BTRX_EVENT_STATUS_UPDATE | MODIF_BTRX_EVENT_VOLUME_UPDATE);

          //init done
          this->initialised = true;
        }

        //propagate success to external callback
        if (callback) {
          callback(success);
        }
      });
    });
  });
}


/******************************************************/
/*          Basic Tasks and Event Handling            */
/******************************************************/

void AudioPathManager::LoopTasks() {
  static uint32_t loop_count = 0;

  if (!this->initialised) {
    return;
  }

  //decrement lock timers, under disabled interrupts
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  if (this->bluetooth_volume_lock_timer > 0) {
    if (--this->bluetooth_volume_lock_timer == 0) {
      //Bluetooth lock timed out: trigger update to make sure we're up to date
      this->UpdateBluetoothVolume();
    }
  }

  if (this->lock_timer > 0) {
    if (--this->lock_timer == 0) {
      //shouldn't really ever happen, it means an unlock somewhere was missed or delayed excessively
      DEBUG_PRINTF("* AudioPathManager lock timed out!\n");
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

  //if not locked, periodically check active input and Bluetooth connection state
  if (this->lock_timer == 0 && loop_count++ % 50 == 0) {
    primask = __get_PRIMASK();
    __disable_irq();
    AudioPathInput determined_input = this->DetermineActiveInput();

    //on change: notify event handlers
    if (determined_input != this->persistent_active_input) {
      this->persistent_active_input = determined_input;
      __set_PRIMASK(primask);
      this->ExecuteCallbacks(AUDIO_EVENT_INPUT_UPDATE);
    } else {
      __set_PRIMASK(primask);
    }

    primask = __get_PRIMASK();
    __disable_irq();
    //check for new Bluetooth connection
    BluetoothReceiverStatus bt_status = this->system.btrx_if.GetStatus();

    if (bt_status.avrcp_link && !this->bluetooth_previously_connected) {
      //new connection established: update volume from Bluetooth
      this->bluetooth_previously_connected = true;
      __set_PRIMASK(primask);
      this->UpdateVolumeFromBluetooth();
    } else {
      this->bluetooth_previously_connected = bt_status.avrcp_link;
      __set_PRIMASK(primask);
    }
  }
}


void AudioPathManager::HandlePowerStateChange(bool on, SuccessCallback&& callback) {
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  if (this->lock_timer > 0) {
    //locked out: queue for later execution
    this->queued_operations.push_back([&, on, callback = std::move(callback)]() mutable {
      this->HandlePowerStateChange(on, std::move(callback));
    });
    __set_PRIMASK(primask);
    return;
  }

  //lock out further operations, and Bluetooth updates
  this->lock_timer = AUDIO_LOCK_TIMEOUT_CYCLES;
  __set_PRIMASK(primask);
  this->bluetooth_volume_lock_timer = AUDIO_BLUETOOTH_LOCK_TIMEOUT_CYCLES;

  //first thing in all cases: mute DAC
  this->system.dac_if.SetMutes(true, true, [&, on, callback = std::move(callback)](bool success) {
    if (!success) {
      DEBUG_PRINTF("* AudioPathManager HandlePowerStateChange failed to mute DAC\n");
    }

    //continue regardless of success: reset min/max and current volume to safe defaults
    this->min_volume_dB = AUDIO_DEFAULT_MIN_VOLUME_DB;
    this->max_volume_dB = AUDIO_DEFAULT_MAX_VOLUME_DB;
    this->ClampAndApplyVolumeGain(AUDIO_DEFAULT_VOLUME_DB, [&, on, callback = std::move(callback), prev_success = success](bool success) {
      if (!success) {
        DEBUG_PRINTF("* AudioPathManager HandlePowerStateChange failed to apply default volume\n");
      }

      //continue regardless of success: disable positive gain
      bool sp_en, pos_gain_allowed;
      this->system.dap_if.GetConfig(sp_en, pos_gain_allowed);
      this->system.dap_if.SetConfig(sp_en, false, [&, on, callback = std::move(callback), prev_success = prev_success && success](bool success) {
        if (!success) {
          DEBUG_PRINTF("* AudioPathManager HandlePowerStateChange failed to disable positive gain\n");
        }

        //continue regardless of success: configure bluetooth according to state
        if (on) {
          //turning on: set bluetooth connectable
          this->system.btrx_if.SetConnectable(true, [&, callback = std::move(callback), prev_success = prev_success && success](bool success) {
            if (!success) {
              DEBUG_PRINTF("* AudioPathManager HandlePowerStateChange failed to enable Bluetooth connections\n");
            }

            //once done: unlock operations and propagate success (true = all succeeded, false otherwise) to external callback
            this->lock_timer = 0;
            if (callback) {
              callback(prev_success && success);
            }
          });
        } else {
          //turning off: cut and disable bluetooth connections
          this->system.btrx_if.CutAndDisableConnections([&, callback = std::move(callback), prev_success = prev_success && success](bool success) {
            if (!success) {
              DEBUG_PRINTF("* AudioPathManager HandlePowerStateChange failed to cut and disable Bluetooth connections\n");
            }

            //once done: unlock operations and propagate success (true = all succeeded, false otherwise) to external callback
            this->lock_timer = 0;
            if (callback) {
              callback(prev_success && success);
            }
          });
        }
      });
    });
  });
}


void AudioPathManager::HandleEvent(EventSource* source, uint32_t event) {
  if (!this->initialised) {
    return;
  }

  if (source == &this->system.btrx_if && event == MODIF_BTRX_EVENT_VOLUME_UPDATE) {
    this->UpdateVolumeFromBluetooth();
  } else {
    //input update (from DAP input info, DAP status, or BTRX status events)
    this->persistent_active_input = this->DetermineActiveInput();
    //trigger event in all cases, because available inputs may have changed (even if active input didn't)
    this->ExecuteCallbacks(AUDIO_EVENT_INPUT_UPDATE);

    if (source == &this->system.btrx_if && event == MODIF_BTRX_EVENT_STATUS_UPDATE) {
      //Bluetooth status update: check for new connection
      BluetoothReceiverStatus bt_status = this->system.btrx_if.GetStatus();

      if (bt_status.avrcp_link && !this->bluetooth_previously_connected) {
        //new connection established: update volume from Bluetooth
        this->bluetooth_previously_connected = true;
        this->UpdateVolumeFromBluetooth();
      } else {
        this->bluetooth_previously_connected = bt_status.avrcp_link;
      }
    }
  }
}


void AudioPathManager::LoadNonVolatileConfigDefaults(StorageSection& section) {
  //write default initial loudness gain
  float default_loudness = AUDIO_DEFAULT_LOUDNESS_GAIN_DB;
  section.SetValue32(AUDIO_NVM_INIT_LOUDNESS_GAIN, *(uint32_t*)&default_loudness);

  //write default loudness tracking behaviour
  section.SetValue8(AUDIO_NVM_LOUDNESS_TRACK_MAX_VOL, AUDIO_DEFAULT_LOUDNESS_TRACK_MAX_VOL ? 1 : 0);

  //write default mixer config
  section.SetValue8(AUDIO_NVM_MIXER_CONFIG, (uint8_t)AUDIO_MIXER_AVG);

  //write default volume step
  float default_step = AUDIO_DEFAULT_VOLUME_STEP_DB;
  section.SetValue32(AUDIO_NVM_VOLUME_STEP, *(uint32_t*)&default_step);
}


/******************************************************/
/*                  Input Handling                    */
/******************************************************/

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

AudioPathInput AudioPathManager::GetActiveInput() const {
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


void AudioPathManager::SetActiveInput(AudioPathInput input, SuccessCallback&& callback, bool queue_if_busy) {
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
      this->queued_operations.push_back([&, input, callback = std::move(callback)]() mutable {
        this->SetActiveInput(input, std::move(callback), false);
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
  this->lock_timer = AUDIO_LOCK_TIMEOUT_CYCLES;
  __set_PRIMASK(primask);

  //check for input availability
  if (!this->IsInputAvailable(input)) {
    //unavailable: unlock operations and propagate failure to external callback
    this->lock_timer = 0;
    if (callback) {
      callback(false);
    }
  }

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


/******************************************************/
/*                  Volume Handling                   */
/******************************************************/

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
  float vol_step = this->GetVolumeStepDB();
  if (isnanf(vol_step) || vol_step < AUDIO_LIMIT_VOLUME_STEP_MIN || vol_step > AUDIO_LIMIT_VOLUME_STEP_MAX) {
    //invalid step: reset to default
    DEBUG_PRINTF("* AudioPathManager CheckAndFixVolumeStep found invalid volume step %.1f\n", vol_step);
    vol_step = AUDIO_DEFAULT_VOLUME_STEP_DB;
    this->non_volatile_config.SetValue32(AUDIO_NVM_VOLUME_STEP, *(uint32_t*)&vol_step);
  }

  //ensure the step is an integer dB multiple
  if (vol_step != roundf(vol_step)) {
    vol_step = roundf(vol_step);
    this->non_volatile_config.SetValue32(AUDIO_NVM_VOLUME_STEP, *(uint32_t*)&vol_step);
  }
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
      this->UpdateBluetoothVolume();
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
    DEBUG_PRINTF("Changing DAP gains to %.1f %.1f\n", new_dap_gains.ch1, new_dap_gains.ch2); //TODO temporary testing printout, remove later
    this->system.dap_if.SetVolumeGains(new_dap_gains, std::move(completion_cb));
  } else if (new_dap_gains.ch1 == current_dap_gains.ch1 && new_dap_gains.ch2 == current_dap_gains.ch2) {
    //DAP is already correct: only adjust DAC, then go to completion callback
    DEBUG_PRINTF("Changing DAC gains to %.1f %.1f\n", new_dac_gain_ch1, new_dac_gain_ch2); //TODO temporary testing printout, remove later
    this->system.dac_if.SetVolumesFromGains(new_dac_gain_ch1, new_dac_gain_ch2, std::move(completion_cb));
  } else {
    //we need to adjust both DAC and DAP: calculate max gain differences for each, to apply smallest ("least positive") change first
    float dac_gain_diff = MAX(new_dac_gain_ch1 - current_dac_gain_ch1, new_dac_gain_ch2 - current_dac_gain_ch2);
    float dap_gain_diff = MAX(new_dap_gains.ch1 - current_dap_gains.ch1, new_dap_gains.ch2 - current_dap_gains.ch2);

    if (dac_gain_diff > dap_gain_diff) {
      //DAC difference is larger, so apply DAP first
      DEBUG_PRINTF("Changing DAP gains to %.1f %.1f\n", new_dap_gains.ch1, new_dap_gains.ch2); //TODO temporary testing printout, remove later
      this->system.dap_if.SetVolumeGains(new_dap_gains, [&, completion_cb = std::move(completion_cb), new_dac_gain_ch1, new_dac_gain_ch2, current_dap_gains](bool success) {
        if (!success) {
          //propagate failure to completion callback
          DEBUG_PRINTF("* AudioPathManager ClampAndApplyVolumeGain DAP-DAC write failed at DAP\n");
          completion_cb(false);
          return;
        }

        //apply DAC afterwards
        DEBUG_PRINTF("Changing DAC gains to %.1f %.1f\n", new_dac_gain_ch1, new_dac_gain_ch2); //TODO temporary testing printout, remove later
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
      DEBUG_PRINTF("Changing DAC gains to %.1f %.1f\n", new_dac_gain_ch1, new_dac_gain_ch2); //TODO temporary testing printout, remove later
      this->system.dac_if.SetVolumesFromGains(new_dac_gain_ch1, new_dac_gain_ch2, [&, completion_cb = std::move(completion_cb), new_dap_gains, current_dac_gain_ch1, current_dac_gain_ch2](bool success) {
        if (!success) {
          //propagate failure to completion callback
          DEBUG_PRINTF("* AudioPathManager ClampAndApplyVolumeGain DAC-DAP write failed at DAC\n");
          completion_cb(false);
          return;
        }

        //apply DAP afterwards
        DEBUG_PRINTF("Changing DAP gains to %.1f %.1f\n", new_dap_gains.ch1, new_dap_gains.ch2); //TODO temporary testing printout, remove later
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

void AudioPathManager::SetCurrentVolumeDB(float volume_dB, SuccessCallback&& callback, bool queue_if_busy) {
  if (isnanf(volume_dB)) {
    throw std::invalid_argument("AudioPathManager SetCurrentVolumeDB given invalid volume");
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
      this->queued_operations.push_back([&, volume_dB, callback = std::move(callback)]() mutable {
        this->SetCurrentVolumeDB(volume_dB, std::move(callback), false);
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
  this->lock_timer = AUDIO_LOCK_TIMEOUT_CYCLES;
  __set_PRIMASK(primask);

  //round volume to nearest step
  this->CheckAndFixVolumeStep();
  float vol_step = this->GetVolumeStepDB();
  float rounded_volume = roundf(volume_dB / vol_step) * vol_step;

  //clamp and apply volume gain to DAC and DAP
  this->ClampAndApplyVolumeGain(rounded_volume, [&, callback = std::move(callback)](bool success) {
    //once done: unlock operations and propagate success to external callback
    this->lock_timer = 0;
    if (callback) {
      callback(success);
    }
  });
}

void AudioPathManager::ChangeCurrentVolumeDB(float volume_change_dB, SuccessCallback&& callback, bool queue_if_busy) {
  if (isnanf(volume_change_dB)) {
    throw std::invalid_argument("AudioPathManager ChangeCurrentVolumeDB given NaN volume change");
  }

  this->SetCurrentVolumeDB(this->current_volume_dB + volume_change_dB, std::move(callback), queue_if_busy);
}

void AudioPathManager::StepCurrentVolumeDB(bool up, SuccessCallback&& callback, bool queue_if_busy) {
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  this->CheckAndFixVolumeStep();

  float vol_change = up ? this->GetVolumeStepDB() : -this->GetVolumeStepDB();
  __set_PRIMASK(primask);

  this->ChangeCurrentVolumeDB(vol_change, std::move(callback), queue_if_busy);
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


void AudioPathManager::SetCurrentRelativeVolume(float relative_volume, SuccessCallback&& callback, bool queue_if_busy) {
  if (isnanf(relative_volume) || relative_volume < 0.0f || relative_volume > 1.0f) {
    throw std::invalid_argument("AudioPathManager SetCurrentRelativeVolume given invalid relative volume, must be in [0, 1]");
  }

  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  this->CheckAndFixVolumeLimits();

  float vol_dB = this->min_volume_dB + relative_volume * (this->max_volume_dB - this->min_volume_dB);
  __set_PRIMASK(primask);

  this->SetCurrentVolumeDB(vol_dB, std::move(callback), queue_if_busy);
}


//volume min/max/step functions
float AudioPathManager::GetMinVolumeDB() const {
  return this->min_volume_dB;
}

float AudioPathManager::GetMaxVolumeDB() const {
  return this->max_volume_dB;
}

float AudioPathManager::GetVolumeStepDB() const {
  uint32_t step_int = this->non_volatile_config.GetValue32(AUDIO_NVM_VOLUME_STEP);
  return *(float*)&step_int;
}

bool AudioPathManager::IsPositiveGainAllowed() const {
  bool sp_en, pos_gain;
  this->system.dap_if.GetConfig(sp_en, pos_gain);
  return pos_gain;
}


void AudioPathManager::SetMinVolumeDB(float min_volume_dB, SuccessCallback&& callback, bool queue_if_busy) {
  if (isnanf(min_volume_dB) || min_volume_dB < AUDIO_LIMIT_MIN_VOLUME_MIN || min_volume_dB > AUDIO_LIMIT_MIN_VOLUME_MAX) {
    throw std::invalid_argument("AudioPathManager SetMinVolumeDB given invalid min volume");
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
      this->queued_operations.push_back([&, min_volume_dB, callback = std::move(callback)]() mutable {
        this->SetMinVolumeDB(min_volume_dB, std::move(callback), false);
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

void AudioPathManager::SetMaxVolumeDB(float max_volume_dB, SuccessCallback&& callback, bool queue_if_busy) {
  if (isnanf(max_volume_dB) || max_volume_dB < AUDIO_LIMIT_MAX_VOLUME_MIN || max_volume_dB > AUDIO_LIMIT_MAX_VOLUME_MAX) {
    throw std::invalid_argument("AudioPathManager SetMaxVolumeDB given invalid max volume");
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
      this->queued_operations.push_back([&, max_volume_dB, callback = std::move(callback)]() mutable {
        this->SetMaxVolumeDB(max_volume_dB, std::move(callback), false);
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

void AudioPathManager::SetVolumeStepDB(float volume_step_dB, SuccessCallback&& callback, bool queue_if_busy) {
  if (isnanf(volume_step_dB) || volume_step_dB < AUDIO_LIMIT_VOLUME_STEP_MIN || volume_step_dB > AUDIO_LIMIT_VOLUME_STEP_MAX) {
    throw std::invalid_argument("AudioPathManager SetVolumeStepDB given invalid volume step");
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
      this->queued_operations.push_back([&, volume_step_dB, callback = std::move(callback)]() mutable {
        this->SetVolumeStepDB(volume_step_dB, std::move(callback), false);
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
  this->lock_timer = AUDIO_LOCK_TIMEOUT_CYCLES;
  __set_PRIMASK(primask);

  this->CheckAndFixVolumeStep();

  //round step to next integer dB multiple
  float rounded_step = roundf(volume_step_dB);

  if (rounded_step == this->GetVolumeStepDB()) {
    //already at desired volume step: nothing to do, unlock operations and report to callback
    this->lock_timer = 0;
    if (callback) {
      callback(true);
    }
  } else {
    //apply new volume step
    this->non_volatile_config.SetValue32(AUDIO_NVM_VOLUME_STEP, *(uint32_t*)&rounded_step);

    //align current volume gain with new step size
    float aligned_volume = roundf(this->current_volume_dB / rounded_step) * rounded_step;

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

void AudioPathManager::SetPositiveGainAllowed(bool pos_gain_allowed, SuccessCallback&& callback, bool queue_if_busy) {
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
      this->queued_operations.push_back([&, pos_gain_allowed, callback = std::move(callback)]() mutable {
        this->SetPositiveGainAllowed(pos_gain_allowed, std::move(callback), false);
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


void AudioPathManager::UpdateBluetoothVolume() {
  //skip update if locked out
  if (this->bluetooth_volume_lock_timer > 0) {
    return;
  }

  //check if Bluetooth is even connected with AVRCP profile
  BluetoothReceiverStatus bt_status = this->system.btrx_if.GetStatus();
  if (!bt_status.connected || !bt_status.avrcp_link) {
    //no AVRCP connection: nothing to do
    return;
  }

  //check whether the current volume (dB) is the same as what the current Bluetooth volume step would round to
  uint8_t current_bt_volume = this->system.btrx_if.GetAbsoluteVolume();
  if (current_bt_volume >= AUDIO_BLUETOOTH_VOL_OFFSET && !this->IsMute()) {
    this->CheckAndFixVolumeLimits();
    this->CheckAndFixVolumeStep();

    //calculate volume in dB corresponding to current Bluetooth volume, round to nearest step, and clamp to limits
    float relative_vol = (float)(current_bt_volume - AUDIO_BLUETOOTH_VOL_OFFSET) / (float)(127 - AUDIO_BLUETOOTH_VOL_OFFSET);
    float vol_dB = this->min_volume_dB + relative_vol * (this->max_volume_dB - this->min_volume_dB);
    float vol_step = this->GetVolumeStepDB();
    float vol_rounded = roundf(vol_dB / vol_step) * vol_step;
    if (vol_rounded < this->min_volume_dB) {
      vol_rounded = this->min_volume_dB;
    } else if (vol_rounded > this->max_volume_dB) {
      vol_rounded = this->max_volume_dB;
    }

    if (vol_rounded == this->current_volume_dB) {
      //current Bluetooth volume already rounds to the current volume step: nothing to do
      return;
    }
  }

  //calculate new Bluetooth absolute volume (0-127)
  uint8_t bt_volume;
  if (this->IsMute()) {
    //mute: volume 0
    bt_volume = 0;
  } else {
    //otherwise: calculate positive value from relative volume
    float relative_vol = this->GetCurrentRelativeVolume();
    float scaled_vol = roundf((float)AUDIO_BLUETOOTH_VOL_OFFSET + relative_vol * (float)(127 - AUDIO_BLUETOOTH_VOL_OFFSET));
    //clamp to valid range
    if (scaled_vol < (float)AUDIO_BLUETOOTH_VOL_OFFSET) {
      bt_volume = AUDIO_BLUETOOTH_VOL_OFFSET;
    } else if (scaled_vol > 127.0f) {
      bt_volume = 127;
    } else {
      bt_volume = (uint8_t)scaled_vol;
    }
  }

  //check whether there's enough of a difference to the current Bluetooth volume to warrant a write
  if (abs((int)bt_volume - (int)current_bt_volume) < AUDIO_BLUETOOTH_VOL_MARGIN) {
    //no: delay update until difference is large enough
    return;
  }

  //lock out further bluetooth volume updates
  this->bluetooth_volume_lock_timer = AUDIO_BLUETOOTH_LOCK_TIMEOUT_CYCLES;

  //write volume to bluetooth (we don't really care about whether this succeeds or not - it's just for synchronisation)
  DEBUG_PRINTF("Setting Bluetooth absolute volume to %u\n", bt_volume); //TODO temporary testing printout, remove later
  this->system.btrx_if.SetAbsoluteVolume(bt_volume, [](bool success) {
    if (!success) {
      DEBUG_PRINTF("* Failed to set Bluetooth absolute volume\n");
    }
  });
}

void AudioPathManager::UpdateVolumeFromBluetooth() {
  //Bluetooth volume update
  uint8_t bt_volume = this->system.btrx_if.GetAbsoluteVolume();

  //lock out Bluetooth volume updates
  this->bluetooth_volume_lock_timer = AUDIO_BLUETOOTH_LOCK_TIMEOUT_CYCLES;

  if (bt_volume >= AUDIO_BLUETOOTH_VOL_OFFSET) {
    //not muted: calculate relative volume from Bluetooth volume, clamp to valid range
    float relative_volume = (float)(bt_volume - AUDIO_BLUETOOTH_VOL_OFFSET) / (float)(127 - AUDIO_BLUETOOTH_VOL_OFFSET);
    if (relative_volume < 0.0f) {
      relative_volume = 0.0f;
    } else if (relative_volume > 1.0f) {
      relative_volume = 1.0f;
    }
    //set relative volume, queue if busy
    this->SetCurrentRelativeVolume(relative_volume, [&](bool success) {
      if (!success) {
        DEBUG_PRINTF("* Volume update from Bluetooth failed\n");
      } else {
        DEBUG_PRINTF("Volume updated from Bluetooth, now %.1f\n", this->current_volume_dB); //TODO temporary testing printout, remove later
      }
    }, true);
  }

  //ensure correct mute state, queue if busy
  this->SetMute(bt_volume < AUDIO_BLUETOOTH_VOL_OFFSET, [&](bool success) {
    if (!success) {
      DEBUG_PRINTF("* Mute update from Bluetooth failed\n");
    } else {
      DEBUG_PRINTF("Mute updated from Bluetooth, now %u\n", this->IsMute()); //TODO temporary testing printout, remove later
    }
  }, true);
}


/******************************************************/
/*                   Mute Handling                    */
/******************************************************/

//mute functions
bool AudioPathManager::IsMute() const {
  bool mute_ch1, mute_ch2;
  this->system.dac_if.GetMutes(mute_ch1, mute_ch2);
  return mute_ch1 && mute_ch2;
}


void AudioPathManager::SetMute(bool mute, SuccessCallback&& callback, bool queue_if_busy) {
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
      this->queued_operations.push_back([&, mute, callback = std::move(callback)]() mutable {
        this->SetMute(mute, std::move(callback), false);
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
        this->UpdateBluetoothVolume();
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


/******************************************************/
/*                 Loudness Handling                  */
/******************************************************/

float AudioPathManager::GetLoudnessGainDB() const {
  DAPGains loudness_gains = this->system.dap_if.GetLoudnessGains();
  //report maximum loudness gain - these should really be the same at all times, but it's better to over-report than under-report
  return MAX(loudness_gains.ch1, loudness_gains.ch2);
}

bool AudioPathManager::IsLoudnessTrackingMaxVolume() const {
  return this->non_volatile_config.GetValue8(AUDIO_NVM_LOUDNESS_TRACK_MAX_VOL) != 0;
}


void AudioPathManager::SetLoudnessGainDB(float loudness_gain_dB, SuccessCallback&& callback, bool queue_if_busy) {
  if (isnanf(loudness_gain_dB) || loudness_gain_dB > AUDIO_LIMIT_LOUDNESS_GAIN_MAX) {
    throw std::invalid_argument("AudioPathManager SetLoudnessGainDB given invalid loudness gain");
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
      this->queued_operations.push_back([&, loudness_gain_dB, callback = std::move(callback)]() mutable {
        this->SetLoudnessGainDB(loudness_gain_dB, std::move(callback), false);
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

void AudioPathManager::SetLoudnessTrackingMaxVolume(bool track_max_volume, SuccessCallback&& callback, bool queue_if_busy) {
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
      this->queued_operations.push_back([&, track_max_volume, callback = std::move(callback)]() mutable {
        this->SetLoudnessTrackingMaxVolume(track_max_volume, std::move(callback), false);
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

  //lock out further operations (even if we don't do any async operations, still want to avoid concurrent modifications)
  this->lock_timer = AUDIO_LOCK_TIMEOUT_CYCLES;
  __set_PRIMASK(primask);

  if (track_max_volume == this->IsLoudnessTrackingMaxVolume()) {
    //already in desired state: nothing to do, unlock operations and report to external callback
    this->lock_timer = 0;
    if (callback) {
      callback(true);
    }
  } else {
    //set new tracking state and refresh current volume (to ensure correct gain split)
    this->non_volatile_config.SetValue8(AUDIO_NVM_LOUDNESS_TRACK_MAX_VOL, track_max_volume ? 1 : 0);
    this->ClampAndApplyVolumeGain(this->current_volume_dB, [&, callback = std::move(callback)](bool success) {
      if (!success) {
        DEBUG_PRINTF("* AudioPathManager SetLoudnessTrackingMaxVolume failed to re-apply current volume\n");
      }

      //once done: unlock operations and report to external callback (tracking state application is successful, regardless of current volume re-application success)
      this->lock_timer = 0;
      if (callback) {
        callback(true);
      }
    });
  }
}


/******************************************************/
/*               Mixer and EQ Handling                */
/******************************************************/

AudioPathMixerMode AudioPathManager::GetMixerMode() const {
  return (AudioPathMixerMode)this->non_volatile_config.GetValue8(AUDIO_NVM_MIXER_CONFIG);
}

AudioPathEQMode AudioPathManager::GetEQMode() const {
  return this->eq_mode;
}

AudioPathCalibrationMode AudioPathManager::GetCalibrationMode() const {
  return this->calibration_mode;
}


void AudioPathManager::UpdateMixerAndEQParams(AudioPathMixerMode mixer, AudioPathEQMode eq, AudioPathCalibrationMode cal, SuccessCallback&& callback) {
  //prepare struct for passing parameters down
  struct {
    AudioPathMixerMode mixer;
    AudioPathEQMode eq;
    AudioPathCalibrationMode cal;
    DAPMixerConfig mixer_cfg;
    const q31_t* biquad_coeffs_ch1;
    const q31_t* biquad_coeffs_ch2;
    DAPBiquadSetup biquad_setup;
  } update_params;

  //copy function parameters
  update_params.mixer = mixer;
  update_params.eq = eq;
  update_params.cal = cal;

  //configure mixer based on selected mode
  memcpy(&update_params.mixer_cfg, (mixer == AUDIO_MIXER_AVG) ? &_audio_dap_mixer_config_avg : &_audio_dap_mixer_config_left, sizeof(DAPMixerConfig));

  //select biquad coefficients and setup params
  update_params.biquad_coeffs_ch1 = (eq == AUDIO_EQ_HIFI) ? (const q31_t*)_audio_dap_biquad_hifi_coeffs_ch1 : (const q31_t*)_audio_dap_biquad_power_coeffs_ch1;
  update_params.biquad_coeffs_ch2 = (eq == AUDIO_EQ_HIFI) ? (const q31_t*)_audio_dap_biquad_hifi_coeffs_ch2 : (const q31_t*)_audio_dap_biquad_power_coeffs_ch2;
  memcpy(&update_params.biquad_setup, (eq == AUDIO_EQ_HIFI) ? &_audio_dap_biquad_hifi_setup : &_audio_dap_biquad_power_setup, sizeof(DAPBiquadSetup));

  //modify parameters based on calibration requirements
  if (cal == AUDIO_CAL_CH2) {
    //ch2 cal: disable mixer output to ch1, disable ch2 biquads
    update_params.mixer_cfg.ch1_into_ch1 = 0;
    update_params.mixer_cfg.ch2_into_ch1 = 0;
    update_params.biquad_setup.ch2_stages = 0;
  } else if (cal == AUDIO_CAL_CH1) {
    //ch1 cal: disable mixer output to ch2, disable ch1 biquads
    update_params.mixer_cfg.ch1_into_ch2 = 0;
    update_params.mixer_cfg.ch2_into_ch2 = 0;
    update_params.biquad_setup.ch1_stages = 0;
  }

  //start by disabling signal processor (and positive gain)
  this->system.dap_if.SetConfig(false, false, [&, callback = std::move(callback), update_params](bool success) {
    if (!success) {
      //propagate failure to external callback
      DEBUG_PRINTF("* AudioPathManager UpdateMixerAndEQParams failed to disable signal processor\n");
      if (callback) {
        callback(false);
      }
      return;
    }

    //callback for further processing after mixer is configured
    auto post_mixer_cb = [&, callback = std::move(callback), update_params](bool success) {
      if (!success) {
        DEBUG_PRINTF("* AudioPathManager UpdateMixerAndEQParams failed to configure mixer\n");
      }

      //callback for further processing after biquad ch1 is configured
      auto post_ch1_cb = [&, callback = std::move(callback), update_params, prev_success = success](bool success) {
        if (!success) {
          DEBUG_PRINTF("* AudioPathManager UpdateMixerAndEQParams failed to configure biquad ch1\n");
        }

        //callback for further processing after biquad ch2 is configured
        auto post_ch2_cb = [&, callback = std::move(callback), update_params, prev_success = prev_success && success](bool success) {
          if (!success) {
            DEBUG_PRINTF("* AudioPathManager UpdateMixerAndEQParams failed to configure biquad ch2\n");
          }

          //callback for further processing after biquad setup is configured
          auto post_setup_cb = [&, callback = std::move(callback), update_params, prev_success = prev_success && success](bool success) {
            if (!success) {
              DEBUG_PRINTF("* AudioPathManager UpdateMixerAndEQParams failed to configure biquad setup\n");
            }

            //if successful to this point: save new mixer/eq/cal params
            if (success && prev_success) {
              this->non_volatile_config.SetValue8(AUDIO_NVM_MIXER_CONFIG, (uint8_t)update_params.mixer);
              this->eq_mode = update_params.eq;
              this->calibration_mode = update_params.cal;
            }

            //re-enable signal processor (in all cases)
            this->system.dap_if.SetConfig(true, false, [&, callback = std::move(callback), prev_success = prev_success && success](bool success) {
              if (!success) {
                DEBUG_PRINTF("* AudioPathManager UpdateMixerAndEQParams failed to re-enable signal processor\n");
              }

              //propagate overall success to callback
              if (callback) {
                callback(success && prev_success);
              }
            });
          };

          //compare biquad setup against existing setup
          DAPBiquadSetup current_setup = this->system.dap_if.GetBiquadSetup();
          if (!success || !prev_success || memcmp(&current_setup, &update_params.biquad_setup, sizeof(DAPBiquadSetup)) == 0) {
            //previously failed, or already configured correctly: skip to next step
            post_setup_cb(true);
          } else {
            //configure new biquad setup
            this->system.dap_if.SetBiquadSetup(update_params.biquad_setup, std::move(post_setup_cb));
          }
        };

        //compare biquad ch2 coefficients against existing coefficients
        const q31_t* current_ch2 = this->system.dap_if.GetBiquadCoefficients(IF_DAP_CH2);
        if (!success || !prev_success || memcmp(current_ch2, update_params.biquad_coeffs_ch2, I2CDEF_DAP_REG_SIZE_SP_BIQUAD) == 0) {
          //previously failed, or already configured correctly: skip to next step
          post_ch2_cb(true);
        } else {
          //configure new ch2 coefficients
          this->system.dap_if.SetBiquadCoefficients(IF_DAP_CH2, update_params.biquad_coeffs_ch2, std::move(post_ch2_cb));
        }
      };

      //compare biquad ch1 coefficients against existing coefficients
      const q31_t* current_ch1 = this->system.dap_if.GetBiquadCoefficients(IF_DAP_CH1);
      if (!success || memcmp(current_ch1, update_params.biquad_coeffs_ch1, I2CDEF_DAP_REG_SIZE_SP_BIQUAD) == 0) {
        //previously failed, or already configured correctly: skip to next step
        post_ch1_cb(true);
      } else {
        //configure new ch1 coefficients
        this->system.dap_if.SetBiquadCoefficients(IF_DAP_CH1, update_params.biquad_coeffs_ch1, std::move(post_ch1_cb));
      }
    };

    //compare mixer against existing mixer
    DAPMixerConfig current_mixer = this->system.dap_if.GetMixerConfig();
    if (memcmp(&current_mixer, &update_params.mixer_cfg, sizeof(DAPMixerConfig)) == 0) {
      //already configured correctly: skip to next step
      post_mixer_cb(true);
    } else {
      //configure new mixer setup
      this->system.dap_if.SetMixerConfig(update_params.mixer_cfg, std::move(post_mixer_cb));
    }
  });
}


void AudioPathManager::SetMixerMode(AudioPathMixerMode mode, SuccessCallback&& callback, bool queue_if_busy) {
  if (mode != AUDIO_MIXER_AVG && mode != AUDIO_MIXER_LEFT) {
    throw std::invalid_argument("AudioPathManager SetMixerMode given invalid mode");
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
      this->queued_operations.push_back([&, mode, callback = std::move(callback)]() mutable {
        this->SetMixerMode(mode, std::move(callback), false);
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
  this->lock_timer = AUDIO_LOCK_TIMEOUT_CYCLES;
  __set_PRIMASK(primask);

  if (mode == this->GetMixerMode()) {
    //already in desired state: nothing to do, unlock operations and report to external callback
    this->lock_timer = 0;
    if (callback) {
      callback(true);
    }
  } else {
    //set new mixer mode and update EQ parameters
    this->UpdateMixerAndEQParams(mode, this->eq_mode, this->calibration_mode, [&, callback = std::move(callback)](bool success) {
      //once done: unlock operations and propagate success to external callback
      this->lock_timer = 0;
      if (callback) {
        callback(success);
      }
    });
  }
}

void AudioPathManager::SetEQMode(AudioPathEQMode mode, SuccessCallback&& callback, bool queue_if_busy) {
  if (mode != AUDIO_EQ_HIFI && mode != AUDIO_EQ_POWER) {
    throw std::invalid_argument("AudioPathManager SetEQMode given invalid mode");
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
      this->queued_operations.push_back([&, mode, callback = std::move(callback)]() mutable {
        this->SetEQMode(mode, std::move(callback), false);
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
  this->lock_timer = AUDIO_LOCK_TIMEOUT_CYCLES;
  __set_PRIMASK(primask);

  if (mode == this->eq_mode) {
    //already in desired state: nothing to do, unlock operations and report to external callback
    this->lock_timer = 0;
    if (callback) {
      callback(true);
    }
  } else {
    //set new EQ mode and update EQ parameters
    this->UpdateMixerAndEQParams(this->GetMixerMode(), mode, this->calibration_mode, [&, callback = std::move(callback)](bool success) {
      //once done: unlock operations and propagate success to external callback
      this->lock_timer = 0;
      if (callback) {
        callback(success);
      }
    });
  }
}

void AudioPathManager::SetCalibrationMode(AudioPathCalibrationMode mode, SuccessCallback&& callback, bool queue_if_busy) {
  if (mode != AUDIO_CAL_NONE && mode != AUDIO_CAL_CH1 && mode != AUDIO_CAL_CH2) {
    throw std::invalid_argument("AudioPathManager SetCalibrationMode given invalid mode");
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
      this->queued_operations.push_back([&, mode, callback = std::move(callback)]() mutable {
        this->SetCalibrationMode(mode, std::move(callback), false);
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
  this->lock_timer = AUDIO_LOCK_TIMEOUT_CYCLES;
  __set_PRIMASK(primask);

  if (mode == this->calibration_mode) {
    //already in desired state: nothing to do, unlock operations and report to external callback
    this->lock_timer = 0;
    if (callback) {
      callback(true);
    }
  } else {
    //set new calibration mode and update EQ parameters
    this->UpdateMixerAndEQParams(this->GetMixerMode(), this->eq_mode, mode, [&, callback = std::move(callback)](bool success) {
      //once done: unlock operations and propagate success to external callback
      this->lock_timer = 0;
      if (callback) {
        callback(success);
      }
    });
  }
}


