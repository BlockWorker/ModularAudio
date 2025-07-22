/*
 * storage.cpp
 *
 *  Created on: Jul 21, 2025
 *      Author: Alex
 */


#include <storage.h>


uint32_t StorageSection::GetStorageOffset() const noexcept {
  return this->storage_offset;
}


bool StorageSection::IsSectionDirty() const noexcept {
  return this->dirty;
}

void StorageSection::SetSectionDirty(bool dirty) noexcept {
  this->dirty = dirty;
}


const uint8_t* StorageSection::operator[](uint32_t offset) const {
  if (offset >= this->size_bytes) {
    throw std::invalid_argument("StorageSection indexing given invalid offset");
  }

  return this->storage.data.data() + this->storage_offset + offset;
}


uint8_t StorageSection::GetValue8(uint32_t offset) const {
  return *this->operator [](offset);
}

uint16_t StorageSection::GetValue16(uint32_t offset) const {
  if (offset > this->size_bytes - 2) {
    throw std::invalid_argument("StorageSection GetValue16 given invalid offset");
  }

  return *(uint16_t*)this->operator [](offset);
}

uint32_t StorageSection::GetValue32(uint32_t offset) const {
  if (offset > this->size_bytes - 4) {
    throw std::invalid_argument("StorageSection GetValue32 given invalid offset");
  }

  return *(uint32_t*)this->operator [](offset);
}


void StorageSection::SetValue8(uint32_t offset, uint8_t value) {
  *this->operator [](offset) = value;
  this->dirty = true;
}

void StorageSection::SetValue16(uint32_t offset, uint16_t value) {
  if (offset > this->size_bytes - 2) {
    throw std::invalid_argument("StorageSection SetValue16 given invalid offset");
  }

  *(uint16_t*)this->operator [](offset) = value;
  this->dirty = true;
}

void StorageSection::SetValue32(uint32_t offset, uint32_t value) {
  if (offset > this->size_bytes - 4) {
    throw std::invalid_argument("StorageSection SetValue32 given invalid offset");
  }

  *(uint32_t*)this->operator [](offset) = value;
  this->dirty = true;
}


void StorageSection::SetData(uint32_t offset, const uint8_t* source, uint32_t length) {
  if (offset > this->size_bytes - length) {
    throw std::invalid_argument("StorageSection SetData given invalid offset for given length");
  }

  memcpy(this->operator [](offset), source, length);
  this->dirty = true;
}


void StorageSection::LoadDefaults() {
  if (this->load_defaults_func) {
    //default loading function defined: use that
    this->load_defaults_func(*this);
  } else {
    //no default loading function defined: zero out the section
    memset(this->operator [](0), 0, this->size_bytes);
  }
  this->dirty = true;
}


StorageSection::StorageSection(Storage& storage, uint32_t size_bytes, std::function<void(StorageSection&)> load_defaults_func) :
    storage(storage), size_bytes(size_bytes), dirty(false), load_defaults_func(load_defaults_func) {
  this->storage_offset = this->storage.AddSection(this);
}


//for direct access to storage buffer - protected because it doesn't automatically set the dirty flag
uint8_t* StorageSection::operator[](uint32_t offset) {
  if (offset >= this->size_bytes) {
    throw std::invalid_argument("StorageSection indexing given invalid offset");
  }

  return this->storage.data.data() + this->storage_offset + offset;
}



Storage::Storage(uint32_t capacity_bytes) noexcept :
    capacity_bytes(capacity_bytes), sections(this->_sections) {}


void Storage::ResetToDefaults() {
  for (auto section : this->_sections) {
    section->LoadDefaults();
  }
}


uint32_t Storage::AddSection(StorageSection* section) {
  if (section == NULL) {
    throw std::invalid_argument("Storage AddSection given null pointer");
  }

  uint32_t old_size = this->data.size();
  uint32_t new_size = this->GetNextSectionOffset(old_size + section->size_bytes);
  if (new_size > this->capacity_bytes) {
    //new section doesn't fit
    throw std::logic_error("Storage AddSection insufficient remaining capacity for given section");
  }

  this->data.resize(new_size);

  this->_sections.push_back(section);

  //size before adding = offset of new section
  return old_size;
}

uint32_t Storage::GetNextSectionOffset(uint32_t minimum) {
  return minimum;
}
