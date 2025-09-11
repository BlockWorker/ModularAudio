/*
 * battery_interface.cpp
 *
 *  Created on: Aug 24, 2025
 *      Author: Alex
 */


#include "battery_interface.h"
#include "system.h"


//scratch space for register discard-reads
static uint8_t bms_scratch[32];


bool BatteryInterface::IsBatteryPresent() const {
  return this->initialised && this->present;
}


BatteryStatus BatteryInterface::GetStatus() const {
  BatteryStatus status;
  status.value = this->registers.Reg8(UARTDEF_BMS_STATUS);
  return status;
}


uint16_t BatteryInterface::GetStackVoltageMV() const {
  return this->registers.Reg16(UARTDEF_BMS_STACK_VOLTAGE);
}


int16_t BatteryInterface::GetCellVoltageMV(uint8_t cell) const {
  if (cell > 4) {
    throw std::invalid_argument("BatteryInterface GetCellVoltageMV given invalid cell, must be 0-4");
  }

  return this->GetCellVoltagesMV()[cell];
}

const int16_t* BatteryInterface::GetCellVoltagesMV() const {
  return (const int16_t*)this->registers[UARTDEF_BMS_CELL_VOLTAGES];
}


int32_t BatteryInterface::GetCurrentMA() const {
  return (int32_t)this->registers.Reg32(UARTDEF_BMS_CURRENT);
}


BatterySoCLevel BatteryInterface::GetSoCConfidenceLevel() const {
  //use lowest level of the two reported ones (should be the same really, but just in case)
  uint8_t fract_level = this->registers[UARTDEF_BMS_SOC_FRACTION][4];
  uint8_t energy_level = this->registers[UARTDEF_BMS_SOC_ENERGY][4];
  uint8_t min_level = MIN(fract_level, energy_level);

  if (min_level > UARTDEF_BMS_SOC_CONFIDENCE_MEASURED_REF) {
    //invalid level - return invalid by default
    return IF_BMS_SOCLVL_INVALID;
  } else {
    return (BatterySoCLevel)min_level;
  }
}

float BatteryInterface::GetSoCFraction() const {
  uint32_t int_val = *(uint32_t*)this->registers[UARTDEF_BMS_SOC_FRACTION];
  return *(float*)&int_val;
}

float BatteryInterface::GetSoCEnergyWh() const {
  uint32_t int_val = *(uint32_t*)this->registers[UARTDEF_BMS_SOC_ENERGY];
  return *(float*)&int_val;
}


float BatteryInterface::GetBatteryHealth() const {
  uint32_t int_val = this->registers.Reg32(UARTDEF_BMS_HEALTH);
  return *(float*)&int_val;
}


int16_t BatteryInterface::GetBatteryTemperatureC() const {
  return (int16_t)this->registers.Reg16(UARTDEF_BMS_BAT_TEMP);
}

int16_t BatteryInterface::GetBMSTemperatureC() const {
  return (int16_t)this->registers.Reg16(UARTDEF_BMS_INT_TEMP);
}


BatterySafetyStatus BatteryInterface::GetSafetyAlerts() const {
  BatterySafetyStatus status;
  status.value = this->registers.Reg16(UARTDEF_BMS_ALERTS);
  return status;
}

BatterySafetyStatus BatteryInterface::GetSafetyFaults() const {
  BatterySafetyStatus status;
  status.value = this->registers.Reg16(UARTDEF_BMS_FAULTS);
  return status;
}


BatteryShutdownType BatteryInterface::GetScheduledShutdown() const {
  return (BatteryShutdownType)*this->registers[UARTDEF_BMS_SHUTDOWN];
}

uint16_t BatteryInterface::GetShutdownCountdownMS() const {
  return *(uint16_t*)(this->registers[UARTDEF_BMS_SHUTDOWN] + 1);
}


uint8_t BatteryInterface::GetSeriesCellCount() const {
  return this->registers.Reg8(UARTDEF_BMS_CELLS_SERIES);
}

uint8_t BatteryInterface::GetParallelCellCount() const {
  return this->registers.Reg8(UARTDEF_BMS_CELLS_PARALLEL);
}

uint8_t BatteryInterface::GetTotalCellCount() const {
  return this->GetSeriesCellCount() * this->GetParallelCellCount();
}


uint16_t BatteryInterface::GetMinCellVoltageMV() const {
  return this->registers.Reg16(UARTDEF_BMS_MIN_VOLTAGE);
}

uint16_t BatteryInterface::GetMaxCellVoltageMV() const {
  return this->registers.Reg16(UARTDEF_BMS_MAX_VOLTAGE);
}


uint32_t BatteryInterface::GetMaxDischargeCurrentMA() const {
  return this->registers.Reg32(UARTDEF_BMS_MAX_DSG_CURRENT);
}

uint32_t BatteryInterface::GetPeakDischargeCurrentMA() const {
  return this->registers.Reg32(UARTDEF_BMS_PEAK_DSG_CURRENT);
}

uint32_t BatteryInterface::GetMaxChargeCurrentMA() const {
  return this->registers.Reg32(UARTDEF_BMS_MAX_CHG_CURRENT);
}



void BatteryInterface::SetBatteryHealth(float health, SuccessCallback&& callback) {
  if (isnanf(health) || health < 0.1f || health > 1.0f) {
    throw std::invalid_argument("BatteryInterface SetBatteryHealth given invalid health, must be in [0.1, 1]");
  }

  this->WriteRegister32Async(UARTDEF_BMS_HEALTH, *(uint32_t*)&health, SuccessToTransferCallback(callback));
}


//disabled because not needed, and shouldn't be used anyway probably
/*void BatteryInterface::ResetModule(SuccessCallback&& callback) {
  //store reset callback and start timer
  this->reset_callback = std::move(callback);
  this->reset_wait_timer = IF_BMS_RESET_TIMEOUT;

  //write reset without callback, since we're not getting an acknowledgement for this write
  this->WriteRegister8Async(UARTDEF_BMS_CONTROL, UARTDEF_BMS_CONTROL_RESET_VALUE << UARTDEF_BMS_CONTROL_RESET_Pos, ModuleTransferCallback());
}*/


void BatteryInterface::ClearChargingFaults(SuccessCallback&& callback) {
  uint8_t control_reg = this->registers.Reg8(UARTDEF_BMS_CONTROL);

  //write existing control register state, with clear charge fault bit set
  this->WriteRegister8Async(UARTDEF_BMS_CONTROL, control_reg | UARTDEF_BMS_CONTROL_CLEAR_CHG_FAULT_Msk, SuccessToTransferCallback(callback));
}


void BatteryInterface::SetShutdownRequest(bool shutdown, SuccessCallback&& callback) {
  //write corresponding control register state
  this->WriteRegister8Async(UARTDEF_BMS_CONTROL, shutdown ? UARTDEF_BMS_CONTROL_REQ_SHUTDOWN_Msk : 0, SuccessToTransferCallback(callback));
}

void BatteryInterface::TriggerFullShutdown(SuccessCallback&& callback) {
  //write corresponding control register state
  this->WriteRegister8Async(UARTDEF_BMS_CONTROL, UARTDEF_BMS_CONTROL_FULL_SHUTDOWN_Msk, SuccessToTransferCallback(callback));
}



void BatteryInterface::InitModule(SuccessCallback&& callback) {
  this->initialised = false;
  this->present = false;

  this->reset_wait_timer = 0;

  //read module ID to check communication
  this->ReadRegister8Async(UARTDEF_BMS_MODULE_ID, [this, callback = std::move(callback)](bool success, uint32_t value, uint16_t) {
    if (!success) {
      //report failure to external callback, init can be regarded as complete with non-present battery
      this->initialised = true;
      if (callback) {
        callback(false);
      }
      return;
    }

    //check correctness of module ID
    if ((uint8_t)value != UARTDEF_BMS_MODULE_ID_VALUE) {
      DEBUG_LOG(DEBUG_ERROR, "Battery module ID incorrect: 0x%02X instead of 0x%02X", (uint8_t)value, UARTDEF_BMS_MODULE_ID_VALUE);
      //report failure to external callback, init can be regarded as complete with non-present battery
      this->initialised = true;
      if (callback) {
        callback(false);
      }
      return;
    }

    //write notification mask (enable all notifications)
    this->WriteRegister16Async(UARTDEF_BMS_NOTIF_MASK, 0xFFF, [this, callback = std::move(callback)](bool success, uint32_t, uint16_t) {
      if (!success) {
        //report failure to external callback, init can be regarded as complete with non-present battery
        this->initialised = true;
        if (callback) {
          callback(false);
        }
        return;
      }

      //read constant registers, making sure they're all successfully received
      this->ReadRegister8Async(UARTDEF_BMS_CELLS_SERIES, [this, callback = std::move(callback)](bool success, uint32_t, uint16_t) {
        if (!success) {
          //report failure to external callback, init can be regarded as complete with non-present battery
          this->initialised = true;
          if (callback) {
            callback(false);
          }
          return;
        }

        this->ReadRegister8Async(UARTDEF_BMS_CELLS_PARALLEL, [this, callback = std::move(callback)](bool success, uint32_t, uint16_t) {
          if (!success) {
            //report failure to external callback, init can be regarded as complete with non-present battery
            this->initialised = true;
            if (callback) {
              callback(false);
            }
            return;
          }

          this->ReadRegister16Async(UARTDEF_BMS_MIN_VOLTAGE, [this, callback = std::move(callback)](bool success, uint32_t, uint16_t) {
            if (!success) {
              //report failure to external callback, init can be regarded as complete with non-present battery
              this->initialised = true;
              if (callback) {
                callback(false);
              }
              return;
            }

            this->ReadRegister16Async(UARTDEF_BMS_MAX_VOLTAGE, [this, callback = std::move(callback)](bool success, uint32_t, uint16_t) {
              if (!success) {
                //report failure to external callback, init can be regarded as complete with non-present battery
                this->initialised = true;
                if (callback) {
                  callback(false);
                }
                return;
              }

              this->ReadRegister32Async(UARTDEF_BMS_MAX_DSG_CURRENT, [this, callback = std::move(callback)](bool success, uint32_t, uint16_t) {
                if (!success) {
                  //report failure to external callback, init can be regarded as complete with non-present battery
                  this->initialised = true;
                  if (callback) {
                    callback(false);
                  }
                  return;
                }

                this->ReadRegister32Async(UARTDEF_BMS_PEAK_DSG_CURRENT, [this, callback = std::move(callback)](bool success, uint32_t, uint16_t) {
                  if (!success) {
                    //report failure to external callback, init can be regarded as complete with non-present battery
                    this->initialised = true;
                    if (callback) {
                      callback(false);
                    }
                    return;
                  }

                  this->ReadRegister32Async(UARTDEF_BMS_MAX_CHG_CURRENT, [this, callback = std::move(callback)](bool success, uint32_t, uint16_t) {
                    if (!success) {
                      //report failure to external callback, init can be regarded as complete with non-present battery
                      this->initialised = true;
                      if (callback) {
                        callback(false);
                      }
                      return;
                    }

                    //read all normal registers once to update registers to their initial values
                    this->ReadRegister8Async(UARTDEF_BMS_STATUS, ModuleTransferCallback());
                    this->ReadRegister16Async(UARTDEF_BMS_STACK_VOLTAGE, ModuleTransferCallback());
                    this->ReadRegisterAsync(UARTDEF_BMS_CELL_VOLTAGES, bms_scratch, ModuleTransferCallback());
                    this->ReadRegister32Async(UARTDEF_BMS_CURRENT, ModuleTransferCallback());
                    this->ReadRegisterAsync(UARTDEF_BMS_SOC_FRACTION, bms_scratch, ModuleTransferCallback());
                    this->ReadRegisterAsync(UARTDEF_BMS_SOC_ENERGY, bms_scratch, ModuleTransferCallback());
                    this->ReadRegister32Async(UARTDEF_BMS_HEALTH, ModuleTransferCallback());
                    this->ReadRegister16Async(UARTDEF_BMS_BAT_TEMP, ModuleTransferCallback());
                    this->ReadRegister16Async(UARTDEF_BMS_INT_TEMP, ModuleTransferCallback());
                    this->ReadRegister16Async(UARTDEF_BMS_ALERTS, ModuleTransferCallback());
                    this->ReadRegister16Async(UARTDEF_BMS_FAULTS, ModuleTransferCallback());
                    this->ReadRegisterAsync(UARTDEF_BMS_SHUTDOWN, bms_scratch, ModuleTransferCallback());
                    this->ReadRegister8Async(UARTDEF_BMS_CONTROL, [this, callback = std::move(callback)](bool, uint32_t, uint16_t) {
                      //after last read is done: init completed successfully (even if read failed - that's non-critical)
                      this->initialised = true;
                      this->present = true;
                      if (callback) {
                        callback(true);
                      }
                    });
                  });
                });
              });
            });
          });
        });
      });
    });
  });
}

void BatteryInterface::LoopTasks() {
  static uint32_t loop_count = 0;

  if (this->initialised) {
    if (this->present) {
      if (loop_count % 50 == 0) {
        //every 50 cycles (500ms) when present, read status, to update register and make sure the battery is still present
        this->ReadRegister8Async(UARTDEF_BMS_STATUS, [this](bool success, uint32_t, uint16_t) {
          if (!success) {
            //read failed: mark battery as not present, execute callbacks
            this->present = false;
            this->ExecuteCallbacks(MODIF_BMS_EVENT_PRESENCE_UPDATE);
          } else {
            //read successful: check for inconsistencies between status bits and other registers
            BatteryStatus status = this->GetStatus();
            if (status.shutdown != (this->GetScheduledShutdown() != IF_BMS_SHDN_NONE)) {
              //shutdown inconsistency detected: re-read shutdown information register
              this->ReadRegisterAsync(UARTDEF_BMS_SHUTDOWN, bms_scratch, ModuleTransferCallback());
            }
            if (status.alert != (this->GetSafetyAlerts().value != 0)) {
              //alert inconsistency detected: re-read alert information register
              this->ReadRegister16Async(UARTDEF_BMS_ALERTS, ModuleTransferCallback());
            }
            if (status.fault != (this->GetSafetyFaults().value != 0)) {
              //fault inconsistency detected: re-read fault information register
              this->ReadRegister16Async(UARTDEF_BMS_FAULTS, ModuleTransferCallback());
            }
          }
        });
      }
    } else {
      if (loop_count % 100 == 0) {
        //every 100 cycles (1s) when not present, attempt to re-initialise, to see if the battery is present now
        this->InitModule([this](bool success) {
          if (success) {
            //successful init: battery is now present, execute callbacks
            this->ExecuteCallbacks(MODIF_BMS_EVENT_PRESENCE_UPDATE);
          }
        });
      }
    }
    loop_count++;
  }

  //allow base handling
  this->RegUARTModuleInterface::LoopTasks();

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



BatteryInterface::BatteryInterface(UART_HandleTypeDef* uart_handle) :
    RegUARTModuleInterface(uart_handle, UARTDEF_BMS_REG_SIZES, IF_BMS_USE_CRC), initialised(false), present(false), reset_wait_timer(0) {}


void BatteryInterface::HandleNotificationData(bool error, bool unsolicited) {
  //allow base handling
  this->RegUARTModuleInterface::HandleNotificationData(error, unsolicited);

  if (error) {
    //notifications with errors should be disregarded
    return;
  }

  //check notification type
  switch ((ModuleInterfaceUARTNotificationType)this->parse_buffer[0]) {
    case IF_UART_TYPE_EVENT:
      switch ((ModuleInterfaceUARTEventNotifType)this->parse_buffer[1]) {
        case IF_UART_EVENT_MCU_RESET:
          //only re-initialise if already initialised, or reset is pending
          if (this->initialised || this->reset_wait_timer > 0) {
            if (this->reset_wait_timer == 0) {
              DEBUG_LOG(DEBUG_WARNING, "BMS module spurious reset detected");
            }
            //perform module re-init
            this->InitModule([this](bool success) {
              //notify system of reset, then call reset callback
              this->ExecuteCallbacks(MODIF_EVENT_MODULE_RESET);
              if (this->reset_callback) {
                this->reset_callback(success);
                //clear reset callback
                this->reset_callback = SuccessCallback();
              }
            });
          }
          break;
        default:
          break;
      }
      break;
    default:
      //other types already handled as desired
      return;
  }
}

void BatteryInterface::OnRegisterUpdate(uint8_t address) {
  //allow base handling
  this->RegUARTModuleInterface::OnRegisterUpdate(address);

  if (!this->initialised || !this->present) {
    return;
  }

  uint32_t event;

  //select corresponding event
  switch (address) {
    case UARTDEF_BMS_STATUS:
      event = MODIF_BMS_EVENT_STATUS_UPDATE;
      break;
    case UARTDEF_BMS_STACK_VOLTAGE:
    case UARTDEF_BMS_CELL_VOLTAGES:
      event = MODIF_BMS_EVENT_VOLTAGE_UPDATE;
      break;
    case UARTDEF_BMS_CURRENT:
      event = MODIF_BMS_EVENT_CURRENT_UPDATE;
      break;
    case UARTDEF_BMS_SOC_FRACTION:
    case UARTDEF_BMS_SOC_ENERGY:
      event = MODIF_BMS_EVENT_SOC_UPDATE;
      break;
    case UARTDEF_BMS_HEALTH:
      event = MODIF_BMS_EVENT_HEALTH_UPDATE;
      break;
    case UARTDEF_BMS_BAT_TEMP:
    case UARTDEF_BMS_INT_TEMP:
      event = MODIF_BMS_EVENT_TEMP_UPDATE;
      break;
    case UARTDEF_BMS_ALERTS:
    case UARTDEF_BMS_FAULTS:
      event = MODIF_BMS_EVENT_SAFETY_UPDATE;
      break;
    case UARTDEF_BMS_SHUTDOWN:
      event = MODIF_BMS_EVENT_SHUTDOWN_UPDATE;
      break;
    default:
      return;
  }

  this->ExecuteCallbacks(event);
}


