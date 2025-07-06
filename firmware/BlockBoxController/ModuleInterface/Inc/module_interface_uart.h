/*
 * module_interface_uart.h
 *
 *  Created on: Jul 5, 2025
 *      Author: Alex
 */

#ifndef INC_MODULE_INTERFACE_UART_H_
#define INC_MODULE_INTERFACE_UART_H_


#include "cpp_main.h"
#include "module_interface.h"
#include "register_set.h"


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
extern "C" {
#endif


#ifdef __cplusplus
}


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


//register-enabled UART module interface
class RegUARTModuleInterface : public UARTModuleInterface {
public:
  const RegisterSet& registers;

  RegUARTModuleInterface(UART_HandleTypeDef* uart_handle, const uint16_t* reg_sizes, bool use_crc = true);
  RegUARTModuleInterface(UART_HandleTypeDef* uart_handle, std::initializer_list<uint16_t> reg_sizes, bool use_crc = true);

protected:
  RegisterSet _registers;

  void HandleNotificationData(bool unsolicited) override;
  virtual void OnRegisterUpdate(uint8_t address);
};


#endif


#endif /* INC_MODULE_INTERFACE_UART_H_ */
