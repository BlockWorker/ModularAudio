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

//maximum length of album/artist/title strings (including zero termination)
#define BT_AVRCP_META_LENGTH 256
//maximum length of non-metadata state strings (including zero termination)
#define BT_STATE_STR_LENGTH 128

//period of status reads, in main loop cycles
#define BT_STATUS_READ_PERIOD 500


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
} BT_DriverState;


//whether the driver has been initialised successfully
extern bool bt_driver_init_complete;
//whether OTA firmware updates are enabled
extern bool bt_driver_ota_upgrade_enabled;

extern BT_DriverState bt_driver_state;


void BT_UARTEx_RxEventCallback(UART_HandleTypeDef* huart, uint16_t Size);
void BT_UART_TxCpltCallback(UART_HandleTypeDef* huart);


HAL_StatusTypeDef BT_EnableOTAUpgrade();

HAL_StatusTypeDef BT_Init();
void BT_Update(uint32_t loop_counter);


#endif /* INC_BT_DRIVER_H_ */
