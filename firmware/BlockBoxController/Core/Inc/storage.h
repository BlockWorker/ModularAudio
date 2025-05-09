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


//section of persistent storage data - e.g. settings for a specific subsystem/module
template<uint32_t _size_bytes>
class StorageSection {
public:
  const uint32_t size_bytes = _size_bytes;

  virtual bool IsBufferDirty() const noexcept = 0;

  virtual void GetRawData(uint8_t* destination, uint32_t buffer_offset, uint32_t size) const noexcept;
  virtual void SetRawData(const uint8_t* source, uint32_t buffer_offset, uint32_t size);

protected:
  uint8_t data_buffer[_size_bytes];
};


//abstract persistent storage device (with read/write handling), supporting multiple sections
class Storage {
public:
  const uint32_t capacity_bytes;
  const std::vector<StorageSection&>& sections = _sections;

  virtual void ReadAllSections() const = 0;
  virtual void WriteAllDirtySections() const = 0;

  virtual void AddSection(StorageSection& section);

  Storage(uint32_t capacity_bytes) noexcept : capacity_bytes(capacity_bytes) {}

protected:
  std::vector<StorageSection&> _sections;
};


#endif


#endif /* INC_STORAGE_H_ */
