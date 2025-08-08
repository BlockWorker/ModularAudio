/*
 * power_amp_interface.cpp
 *
 *  Created on: Jul 14, 2025
 *      Author: Alex
 */


#include "power_amp_interface.h"
#include "math.h"


static uint8_t poweramp_scratch[128];


PowerAmpStatus PowerAmpInterface::GetStatus() const {
  PowerAmpStatus status;
  status.value = this->registers.Reg16(I2CDEF_POWERAMP_STATUS);
  return status;
}


bool PowerAmpInterface::IsManualShutdownActive() const {
  return (this->registers.Reg8(I2CDEF_POWERAMP_CONTROL) & I2CDEF_POWERAMP_CONTROL_AMP_MAN_SD_Msk) != 0;
}


float PowerAmpInterface::GetPVDDTargetVoltage() const {
  uint32_t value = this->registers.Reg32(I2CDEF_POWERAMP_PVDD_TARGET);
  return *(float*)&value;
}

float PowerAmpInterface::GetPVDDRequestedVoltage() const {
  uint32_t value = this->registers.Reg32(I2CDEF_POWERAMP_PVDD_REQ);
  return *(float*)&value;
}

float PowerAmpInterface::GetPVDDMeasuredVoltage() const {
  uint32_t value = this->registers.Reg32(I2CDEF_POWERAMP_PVDD_MEASURED);
  return *(float*)&value;
}


//fast = true: 0.1s smoothing; fast = false: 1s smoothing
PowerAmpMeasurements PowerAmpInterface::GetOutputMeasurements(bool fast) const {
  PowerAmpMeasurements measurements;

  uint8_t reg = fast ? I2CDEF_POWERAMP_MON_VRMS_FAST_A : I2CDEF_POWERAMP_MON_VRMS_SLOW_A;
  memcpy(&measurements, this->registers[reg], sizeof(PowerAmpMeasurements));

  return measurements;
}


bool PowerAmpInterface::IsSafetyShutdownActive() const {
  return (this->registers.Reg8(I2CDEF_POWERAMP_SAFETY_STATUS) & I2CDEF_POWERAMP_SAFETY_STATUS_SAFETY_SERR_SD_Msk) != 0;
}

PowerAmpErrWarnSource PowerAmpInterface::GetSafetyErrorSource() const {
  PowerAmpErrWarnSource source;
  source.value = this->registers.Reg16(I2CDEF_POWERAMP_SERR_SOURCE);
  return source;
}

PowerAmpErrWarnSource PowerAmpInterface::GetSafetyWarningSource() const {
  PowerAmpErrWarnSource source;
  source.value = this->registers.Reg16(I2CDEF_POWERAMP_SWARN_SOURCE);
  return source;
}


PowerAmpThresholdSet PowerAmpInterface::GetSafetyThresholds(PowerAmpThresholdType type) const {
  PowerAmpThresholdSet thresholds;

  uint8_t reg;
  switch (type) {
    case IF_POWERAMP_THR_IRMS_ERROR:
      reg = I2CDEF_POWERAMP_SERR_IRMS_INST_A;
      break;
    case IF_POWERAMP_THR_PAVG_ERROR:
      reg = I2CDEF_POWERAMP_SERR_PAVG_INST_A;
      break;
    case IF_POWERAMP_THR_PAPP_ERROR:
      reg = I2CDEF_POWERAMP_SERR_PAPP_INST_A;
      break;
    case IF_POWERAMP_THR_IRMS_WARNING:
      reg = I2CDEF_POWERAMP_SWARN_IRMS_INST_A;
      break;
    case IF_POWERAMP_THR_PAVG_WARNING:
      reg = I2CDEF_POWERAMP_SWARN_PAVG_INST_A;
      break;
    case IF_POWERAMP_THR_PAPP_WARNING:
      reg = I2CDEF_POWERAMP_SWARN_PAPP_INST_A;
      break;
    default:
      throw std::invalid_argument("PowerAmpInterface GetSafetyThresholds given invalid threshold type");
  }

  memcpy(&thresholds, this->registers[reg], sizeof(PowerAmpThresholdSet));

  return thresholds;
}



void PowerAmpInterface::SetManualShutdownActive(bool manual_shutdown, SuccessCallback&& callback) {
  uint8_t config_val =
      I2CDEF_POWERAMP_CONTROL_INT_EN_Msk |
      (manual_shutdown ? I2CDEF_POWERAMP_CONTROL_AMP_MAN_SD_Msk : 0);

  //write desired value
  this->WriteRegister8Async(I2CDEF_POWERAMP_CONTROL, config_val, [&, callback = std::move(callback), config_val](bool, uint32_t, uint16_t) {
    //read back value to ensure correctness and up-to-date register state
    this->ReadRegister8Async(I2CDEF_POWERAMP_CONTROL, callback ? [&, callback = std::move(callback), config_val](bool success, uint32_t value, uint16_t) {
      //report result (and value correctness) to external callback
      callback(success && (uint8_t)value == config_val);
    } : ModuleTransferCallback());
  });
}


void PowerAmpInterface::ResetModule(SuccessCallback&& callback) {
  //store reset callback and start timer
  this->reset_callback = std::move(callback);
  this->reset_wait_timer = IF_POWERAMP_RESET_TIMEOUT;
  this->initialised = false;

  //write reset, synchronously, ignoring exceptions (we expect this transmission to "fail")
  IgnoreExceptions(this->WriteRegister8(I2CDEF_POWERAMP_CONTROL, I2CDEF_POWERAMP_CONTROL_RESET_VALUE << I2CDEF_POWERAMP_CONTROL_RESET_Pos));
}


void PowerAmpInterface::SetPVDDTargetVoltage(float voltage, SuccessCallback&& callback) {
  if (isnanf(voltage) || voltage < IF_POWERAMP_PVDD_TARGET_MIN || voltage > IF_POWERAMP_PVDD_TARGET_MAX) {
    throw std::invalid_argument("PowerAmpInterface SetPVDDTargetVoltage given invalid voltage, must be in range [18.7, 53.5]");
  }

  uint32_t voltage_val = 0;
  ((float*)&voltage_val)[0] = voltage;

  //write desired value
  this->WriteRegister32Async(I2CDEF_POWERAMP_PVDD_TARGET, voltage_val, [&, callback = std::move(callback), voltage_val](bool, uint32_t, uint16_t) {
    //read back value to ensure correctness and up-to-date register state
    this->ReadRegister32Async(I2CDEF_POWERAMP_PVDD_TARGET, callback ? [&, callback = std::move(callback), voltage_val](bool success, uint32_t value, uint16_t) {
      //report result (and value correctness) to external callback
      callback(success && value == voltage_val);
    } : ModuleTransferCallback());
  });
}


void PowerAmpInterface::SetSafetyThresholds(PowerAmpThresholdType type, const PowerAmpThresholdSet* thresholds, SuccessCallback&& callback) {
  if (thresholds == NULL) {
    throw std::invalid_argument("PowerAmpInterface SetSafetyThresholds given null pointer");
  }

  uint8_t reg;
  switch (type) {
    case IF_POWERAMP_THR_IRMS_ERROR:
      reg = I2CDEF_POWERAMP_SERR_IRMS_INST_A;
      break;
    case IF_POWERAMP_THR_PAVG_ERROR:
      reg = I2CDEF_POWERAMP_SERR_PAVG_INST_A;
      break;
    case IF_POWERAMP_THR_PAPP_ERROR:
      reg = I2CDEF_POWERAMP_SERR_PAPP_INST_A;
      break;
    case IF_POWERAMP_THR_IRMS_WARNING:
      reg = I2CDEF_POWERAMP_SWARN_IRMS_INST_A;
      break;
    case IF_POWERAMP_THR_PAVG_WARNING:
      reg = I2CDEF_POWERAMP_SWARN_PAVG_INST_A;
      break;
    case IF_POWERAMP_THR_PAPP_WARNING:
      reg = I2CDEF_POWERAMP_SWARN_PAPP_INST_A;
      break;
    default:
      throw std::invalid_argument("PowerAmpInterface SetSafetyThresholds given invalid threshold type");
  }

  //write desired thresholds
  this->WriteMultiRegisterAsync(reg, (const uint8_t*)thresholds, 15, [&, callback = std::move(callback), reg, thresholds](bool, uint32_t, uint16_t) {
    //read back thresholds to ensure correctness and up-to-date register state
    this->ReadMultiRegisterAsync(reg, poweramp_scratch, 15, callback ? [&, callback = std::move(callback), reg, thresholds](bool success, uint32_t, uint16_t) {
      //report result (and threshold correctness) to external callback
      callback(success && memcmp(thresholds, this->registers[reg], sizeof(PowerAmpThresholdSet)) == 0);
    } : ModuleTransferCallback());
  });
}



void PowerAmpInterface::InitModule(SuccessCallback&& callback) {
  this->initialised = false;

  this->reset_wait_timer = 0;

  //read module ID to check communication
  this->ReadRegister8Async(I2CDEF_POWERAMP_MODULE_ID, [&, callback = std::move(callback)](bool success, uint32_t value, uint16_t) {
    if (!success) {
      //report failure to external callback
      if (callback) {
        callback(false);
      }
      return;
    }

    //check correctness of module ID
    if ((uint8_t)value != I2CDEF_POWERAMP_MODULE_ID_VALUE) {
      DEBUG_PRINTF("* PowerAmp module ID incorrect: 0x%02X instead of 0x%02X\n", (uint8_t)value, I2CDEF_POWERAMP_MODULE_ID_VALUE);
      //report failure to external callback
      if (callback) {
        callback(false);
      }
      return;
    }

    //write interrupt mask (enable all interrupts)
    this->SetInterruptMask(0xFF, [&, callback = std::move(callback)](bool success) {
      if (!success) {
        //report failure to external callback
        if (callback) {
          callback(false);
        }
        return;
      }

      //set manual shutdown active initially (also enables interrupts)
      this->SetManualShutdownActive(true, [&, callback = std::move(callback)](bool success) {
        if (!success) {
          //report failure to external callback
          if (callback) {
            callback(false);
          }
          return;
        }

        //read all registers once to update registers to their initial values
        this->ReadMultiRegisterAsync(I2CDEF_POWERAMP_PVDD_TARGET, poweramp_scratch, 3, ModuleTransferCallback());
        this->ReadMultiRegisterAsync(I2CDEF_POWERAMP_MON_VRMS_FAST_A, poweramp_scratch, 32, ModuleTransferCallback());
        this->ReadMultiRegisterAsync(I2CDEF_POWERAMP_SERR_IRMS_INST_A, poweramp_scratch, 15, ModuleTransferCallback());
        this->ReadMultiRegisterAsync(I2CDEF_POWERAMP_SERR_PAVG_INST_A, poweramp_scratch, 15, ModuleTransferCallback());
        this->ReadMultiRegisterAsync(I2CDEF_POWERAMP_SERR_PAPP_INST_A, poweramp_scratch, 15, ModuleTransferCallback());
        this->ReadMultiRegisterAsync(I2CDEF_POWERAMP_SWARN_IRMS_INST_A, poweramp_scratch, 15, ModuleTransferCallback());
        this->ReadMultiRegisterAsync(I2CDEF_POWERAMP_SWARN_PAVG_INST_A, poweramp_scratch, 15, ModuleTransferCallback());
        this->ReadMultiRegisterAsync(I2CDEF_POWERAMP_SWARN_PAPP_INST_A, poweramp_scratch, 15, ModuleTransferCallback());
        this->ReadMultiRegisterAsync(I2CDEF_POWERAMP_SAFETY_STATUS, poweramp_scratch, 3, [&, callback = std::move(callback)](bool, uint32_t, uint16_t) {
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

void PowerAmpInterface::LoopTasks() {
  static uint32_t loop_count = 0;

  if (this->initialised) {
    if (loop_count % 50 == 0) {
      //every 50 cycles (500ms), read status and safety status - no callback needed, we're only reading to update the registers
      this->ReadRegister16Async(I2CDEF_POWERAMP_STATUS, ModuleTransferCallback());
      this->ReadMultiRegisterAsync(I2CDEF_POWERAMP_SAFETY_STATUS, poweramp_scratch, 3, ModuleTransferCallback());
    }

    if (loop_count % 100 == 10) {
      //every 100 cycles (1s), read PVDD information
      this->ReadMultiRegisterAsync(I2CDEF_POWERAMP_PVDD_TARGET, poweramp_scratch, 3, ModuleTransferCallback());

      //read output monitor values if enabled
      if (this->monitor_measurements) {
        this->ReadMultiRegisterAsync(I2CDEF_POWERAMP_MON_VRMS_FAST_A, poweramp_scratch, 32, [&](bool, uint32_t, uint16_t) {
          this->ExecuteCallbacks(MODIF_POWERAMP_EVENT_MEASUREMENT_UPDATE);
        });
      }
    }

    loop_count++;
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



PowerAmpInterface::PowerAmpInterface(I2CHardwareInterface& hw_interface, uint8_t i2c_address, GPIO_TypeDef* int_port, uint16_t int_pin) :
        IntRegI2CModuleInterface(hw_interface, i2c_address, I2CDEF_POWERAMP_REG_SIZES, int_port, int_pin, IF_POWERAMP_USE_CRC), monitor_measurements(false), initialised(false), reset_wait_timer(0) {}



void PowerAmpInterface::OnRegisterUpdate(uint8_t address) {
  //allow base handling
  this->IntRegI2CModuleInterface::OnRegisterUpdate(address);

  uint32_t event;

  //select corresponding event
  switch (address) {
    case I2CDEF_POWERAMP_STATUS:
      event = MODIF_POWERAMP_EVENT_STATUS_UPDATE;
      break;
    case I2CDEF_POWERAMP_SAFETY_STATUS:
    case I2CDEF_POWERAMP_SERR_SOURCE:
    case I2CDEF_POWERAMP_SWARN_SOURCE:
      event = MODIF_POWERAMP_EVENT_SAFETY_UPDATE;
      break;
    case I2CDEF_POWERAMP_PVDD_TARGET:
    case I2CDEF_POWERAMP_PVDD_REQ:
    case I2CDEF_POWERAMP_PVDD_MEASURED:
      event = MODIF_POWERAMP_EVENT_PVDD_UPDATE;
      break;
    default:
      return;
  }

  this->ExecuteCallbacks(event);
}

void PowerAmpInterface::OnI2CInterrupt(uint16_t interrupt_flags) {
  //allow base handling
  this->IntRegI2CModuleInterface::OnI2CInterrupt(interrupt_flags);

  if (interrupt_flags == MODIF_I2C_INT_RESET_FLAG) {
    //reset condition: only re-initialise if already initialised, or reset is pending
    if (this->initialised || this->reset_wait_timer > 0) {
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

  //all other interrupts indicate a status update, so read status
  this->ReadRegister16Async(I2CDEF_POWERAMP_STATUS, ModuleTransferCallback());

  if ((interrupt_flags & I2CDEF_POWERAMP_INT_FLAGS_INT_SERR_Msk) != 0) {
    //safety error: read safety status and error source
    this->ReadMultiRegisterAsync(I2CDEF_POWERAMP_SAFETY_STATUS, poweramp_scratch, 2, ModuleTransferCallback());
  }

  if ((interrupt_flags & I2CDEF_POWERAMP_INT_FLAGS_INT_SWARN_Msk) != 0) {
    //safety warning: read warning source
    this->ReadRegister16Async(I2CDEF_POWERAMP_SWARN_SOURCE, ModuleTransferCallback());
  }

  if ((interrupt_flags & (I2CDEF_POWERAMP_INT_FLAGS_INT_PVDD_ERR_Msk | I2CDEF_POWERAMP_INT_FLAGS_INT_PVDD_REDDONE_Msk | I2CDEF_POWERAMP_INT_FLAGS_INT_PVDD_OLIM_Msk)) != 0) {
    //PVDD event: read PVDD information
    this->ReadMultiRegisterAsync(I2CDEF_POWERAMP_PVDD_TARGET, poweramp_scratch, 3, ModuleTransferCallback());
  }
}

