/*
 * charger_interface.cpp
 *
 *  Created on: Aug 25, 2025
 *      Author: Alex
 */


#include "charger_interface.h"

/*//constants for ADC conversion
#define IF_CHG_ADC_VREF 3.3f
#define IF_CHG_ADC_FULL_SCALE ((float)UINT16_MAX)
#define IF_CHG_ADC_ISNS_GAIN 20.0f
#define IF_CHG_ADC_ISNS_R 0.004f
//ADC conversion factor, from counts to amps
#define IF_CHG_ADC_CONV_FACTOR (IF_CHG_ADC_VREF / (IF_CHG_ADC_FULL_SCALE * IF_CHG_ADC_ISNS_GAIN * IF_CHG_ADC_ISNS_R))*/


bool ChargerInterface::IsAdapterPresent() const {
  return this->initialised && this->adapter_present;
}


bool ChargerInterface::IsLearnModeActive() const {
  return (this->registers.Reg16(I2CDEF_CHG_CHG_OPTION) & I2CDEF_CHG_CHG_OPTION_LEARN_Msk) != 0;
}


uint16_t ChargerInterface::GetFastChargeCurrentMA() const {
  return this->registers.Reg16(I2CDEF_CHG_CHG_CURRENT) & I2CDEF_CHG_CHG_CURRENT_MASK;
}

uint16_t ChargerInterface::GetChargeEndVoltageMV() const {
  return this->registers.Reg16(I2CDEF_CHG_CHG_VOLTAGE) & I2CDEF_CHG_CHG_VOLTAGE_MASK;
}


float ChargerInterface::GetMaxAdapterCurrentA() const {
  //get current in 2.5mA steps
  uint16_t current_steps = this->registers.Reg16(I2CDEF_CHG_INPUT_CURRENT) & I2CDEF_CHG_INPUT_CURRENT_MASK;

  //convert to amps and return
  return (float)current_steps * 0.0025f;
}

float ChargerInterface::GetAdapterCurrentA() const {
  return NAN;//(float)this->adapter_current_raw * IF_CHG_ADC_CONV_FACTOR;
}



void ChargerInterface::SetLearnModeActive(bool learn_mode, SuccessCallback&& callback) {
  //prepare charge options register
  uint16_t chg_options =
      I2CDEF_CHG_CHG_OPTION_ACOC_EN_Msk | //enable adapter overcurrent protection
      I2CDEF_CHG_CHG_OPTION_BOOST_EN_Msk | //enable boost mode
      (learn_mode ? I2CDEF_CHG_CHG_OPTION_LEARN_Msk : 0) | //enable/disable learn mode as requested
      I2CDEF_CHG_CHG_OPTION_IFAULT_HI_EN_Msk | //enable high-side FET short circuit protection
      (0x3 << I2CDEF_CHG_CHG_OPTION_BAT_DEPL_Pos) | //set depletion threshold to 70.97% of voltage setting
      (0x3 << I2CDEF_CHG_CHG_OPTION_WDT_Pos) | //set WDT to enabled at 175s
      I2CDEF_CHG_CHG_OPTION_ACOK_DEG_T_Msk; //set ACOK deglitch time to 1.3s

  //write options register
  this->WriteRegister16Async(I2CDEF_CHG_CHG_OPTION, chg_options, [this, callback = std::move(callback), chg_options](bool, uint32_t, uint16_t) {
    //read back value to ensure correctness and up-to-date register state
    this->ReadRegister16Async(I2CDEF_CHG_CHG_OPTION, callback ? [this, callback = std::move(callback), chg_options](bool success, uint32_t value, uint16_t) {
      //report result (and value correctness, ignoring status bits) to external callback
      uint16_t value_without_status = (uint16_t)value & ~(I2CDEF_CHG_CHG_OPTION_BOOST_ACTIVE_Msk | I2CDEF_CHG_CHG_OPTION_AC_PRES_Msk);
      callback(success && value_without_status == chg_options);
    } : ModuleTransferCallback());
  });
}


void ChargerInterface::SetFastChargeCurrentMA(uint16_t current_mA, SuccessCallback&& callback) {
  uint16_t masked_current = current_mA & I2CDEF_CHG_CHG_CURRENT_MASK;

  //write current register
  this->WriteRegister16Async(I2CDEF_CHG_CHG_CURRENT, masked_current, [this, callback = std::move(callback), masked_current](bool, uint32_t, uint16_t) {
    //read back value to ensure correctness and up-to-date register state
    this->ReadRegister16Async(I2CDEF_CHG_CHG_CURRENT, callback ? [this, callback = std::move(callback), masked_current](bool success, uint32_t value, uint16_t) {
      //report result (and value correctness) to external callback
      callback(success && (uint16_t)value == masked_current);
    } : ModuleTransferCallback());
  });
}

void ChargerInterface::SetChargeEndVoltageMV(uint16_t voltage_mV, SuccessCallback&& callback) {
  uint16_t masked_voltage = voltage_mV & I2CDEF_CHG_CHG_VOLTAGE_MASK;

  //write current register
  this->WriteRegister16Async(I2CDEF_CHG_CHG_VOLTAGE, masked_voltage, [this, callback = std::move(callback), masked_voltage](bool, uint32_t, uint16_t) {
    //read back value to ensure correctness and up-to-date register state
    this->ReadRegister16Async(I2CDEF_CHG_CHG_VOLTAGE, callback ? [this, callback = std::move(callback), masked_voltage](bool success, uint32_t value, uint16_t) {
      //report result (and value correctness) to external callback
      callback(success && (uint16_t)value == masked_voltage);
    } : ModuleTransferCallback());
  });
}


void ChargerInterface::SetMaxAdapterCurrentA(float current_A, SuccessCallback&& callback) {
  if (isnanf(current_A) || current_A < 0.33f || current_A > 20.16f) {
    throw std::invalid_argument("ChargerInterface SetMaxAdapterCurrentA given invalid current, must be in [0.33, 20.16]");
  }

  //convert to 2.5mA steps
  uint16_t current_steps = (uint16_t)roundf(current_A / 0.0025f);
  uint16_t masked_current = current_steps & I2CDEF_CHG_INPUT_CURRENT_MASK;

  //write current register
  this->WriteRegister16Async(I2CDEF_CHG_INPUT_CURRENT, masked_current, [this, callback = std::move(callback), masked_current](bool, uint32_t, uint16_t) {
    //read back value to ensure correctness and up-to-date register state
    this->ReadRegister16Async(I2CDEF_CHG_INPUT_CURRENT, callback ? [this, callback = std::move(callback), masked_current](bool success, uint32_t value, uint16_t) {
      //report result (and value correctness) to external callback
      callback(success && (uint16_t)value == masked_current);
    } : ModuleTransferCallback());
  });
}



void ChargerInterface::HandleInterrupt(ModuleInterfaceInterruptType type, uint16_t extra) noexcept {
  switch (type) {
    case IF_EXTI:
    {
      //reset debounce counter
      uint32_t primask = __get_PRIMASK();
      __disable_irq();
      if (this->acok_state == HAL_GPIO_ReadPin(this->acok_port, this->acok_pin)) {
        //same state as saved: set counter to zero, nothing to debounce
        this->acok_debounce_counter = 0;
      } else {
        //different state than saved: (re)start debounce countdown
        this->acok_debounce_counter = IF_CHG_ACOK_DEBOUNCE_CYCLES;
      }
      __set_PRIMASK(primask);
      break;
    }
    /*case IF_ADC:
      this->adapter_current_raw = (uint16_t)HAL_ADC_GetValue(this->current_adc);
      HAL_ADC_Stop_IT(this->current_adc);
      DEBUG_PRINTF("ADC raw: %u\n", this->adapter_current_raw);
      break;*/
    default:
      //allow base handling
      this->RegI2CModuleInterface::HandleInterrupt(type, extra);
      break;
  }
}


void ChargerInterface::Init() {
  //allow base handling
  this->RegI2CModuleInterface::Init();

  //initialise ACOK pin state with true initial pin state
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  this->acok_state = HAL_GPIO_ReadPin(this->acok_port, this->acok_pin);
  this->acok_debounce_counter = 0;
  __set_PRIMASK(primask);

  /*//init ADC
  HAL_ADCEx_Calibration_Start(this->current_adc, ADC_CALIB_OFFSET_LINEARITY, ADC_SINGLE_ENDED);*/
}


void ChargerInterface::InitModule(SuccessCallback&& callback) {
  this->initialised = false;
  this->adapter_present = false;
  //this->adapter_current_raw = 0;

  if (this->acok_state == GPIO_PIN_RESET) {
    //ACOK signal low: not present, init done at this point (considered "unsuccessful")
    this->initialised = true;
    if (callback) {
      callback(false);
    }
  } else {
    //ACOK signal high: should be present
    //read device ID to check communication
    this->ReadRegister16Async(I2CDEF_CHG_DEV_ID, [this, callback = std::move(callback)](bool success, uint32_t value, uint16_t) {
      if (!success) {
        //report failure to external callback, init can be regarded as complete with non-present charger
        this->initialised = true;
        if (callback) {
          callback(false);
        }
        return;
      }

      //check correctness of device ID
      if ((uint16_t)value != I2CDEF_CHG_DEV_ID_VALUE) {
        DEBUG_PRINTF("* Charger device ID incorrect: 0x%04X instead of 0x%04X\n", (uint16_t)value, I2CDEF_CHG_DEV_ID_VALUE);
        //report failure to external callback, init can be regarded as complete with non-present charger
        this->initialised = true;
        if (callback) {
          callback(false);
        }
        return;
      }

      //setup charger config with learn mode initially disabled
      this->SetLearnModeActive(false, [this, callback = std::move(callback)](bool success) {
        if (!success) {
          //report failure to external callback, init can be regarded as complete with non-present charger
          this->initialised = true;
          if (callback) {
            callback(false);
          }
          return;
        }

        //setup default "safe" adapter current limit
        this->SetMaxAdapterCurrentA(IF_CHG_SAFE_ADAPTER_CURRENT_A, [this, callback = std::move(callback)](bool success) {
          if (!success) {
            //report failure to external callback, init can be regarded as complete with non-present charger
            this->initialised = true;
            if (callback) {
              callback(false);
            }
            return;
          }

          //read charge current/voltage set point registers
          this->ReadRegister16Async(I2CDEF_CHG_CHG_CURRENT, ModuleTransferCallback());
          this->ReadRegister16Async(I2CDEF_CHG_CHG_VOLTAGE, [this, callback = std::move(callback)](bool, uint32_t, uint16_t) {
            //after last read is done: init completed successfully with charger present (even if read failed - that's non-critical)
            this->initialised = true;
            this->adapter_present = true;
            if (callback) {
              callback(true);
            }
          });
        });
      });
    });
  }
}

void ChargerInterface::LoopTasks() {
  static uint32_t loop_count = 0;

  //handle ACOK pin debounce
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  GPIO_PinState new_state = HAL_GPIO_ReadPin(this->acok_port, this->acok_pin);
  if (this->acok_state == new_state) {
    //same state as saved: set counter to zero, nothing to debounce
    this->acok_debounce_counter = 0;
  } else if (this->acok_debounce_counter == 0) {
    //different state than saved, but not counting down: (re)start debounce countdown to be safe
    this->acok_debounce_counter = IF_CHG_ACOK_DEBOUNCE_CYCLES;
  } else {
    //different state than saved, and counting down: decrement counter
    if (--this->acok_debounce_counter == 0) {
      //countdown finished: debounce done, save new state
      this->acok_state = new_state;
    }
  }
  __set_PRIMASK(primask);

  if (this->initialised) {
    if (this->adapter_present) {
      //check if ACOK has gone low
      if (this->acok_state != GPIO_PIN_SET) {
        //no longer present; flush all outstanding transfers and execute callbacks
        primask = __get_PRIMASK();
        __disable_irq();
        this->adapter_present = false;

        for (auto transfer : this->queued_transfers) {
          delete transfer;
        }
        this->queued_transfers.clear();

        this->ResetHardwareInterface();
        __set_PRIMASK(primask);

        this->ExecuteCallbacks(MODIF_CHG_EVENT_PRESENCE_UPDATE);
      } else {
        //ACOK still high: still present, do periodic reads
        if (loop_count % 100 == 0) {
          //every 100 cycles (1000ms), read config/status - no callback needed, we're only reading to update the register
          this->ReadRegister16Async(I2CDEF_CHG_CHG_OPTION, ModuleTransferCallback());
          /*//also start ADC conversion
          HAL_ADC_Start_IT(this->current_adc);*/
        }
      }
    } else {
      //check if ACOK has gone high
      if (this->acok_state == GPIO_PIN_SET) {
        //attempt to re-init module
        this->InitModule([this](bool success) {
          if (success) {
            //successful init: charger is now present, execute callbacks
            this->ExecuteCallbacks(MODIF_CHG_EVENT_PRESENCE_UPDATE);
          }
        });
      }
    }
    loop_count++;
  }

  //allow base handling
  this->RegI2CModuleInterface::LoopTasks();
}



ChargerInterface::ChargerInterface(I2CHardwareInterface& hw_interface, uint8_t i2c_address, GPIO_TypeDef* acok_port, uint16_t acok_pin, ADC_HandleTypeDef* current_adc) :
    RegI2CModuleInterface(hw_interface, i2c_address, I2CDEF_CHG_REG_SIZES, IF_CHG_USE_CRC), acok_port(acok_port), acok_pin(acok_pin), current_adc(current_adc), initialised(false),
    adapter_present(false), acok_state(GPIO_PIN_RESET), acok_debounce_counter(0)/*, adapter_current_raw(0)*/ {}


void ChargerInterface::OnRegisterUpdate(uint8_t address) {
  //allow base handling
  this->RegI2CModuleInterface::OnRegisterUpdate(address);

  if (!this->initialised || !this->adapter_present) {
    return;
  }

  uint32_t event;

  //select corresponding event
  switch (address) {
    case I2CDEF_CHG_CHG_OPTION:
      event = MODIF_CHG_EVENT_LEARN_UPDATE;
      break;
    default:
      return;
  }

  this->ExecuteCallbacks(event);
}


