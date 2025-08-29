/*
 * power_manager.cpp
 *
 *  Created on: Aug 28, 2025
 *      Author: Alex
 */

#include "power_manager.h"
#include "system.h"
#include "i2c_defines_charger.h"


/******************************************************/
/*       Constant definitions and configuration       */
/******************************************************/

//non-volatile storage layout
//max adapter current in A, float (4B)
#define PWR_NVM_MAX_ADAPTER_CURRENT 0
//charge target voltage state, enum (1B)
#define PWR_NVM_CHARGE_TARGET_STATE 4
//charge current target, unsigned mA (2B)
#define PWR_NVM_CHARGE_CURRENT 6
//auto-shutdown delay, unsigned milliseconds (4B)
#define PWR_NVM_ASD_DELAY 8
//total storage size in bytes
#define PWR_NVM_TOTAL_BYTES 12

//config default values
#define PWR_ADAPTER_CURRENT_A_DEFAULT IF_CHG_SAFE_ADAPTER_CURRENT_A
#define PWR_CHG_TARGET_STATE_DEFAULT PWR_CHG_TARGET_80
#define PWR_CHG_CURRENT_MA_DEFAULT 4992u
#define PWR_ASD_DELAY_MS_DEFAULT 600000u

//charging configuration
//re-write period, in main loop cycles (to keep charger watchdog happy)
#define PWR_CHG_REWRITE_CYCLES (60000 / MAIN_LOOP_PERIOD_MS)
//minimum voltage difference to target to allow charging to start, in volts
#define PWR_CHG_START_MIN_VOLTAGE_DIFF_V 0.3f
//maximum voltage difference to target to allow charging to end (assuming current condition is also met), in volts
#define PWR_CHG_END_MAX_VOLTAGE_DIFF_V 0.1f
//maximum charging current below which charging is allowed to end (assuming voltage condition is also met), in amps
#define PWR_CHG_END_MAX_CURRENT_A 0.25f
//time for which the voltage and current conditions above need to be fulfilled to allow charging to end, in main loop cycles
#define PWR_CHG_END_STABLE_CYCLES (10000 / MAIN_LOOP_PERIOD_MS)

//charge status LED PWM timer and channels
#define PWR_CHG_LED_TIMER htim2
#define PWR_CHG_LED_CHARGING_CH TIM_CHANNEL_1
#define PWR_CHG_LED_IDLE_CH TIM_CHANNEL_2
#define PWR_CHG_LED_CHARGING_REG PWR_CHG_LED_TIMER.Instance->CCR1
#define PWR_CHG_LED_IDLE_REG PWR_CHG_LED_TIMER.Instance->CCR2


static_assert(PWR_ADAPTER_CURRENT_A_DEFAULT >= PWR_ADAPTER_CURRENT_A_MIN && PWR_ADAPTER_CURRENT_A_DEFAULT <= PWR_ADAPTER_CURRENT_A_MAX);
static_assert(PWR_CHG_TARGET_STATE_DEFAULT < _PWR_CHG_TARGET_COUNT);
static_assert(PWR_CHG_CURRENT_MA_DEFAULT >= 1000 * PWR_CHG_CURRENT_A_MIN && PWR_CHG_CURRENT_MA_DEFAULT <= 1000 * PWR_CHG_CURRENT_A_MAX && (PWR_CHG_CURRENT_MA_DEFAULT & ~I2CDEF_CHG_CHG_CURRENT_MASK) == 0);
static_assert(PWR_ASD_DELAY_MS_DEFAULT >= PWR_ASD_DELAY_MS_MIN && PWR_ASD_DELAY_MS_DEFAULT <= PWR_ASD_DELAY_MS_MAX);


//charging target voltage per cell, for each target state
static const float _power_charge_cell_voltage_targets[_PWR_CHG_TARGET_COUNT] = {
  3.851f, //70%
  3.890f, //75%
  3.934f, //80%
  3.987f, //85%
  4.035f, //90%
  4.081f, //95%
  4.200f //100%
};


/******************************************************/
/*                   Initialisation                   */
/******************************************************/

PowerManager::PowerManager(BlockBoxV2System& system) :
    system(system), non_volatile_config(system.eeprom_if, PWR_NVM_TOTAL_BYTES, PowerManager::LoadNonVolatileConfigDefaults), initialised(false), lock_timer(0), charging_active(false),
    charging_end_condition_cycles(0), auto_shutdown_last_reset(0) {}


void PowerManager::Init(SuccessCallback&& callback) {
  this->initialised = false;
  this->lock_timer = 0;
  this->charging_active = false;
  this->charging_end_condition_cycles = 0;
  this->auto_shutdown_last_reset = HAL_GetTick();

  //set up status LED timer
  if (HAL_TIM_Base_Start(&PWR_CHG_LED_TIMER) != HAL_OK) {
    if (callback) {
      callback(false);
    }
    return;
  }
  if (HAL_TIM_PWM_Start(&PWR_CHG_LED_TIMER, PWR_CHG_LED_CHARGING_CH) != HAL_OK) {
    if (callback) {
      callback(false);
    }
    return;
  }
  if (HAL_TIM_PWM_Start(&PWR_CHG_LED_TIMER, PWR_CHG_LED_IDLE_CH) != HAL_OK) {
    if (callback) {
      callback(false);
    }
    return;
  }
  //LEDs off initially
  PWR_CHG_LED_CHARGING_REG = 0;
  PWR_CHG_LED_IDLE_REG = 0;

  if (this->system.chg_if.IsAdapterPresent()) {
    //adapter present: explicitly reset and disable charger
    this->system.chg_if.SetChargeEndVoltageMV(0, [this, callback = std::move(callback)](bool success) {
      if (!success) {
        //propagate failure to external callback
        DEBUG_PRINTF("* PowerManager Init failed to clear charger voltage\n");
        if (callback) {
          callback(false);
        }
        return;
      }

      this->system.chg_if.SetFastChargeCurrentMA(0, [this, callback = std::move(callback)](bool success) {
        if (success) {
          /*this->system.chg_if.RegisterCallback(std::bind(&PowerManager::HandleEvent, this, std::placeholders::_1, std::placeholders::_2),
                                               MODIF_CHG_EVENT_PRESENCE_UPDATE);*/

          //init done
          this->initialised = true;
        } else {
          DEBUG_PRINTF("* PowerManager Init failed to clear charger current\n");
        }

        //propagate success to external callback
        if (callback) {
          callback(success);
        }
      });
    });
  } else {
    //adapter not present: init done as-is
    this->initialised = true;
    if (callback) {
      callback(true);
    }
  }
}


/******************************************************/
/*           Basic Tasks and Event Handling           */
/******************************************************/

void PowerManager::LoopTasks() {
  static uint32_t charge_refresh_loop_count = 0;

  if (!this->initialised) {
    return;
  }

  //decrement lock timer, under disabled interrupts
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  if (this->lock_timer > 0) {
    if (--this->lock_timer == 0) {
      //shouldn't really ever happen, it means an unlock somewhere was missed or delayed excessively
      DEBUG_PRINTF("* PowerManager lock timed out!\n");
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

  bool adapter_present = this->system.chg_if.IsAdapterPresent();

  //handle adapter logic, if not locked out
  primask = __get_PRIMASK();
  __disable_irq();
  if (this->lock_timer == 0) {
      //lock out
      this->lock_timer = PWR_LOCK_TIMEOUT_CYCLES;
      __set_PRIMASK(primask);

      //ensure adapter is configured for correct max input current if present
      float max_current = this->GetAdapterMaxCurrentA();
      if (adapter_present && this->system.chg_if.GetMaxAdapterCurrentA() != max_current) {
        this->system.chg_if.SetMaxAdapterCurrentA(max_current, [this](bool success) {
          if (!success) {
            DEBUG_PRINTF("* PowerManager failed to write max adapter current after detecting setting difference\n");
          } else {
            float actual_max_current = this->system.chg_if.GetMaxAdapterCurrentA();
            if (actual_max_current != this->GetAdapterMaxCurrentA()) {
              //if true value is different from current config: overwrite NVM value
              this->non_volatile_config.SetValue32(PWR_NVM_MAX_ADAPTER_CURRENT, *(uint32_t*)&actual_max_current);
            }
            DEBUG_PRINTF("PowerManager wrote max adapter current after detecting setting difference: %.3f\n", actual_max_current); //TODO: temp printout
          }
          this->lock_timer = 0;
        });
      } else {
        this->lock_timer = 0;
      }
  } else {
    __set_PRIMASK(primask);
  }

  //handle charging logic, if not locked out
  primask = __get_PRIMASK();
  __disable_irq();
  if (this->lock_timer == 0) {
    //lock out
    this->lock_timer = PWR_LOCK_TIMEOUT_CYCLES;
    __set_PRIMASK(primask);

    if (this->charging_active) {
      //active: check if we need to stop instantly
      if (!adapter_present) {
        //adapter disconnected: just mark as inactive
        this->charging_active = false;
        this->lock_timer = 0;
      } else if (!this->system.bat_if.IsBatteryPresent() || this->GetChargingTargetCurrentMA() < 128) {
        //battery disconnected or current target below minimum: mark as inactive and explicitly disable charger
        this->charging_active = false;
        this->system.chg_if.SetFastChargeCurrentMA(0, [this](bool success) {
          if (!success) {
            DEBUG_PRINTF("* PowerManager failed to disable charger on battery removal or manual disable\n");
          }
          this->lock_timer = 0;
        });
      } else {
        //no instant stop required: check normal charging end conditions
        //difference between voltage target and stack voltage (positive difference means battery below target)
        float voltage_difference_V = ((float)this->system.chg_if.GetChargeEndVoltageMV() - (float)this->system.bat_if.GetStackVoltageMV()) / 1000.0f;
        //charging current (positive battery current means charging)
        float current_A = (float)this->system.bat_if.GetCurrentMA() / 1000.0f;
        if (voltage_difference_V < PWR_CHG_END_MAX_VOLTAGE_DIFF_V && current_A < PWR_CHG_END_MAX_CURRENT_A) {
          //end conditions met: increment counter
          if (++this->charging_end_condition_cycles >= PWR_CHG_END_STABLE_CYCLES) {
            //end conditions stable for long enough: stop charging
            this->system.chg_if.SetFastChargeCurrentMA(0, [this](bool success) {
              if (!success) {
                DEBUG_PRINTF("* PowerManager failed to disable charger upon reaching end condition\n");
              } else {
                DEBUG_PRINTF("PowerManager stopped charging due to end condition\n"); //TODO temp printout
                this->charging_active = false;
              }
              this->lock_timer = 0;
            });
          }
        } else {
          //end conditions not met: reset counter
          this->charging_end_condition_cycles = 0;

          if (++charge_refresh_loop_count % PWR_CHG_REWRITE_CYCLES == 0) {
            //periodically rewrite charging current if we're still charging
            this->system.chg_if.SetFastChargeCurrentMA(this->GetChargingTargetCurrentMA(), [this](bool success) {
              if (!success) {
                DEBUG_PRINTF("* PowerManager failed to refresh charging current\n");
              } else {
                DEBUG_PRINTF("PowerManager refreshed charging current: %u\n", this->GetChargingTargetCurrentMA()); //TODO temp printout
              }
            });
          }

          this->lock_timer = 0;
        }
      }
    } else {
      //inactive: check if starting is physically possible (adapter and battery present, current above minimum)
      if (adapter_present && this->system.bat_if.IsBatteryPresent() && this->GetChargingTargetCurrentMA() >= 128) {
        //check charge starting condition
        //difference between voltage target and stack voltage (positive difference means battery below target)
        uint16_t target_voltage_mV = this->GetTargetStateVoltageMV(this->GetChargingTargetState());
        float voltage_difference_V = ((float)target_voltage_mV - (float)this->system.bat_if.GetStackVoltageMV()) / 1000.0f;
        if (voltage_difference_V > PWR_CHG_START_MIN_VOLTAGE_DIFF_V) {
          //starting condition met: begin charging
          //write voltage target
          this->system.chg_if.SetChargeEndVoltageMV(target_voltage_mV, [this, target_voltage_mV](bool success) {
            if (!success) {
              DEBUG_PRINTF("* PowerManager failed to write voltage on charge start\n");
              this->lock_timer = 0;
            } else {
              DEBUG_PRINTF("PowerManager wrote voltage on charge start: %u\n", target_voltage_mV); //TODO: temp printout
              //write charge current
              this->system.chg_if.SetFastChargeCurrentMA(this->GetChargingTargetCurrentMA(), [this](bool success) {
                if (!success) {
                  DEBUG_PRINTF("* PowerManager failed to write current on charge start\n");
                } else {
                  DEBUG_PRINTF("PowerManager wrote current on charge start: %u\n", this->GetChargingTargetCurrentMA()); //TODO: temp printout
                  //mark as charging
                  charge_refresh_loop_count = 0;
                  this->charging_end_condition_cycles = 0;
                  this->charging_active = true;
                }
                this->lock_timer = 0;
              });
            }
          });
        } else {
          this->lock_timer = 0;
        }
      } else {
        this->lock_timer = 0;
      }
    }
  } else {
    __set_PRIMASK(primask);
  }

  //update charging LEDs
  if (adapter_present) {
    if (this->charging_active) {
      //charging: charging LED pulsing, idle LED off
      float brightness_wave = cosf(2.0f * M_PI * (float)(HAL_GetTick() % 5000) / 5000.0f);
      PWR_CHG_LED_CHARGING_REG = 0x8FFF + (int32_t)roundf(brightness_wave * (float)0x7000);
      PWR_CHG_LED_IDLE_REG = 0;
    } else {
      //not charging: charging LED off, idle LED on
      PWR_CHG_LED_CHARGING_REG = 0;
      PWR_CHG_LED_IDLE_REG = 0xFFFF;
    }
  } else {
    //adapter not present: both LEDs off
    PWR_CHG_LED_CHARGING_REG = 0;
    PWR_CHG_LED_IDLE_REG = 0;
  }

  //TODO auto-shutdown handling
}


/*void PowerManager::HandlePowerStateChange(bool on, SuccessCallback&& callback) {

}*/


void PowerManager::HandleEvent(EventSource* source, uint32_t event) {
  //TODO see what's needed, if anything
}


void PowerManager::LoadNonVolatileConfigDefaults(StorageSection& section) {
  //write default maximum adapter current
  float default_adapter_current = PWR_ADAPTER_CURRENT_A_DEFAULT;
  section.SetValue32(PWR_NVM_MAX_ADAPTER_CURRENT, *(uint32_t*)&default_adapter_current);

  //write default charge target state
  section.SetValue8(PWR_NVM_CHARGE_TARGET_STATE, (uint8_t)PWR_CHG_TARGET_STATE_DEFAULT);

  //write default charge current
  section.SetValue16(PWR_NVM_CHARGE_CURRENT, PWR_CHG_CURRENT_MA_DEFAULT);

  //write default auto-shutdown delay
  section.SetValue32(PWR_NVM_ASD_DELAY, PWR_ASD_DELAY_MS_DEFAULT);
}


/******************************************************/
/*                 Charging Handling                  */
/******************************************************/

float PowerManager::GetAdapterMaxCurrentA() const {
  uint32_t int_val = this->non_volatile_config.GetValue32(PWR_NVM_MAX_ADAPTER_CURRENT);
  return *(float*)&int_val;
}


void PowerManager::SetAdapterMaxCurrentA(float current_A, SuccessCallback&& callback, bool queue_if_busy) {
  if (isnanf(current_A) || current_A < PWR_ADAPTER_CURRENT_A_MIN || current_A > PWR_ADAPTER_CURRENT_A_MAX) {
    throw std::invalid_argument("PowerManager SetAdapterMaxCurrentA given invalid current, must be in [0.32, 20.16]");
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
      this->queued_operations.push_back([this, current_A, callback = std::move(callback)]() mutable {
        this->SetAdapterMaxCurrentA(current_A, std::move(callback), false);
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
  this->lock_timer = PWR_LOCK_TIMEOUT_CYCLES;
  __set_PRIMASK(primask);

  if (this->GetAdapterMaxCurrentA() == current_A) {
    //already in desired state: unlock operations and propagate success to external callback
    this->lock_timer = 0;
    if (callback) {
      callback(true);
    }
  } else {
    //apply new state
    if (this->system.chg_if.IsAdapterPresent()) {
      //adapter present: apply to the charger directly
      this->system.chg_if.SetMaxAdapterCurrentA(current_A, [this, callback = std::move(callback)](bool success) {
        if (!success) {
          DEBUG_PRINTF("* PowerManager SetAdapterMaxCurrentA failed to write current\n");
        } else {
          //save new state on success
          float actual_max_current = this->system.chg_if.GetMaxAdapterCurrentA();
          this->non_volatile_config.SetValue32(PWR_NVM_MAX_ADAPTER_CURRENT, *(uint32_t*)&actual_max_current);
          DEBUG_PRINTF("PowerManager SetAdapterMaxCurrentA wrote current: %.3f\n", actual_max_current); //TODO: temp printout
        }

        //unlock and propagate success to external callback
        this->lock_timer = 0;
        if (callback) {
          callback(success);
        }
      });
    } else {
      //adapter not present: just save and unlock
      this->non_volatile_config.SetValue32(PWR_NVM_MAX_ADAPTER_CURRENT, *(uint32_t*)&current_A);
      this->lock_timer = 0;
      if (callback) {
        callback(true);
      }
    }
  }
}


/******************************************************/
/*                 Charging Handling                  */
/******************************************************/

bool PowerManager::IsCharging() const {
  return this->initialised && this->charging_active;
}

PowerChargingTargetState PowerManager::GetChargingTargetState() const {
  return (PowerChargingTargetState)this->non_volatile_config.GetValue8(PWR_NVM_CHARGE_TARGET_STATE);
}

uint16_t PowerManager::GetChargingTargetCurrentMA() const {
  return this->non_volatile_config.GetValue16(PWR_NVM_CHARGE_CURRENT);
}

float PowerManager::GetChargingTargetCurrentA() const {
  return (float)this->GetChargingTargetCurrentMA() / 1000.0f;
}


uint16_t PowerManager::GetTargetStateVoltageMV(PowerChargingTargetState target) {
  if (target >= _PWR_CHG_TARGET_COUNT) {
    throw std::invalid_argument("PowerManager GetTargetStateVoltageMV given invalid target state");
  }

  uint8_t series_cells = this->system.bat_if.GetSeriesCellCount();

  //get target voltage for full pack based on the state and cell configuration
  float target_voltage = _power_charge_cell_voltage_targets[target] * (float)series_cells;
  //convert to whole millivolts
  uint16_t target_voltage_mV = (uint16_t)floorf(target_voltage * 1000.0f);
  //get maximum target voltage for full pack, as reported by battery - ensuring it's at least 4.1V per cell
  uint16_t max_voltage_mV = MAX(this->system.bat_if.GetMaxCellVoltageMV(), 4100) * series_cells;
  //clamp target voltage to ensure it's below maximum
  if (target_voltage_mV > max_voltage_mV) {
    target_voltage_mV = max_voltage_mV;
  }
  //round down to the nearest valid step
  return target_voltage_mV & I2CDEF_CHG_CHG_VOLTAGE_MASK;
}

void PowerManager::SetChargingTargetState(PowerChargingTargetState target, SuccessCallback&& callback, bool queue_if_busy) {
  if (target >= _PWR_CHG_TARGET_COUNT) {
    throw std::invalid_argument("PowerManager SetChargingTargetState given invalid target state");
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
      this->queued_operations.push_back([this, target, callback = std::move(callback)]() mutable {
        this->SetChargingTargetState(target, std::move(callback), false);
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
  this->lock_timer = PWR_LOCK_TIMEOUT_CYCLES;
  __set_PRIMASK(primask);

  if (this->GetChargingTargetState() == target) {
    //already in desired state: unlock operations and propagate success to external callback
    this->lock_timer = 0;
    if (callback) {
      callback(true);
    }
  } else {
    //apply new state
    if (this->system.chg_if.IsAdapterPresent() && this->system.bat_if.IsBatteryPresent()) {
      //adapter and battery present: apply to the charger directly
      uint16_t target_voltage_mV = this->GetTargetStateVoltageMV(target);
      if (target_voltage_mV < 10000) {
        //shouldn't be possible - abort
        DEBUG_PRINTF("* PowerManager SetChargingTargetState encountered invalid internal target voltage\n");
        this->lock_timer = 0;
        if (callback) {
          callback(false);
        }
        return;
      }

      this->system.chg_if.SetChargeEndVoltageMV(target_voltage_mV, [this, target, target_voltage_mV, callback = std::move(callback)](bool success) {
        if (!success) {
          DEBUG_PRINTF("* PowerManager SetChargingTargetState failed to write voltage\n");
        } else {
          DEBUG_PRINTF("PowerManager SetChargingTargetState wrote voltage: %u\n", target_voltage_mV); //TODO: temp printout
          //save new state on success
          this->non_volatile_config.SetValue8(PWR_NVM_CHARGE_TARGET_STATE, (uint8_t)target);
        }

        //unlock and propagate success to external callback
        this->lock_timer = 0;
        if (callback) {
          callback(success);
        }
      });
    } else {
      //adapter or battery not present: just save and unlock
      this->non_volatile_config.SetValue8(PWR_NVM_CHARGE_TARGET_STATE, (uint8_t)target);
      this->lock_timer = 0;
      if (callback) {
        callback(true);
      }
    }
  }
}

void PowerManager::SetChargingTargetCurrentA(float current_A, SuccessCallback&& callback, bool queue_if_busy) {
  if (isnanf(current_A) || current_A > PWR_CHG_CURRENT_A_MAX) {
    throw std::invalid_argument("PowerManager SetChargingTargetCurrentA given invalid target current, must be in [0, 8.128]");
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
      this->queued_operations.push_back([this, current_A, callback = std::move(callback)]() mutable {
        this->SetChargingTargetCurrentA(current_A, std::move(callback), false);
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
  this->lock_timer = PWR_LOCK_TIMEOUT_CYCLES;
  __set_PRIMASK(primask);

  //calculate target current in mA
  uint16_t target_current_mA = (uint16_t)floorf(current_A * 1000.0f);
  //get battery's maximum supported charge current - ensuring it's at least 128mA
  uint16_t max_current_mA = MAX(this->system.bat_if.GetMaxChargeCurrentMA(), 128);
  //clamp target current to be below maximum
  if (target_current_mA > max_current_mA) {
    target_current_mA = max_current_mA;
  }
  //round to nearest valid step
  target_current_mA &= I2CDEF_CHG_CHG_CURRENT_MASK;
  //reset to zero if below minimum
  if (target_current_mA < 128) {
    target_current_mA = 0;
  }

  if (this->GetChargingTargetCurrentMA() == target_current_mA) {
    //already in desired state: unlock operations and propagate success to external callback
    this->lock_timer = 0;
    if (callback) {
      callback(true);
    }
  } else {
    //apply new current
    if (this->charging_active && this->system.chg_if.IsAdapterPresent() && this->system.bat_if.IsBatteryPresent()) {
      //adapter and battery present, and charging: apply to the charger directly
      this->system.chg_if.SetFastChargeCurrentMA(target_current_mA, [this, target_current_mA, callback = std::move(callback)](bool success) {
        if (!success) {
          DEBUG_PRINTF("* PowerManager SetChargingTargetCurrentA failed to write current\n");
        } else {
          //save new current on success
          this->non_volatile_config.SetValue16(PWR_NVM_CHARGE_CURRENT, target_current_mA);
          if (target_current_mA < 128) {
            this->charging_active = false;
          }
        }

        //unlock and propagate success to external callback
        this->lock_timer = 0;
        if (callback) {
          callback(success);
        }
      });
    } else {
      //adapter or battery not present, or not chargng: just save and unlock
      this->non_volatile_config.SetValue16(PWR_NVM_CHARGE_CURRENT, target_current_mA);
      this->lock_timer = 0;
      if (callback) {
        callback(true);
      }
    }
  }
}


/******************************************************/
/*               Auto-Shutdown Handling               */
/******************************************************/

uint32_t PowerManager::GetAutoShutdownDelayMS() const {
  return this->non_volatile_config.GetValue32(PWR_NVM_ASD_DELAY);
}


void PowerManager::SetAutoShutdownDelayMS(uint32_t delay_ms) {
  if (delay_ms < PWR_ASD_DELAY_MS_MIN || delay_ms > PWR_ASD_DELAY_MS_MAX) {
    throw std::invalid_argument("PowerManager SetAutoShutdownDelayMS given invalid delay, must be at least 60000");
  }

  //reset timer first to avoid inadvertent trigger
  this->ResetAutoShutdownTimer();

  //save new delay
  this->non_volatile_config.SetValue32(PWR_NVM_ASD_DELAY, delay_ms);
}

void PowerManager::ResetAutoShutdownTimer() {
  this->auto_shutdown_last_reset = HAL_GetTick();
}

