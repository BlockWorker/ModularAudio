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
//initial values calculated for 8 MHz I2CCLK: stretch timeout 8ms in release, 16ms in debug, low timeout 16ms in release, 65ms in debug
#ifdef DEBUG
#define I2C_SCL_STRETCH_TIMEOUT (63 << I2C_TIMEOUTR_TIMEOUTB_Pos)
#define I2C_SCL_LOW_TIMEOUT (255 << I2C_TIMEOUTR_TIMEOUTA_Pos)
#else
#define I2C_SCL_STRETCH_TIMEOUT (31 << I2C_TIMEOUTR_TIMEOUTB_Pos)
#define I2C_SCL_LOW_TIMEOUT (63 << I2C_TIMEOUTR_TIMEOUTA_Pos)
#endif

//timeout for non-idle state, in main loop cycles
#ifdef DEBUG
#define I2C_NONIDLE_TIMEOUT 10
#else
#define I2C_NONIDLE_TIMEOUT 3
#endif

//timeout for busy peripheral despite idle driver, in main loop cycles
#ifdef DEBUG
#define I2C_PERIPHERAL_BUSY_TIMEOUT 20
#else
#define I2C_PERIPHERAL_BUSY_TIMEOUT 10
#endif

//I2C instance to use
#define I2C_INSTANCE hi2c3
#define I2C_INT_PORT I2C3_INT_N_GPIO_Port
#define I2C_INT_PIN I2C3_INT_N_Pin
#define I2C_FORCE_RESET() __HAL_RCC_I2C3_FORCE_RESET()
#define I2C_RELEASE_RESET() __HAL_RCC_I2C3_RELEASE_RESET()


HAL_StatusTypeDef I2C_Init();
void I2C_TriggerInterrupt(uint8_t interrupt_bit);
void I2C_LoopUpdate();

void I2C_TimeoutISR();


#endif /* INC_I2C_H_ */
