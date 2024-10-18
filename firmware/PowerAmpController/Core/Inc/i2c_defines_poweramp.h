/*
 * i2c_defines_poweramp.h
 *
 *  Created on: Oct 15, 2024
 *      Author: Alex
 *
 *  Power Amp Controller I2C register definitions
 *
 *  Register map:
 *  * General registers
 *   - 0x01: STATUS: General status (1B, bit field, r)
 *   - 0x08: CONTROL: General control (1B, bit field, rw)
 *  * Interrupt registers
 *   - 0x10: INT_MASK: Interrupt mask (1B, bit field, rw, high = interrupt enable)
 *   - 0x11: INT_FLAGS: Interrupt flags (1B, bit field, rc, high = interrupt occurred)
 *  * PVDD registers
 *   - 0x20: PVDD_REQ: PVDD requested voltage (4B, float little endian, rw)
 *   - 0x21: PVDD_MEASURED: PVDD measured voltage (4B, float little endian, r)
 *   - 0x22: PVDD_OFFSET: PVDD request offset (4B, float little endian, r)
 *  * Output monitor registers
 *   - 0x30-0x33: MON_VRMS_FAST_[A-D]: Channel A-D rms voltage, 0.1s time constant (each 4B, float little endian, r)
 *   - 0x34-0x37: MON_IRMS_FAST_[A-D]: Channel A-D rms current, 0.1s time constant (each 4B, float little endian, r)
 *   - 0x38-0x3B: MON_PAVG_FAST_[A-D]: Channel A-D avg real power, 0.1s time constant (each 4B, float little endian, r)
 *   - 0x3C-0x3F: MON_PAPP_FAST_[A-D]: Channel A-D avg apparent power, 0.1s time constant (each 4B, float little endian, r)
 *   - 0x40-0x43: MON_VRMS_SLOW_[A-D]: Channel A-D rms voltage, 1s time constant (each 4B, float little endian, r)
 *   - 0x44-0x47: MON_IRMS_SLOW_[A-D]: Channel A-D rms current, 1s time constant (each 4B, float little endian, r)
 *   - 0x48-0x4B: MON_PAVG_SLOW_[A-D]: Channel A-D avg real power, 1s time constant (each 4B, float little endian, r)
 *   - 0x4C-0x4F: MON_PAPP_SLOW_[A-D]: Channel A-D avg apparent power, 1s time constant (each 4B, float little endian, r)
 *  * Safety system registers
 *   - 0x50-0x54: SERR_IRMS_INST_[A-D,SUM]: Channel A-D and sum current error limit, instantaneous (each 4B, float little endian, rw)
 *   - 0x55-0x59: SERR_IRMS_FAST_[A-D,SUM]: Channel A-D and sum current error limit, 0.1s time constant (each 4B, float little endian, rw)
 *   - 0x5A-0x5E: SERR_IRMS_SLOW_[A-D,SUM]: Channel A-D and sum current error limit, 1s time constant (each 4B, float little endian, rw)
 *   - 0x60-0x64: SERR_PAVG_INST_[A-D,SUM]: Channel A-D and sum real power error limit, instantaneous (each 4B, float little endian, rw)
 *   - 0x65-0x69: SERR_PAVG_FAST_[A-D,SUM]: Channel A-D and sum real power error limit, 0.1s time constant (each 4B, float little endian, rw)
 *   - 0x6A-0x6E: SERR_PAVG_SLOW_[A-D,SUM]: Channel A-D and sum real power error limit, 1s time constant (each 4B, float little endian, rw)
 *   - 0x70-0x74: SERR_PAPP_INST_[A-D,SUM]: Channel A-D and sum apparent power error limit, instantaneous (each 4B, float little endian, rw)
 *   - 0x75-0x79: SERR_PAPP_FAST_[A-D,SUM]: Channel A-D and sum apparent power error limit, 0.1s time constant (each 4B, float little endian, rw)
 *   - 0x7A-0x7E: SERR_PAPP_SLOW_[A-D,SUM]: Channel A-D and sum apparent power error limit, 1s time constant (each 4B, float little endian, rw)
 *   - 0x80-0x84: SWARN_IRMS_INST_[A-D,SUM]: Channel A-D and sum current warning limit, instantaneous (each 4B, float little endian, rw)
 *   - 0x85-0x89: SWARN_IRMS_FAST_[A-D,SUM]: Channel A-D and sum current warning limit, 0.1s time constant (each 4B, float little endian, rw)
 *   - 0x8A-0x8E: SWARN_IRMS_SLOW_[A-D,SUM]: Channel A-D and sum current warning limit, 1s time constant (each 4B, float little endian, rw)
 *   - 0x90-0x94: SWARN_PAVG_INST_[A-D,SUM]: Channel A-D and sum real power warning limit, instantaneous (each 4B, float little endian, rw)
 *   - 0x95-0x99: SWARN_PAVG_FAST_[A-D,SUM]: Channel A-D and sum real power warning limit, 0.1s time constant (each 4B, float little endian, rw)
 *   - 0x9A-0x9E: SWARN_PAVG_SLOW_[A-D,SUM]: Channel A-D and sum real power warning limit, 1s time constant (each 4B, float little endian, rw)
 *   - 0xA0-0xA4: SWARN_PAPP_INST_[A-D,SUM]: Channel A-D and sum apparent power warning limit, instantaneous (each 4B, float little endian, rw)
 *   - 0xA5-0xA9: SWARN_PAPP_FAST_[A-D,SUM]: Channel A-D and sum apparent power warning limit, 0.1s time constant (each 4B, float little endian, rw)
 *   - 0xAA-0xAE: SWARN_PAPP_SLOW_[A-D,SUM]: Channel A-D and sum apparent power warning limit, 1s time constant (each 4B, float little endian, rw)
 *   - 0xB0: SAFETY_STATUS: Status of safety system (1B, bit field, r)
 *   - 0xB1: SERR_SOURCE: Indication of error source (2B, bit field, rc)
 *   - 0xB2: SWARN_SOURCE: Indication of warning source (2B, bit field, rc)
 *  * Misc registers
 *   - 0xFF: MODULE_ID: Module ID (1B, hex, r)
 *
 *  Bit field definitions:
 *  * STATUS (0x01, 1B):
 *
 *  * CONTROL (0x08, 1B):
 *
 *  * INT_MASK (0x10, 1B):
 *
 *  * INT_FLAGS (0x11, 1B):
 *
 *  * SAFETY_STATUS (0xB0, 1B):
 *
 *  * SERR_SOURCE (0xB1, 2B):
 *
 *  * SWARN_SOURCE (0xB2, 2B):
 *
 */

#ifndef INC_I2C_DEFINES_POWERAMP_H_
#define INC_I2C_DEFINES_POWERAMP_H_


//virtual register sizes in bytes - 0 means register is invalid
#define I2CDEF_POWERAMP_REG_SIZES {\
  0, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0,\
  1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,\
  4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,\
  4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0,\
  4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0,\
  4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0,\
  4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0,\
  4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0,\
  4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 0,\
  1, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 }


//General registers
#define I2CDEF_POWERAMP_STATUS 0x01

#define I2CDEF_POWERAMP_CONTROL 0x08


//Interrupt registers
#define I2CDEF_POWERAMP_INT_MASK 0x10

#define I2CDEF_POWERAMP_INT_FLAGS 0x11


//PVDD registers
#define I2CDEF_POWERAMP_PVDD_REQ 0x20

#define I2CDEF_POWERAMP_PVDD_MEASURED 0x21

#define I2CDEF_POWERAMP_PVDD_OFFSET 0x22


//Output monitor registers
#define I2CDEF_POWERAMP_MON_VRMS_FAST_A 0x30
#define I2CDEF_POWERAMP_MON_VRMS_FAST_B 0x31
#define I2CDEF_POWERAMP_MON_VRMS_FAST_C 0x32
#define I2CDEF_POWERAMP_MON_VRMS_FAST_D 0x33

#define I2CDEF_POWERAMP_MON_IRMS_FAST_A 0x34
#define I2CDEF_POWERAMP_MON_IRMS_FAST_B 0x35
#define I2CDEF_POWERAMP_MON_IRMS_FAST_C 0x36
#define I2CDEF_POWERAMP_MON_IRMS_FAST_D 0x37

#define I2CDEF_POWERAMP_MON_PAVG_FAST_A 0x38
#define I2CDEF_POWERAMP_MON_PAVG_FAST_B 0x39
#define I2CDEF_POWERAMP_MON_PAVG_FAST_C 0x3A
#define I2CDEF_POWERAMP_MON_PAVG_FAST_D 0x3B

#define I2CDEF_POWERAMP_MON_PAPP_FAST_A 0x3C
#define I2CDEF_POWERAMP_MON_PAPP_FAST_B 0x3D
#define I2CDEF_POWERAMP_MON_PAPP_FAST_C 0x3E
#define I2CDEF_POWERAMP_MON_PAPP_FAST_D 0x3F

#define I2CDEF_POWERAMP_MON_VRMS_SLOW_A 0x40
#define I2CDEF_POWERAMP_MON_VRMS_SLOW_B 0x41
#define I2CDEF_POWERAMP_MON_VRMS_SLOW_C 0x42
#define I2CDEF_POWERAMP_MON_VRMS_SLOW_D 0x43

#define I2CDEF_POWERAMP_MON_IRMS_SLOW_A 0x44
#define I2CDEF_POWERAMP_MON_IRMS_SLOW_B 0x45
#define I2CDEF_POWERAMP_MON_IRMS_SLOW_C 0x46
#define I2CDEF_POWERAMP_MON_IRMS_SLOW_D 0x47

#define I2CDEF_POWERAMP_MON_PAVG_SLOW_A 0x48
#define I2CDEF_POWERAMP_MON_PAVG_SLOW_B 0x49
#define I2CDEF_POWERAMP_MON_PAVG_SLOW_C 0x4A
#define I2CDEF_POWERAMP_MON_PAVG_SLOW_D 0x4B

#define I2CDEF_POWERAMP_MON_PAPP_SLOW_A 0x4C
#define I2CDEF_POWERAMP_MON_PAPP_SLOW_B 0x4D
#define I2CDEF_POWERAMP_MON_PAPP_SLOW_C 0x4E
#define I2CDEF_POWERAMP_MON_PAPP_SLOW_D 0x4F


//Safety system registers
#define I2CDEF_POWERAMP_SERR_IRMS_INST_A 0x50
#define I2CDEF_POWERAMP_SERR_IRMS_INST_B 0x51
#define I2CDEF_POWERAMP_SERR_IRMS_INST_C 0x52
#define I2CDEF_POWERAMP_SERR_IRMS_INST_D 0x53
#define I2CDEF_POWERAMP_SERR_IRMS_INST_SUM 0x54

#define I2CDEF_POWERAMP_SERR_IRMS_FAST_A 0x55
#define I2CDEF_POWERAMP_SERR_IRMS_FAST_B 0x56
#define I2CDEF_POWERAMP_SERR_IRMS_FAST_C 0x57
#define I2CDEF_POWERAMP_SERR_IRMS_FAST_D 0x58
#define I2CDEF_POWERAMP_SERR_IRMS_FAST_SUM 0x59

#define I2CDEF_POWERAMP_SERR_IRMS_SLOW_A 0x5A
#define I2CDEF_POWERAMP_SERR_IRMS_SLOW_B 0x5B
#define I2CDEF_POWERAMP_SERR_IRMS_SLOW_C 0x5C
#define I2CDEF_POWERAMP_SERR_IRMS_SLOW_D 0x5D
#define I2CDEF_POWERAMP_SERR_IRMS_SLOW_SUM 0x5E

#define I2CDEF_POWERAMP_SERR_PAVG_INST_A 0x60
#define I2CDEF_POWERAMP_SERR_PAVG_INST_B 0x61
#define I2CDEF_POWERAMP_SERR_PAVG_INST_C 0x62
#define I2CDEF_POWERAMP_SERR_PAVG_INST_D 0x63
#define I2CDEF_POWERAMP_SERR_PAVG_INST_SUM 0x64

#define I2CDEF_POWERAMP_SERR_PAVG_FAST_A 0x65
#define I2CDEF_POWERAMP_SERR_PAVG_FAST_B 0x66
#define I2CDEF_POWERAMP_SERR_PAVG_FAST_C 0x67
#define I2CDEF_POWERAMP_SERR_PAVG_FAST_D 0x68
#define I2CDEF_POWERAMP_SERR_PAVG_FAST_SUM 0x69

#define I2CDEF_POWERAMP_SERR_PAVG_SLOW_A 0x6A
#define I2CDEF_POWERAMP_SERR_PAVG_SLOW_B 0x6B
#define I2CDEF_POWERAMP_SERR_PAVG_SLOW_C 0x6C
#define I2CDEF_POWERAMP_SERR_PAVG_SLOW_D 0x6D
#define I2CDEF_POWERAMP_SERR_PAVG_SLOW_SUM 0x6E

#define I2CDEF_POWERAMP_SERR_PAPP_INST_A 0x70
#define I2CDEF_POWERAMP_SERR_PAPP_INST_B 0x71
#define I2CDEF_POWERAMP_SERR_PAPP_INST_C 0x72
#define I2CDEF_POWERAMP_SERR_PAPP_INST_D 0x73
#define I2CDEF_POWERAMP_SERR_PAPP_INST_SUM 0x74

#define I2CDEF_POWERAMP_SERR_PAPP_FAST_A 0x75
#define I2CDEF_POWERAMP_SERR_PAPP_FAST_B 0x76
#define I2CDEF_POWERAMP_SERR_PAPP_FAST_C 0x77
#define I2CDEF_POWERAMP_SERR_PAPP_FAST_D 0x78
#define I2CDEF_POWERAMP_SERR_PAPP_FAST_SUM 0x79

#define I2CDEF_POWERAMP_SERR_PAPP_SLOW_A 0x7A
#define I2CDEF_POWERAMP_SERR_PAPP_SLOW_B 0x7B
#define I2CDEF_POWERAMP_SERR_PAPP_SLOW_C 0x7C
#define I2CDEF_POWERAMP_SERR_PAPP_SLOW_D 0x7D
#define I2CDEF_POWERAMP_SERR_PAPP_SLOW_SUM 0x7E

#define I2CDEF_POWERAMP_SWARN_IRMS_INST_A 0x80
#define I2CDEF_POWERAMP_SWARN_IRMS_INST_B 0x81
#define I2CDEF_POWERAMP_SWARN_IRMS_INST_C 0x82
#define I2CDEF_POWERAMP_SWARN_IRMS_INST_D 0x83
#define I2CDEF_POWERAMP_SWARN_IRMS_INST_SUM 0x84

#define I2CDEF_POWERAMP_SWARN_IRMS_FAST_A 0x85
#define I2CDEF_POWERAMP_SWARN_IRMS_FAST_B 0x86
#define I2CDEF_POWERAMP_SWARN_IRMS_FAST_C 0x87
#define I2CDEF_POWERAMP_SWARN_IRMS_FAST_D 0x88
#define I2CDEF_POWERAMP_SWARN_IRMS_FAST_SUM 0x89

#define I2CDEF_POWERAMP_SWARN_IRMS_SLOW_A 0x8A
#define I2CDEF_POWERAMP_SWARN_IRMS_SLOW_B 0x8B
#define I2CDEF_POWERAMP_SWARN_IRMS_SLOW_C 0x8C
#define I2CDEF_POWERAMP_SWARN_IRMS_SLOW_D 0x8D
#define I2CDEF_POWERAMP_SWARN_IRMS_SLOW_SUM 0x8E

#define I2CDEF_POWERAMP_SWARN_PAVG_INST_A 0x90
#define I2CDEF_POWERAMP_SWARN_PAVG_INST_B 0x91
#define I2CDEF_POWERAMP_SWARN_PAVG_INST_C 0x92
#define I2CDEF_POWERAMP_SWARN_PAVG_INST_D 0x93
#define I2CDEF_POWERAMP_SWARN_PAVG_INST_SUM 0x94

#define I2CDEF_POWERAMP_SWARN_PAVG_FAST_A 0x95
#define I2CDEF_POWERAMP_SWARN_PAVG_FAST_B 0x96
#define I2CDEF_POWERAMP_SWARN_PAVG_FAST_C 0x97
#define I2CDEF_POWERAMP_SWARN_PAVG_FAST_D 0x98
#define I2CDEF_POWERAMP_SWARN_PAVG_FAST_SUM 0x99

#define I2CDEF_POWERAMP_SWARN_PAVG_SLOW_A 0x9A
#define I2CDEF_POWERAMP_SWARN_PAVG_SLOW_B 0x9B
#define I2CDEF_POWERAMP_SWARN_PAVG_SLOW_C 0x9C
#define I2CDEF_POWERAMP_SWARN_PAVG_SLOW_D 0x9D
#define I2CDEF_POWERAMP_SWARN_PAVG_SLOW_SUM 0x9E

#define I2CDEF_POWERAMP_SWARN_PAPP_INST_A 0xA0
#define I2CDEF_POWERAMP_SWARN_PAPP_INST_B 0xA1
#define I2CDEF_POWERAMP_SWARN_PAPP_INST_C 0xA2
#define I2CDEF_POWERAMP_SWARN_PAPP_INST_D 0xA3
#define I2CDEF_POWERAMP_SWARN_PAPP_INST_SUM 0xA4

#define I2CDEF_POWERAMP_SWARN_PAPP_FAST_A 0xA5
#define I2CDEF_POWERAMP_SWARN_PAPP_FAST_B 0xA6
#define I2CDEF_POWERAMP_SWARN_PAPP_FAST_C 0xA7
#define I2CDEF_POWERAMP_SWARN_PAPP_FAST_D 0xA8
#define I2CDEF_POWERAMP_SWARN_PAPP_FAST_SUM 0xA9

#define I2CDEF_POWERAMP_SWARN_PAPP_SLOW_A 0xAA
#define I2CDEF_POWERAMP_SWARN_PAPP_SLOW_B 0xAB
#define I2CDEF_POWERAMP_SWARN_PAPP_SLOW_C 0xAC
#define I2CDEF_POWERAMP_SWARN_PAPP_SLOW_D 0xAD
#define I2CDEF_POWERAMP_SWARN_PAPP_SLOW_SUM 0xAE

#define I2CDEF_POWERAMP_SAFETY_STATUS 0xB0

#define I2CDEF_POWERAMP_SERR_SOURCE 0xB1

#define I2CDEF_POWERAMP_SWARN_SOURCE 0xB2


//Misc registers
#define I2CDEF_POWERAMP_MODULE_ID 0xFF


#endif /* INC_I2C_DEFINES_POWERAMP_H_ */
