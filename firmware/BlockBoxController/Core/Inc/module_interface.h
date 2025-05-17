/*
 * module_interface.h
 *
 *  Created on: May 8, 2025
 *      Author: Alex
 */

#ifndef INC_MODULE_INTERFACE_H_
#define INC_MODULE_INTERFACE_H_

#include "cpp_main.h"

//common constants
//blocking operation timeout
#define MODIF_BLOCKING_TIMEOUT_MS 20
//common interface events
#define MODIF_EVENT_INTERRUPT 0
#define MODIF_EVENT_ERROR 1

//UART-specific constants
//buffer sizes
#define MODIF_UART_RXBUF_SIZE 4096
//special byte values
#define MODIF_UART_START_BYTE 0xF1
#define MODIF_UART_END_BYTE 0xFA
#define MODIF_UART_ESCAPE_BYTE 0xFF
//common Master -> Slave transfer types
typedef enum {
  IF_UART_TYPE_READ = 0x00,
  IF_UART_TYPE_WRITE = 0x01
} ModuleInterfaceUARTCommandType;
//common Slave -> Master transfer types
typedef enum {
  IF_UART_TYPE_EVENT = 0x00,
  IF_UART_TYPE_CHANGE_NOTIFICATION = 0x01,
  IF_UART_TYPE_READ_DATA = 0x02
} ModuleInterfaceUARTNotificationType;
//common event notification types
typedef enum {
  IF_UART_EVENT_MCU_RESET = 0x00,
  IF_UART_EVENT_WRITE_ACK = 0x01,
  IF_UART_EVENT_ERROR = 0x02
} ModuleInterfaceUARTEventNotifType;
//error types
typedef enum {
  IF_UART_ERROR_UNKNOWN = 0,
  IF_UART_ERROR_UART_FORMAT_ERROR_REMOTE = 0x8001,
  IF_UART_ERROR_INTERNAL_UART_ERROR_REMOTE = 0x8002,
  IF_UART_ERROR_UART_COMMAND_NOT_ALLOWED = 0x8003,
  IF_UART_ERROR_UART_CRC_ERROR_REMOTE = 0x8004,
  IF_UART_ERROR_UART_FORMAT_ERROR_LOCAL = 0xFFF0,
  IF_UART_ERROR_UART_CRC_ERROR_LOCAL = 0xFFF1
} ModuleInterfaceUARTErrorType;


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
  TF_READ = 0x1,
  TF_WRITE = 0x2
} ModuleTransferType;

//callback type for module register transfers - arguments: success, context, value where applicable, length in bytes
typedef void (*ModuleTransferCallback)(bool, uintptr_t, uint32_t, uint16_t);

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

  virtual void Init();
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

  void HandleAsyncTransferDone(ModuleInterfaceInterruptType itype) noexcept;

  virtual void HandleDataUpdate(uint8_t reg_addr, const uint8_t* buf, uint16_t length) noexcept;
};


//interface for interacting with other system modules through a UART bus
class UARTModuleInterface : public ModuleInterface {
public:
  UART_HandleTypeDef* const uart_handle;
  const bool uses_crc;

  void ReadRegister(uint8_t reg_addr, uint8_t* buf, uint16_t length) override;
  void WriteRegister(uint8_t reg_addr, const uint8_t* buf, uint16_t length) override;

  void HandleInterrupt(ModuleInterfaceInterruptType type, uint16_t extra) noexcept override;

  void Init() override;
  void LoopTasks() override;

  //return value is only valid inside error event handlers
  ModuleInterfaceUARTErrorType GetCurrentError() noexcept;

  UARTModuleInterface(UART_HandleTypeDef* uart_handle, bool use_crc = true);

protected:
  uint8_t rx_buffer[MODIF_UART_RXBUF_SIZE] = { 0 };
  uint32_t rx_buffer_write_offset = 0;
  uint32_t rx_buffer_read_offset = 0;
  bool rx_escape_active = false;
  bool rx_skip_to_start = true;

  std::vector<uint8_t> parse_buffer;

  bool interrupt_error = false;
  ModuleInterfaceUARTErrorType current_error;

  ModuleTransferQueueItem* CreateTransferQueueItem() override;
  void StartQueuedAsyncTransfer() noexcept override;

  void ProcessRawReceivedData() noexcept;
  void ParseRawNotification();

  virtual void HandleNotificationData(bool unsolicited);

  virtual bool IsCommandError(bool* should_retry) noexcept;

  void Reset();
};

#endif


#endif /* INC_MODULE_INTERFACE_H_ */
