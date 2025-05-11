/*
 * module_interface.h
 *
 *  Created on: May 8, 2025
 *      Author: Alex
 */

#ifndef INC_MODULE_INTERFACE_H_
#define INC_MODULE_INTERFACE_H_

#include "cpp_main.h"


#define MODIF_BLOCKING_TIMEOUT_MS 20


#ifdef __cplusplus

#include <vector>
#include <deque>

extern "C" {
#endif

typedef enum {
  IF_EXTI,
  IF_TX_COMPLETE,
  IF_RX_COMPLETE,
  IF_ERROR
} ModuleInterfaceInterruptType;

#ifdef __cplusplus
}

class ModuleInterface;

//callback type for module events - arguments: reference to module interface, event type
typedef void (*ModuleEventCallback)(ModuleInterface&, uint32_t);

typedef struct {
  ModuleEventCallback func;
  uint32_t event_mask;
} ModuleEventCallbackRegistration;

//transfer types
typedef enum {
  TF_READ,
  TF_WRITE
} ModuleTransferType;

//callback type for module register transfers - arguments: success, context, value where applicable
typedef void (*ModuleTransferCallback)(bool, uintptr_t, uint32_t);

class ModuleTransferQueueItem {
public:
  ModuleTransferType type;

  uint8_t reg_addr;
  uint8_t* data_ptr;
  uint16_t length;

  uint32_t value_buffer;
  bool success;

  ModuleTransferCallback callback;
  uintptr_t context;

  virtual ~ModuleTransferQueueItem() = default;
};


//abstract interface for interacting with other system modules
class ModuleInterface {
public:
  virtual void ReadRegister(uint8_t reg_addr, uint8_t* buf, uint16_t length) = 0;
  uint8_t ReadRegister8(uint8_t reg_addr);
  uint16_t ReadRegister16(uint16_t reg_addr);
  uint32_t ReadRegister32(uint16_t reg_addr);

  void ReadRegisterAsync(uint8_t reg_addr, uint8_t* buf, uint16_t length, ModuleTransferCallback cb, uintptr_t context);
  void ReadRegister8Async(uint8_t reg_addr, ModuleTransferCallback cb, uintptr_t context);
  void ReadRegister16Async(uint8_t reg_addr, ModuleTransferCallback cb, uintptr_t context);
  void ReadRegister32Async(uint8_t reg_addr, ModuleTransferCallback cb, uintptr_t context);

  virtual void WriteRegister(uint8_t reg_addr, const uint8_t* buf, uint16_t length) = 0;
  void WriteRegister8(uint8_t reg_addr, uint8_t value);
  void WriteRegister16(uint16_t reg_addr, uint16_t value);
  void WriteRegister32(uint16_t reg_addr, uint32_t value);

  void WriteRegisterAsync(uint8_t reg_addr, const uint8_t* buf, uint16_t length, ModuleTransferCallback cb, uintptr_t context);
  void WriteRegister8Async(uint8_t reg_addr, uint8_t value, ModuleTransferCallback cb, uintptr_t context);
  void WriteRegister16Async(uint16_t reg_addr, uint16_t value, ModuleTransferCallback cb, uintptr_t context);
  void WriteRegister32Async(uint16_t reg_addr, uint32_t value, ModuleTransferCallback cb, uintptr_t context);

  virtual void HandleInterrupt(ModuleInterfaceInterruptType type, uint16_t extra) noexcept = 0;

  void RegisterCallback(ModuleEventCallback cb, uint32_t event_mask);
  void UnregisterCallback(ModuleEventCallback cb);

  virtual void LoopTasks();

  virtual ~ModuleInterface();

protected:
  std::vector<ModuleEventCallbackRegistration> registered_callbacks;
  std::deque<ModuleTransferQueueItem*> queued_transfers;
  std::vector<ModuleTransferQueueItem*> completed_transfers;
  bool async_transfer_active = false;

  virtual ModuleTransferQueueItem* CreateTransferQueueItem();
  virtual void StartQueuedAsyncTransfer() noexcept = 0;

  void ExecuteCallbacks(uint32_t event) noexcept;
};


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

private:
  void HandleAsyncTransferDone(ModuleInterfaceInterruptType itype) noexcept;
};


//interface for interacting with other system modules through a UART bus
class UARTModuleInterface : public ModuleInterface {
public:
  UART_HandleTypeDef* const uart_handle;
  const bool uses_crc;

  void ReadRegister(uint8_t reg_addr, uint8_t* buf, uint16_t length) override;
  void WriteRegister(uint8_t reg_addr, const uint8_t* buf, uint16_t length) override;

  void HandleInterrupt(ModuleInterfaceInterruptType type, uint16_t extra) noexcept override;

  UARTModuleInterface(UART_HandleTypeDef* uart_handle, bool use_crc = true);

protected:
  void StartQueuedAsyncTransfer() noexcept override;
};

#endif


#endif /* INC_MODULE_INTERFACE_H_ */
