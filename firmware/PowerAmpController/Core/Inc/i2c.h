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

//timeouts for clock extension and low clock, in units of 2048 I2CCLK cycles (not to be confused with SCL cycles!)
//initial values calculated for 8 MHz I2CCLK: 4ms stretch timeout, 8ms low timeout
#define I2C_SCL_STRETCH_TIMEOUT (15 << I2C_TIMEOUTR_TIMEOUTB_Pos)
#define I2C_SCL_LOW_TIMEOUT (31 << I2C_TIMEOUTR_TIMEOUTA_Pos)

//timeout for non-idle state, in main loop cycles
#define I2C_NONIDLE_TIMEOUT 3


HAL_StatusTypeDef I2C_Init();
void I2C_TriggerInterrupt(uint8_t interrupt_bit);
void I2C_LoopUpdate();

void I2C_TimeoutISR();


#endif /* INC_I2C_H_ */
