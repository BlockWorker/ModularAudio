/*
 * EVE_mytarget.cpp
 *
 *  Created on: Feb 23, 2025
 *      Author: Alex
 */

#include "EVE.h"


//OSPI regular-command config for single-SPI main memory read
static const OSPI_RegularCmdTypeDef CMD_MAIN_SINGLE_READ = {
  HAL_OSPI_OPTYPE_READ_CFG,             // Operation Type: Read
  HAL_OSPI_FLASH_ID_1,                  // Flash Id: Unused
  0,                                    // Instruction: Unused
  HAL_OSPI_INSTRUCTION_NONE,            // Instruction mode: No instruction in this mmap mode
  HAL_OSPI_INSTRUCTION_8_BITS,          // Instruction size: Unused
  HAL_OSPI_INSTRUCTION_DTR_DISABLE,     // Instruction rate: Unused
  0,                                    // Address: Unused
  HAL_OSPI_ADDRESS_1_LINE,              // Address mode: Single SPI
  HAL_OSPI_ADDRESS_24_BITS,             // Address size: Full address including command bits (24 bits)
  HAL_OSPI_ADDRESS_DTR_DISABLE,         // Address rate: One transfer per clock
  0,                                    // Alternate bytes: Unused
  HAL_OSPI_ALTERNATE_BYTES_NONE,        // Alternate bytes mode: No alternate bytes
  HAL_OSPI_ALTERNATE_BYTES_8_BITS,      // Alternate bytes length: Unused
  HAL_OSPI_ALTERNATE_BYTES_DTR_DISABLE, // Alternate bytes rate: Unused
  HAL_OSPI_DATA_1_LINE,                 // Data mode: Single SPI
  1,                                    // Data length: Unused
  HAL_OSPI_DATA_DTR_DISABLE,            // Data rate: One transfer per clock
  8,                                    // Dummy Cycles: One byte (8)
  HAL_OSPI_DQS_DISABLE,                 // DQS: Unused and must be disabled according to MCU errata
  HAL_OSPI_SIOO_INST_EVERY_CMD          // SIOO mode: Standard mode
};

//OSPI regular-command config for quad-SPI main memory read
static const OSPI_RegularCmdTypeDef CMD_MAIN_QUAD_READ = {
  HAL_OSPI_OPTYPE_READ_CFG,             // Operation Type: Read
  HAL_OSPI_FLASH_ID_1,                  // Flash Id: Unused
  0,                                    // Instruction: Unused
  HAL_OSPI_INSTRUCTION_NONE,            // Instruction mode: No instruction in this mmap mode
  HAL_OSPI_INSTRUCTION_8_BITS,          // Instruction size: Unused
  HAL_OSPI_INSTRUCTION_DTR_DISABLE,     // Instruction rate: Unused
  0,                                    // Address: Unused
  HAL_OSPI_ADDRESS_4_LINES,             // Address mode: Quad SPI
  HAL_OSPI_ADDRESS_24_BITS,             // Address size: Full address including command bits (24 bits)
  HAL_OSPI_ADDRESS_DTR_DISABLE,         // Address rate: One transfer per clock
  0,                                    // Alternate bytes: Unused
  HAL_OSPI_ALTERNATE_BYTES_NONE,        // Alternate bytes mode: No alternate bytes
  HAL_OSPI_ALTERNATE_BYTES_8_BITS,      // Alternate bytes length: Unused
  HAL_OSPI_ALTERNATE_BYTES_DTR_DISABLE, // Alternate bytes rate: Unused
  HAL_OSPI_DATA_4_LINES,                // Data mode: Quad SPI
  1,                                    // Data length: Unused
  HAL_OSPI_DATA_DTR_DISABLE,            // Data rate: One transfer per clock
  2,                                    // Dummy Cycles: One byte (2)
  HAL_OSPI_DQS_DISABLE,                 // DQS: Unused and must be disabled according to MCU errata
  HAL_OSPI_SIOO_INST_EVERY_CMD          // SIOO mode: Standard mode
};

//OSPI regular-command config for single-SPI main memory write
static const OSPI_RegularCmdTypeDef CMD_MAIN_SINGLE_WRITE = {
  HAL_OSPI_OPTYPE_WRITE_CFG,            // Operation Type: Read
  HAL_OSPI_FLASH_ID_1,                  // Flash Id: Unused
  0,                                    // Instruction: Unused
  HAL_OSPI_INSTRUCTION_NONE,            // Instruction mode: No instruction in this mmap mode
  HAL_OSPI_INSTRUCTION_8_BITS,          // Instruction size: Unused
  HAL_OSPI_INSTRUCTION_DTR_DISABLE,     // Instruction rate: Unused
  0,                                    // Address: Unused
  HAL_OSPI_ADDRESS_1_LINE,              // Address mode: Single SPI
  HAL_OSPI_ADDRESS_24_BITS,             // Address size: Full address including command bits (24 bits)
  HAL_OSPI_ADDRESS_DTR_DISABLE,         // Address rate: One transfer per clock
  0,                                    // Alternate bytes: Unused
  HAL_OSPI_ALTERNATE_BYTES_NONE,        // Alternate bytes mode: No alternate bytes
  HAL_OSPI_ALTERNATE_BYTES_8_BITS,      // Alternate bytes length: Unused
  HAL_OSPI_ALTERNATE_BYTES_DTR_DISABLE, // Alternate bytes rate: Unused
  HAL_OSPI_DATA_1_LINE,                 // Data mode: Single SPI
  1,                                    // Data length: Unused
  HAL_OSPI_DATA_DTR_DISABLE,            // Data rate: One transfer per clock
  0,                                    // Dummy Cycles: None for write
  HAL_OSPI_DQS_ENABLE,                  // DQS: Unused, but must be *enabled* according to MCU errata
  HAL_OSPI_SIOO_INST_EVERY_CMD          // SIOO mode: Unused
};

//OSPI regular-command config for quad-SPI main memory write
static const OSPI_RegularCmdTypeDef CMD_MAIN_QUAD_WRITE = {
  HAL_OSPI_OPTYPE_WRITE_CFG,            // Operation Type: Read
  HAL_OSPI_FLASH_ID_1,                  // Flash Id: Unused
  0,                                    // Instruction: Unused
  HAL_OSPI_INSTRUCTION_NONE,            // Instruction mode: No instruction in this mmap mode
  HAL_OSPI_INSTRUCTION_8_BITS,          // Instruction size: Unused
  HAL_OSPI_INSTRUCTION_DTR_DISABLE,     // Instruction rate: Unused
  0,                                    // Address: Unused
  HAL_OSPI_ADDRESS_4_LINES,             // Address mode: Quad SPI
  HAL_OSPI_ADDRESS_24_BITS,             // Address size: Full address including command bits (24 bits)
  HAL_OSPI_ADDRESS_DTR_DISABLE,         // Address rate: One transfer per clock
  0,                                    // Alternate bytes: Unused
  HAL_OSPI_ALTERNATE_BYTES_NONE,        // Alternate bytes mode: No alternate bytes
  HAL_OSPI_ALTERNATE_BYTES_8_BITS,      // Alternate bytes length: Unused
  HAL_OSPI_ALTERNATE_BYTES_DTR_DISABLE, // Alternate bytes rate: Unused
  HAL_OSPI_DATA_4_LINES,                // Data mode: Quad SPI
  1,                                    // Data length: Unused
  HAL_OSPI_DATA_DTR_DISABLE,            // Data rate: One transfer per clock
  0,                                    // Dummy Cycles: None for write
  HAL_OSPI_DQS_ENABLE,                  // DQS: Unused, but must be *enabled* according to MCU errata
  HAL_OSPI_SIOO_INST_EVERY_CMD          // SIOO mode: Unused
};

//OSPI regular-command config for single-SPI func memory read
static const OSPI_RegularCmdTypeDef CMD_FUNC_SINGLE_READ = {
  HAL_OSPI_OPTYPE_READ_CFG,             // Operation Type: Read
  HAL_OSPI_FLASH_ID_1,                  // Flash Id: Unused
  0x30,                                 // Instruction: Fixed upper address bits (0x30), command bits 00 (read)
  HAL_OSPI_INSTRUCTION_1_LINE,          // Instruction mode: Single SPI
  HAL_OSPI_INSTRUCTION_8_BITS,          // Instruction size: One byte
  HAL_OSPI_INSTRUCTION_DTR_DISABLE,     // Instruction rate: One transfer per clock
  0,                                    // Address: Unused
  HAL_OSPI_ADDRESS_1_LINE,              // Address mode: Single SPI
  HAL_OSPI_ADDRESS_16_BITS,             // Address size: Remaining two address bytes (16 bits)
  HAL_OSPI_ADDRESS_DTR_DISABLE,         // Address rate: One transfer per clock
  0,                                    // Alternate bytes: Unused
  HAL_OSPI_ALTERNATE_BYTES_NONE,        // Alternate bytes mode: No alternate bytes
  HAL_OSPI_ALTERNATE_BYTES_8_BITS,      // Alternate bytes length: Unused
  HAL_OSPI_ALTERNATE_BYTES_DTR_DISABLE, // Alternate bytes rate: Unused
  HAL_OSPI_DATA_1_LINE,                 // Data mode: Single SPI
  1,                                    // Data length: Unused
  HAL_OSPI_DATA_DTR_DISABLE,            // Data rate: One transfer per clock
  8,                                    // Dummy Cycles: One byte (8)
  HAL_OSPI_DQS_DISABLE,                 // DQS: Unused and must be disabled according to MCU errata
  HAL_OSPI_SIOO_INST_EVERY_CMD          // SIOO mode: Standard mode
};

//OSPI regular-command config for quad-SPI func memory read
static const OSPI_RegularCmdTypeDef CMD_FUNC_QUAD_READ = {
  HAL_OSPI_OPTYPE_READ_CFG,             // Operation Type: Read
  HAL_OSPI_FLASH_ID_1,                  // Flash Id: Unused
  0x30,                                 // Instruction: Fixed upper address bits (0x30), command bits 00 (read)
  HAL_OSPI_INSTRUCTION_4_LINES,         // Instruction mode: Quad SPI
  HAL_OSPI_INSTRUCTION_8_BITS,          // Instruction size: One byte
  HAL_OSPI_INSTRUCTION_DTR_DISABLE,     // Instruction rate: One transfer per clock
  0,                                    // Address: Unused
  HAL_OSPI_ADDRESS_4_LINES,             // Address mode: Quad SPI
  HAL_OSPI_ADDRESS_16_BITS,             // Address size: Remaining two address bytes (16 bits)
  HAL_OSPI_ADDRESS_DTR_DISABLE,         // Address rate: One transfer per clock
  0,                                    // Alternate bytes: Unused
  HAL_OSPI_ALTERNATE_BYTES_NONE,        // Alternate bytes mode: No alternate bytes
  HAL_OSPI_ALTERNATE_BYTES_8_BITS,      // Alternate bytes length: Unused
  HAL_OSPI_ALTERNATE_BYTES_DTR_DISABLE, // Alternate bytes rate: Unused
  HAL_OSPI_DATA_4_LINES,                // Data mode: Quad SPI
  1,                                    // Data length: Unused
  HAL_OSPI_DATA_DTR_DISABLE,            // Data rate: One transfer per clock
  2,                                    // Dummy Cycles: One byte (2)
  HAL_OSPI_DQS_DISABLE,                 // DQS: Unused and must be disabled according to MCU errata
  HAL_OSPI_SIOO_INST_EVERY_CMD          // SIOO mode: Standard mode
};

//OSPI regular-command config for single-SPI func memory write
static const OSPI_RegularCmdTypeDef CMD_FUNC_SINGLE_WRITE = {
  HAL_OSPI_OPTYPE_WRITE_CFG,            // Operation Type: Read
  HAL_OSPI_FLASH_ID_1,                  // Flash Id: Unused
  0x30 | 0x80,                          // Instruction: Fixed upper address bits (0x30), command bits 10 (write)
  HAL_OSPI_INSTRUCTION_1_LINE,          // Instruction mode: Single SPI
  HAL_OSPI_INSTRUCTION_8_BITS,          // Instruction size: One byte
  HAL_OSPI_INSTRUCTION_DTR_DISABLE,     // Instruction rate: One transfer per clock
  0,                                    // Address: Unused
  HAL_OSPI_ADDRESS_1_LINE,              // Address mode: Single SPI
  HAL_OSPI_ADDRESS_16_BITS,             // Address size: Remaining two address bytes (16 bits)
  HAL_OSPI_ADDRESS_DTR_DISABLE,         // Address rate: One transfer per clock
  0,                                    // Alternate bytes: Unused
  HAL_OSPI_ALTERNATE_BYTES_NONE,        // Alternate bytes mode: No alternate bytes
  HAL_OSPI_ALTERNATE_BYTES_8_BITS,      // Alternate bytes length: Unused
  HAL_OSPI_ALTERNATE_BYTES_DTR_DISABLE, // Alternate bytes rate: Unused
  HAL_OSPI_DATA_1_LINE,                 // Data mode: Single SPI
  1,                                    // Data length: Unused
  HAL_OSPI_DATA_DTR_DISABLE,            // Data rate: One transfer per clock
  0,                                    // Dummy Cycles: None for write
  HAL_OSPI_DQS_ENABLE,                  // DQS: Unused, but must be *enabled* according to MCU errata
  HAL_OSPI_SIOO_INST_EVERY_CMD          // SIOO mode: Unused
};

//OSPI regular-command config for quad-SPI func memory write
static const OSPI_RegularCmdTypeDef CMD_FUNC_QUAD_WRITE = {
  HAL_OSPI_OPTYPE_WRITE_CFG,            // Operation Type: Read
  HAL_OSPI_FLASH_ID_1,                  // Flash Id: Unused
  0x30 | 0x80,                          // Instruction: Fixed upper address bits (0x30), command bits 10 (write)
  HAL_OSPI_INSTRUCTION_4_LINES,         // Instruction mode: Quad SPI
  HAL_OSPI_INSTRUCTION_8_BITS,          // Instruction size: One byte
  HAL_OSPI_INSTRUCTION_DTR_DISABLE,     // Instruction rate: One transfer per clock
  0,                                    // Address: Unused
  HAL_OSPI_ADDRESS_4_LINES,             // Address mode: Quad SPI
  HAL_OSPI_ADDRESS_16_BITS,             // Address size: Remaining two address bytes (16 bits)
  HAL_OSPI_ADDRESS_DTR_DISABLE,         // Address rate: One transfer per clock
  0,                                    // Alternate bytes: Unused
  HAL_OSPI_ALTERNATE_BYTES_NONE,        // Alternate bytes mode: No alternate bytes
  HAL_OSPI_ALTERNATE_BYTES_8_BITS,      // Alternate bytes length: Unused
  HAL_OSPI_ALTERNATE_BYTES_DTR_DISABLE, // Alternate bytes rate: Unused
  HAL_OSPI_DATA_4_LINES,                // Data mode: Quad SPI
  1,                                    // Data length: Unused
  HAL_OSPI_DATA_DTR_DISABLE,            // Data rate: One transfer per clock
  0,                                    // Dummy Cycles: None for write
  HAL_OSPI_DQS_ENABLE,                  // DQS: Unused, but must be *enabled* according to MCU errata
  HAL_OSPI_SIOO_INST_EVERY_CMD          // SIOO mode: Unused
};

//OSPI memory-map config
static const OSPI_MemoryMappedTypeDef MMAP_CFG = {
  HAL_OSPI_TIMEOUT_COUNTER_DISABLE, // Timeout mode: Disabled
  0                                 // Timeout value: Unused
};


void EVETargetPHY::SendHostCommand(uint8_t command, uint8_t param) {
  //host commands are only supported in single-spi mode
  if (this->transfer_mode != TRANSFERMODE_SINGLE) {
    throw std::logic_error("EVE SendHostCommand only supported in single-spi mode");
  }

  //end any current mmap due to different command config
  this->EndMMap();

  //configure command
  OSPI_RegularCmdTypeDef hcmd = { 0 };
  hcmd.OperationType = HAL_OSPI_OPTYPE_COMMON_CFG;
  hcmd.Instruction = (uint32_t)command; //instruction byte is command itself
  hcmd.InstructionMode = HAL_OSPI_INSTRUCTION_1_LINE; //host commands are only supported in single-spi mode
  hcmd.InstructionSize = HAL_OSPI_INSTRUCTION_8_BITS;
  hcmd.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;
  hcmd.AddressMode = HAL_OSPI_ADDRESS_NONE;
  hcmd.AlternateBytes = ((uint32_t)param) << 8; //alternate bytes for parameter byte and third (zero) byte - for some reason, seems to be in big endian?
  hcmd.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_1_LINE;
  hcmd.AlternateBytesSize = HAL_OSPI_ALTERNATE_BYTES_16_BITS;
  hcmd.AlternateBytesDtrMode = HAL_OSPI_ALTERNATE_BYTES_DTR_DISABLE;
  hcmd.DataMode = HAL_OSPI_DATA_NONE; //no data transfer for command
  hcmd.DummyCycles = 0;
  hcmd.DQSMode = HAL_OSPI_DQS_DISABLE; //DQS not used
  hcmd.SIOOMode = HAL_OSPI_SIOO_INST_EVERY_CMD;

  //send command
  ThrowOnHALErrorMsg(HAL_OSPI_Command(&hospi1, &hcmd, EVE_PHY_SMALL_TRANSFER_TIMEOUT), "EVE SendHostCommand");
}


//directly read from the display; ends any ongoing memory mapping
void EVETargetPHY::DirectReadBuffer(uint32_t address, uint8_t* buf, uint32_t size, uint32_t timeout) {
  if (buf == NULL) {
    throw std::invalid_argument("EVE DirectReadBuffer buf pointer must not be null");
  }

  //end any current mmap due to different command config
  this->EndMMap();

  //configure read
  OSPI_RegularCmdTypeDef dcmd = { 0 };
  dcmd.OperationType = HAL_OSPI_OPTYPE_COMMON_CFG;
  dcmd.InstructionMode = HAL_OSPI_INSTRUCTION_NONE; //no instruction necessary
  dcmd.Address = (address & 0x3FFFFF); //address: 22 bits, command bits: 00 (read)
  dcmd.AddressMode = (this->transfer_mode == TRANSFERMODE_QUAD) ? HAL_OSPI_ADDRESS_4_LINES : HAL_OSPI_ADDRESS_1_LINE; //single-spi or quad-spi depending on setting
  dcmd.AddressSize = HAL_OSPI_ADDRESS_24_BITS;
  dcmd.AddressDtrMode = HAL_OSPI_ADDRESS_DTR_DISABLE;
  dcmd.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
  dcmd.DataMode = (this->transfer_mode == TRANSFERMODE_QUAD) ? HAL_OSPI_DATA_4_LINES : HAL_OSPI_DATA_1_LINE;
  dcmd.NbData = size;
  dcmd.DataDtrMode = HAL_OSPI_DATA_DTR_DISABLE;
  dcmd.DummyCycles = (this->transfer_mode == TRANSFERMODE_QUAD) ? 2 : 8; //one dummy byte = 8 cycles for single-spi, 2 cycles for quad-spi
  dcmd.DQSMode = HAL_OSPI_DQS_DISABLE; //DQS not used and must be disabled for reads according to MCU errata
  dcmd.SIOOMode = HAL_OSPI_SIOO_INST_EVERY_CMD;

  //apply read config
  ThrowOnHALErrorMsg(HAL_OSPI_Command(&hospi1, &dcmd, 2), "EVE DirectReadBuffer setup");

  //perform actual read
  ThrowOnHALErrorMsg(HAL_OSPI_Receive(&hospi1, buf, timeout), "EVE DirectReadBuffer read");
}

void EVETargetPHY::DirectRead8(uint32_t address, uint8_t* value_ptr) {
  this->DirectReadBuffer(address, value_ptr, 1, EVE_PHY_SMALL_TRANSFER_TIMEOUT);
}

void EVETargetPHY::DirectRead16(uint32_t address, uint16_t* value_ptr) {
  this->DirectReadBuffer(address, (uint8_t*)value_ptr, 2, EVE_PHY_SMALL_TRANSFER_TIMEOUT);
}

void EVETargetPHY::DirectRead32(uint32_t address, uint32_t* value_ptr) {
  this->DirectReadBuffer(address, (uint8_t*)value_ptr, 4, EVE_PHY_SMALL_TRANSFER_TIMEOUT);
}


//directly write to the display; ends any ongoing memory mapping
void EVETargetPHY::DirectWriteBuffer(uint32_t address, const uint8_t* buf, uint32_t size, uint32_t timeout) {
  if (buf == NULL) {
    throw std::invalid_argument("EVE DirectWriteBuffer buf pointer must not be null");
  }

  //end any current mmap due to different command config
  this->EndMMap();

  //configure write
  OSPI_RegularCmdTypeDef dcmd = { 0 };
  dcmd.OperationType = HAL_OSPI_OPTYPE_COMMON_CFG;
  dcmd.InstructionMode = HAL_OSPI_INSTRUCTION_NONE; //no instruction necessary
  dcmd.Address = (address & 0x3FFFFF) | 0x800000; //address: 22 bits, command bits: 10 (write)
  dcmd.AddressMode = (this->transfer_mode == TRANSFERMODE_QUAD) ? HAL_OSPI_ADDRESS_4_LINES : HAL_OSPI_ADDRESS_1_LINE; //single-spi or quad-spi depending on setting
  dcmd.AddressSize = HAL_OSPI_ADDRESS_24_BITS;
  dcmd.AddressDtrMode = HAL_OSPI_ADDRESS_DTR_DISABLE;
  dcmd.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
  dcmd.DataMode = (this->transfer_mode == TRANSFERMODE_QUAD) ? HAL_OSPI_DATA_4_LINES : HAL_OSPI_DATA_1_LINE;
  dcmd.NbData = size;
  dcmd.DataDtrMode = HAL_OSPI_DATA_DTR_DISABLE;
  dcmd.DummyCycles = 0; //no dummy byte for writes
  dcmd.DQSMode = HAL_OSPI_DQS_DISABLE; //DQS not used (must be enabled for writes according to MCU errata, but only in mmap mode, not for direct writes)
  dcmd.SIOOMode = HAL_OSPI_SIOO_INST_EVERY_CMD;

  //apply write config
  ThrowOnHALErrorMsg(HAL_OSPI_Command(&hospi1, &dcmd, 2), "EVE DirectWriteBuffer setup");

  //perform actual write
  ThrowOnHALErrorMsg(HAL_OSPI_Transmit(&hospi1, (uint8_t*)buf, timeout), "EVE DirectWriteBuffer write");
}

void EVETargetPHY::DirectWrite8(uint32_t address, uint8_t value) {
  this->DirectWriteBuffer(address, &value, 1, EVE_PHY_SMALL_TRANSFER_TIMEOUT);
}

void EVETargetPHY::DirectWrite16(uint32_t address, uint16_t value) {
   this->DirectWriteBuffer(address, (uint8_t*)&value, 2, EVE_PHY_SMALL_TRANSFER_TIMEOUT);
}

void EVETargetPHY::DirectWrite32(uint32_t address, uint32_t value) {
   this->DirectWriteBuffer(address, (uint8_t*)&value, 4, EVE_PHY_SMALL_TRANSFER_TIMEOUT);
}



void EVETargetPHY::EnsureMMapMode(EVEMMapMode mode) {
  //reset current state to unknown if OSPI peripheral is not in mmapped state anymore
  if (HAL_OSPI_GetState(&hospi1) != HAL_OSPI_STATE_BUSY_MEM_MAPPED) {
    this->mmap_mode = MMAP_UNKNOWN;
  }

  switch (mode) {
    case MMAP_MAIN_RAM:
      switch (this->mmap_mode) {
        case MMAP_MAIN_RAM:
          //main mmap already active
          return;
        case MMAP_FUNC_RAM:
          //mmap active, but wrong kind (func) - deactivate that mmap first, then fall through to default case
          this->EndMMap();
        default:
          //mmap not active - activate main mmap
          this->configure_main_mmap();
          return;
      }

    case MMAP_FUNC_RAM:
      switch (this->mmap_mode) {
        case MMAP_FUNC_RAM:
          //func mmap already active
          return;
        case MMAP_MAIN_RAM:
          //mmap active, but wrong kind (main) - deactivate that mmap first, then fall through to default case
          this->EndMMap();
        default:
          //mmap not active - activate func mmap
          this->configure_func_mmap();
          return;
      }

    default:
      //interpret everything else as "disable mmap"
      switch (this->mmap_mode) {
        case MMAP_FUNC_RAM:
        case MMAP_MAIN_RAM:
          //any mmap active - deactivate that mmap
          this->EndMMap();
        default:
          //mmap already inactive
          return;
      }
  }
}

void EVETargetPHY::EndMMap() {
  if (HAL_OSPI_GetState(&hospi1) == HAL_OSPI_STATE_BUSY_MEM_MAPPED) {
    //peripheral busy: abort current mmap
    ThrowOnHALErrorMsg(HAL_OSPI_Abort(&hospi1), "EVE EndMMap abort");
  }
  this->mmap_mode = MMAP_UNKNOWN;
}

void EVETargetPHY::SetTransferMode(EVETransferMode mode) {
  if (this->transfer_mode == mode) {
    //already in desired mode
    return;
  }

  if (mode != TRANSFERMODE_SINGLE && mode != TRANSFERMODE_QUAD) {
    //invalid transfer mode requested
    throw std::invalid_argument("EVE SetTransferMode only supports single and quad modes");
  }

  //save currently active mmap mode, stop active mmap if there is one
  EVEMMapMode active_mmap_mode = this->GetMMapMode();
  this->EndMMap();

  this->transfer_mode = mode;

  if (active_mmap_mode == MMAP_MAIN_RAM || active_mmap_mode == MMAP_FUNC_RAM) {
    //mmap was active: restart mmap with new mode
    this->EnsureMMapMode(active_mmap_mode);
  }
}

EVEMMapMode EVETargetPHY::GetMMapMode() noexcept {
  //reset current state to unknown if OSPI peripheral is not in mmapped state anymore
  if (HAL_OSPI_GetState(&hospi1) != HAL_OSPI_STATE_BUSY_MEM_MAPPED) {
    this->mmap_mode = MMAP_UNKNOWN;
  }
  return this->mmap_mode;
}

EVETransferMode EVETargetPHY::GetTransferMode() const noexcept {
  return this->transfer_mode;
}

//configure OSPI for main ram mmap mode (direct addressing, needs software-side handling of "read" addresses vs. "write" addresses)
void EVETargetPHY::configure_main_mmap() {
  const OSPI_RegularCmdTypeDef* read_cfg = (this->transfer_mode == TRANSFERMODE_QUAD) ? &CMD_MAIN_QUAD_READ : &CMD_MAIN_SINGLE_READ;
  const OSPI_RegularCmdTypeDef* write_cfg = (this->transfer_mode == TRANSFERMODE_QUAD) ? &CMD_MAIN_QUAD_WRITE : &CMD_MAIN_SINGLE_WRITE;

  //apply read config
  ThrowOnHALErrorMsg(HAL_OSPI_Command(&hospi1, (OSPI_RegularCmdTypeDef*)read_cfg, 2), "EVE configure_main_mmap setup");

  //apply write config
  ThrowOnHALErrorMsg(HAL_OSPI_Command(&hospi1, (OSPI_RegularCmdTypeDef*)write_cfg, 2), "EVE configure_main_mmap setup");

  //enable mmap mode
  ThrowOnHALErrorMsg(HAL_OSPI_MemoryMapped(&hospi1, (OSPI_MemoryMappedTypeDef*)&MMAP_CFG), "EVE configure_main_mmap mmap");

  this->mmap_mode = MMAP_MAIN_RAM;
}

//configure OSPI for func ram mmap mode (addressing with upper 6 bits fixed to 0x30, automatically handles read/write bit setting)
void EVETargetPHY::configure_func_mmap() {
  const OSPI_RegularCmdTypeDef* read_cfg = (this->transfer_mode == TRANSFERMODE_QUAD) ? &CMD_FUNC_QUAD_READ : &CMD_FUNC_SINGLE_READ;
  const OSPI_RegularCmdTypeDef* write_cfg = (this->transfer_mode == TRANSFERMODE_QUAD) ? &CMD_FUNC_QUAD_WRITE : &CMD_FUNC_SINGLE_WRITE;

  //apply read config
  ThrowOnHALErrorMsg(HAL_OSPI_Command(&hospi1, (OSPI_RegularCmdTypeDef*)read_cfg, 2), "EVE configure_func_mmap setup");

  //apply write config
  ThrowOnHALErrorMsg(HAL_OSPI_Command(&hospi1, (OSPI_RegularCmdTypeDef*)write_cfg, 2), "EVE configure_func_mmap setup");

  //enable mmap mode
  ThrowOnHALErrorMsg(HAL_OSPI_MemoryMapped(&hospi1, (OSPI_MemoryMappedTypeDef*)&MMAP_CFG), "EVE configure_func_mmap mmap");

  this->mmap_mode = MMAP_FUNC_RAM;
}


