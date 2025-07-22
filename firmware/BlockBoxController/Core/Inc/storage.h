/*
 * storage.h
 *
 *  Created on: May 4, 2025
 *      Author: Alex
 */

#ifndef INC_STORAGE_H_
#define INC_STORAGE_H_


#include "cpp_main.h"


#ifdef __cplusplus

#include <vector>

extern "C" {
#endif


#ifdef __cplusplus
}


class Storage;


//section of persistent storage data - e.g. settings for a specific subsystem/module
class StorageSection {
public:
  Storage& storage;
  const uint32_t size_bytes;

  uint32_t GetStorageOffset() const noexcept;

  bool IsSectionDirty() const noexcept;
  void SetSectionDirty(bool dirty) noexcept;

  const uint8_t* operator[](uint32_t offset) const;

  uint8_t GetValue8(uint32_t offset) const;
  uint16_t GetValue16(uint32_t offset) const;
  uint32_t GetValue32(uint32_t offset) const;

  void SetValue8(uint32_t offset, uint8_t value);
  void SetValue16(uint32_t offset, uint16_t value);
  void SetValue32(uint32_t offset, uint32_t value);

  void SetData(uint32_t offset, const uint8_t* source, uint32_t length);

  void LoadDefaults();

  StorageSection(Storage& storage, uint32_t size_bytes, std::function<void(StorageSection&)> load_defaults_func);

protected:
  bool dirty;
  uint32_t storage_offset;
  std::function<void(StorageSection&)> load_defaults_func;

  //for direct access to storage buffer - protected because it doesn't automatically set the dirty flag
  uint8_t* operator[](uint32_t offset);
};


//abstract persistent storage device (with read/write handling), supporting multiple sections
class Storage {
  friend StorageSection;
public:
  const uint32_t capacity_bytes;
  const std::vector<StorageSection*>& sections;

  virtual void ReadAllSections(SuccessCallback&& callback) = 0;
  virtual void WriteAllDirtySections(SuccessCallback&& callback) = 0;

  virtual void ResetToDefaults();

  Storage(uint32_t capacity_bytes) noexcept;

protected:
  std::vector<uint8_t> data;
  std::vector<StorageSection*> _sections;

  uint32_t AddSection(StorageSection* section);
  virtual uint32_t GetNextSectionOffset(uint32_t minimum);
};


#endif


#endif /* INC_STORAGE_H_ */
