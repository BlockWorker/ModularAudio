/*
 * iot_driver.h
 *
 *  Created on: Mar 2, 2025
 *      Author: Alex
 */

#ifndef INC_IOT_DRIVER_H_
#define INC_IOT_DRIVER_H_

#include "main.h"
#include "iot_defines.h"


//size of UART receive circular buffer in bytes, must be a power of two
#define IOT_RXBUF_SIZE 8192

//delay between command end and next command start, in ms
#define IOT_CMD_DELAY 150
//timeout duration for commands after their start, in ms
#define IOT_CMD_TIMEOUT 5000


extern bool iot_driver_init_complete;


void IOT_UARTEx_RxEventCallback(UART_HandleTypeDef* huart, uint16_t Size);
void IOT_UART_TxCpltCallback(UART_HandleTypeDef* huart);

HAL_StatusTypeDef IOT_Init();
void IOT_Update();


#endif /* INC_IOT_DRIVER_H_ */
