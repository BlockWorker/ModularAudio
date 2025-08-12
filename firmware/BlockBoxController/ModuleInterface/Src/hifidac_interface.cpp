/*
 * hifidac_interface.cpp
 *
 *  Created on: Jul 13, 2025
 *      Author: Alex
 */


#include "hifidac_interface.h"


static_assert(MODIF_I2C_INT_RESET_FLAG == I2CDEF_HIFIDAC_INT_FLAGS_INT_RESET_Msk);


static uint8_t hifidac_scratch[16];


HiFiDACStatus HiFiDACInterface::GetStatus() const {
  HiFiDACStatus status;
  status.value = this->registers.Reg8(I2CDEF_HIFIDAC_STATUS);
  return status;
}

HiFiDACConfig HiFiDACInterface::GetConfig() const {
  HiFiDACConfig config;
  config.value = this->registers.Reg8(I2CDEF_HIFIDAC_CONTROL);
  return config;
}


void HiFiDACInterface::GetVolumes(uint8_t& volume_ch1, uint8_t& volume_ch2) const {
  uint16_t volumes = this->registers.Reg16(I2CDEF_HIFIDAC_VOLUME);
  volume_ch1 = (uint8_t)volumes;
  volume_ch2 = (uint8_t)(volumes >> 8);
}

void HiFiDACInterface::GetVolumesAsGains(float& gain_ch1, float& gain_ch2) const {
  uint8_t vol_ch1, vol_ch2;
  this->GetVolumes(vol_ch1, vol_ch2);
  gain_ch1 = (float)vol_ch1 * IF_HIFIDAC_VOLUME_GAIN_PER_STEP;
  gain_ch2 = (float)vol_ch2 * IF_HIFIDAC_VOLUME_GAIN_PER_STEP;
}

void HiFiDACInterface::GetMutes(bool& mute_ch1, bool& mute_ch2) const {
  uint8_t mutes = this->registers.Reg8(I2CDEF_HIFIDAC_MUTE);
  mute_ch1 = (mutes & I2CDEF_HIFIDAC_MUTE_MUTE_CH1_Msk) != 0;
  mute_ch2 = (mutes & I2CDEF_HIFIDAC_MUTE_MUTE_CH2_Msk) != 0;
}


HiFiDACSignalPathSetup HiFiDACInterface::GetSignalPathSetup() const {
  HiFiDACSignalPathSetup setup;
  setup.value = this->registers.Reg8(I2CDEF_HIFIDAC_PATH);
  return setup;
}


HiFiDACInternalClockConfig HiFiDACInterface::GetInternalClockConfig() const {
  HiFiDACInternalClockConfig config;
  config.value = this->registers.Reg8(I2CDEF_HIFIDAC_CLK_CFG);
  return config;
}

uint16_t HiFiDACInterface::GetI2SMasterClockDivider() const {
  //register contains divider minus one - correct for that offset
  return (uint16_t)this->registers.Reg8(I2CDEF_HIFIDAC_MASTER_DIV) + 1;
}


uint8_t HiFiDACInterface::GetTDMSlotCount() const {
  //register contains slot count minus one - correct for that offset
  return this->registers.Reg8(I2CDEF_HIFIDAC_TDM_SLOT_NUM) + 1;
}

void HiFiDACInterface::GetChannelTDMSlots(uint8_t& slot_ch1, uint8_t& slot_ch2) const {
  uint16_t slots = this->registers.Reg16(I2CDEF_HIFIDAC_CH_SLOTS);
  slot_ch1 = (uint8_t)slots;
  slot_ch2 = (uint8_t)(slots >> 8);
}


HiFiDACFilterShape HiFiDACInterface::GetInterpolationFilterShape() const {
  return (HiFiDACFilterShape)this->registers.Reg8(I2CDEF_HIFIDAC_FILTER_SHAPE);
}


void HiFiDACInterface::GetSecondHarmonicCorrectionCoefficients(int16_t& thd_c2_ch1, int16_t& thd_c2_ch2) const {
  uint32_t coeffs = this->registers.Reg32(I2CDEF_HIFIDAC_THD_C2);
  thd_c2_ch1 = (int16_t)coeffs;
  thd_c2_ch2 = (int16_t)(coeffs >> 16);
}

void HiFiDACInterface::GetThirdHarmonicCorrectionCoefficients(int16_t& thd_c3_ch1, int16_t& thd_c3_ch2) const {
  uint32_t coeffs = this->registers.Reg32(I2CDEF_HIFIDAC_THD_C3);
  thd_c3_ch1 = (int16_t)coeffs;
  thd_c3_ch2 = (int16_t)(coeffs >> 16);
}



void HiFiDACInterface::SetConfig(HiFiDACConfig config, SuccessCallback&& callback) {
  uint8_t config_val = (config.value & 0xF) | I2CDEF_HIFIDAC_CONTROL_INT_EN_Msk;

  //write desired value
  this->WriteRegister8Async(I2CDEF_HIFIDAC_CONTROL, config_val, [&, callback = std::move(callback), config_val](bool, uint32_t, uint16_t) {
    //read back value to ensure correctness and up-to-date register state
    this->ReadRegister8Async(I2CDEF_HIFIDAC_CONTROL, callback ? [&, callback = std::move(callback), config_val](bool success, uint32_t value, uint16_t) {
      //report result (and value correctness) to external callback
      callback(success && (uint8_t)value == config_val);
    } : ModuleTransferCallback());
  });
}


void HiFiDACInterface::ResetModule(SuccessCallback&& callback) {
  //store reset callback and start timer
  this->reset_callback = std::move(callback);
  this->reset_wait_timer = IF_HIFIDAC_RESET_TIMEOUT;
  this->initialised = false;

  //write reset, synchronously, ignoring exceptions (we expect this transmission to "fail")
  IgnoreExceptions(this->WriteRegister8(I2CDEF_HIFIDAC_CONTROL, I2CDEF_HIFIDAC_CONTROL_RESET_VALUE << I2CDEF_HIFIDAC_CONTROL_RESET_Pos));
}


void HiFiDACInterface::SetVolumes(uint8_t volume_ch1, uint8_t volume_ch2, SuccessCallback&& callback) {
  uint16_t volume_val = volume_ch1;
  ((uint8_t*)&volume_val)[1] = volume_ch2;

  //write desired value
  this->WriteRegister16Async(I2CDEF_HIFIDAC_VOLUME, volume_val, [&, callback = std::move(callback), volume_val](bool, uint32_t, uint16_t) {
    //read back value to ensure correctness and up-to-date register state
    this->ReadRegister16Async(I2CDEF_HIFIDAC_VOLUME, callback ? [&, callback = std::move(callback), volume_val](bool success, uint32_t value, uint16_t) {
      //report result (and value correctness) to external callback
      callback(success && (uint16_t)value == volume_val);
    } : ModuleTransferCallback());
  });
}

void HiFiDACInterface::SetVolumesFromGains(float gain_ch1, float gain_ch2, SuccessCallback&& callback) {
  if (gain_ch1 < -127.5f || gain_ch1 > 0.0f || gain_ch2 < -127.5f || gain_ch2 > 0.0f) {
    throw std::invalid_argument("HiFiDACInterface SetVolumesFromGains invalid gain specified - must be in range [-127.5, 0]");
  }

  uint8_t vol_ch1 = (uint8_t)(gain_ch1 / IF_HIFIDAC_VOLUME_GAIN_PER_STEP);
  uint8_t vol_ch2 = (uint8_t)(gain_ch2 / IF_HIFIDAC_VOLUME_GAIN_PER_STEP);
  this->SetVolumes(vol_ch1, vol_ch2, std::move(callback));
}

void HiFiDACInterface::SetMutes(bool mute_ch1, bool mute_ch2, SuccessCallback&& callback) {
  uint8_t mute_val =
      (mute_ch1 ? I2CDEF_HIFIDAC_MUTE_MUTE_CH1_Msk : 0) |
      (mute_ch2 ? I2CDEF_HIFIDAC_MUTE_MUTE_CH2_Msk : 0);

  //write desired value
  this->WriteRegister8Async(I2CDEF_HIFIDAC_MUTE, mute_val, [&, callback = std::move(callback), mute_val](bool, uint32_t, uint16_t) {
    //read back value to ensure correctness and up-to-date register state
    this->ReadRegister8Async(I2CDEF_HIFIDAC_MUTE, callback ? [&, callback = std::move(callback), mute_val](bool success, uint32_t value, uint16_t) {
      //report result (and value correctness) to external callback
      callback(success && (uint8_t)value == mute_val);
    } : ModuleTransferCallback());
  });
}


void HiFiDACInterface::SetSignalPathSetup(HiFiDACSignalPathSetup setup, SuccessCallback&& callback) {
  uint8_t setup_val = setup.value & 0x7F;

  //write desired value
  this->WriteRegister8Async(I2CDEF_HIFIDAC_PATH, setup_val, [&, callback = std::move(callback), setup_val](bool, uint32_t, uint16_t) {
    //read back value to ensure correctness and up-to-date register state
    this->ReadRegister8Async(I2CDEF_HIFIDAC_PATH, callback ? [&, callback = std::move(callback), setup_val](bool success, uint32_t value, uint16_t) {
      //report result (and value correctness) to external callback
      callback(success && (uint8_t)value == setup_val);
    } : ModuleTransferCallback());
  });
}


void HiFiDACInterface::SetInternalClockConfig(HiFiDACInternalClockConfig config, SuccessCallback&& callback) {
  uint8_t config_val = config.value;

  //write desired value
  this->WriteRegister8Async(I2CDEF_HIFIDAC_CLK_CFG, config_val, [&, callback = std::move(callback), config_val](bool, uint32_t, uint16_t) {
    //read back value to ensure correctness and up-to-date register state
    this->ReadRegister8Async(I2CDEF_HIFIDAC_CLK_CFG, callback ? [&, callback = std::move(callback), config_val](bool success, uint32_t value, uint16_t) {
      //report result (and value correctness) to external callback
      callback(success && (uint8_t)value == config_val);
    } : ModuleTransferCallback());
  });
}

void HiFiDACInterface::SetI2SMasterClockDivider(uint16_t divider, SuccessCallback&& callback) {
  if (divider < 1 || divider > 256) {
    throw std::invalid_argument("HiFiDACInterface SetI2SMasterClockDivider given invalid divider - must be in range [1, 256]");
  }

  uint8_t divider_val = (uint8_t)(divider - 1);

  //write desired value
  this->WriteRegister8Async(I2CDEF_HIFIDAC_MASTER_DIV, divider_val, [&, callback = std::move(callback), divider_val](bool, uint32_t, uint16_t) {
    //read back value to ensure correctness and up-to-date register state
    this->ReadRegister8Async(I2CDEF_HIFIDAC_MASTER_DIV, callback ? [&, callback = std::move(callback), divider_val](bool success, uint32_t value, uint16_t) {
      //report result (and value correctness) to external callback
      callback(success && (uint8_t)value == divider_val);
    } : ModuleTransferCallback());
  });
}


void HiFiDACInterface::SetTDMSlotCount(uint8_t slot_count, SuccessCallback&& callback) {
  if (slot_count < 1 || slot_count > 32) {
    throw std::invalid_argument("HiFiDACInterface SetTDMSlotCount given invalid slot count - must be in range [1, 32]");
  }

  uint8_t count_val = slot_count - 1;

  //write desired value
  this->WriteRegister8Async(I2CDEF_HIFIDAC_TDM_SLOT_NUM, count_val, [&, callback = std::move(callback), count_val](bool, uint32_t, uint16_t) {
    //read back value to ensure correctness and up-to-date register state
    this->ReadRegister8Async(I2CDEF_HIFIDAC_TDM_SLOT_NUM, callback ? [&, callback = std::move(callback), count_val](bool success, uint32_t value, uint16_t) {
      //report result (and value correctness) to external callback
      callback(success && (uint8_t)value == count_val);
    } : ModuleTransferCallback());
  });
}

void HiFiDACInterface::SetChannelTDMSlots(uint8_t slot_ch1, uint8_t slot_ch2, SuccessCallback&& callback) {
  uint8_t slot_count = this->GetTDMSlotCount();
  if (slot_ch1 >= slot_count || slot_ch2 >= slot_count) {
    throw std::invalid_argument("HiFiDACInterface SetChannelTDMSlots given invalid slots - must be in range [0, slot_count - 1]");
  }

  uint16_t slots_val = slot_ch1;
  ((uint8_t*)&slots_val)[1] = slot_ch2;

  //write desired value
  this->WriteRegister16Async(I2CDEF_HIFIDAC_CH_SLOTS, slots_val, [&, callback = std::move(callback), slots_val](bool, uint32_t, uint16_t) {
    //read back value to ensure correctness and up-to-date register state
    this->ReadRegister16Async(I2CDEF_HIFIDAC_CH_SLOTS, callback ? [&, callback = std::move(callback), slots_val](bool success, uint32_t value, uint16_t) {
      //report result (and value correctness) to external callback
      callback(success && (uint16_t)value == slots_val);
    } : ModuleTransferCallback());
  });
}


void HiFiDACInterface::SetInterpolationFilterShape(HiFiDACFilterShape filter_shape, SuccessCallback&& callback) {
  if ((uint8_t)filter_shape > 7) {
    throw std::invalid_argument("HiFiDACInterface SetInterpolationFilterShape given invalid filter shape");
  }

  //write desired value
  this->WriteRegister8Async(I2CDEF_HIFIDAC_FILTER_SHAPE, filter_shape, [&, callback = std::move(callback), filter_shape](bool, uint32_t, uint16_t) {
    //read back value to ensure correctness and up-to-date register state
    this->ReadRegister8Async(I2CDEF_HIFIDAC_FILTER_SHAPE, callback ? [&, callback = std::move(callback), filter_shape](bool success, uint32_t value, uint16_t) {
      //report result (and value correctness) to external callback
      callback(success && (uint8_t)value == filter_shape);
    } : ModuleTransferCallback());
  });
}


void HiFiDACInterface::SetSecondHarmonicCorrectionCoefficients(int16_t thd_c2_ch1, int16_t thd_c2_ch2, SuccessCallback&& callback) {
  uint32_t coeff_val = 0;
  ((int16_t*)&coeff_val)[0] = thd_c2_ch1;
  ((int16_t*)&coeff_val)[1] = thd_c2_ch2;

  //write desired value
  this->WriteRegister32Async(I2CDEF_HIFIDAC_THD_C2, coeff_val, [&, callback = std::move(callback), coeff_val](bool, uint32_t, uint16_t) {
    //read back value to ensure correctness and up-to-date register state
    this->ReadRegister32Async(I2CDEF_HIFIDAC_THD_C2, callback ? [&, callback = std::move(callback), coeff_val](bool success, uint32_t value, uint16_t) {
      //report result (and value correctness) to external callback
      callback(success && value == coeff_val);
    } : ModuleTransferCallback());
  });
}

void HiFiDACInterface::SetThirdHarmonicCorrectionCoefficients(int16_t thd_c3_ch1, int16_t thd_c3_ch2, SuccessCallback&& callback) {
  uint32_t coeff_val = 0;
  ((int16_t*)&coeff_val)[0] = thd_c3_ch1;
  ((int16_t*)&coeff_val)[1] = thd_c3_ch2;

  //write desired value
  this->WriteRegister32Async(I2CDEF_HIFIDAC_THD_C3, coeff_val, [&, callback = std::move(callback), coeff_val](bool, uint32_t, uint16_t) {
    //read back value to ensure correctness and up-to-date register state
    this->ReadRegister32Async(I2CDEF_HIFIDAC_THD_C3, callback ? [&, callback = std::move(callback), coeff_val](bool success, uint32_t value, uint16_t) {
      //report result (and value correctness) to external callback
      callback(success && value == coeff_val);
    } : ModuleTransferCallback());
  });
}



void HiFiDACInterface::InitModule(SuccessCallback&& callback) {
  this->initialised = false;

  this->reset_wait_timer = 0;

  //read module ID to check communication
  this->ReadRegister8Async(I2CDEF_HIFIDAC_MODULE_ID, [&, callback = std::move(callback)](bool success, uint32_t value, uint16_t) {
    if (!success) {
      //report failure to external callback
      if (callback) {
        callback(false);
      }
      return;
    }

    //check correctness of module ID
    if ((uint8_t)value != I2CDEF_HIFIDAC_MODULE_ID_VALUE) {
      DEBUG_PRINTF("* HiFiDAC module ID incorrect: 0x%02X instead of 0x%02X\n", (uint8_t)value, I2CDEF_HIFIDAC_MODULE_ID_VALUE);
      //report failure to external callback
      if (callback) {
        callback(false);
      }
      return;
    }

    //write interrupt mask (enable all interrupts)
    this->SetInterruptMask(0xF, [&, callback = std::move(callback)](bool success) {
      if (!success) {
        //report failure to external callback
        if (callback) {
          callback(false);
        }
        return;
      }

      //set basic config (async slave, dac disabled) to enable interrupts
      HiFiDACConfig cfg;
      cfg.dac_enable = false;
      cfg.sync_mode = false;
      cfg.master_mode = false;
      this->SetConfig(cfg, [&, callback = std::move(callback)](bool success) {
        if (!success) {
          //report failure to external callback
          if (callback) {
            callback(false);
          }
          return;
        }

        //read all registers once to update registers to their initial values
        this->ReadMultiRegisterAsync(I2CDEF_HIFIDAC_VOLUME, hifidac_scratch, 7, ModuleTransferCallback());
        this->ReadMultiRegisterAsync(I2CDEF_HIFIDAC_FILTER_SHAPE, hifidac_scratch, 3, [&, callback = std::move(callback)](bool, uint32_t, uint16_t) {
          //after last read is done: init completed successfully (even if read failed - that's non-critical)
          this->initialised = true;
          if (callback) {
            callback(true);
          }
        });
      });
    });
  });
}

void HiFiDACInterface::LoopTasks() {
  static uint32_t loop_count = 0;

  if (this->initialised && loop_count++ % 50 == 0) {
    //every 50 cycles (500ms), read status - no callback needed, we're only reading to update the register
    this->ReadRegister8Async(I2CDEF_HIFIDAC_STATUS, ModuleTransferCallback());
  }

  //allow base handling
  this->IntRegI2CModuleInterface::LoopTasks();

  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  if (this->reset_wait_timer > 0) {
    //check for reset timeout
    if (--this->reset_wait_timer == 0) {
      //timed out: report to callback
      __set_PRIMASK(primask);
      if (this->reset_callback) {
        this->reset_callback(false);
        //clear reset callback
        this->reset_callback = SuccessCallback();
      }
    }
  }
  __set_PRIMASK(primask);
}



HiFiDACInterface::HiFiDACInterface(I2CHardwareInterface& hw_interface, uint8_t i2c_address, GPIO_TypeDef* int_port, uint16_t int_pin) :
        IntRegI2CModuleInterface(hw_interface, i2c_address, I2CDEF_HIFIDAC_REG_SIZES, int_port, int_pin, IF_HIFIDAC_USE_CRC), initialised(false), reset_wait_timer(0) {}


void HiFiDACInterface::OnRegisterUpdate(uint8_t address) {
  //allow base handling
  this->IntRegI2CModuleInterface::OnRegisterUpdate(address);

  if (address == I2CDEF_HIFIDAC_STATUS) {
    this->ExecuteCallbacks(MODIF_HIFIDAC_EVENT_STATUS_UPDATE);
  }
}

void HiFiDACInterface::OnI2CInterrupt(uint16_t interrupt_flags) {
  //allow base handling
  this->IntRegI2CModuleInterface::OnI2CInterrupt(interrupt_flags);

  if ((interrupt_flags & MODIF_I2C_INT_RESET_FLAG) != 0) {
    //reset condition: only re-initialise if already initialised, or reset is pending
    if (this->initialised || this->reset_wait_timer > 0) {
      if (this->reset_wait_timer == 0) {
        DEBUG_PRINTF("HiFiDAC module spurious reset detected\n");
      }
      //perform module re-init
      this->InitModule([&](bool success) {
        //notify system of reset, then call reset callback
        this->ExecuteCallbacks(MODIF_EVENT_MODULE_RESET);
        if (this->reset_callback) {
          this->reset_callback(success);
          //clear reset callback
          this->reset_callback = SuccessCallback();
        }
      });
    }
    return;
  } else if (interrupt_flags != 0) {
    //all other DAC interrupts just affect the status register, so read that
    this->ReadRegister8Async(I2CDEF_HIFIDAC_STATUS, ModuleTransferCallback());
  }
}

