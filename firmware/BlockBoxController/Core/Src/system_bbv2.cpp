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
  this->eeprom_if.InitModule([&, callback = std::move(callback)](bool success) {
    DEBUG_PRINTF("EEPROM init complete, success %u\n", success);
    //propagate result to external callback
    if (callback) {
      callback(success);
    }
  });
}

void BlockBoxV2System::InitDAP(SuccessCallback&& callback) {
  this->dap_if.ResetModule([&, callback = std::move(callback)](bool success) {
    DEBUG_PRINTF("DAP reset/init complete, success %u\n", success);
    if (!success) {
      //propagate failure to external callback
      if (callback) {
        callback(false);
      }
      return;
    }

    //this->dap_if.monitor_src_stats = true;
    this->dap_if.SetI2SInputSampleRate(IF_DAP_INPUT_I2S1, IF_DAP_SR_96K, [&, callback = std::move(callback)](bool success) {
      DEBUG_PRINTF("DAP I2S1 sample rate set to 96K, success %u\n", success);
      if (!success) {
        //propagate failure to external callback
        if (callback) {
          callback(false);
        }
        return;
      }

      this->dap_if.SetVolumeGains({ -10.0f, -10.0f }, [&, callback = std::move(callback)](bool success) { //TODO remove this after testing
        DEBUG_PRINTF("DAP vol gains set, success %u\n", success);
        //propagate result to external callback
        if (callback) {
          callback(success);
        }
      });
    });
  });
}

void BlockBoxV2System::InitHiFiDAC(SuccessCallback&& callback) {
  this->dac_if.ResetModule([&, callback = std::move(callback)](bool success) {
    DEBUG_PRINTF("HiFiDAC reset/init complete, success %u\n", success);
    if (!success) {
      //propagate failure to external callback
      if (callback) {
        callback(false);
      }
      return;
    }

    HiFiDACConfig cfg;
    cfg.dac_enable = true;
    cfg.sync_mode = true;
    cfg.master_mode = false;
    this->dac_if.SetConfig(cfg, [&, callback = std::move(callback)](bool success) {
      DEBUG_PRINTF("HiFiDAC config set to enabled/sync/slave, success %u\n", success);
      //propagate result to external callback
      if (callback) {
        callback(success);
      }
    });
  });
}

void BlockBoxV2System::InitPowerAmp(SuccessCallback&& callback) {
  //Note: Don't reset the amp module (unless we really have to), since it causes a transient on the PVDD tracking signal, potentially spiking the input voltage too
  this->amp_if.InitModule([&, callback = std::move(callback)](bool success) {
    DEBUG_PRINTF("PowerAmp reset/init complete, success %u\n", success);
    if (!success) {
      //propagate failure to external callback
      if (callback) {
        callback(false);
      }
      return;
    }

    this->amp_if.monitor_measurements = true; //TODO disable monitoring at this point later, only here for testing
    this->amp_if.SetManualShutdownActive(true, [&, callback = std::move(callback)](bool success) {
      DEBUG_PRINTF("PowerAmp set manual shutdown, success %u\n", success);
      if (!success) {
        //propagate failure to external callback
        if (callback) {
          callback(false);
        }
        return;
      }

      this->amp_if.SetPVDDTargetVoltage(25.0f, [&, callback = std::move(callback)](bool success) {
        DEBUG_PRINTF("PowerAmp started voltage reduction, success %u\n", success);
        //propagate result to external callback
        if (callback) {
          callback(success);
        }
      });
    });
  });
}

void BlockBoxV2System::InitBluetoothReceiver(SuccessCallback&& callback) {
  this->btrx_if.ResetModule([&, callback = std::move(callback)](bool success) {
    DEBUG_PRINTF("BTRX reset/init complete, success %u\n", success);
    if (!success) {
      //propagate failure to external callback
      if (callback) {
        callback(false);
      }
      return;
    }

    this->btrx_if.SetDiscoverable(true, [&, callback = std::move(callback)](bool success) { //TODO remove this after testing
      DEBUG_PRINTF("BTRX set discoverable, success %u\n", success);
      //propagate result to external callback
      if (callback) {
        callback(success);
      }
    });
  });
}

void BlockBoxV2System::Init() {
  this->main_i2c_hw.Init();
  this->eeprom_if.Init();
  this->dap_if.Init();
  this->dac_if.Init();
  this->amp_if.Init();
  this->btrx_if.Init();


  //debug printout callbacks
  /*
  this->dap_if.RegisterCallback([&](EventSource&, uint32_t event) {
    switch (event) {
      case MODIF_DAP_EVENT_STATUS_UPDATE:
        DEBUG_PRINTF("DAP status update: 0x%02X\n", this->dap_if.GetStatus().value);
        break;
      case MODIF_DAP_EVENT_INPUTS_UPDATE:
        DEBUG_PRINTF("DAP inputs update: active 0x%02X, available 0x%02X\n", this->dap_if.GetActiveInput(), this->dap_if.GetAvailableInputs().value);
        break;
      case MODIF_DAP_EVENT_INPUT_RATE_UPDATE:
        DEBUG_PRINTF("DAP input rate update: %u\n", this->dap_if.GetSRCInputSampleRate());
        break;
      case MODIF_DAP_EVENT_SRC_STATS_UPDATE:
        DEBUG_PRINTF("DAP SRC stats update: rate error %.4f, fill error %.1f\n", this->dap_if.GetSRCInputRateErrorRelative(), this->dap_if.GetSRCBufferFillErrorSamples());
        break;
      default:
        break;
    }
  }, MODIF_DAP_EVENT_STATUS_UPDATE | MODIF_DAP_EVENT_INPUTS_UPDATE | MODIF_DAP_EVENT_INPUT_RATE_UPDATE | MODIF_DAP_EVENT_SRC_STATS_UPDATE);

  this->dac_if.RegisterCallback([&](EventSource&, uint32_t event) {
    DEBUG_PRINTF("HiFiDAC status update: 0x%02X\n", this->dac_if.GetStatus().value);
  }, MODIF_HIFIDAC_EVENT_STATUS_UPDATE);

  this->amp_if.RegisterCallback([&](EventSource&, uint32_t event) {
    switch (event) {
      case MODIF_POWERAMP_EVENT_STATUS_UPDATE:
        DEBUG_PRINTF("PowerAmp status update: 0x%04X\n", this->amp_if.GetStatus().value);
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

  this->btrx_if.RegisterCallback([&](EventSource&, uint32_t event) {
    switch (event) {
      case MODIF_BTRX_EVENT_STATUS_UPDATE:
        DEBUG_PRINTF("BTRX status update: 0x%04X\n", this->btrx_if.GetStatus().value);
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


  //module init process
  HAL_Delay(500);

  this->InitEEPROM([&](bool success) {
    if (!success) {
      DEBUG_PRINTF("* EEPROM init failed, defaults have been loaded, continuing\n");
    }

    this->gui_mgr.Init();

    this->gui_mgr.SetInitProgress("Initialising Power Amplifier...", false);
    this->InitPowerAmp([&](bool success) {
      if (!success) {
        this->gui_mgr.SetInitProgress("Failed to initialise Power Amplifier!", true);
        return;
      }

      this->gui_mgr.SetInitProgress("Initialising HiFi DAC...", false);
      this->InitHiFiDAC([&](bool success) {
        if (!success) {
          this->gui_mgr.SetInitProgress("Failed to initialise HiFi DAC!", true);
          return;
        }

        this->gui_mgr.SetInitProgress("Initialising Digital Audio Processor...", false);
        this->InitDAP([&](bool success) {
          if (!success) {
            this->gui_mgr.SetInitProgress("Failed to init Digital Audio Processor!", true);
            return;
          }

          this->gui_mgr.SetInitProgress("Initialising Bluetooth Receiver...", false);
          this->InitBluetoothReceiver([&](bool success) {
            if (!success) {
              this->gui_mgr.SetInitProgress("Failed to initialise Bluetooth Receiver!", true);
              return;
            }

            //init done
            this->gui_mgr.SetInitProgress(NULL, false);
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
  this->btrx_if.LoopTasks();
  this->gui_mgr.Update();
}


BlockBoxV2System::BlockBoxV2System() :
    main_i2c_hw(&BBV2_I2C_MAIN_HANDLE, _BlockBoxV2_I2C_Main_HardwareReset),
    eeprom_if(this->main_i2c_hw, BBV2_EEPROM_I2C_ADDR),
    dap_if(this->main_i2c_hw, BBV2_DAP_I2C_ADDR, BBV2_DAP_INT_PORT, BBV2_DAP_INT_PIN),
    dac_if(this->main_i2c_hw, BBV2_HIFIDAC_I2C_ADDR, BBV2_HIFIDAC_INT_PORT, BBV2_HIFIDAC_INT_PIN),
    amp_if(this->main_i2c_hw, BBV2_POWERAMP_I2C_ADDR, BBV2_POWERAMP_INT_PORT, BBV2_POWERAMP_INT_PIN),
    btrx_if(&BBV2_BTRX_UART_HANDLE),
    gui_mgr(*this) {}


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


