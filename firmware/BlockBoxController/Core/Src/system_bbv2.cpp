/*
 * system_bbv2.cpp
 *
 *  Created on: May 10, 2025
 *      Author: Alex
 */

#include "system.h"
#include "retarget.h"


/***************************************************/
/*        BlockBox v2 system configuration         */
/***************************************************/

#define BBV2_I2C_MAIN_HANDLE hi2c5
#define BBV2_I2C_MAIN_FORCE_RESET() __HAL_RCC_I2C5_FORCE_RESET()
#define BBV2_I2C_MAIN_RELEASE_RESET() __HAL_RCC_I2C5_RELEASE_RESET()

#define BBV2_EEPROM_I2C_ADDR 0x57

#define BBV2_DAP_INT_PORT I2C5_INT3_N_GPIO_Port
#define BBV2_DAP_INT_PIN I2C5_INT3_N_Pin
#define BBV2_DAP_I2C_ADDR 0x4A

#define BBV2_HIFIDAC_INT_PORT I2C5_INT2_N_GPIO_Port
#define BBV2_HIFIDAC_INT_PIN I2C5_INT2_N_Pin
#define BBV2_HIFIDAC_I2C_ADDR 0x1D

#define BBV2_POWERAMP_INT_PORT I2C5_INT1_N_GPIO_Port
#define BBV2_POWERAMP_INT_PIN I2C5_INT1_N_Pin
#define BBV2_POWERAMP_I2C_ADDR 0x11

#define BBV2_RTC_I2C_ADDR 0x68

#define BBV2_BTRX_UART_HANDLE huart1


static void _BlockBoxV2_I2C_Main_HardwareReset() {
  BBV2_I2C_MAIN_FORCE_RESET();
  int i;
  for (i = 0; i < 10; i++) __NOP();
  BBV2_I2C_MAIN_RELEASE_RESET();
}


/***************************************************/
/*          System class implementation            */
/***************************************************/

void BlockBoxV2System::InitEEPROM(SuccessCallback&& callback) {
  this->eeprom_if.InitModule([this, callback = std::move(callback)](bool success) {
    DEBUG_PRINTF("EEPROM init complete, success %u\n", success);
    //propagate result to external callback
    if (callback) {
      callback(success);
    }
  });
}

void BlockBoxV2System::InitDAP(SuccessCallback&& callback) {
  this->dap_if.ResetModule([this, callback = std::move(callback)](bool success) {
    DEBUG_PRINTF("DAP reset/init complete, success %u\n", success);
    if (!success) {
      //propagate failure to external callback
      if (callback) {
        callback(false);
      }
      return;
    }

    if (callback) {
      callback(true);
    }
  });
}

void BlockBoxV2System::InitHiFiDAC(SuccessCallback&& callback) {
  this->dac_if.ResetModule([this, callback = std::move(callback)](bool success) {
    DEBUG_PRINTF("HiFiDAC reset/init complete, success %u\n", success);
    if (!success) {
      //propagate failure to external callback
      if (callback) {
        callback(false);
      }
      return;
    }

    if (callback) {
      callback(true);
    }
  });
}

void BlockBoxV2System::InitPowerAmp(SuccessCallback&& callback) {
  /* TODO amp controller dead, disabled this
  //Note: Don't reset the amp module (unless we really have to), since it causes a transient on the PVDD tracking signal, potentially spiking the input voltage too
  this->amp_if.InitModule([this, callback = std::move(callback)](bool success) {
    DEBUG_PRINTF("PowerAmp reset/init complete, success %u\n", success);
    if (!success) {
      //propagate failure to external callback
      if (callback) {
        callback(false);
      }
      return;
    }

    this->amp_if.SetManualShutdownActive(true, [this, callback = std::move(callback)](bool success) {
      DEBUG_PRINTF("PowerAmp set manual shutdown, success %u\n", success);
      if (!success) {
        //propagate failure to external callback
        if (callback) {
          callback(false);
        }
        return;
      }

      this->amp_if.SetPVDDTargetVoltage(IF_POWERAMP_PVDD_TARGET_MIN, [this, callback = std::move(callback)](bool success) {
        DEBUG_PRINTF("PowerAmp started voltage reduction, success %u\n", success);
        //propagate result to external callback
        if (callback) {
          callback(success);
        }
      });
    });
  });
  */
  if (callback) {
    callback(true);
  }
}

void BlockBoxV2System::InitBluetoothReceiver(SuccessCallback&& callback) {
  this->btrx_if.ResetModule([this, callback = std::move(callback)](bool success) {
    DEBUG_PRINTF("BTRX reset/init complete, success %u\n", success);
    if (!success) {
      //propagate failure to external callback
      if (callback) {
        callback(false);
      }
      return;
    }

    if (callback) {
      callback(true);
    }
  });
}

void BlockBoxV2System::Init() {
  this->powered_on = false;

  this->main_i2c_hw.Init();
  this->eeprom_if.Init();
  this->dap_if.Init();
  this->dac_if.Init();
  this->amp_if.Init();
  this->rtc_if.Init();
  this->btrx_if.Init();


  //debug printout callbacks
  /*this->dap_if.RegisterCallback([this](EventSource*, uint32_t event) {
    switch (event) {
      case MODIF_DAP_EVENT_STATUS_UPDATE:
        DEBUG_PRINTF("DAP status update: 0x%02X\n", this->dap_if.GetStatus().value);
        break;
      case MODIF_DAP_EVENT_INPUTS_UPDATE:
        DEBUG_PRINTF("DAP inputs update: active 0x%02X, available 0x%02X\n", this->dap_if.GetActiveInput(), this->dap_if.GetAvailableInputs().value);
        break;
      case MODIF_DAP_EVENT_INPUT_RATE_UPDATE:
        //DEBUG_PRINTF("DAP input rate update: %u\n", this->dap_if.GetSRCInputSampleRate());
        break;
      case MODIF_DAP_EVENT_SRC_STATS_UPDATE:
        //DEBUG_PRINTF("DAP SRC stats update: rate error %.4f, fill error %.1f\n", this->dap_if.GetSRCInputRateErrorRelative(), this->dap_if.GetSRCBufferFillErrorSamples());
        break;
      default:
        break;
    }
  }, MODIF_DAP_EVENT_STATUS_UPDATE | MODIF_DAP_EVENT_INPUTS_UPDATE | MODIF_DAP_EVENT_INPUT_RATE_UPDATE | MODIF_DAP_EVENT_SRC_STATS_UPDATE);

  this->dac_if.RegisterCallback([this](EventSource*, uint32_t event) {
    //DEBUG_PRINTF("HiFiDAC status update: 0x%02X\n", this->dac_if.GetStatus().value);
  }, MODIF_HIFIDAC_EVENT_STATUS_UPDATE);

  this->amp_if.RegisterCallback([this](EventSource*, uint32_t event) {
    switch (event) {
      case MODIF_POWERAMP_EVENT_STATUS_UPDATE:
        //DEBUG_PRINTF("PowerAmp status update: 0x%04X\n", this->amp_if.GetStatus().value);
        break;
      case MODIF_POWERAMP_EVENT_SAFETY_UPDATE:
        //DEBUG_PRINTF("PowerAmp safety update: shutdown man %u safety %u, err 0x%04X, warn 0x%04X\n", this->amp_if.IsManualShutdownActive(), this->amp_if.IsSafetyShutdownActive(), this->amp_if.GetSafetyErrorSource().value, this->amp_if.GetSafetyWarningSource().value);
        break;
      case MODIF_POWERAMP_EVENT_PVDD_UPDATE:
        //DEBUG_PRINTF("PowerAmp PVDD update: target %.2f, request %.2f, measured %.2f\n", this->amp_if.GetPVDDTargetVoltage(), this->amp_if.GetPVDDRequestedVoltage(), this->amp_if.GetPVDDMeasuredVoltage());
        break;
      case MODIF_POWERAMP_EVENT_MEASUREMENT_UPDATE:
      {
        PowerAmpMeasurements m = this->amp_if.GetOutputMeasurements(false);
        DEBUG_PRINTF("PowerAmp measurements update: Vrms %.2f %.2f %.2f %.2f; ChA Irms %.2f, Pavg %.2f, Papp %.2f\n", m.voltage_rms.ch_a, m.voltage_rms.ch_b, m.voltage_rms.ch_c, m.voltage_rms.ch_d, m.current_rms.ch_a, m.power_average.ch_a, m.power_apparent.ch_a);
        break;
      }
      default:
        break;
    }
  }, MODIF_POWERAMP_EVENT_STATUS_UPDATE | MODIF_POWERAMP_EVENT_SAFETY_UPDATE | MODIF_POWERAMP_EVENT_PVDD_UPDATE | MODIF_POWERAMP_EVENT_MEASUREMENT_UPDATE);

  this->btrx_if.RegisterCallback([this](EventSource*, uint32_t event) {
    switch (event) {
      case MODIF_BTRX_EVENT_STATUS_UPDATE:
        //DEBUG_PRINTF("BTRX status update: 0x%04X\n", this->btrx_if.GetStatus().value);
        break;
      case MODIF_BTRX_EVENT_VOLUME_UPDATE:
        DEBUG_PRINTF("BTRX volume update: %u\n", this->btrx_if.GetAbsoluteVolume());
        break;
      case MODIF_BTRX_EVENT_MEDIA_META_UPDATE:
        DEBUG_PRINTF("BTRX media meta update: %s - %s (%s)\n", this->btrx_if.GetMediaArtist(), this->btrx_if.GetMediaTitle(), this->btrx_if.GetMediaAlbum());
        break;
      case MODIF_BTRX_EVENT_DEVICE_UPDATE:
        DEBUG_PRINTF("BTRX device update: %s (0x%012llX)\n", this->btrx_if.GetDeviceName(), this->btrx_if.GetDeviceAddress());
        break;
      case MODIF_BTRX_EVENT_CONN_STATS_UPDATE:
        DEBUG_PRINTF("BTRX conn stats update: RSSI %d, quality %u\n", this->btrx_if.GetConnectionRSSI(), this->btrx_if.GetConnectionQuality());
        break;
      case MODIF_BTRX_EVENT_CODEC_UPDATE:
        DEBUG_PRINTF("BTRX codec update: %s\n", this->btrx_if.GetActiveCodec());
        break;
      default:
        break;
    }
  }, MODIF_BTRX_EVENT_STATUS_UPDATE | MODIF_BTRX_EVENT_VOLUME_UPDATE | MODIF_BTRX_EVENT_MEDIA_META_UPDATE | MODIF_BTRX_EVENT_DEVICE_UPDATE | MODIF_BTRX_EVENT_CONN_STATS_UPDATE | MODIF_BTRX_EVENT_CODEC_UPDATE);
  //*/

  this->audio_mgr.RegisterCallback([this](EventSource*, uint32_t event) {
    static AudioPathInput last_input = AUDIO_INPUT_SPDIF;
    switch (event) {
      case AUDIO_EVENT_INPUT_UPDATE:
      {
        AudioPathInput input = this->audio_mgr.GetActiveInput();
        if (input != last_input) {
          last_input = input;
          DEBUG_PRINTF("Audio input update: active %u\n", input);
        }
        break;
      }
      case AUDIO_EVENT_VOLUME_UPDATE:
        DEBUG_PRINTF("Audio volume update: %.1f\n", this->audio_mgr.GetCurrentVolumeDB());
        break;
      case AUDIO_EVENT_MUTE_UPDATE:
        DEBUG_PRINTF("Audio mute update: %u\n", this->audio_mgr.IsMute());
        break;
    }
  }, AUDIO_EVENT_INPUT_UPDATE | AUDIO_EVENT_VOLUME_UPDATE | AUDIO_EVENT_MUTE_UPDATE);


  //module init process
  HAL_Delay(500);

  this->InitEEPROM([this](bool success) {
    if (!success) {
      DEBUG_PRINTF("* EEPROM init failed, defaults have been loaded, continuing\n");
    }

    this->gui_mgr.Init();

    this->gui_mgr.SetInitProgress("Initialising Power Amplifier...", false);
    this->InitPowerAmp([this](bool success) {
      if (!success) {
        this->gui_mgr.SetInitProgress("Failed to initialise Power Amplifier!", true);
        return;
      }

      this->gui_mgr.SetInitProgress("Initialising HiFi DAC...", false);
      this->InitHiFiDAC([this](bool success) {
        if (!success) {
          this->gui_mgr.SetInitProgress("Failed to initialise HiFi DAC!", true);
          return;
        }

        this->gui_mgr.SetInitProgress("Initialising Digital Audio Processor...", false);
        this->InitDAP([this](bool success) {
          if (!success) {
            this->gui_mgr.SetInitProgress("Failed to init Digital Audio Processor!", true);
            return;
          }

          this->gui_mgr.SetInitProgress("Initialising Real-Time Clock...", false);
          this->rtc_if.InitModule([this](bool success) {
            if (!success) {
              //ignore failure, RTC is non-critical
              DEBUG_PRINTF("* System failed to initialise RTC!");
            }

            this->gui_mgr.SetInitProgress("Initialising Bluetooth Receiver...", false);
            this->InitBluetoothReceiver([this](bool success) {
              if (!success) {
                this->gui_mgr.SetInitProgress("Failed to initialise Bluetooth Receiver!", true);
                return;
              }

              this->gui_mgr.SetInitProgress("Initialising Audio Manager...", false);
              this->audio_mgr.Init([this](bool success) {
                if (!success) {
                  this->gui_mgr.SetInitProgress("Failed to initialise Audio Manager!", true);
                  return;
                }

                //init done - TODO only because amp controller is dead
                this->gui_mgr.SetInitProgress(NULL, false);
                /*this->gui_mgr.SetInitProgress("Initialising Amplifier Manager...", false);
                this->amp_mgr.Init([this](bool success) {
                  if (!success) {
                    this->gui_mgr.SetInitProgress("Failed to initialise Amplifier Manager!", true);
                    return;
                  }

                  //init done
                  this->gui_mgr.SetInitProgress(NULL, false);
                });*/
              });
            });
          });
        });
      });
    });
  });
}


void BlockBoxV2System::LoopTasks() {
  this->main_i2c_hw.LoopTasks();
  this->eeprom_if.LoopTasks();
  this->dap_if.LoopTasks();
  this->dac_if.LoopTasks();
  this->amp_if.LoopTasks();
  this->rtc_if.LoopTasks();
  this->btrx_if.LoopTasks();
  this->audio_mgr.LoopTasks();
  this->amp_mgr.LoopTasks();
  this->gui_mgr.Update();
}


bool BlockBoxV2System::IsPoweredOn() const {
  return this->powered_on;
}

void BlockBoxV2System::SetPowerState(bool on, SuccessCallback&& callback) {
  //apply state to audio manager first (mute, volume reset etc)
  this->audio_mgr.HandlePowerStateChange(on, [this, on, callback = std::move(callback)](bool success) {
    if (!success) {
      DEBUG_PRINTF("* BlockBoxV2System SetPowerState failed to set AudioManager power state\n");
      //attempt reset
      this->audio_mgr.HandlePowerStateChange(!on, [callback = std::move(callback)](bool success) {
        if (!success) {
          DEBUG_PRINTF("* BlockBoxV2System SetPowerState failed to set AudioManager power state, then failed to reset it!\n");
        }

        //propagate failure to external callback
        if (callback) {
          callback(false);
        }
      });
      return;
    }

    if (success) {
      //save new power state
      this->powered_on = on;
    }

    //propagate success to external callback
    if (callback) {
      callback(success);
    }

    //apply state to amplifier manager next (amp shutdown state, PVDD update etc) - TODO disabled because amp controller dead
    /*this->amp_mgr.HandlePowerStateChange(on, [this, on, callback = std::move(callback)](bool success) {
      if (!success) {
        DEBUG_PRINTF("* BlockBoxV2System SetPowerState failed to set AmpManager power state\n");
        //attempt reset
        this->audio_mgr.HandlePowerStateChange(!on, [this, on, callback = std::move(callback)](bool success) {
          if (!success) {
            DEBUG_PRINTF("* BlockBoxV2System SetPowerState failed to set AmpManager power state, then failed to reset AudioManager!\n");
          }

          this->amp_mgr.HandlePowerStateChange(!on, [callback = std::move(callback)](bool success) {
            if (!success) {
              DEBUG_PRINTF("* BlockBoxV2System SetPowerState failed to set AmpManager power state, then failed to reset it!\n");
            }

            //propagate failure to external callback
            if (callback) {
              callback(false);
            }
          });
        });
        return;
      }

      if (success) {
        //save new power state
        this->powered_on = on;
      }

      //propagate success to external callback
      if (callback) {
        callback(success);
      }
    });*/
  });
}


BlockBoxV2System::BlockBoxV2System() :
    main_i2c_hw(&BBV2_I2C_MAIN_HANDLE, _BlockBoxV2_I2C_Main_HardwareReset),
    eeprom_if(this->main_i2c_hw, BBV2_EEPROM_I2C_ADDR),
    dap_if(this->main_i2c_hw, BBV2_DAP_I2C_ADDR, BBV2_DAP_INT_PORT, BBV2_DAP_INT_PIN),
    dac_if(this->main_i2c_hw, BBV2_HIFIDAC_I2C_ADDR, BBV2_HIFIDAC_INT_PORT, BBV2_HIFIDAC_INT_PIN),
    amp_if(this->main_i2c_hw, BBV2_POWERAMP_I2C_ADDR, BBV2_POWERAMP_INT_PORT, BBV2_POWERAMP_INT_PIN),
    rtc_if(this->main_i2c_hw, BBV2_RTC_I2C_ADDR),
    btrx_if(&BBV2_BTRX_UART_HANDLE),
    gui_mgr(*this),
    audio_mgr(*this),
    amp_mgr(*this),
    powered_on(false) {}


/***************************************************/
/*              Interrupt forwarding               */
/***************************************************/

void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *hi2c) {
  if (hi2c == &BBV2_I2C_MAIN_HANDLE) {
    bbv2_system.main_i2c_hw.HandleInterrupt(IF_TX_COMPLETE);
  }
}

void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c) {
  if (hi2c == &BBV2_I2C_MAIN_HANDLE) {
    bbv2_system.main_i2c_hw.HandleInterrupt(IF_RX_COMPLETE);
  }
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c) {
  if (hi2c == &BBV2_I2C_MAIN_HANDLE) {
    bbv2_system.main_i2c_hw.HandleInterrupt(IF_ERROR);
  }
}


void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
  switch (GPIO_Pin) {
    case BBV2_DAP_INT_PIN:
      bbv2_system.dap_if.HandleInterrupt(IF_EXTI, GPIO_Pin);
      break;
    case BBV2_HIFIDAC_INT_PIN:
      bbv2_system.dac_if.HandleInterrupt(IF_EXTI, GPIO_Pin);
      break;
    case BBV2_POWERAMP_INT_PIN:
      bbv2_system.amp_if.HandleInterrupt(IF_EXTI, GPIO_Pin);
      break;
    default:
      break;
  }
}


void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef* huart, uint16_t Size) {
  if (huart == &BBV2_BTRX_UART_HANDLE) {
    bbv2_system.btrx_if.HandleInterrupt(IF_RX_COMPLETE, Size);
  }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef* huart) {
  if (huart == &BBV2_BTRX_UART_HANDLE) {
    bbv2_system.btrx_if.HandleInterrupt(IF_TX_COMPLETE, 0);
  } else {
    Retarget_UART_TxCpltCallback(huart);
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef* huart) {
  if (huart == &BBV2_BTRX_UART_HANDLE) {
    bbv2_system.btrx_if.HandleInterrupt(IF_ERROR, 0);
  }
}


