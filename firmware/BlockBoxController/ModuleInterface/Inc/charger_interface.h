/*
 * charger_interface.h
 *
 *  Created on: Aug 25, 2025
 *      Author: Alex
 */

#ifndef INC_CHARGER_INTERFACE_H_
#define INC_CHARGER_INTERFACE_H_


#include "cpp_main.h"
#include "module_interface_i2c.h"
#include "i2c_defines_charger.h"


//whether the charger interface uses CRC for communication
#define IF_CHG_USE_CRC false

//charger interface events
#define MODIF_CHG_EVENT_PRESENCE_UPDATE (1u << 8)
#define MODIF_CHG_EVENT_LEARN_UPDATE (1u << 9)

//default "safe" maximum adapter current, in amps
#define IF_CHG_SAFE_ADAPTER_CURRENT_A 5.12f

//ACOK signal debounce time, in main loop cycles
#define IF_CHG_ACOK_DEBOUNCE_CYCLES (100 / MAIN_LOOP_PERIOD_MS)


#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
}


class ChargerInterface : public RegI2CModuleInterface {
public:
  bool IsAdapterPresent() const;

  bool IsLearnModeActive() const;

  uint16_t GetFastChargeCurrentMA() const;
  uint16_t GetChargeEndVoltageMV() const;

  float GetMaxAdapterCurrentA() const;
  float GetAdapterCurrentA() const;


  void SetLearnModeActive(bool learn_mode, SuccessCallback&& callback);

  void SetFastChargeCurrentMA(uint16_t current_mA, SuccessCallback&& callback);
  void SetChargeEndVoltageMV(uint16_t voltage_mV, SuccessCallback&& callback);

  void SetMaxAdapterCurrentA(float current_A, SuccessCallback&& callback);


  void HandleInterrupt(ModuleInterfaceInterruptType type, uint16_t extra) noexcept override;

  void Init() override;

  void InitModule(SuccessCallback&& callback);
  void LoopTasks() override;


  ChargerInterface(I2CHardwareInterface& hw_interface, uint8_t i2c_address, GPIO_TypeDef* acok_port, uint16_t acok_pin, ADC_HandleTypeDef* current_adc);

protected:
  GPIO_TypeDef* const acok_port;
  const uint16_t acok_pin;
  ADC_HandleTypeDef* const current_adc;

  bool initialised;
  bool adapter_present;

  GPIO_PinState acok_state;
  uint32_t acok_debounce_counter;

  uint16_t adapter_current_raw;

  void OnRegisterUpdate(uint8_t address) override;
};


#endif


#endif /* INC_CHARGER_INTERFACE_H_ */
