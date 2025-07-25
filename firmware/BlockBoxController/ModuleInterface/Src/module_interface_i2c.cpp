/*
 * module_interface_i2c.cpp
 *
 *  Created on: May 9, 2025
 *      Author: Alex
 */


#include "module_interface_i2c.h"


//timeouts for clock extension and low clock, in units of 2048 I2CCLK cycles (not to be confused with SCL cycles!)
//initial values calculated for 8 MHz I2CCLK: stretch timeout 8ms in release, 32ms in debug, low timeout 16ms in release, 64ms in debug
#ifdef DEBUG
#define I2C_SCL_STRETCH_TIMEOUT (127 << I2C_TIMEOUTR_TIMEOUTB_Pos)
#define I2C_SCL_LOW_TIMEOUT (255 << I2C_TIMEOUTR_TIMEOUTA_Pos)
#else
#define I2C_SCL_STRETCH_TIMEOUT (31 << I2C_TIMEOUTR_TIMEOUTB_Pos)
#define I2C_SCL_LOW_TIMEOUT (63 << I2C_TIMEOUTR_TIMEOUTA_Pos)
#endif

//timeout for non-idle state, in main loop cycles
#ifdef DEBUG
#define I2C_NONIDLE_TIMEOUT 40
#else
#define I2C_NONIDLE_TIMEOUT 20
#endif

//timeout for busy I2C peripheral despite idle driver, in main loop cycles
#ifdef DEBUG
#define I2C_PERIPHERAL_BUSY_TIMEOUT 20
#else
#define I2C_PERIPHERAL_BUSY_TIMEOUT 10
#endif

//number of internal retries for failed I2C transfers
#define I2C_INTERNAL_RETRIES 3

//TODO: potential transfer-specific timeout as well?


//async transfer queue item, extended for I2C-specific information
class I2CModuleTransferQueueItem : public ModuleTransferQueueItem {
public:
  uint8_t* add_buffer;

  uint16_t* reg_sizes;
  uint8_t reg_count;

  uint8_t retry_count;

  ~I2CModuleTransferQueueItem() override {
    if (this->add_buffer != NULL) {
      delete[] this->add_buffer;
    }
    if (this->reg_sizes != NULL) {
      delete[] this->reg_sizes;
    }
  }
};


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


/*********************************************************/
/*                I2C Hardware Interface                 */
/*********************************************************/

bool I2CHardwareInterface::IsBusy() noexcept {
  return this->active_async_interface != NULL || this->i2c_handle->State != HAL_I2C_STATE_READY;
}


void I2CHardwareInterface::Read(uint8_t i2c_addr, uint16_t reg_addr, uint16_t reg_addr_size, uint8_t* buf, uint16_t length) {
  if (buf == NULL || length == 0) {
    throw std::invalid_argument("I2CHardwareInterface transfers require non-null buffer and non-zero length");
  }

  ThrowOnHALErrorMsg(HAL_I2C_Mem_Read(this->i2c_handle, (uint16_t)i2c_addr << 1, reg_addr, reg_addr_size, buf, length, MODIF_BLOCKING_TIMEOUT_MS), "I2C read");
}

void I2CHardwareInterface::ReadAsync(I2CModuleInterface* interface, uint16_t reg_addr, uint8_t* buf, uint16_t length) {
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

  //start non-idle timeout
  this->non_idle_timeout = I2C_NONIDLE_TIMEOUT;
  //remember given interface as active
  this->active_async_interface = interface;

  //start transfer
  try {
    ThrowOnHALErrorMsg(HAL_I2C_Mem_Read_IT(this->i2c_handle, (uint16_t)interface->i2c_address << 1, reg_addr, interface->reg_addr_size, buf, length), "I2C async read");
  } catch (...) {
    //on error: clear active interface, re-enable interrupts, then rethrow
    this->active_async_interface = NULL;
    __set_PRIMASK(primask);
    throw;
  }

  __set_PRIMASK(primask);
}


void I2CHardwareInterface::Write(uint8_t i2c_addr, uint16_t reg_addr, uint16_t reg_addr_size, const uint8_t* buf, uint16_t length) {
  if (buf == NULL || length == 0) {
    throw std::invalid_argument("I2CHardwareInterface transfers require non-null buffer and non-zero length");
  }

  ThrowOnHALErrorMsg(HAL_I2C_Mem_Write(i2c_handle, (uint16_t)i2c_addr << 1, (uint16_t)reg_addr, reg_addr_size, (uint8_t*)buf, length, MODIF_BLOCKING_TIMEOUT_MS), "I2C write");
}

void I2CHardwareInterface::WriteAsync(I2CModuleInterface* interface, uint16_t reg_addr, const uint8_t* buf, uint16_t length) {
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

  //start non-idle timeout
  this->non_idle_timeout = I2C_NONIDLE_TIMEOUT;
  //remember given interface as active
  this->active_async_interface = interface;

  //start transfer
  try {
    ThrowOnHALErrorMsg(HAL_I2C_Mem_Write_IT(this->i2c_handle, (uint16_t)interface->i2c_address << 1, reg_addr, interface->reg_addr_size, (uint8_t*)buf, length), "I2C async write");
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
      DEBUG_PRINTF("* I2C error interrupt, code %lu\n", HAL_I2C_GetError(this->i2c_handle));

      if (this->active_async_interface != NULL) {
        //we have an active async transfer: pass interrupt to target interface
        this->active_async_interface->HandleInterrupt(type, 0);
        //clear active interface
        this->active_async_interface = NULL;
      }

      //reset hardware
      this->Reset();
      break;
    case IF_RX_COMPLETE:
    case IF_TX_COMPLETE:
      if (this->active_async_interface != NULL) {
        //we have an active async transfer: pass interrupt to target interface
        this->active_async_interface->HandleInterrupt(type, 0);
        //clear active interface
        this->active_async_interface = NULL;
      }

      //find and start next queued transfer
      this->StartNextTransfer();
      break;
    default:
      break;
  }

  __set_PRIMASK(primask);
}


void I2CHardwareInterface::Init() {
  //shouldn't be clearing interfaces, since they're constructed by now and won't automatically re-register if cleared
  //this->registered_interfaces.clear();

  this->active_async_interface = NULL;
  this->Reset();
}

void I2CHardwareInterface::LoopTasks() {
  //perform timeout checks under disabled interrupts
  uint32_t primask = __get_PRIMASK();
  __disable_irq();

  if (!this->IsBusy() && __HAL_I2C_GET_FLAG(this->i2c_handle, I2C_FLAG_BUSY) == SET) { //driver idle but peripheral busy: check timeout
    if (++this->idle_busy_count > I2C_PERIPHERAL_BUSY_TIMEOUT) { //peripheral busy for too long, reset
      DEBUG_PRINTF("* I2C Error: Peripheral busy in idle state timeout\n");
      this->Reset();
      __set_PRIMASK(primask);
      return;
    }
  } else {
    this->idle_busy_count = 0;
  }

  if (this->non_idle_timeout > 0) { //handle non-idle state timeouts
    if (!this->IsBusy()) { //reset timeout if idle
      this->non_idle_timeout = 0;
    } else {
      if (--this->non_idle_timeout == 0) { //timed out: error
        DEBUG_PRINTF("* I2C Error: Non-idle state timed out\n");
        this->Reset();
        __set_PRIMASK(primask);
        return;
      }
    }
  }

  __set_PRIMASK(primask);
}


I2CHardwareInterface::I2CHardwareInterface(I2C_HandleTypeDef* i2c_handle, void (*hardware_reset_func)()) : i2c_handle(i2c_handle), hardware_reset_func(hardware_reset_func) {
  if (i2c_handle == NULL || hardware_reset_func == NULL) {
    throw std::invalid_argument("I2CHardwareInterface I2C handle and hardware reset func cannot be null");
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


void I2CHardwareInterface::StartNextTransfer() noexcept {
  for (auto i = this->registered_interfaces.begin(); i < this->registered_interfaces.end(); i++) {
    (*i)->StartQueuedAsyncTransfer();

    if (this->active_async_interface != NULL) {
      //transfer has been started: break out of loop
      break;
    }
  }
}

void I2CHardwareInterface::Reset() noexcept {
  //perform reset handling under disabled interrupts
  uint32_t primask = __get_PRIMASK();
  __disable_irq();

  HAL_I2C_DeInit(this->i2c_handle);
  this->hardware_reset_func();
  HAL_I2C_Init(this->i2c_handle);

  //enable I2C timeouts
  WRITE_REG(this->i2c_handle->Instance->TIMEOUTR, (I2C_SCL_STRETCH_TIMEOUT | I2C_SCL_LOW_TIMEOUT));
  SET_BIT(this->i2c_handle->Instance->TIMEOUTR, (I2C_TIMEOUTR_TEXTEN | I2C_TIMEOUTR_TIMOUTEN));

  //reset internal state and timeout counters
  this->active_async_interface = NULL;
  this->idle_busy_count = 0;
  this->non_idle_timeout = 0;

  //reset interfaces (i.e. inform them that no async transfer is ongoing)
  for (auto i = this->registered_interfaces.begin(); i < this->registered_interfaces.end(); i++) {
    (*i)->async_transfer_active = false;
  }

  //find and start next queued transfer
  this->StartNextTransfer();

  __set_PRIMASK(primask);
}


/*********************************************************/
/*       I2C Module Interface - CRC calculations         */
/*********************************************************/

//calculate CRC, starting with existing CRC state (initial state at the start of a calculation should be 0)
static uint8_t _I2C_CRC_Accumulate(const uint8_t* buf, uint16_t length, uint8_t state) noexcept {
  int i;
  for (i = 0; i < length; i++) {
    state = _i2c_crc_table[buf[i] ^ state];
  }
  return state;
}

//check CRC on received data for a single-register read
static bool _I2C_CRC_SingleReadCheck(uint8_t i2c_addr, uint16_t reg_addr, uint16_t reg_addr_size, const uint8_t* rx_buf, uint16_t length) noexcept {
  //calculate CRC checksum (taking into account I2C and register address bytes too)
  uint8_t crc;
  if (reg_addr_size == I2C_MEMADD_SIZE_8BIT) {
    uint8_t crc_prefix[3] = { (uint8_t)(i2c_addr << 1), (uint8_t)reg_addr, (uint8_t)((i2c_addr << 1) | 1) };
    crc = _I2C_CRC_Accumulate(crc_prefix, 3, 0);
  } else {
    uint8_t crc_prefix[4] = { (uint8_t)(i2c_addr << 1), (uint8_t)(reg_addr >> 8), (uint8_t)reg_addr, (uint8_t)((i2c_addr << 1) | 1) };
    crc = _I2C_CRC_Accumulate(crc_prefix, 4, 0);
  }
  crc = _I2C_CRC_Accumulate(rx_buf, length + 1, crc);
  return crc == 0;
}

//check CRCs on received data for a multi-register read
static bool _I2C_CRC_MultiReadCheck(uint8_t i2c_addr, uint16_t reg_addr_first, uint16_t reg_addr_size, const uint8_t* rx_buf, const uint16_t* reg_sizes, uint8_t count) noexcept {
  //calculate first CRC checksum (taking into account I2C and register address bytes too)
  uint16_t reg_size = reg_sizes[0];
  uint8_t crc;
  if (reg_addr_size == I2C_MEMADD_SIZE_8BIT) {
    uint8_t crc_prefix[3] = { (uint8_t)(i2c_addr << 1), (uint8_t)reg_addr_first, (uint8_t)((i2c_addr << 1) | 1) };
    crc = _I2C_CRC_Accumulate(crc_prefix, 3, 0);
  } else {
    uint8_t crc_prefix[4] = { (uint8_t)(i2c_addr << 1), (uint8_t)(reg_addr_first >> 8), (uint8_t)reg_addr_first, (uint8_t)((i2c_addr << 1) | 1) };
    crc = _I2C_CRC_Accumulate(crc_prefix, 4, 0);
  }
  crc = _I2C_CRC_Accumulate(rx_buf, reg_size + 1, crc);
  if (crc != 0) {
    return false;
  }

  //calculate consecutive CRC checksums (without I2C and register address bytes)
  uint16_t rx_buf_offset = reg_size + 1;
  for (int i = 1; i < count; i++) {
    reg_size = reg_sizes[i];
    crc = _I2C_CRC_Accumulate(rx_buf + rx_buf_offset, reg_size + 1, 0);
    if (crc != 0) {
      return false;
    }
    rx_buf_offset += reg_size + 1;
  }

  return true;
}

//prepare a newly allocated transmit buffer with the correct CRC for a single-register write
static uint8_t* _I2C_CRC_PrepareSingleWrite(uint8_t i2c_addr, uint16_t reg_addr, uint16_t reg_addr_size, const uint8_t* buf, uint16_t length) {
  //allocate new transmit buffer with one extra byte for CRC, and copy data into it
  uint8_t* tx_buf = new uint8_t[length + 1];
  memcpy(tx_buf, buf, length);

  //calculate CRC checksum (taking into account I2C and register address bytes too)
  uint8_t crc;
  if (reg_addr_size == I2C_MEMADD_SIZE_8BIT) {
    uint8_t crc_prefix[2] = { (uint8_t)(i2c_addr << 1), (uint8_t)reg_addr };
    crc = _I2C_CRC_Accumulate(crc_prefix, 2, 0);
  } else {
    uint8_t crc_prefix[3] = { (uint8_t)(i2c_addr << 1), (uint8_t)(reg_addr >> 8), (uint8_t)reg_addr };
    crc = _I2C_CRC_Accumulate(crc_prefix, 3, 0);
  }
  crc = _I2C_CRC_Accumulate(tx_buf, length, crc);
  //insert CRC sum at end of transmit buffer
  tx_buf[length] = crc;

  return tx_buf;
}

//prepare a newly allocated transmit buffer with the correct CRCs for a multi-register write
static uint8_t* _I2C_CRC_PrepareMultiWrite(uint8_t i2c_addr, uint16_t reg_addr_first, uint16_t reg_addr_size, const uint8_t* buf, const uint16_t* reg_sizes, uint8_t count, uint16_t total_length) {
  //allocate new transmit buffer with `count` extra bytes for CRCs
  uint8_t* tx_buf = new uint8_t[total_length + count];

  //copy first register and calculate first CRC checksum (taking into account I2C and register address bytes too)
  uint16_t reg_size = reg_sizes[0];
  memcpy(tx_buf, buf, reg_size);
  uint8_t crc;
  if (reg_addr_size == I2C_MEMADD_SIZE_8BIT) {
    uint8_t crc_prefix[2] = { (uint8_t)(i2c_addr << 1), (uint8_t)reg_addr_first };
    crc = _I2C_CRC_Accumulate(crc_prefix, 2, 0);
  } else {
    uint8_t crc_prefix[3] = { (uint8_t)(i2c_addr << 1), (uint8_t)(reg_addr_first >> 8), (uint8_t)reg_addr_first };
    crc = _I2C_CRC_Accumulate(crc_prefix, 3, 0);
  }
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

  return tx_buf;
}


/*********************************************************/
/*            I2C Module Interface - Basics              */
/*********************************************************/

void I2CModuleInterface::ReadRegister(uint16_t reg_addr, uint8_t* buf, uint16_t length) {
  if (buf == NULL || length == 0) {
    throw std::invalid_argument("ModuleInterface transfers require non-null buffer and nonzero length");
  }

  if (this->uses_crc) {
    //CRC mode: allocate new receive buffer with one extra byte for CRC, then receive data
    uint8_t* rx_buf = new uint8_t[length + 1];
    try {
      this->hw_interface.Read(this->i2c_address, reg_addr, this->reg_addr_size, rx_buf, length + 1);
    } catch (...) {
      //on error: free receive buffer before rethrowing to avoid memory leak
      delete[] rx_buf;
      throw;
    }

    if (_I2C_CRC_SingleReadCheck(this->i2c_address, reg_addr, this->reg_addr_size, rx_buf, length)) {
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
    this->hw_interface.Read(this->i2c_address, reg_addr, this->reg_addr_size, buf, length);
  }

  //handle data update
  this->HandleDataUpdate(reg_addr, buf, length);
}

void I2CModuleInterface::WriteRegister(uint16_t reg_addr, const uint8_t* buf, uint16_t length) {
  if (buf == NULL || length == 0) {
    throw std::invalid_argument("ModuleInterface transfers require non-null buffer and nonzero length");
  }

  if (this->uses_crc) {
    //CRC mode: prepare transmit buffer with CRC
    uint8_t* tx_buf = _I2C_CRC_PrepareSingleWrite(this->i2c_address, reg_addr, this->reg_addr_size, buf, length);

    //write transmit buffer and free it afterwards
    try {
      this->hw_interface.Write(this->i2c_address, reg_addr, this->reg_addr_size, tx_buf, length + 1);
    } catch (...) {
      //on error: free transmit buffer before rethrowing to avoid memory leak
      delete[] tx_buf;
      throw;
    }
    delete[] tx_buf;
  } else {
    //no CRC: just do a basic mem write
    this->hw_interface.Write(this->i2c_address, reg_addr, this->reg_addr_size, buf, length);
  }

  //handle data update
  this->HandleDataUpdate(reg_addr, buf, length);
}


I2CModuleInterface::I2CModuleInterface(I2CHardwareInterface& hw_interface, uint8_t i2c_address, uint16_t reg_addr_size, bool use_crc) :
    hw_interface(hw_interface), i2c_address(i2c_address), reg_addr_size(reg_addr_size), uses_crc(use_crc) {
  if (this->reg_addr_size != I2C_MEMADD_SIZE_8BIT && this->reg_addr_size != I2C_MEMADD_SIZE_16BIT) {
    //invalid address size
    throw std::invalid_argument("I2CModuleInterface register address size invalid, must be either I2C_MEMADD_SIZE_8BIT or I2C_MEMADD_SIZE_16BIT");
  }

  this->hw_interface.RegisterInterface(this);
}

I2CModuleInterface::~I2CModuleInterface() {
  this->hw_interface.UnregisterInterface(this);
}


/*********************************************************/
/*         I2C Module Interface - Multi-Register         */
/*********************************************************/

void I2CModuleInterface::ReadMultiRegister(uint16_t reg_addr_first, uint8_t* buf, const uint16_t* reg_sizes, uint16_t count) {
  if (buf == NULL || reg_sizes == NULL || count == 0 || (uint32_t)reg_addr_first + (uint32_t)count > (this->reg_addr_size == I2C_MEMADD_SIZE_8BIT ? UINT8_MAX : UINT16_MAX)) {
    throw std::invalid_argument("I2CModuleInterface multi-transfers require non-null buffer and register sizes, nonzero count, and cannot go above register 255");
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
      this->hw_interface.Read(this->i2c_address, reg_addr_first, this->reg_addr_size, rx_buf, total_length + count);
    } catch (...) {
      //on error: free receive buffer before rethrowing to avoid memory leak
      delete[] rx_buf;
      throw;
    }

    if (!_I2C_CRC_MultiReadCheck(this->i2c_address, reg_addr_first, this->reg_addr_size, rx_buf, reg_sizes, count)) {
      //CRC check failed: free receive buffer (without copying) and throw exception
      delete[] rx_buf;
      throw DriverError(DRV_FAILED, "I2CModuleInterface CRC check failed on read");
    }

    //all CRC checks passed: copy actual register data to output, then free receive buffer
    uint16_t rx_buf_offset = 0;
    uint16_t out_buf_offset = 0;
    uint16_t reg_size;
    for (int i = 0; i < count; i++) {
      reg_size = reg_sizes[i];
      memcpy(buf + out_buf_offset, rx_buf + rx_buf_offset, reg_size);
      rx_buf_offset += reg_size + 1;
      out_buf_offset += reg_size;
    }
    delete[] rx_buf;
  } else {
    //no CRC: just do a basic mem read for all registers
    this->hw_interface.Read(this->i2c_address, reg_addr_first, this->reg_addr_size, buf, total_length);
  }

  //handle data updates
  uint16_t reg_offset = 0;
  uint16_t reg_size;
  for (int i = 0; i < count; i++) {
    reg_size = reg_sizes[i];
    this->HandleDataUpdate(reg_addr_first + i, buf + reg_offset, reg_size);
    reg_offset += reg_size;
  }
}

void I2CModuleInterface::WriteMultiRegister(uint16_t reg_addr_first, const uint8_t* buf, const uint16_t* reg_sizes, uint16_t count) {
  if (buf == NULL || reg_sizes == NULL || count == 0 || (uint32_t)reg_addr_first + (uint32_t)count > (this->reg_addr_size == I2C_MEMADD_SIZE_8BIT ? UINT8_MAX : UINT16_MAX)) {
    throw std::invalid_argument("I2CModuleInterface multi-transfers require non-null buffer and register sizes, nonzero count, and cannot go above register 255");
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
    //CRC mode: prepare new transmit buffer with CRCs
    uint8_t* tx_buf = _I2C_CRC_PrepareMultiWrite(this->i2c_address, reg_addr_first, this->reg_addr_size, buf, reg_sizes, count, total_length);

    //write transmit buffer and free it afterwards
    try {
      this->hw_interface.Write(this->i2c_address, reg_addr_first, this->reg_addr_size, tx_buf, total_length + count);
    } catch (...) {
      //on error: free transmit buffer before rethrowing to avoid memory leak
      delete[] tx_buf;
      throw;
    }
    delete[] tx_buf;
  } else {
    //no CRC: just do a basic mem write for all registers
    this->hw_interface.Write(this->i2c_address, reg_addr_first, this->reg_addr_size, buf, total_length);
  }

  //handle data updates
  uint16_t reg_offset = 0;
  uint16_t reg_size;
  for (int i = 0; i < count; i++) {
    reg_size = reg_sizes[i];
    this->HandleDataUpdate(reg_addr_first + i, buf + reg_offset, reg_size);
    reg_offset += reg_size;
  }
}


static inline void _I2CModuleInterface_QueueMultiTransfer(std::deque<ModuleTransferQueueItem*>& queue, ModuleTransferType type, uint16_t reg_addr_first, uint16_t reg_addr_size, uint8_t* buf,
                                                          const uint16_t* reg_sizes, uint16_t count, ModuleTransferCallback&& callback) {
  if (buf == NULL || reg_sizes == NULL || count == 0 || (uint32_t)reg_addr_first + (uint32_t)count > (reg_addr_size == I2C_MEMADD_SIZE_8BIT ? UINT8_MAX : UINT16_MAX)) {
    throw std::invalid_argument("I2CModuleInterface multi-transfers require non-null buffer and register sizes, nonzero count, and cannot go above register 255");
  }

  //calculate total data length in bytes
  uint16_t total_length = 0;
  for (int i = 0; i < count; i++) {
    if (reg_sizes[i] == 0) {
      throw std::invalid_argument("I2CModuleInterface multi-transfers require nonzero register sizes");
    }
    total_length += reg_sizes[i];
  }

  //initialise new transfer for the read operation
  auto new_transfer = new I2CModuleTransferQueueItem;
  new_transfer->type = type;
  new_transfer->reg_addr = reg_addr_first;
  new_transfer->data_ptr = buf;
  new_transfer->length = total_length;
  new_transfer->value_buffer = 0;
  new_transfer->success = false;
  new_transfer->callback = std::move(callback);
  new_transfer->add_buffer = NULL;
  new_transfer->reg_count = count;
  new_transfer->retry_count = 0;

  //copy register sizes to dynamically allocated buffer (so that given argument buffer doesn't need to remain valid)
  try {
    auto reg_sizes_copy = new uint16_t[count];
    memcpy(reg_sizes_copy, reg_sizes, count * sizeof(uint16_t));
    new_transfer->reg_sizes = reg_sizes_copy;
  } catch (...) {
    //on error: free transfer item before rethrowing
    delete new_transfer;
    throw;
  }

  //add transfer to queue
  try {
    queue.push_back(new_transfer);
  } catch (...) {
    //on error: free transfer item before rethrowing
    delete new_transfer;
    throw;
  }
}

void I2CModuleInterface::ReadMultiRegisterAsync(uint16_t reg_addr_first, uint8_t* buf, const uint16_t* reg_sizes, uint16_t count, ModuleTransferCallback&& callback) {
  _I2CModuleInterface_QueueMultiTransfer(this->queued_transfers, TF_READ, reg_addr_first, this->reg_addr_size, buf, reg_sizes, count, std::move(callback));
  this->StartQueuedAsyncTransfer();
}

void I2CModuleInterface::WriteMultiRegisterAsync(uint16_t reg_addr_first, const uint8_t* buf, const uint16_t* reg_sizes, uint16_t count, ModuleTransferCallback&& callback) {
  _I2CModuleInterface_QueueMultiTransfer(this->queued_transfers, TF_WRITE, reg_addr_first, this->reg_addr_size, (uint8_t*)buf, reg_sizes, count, std::move(callback));
  this->StartQueuedAsyncTransfer();
}


/*********************************************************/
/*             I2C Module Interface - Async              */
/*********************************************************/

ModuleTransferQueueItem* I2CModuleInterface::CreateTransferQueueItem() {
  //initialise to empty defaults
  auto item = new I2CModuleTransferQueueItem;
  item->add_buffer = NULL;
  item->reg_sizes = NULL;
  item->reg_count = 1;
  item->retry_count = 0;
  return item;
}


void I2CModuleInterface::HandleAsyncTransferDone(ModuleInterfaceInterruptType itype) noexcept {
  if (!this->async_transfer_active || this->queued_transfers.empty()) {
    //no active transfer or empty queue: nothing to do
    this->async_transfer_active = false;
    return;
  }

  //whether the transfer should be retried if it failed
  bool retry = true;

  auto transfer = (I2CModuleTransferQueueItem*)this->queued_transfers.front();

  if (itype == IF_RX_COMPLETE && transfer->type == TF_READ) {
    //successful read: handle CRC if necessary
    if (this->uses_crc) {
      //CRC mode: derive success from CRC check and copy data
      if (transfer->data_ptr == NULL || transfer->add_buffer == NULL) {
        //no target or receive buffer: should never be possible!
        DEBUG_PRINTF("* I2C async receive completed, but data_ptr and/or add_buffer is null!\n");
        transfer->success = false;
        retry = false;
      } else if (transfer->reg_sizes == NULL) {
        //single read
        transfer->success = _I2C_CRC_SingleReadCheck(this->i2c_address, transfer->reg_addr, this->reg_addr_size, transfer->add_buffer, transfer->length);
        if (transfer->success) {
          //copy data if CRC matches
          memcpy(transfer->data_ptr, transfer->add_buffer, transfer->length);
        }
      } else {
        //multi-read
        transfer->success = _I2C_CRC_MultiReadCheck(this->i2c_address, transfer->reg_addr, this->reg_addr_size, transfer->add_buffer, transfer->reg_sizes, transfer->reg_count);
        if (transfer->success) {
          //copy data for each register if CRCs match
          uint16_t rx_buf_offset = 0;
          uint16_t out_buf_offset = 0;
          uint16_t reg_size;
          for (int i = 0; i < transfer->reg_count; i++) {
            reg_size = transfer->reg_sizes[i];
            memcpy(transfer->data_ptr + out_buf_offset, transfer->add_buffer + rx_buf_offset, reg_size);
            rx_buf_offset += reg_size + 1;
            out_buf_offset += reg_size;
          }
        }
      }
    } else {
      //no CRC: assume success
      transfer->success = true;
    }
  } else if (itype == IF_TX_COMPLETE && transfer->type == TF_WRITE) {
    //successful write
    transfer->success = true;
  } else {
    //anything else: report failure
    transfer->success = false;
  }

  //in case of successful transfer, handle data update(s)
  if (transfer->success) {
    if (transfer->reg_count < 2 || transfer->reg_sizes == NULL) {
      //single transfer
      this->HandleDataUpdate(transfer->reg_addr, transfer->data_ptr, transfer->length);
    } else {
      //multi-transfer
      uint16_t reg_offset = 0;
      uint16_t reg_size;
      for (int i = 0; i < transfer->reg_count; i++) {
        reg_size = transfer->reg_sizes[i];
        this->HandleDataUpdate(transfer->reg_addr + i, transfer->data_ptr + reg_offset, reg_size);
        reg_offset += reg_size;
      }
    }
  }

  if (!transfer->success && retry) {
    //failed with possibility of retry: increment retry counter, check if we're out of retries
    if (transfer->retry_count++ >= I2C_INTERNAL_RETRIES) {
      //out of retries: don't retry again
      retry = false;
      DEBUG_PRINTF("* I2CModuleInterface async transfer failed to complete too many times to retry!\n");
    } else {
      DEBUG_PRINTF("I2CModuleInterface async transfer retrying on completion\n");
    }
  }

  if (transfer->success || !retry) {
    //successful, or failed without retry: put transfer into the completed list and remove it from the queue
    try {
      this->completed_transfers.push_back(transfer);
      this->queued_transfers.pop_front();
    } catch (...) {
      //list operation failed: force a retry (should be a very unlikely case)
      DEBUG_PRINTF("*** I2CModuleInterface retry forced due to exception when trying to finish a transfer!\n");
    }
  }

  //async transfer is no longer active, ready for the next (or retry of current one)
  this->async_transfer_active = false;
}


void I2CModuleInterface::HandleInterrupt(ModuleInterfaceInterruptType type, uint16_t extra) noexcept {
  UNUSED(extra);

  //perform interrupt handling under disabled interrupts
  uint32_t primask = __get_PRIMASK();
  __disable_irq();

  switch (type) {
    case IF_ERROR:
      this->ExecuteCallbacks(MODIF_EVENT_ERROR);
    case IF_RX_COMPLETE:
    case IF_TX_COMPLETE:
      this->HandleAsyncTransferDone(type);
      break;
    case IF_EXTI:
      //just run callbacks here - basic interface isn't inherently interrupt-enabled
      this->ExecuteCallbacks(MODIF_EVENT_INTERRUPT);
      break;
    default:
      break;
  }

  __set_PRIMASK(primask);
}

void I2CModuleInterface::StartQueuedAsyncTransfer() noexcept {
  //perform start checks and handling under disabled interrupts
  uint32_t primask = __get_PRIMASK();
  __disable_irq();

  if (this->async_transfer_active || this->hw_interface.IsBusy() || this->queued_transfers.empty()) {
    //busy or nothing queued: don't do anything
    __set_PRIMASK(primask);
    return;
  }

  //whether the transfer should be retried if it failed
  bool retry = true;
  //whether a transfer failure should reset the I2C hardware
  bool reset_on_fail = false;

  auto transfer = (I2CModuleTransferQueueItem*)this->queued_transfers.front();

  try {
    try {
      switch (transfer->type) {
        case TF_READ:
          if (this->uses_crc) {
            //CRC mode: allocate new receive buffer with `reg_count` extra bytes for CRCs
            uint8_t* rx_buf = new uint8_t[transfer->length + transfer->reg_count];
            //start the async read
            try {
              //reset if we encounter a hardware exception
              reset_on_fail = true;
              this->hw_interface.ReadAsync(this, transfer->reg_addr, rx_buf, transfer->length + transfer->reg_count);
              transfer->add_buffer = rx_buf;
            } catch (...) {
              //on error: free receive buffer before rethrowing to avoid memory leak
              delete[] rx_buf;
              throw;
            }
          } else {
            //no CRC: just start a basic async mem read
            //reset if we encounter a hardware exception
            reset_on_fail = true;
            this->hw_interface.ReadAsync(this, transfer->reg_addr, transfer->data_ptr, transfer->length);
          }
          break;
        case TF_WRITE:
          if (this->uses_crc) {
            //CRC mode: prepare new transmit buffer with CRCs
            uint8_t* tx_buf;
            if (transfer->reg_sizes == NULL) {
              //single write
              tx_buf = _I2C_CRC_PrepareSingleWrite(this->i2c_address, transfer->reg_addr, this->reg_addr_size, transfer->data_ptr, transfer->length);
            } else {
              //multi-write
              tx_buf = _I2C_CRC_PrepareMultiWrite(this->i2c_address, transfer->reg_addr, this->reg_addr_size, transfer->data_ptr, transfer->reg_sizes, transfer->reg_count, transfer->length);
            }
            //start the async write
            try {
              //reset if we encounter a hardware exception
              reset_on_fail = true;
              this->hw_interface.WriteAsync(this, transfer->reg_addr, tx_buf, transfer->length + transfer->reg_count);
              transfer->add_buffer = tx_buf;
            } catch (...) {
              //on error: free transmit buffer before rethrowing to avoid memory leak
              delete[] tx_buf;
              throw;
            }
          } else {
            //no CRC: just start a basic async mem transfer
            //reset if we encounter a hardware exception
            reset_on_fail = true;
            this->hw_interface.WriteAsync(this, transfer->reg_addr, transfer->data_ptr, transfer->length);
          }
          break;
        default:
          retry = false;
          InlineFormat(throw std::runtime_error(__msg), "I2CModuleInterface async transfer with invalid type %u", transfer->type);
      }

      this->async_transfer_active = true;
    } catch (const std::exception& exc) {
      //encountered "readable" exception: just print the message and rethrow for the outer handler
      DEBUG_PRINTF("* I2CModuleInterface async transfer start exception: %s\n", exc.what());
      throw;
    }
  } catch (...) {
    if (retry) {
      //failed with possibility of retry: increment retry counter, check if we're out of retries
      if (transfer->retry_count++ >= I2C_INTERNAL_RETRIES) {
        //out of retries: don't retry again
        retry = false;
      }
    }

    if (!retry) {
      //failed without retry: put transfer into the completed list and remove it from the queue
      DEBUG_PRINTF("* I2CModuleInterface async transfer failed to start too many times to retry!\n");
      transfer->success = false;
      try {
        this->completed_transfers.push_back(transfer);
        this->queued_transfers.pop_front();
      } catch (...) {
        //list operation failed: force another retry (should be a very unlikely case)
        DEBUG_PRINTF("*** Retry forced due to exception when trying to mark the failed transfer as done!\n");
      }
    } else {
      DEBUG_PRINTF("I2CModuleInterface async transfer retrying on start\n");
    }

    if (reset_on_fail) {
      this->hw_interface.Reset();
    }
  }

  __set_PRIMASK(primask);
}


void I2CModuleInterface::HandleDataUpdate(uint16_t reg_addr, const uint8_t* buf, uint16_t length) noexcept {
  //nothing to do in base class implementation
  UNUSED(reg_addr);
  UNUSED(buf);
  UNUSED(length);
}



/*********************************************************/
/*          Reg I2C Module Interface - Helpers           */
/*********************************************************/

uint16_t RegI2CModuleInterface::GetRegisterSize(uint8_t reg_addr) {
  uint16_t size = this->registers.reg_sizes[reg_addr];

  if (size == 0) {
    //invalid register
    throw std::invalid_argument("RegI2CModuleInterface transfers require all involved registers to be valid");
  }

  return size;
}

const uint16_t* RegI2CModuleInterface::GetMultiRegisterSizes(uint8_t reg_addr_first, uint8_t count) {
  //check for at least 1 register and potential overrun
  if (count == 0 || (uint16_t)reg_addr_first + (uint16_t)count > 255) {
    throw std::invalid_argument("RegI2CModuleInterface multi-transfers require nonzero count and cannot go above register 255");
  }

  //check validity of involved registers
  for (uint8_t i = 0; i < count; i++) {
    if (this->registers.reg_sizes[reg_addr_first + i] == 0) {
      //range includes invalid register
      throw std::invalid_argument("RegI2CModuleInterface transfers require all involved registers to be valid");
    }
  }

  //return direct pointer to register sizes
  return this->registers.reg_sizes + reg_addr_first;
}


/*********************************************************/
/*       Reg I2C Module Interface - Basics Shadows       */
/*********************************************************/

void RegI2CModuleInterface::ReadRegister(uint8_t reg_addr, uint8_t* buf) {
  uint16_t length = this->GetRegisterSize(reg_addr);
  this->I2CModuleInterface::ReadRegister(reg_addr, buf, length);
}

uint8_t RegI2CModuleInterface::ReadRegister8(uint8_t reg_addr) {
  uint16_t length = this->GetRegisterSize(reg_addr);

  if (length == 1) {
    return this->I2CModuleInterface::ReadRegister8(reg_addr);
  } else {
    throw std::invalid_argument("RegI2CModuleInterface attempted to read register of wrong size (function expects 8-bit)");
  }
}

uint16_t RegI2CModuleInterface::ReadRegister16(uint8_t reg_addr) {
  uint16_t length = this->GetRegisterSize(reg_addr);

  if (length == 2) {
    return this->I2CModuleInterface::ReadRegister16(reg_addr);
  } else {
    throw std::invalid_argument("RegI2CModuleInterface attempted to read register of wrong size (function expects 16-bit)");
  }
}

uint32_t RegI2CModuleInterface::ReadRegister32(uint8_t reg_addr) {
  uint16_t length = this->GetRegisterSize(reg_addr);

  if (length == 4) {
    return this->I2CModuleInterface::ReadRegister32(reg_addr);
  } else if (length == 3) {
    //support 24-bit registers in this call too
    return this->I2CModuleInterface::ReadRegister32(reg_addr) & 0xFFFFFF;
  } else {
    throw std::invalid_argument("RegI2CModuleInterface attempted to read register of wrong size (function expects 24-bit or 32-bit)");
  }
}


void RegI2CModuleInterface::ReadRegisterAsync(uint8_t reg_addr, uint8_t* buf, ModuleTransferCallback&& callback) {
  uint16_t length = this->GetRegisterSize(reg_addr);
  this->I2CModuleInterface::ReadRegisterAsync(reg_addr, buf, length, std::move(callback));
}

void RegI2CModuleInterface::ReadRegister8Async(uint8_t reg_addr, ModuleTransferCallback&& callback) {
  uint16_t length = this->GetRegisterSize(reg_addr);

  if (length == 1) {
    this->I2CModuleInterface::ReadRegister8Async(reg_addr, std::move(callback));
  } else {
    throw std::invalid_argument("RegI2CModuleInterface attempted to read register of wrong size (function expects 8-bit)");
  }
}

void RegI2CModuleInterface::ReadRegister16Async(uint8_t reg_addr, ModuleTransferCallback&& callback) {
  uint16_t length = this->GetRegisterSize(reg_addr);

  if (length == 2) {
    return this->I2CModuleInterface::ReadRegister16Async(reg_addr, std::move(callback));
  } else {
    throw std::invalid_argument("RegI2CModuleInterface attempted to read register of wrong size (function expects 16-bit)");
  }
}

void RegI2CModuleInterface::ReadRegister32Async(uint8_t reg_addr, ModuleTransferCallback&& callback) {
  uint16_t length = this->GetRegisterSize(reg_addr);

  //TODO no support for 24-bit registers in async (not easily doable with provided base functions) - implement later if required
  if (length == 4) {
    return this->I2CModuleInterface::ReadRegister32Async(reg_addr, std::move(callback));
  } else {
    throw std::invalid_argument("RegI2CModuleInterface attempted to read register of wrong size (function expects 32-bit)");
  }
}


void RegI2CModuleInterface::WriteRegister(uint8_t reg_addr, const uint8_t* buf) {
  uint16_t length = this->GetRegisterSize(reg_addr);
  this->I2CModuleInterface::WriteRegister(reg_addr, buf, length);
}

void RegI2CModuleInterface::WriteRegister8(uint8_t reg_addr, uint8_t value) {
  uint16_t length = this->GetRegisterSize(reg_addr);

  if (length == 1) {
    this->I2CModuleInterface::WriteRegister8(reg_addr, value);
  } else {
    throw std::invalid_argument("RegI2CModuleInterface attempted to write register of wrong size (function expects 8-bit)");
  }
}

void RegI2CModuleInterface::WriteRegister16(uint8_t reg_addr, uint16_t value) {
  uint16_t length = this->GetRegisterSize(reg_addr);

  if (length == 2) {
    this->I2CModuleInterface::WriteRegister16(reg_addr, value);
  } else {
    throw std::invalid_argument("RegI2CModuleInterface attempted to write register of wrong size (function expects 16-bit)");
  }
}

void RegI2CModuleInterface::WriteRegister32(uint8_t reg_addr, uint32_t value) {
  uint16_t length = this->GetRegisterSize(reg_addr);

  if (length == 4) {
    this->I2CModuleInterface::WriteRegister32(reg_addr, value);
  } else if (length == 3) {
    //support 24-bit registers in this call too
    this->I2CModuleInterface::WriteRegister32(reg_addr, value & 0xFFFFFF);
  } else {
    throw std::invalid_argument("RegI2CModuleInterface attempted to write register of wrong size (function expects 24-bit or 32-bit)");
  }
}


void RegI2CModuleInterface::WriteRegisterAsync(uint8_t reg_addr, const uint8_t* buf, ModuleTransferCallback&& callback) {
  uint16_t length = this->GetRegisterSize(reg_addr);
  this->I2CModuleInterface::WriteRegisterAsync(reg_addr, buf, length, std::move(callback));
}

void RegI2CModuleInterface::WriteRegister8Async(uint8_t reg_addr, uint8_t value, ModuleTransferCallback&& callback) {
  uint16_t length = this->GetRegisterSize(reg_addr);

  if (length == 1) {
    this->I2CModuleInterface::WriteRegister8Async(reg_addr, value, std::move(callback));
  } else {
    throw std::invalid_argument("RegI2CModuleInterface attempted to write register of wrong size (function expects 8-bit)");
  }
}

void RegI2CModuleInterface::WriteRegister16Async(uint8_t reg_addr, uint16_t value, ModuleTransferCallback&& callback) {
  uint16_t length = this->GetRegisterSize(reg_addr);

  if (length == 2) {
    this->I2CModuleInterface::WriteRegister16Async(reg_addr, value, std::move(callback));
  } else {
    throw std::invalid_argument("RegI2CModuleInterface attempted to write register of wrong size (function expects 16-bit)");
  }
}

void RegI2CModuleInterface::WriteRegister32Async(uint8_t reg_addr, uint32_t value, ModuleTransferCallback&& callback) {
  uint16_t length = this->GetRegisterSize(reg_addr);

  //TODO no support for 24-bit registers in async (not easily doable with provided base functions) - implement later if required
  if (length == 4) {
    this->I2CModuleInterface::WriteRegister32Async(reg_addr, value, std::move(callback));
  } else {
    throw std::invalid_argument("RegI2CModuleInterface attempted to write register of wrong size (function expects 32-bit)");
  }
}


/*********************************************************/
/*   Reg I2C Module Interface - Multi-Register Shadows   */
/*********************************************************/

void RegI2CModuleInterface::ReadMultiRegister(uint8_t reg_addr_first, uint8_t* buf, uint8_t count) {
  const uint16_t* reg_sizes = this->GetMultiRegisterSizes(reg_addr_first, count);
  this->I2CModuleInterface::ReadMultiRegister(reg_addr_first, buf, reg_sizes, count);
}

void RegI2CModuleInterface::ReadMultiRegisterAsync(uint8_t reg_addr_first, uint8_t* buf, uint8_t count, ModuleTransferCallback&& callback) {
  const uint16_t* reg_sizes = this->GetMultiRegisterSizes(reg_addr_first, count);
  this->I2CModuleInterface::ReadMultiRegisterAsync(reg_addr_first, buf, reg_sizes, count, std::move(callback));
}


void RegI2CModuleInterface::WriteMultiRegister(uint8_t reg_addr_first, const uint8_t* buf, uint8_t count) {
  const uint16_t* reg_sizes = this->GetMultiRegisterSizes(reg_addr_first, count);
  this->I2CModuleInterface::WriteMultiRegister(reg_addr_first, buf, reg_sizes, count);
}

void RegI2CModuleInterface::WriteMultiRegisterAsync(uint8_t reg_addr_first, const uint8_t* buf, uint8_t count, ModuleTransferCallback&& callback) {
  const uint16_t* reg_sizes = this->GetMultiRegisterSizes(reg_addr_first, count);
  this->I2CModuleInterface::WriteMultiRegisterAsync(reg_addr_first, buf, reg_sizes, count, std::move(callback));
}


/*********************************************************/
/*     Reg I2C Module Interface - Register Handling      */
/*********************************************************/

RegI2CModuleInterface::RegI2CModuleInterface(I2CHardwareInterface& hw_interface, uint8_t i2c_address, const uint16_t* reg_sizes, bool use_crc) :
    I2CModuleInterface(hw_interface, i2c_address, I2C_MEMADD_SIZE_8BIT, use_crc), registers(this->_registers), _registers(reg_sizes) {}

RegI2CModuleInterface::RegI2CModuleInterface(I2CHardwareInterface& hw_interface, uint8_t i2c_address, std::initializer_list<uint16_t> reg_sizes, bool use_crc) :
    I2CModuleInterface(hw_interface, i2c_address, I2C_MEMADD_SIZE_8BIT, use_crc), registers(this->_registers), _registers(reg_sizes) {}


void RegI2CModuleInterface::HandleDataUpdate(uint16_t reg_addr, const uint8_t* buf, uint16_t length) noexcept {
  //allow base handling
  this->I2CModuleInterface::HandleDataUpdate(reg_addr, buf, length);

  uint8_t reg_addr_8 = (uint8_t)reg_addr;

  //ensure that the register is valid and the length matches
  if (this->_registers.reg_sizes[reg_addr_8] > 0 && length == this->_registers.reg_sizes[reg_addr_8]) {
    //copy notification data to the corresponding register
    memcpy(this->_registers[reg_addr_8], buf, length);
    this->OnRegisterUpdate(reg_addr_8);
  } else {
    //invalid register or length mismatch
    DEBUG_PRINTF("* I2C register notification for address 0x%02X length %u, expected %u\n", reg_addr_8, length, this->_registers.reg_sizes[reg_addr_8]);
  }
}

void RegI2CModuleInterface::OnRegisterUpdate(uint8_t address) {
  //address in base implementation
  UNUSED(address);
  //execute callbacks
  this->ExecuteCallbacks(MODIF_EVENT_REGISTER_UPDATE);
  //TODO temporary debug printout
  //DEBUG_PRINTF("I2C register 0x%02X updated\n", address);
}



/*********************************************************/
/*   IntReg I2C Module Interface - Interrupt Handling    */
/*********************************************************/

uint16_t IntRegI2CModuleInterface::GetInterruptMask() {
  switch (this->int_reg_size) {
    case 1:
      return (uint16_t)this->registers.Reg8(MODIF_I2C_INT_MASK_REG);
    case 2:
      return this->registers.Reg16(MODIF_I2C_INT_MASK_REG);
    default:
      //should never happen
      throw std::logic_error("IntRegI2CModuleInterface invalid state: Bad int_reg_size");
  }
}

void IntRegI2CModuleInterface::SetInterruptMask(uint16_t mask, SuccessCallback&& callback) {
  switch (this->int_reg_size) {
    case 1:
      //write mask
      this->WriteRegister8Async(MODIF_I2C_INT_MASK_REG, (uint8_t)mask, [&, mask, callback = std::move(callback)](bool, uint32_t, uint16_t) {
        //read back mask to ensure correctness
        this->ReadRegister8Async(MODIF_I2C_INT_MASK_REG, callback ? [&, mask, callback = std::move(callback)](bool success, uint32_t value, uint16_t) {
          //propagate success and correctness to external callback
          callback(success && (uint8_t)value == (uint8_t)mask);
        } : ModuleTransferCallback());
      });
      break;
    case 2:
      //write mask
      this->WriteRegister16Async(MODIF_I2C_INT_MASK_REG, mask, [&, mask, callback = std::move(callback)](bool, uint32_t, uint16_t) {
        //read back mask to ensure correctness
        this->ReadRegister16Async(MODIF_I2C_INT_MASK_REG, callback ? [&, mask, callback = std::move(callback)](bool success, uint32_t value, uint16_t) {
          //propagate success and correctness to external callback
          callback(success && (uint16_t)value == mask);
        } : ModuleTransferCallback());
      });
      break;
    default:
      //should never happen
      throw std::logic_error("IntRegI2CModuleInterface invalid state: Bad int_reg_size");
  }
}


void IntRegI2CModuleInterface::HandleInterrupt(ModuleInterfaceInterruptType type, uint16_t extra) noexcept {
  //allow base handling
  this->RegI2CModuleInterface::HandleInterrupt(type, extra);


  uint32_t primask = __get_PRIMASK();
  __disable_irq();

  //process external interrupt
  if (type == IF_EXTI && extra == this->int_pin && this->current_interrupt_timer == 0) {
    this->current_interrupt_timer = MODIF_I2C_INT_HANDLING_TIMEOUT;

    try {
      //read interrupt flags register with corresponding size
      switch (this->int_reg_size) {
        case 1:
          this->ReadRegister8Async(MODIF_I2C_INT_FLAGS_REG, [&](bool success, uint32_t value, uint16_t) {
            if (!success) {
              //handle read failure
              DEBUG_PRINTF("* IntRegI2CModuleInterface interrupt flags read failed\n");
              this->current_interrupt_timer = 0;
              return;
            }

            //clear given flags in register
            uint8_t flags = (uint8_t)value;
            this->WriteRegister8Async(MODIF_I2C_INT_FLAGS_REG, ~flags, [&](bool, uint32_t, uint16_t) {
              //after clear, interrupt handling will be complete
              this->current_interrupt_timer = 0;
            });

            //propagate to further interrupt handling
            this->OnI2CInterrupt(flags);
          });
          break;
        case 2:
          this->ReadRegister16Async(MODIF_I2C_INT_FLAGS_REG, [&](bool success, uint32_t value, uint16_t) {
            if (!success) {
              //handle read failure
              DEBUG_PRINTF("* IntRegI2CModuleInterface interrupt flags read failed\n");
              this->current_interrupt_timer = 0;
              return;
            }

            //clear given flags in register
            uint16_t flags = (uint16_t)value;
            this->WriteRegister16Async(MODIF_I2C_INT_FLAGS_REG, ~flags, [&](bool, uint32_t, uint16_t) {
              //after clear, interrupt handling will be complete
              this->current_interrupt_timer = 0;
            });

            //propagate to further interrupt handling
            this->OnI2CInterrupt(flags);
          });
          break;
        default:
          //should never happen
          throw std::logic_error("IntRegI2CModuleInterface invalid state: Bad int_reg_size");
      }
    } catch (std::exception& exc) {
      DEBUG_PRINTF("* IntRegI2CModuleInterface exception when handling interrupt: %s\n", exc.what());
    } catch (...) {
      DEBUG_PRINTF("* IntRegI2CModuleInterface unknown exception when handling interrupt\n");
    }
  }

  __set_PRIMASK(primask);
}


void IntRegI2CModuleInterface::LoopTasks() {
  uint32_t primask = __get_PRIMASK();
  __disable_irq();

  //check for interrupt or interrupt timeout
  if (this->current_interrupt_timer > 0) {
    //currently handling interrupt: decrement timer - TODO check if we need any additional logic/logging on interrupt handling timeout
    this->current_interrupt_timer--;
  } else {
    //currently not handling any interrupt: check for undetected interrupt condition (pin)
    if (HAL_GPIO_ReadPin(this->int_port, this->int_pin) == GPIO_PIN_RESET) {
      this->HandleInterrupt(IF_EXTI, this->int_pin);
    }
  }

  __set_PRIMASK(primask);

  //allow base handling
  this->RegI2CModuleInterface::LoopTasks();
}


void IntRegI2CModuleInterface::CheckInterruptRegisterDefinitions() {
  //check that both interrupt registers are either 1 or 2 bytes long (and equal)
  if (this->_registers.reg_sizes[MODIF_I2C_INT_MASK_REG] == 1 && this->_registers.reg_sizes[MODIF_I2C_INT_FLAGS_REG] == 1) {
    this->int_reg_size = 1;
  } else if (this->_registers.reg_sizes[MODIF_I2C_INT_MASK_REG] == 2 && this->_registers.reg_sizes[MODIF_I2C_INT_FLAGS_REG] == 2) {
    this->int_reg_size = 2;
  } else {
    //interrupt registers invalid, bad size, or unequal: doesn't fit the standard interrupt register model
    throw std::invalid_argument("IntRegI2CModuleInterface requires standard INT_MASK and INT_FLAGS registers to be valid, of equal size, and either 1 or 2 bytes long");
  }
}

IntRegI2CModuleInterface::IntRegI2CModuleInterface(I2CHardwareInterface& hw_interface, uint8_t i2c_address, const uint16_t* reg_sizes, GPIO_TypeDef* int_port, uint16_t int_pin, bool use_crc) :
    RegI2CModuleInterface(hw_interface, i2c_address, reg_sizes, use_crc), int_port(int_port), int_pin(int_pin), current_interrupt_timer(0) {
  this->CheckInterruptRegisterDefinitions();
}

IntRegI2CModuleInterface::IntRegI2CModuleInterface(I2CHardwareInterface& hw_interface, uint8_t i2c_address, std::initializer_list<uint16_t> reg_sizes, GPIO_TypeDef* int_port, uint16_t int_pin, bool use_crc) :
    RegI2CModuleInterface(hw_interface, i2c_address, reg_sizes, use_crc), int_port(int_port), int_pin(int_pin), current_interrupt_timer(0) {
  this->CheckInterruptRegisterDefinitions();
}


void IntRegI2CModuleInterface::OnI2CInterrupt(uint16_t interrupt_flags) {
  //nothing to do in base implementation
  UNUSED(interrupt_flags);
  //TODO temporary debug printout
  //DEBUG_PRINTF("I2C interrupt 0x%04X\n", interrupt_flags);
}

