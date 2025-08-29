/*
 * eeprom_interface.h
 *
 *  Created on: Jul 21, 2025
 *      Author: Alex
 */

#ifndef INC_EEPROM_INTERFACE_H_
#define INC_EEPROM_INTERFACE_H_


#include "cpp_main.h"
#include "module_interface_i2c.h"
#include "storage.h"


//whether the EEPROM interface uses CRC for communication
#define IF_EEPROM_USE_CRC false

//EEPROM page size, in bytes
#define IF_EEPROM_PAGE_SIZE 32

//EEPROM header address definitions
//32-bit CRC of the entire storage (header + allocated storage section space) (stored in big endian order)
#define IF_EEPROM_HEADER_CRC 0
//32-bit storage/config version, for compatibility checking
#define IF_EEPROM_HEADER_VERSION 4
//starting address of the actual storage space
#define IF_EEPROM_STORAGE_START (1 * IF_EEPROM_PAGE_SIZE)

//EEPROM total size, in bytes
#define IF_EEPROM_SIZE_TOTAL 4096
//EEPROM size available for storage space, in bytes
#define IF_EEPROM_SIZE_STORAGE (IF_EEPROM_SIZE_TOTAL - IF_EEPROM_STORAGE_START)

//EEPROM storage version number - change this whenever breaking changes to EEPROM data layout are made!
#define IF_EEPROM_VERSION_NUMBER 5


#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
}


class EEPROMInterface : protected I2CModuleInterface, public Storage {
public:
  using I2CModuleInterface::hw_interface;
  using I2CModuleInterface::i2c_address;
  using I2CModuleInterface::reg_addr_size;
  using I2CModuleInterface::uses_crc;

  using I2CModuleInterface::HandleInterrupt;
  using I2CModuleInterface::Init;
  using I2CModuleInterface::LoopTasks;


  void ReadAllSections(SuccessCallback&& callback) override;
  void WriteAllDirtySections(SuccessCallback&& callback) override;

  void ResetToDefaults() override;


  void InitModule(SuccessCallback&& callback);


  EEPROMInterface(I2CHardwareInterface& hw_interface, uint8_t i2c_address);

protected:
  bool initialised;
  uint8_t header_data[IF_EEPROM_STORAGE_START];

  //reads the section with the given index, as well as all following sections (recursively)
  void ReadNextSection(uint32_t section_index, SuccessCallback&& callback);

  //writes the 32-byte page with the given index in the section with the given index, as well as all following pages of this and all following sections (recursively)
  void WriteNextSectionPageIfDirty(uint32_t section_index, uint32_t page_index, SuccessCallback&& callback);

  void ReadAndCheckHeader(SuccessCallback&& callback);
  void WriteHeader(SuccessCallback&& callback);

  uint32_t GetNextSectionOffset(uint32_t minimum) override;

};


#endif


#endif /* INC_EEPROM_INTERFACE_H_ */
