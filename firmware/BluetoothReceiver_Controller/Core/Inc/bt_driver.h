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
#define BT_CMD_TIMEOUT 8000
//maximum retransmit attempts per command
#define BT_CMD_RETRANSMIT_ATTEMPTS 3

//maximum length of album/artist/title strings (including zero termination)
#define BT_AVRCP_META_LENGTH 256
//maximum length of non-metadata state strings (including zero termination)
#define BT_STATE_STR_LENGTH 128

//period of status reads, in main loop cycles
#define BT_STATUS_READ_PERIOD 300


typedef struct {
  bool connectable;
  bool discoverable;

  uint64_t connected_device_addr; //zero means no connection (and the following parameters are undefined)
  char connected_device_name[BT_STATE_STR_LENGTH];
  int16_t connected_device_rssi;
  uint16_t connected_device_link_quality;

  uint8_t a2dp_link_id; //zero means no link (and the following parameters are undefined)
  bool a2dp_stream_active;
  char a2dp_codec[BT_STATE_STR_LENGTH];

  uint8_t avrcp_link_id; //zero means no link (and the following parameters are undefined)
  bool avrcp_playing;
  char avrcp_title[BT_AVRCP_META_LENGTH];
  char avrcp_artist[BT_AVRCP_META_LENGTH];
  char avrcp_album[BT_AVRCP_META_LENGTH];

  uint8_t abs_volume;

  //whether the driver has been initialised successfully
  bool init_complete;
  //whether OTA firmware updates are enabled
  bool ota_upgrade_enabled;
} BT_DriverState;

extern BT_DriverState bt_driver_state;


HAL_StatusTypeDef BT_Command_CLOSE_ALL();
HAL_StatusTypeDef BT_Command_CONNECTABLE(bool connectable);
HAL_StatusTypeDef BT_Command_DISCOVERABLE(bool discoverable);
HAL_StatusTypeDef BT_Command_MUSIC(uint8_t link_id, const char* command);
HAL_StatusTypeDef BT_Command_TONE(const char* tone_string);
HAL_StatusTypeDef BT_Command_VOLUME(uint8_t link_id, uint8_t volume);

void BT_UARTEx_RxEventCallback(UART_HandleTypeDef* huart, uint16_t Size);
void BT_UART_TxCpltCallback(UART_HandleTypeDef* huart);
void BT_UART_ErrorCallback(UART_HandleTypeDef* huart);

HAL_StatusTypeDef BT_EnableOTAUpgrade();

HAL_StatusTypeDef BT_Init();
void BT_Update(uint32_t loop_counter);


#endif /* INC_BT_DRIVER_H_ */
