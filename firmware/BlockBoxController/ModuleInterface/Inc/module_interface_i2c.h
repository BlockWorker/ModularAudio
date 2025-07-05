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

  void Read(uint8_t i2c_addr, uint8_t reg_addr, uint8_t* buf, uint16_t length);
  void ReadAsync(I2CModuleInterface* interface, uint8_t reg_addr, uint8_t* buf, uint16_t length);

  void Write(uint8_t i2c_addr, uint8_t reg_addr, const uint8_t* buf, uint16_t length);
  void WriteAsync(I2CModuleInterface* interface, uint8_t reg_addr, const uint8_t* buf, uint16_t length);

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
  GPIO_TypeDef* const int_port;
  const uint16_t int_pin;
  const uint8_t i2c_address;
  const bool uses_crc;

  void ReadRegister(uint8_t reg_addr, uint8_t* buf, uint16_t length) override;
  void WriteRegister(uint8_t reg_addr, const uint8_t* buf, uint16_t length) override;

  //read `count` consecutive registers with the given sizes
  void ReadMultiRegister(uint8_t reg_addr_first, uint8_t* buf, const uint16_t* reg_sizes, uint8_t count);
  void ReadMultiRegisterAsync(uint8_t reg_addr_first, uint8_t* buf, const uint16_t* reg_sizes, uint8_t count, ModuleTransferCallback cb, uintptr_t context);

  //write `count` consecutive registers with the given sizes
  void WriteMultiRegister(uint8_t reg_addr_first, const uint8_t* buf, const uint16_t* reg_sizes, uint8_t count);
  void WriteMultiRegisterAsync(uint8_t reg_addr_first, const uint8_t* buf, const uint16_t* reg_sizes, uint8_t count, ModuleTransferCallback cb, uintptr_t context);

  void HandleInterrupt(ModuleInterfaceInterruptType type, uint16_t extra) noexcept override;

  I2CModuleInterface(I2CHardwareInterface& hw_interface, GPIO_TypeDef* int_port, uint16_t int_pin, uint8_t i2c_address, bool use_crc = true);
  ~I2CModuleInterface() override;

protected:
  ModuleTransferQueueItem* CreateTransferQueueItem() override;
  void StartQueuedAsyncTransfer() noexcept override;

  void HandleAsyncTransferDone(ModuleInterfaceInterruptType itype) noexcept;

  virtual void HandleDataUpdate(uint8_t reg_addr, const uint8_t* buf, uint16_t length) noexcept;
};

#endif


#endif /* INC_MODULE_INTERFACE_I2C_H_ */
