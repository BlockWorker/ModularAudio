/*
 * module_interface_i2c.h
 *
 *  Created on: Jul 5, 2025
 *      Author: Alex
 */

#ifndef INC_MODULE_INTERFACE_I2C_H_
#define INC_MODULE_INTERFACE_I2C_H_


#include "cpp_main.h"
#include "module_interface.h"
#include "register_set.h"


//register addresses for standardised interrupt handling (IntRegI2CModuleInterface)
#define MODIF_I2C_INT_MASK_REG 0x10
#define MODIF_I2C_INT_FLAGS_REG 0x11
//interrupt flag for universal "reset" interrupt - all other values reflect the respective INT_FLAGS register exactly
#define MODIF_I2C_INT_RESET_FLAG 0

//timeout for interrupt handling, in main loop cycles
#define MODIF_I2C_INT_HANDLING_TIMEOUT (200 / MAIN_LOOP_PERIOD_MS)


#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
}


class I2CModuleInterface;

//interface for managing a (potentially shared) I2C bus
class I2CHardwareInterface {
  friend I2CModuleInterface;
public:
  I2C_HandleTypeDef* const i2c_handle;

  bool IsBusy() noexcept;

  void Read(uint8_t i2c_addr, uint16_t reg_addr, uint16_t reg_addr_size, uint8_t* buf, uint16_t length);
  void ReadAsync(I2CModuleInterface* interface, uint16_t reg_addr, uint8_t* buf, uint16_t length);

  void Write(uint8_t i2c_addr, uint16_t reg_addr, uint16_t reg_addr_size, const uint8_t* buf, uint16_t length);
  void WriteAsync(I2CModuleInterface* interface, uint16_t reg_addr, const uint8_t* buf, uint16_t length);

  void HandleInterrupt(ModuleInterfaceInterruptType type) noexcept;

  void Init();
  void LoopTasks();

  I2CHardwareInterface(I2C_HandleTypeDef* i2c_handle, void (*hardware_reset_func)());

private:
  void (*const hardware_reset_func)();

  std::vector<I2CModuleInterface*> registered_interfaces;
  I2CModuleInterface* active_async_interface = NULL;

  uint32_t idle_busy_count;
  uint32_t non_idle_timeout;

  void RegisterInterface(I2CModuleInterface* interface);
  void UnregisterInterface(I2CModuleInterface* interface);

  void StartNextTransfer() noexcept;
  void Reset() noexcept;
};


//interface for interacting with other system modules through an I2C bus
class I2CModuleInterface : public ModuleInterface {
  friend I2CHardwareInterface;
public:
  I2CHardwareInterface& hw_interface;
  const uint8_t i2c_address;
  const uint16_t reg_addr_size;
  const bool uses_crc;

#pragma GCC diagnostic ignored "-Woverloaded-virtual="
  void ReadRegister(uint16_t reg_addr, uint8_t* buf, uint16_t length) override;
  void WriteRegister(uint16_t reg_addr, const uint8_t* buf, uint16_t length) override;
#pragma GCC diagnostic pop

  //read `count` consecutive registers with the given sizes
  void ReadMultiRegister(uint16_t reg_addr_first, uint8_t* buf, const uint16_t* reg_sizes, uint16_t count);
  void ReadMultiRegisterAsync(uint16_t reg_addr_first, uint8_t* buf, const uint16_t* reg_sizes, uint16_t count, ModuleTransferCallback&& callback);

  //write `count` consecutive registers with the given sizes
  void WriteMultiRegister(uint16_t reg_addr_first, const uint8_t* buf, const uint16_t* reg_sizes, uint16_t count);
  void WriteMultiRegisterAsync(uint16_t reg_addr_first, const uint8_t* buf, const uint16_t* reg_sizes, uint16_t count, ModuleTransferCallback&& callback);

  void HandleInterrupt(ModuleInterfaceInterruptType type, uint16_t extra) noexcept override;

  I2CModuleInterface(I2CHardwareInterface& hw_interface, uint8_t i2c_address, uint16_t reg_addr_size, bool use_crc = true);
  ~I2CModuleInterface() override;

protected:
  ModuleTransferQueueItem* CreateTransferQueueItem() override;
  void StartQueuedAsyncTransfer() noexcept override;

  void HandleAsyncTransferDone(ModuleInterfaceInterruptType itype) noexcept;

  virtual void HandleDataUpdate(uint16_t reg_addr, const uint8_t* buf, uint16_t length) noexcept;
};


//register-enabled I2C module interface - currently restricted to 8-bit register addresses
class RegI2CModuleInterface : public I2CModuleInterface {
public:
  const RegisterSet& registers;

  //register read/write functions with automatic length inference based on the register set
  void ReadRegister(uint8_t reg_addr, uint8_t* buf);
  uint8_t ReadRegister8(uint8_t reg_addr);
  uint16_t ReadRegister16(uint8_t reg_addr);
  uint32_t ReadRegister32(uint8_t reg_addr);

  void ReadRegisterAsync(uint8_t reg_addr, uint8_t* buf, ModuleTransferCallback&& callback);
  void ReadRegister8Async(uint8_t reg_addr, ModuleTransferCallback&& callback);
  void ReadRegister16Async(uint8_t reg_addr, ModuleTransferCallback&& callback);
  void ReadRegister32Async(uint8_t reg_addr, ModuleTransferCallback&& callback);

  void WriteRegister(uint8_t reg_addr, const uint8_t* buf);
  void WriteRegister8(uint8_t reg_addr, uint8_t value);
  void WriteRegister16(uint8_t reg_addr, uint16_t value);
  void WriteRegister32(uint8_t reg_addr, uint32_t value);

  void WriteRegisterAsync(uint8_t reg_addr, const uint8_t* buf, ModuleTransferCallback&& callback);
  void WriteRegister8Async(uint8_t reg_addr, uint8_t value, ModuleTransferCallback&& callback);
  void WriteRegister16Async(uint8_t reg_addr, uint16_t value, ModuleTransferCallback&& callback);
  void WriteRegister32Async(uint8_t reg_addr, uint32_t value, ModuleTransferCallback&& callback);

  //read `count` consecutive registers with the given sizes - infers the register sizes automatically from the register set
  void ReadMultiRegister(uint8_t reg_addr_first, uint8_t* buf, uint8_t count);
  void ReadMultiRegisterAsync(uint8_t reg_addr_first, uint8_t* buf, uint8_t count, ModuleTransferCallback&& callback);

  //write `count` consecutive registers with the given sizes - infers the register sizes automatically from the register set
  void WriteMultiRegister(uint8_t reg_addr_first, const uint8_t* buf, uint8_t count);
  void WriteMultiRegisterAsync(uint8_t reg_addr_first, const uint8_t* buf, uint8_t count, ModuleTransferCallback&& callback);

  RegI2CModuleInterface(I2CHardwareInterface& hw_interface, uint8_t i2c_address, const uint16_t* reg_sizes, bool use_crc = true);
  RegI2CModuleInterface(I2CHardwareInterface& hw_interface, uint8_t i2c_address, std::initializer_list<uint16_t> reg_sizes, bool use_crc = true);

protected:
  RegisterSet _registers;

  //register size retrieval functions with validity checking
  uint16_t GetRegisterSize(uint8_t reg_addr);
  const uint16_t* GetMultiRegisterSizes(uint8_t reg_addr_first, uint8_t count);

  void HandleDataUpdate(uint16_t reg_addr, const uint8_t* buf, uint16_t length) noexcept override;
  virtual void OnRegisterUpdate(uint8_t address);
};


//register- and interrupt-enabled I2C module interface (using standardised interrupt control registers)
class IntRegI2CModuleInterface : public RegI2CModuleInterface {
public:
  GPIO_TypeDef* const int_port;
  const uint16_t int_pin;

  uint16_t GetInterruptMask();
  void SetInterruptMask(uint16_t mask, SuccessCallback&& callback);

  void HandleInterrupt(ModuleInterfaceInterruptType type, uint16_t extra) noexcept override;

  void LoopTasks() override;

  IntRegI2CModuleInterface(I2CHardwareInterface& hw_interface, uint8_t i2c_address, const uint16_t* reg_sizes, GPIO_TypeDef* int_port, uint16_t int_pin, bool use_crc = true);
  IntRegI2CModuleInterface(I2CHardwareInterface& hw_interface, uint8_t i2c_address, std::initializer_list<uint16_t> reg_sizes, GPIO_TypeDef* int_port, uint16_t int_pin, bool use_crc = true);

protected:
  uint8_t int_reg_size; //size of interrupt mask/flags registers: 1 = one byte, 2 = two bytes
  uint32_t current_interrupt_timer;

  void CheckInterruptRegisterDefinitions();

  virtual void OnI2CInterrupt(uint16_t interrupt_flags);
};


#endif


#endif /* INC_MODULE_INTERFACE_I2C_H_ */
