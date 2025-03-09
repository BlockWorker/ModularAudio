/*
 * uart_host.h
 *
 *  Created on: Mar 8, 2025
 *      Author: Alex
 */

#ifndef INC_UART_HOST_H_
#define INC_UART_HOST_H_

#include "main.h"
#include "uart_defines_btrx.h"


//size of UART receive circular buffer in bytes, must be a power of two
#define UARTH_RXBUF_SIZE 8192

//maximum number of retransmit attempts for notifications
#define UARTH_NOTIF_RETRANSMIT_ATTEMPTS 3

//period between (unforced) change notification checks, in main loop cycles
#define UARTH_CHANGE_NOTIF_CHECK_PERIOD 100


HAL_StatusTypeDef UARTH_Notification_Event_MCUReset();
HAL_StatusTypeDef UARTH_Notification_Event_Error(uint16_t error_code, bool high_priority);
HAL_StatusTypeDef UARTH_Notification_Event_BTReset();

void UARTH_UARTEx_RxEventCallback(UART_HandleTypeDef* huart, uint16_t Size);
void UARTH_UART_TxCpltCallback(UART_HandleTypeDef* huart);
void UARTH_UART_ErrorCallback(UART_HandleTypeDef* huart);

void UARTH_ForceCheckChangeNotification();

HAL_StatusTypeDef UARTH_Init();
void UARTH_Update(uint32_t loop_counter);


#endif /* INC_UART_HOST_H_ */
