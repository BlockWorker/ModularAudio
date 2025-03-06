/*
 * bt_driver.h
 *
 *  Created on: Mar 2, 2025
 *      Author: Alex
 */

#ifndef INC_BT_DRIVER_H_
#define INC_BT_DRIVER_H_

#include "main.h"
#include "bt_defines.h"


//size of UART receive circular buffer in bytes, must be a power of two
#define BT_RXBUF_SIZE 8192

//delay between command end and next command start, in ms
#define BT_CMD_DELAY 150
//timeout duration for commands after their start, in ms
#define BT_CMD_TIMEOUT 5000


typedef struct {

};


extern bool bt_driver_init_complete;


void BT_UARTEx_RxEventCallback(UART_HandleTypeDef* huart, uint16_t Size);
void BT_UART_TxCpltCallback(UART_HandleTypeDef* huart);

HAL_StatusTypeDef BT_Init();
void BT_Update();


#endif /* INC_BT_DRIVER_H_ */
