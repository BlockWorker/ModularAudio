/*
 * eeprom_interface.cpp
 *
 *  Created on: Jul 21, 2025
 *      Author: Alex
 */


#include "eeprom_interface.h"


//EEPROM CRC data - polynomial: 0x32C00699 (HD 6 up to ~32Kbit length)
static const uint32_t _eeprom_crc_table[256] = {
  0x00000000, 0x32C00699, 0x65800D32, 0x57400BAB, 0xCB001A64, 0xF9C01CFD, 0xAE801756, 0x9C4011CF,
  0xA4C03251, 0x960034C8, 0xC1403F63, 0xF38039FA, 0x6FC02835, 0x5D002EAC, 0x0A402507, 0x3880239E,
  0x7B40623B, 0x498064A2, 0x1EC06F09, 0x2C006990, 0xB040785F, 0x82807EC6, 0xD5C0756D, 0xE70073F4,
  0xDF80506A, 0xED4056F3, 0xBA005D58, 0x88C05BC1, 0x14804A0E, 0x26404C97, 0x7100473C, 0x43C041A5,
  0xF680C476, 0xC440C2EF, 0x9300C944, 0xA1C0CFDD, 0x3D80DE12, 0x0F40D88B, 0x5800D320, 0x6AC0D5B9,
  0x5240F627, 0x6080F0BE, 0x37C0FB15, 0x0500FD8C, 0x9940EC43, 0xAB80EADA, 0xFCC0E171, 0xCE00E7E8,
  0x8DC0A64D, 0xBF00A0D4, 0xE840AB7F, 0xDA80ADE6, 0x46C0BC29, 0x7400BAB0, 0x2340B11B, 0x1180B782,
  0x2900941C, 0x1BC09285, 0x4C80992E, 0x7E409FB7, 0xE2008E78, 0xD0C088E1, 0x8780834A, 0xB54085D3,
  0xDFC18E75, 0xED0188EC, 0xBA418347, 0x888185DE, 0x14C19411, 0x26019288, 0x71419923, 0x43819FBA,
  0x7B01BC24, 0x49C1BABD, 0x1E81B116, 0x2C41B78F, 0xB001A640, 0x82C1A0D9, 0xD581AB72, 0xE741ADEB,
  0xA481EC4E, 0x9641EAD7, 0xC101E17C, 0xF3C1E7E5, 0x6F81F62A, 0x5D41F0B3, 0x0A01FB18, 0x38C1FD81,
  0x0041DE1F, 0x3281D886, 0x65C1D32D, 0x5701D5B4, 0xCB41C47B, 0xF981C2E2, 0xAEC1C949, 0x9C01CFD0,
  0x29414A03, 0x1B814C9A, 0x4CC14731, 0x7E0141A8, 0xE2415067, 0xD08156FE, 0x87C15D55, 0xB5015BCC,
  0x8D817852, 0xBF417ECB, 0xE8017560, 0xDAC173F9, 0x46816236, 0x744164AF, 0x23016F04, 0x11C1699D,
  0x52012838, 0x60C12EA1, 0x3781250A, 0x05412393, 0x9901325C, 0xABC134C5, 0xFC813F6E, 0xCE4139F7,
  0xF6C11A69, 0xC4011CF0, 0x9341175B, 0xA18111C2, 0x3DC1000D, 0x0F010694, 0x58410D3F, 0x6A810BA6,
  0x8D431A73, 0xBF831CEA, 0xE8C31741, 0xDA0311D8, 0x46430017, 0x7483068E, 0x23C30D25, 0x11030BBC,
  0x29832822, 0x1B432EBB, 0x4C032510, 0x7EC32389, 0xE2833246, 0xD04334DF, 0x87033F74, 0xB5C339ED,
  0xF6037848, 0xC4C37ED1, 0x9383757A, 0xA14373E3, 0x3D03622C, 0x0FC364B5, 0x58836F1E, 0x6A436987,
  0x52C34A19, 0x60034C80, 0x3743472B, 0x058341B2, 0x99C3507D, 0xAB0356E4, 0xFC435D4F, 0xCE835BD6,
  0x7BC3DE05, 0x4903D89C, 0x1E43D337, 0x2C83D5AE, 0xB0C3C461, 0x8203C2F8, 0xD543C953, 0xE783CFCA,
  0xDF03EC54, 0xEDC3EACD, 0xBA83E166, 0x8843E7FF, 0x1403F630, 0x26C3F0A9, 0x7183FB02, 0x4343FD9B,
  0x0083BC3E, 0x3243BAA7, 0x6503B10C, 0x57C3B795, 0xCB83A65A, 0xF943A0C3, 0xAE03AB68, 0x9CC3ADF1,
  0xA4438E6F, 0x968388F6, 0xC1C3835D, 0xF30385C4, 0x6F43940B, 0x5D839292, 0x0AC39939, 0x38039FA0,
  0x52829406, 0x6042929F, 0x37029934, 0x05C29FAD, 0x99828E62, 0xAB4288FB, 0xFC028350, 0xCEC285C9,
  0xF642A657, 0xC482A0CE, 0x93C2AB65, 0xA102ADFC, 0x3D42BC33, 0x0F82BAAA, 0x58C2B101, 0x6A02B798,
  0x29C2F63D, 0x1B02F0A4, 0x4C42FB0F, 0x7E82FD96, 0xE2C2EC59, 0xD002EAC0, 0x8742E16B, 0xB582E7F2,
  0x8D02C46C, 0xBFC2C2F5, 0xE882C95E, 0xDA42CFC7, 0x4602DE08, 0x74C2D891, 0x2382D33A, 0x1142D5A3,
  0xA4025070, 0x96C256E9, 0xC1825D42, 0xF3425BDB, 0x6F024A14, 0x5DC24C8D, 0x0A824726, 0x384241BF,
  0x00C26221, 0x320264B8, 0x65426F13, 0x5782698A, 0xCBC27845, 0xF9027EDC, 0xAE427577, 0x9C8273EE,
  0xDF42324B, 0xED8234D2, 0xBAC23F79, 0x880239E0, 0x1442282F, 0x26822EB6, 0x71C2251D, 0x43022384,
  0x7B82001A, 0x49420683, 0x1E020D28, 0x2CC20BB1, 0xB0821A7E, 0x82421CE7, 0xD502174C, 0xE7C211D5
};


//calculate CRC, starting with given crc state
static uint32_t _EEPROM_CRC_Accumulate(const uint8_t* buf, uint32_t length, uint32_t crc) {
  uint32_t i;
  for (i = 0; i < length; i++) {
    crc = ((crc << 8) ^ _eeprom_crc_table[buf[i] ^ (uint8_t)(crc >> 24)]);
  }
  return crc;
}


void EEPROMInterface::ReadAllSections(SuccessCallback&& callback) {
  if (this->_sections.size() == 0) {
    throw std::logic_error("EEPROMInterface ReadAllSections called with no sections registered");
  }

  //clear data array (zero out) - TODO see if this is even needed
  memset(this->data.data(), 0, this->data.size());

  //do actual read, starting at first section
  this->ReadNextSection(0, [&, callback = std::move(callback)](bool success) mutable {
    if (!success) {
      //propagate failure to external callback
      if (callback) {
        callback(false);
      }
      return;
    }

    //perform header checks
    this->ReadAndCheckHeader(std::move(callback));
  });
}

void EEPROMInterface::WriteAllDirtySections(SuccessCallback&& callback) {
  if (this->_sections.size() == 0) {
    throw std::logic_error("EEPROMInterface WriteAllDirtySections called with no sections registered");
  }

  //do actual write, starting at first page of first section
  this->WriteNextSectionPageIfDirty(0, 0, [&, callback = std::move(callback)](bool success) mutable {
    if (!success) {
      //propagate failure to external callback
      if (callback) {
        callback(false);
      }
      return;
    }

    //write new header accordingly
    HAL_Delay(4);
    this->WriteHeader(std::move(callback));
  });
}


void EEPROMInterface::ResetToDefaults() {
  //main functionality is already done in base
  this->Storage::ResetToDefaults();

  //storage is initialised and valid once reset is done
  this->initialised = true;
}



void EEPROMInterface::InitModule(SuccessCallback&& callback) {
  this->initialised = false;

  //attempt to read all data from the EEPROM
  this->ReadAllSections([&, callback = std::move(callback)](bool success) {
    if (success) {
      //success: init done
      this->initialised = true;
    } else {
      //failed: reset to default configuration
      this->ResetToDefaults();
    }

    //report result to external callback
    if (callback) {
      callback(success);
    }
  });
}



EEPROMInterface::EEPROMInterface(I2CHardwareInterface& hw_interface, uint8_t i2c_address) :
    I2CModuleInterface(hw_interface, i2c_address, I2C_MEMADD_SIZE_16BIT, IF_EEPROM_USE_CRC), Storage(IF_EEPROM_SIZE_STORAGE), initialised(false) {}



//reads the section with the given index, as well as all following sections (recursively)
void EEPROMInterface::ReadNextSection(uint32_t section_index, SuccessCallback&& callback) {
  auto section = this->_sections[section_index];
  uint32_t storage_offset = section->GetStorageOffset();

  this->ReadRegisterAsync(IF_EEPROM_STORAGE_START + storage_offset, this->data.data() + storage_offset, section->size_bytes,
                          [&, section_index, callback = std::move(callback)](bool success, uint32_t, uint16_t) mutable {
    if (!success) {
      //propagate failure to external callback
      DEBUG_PRINTF("* EEPROM read failed at section %lu\n", section_index);
      if (callback) {
        callback(false);
      }
      return;
    }

    //read next section, if there is one
    if (section_index + 1 < this->_sections.size()) {
      this->ReadNextSection(section_index + 1, std::move(callback));
    } else {
      //no more sections to read: report success to external callback
      if (callback) {
        callback(true);
      }
    }
  });
}

//writes the 32-byte page with the given index in the section with the given index, as well as all following pages of this and all following sections (recursively)
void EEPROMInterface::WriteNextSectionPageIfDirty(uint32_t section_index, uint32_t page_index, SuccessCallback&& callback) {
  auto section = this->_sections[section_index];

  if (!section->IsSectionDirty()) {
    //section not dirty: skip to next section, if there is one
    if (section_index + 1 < this->_sections.size()) {
      this->WriteNextSectionPageIfDirty(section_index + 1, 0, std::move(callback));
    } else {
      //no more sections to write: report success to external callback
      if (callback) {
        callback(true);
      }
    }
    return;
  }

  //section is dirty: proceed to page write
  uint32_t storage_offset = section->GetStorageOffset();
  uint32_t offset_in_section = page_index * IF_EEPROM_PAGE_SIZE;

  if (offset_in_section >= section->size_bytes) {
    throw std::invalid_argument("EEPROMInterface WriteNextSectionPageIfDirty given invalid page for the given section");
  }

  uint32_t data_array_offset = storage_offset + offset_in_section;
  uint32_t write_length = MIN(section->size_bytes - offset_in_section, IF_EEPROM_PAGE_SIZE);

  this->WriteRegisterAsync(IF_EEPROM_STORAGE_START + data_array_offset, this->data.data() + data_array_offset, write_length,
                           [&, section_index, page_index, sec_size = section->size_bytes, callback = std::move(callback)](bool success, uint32_t, uint16_t) mutable {
    if (!success) {
      //propagate failure to external callback
      DEBUG_PRINTF("* EEPROM write failed at section %lu page %lu\n", section_index, page_index);
      if (callback) {
        callback(false);
      }
      return;
    }

    //write next page, if there is one; otherwise start writing next section, if there is one
    uint32_t next_page_offset = (page_index + 1) * IF_EEPROM_PAGE_SIZE;
    if (next_page_offset < sec_size) {
      HAL_Delay(4);
      this->WriteNextSectionPageIfDirty(section_index, page_index + 1, std::move(callback));
    } else if (section_index + 1 < this->_sections.size()) {
      HAL_Delay(4);
      this->WriteNextSectionPageIfDirty(section_index + 1, 0, std::move(callback));
    } else {
      //no more pages or sections to write: report success to external callback
      if (callback) {
        callback(true);
      }
    }
  });
}


void EEPROMInterface::ReadAndCheckHeader(SuccessCallback&& callback) {
  //read header page
  this->ReadRegisterAsync(0, this->header_data, IF_EEPROM_STORAGE_START, [&, callback = std::move(callback)](bool success, uint32_t, uint16_t) {
    if (!success) {
      //propagate failure to external callback
      DEBUG_PRINTF("* EEPROM header read failed\n");
      if (callback) {
        callback(false);
      }
      return;
    }

    //start CRC calculation with header page (excluding CRC value itself)
    uint32_t crc = _EEPROM_CRC_Accumulate(this->header_data + IF_EEPROM_HEADER_VERSION, IF_EEPROM_STORAGE_START - IF_EEPROM_HEADER_VERSION, 0);

    //continue CRC calculation for main storage data, section by section
    for (auto section : this->_sections) {
      crc = _EEPROM_CRC_Accumulate(this->data.data() + section->GetStorageOffset(), section->size_bytes, crc);
    }

    //finish CRC calculation with stored CRC value, check if result is 0
    if (_EEPROM_CRC_Accumulate(this->header_data + IF_EEPROM_HEADER_CRC, 4, crc) != 0) {
      //CRC mismatch: failure, propagate to external callback
      DEBUG_PRINTF("* EEPROM read CRC mismatch\n");
      if (callback) {
        callback(false);
      }
      return;
    }

    //CRC matches: success depends on version number match
    uint32_t stored_version = *(uint32_t*)(this->header_data + IF_EEPROM_HEADER_VERSION);
    if (stored_version != IF_EEPROM_VERSION_NUMBER) DEBUG_PRINTF("* EEPROM version mismatch (read %lu, current %u)\n", stored_version, IF_EEPROM_VERSION_NUMBER);
    if (callback) {
      callback(stored_version == IF_EEPROM_VERSION_NUMBER);
    }
  });
}

void EEPROMInterface::WriteHeader(SuccessCallback&& callback) {
  //prepare header page (except for CRC): zero out and write version number
  memset(this->header_data, 0, sizeof(this->header_data));
  *(uint32_t*)(this->header_data + IF_EEPROM_HEADER_VERSION) = IF_EEPROM_VERSION_NUMBER;

  //start CRC calculation with header page (excluding CRC value space)
  uint32_t crc = _EEPROM_CRC_Accumulate(this->header_data + IF_EEPROM_HEADER_VERSION, IF_EEPROM_STORAGE_START - IF_EEPROM_HEADER_VERSION, 0);

  //continue CRC calculation for main storage data, section by section
  for (auto section : this->_sections) {
    crc = _EEPROM_CRC_Accumulate(this->data.data() + section->GetStorageOffset(), section->size_bytes, crc);
  }

  //write CRC value into header page, in big-endian order to ensure crc(data..crc) = 0
  this->header_data[IF_EEPROM_HEADER_CRC] = (uint8_t)(crc >> 24);
  this->header_data[IF_EEPROM_HEADER_CRC + 1] = (uint8_t)(crc >> 16);
  this->header_data[IF_EEPROM_HEADER_CRC + 2] = (uint8_t)(crc >> 8);
  this->header_data[IF_EEPROM_HEADER_CRC + 3] = (uint8_t)crc;

  //write header page to EEPROM
  this->WriteRegisterAsync(0, this->header_data, IF_EEPROM_STORAGE_START, SuccessToTransferCallback(callback));
}


uint32_t EEPROMInterface::GetNextSectionOffset(uint32_t minimum) {
  //round up to next full page
  return ((minimum + IF_EEPROM_PAGE_SIZE - 1) / IF_EEPROM_PAGE_SIZE) * IF_EEPROM_PAGE_SIZE;
}

