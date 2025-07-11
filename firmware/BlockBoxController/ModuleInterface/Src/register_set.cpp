/*
 * register_set.cpp
 *
 *  Created on: Jul 5, 2025
 *      Author: Alex
 */


#include "register_set.h"


uint8_t* RegisterSet::operator[](uint8_t reg_addr) {
  if (this->reg_sizes[reg_addr] == 0) {
    throw std::invalid_argument("RegisterSet attempted access to invalid register");
  }

  return this->data.data() + this->data_offsets[reg_addr];
}

const uint8_t* RegisterSet::operator[](uint8_t reg_addr) const {
  if (this->reg_sizes[reg_addr] == 0) {
    throw std::invalid_argument("RegisterSet attempted access to invalid register");
  }

  return this->data.data() + this->data_offsets[reg_addr];
}

uint8_t& RegisterSet::Reg8(uint8_t reg_addr) {
  if (this->reg_sizes[reg_addr] != 1) {
    throw std::invalid_argument("RegisterSet attempted 8-bit access to invalid or differently-sized register");
  }

  return *this->operator [](reg_addr);
}

uint8_t RegisterSet::Reg8(uint8_t reg_addr) const {
  if (this->reg_sizes[reg_addr] != 1) {
    throw std::invalid_argument("RegisterSet attempted 8-bit access to invalid or differently-sized register");
  }

  return *this->operator [](reg_addr);
}

uint16_t& RegisterSet::Reg16(uint8_t reg_addr) {
  if (this->reg_sizes[reg_addr] != 2) {
    throw std::invalid_argument("RegisterSet attempted 16-bit access to invalid or differently-sized register");
  }

  return *(uint16_t*)(this->operator [](reg_addr));
}

uint16_t RegisterSet::Reg16(uint8_t reg_addr) const {
  if (this->reg_sizes[reg_addr] != 2) {
    throw std::invalid_argument("RegisterSet attempted 16-bit access to invalid or differently-sized register");
  }

  return *(uint16_t*)(this->operator [](reg_addr));
}

uint32_t& RegisterSet::Reg32(uint8_t reg_addr) {
  if (this->reg_sizes[reg_addr] != 4) {
    throw std::invalid_argument("RegisterSet attempted 32-bit access to invalid or differently-sized register");
  }

  return *(uint32_t*)(this->operator [](reg_addr));
}

uint32_t RegisterSet::Reg32(uint8_t reg_addr) const {
  if (this->reg_sizes[reg_addr] != 4) {
    throw std::invalid_argument("RegisterSet attempted 32-bit access to invalid or differently-sized register");
  }

  return *(uint32_t*)(this->operator [](reg_addr));
}


RegisterSet::RegisterSet(const uint16_t* reg_sizes) : reg_sizes(this->_reg_sizes) {
  this->Init(reg_sizes);
}

RegisterSet::RegisterSet(std::initializer_list<uint16_t> reg_sizes) : reg_sizes(this->_reg_sizes) {
  if (reg_sizes.size() != 256) {
    throw std::logic_error("RegisterSet size initialiser must have exactly 256 elements");
  }

  this->Init(reg_sizes.begin());
}

void RegisterSet::Init(const uint16_t* reg_sizes) {
  //calculate offsets for registers
  uint16_t offset = 0;
  for (int i = 0; i < 256; i++) {
    //get and store the current register size
    uint16_t size = reg_sizes[i];
    this->_reg_sizes[i] = size;

    //align multi-byte registers according to their size - TODO: re-enable this logic if alignment is desired/needed
    /*if (size >= 5) {
      //5+ bytes: align on 8 byte boundary
      offset = ((offset + 7) / 8) * 8;
    } else if (size >= 3) {
      //3-4 bytes: align on 4 byte boundary
      offset = ((offset + 3) / 4) * 4;
    } else if (size == 2) {
      //2 bytes: align on 2 byte boundary
      offset = ((offset + 1) / 2) * 2;
    }*/

    //save current offset for the current register
    this->data_offsets[i] = offset;

    //increment offset by register size
    offset += size;
  }

  //initialise data vector with the total size
  this->data.resize(offset);
}
