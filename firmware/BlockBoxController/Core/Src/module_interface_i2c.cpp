/*
 * module_interface_i2c.cpp
 *
 *  Created on: May 9, 2025
 *      Author: Alex
 */


#include "module_interface.h"


//I2C CRC data
static const uint8_t _i2c_crc_table[256] = {
  0x00, 0x7F, 0xFE, 0x81, 0x83, 0xFC, 0x7D, 0x02, 0x79, 0x06, 0x87, 0xF8, 0xFA, 0x85, 0x04, 0x7B,
  0xF2, 0x8D, 0x0C, 0x73, 0x71, 0x0E, 0x8F, 0xF0, 0x8B, 0xF4, 0x75, 0x0A, 0x08, 0x77, 0xF6, 0x89,
  0x9B, 0xE4, 0x65, 0x1A, 0x18, 0x67, 0xE6, 0x99, 0xE2, 0x9D, 0x1C, 0x63, 0x61, 0x1E, 0x9F, 0xE0,
  0x69, 0x16, 0x97, 0xE8, 0xEA, 0x95, 0x14, 0x6B, 0x10, 0x6F, 0xEE, 0x91, 0x93, 0xEC, 0x6D, 0x12,
  0x49, 0x36, 0xB7, 0xC8, 0xCA, 0xB5, 0x34, 0x4B, 0x30, 0x4F, 0xCE, 0xB1, 0xB3, 0xCC, 0x4D, 0x32,
  0xBB, 0xC4, 0x45, 0x3A, 0x38, 0x47, 0xC6, 0xB9, 0xC2, 0xBD, 0x3C, 0x43, 0x41, 0x3E, 0xBF, 0xC0,
  0xD2, 0xAD, 0x2C, 0x53, 0x51, 0x2E, 0xAF, 0xD0, 0xAB, 0xD4, 0x55, 0x2A, 0x28, 0x57, 0xD6, 0xA9,
  0x20, 0x5F, 0xDE, 0xA1, 0xA3, 0xDC, 0x5D, 0x22, 0x59, 0x26, 0xA7, 0xD8, 0xDA, 0xA5, 0x24, 0x5B,
  0x92, 0xED, 0x6C, 0x13, 0x11, 0x6E, 0xEF, 0x90, 0xEB, 0x94, 0x15, 0x6A, 0x68, 0x17, 0x96, 0xE9,
  0x60, 0x1F, 0x9E, 0xE1, 0xE3, 0x9C, 0x1D, 0x62, 0x19, 0x66, 0xE7, 0x98, 0x9A, 0xE5, 0x64, 0x1B,
  0x09, 0x76, 0xF7, 0x88, 0x8A, 0xF5, 0x74, 0x0B, 0x70, 0x0F, 0x8E, 0xF1, 0xF3, 0x8C, 0x0D, 0x72,
  0xFB, 0x84, 0x05, 0x7A, 0x78, 0x07, 0x86, 0xF9, 0x82, 0xFD, 0x7C, 0x03, 0x01, 0x7E, 0xFF, 0x80,
  0xDB, 0xA4, 0x25, 0x5A, 0x58, 0x27, 0xA6, 0xD9, 0xA2, 0xDD, 0x5C, 0x23, 0x21, 0x5E, 0xDF, 0xA0,
  0x29, 0x56, 0xD7, 0xA8, 0xAA, 0xD5, 0x54, 0x2B, 0x50, 0x2F, 0xAE, 0xD1, 0xD3, 0xAC, 0x2D, 0x52,
  0x40, 0x3F, 0xBE, 0xC1, 0xC3, 0xBC, 0x3D, 0x42, 0x39, 0x46, 0xC7, 0xB8, 0xBA, 0xC5, 0x44, 0x3B,
  0xB2, 0xCD, 0x4C, 0x33, 0x31, 0x4E, 0xCF, 0xB0, 0xCB, 0xB4, 0x35, 0x4A, 0x48, 0x37, 0xB6, 0xC9
};


//calculate CRC, starting with existing CRC state (initial state at the start of a calculation should be 0)
static uint8_t _I2C_CRC_Accumulate(uint8_t* buf, uint16_t length, uint8_t state) {
  int i;
  for (i = 0; i < length; i++) {
    state = _i2c_crc_table[buf[i] ^ state];
  }
  return state;
}


/*********************************************************/
/*                I2C Hardware Interface                 */
/*********************************************************/

bool I2CHardwareInterface::IsBusy() noexcept {
  return this->active_async_interface != NULL || this->i2c_handle->State != HAL_I2C_STATE_READY;
}


void I2CHardwareInterface::Read(uint8_t i2c_addr, uint8_t reg_addr, uint8_t* buf, uint16_t length) {
  if (buf == NULL || length == 0) {
    throw std::invalid_argument("I2CHardwareInterface transfers require non-null buffer and non-zero length");
  }

  ThrowOnHALErrorMsg(HAL_I2C_Mem_Read(this->i2c_handle, (uint16_t)i2c_addr << 1, (uint16_t)reg_addr, I2C_MEMADD_SIZE_8BIT, buf, length, MODIF_BLOCKING_TIMEOUT_MS), "I2C read");
}

void I2CHardwareInterface::ReadAsync(I2CModuleInterface* interface, uint8_t i2c_addr, uint8_t reg_addr, uint8_t* buf, uint16_t length) {
  if (interface == NULL || buf == NULL || length == 0) {
    throw std::invalid_argument("I2CHardwareInterface async transfers require non-null interface and buffer and non-zero length");
  }

  //perform pre-checks and transfer start under disabled interrupts
  uint32_t primask = __get_PRIMASK();
  __disable_irq();

  if (this->IsBusy()) {
    //already busy: re-enable interrupts and throw exception
    __set_PRIMASK(primask);
    throw DriverError(DRV_BUSY, "I2CHardwareInterface is already busy");
  }

  //remember given interface as active
  this->active_async_interface = interface;

  //start transfer
  try {
    ThrowOnHALErrorMsg(HAL_I2C_Mem_Read_IT(this->i2c_handle, (uint16_t)i2c_addr << 1, (uint16_t)reg_addr, I2C_MEMADD_SIZE_8BIT, buf, length), "I2C async read");
  } catch (...) {
    //on error: clear active interface, re-enable interrupts, then rethrow
    this->active_async_interface = NULL;
    __set_PRIMASK(primask);
    throw;
  }

  __set_PRIMASK(primask);
}


void I2CHardwareInterface::Write(uint8_t i2c_addr, uint8_t reg_addr, const uint8_t* buf, uint16_t length) {
  if (buf == NULL || length == 0) {
    throw std::invalid_argument("I2CHardwareInterface transfers require non-null buffer and non-zero length");
  }

  ThrowOnHALErrorMsg(HAL_I2C_Mem_Write(i2c_handle, (uint16_t)i2c_addr << 1, (uint16_t)reg_addr, I2C_MEMADD_SIZE_8BIT, (uint8_t*)buf, length, MODIF_BLOCKING_TIMEOUT_MS), "I2C write");
}

void I2CHardwareInterface::WriteAsync(I2CModuleInterface* interface, uint8_t i2c_addr, uint8_t reg_addr, const uint8_t* buf, uint16_t length) {
  if (interface == NULL || buf == NULL || length == 0) {
    throw std::invalid_argument("I2CHardwareInterface async transfers require non-null interface and buffer and non-zero length");
  }

  //perform pre-checks and transfer start under disabled interrupts
  uint32_t primask = __get_PRIMASK();
  __disable_irq();

  if (this->IsBusy()) {
    //already busy: re-enable interrupts and throw exception
    __set_PRIMASK(primask);
    throw DriverError(DRV_BUSY, "I2CHardwareInterface is already busy");
  }

  //remember given interface as active
  this->active_async_interface = interface;

  //start transfer
  try {
    ThrowOnHALErrorMsg(HAL_I2C_Mem_Write_IT(this->i2c_handle, (uint16_t)i2c_addr << 1, (uint16_t)reg_addr, I2C_MEMADD_SIZE_8BIT, (uint8_t*)buf, length), "I2C async write");
  } catch (...) {
    //on error: clear active interface, re-enable interrupts, then rethrow
    this->active_async_interface = NULL;
    __set_PRIMASK(primask);
    throw;
  }

  __set_PRIMASK(primask);
}


void I2CHardwareInterface::HandleInterrupt(ModuleInterfaceInterruptType type) noexcept {
  //perform interrupt handling under disabled interrupts
  uint32_t primask = __get_PRIMASK();
  __disable_irq();

  switch (type) {
    case IF_ERROR:
      //reset I2C driver before continuing with end-of-transfer handling
      HAL_I2C_DeInit(this->i2c_handle);
      HAL_I2C_Init(this->i2c_handle);
    case IF_RX_COMPLETE:
    case IF_TX_COMPLETE:
      if (this->active_async_interface != NULL) {
        //we have an active async transfer: pass interrupt to target interface
        this->active_async_interface->HandleInterrupt(type, 0);
        //clear active interface
        this->active_async_interface = NULL;
      }

      //find and start next queued transfer
      for (auto i = this->registered_interfaces.begin(); i < this->registered_interfaces.end(); i++) {
        try {
          (*i)->StartQueuedAsyncTransfer();
        } catch (const std::exception& err) {
          DEBUG_PRINTF("* Exception on I2C async transfer start: %s\n", err.what());
          continue;
        } catch (...) {
          DEBUG_PRINTF("* Unknown exception on I2C async transfer start\n");
          continue;
        }

        if (this->active_async_interface != NULL) {
          //transfer has been started: break out of loop
          break;
        }
      }
      break;
    default:
      break;
  }

  __set_PRIMASK(primask);
}


I2CHardwareInterface::I2CHardwareInterface(I2C_HandleTypeDef* i2c_handle) : i2c_handle(i2c_handle) {
  if (i2c_handle == NULL) {
    throw std::invalid_argument("I2CHardwareInterface I2C handle cannot be null");
  }
}


void I2CHardwareInterface::RegisterInterface(I2CModuleInterface* interface) {
  this->registered_interfaces.push_back(interface);
}

void I2CHardwareInterface::UnregisterInterface(I2CModuleInterface* interface) {
  for (auto i = this->registered_interfaces.begin(); i < this->registered_interfaces.end(); i++) {
    if (*i == interface) {
      this->registered_interfaces.erase(i);
      return;
    }
  }
}


/*********************************************************/
/*            I2C Module Interface - Basics              */
/*********************************************************/

void I2CModuleInterface::ReadRegister(uint8_t reg_addr, uint8_t* buf, uint16_t length) {
  if (buf == NULL || length == 0) {
    throw std::invalid_argument("ModuleInterface transfers require non-null buffer and nonzero length");
  }

  if (this->uses_crc) {
    //CRC mode: allocate new receive buffer with one extra byte for CRC, then receive data
    uint8_t* rx_buf = new uint8_t[length + 1];
    try {
      this->hw_interface.Read(this->i2c_address, reg_addr, rx_buf, length + 1);
    } catch (...) {
      //on error: free receive buffer before rethrowing to avoid memory leak
      delete[] rx_buf;
      throw;
    }

    //calculate CRC checksum (taking into account I2C and register address bytes too)
    uint8_t crc_prefix[3] = { (uint8_t)(this->i2c_address << 1), reg_addr, (uint8_t)((this->i2c_address << 1) | 1) };
    uint8_t crc = _I2C_CRC_Accumulate(crc_prefix, 3, 0);
    crc = _I2C_CRC_Accumulate(rx_buf, length + 1, crc);

    if (crc == 0) {
      //CRC check is good: copy data to output buffer and free receive buffer
      memcpy(buf, rx_buf, length);
      delete[] rx_buf;
    } else {
      //CRC check failed: free receive buffer (without copying) and throw exception
      delete[] rx_buf;
      throw DriverError(DRV_FAILED, "I2CModuleInterface CRC check failed on read");
    }
  } else {
    //no CRC: just do a basic mem read
    this->hw_interface.Read(this->i2c_address, reg_addr, buf, length);
  }
}

void I2CModuleInterface::WriteRegister(uint8_t reg_addr, const uint8_t* buf, uint16_t length) {
  if (buf == NULL || length == 0) {
    throw std::invalid_argument("ModuleInterface transfers require non-null buffer and nonzero length");
  }

  if (this->uses_crc) {
    //CRC mode: allocate new transmit buffer with one extra byte for CRC, and copy data into it
    uint8_t* tx_buf = new uint8_t[length + 1];
    memcpy(tx_buf, buf, length);

    //calculate CRC checksum (taking into account I2C and register address bytes too)
    uint8_t crc_prefix[2] = { (uint8_t)(this->i2c_address << 1), reg_addr };
    uint8_t crc = _I2C_CRC_Accumulate(crc_prefix, 2, 0);
    crc = _I2C_CRC_Accumulate(tx_buf, length, crc);
    //insert CRC sum at end of transmit buffer
    tx_buf[length] = crc;

    //write transmit buffer and free it afterwards
    try {
      this->hw_interface.Write(this->i2c_address, reg_addr, tx_buf, length + 1);
    } catch (...) {
      //on error: free transmit buffer before rethrowing to avoid memory leak
      delete[] tx_buf;
      throw;
    }
    delete[] tx_buf;
  } else {
    //no CRC: just do a basic mem write
    this->hw_interface.Write(this->i2c_address, reg_addr, buf, length);
  }
}


I2CModuleInterface::I2CModuleInterface(I2CHardwareInterface& hw_interface, GPIO_TypeDef* int_port, uint16_t int_pin, uint8_t i2c_address, bool use_crc)
                                      : hw_interface(hw_interface), int_port(int_port), int_pin(int_pin), i2c_address(i2c_address), uses_crc(use_crc) {
  this->hw_interface.RegisterInterface(this);
}

I2CModuleInterface::~I2CModuleInterface() {
  this->ModuleInterface::~ModuleInterface();
  this->hw_interface.UnregisterInterface(this);
}


/*********************************************************/
/*         I2C Module Interface - Multi-Register         */
/*********************************************************/

void I2CModuleInterface::ReadMultiRegister(uint8_t reg_addr_first, uint8_t* buf, const uint16_t* reg_sizes, uint8_t count) {
  if (buf == NULL || reg_sizes == NULL || count == 0) {
    throw std::invalid_argument("I2CModuleInterface multi-transfers require non-null buffer and register sizes and nonzero count");
  }

  //calculate total data length in bytes
  uint16_t total_length = 0;
  for (int i = 0; i < count; i++) {
    if (reg_sizes[i] == 0) {
      throw std::invalid_argument("I2CModuleInterface multi-transfers require nonzero register sizes");
    }
    total_length += reg_sizes[i];
  }

  if (this->uses_crc) {
    //CRC mode: allocate new receive buffer with `count` extra bytes for CRCs, then receive data
    uint8_t* rx_buf = new uint8_t[total_length + count];
    try {
      this->hw_interface.Read(this->i2c_address, reg_addr_first, rx_buf, total_length + count);
    } catch (...) {
      //on error: free receive buffer before rethrowing to avoid memory leak
      delete[] rx_buf;
      throw;
    }

    //calculate first CRC checksum (taking into account I2C and register address bytes too)
    uint16_t reg_size = reg_sizes[0];
    uint8_t crc_prefix[3] = { (uint8_t)(this->i2c_address << 1), reg_addr_first, (uint8_t)((this->i2c_address << 1) | 1) };
    uint8_t crc = _I2C_CRC_Accumulate(crc_prefix, 3, 0);
    crc = _I2C_CRC_Accumulate(rx_buf, reg_size + 1, crc);
    if (crc != 0) {
      //CRC check failed: free receive buffer (without copying) and throw exception
      delete[] rx_buf;
      throw DriverError(DRV_FAILED, "I2CModuleInterface CRC check failed on read");
    }

    //calculate consecutive CRC checksums (without I2C and register address bytes)
    uint16_t rx_buf_offset = reg_size + 1;
    for (int i = 1; i < count; i++) {
      reg_size = reg_sizes[i];
      crc = _I2C_CRC_Accumulate(rx_buf + rx_buf_offset, reg_size + 1, 0);
      if (crc != 0) {
        //CRC check failed: free receive buffer (without copying) and throw exception
        delete[] rx_buf;
        throw DriverError(DRV_FAILED, "I2CModuleInterface CRC check failed on read");
      }
      rx_buf_offset += reg_size + 1;
    }

    //all CRC checks passed: copy actual register data to output, then free receive buffer
    rx_buf_offset = 0;
    uint16_t out_buf_offset = 0;
    for (int i = 0; i < count; i++) {
      reg_size = reg_sizes[i];
      memcpy(buf + out_buf_offset, rx_buf + rx_buf_offset, reg_size);
      rx_buf_offset += reg_size + 1;
      out_buf_offset += reg_size;
    }
    delete[] rx_buf;
  } else {
    //no CRC: just do a basic mem read for all registers
    this->hw_interface.Read(this->i2c_address, reg_addr_first, buf, total_length);
  }
}

void I2CModuleInterface::ReadMultiRegisterAsync(uint8_t reg_addr_first, uint8_t* buf, const uint16_t* reg_sizes, uint8_t count, ModuleTransferCallback cb, uintptr_t context) {

}


void I2CModuleInterface::WriteMultiRegister(uint8_t reg_addr_first, const uint8_t* buf, const uint16_t* reg_sizes, uint8_t count) {
  if (buf == NULL || reg_sizes == NULL || count == 0) {
    throw std::invalid_argument("I2CModuleInterface multi-transfers require non-null buffer and register sizes and nonzero count");
  }

  //calculate total data length in bytes
  uint16_t total_length = 0;
  for (int i = 0; i < count; i++) {
    if (reg_sizes[i] == 0) {
      throw std::invalid_argument("I2CModuleInterface multi-transfers require nonzero register sizes");
    }
    total_length += reg_sizes[i];
  }

  if (this->uses_crc) {
    //CRC mode: allocate new transmit buffer with `count` extra bytes for CRCs
    uint8_t* tx_buf = new uint8_t[total_length + count];

    //copy first register and calculate first CRC checksum (taking into account I2C and register address bytes too)
    uint16_t reg_size = reg_sizes[0];
    memcpy(tx_buf, buf, reg_size);
    uint8_t crc_prefix[2] = { (uint8_t)(this->i2c_address << 1), reg_addr_first };
    uint8_t crc = _I2C_CRC_Accumulate(crc_prefix, 2, 0);
    crc = _I2C_CRC_Accumulate(tx_buf, reg_size, crc);
    //insert CRC sum after the data
    tx_buf[reg_size] = crc;

    //copy consecutive registers and calculate their CRC checksums (without I2C and register address bytes)
    uint16_t tx_buf_offset = reg_size + 1;
    uint16_t in_buf_offset = reg_size;
    for (int i = 1; i < count; i++) {
      reg_size = reg_sizes[i];
      memcpy(tx_buf + tx_buf_offset, buf + in_buf_offset, reg_size);
      crc = _I2C_CRC_Accumulate(tx_buf + tx_buf_offset, reg_size, 0);
      tx_buf[tx_buf_offset + reg_size] = crc;
      tx_buf_offset += reg_size + 1;
      in_buf_offset += reg_size;
    }

    //write transmit buffer and free it afterwards
    try {
      this->hw_interface.Write(this->i2c_address, reg_addr_first, tx_buf, total_length + count);
    } catch (...) {
      //on error: free transmit buffer before rethrowing to avoid memory leak
      delete[] tx_buf;
      throw;
    }
    delete[] tx_buf;
  } else {
    //no CRC: just do a basic mem write for all registers
    this->hw_interface.Write(this->i2c_address, reg_addr_first, buf, total_length);
  }
}

void I2CModuleInterface::WriteMultiRegisterAsync(uint8_t reg_addr_first, const uint8_t* buf, const uint16_t* reg_sizes, uint8_t count, ModuleTransferCallback cb, uintptr_t context) {

}


/*********************************************************/
/*             I2C Module Interface - Async              */
/*********************************************************/

void I2CModuleInterface::HandleInterrupt(ModuleInterfaceInterruptType type, uint16_t extra) noexcept {

}

void I2CModuleInterface::StartQueuedAsyncTransfer() {

}

