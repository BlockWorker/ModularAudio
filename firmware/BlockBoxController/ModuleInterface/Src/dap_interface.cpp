/*
 * dap_interface.cpp
 *
 *  Created on: Jul 12, 2025
 *      Author: Alex
 */


#include "dap_interface.h"
#include <math.h>


//write buffers for larger structures - TODO make something more flexible if overlapping writes become a problem
static DAPMixerConfig mixer_write_buf;
static DAPGains volume_write_buf;
static DAPGains loudness_write_buf;

//scratch space for register discard-reads
static uint8_t dap_scratch[1200];


DAPStatus DAPInterface::GetStatus() const {
  DAPStatus status;
  status.value = this->registers.Reg8(I2CDEF_DAP_STATUS);
  return status;
}

void DAPInterface::GetConfig(bool& sp_enabled, bool& pos_gain_allowed) const {
  uint8_t config_val = this->registers.Reg8(I2CDEF_DAP_CONTROL);
  sp_enabled = (config_val & I2CDEF_DAP_CONTROL_SP_EN_Msk) != 0;
  pos_gain_allowed = (config_val & I2CDEF_DAP_CONTROL_ALLOW_POS_GAIN_Msk) != 0;
}


DAPInput DAPInterface::GetActiveInput() const {
  return (DAPInput)this->registers.Reg8(I2CDEF_DAP_INPUT_ACTIVE);
}

DAPAvailableInputs DAPInterface::GetAvailableInputs() const {
  DAPAvailableInputs inputs;
  inputs.value = this->registers.Reg8(I2CDEF_DAP_INPUTS_AVAILABLE);
  return inputs;
}

bool DAPInterface::IsInputAvailable(DAPInput input) const {
  uint8_t inputs_val = this->registers.Reg8(I2CDEF_DAP_INPUTS_AVAILABLE);
  switch (input) {
    case IF_DAP_INPUT_I2S1:
      return (inputs_val & I2CDEF_DAP_INPUTS_AVAILABLE_I2S1_Msk) != 0;
    case IF_DAP_INPUT_I2S2:
      return (inputs_val & I2CDEF_DAP_INPUTS_AVAILABLE_I2S2_Msk) != 0;
    case IF_DAP_INPUT_I2S3:
      return (inputs_val & I2CDEF_DAP_INPUTS_AVAILABLE_I2S3_Msk) != 0;
    case IF_DAP_INPUT_USB:
      return (inputs_val & I2CDEF_DAP_INPUTS_AVAILABLE_USB_Msk) != 0;
    case IF_DAP_INPUT_SPDIF:
      return (inputs_val & I2CDEF_DAP_INPUTS_AVAILABLE_SPDIF_Msk) != 0;
    default:
      return false;
  }
}


DAPSampleRate DAPInterface::GetI2SInputSampleRate(DAPInput input) const {
  switch (input) {
    case IF_DAP_INPUT_I2S1:
      return (DAPSampleRate)this->registers.Reg32(I2CDEF_DAP_I2S1_SAMPLE_RATE);
    case IF_DAP_INPUT_I2S2:
      return (DAPSampleRate)this->registers.Reg32(I2CDEF_DAP_I2S2_SAMPLE_RATE);
    case IF_DAP_INPUT_I2S3:
      return (DAPSampleRate)this->registers.Reg32(I2CDEF_DAP_I2S3_SAMPLE_RATE);
    default:
      throw std::invalid_argument("DAPInterface GetI2SInputSampleRate given invalid or non-I2S input");
  }
}


DAPSampleRate DAPInterface::GetSRCInputSampleRate() const {
  return (DAPSampleRate)this->registers.Reg32(I2CDEF_DAP_SRC_INPUT_RATE);
}

float DAPInterface::GetSRCInputRateErrorRelative() const {
  uint32_t int_val = this->registers.Reg32(I2CDEF_DAP_SRC_RATE_ERROR);
  return *(float*)&int_val;
}

float DAPInterface::GetSRCBufferFillErrorSamples() const {
  uint32_t int_val = this->registers.Reg32(I2CDEF_DAP_SRC_BUFFER_ERROR);
  return *(float*)&int_val;
}


DAPMixerConfig DAPInterface::GetMixerConfig() const {
  DAPMixerConfig config;
  memcpy(&config, this->registers[I2CDEF_DAP_MIXER_GAINS], sizeof(DAPMixerConfig));
  return config;
}

DAPGains DAPInterface::GetVolumeGains() const {
  DAPGains gains;
  memcpy(&gains, this->registers[I2CDEF_DAP_VOLUME_GAINS], sizeof(DAPGains));
  return gains;
}

DAPGains DAPInterface::GetLoudnessGains() const {
  DAPGains gains;
  memcpy(&gains, this->registers[I2CDEF_DAP_LOUDNESS_GAINS], sizeof(DAPGains));
  return gains;
}


const q31_t* DAPInterface::GetBiquadCoefficients(DAPChannel channel) const {
  switch (channel) {
    case IF_DAP_CH1:
      return (const q31_t*)this->registers[I2CDEF_DAP_BIQUAD_COEFFS_CH1];
    case IF_DAP_CH2:
      return (const q31_t*)this->registers[I2CDEF_DAP_BIQUAD_COEFFS_CH2];
    default:
      throw std::invalid_argument("DAPInterface GetBiquadCoefficients given invalid channel");
  }
}

DAPBiquadSetup DAPInterface::GetBiquadSetup() const {
  DAPBiquadSetup setup;
  memcpy(&setup, this->registers[I2CDEF_DAP_BIQUAD_SETUP], sizeof(DAPBiquadSetup));
  return setup;
}

const q31_t* DAPInterface::GetFIRCoefficients(DAPChannel channel) const {
  switch (channel) {
    case IF_DAP_CH1:
      return (const q31_t*)this->registers[I2CDEF_DAP_FIR_COEFFS_CH1];
    case IF_DAP_CH2:
      return (const q31_t*)this->registers[I2CDEF_DAP_FIR_COEFFS_CH2];
    default:
      throw std::invalid_argument("DAPInterface GetFIRCoefficients given invalid channel");
  }
}

DAPFIRSetup DAPInterface::GetFIRSetup() const {
  DAPFIRSetup setup;
  memcpy(&setup, this->registers[I2CDEF_DAP_FIR_SETUP], sizeof(DAPFIRSetup));
  return setup;
}



void DAPInterface::SetConfig(bool sp_enabled, bool pos_gain_allowed, SuccessCallback&& callback) {
  uint8_t config_val =
      I2CDEF_DAP_CONTROL_INT_EN_Msk |
      (sp_enabled ? I2CDEF_DAP_CONTROL_SP_EN_Msk : 0) |
      (pos_gain_allowed ? I2CDEF_DAP_CONTROL_ALLOW_POS_GAIN_Msk : 0);

  //write desired value
  this->WriteRegister8Async(I2CDEF_DAP_CONTROL, config_val, [&, callback = std::move(callback), config_val](bool, uint32_t, uint16_t) {
    //read back value to ensure correctness and up-to-date register state
    this->ReadRegister8Async(I2CDEF_DAP_CONTROL, callback ? [&, callback = std::move(callback), config_val](bool success, uint32_t value, uint16_t) {
      //report result (and value correctness) to external callback
      callback(success && (uint8_t)value == config_val);
    } : ModuleTransferCallback());
  });
}


void DAPInterface::ResetModule(SuccessCallback&& callback) {
  //store reset callback and start timer
  this->reset_callback = std::move(callback);
  this->reset_wait_timer = IF_DAP_RESET_TIMEOUT;
  this->initialised = false;

  //write reset, synchronously, ignoring exceptions (we expect this transmission to "fail")
  IgnoreExceptions(this->WriteRegister8(I2CDEF_DAP_CONTROL, I2CDEF_DAP_CONTROL_RESET_VALUE << I2CDEF_DAP_CONTROL_RESET_Pos));
}


void DAPInterface::SetActiveInput(DAPInput input, SuccessCallback&& callback) {
  if (input < IF_DAP_INPUT_I2S1 || input > IF_DAP_INPUT_SPDIF || !this->IsInputAvailable(input)) {
    throw std::invalid_argument("DAPInterface SetActiveInput given invalid or unavailable input");
  }

  //write desired value
  this->WriteRegister8Async(I2CDEF_DAP_INPUT_ACTIVE, (uint8_t)input, [&, callback = std::move(callback), input](bool, uint32_t, uint16_t) {
    //read back value to ensure correctness and up-to-date register state
    this->ReadRegister8Async(I2CDEF_DAP_INPUT_ACTIVE, callback ? [&, callback = std::move(callback), input](bool success, uint32_t value, uint16_t) {
      //report result (and value correctness) to external callback
      callback(success && (uint8_t)value == (uint8_t)input);
    } : ModuleTransferCallback());
  });
}


void DAPInterface::SetI2SInputSampleRate(DAPInput input, DAPSampleRate sample_rate, SuccessCallback&& callback) {
  uint8_t reg;
  switch (input) {
    case IF_DAP_INPUT_I2S1:
      reg = I2CDEF_DAP_I2S1_SAMPLE_RATE;
      break;
    case IF_DAP_INPUT_I2S2:
      reg = I2CDEF_DAP_I2S2_SAMPLE_RATE;
      break;
    case IF_DAP_INPUT_I2S3:
      reg = I2CDEF_DAP_I2S3_SAMPLE_RATE;
      break;
    default:
      throw std::invalid_argument("DAPInterface SetI2SInputSampleRate given invalid or non-I2S input");
  }

  if (sample_rate != IF_DAP_SR_44K && sample_rate != IF_DAP_SR_48K && sample_rate != IF_DAP_SR_96K) {
    throw std::invalid_argument("DAPInterface SetI2SInputSampleRate given invalid sample rate");
  }

  //write desired value
  this->WriteRegister32Async(reg, (uint32_t)sample_rate, [&, callback = std::move(callback), sample_rate, reg](bool, uint32_t, uint16_t) {
    //read back value to ensure correctness and up-to-date register state
    this->ReadRegister32Async(reg, callback ? [&, callback = std::move(callback), sample_rate](bool success, uint32_t value, uint16_t) {
      //report result (and value correctness) to external callback
      callback(success && value == (uint32_t)sample_rate);
    } : ModuleTransferCallback());
  });
}


void DAPInterface::SetMixerConfig(DAPMixerConfig config, SuccessCallback&& callback) {
  memcpy(&mixer_write_buf, &config, sizeof(DAPMixerConfig));

  //write desired config
  this->WriteRegisterAsync(I2CDEF_DAP_MIXER_GAINS, (const uint8_t*)&mixer_write_buf, [&, callback = std::move(callback), config](bool, uint32_t, uint16_t) {
    //read back config to ensure correctness and up-to-date register state
    this->ReadRegisterAsync(I2CDEF_DAP_MIXER_GAINS, dap_scratch, callback ? [&, callback = std::move(callback), config](bool success, uint32_t, uint16_t) {
      //report result (and config correctness) to external callback
      callback(success && memcmp(this->registers[I2CDEF_DAP_MIXER_GAINS], &config, sizeof(DAPMixerConfig)) == 0);
    } : ModuleTransferCallback());
  });
}

void DAPInterface::SetVolumeGains(DAPGains gains, SuccessCallback&& callback) {
  bool sp_enabled, pos_gain_allowed;
  this->GetConfig(sp_enabled, pos_gain_allowed);
  if (isnanf(gains.ch1) || gains.ch1 < IF_DAP_VOLUME_GAIN_MIN || gains.ch1 > (pos_gain_allowed ? IF_DAP_VOLUME_GAIN_MAX : 0.0f) ||
      isnanf(gains.ch2) || gains.ch2 < IF_DAP_VOLUME_GAIN_MIN || gains.ch2 > (pos_gain_allowed ? IF_DAP_VOLUME_GAIN_MAX : 0.0f)) {
    throw std::invalid_argument("DAPInterface SetVolumeGains given invalid gains - must be in range [-120, 0] or [-120, 20] if positive gain is allowed");
  }

  memcpy(&volume_write_buf, &gains, sizeof(DAPGains));

  //write desired config
  this->WriteRegisterAsync(I2CDEF_DAP_VOLUME_GAINS, (const uint8_t*)&volume_write_buf, [&, callback = std::move(callback), gains](bool, uint32_t, uint16_t) {
    //read back config to ensure correctness and up-to-date register state
    this->ReadRegisterAsync(I2CDEF_DAP_VOLUME_GAINS, dap_scratch, callback ? [&, callback = std::move(callback), gains](bool success, uint32_t, uint16_t) {
      //report result (and config correctness) to external callback
      callback(success && memcmp(this->registers[I2CDEF_DAP_VOLUME_GAINS], &gains, sizeof(DAPGains)) == 0);
    } : ModuleTransferCallback());
  });
}

void DAPInterface::SetLoudnessGains(DAPGains gains, SuccessCallback&& callback) {
  if (isnanf(gains.ch1) || gains.ch1 > IF_DAP_LOUDNESS_GAIN_MAX ||
      isnanf(gains.ch2) || gains.ch2 > IF_DAP_LOUDNESS_GAIN_MAX) {
    throw std::invalid_argument("DAPInterface SetLoudnessGains given invalid gains - must be 0 or less");
  }

  memcpy(&loudness_write_buf, &gains, sizeof(DAPGains));

  //write desired config
  this->WriteRegisterAsync(I2CDEF_DAP_LOUDNESS_GAINS, (const uint8_t*)&loudness_write_buf, [&, callback = std::move(callback), gains](bool, uint32_t, uint16_t) {
    //read back config to ensure correctness and up-to-date register state
    this->ReadRegisterAsync(I2CDEF_DAP_LOUDNESS_GAINS, dap_scratch, callback ? [&, callback = std::move(callback), gains](bool success, uint32_t, uint16_t) {
      //report result (and config correctness) to external callback
      callback(success && memcmp(this->registers[I2CDEF_DAP_LOUDNESS_GAINS], &gains, sizeof(DAPGains)) == 0);
    } : ModuleTransferCallback());
  });
}


static inline void _DAPInterface_EnsureSignalProcessorDisabled(const DAPInterface* dap_if) {
  bool sp_enabled, pos_gain_allowed;
  dap_if->GetConfig(sp_enabled, pos_gain_allowed);
  if (sp_enabled) {
    throw std::logic_error("DAPInterface signal processor must be disabled before adjusting filter parameters/setups");
  }
}

void DAPInterface::SetBiquadCoefficients(DAPChannel channel, const q31_t* coeff_buffer, SuccessCallback&& callback) {
  if (coeff_buffer == NULL) {
    throw std::invalid_argument("DAPInterface SetBiquadCoefficients given null buffer");
  }

  uint8_t reg;
  switch (channel) {
    case IF_DAP_CH1:
      reg = I2CDEF_DAP_BIQUAD_COEFFS_CH1;
      break;
    case IF_DAP_CH2:
      reg = I2CDEF_DAP_BIQUAD_COEFFS_CH2;
      break;
    default:
      throw std::invalid_argument("DAPInterface SetBiquadCoefficients given invalid channel");
  }

  _DAPInterface_EnsureSignalProcessorDisabled(this);

  //write desired coefficients
  this->WriteRegisterAsync(reg, (const uint8_t*)coeff_buffer, [&, callback = std::move(callback), coeff_buffer, reg](bool, uint32_t, uint16_t) {
    //read back coefficients to ensure correctness and up-to-date register state
    this->ReadRegisterAsync(reg, dap_scratch, callback ? [&, callback = std::move(callback), coeff_buffer, reg](bool success, uint32_t, uint16_t) {
      //report result (and coefficient correctness) to external callback
      callback(success && memcmp(this->registers[reg], coeff_buffer, this->registers.reg_sizes[reg]) == 0);
    } : ModuleTransferCallback());
  });
}

void DAPInterface::SetBiquadSetup(DAPBiquadSetup setup, SuccessCallback&& callback) {
  _DAPInterface_EnsureSignalProcessorDisabled(this);

  uint32_t setup_value;
  memcpy(&setup_value, &setup, sizeof(uint32_t));

  //write desired setup
  this->WriteRegister32Async(I2CDEF_DAP_BIQUAD_SETUP, setup_value, [&, callback = std::move(callback), setup_value](bool, uint32_t, uint16_t) {
    //read back setup to ensure correctness and up-to-date register state
    this->ReadRegister32Async(I2CDEF_DAP_BIQUAD_SETUP, callback ? [&, callback = std::move(callback), setup_value](bool success, uint32_t value, uint16_t) {
      //report result (and setup correctness) to external callback
      callback(success && value == setup_value);
    } : ModuleTransferCallback());
  });
}


void DAPInterface::SetFIRCoefficients(DAPChannel channel, const q31_t* coeff_buffer, SuccessCallback&& callback) {
  if (coeff_buffer == NULL) {
    throw std::invalid_argument("DAPInterface SetFIRCoefficients given null buffer");
  }

  uint8_t reg;
  switch (channel) {
    case IF_DAP_CH1:
      reg = I2CDEF_DAP_FIR_COEFFS_CH1;
      break;
    case IF_DAP_CH2:
      reg = I2CDEF_DAP_FIR_COEFFS_CH2;
      break;
    default:
      throw std::invalid_argument("DAPInterface SetFIRCoefficients given invalid channel");
  }

  _DAPInterface_EnsureSignalProcessorDisabled(this);

  //write desired coefficients
  this->WriteRegisterAsync(reg, (const uint8_t*)coeff_buffer, [&, callback = std::move(callback), coeff_buffer, reg](bool, uint32_t, uint16_t) {
    //read back coefficients to ensure correctness and up-to-date register state
    this->ReadRegisterAsync(reg, dap_scratch, callback ? [&, callback = std::move(callback), coeff_buffer, reg](bool success, uint32_t, uint16_t) {
      //report result (and coefficient correctness) to external callback
      callback(success && memcmp(this->registers[reg], coeff_buffer, this->registers.reg_sizes[reg]) == 0);
    } : ModuleTransferCallback());
  });
}

void DAPInterface::SetFIRSetup(DAPFIRSetup setup, SuccessCallback&& callback) {
  _DAPInterface_EnsureSignalProcessorDisabled(this);

  uint32_t setup_value;
  memcpy(&setup_value, &setup, sizeof(uint32_t));

  //write desired setup
  this->WriteRegister32Async(I2CDEF_DAP_FIR_SETUP, setup_value, [&, callback = std::move(callback), setup_value](bool, uint32_t, uint16_t) {
    //read back setup to ensure correctness and up-to-date register state
    this->ReadRegister32Async(I2CDEF_DAP_FIR_SETUP, callback ? [&, callback = std::move(callback), setup_value](bool success, uint32_t value, uint16_t) {
      //report result (and setup correctness) to external callback
      callback(success && value == setup_value);
    } : ModuleTransferCallback());
  });
}



void DAPInterface::InitModule(SuccessCallback&& callback) {
  this->initialised = false;

  this->reset_wait_timer = 0;

  //read module ID to check communication
  this->ReadRegister8Async(I2CDEF_DAP_MODULE_ID, [&, callback = std::move(callback)](bool success, uint32_t value, uint16_t) {
    if (!success) {
      //report failure to external callback
      if (callback) {
        callback(false);
      }
      return;
    }

    //check correctness of module ID
    if ((uint8_t)value != I2CDEF_DAP_MODULE_ID_VALUE) {
      DEBUG_PRINTF("* DAP module ID incorrect: 0x%02X instead of 0x%02X\n", (uint8_t)value, I2CDEF_DAP_MODULE_ID_VALUE);
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

      //set config to enable interrupts and disable signal processor and positive gain for now
      this->SetConfig(false, false, [&, callback = std::move(callback)](bool success) {
        if (!success) {
          //report failure to external callback
          if (callback) {
            callback(false);
          }
          return;
        }

        //read all registers once to update registers to their initial values
        this->ReadMultiRegisterAsync(I2CDEF_DAP_INPUT_ACTIVE, dap_scratch, 2, ModuleTransferCallback());
        this->ReadMultiRegisterAsync(I2CDEF_DAP_I2S1_SAMPLE_RATE, dap_scratch, 3, ModuleTransferCallback());
        this->ReadMultiRegisterAsync(I2CDEF_DAP_SRC_INPUT_RATE, dap_scratch, 3, ModuleTransferCallback());
        this->ReadMultiRegisterAsync(I2CDEF_DAP_MIXER_GAINS, dap_scratch, 5, ModuleTransferCallback());
        this->ReadMultiRegisterAsync(I2CDEF_DAP_BIQUAD_COEFFS_CH1, dap_scratch, 2, ModuleTransferCallback());
        this->ReadRegisterAsync(I2CDEF_DAP_FIR_COEFFS_CH1, dap_scratch, ModuleTransferCallback());
        this->ReadRegisterAsync(I2CDEF_DAP_FIR_COEFFS_CH2, dap_scratch, [&, callback = std::move(callback)](bool, uint32_t, uint16_t) {
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

void DAPInterface::LoopTasks() {
  static uint32_t loop_count = 0;

  if (this->initialised && loop_count++ % 50 == 0) {
    //every 50 cycles (500ms), read status - no callback needed, we're only reading to update the register
    this->ReadRegister8Async(I2CDEF_DAP_STATUS, ModuleTransferCallback());

    //if SRC stats monitoring is requested, read the corresponding registers periodically too
    if (this->monitor_src_stats) {
      this->ReadRegister32Async(I2CDEF_DAP_SRC_RATE_ERROR, ModuleTransferCallback());
      this->ReadRegister32Async(I2CDEF_DAP_SRC_BUFFER_ERROR, ModuleTransferCallback());
    }
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



DAPInterface::DAPInterface(I2CHardwareInterface& hw_interface, uint8_t i2c_address, GPIO_TypeDef* int_port, uint16_t int_pin) :
    IntRegI2CModuleInterface(hw_interface, i2c_address, I2CDEF_DAP_REG_SIZES, int_port, int_pin, IF_DAP_USE_CRC), monitor_src_stats(false), initialised(false), reset_wait_timer(0) {}



void DAPInterface::OnRegisterUpdate(uint8_t address) {
  //allow base handling
  this->IntRegI2CModuleInterface::OnRegisterUpdate(address);

  uint32_t event;

  //select corresponding event
  switch (address) {
    case I2CDEF_DAP_STATUS:
      event = MODIF_DAP_EVENT_STATUS_UPDATE;
      break;
    case I2CDEF_DAP_INPUT_ACTIVE:
    case I2CDEF_DAP_INPUTS_AVAILABLE:
      event = MODIF_DAP_EVENT_INPUTS_UPDATE;
      break;
    case I2CDEF_DAP_SRC_INPUT_RATE:
      event = MODIF_DAP_EVENT_INPUT_RATE_UPDATE;
      break;
    case I2CDEF_DAP_SRC_RATE_ERROR:
    case I2CDEF_DAP_SRC_BUFFER_ERROR:
      event = MODIF_DAP_EVENT_SRC_STATS_UPDATE;
      break;
    default:
      return;
  }

  this->ExecuteCallbacks(event);
}

void DAPInterface::OnI2CInterrupt(uint16_t interrupt_flags) {
  //allow base handling
  this->IntRegI2CModuleInterface::OnI2CInterrupt(interrupt_flags);

  if (interrupt_flags == MODIF_I2C_INT_RESET_FLAG) {
    //reset condition: only re-initialise if already initialised, or reset is pending
    if (this->initialised || this->reset_wait_timer > 0) {
      if (this->reset_wait_timer == 0) {
        DEBUG_PRINTF("DAP module spurious reset detected\n");
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
  }

  if ((interrupt_flags & I2CDEF_DAP_INT_FLAGS_INT_SRC_READY_Msk) != 0) {
    //SRC ready state changed: read status
    this->ReadRegister8Async(I2CDEF_DAP_STATUS, ModuleTransferCallback());
  }

  if ((interrupt_flags & I2CDEF_DAP_INT_FLAGS_INT_ACTIVE_INPUT_Msk) != 0) {
    //active input changed: read active input
    this->ReadRegister8Async(I2CDEF_DAP_INPUT_ACTIVE, ModuleTransferCallback());
  }

  if ((interrupt_flags & I2CDEF_DAP_INT_FLAGS_INT_INPUT_AVAILABLE_Msk) != 0) {
    //available inputs changed: read available inputs
    this->ReadRegister8Async(I2CDEF_DAP_INPUTS_AVAILABLE, ModuleTransferCallback());
  }

  if ((interrupt_flags & I2CDEF_DAP_INT_FLAGS_INT_INPUT_RATE_Msk) != 0) {
    //input rate changed: read input rate
    this->ReadRegister32Async(I2CDEF_DAP_SRC_INPUT_RATE, ModuleTransferCallback());
  }
}

