/*
 * module_interface_uart.cpp
 *
 *  Created on: May 9, 2025
 *      Author: Alex
 */

#include "module_interface.h"

void UARTModuleInterface::ReadRegister(uint8_t reg_addr, uint8_t* buf, uint16_t length) {

}

void UARTModuleInterface::WriteRegister(uint8_t reg_addr, const uint8_t* buf, uint16_t length) {

}


void UARTModuleInterface::HandleInterrupt(ModuleInterfaceInterruptType type, uint16_t extra) noexcept {

}


UARTModuleInterface::UARTModuleInterface(UART_HandleTypeDef* uart_handle, bool use_crc) : uart_handle(uart_handle), uses_crc(use_crc) {
  if (uart_handle == NULL) {
    throw std::invalid_argument("UARTModuleInterface UART handle cannot be null");
  }
}

void UARTModuleInterface::StartQueuedAsyncTransfer() noexcept {

}

