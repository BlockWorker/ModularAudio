/*
 * register_set.h
 *
 *  Created on: Jul 5, 2025
 *      Author: Alex
 */

#ifndef INC_REGISTER_SET_H_
#define INC_REGISTER_SET_H_


#include "cpp_main.h"


#ifdef __cplusplus

#include <vector>

extern "C" {
#endif


#ifdef __cplusplus
}


//generalised structure for managing partially-contiguous register sets
class RegisterSet {
public:
  const uint16_t* reg_sizes;

  uint8_t* operator[](uint8_t reg_addr);
  const uint8_t* operator[](uint8_t reg_addr) const;

  uint8_t& Reg8(uint8_t reg_addr);
  uint8_t Reg8(uint8_t reg_addr) const;

  uint16_t& Reg16(uint8_t reg_addr);
  uint16_t Reg16(uint8_t reg_addr) const;

  uint32_t& Reg32(uint8_t reg_addr);
  uint32_t Reg32(uint8_t reg_addr) const;

  RegisterSet(const uint16_t* reg_sizes);
  RegisterSet(std::initializer_list<uint16_t> reg_sizes);

protected:
  std::vector<uint8_t> data;
  uint16_t data_offsets[256];
  uint16_t _reg_sizes[256];

  void Init(const uint16_t* reg_sizes);
};


#endif


#endif /* INC_REGISTER_SET_H_ */
