/*
 * i2c.h
 *
 *  Created on: Oct 15, 2024
 *      Author: Alex
 */

#ifndef INC_I2C_H_
#define INC_I2C_H_


#include "main.h"
#include "i2c_defines_poweramp.h"


//size of virtual read/write buffers, in bytes - equals maximum virtual register size
#define I2C_VIRT_BUFFER_SIZE 4


HAL_StatusTypeDef I2C_Init();
void I2C_TriggerInterrupt(uint8_t interrupt_bit);


#endif /* INC_I2C_H_ */
