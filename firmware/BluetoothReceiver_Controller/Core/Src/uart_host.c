/*
 * uart_host.c
 *
 *  Created on: Mar 8, 2025
 *      Author: Alex
 */

#include "uart_host.h"
#include "bt_driver.h"
#include <stdlib.h>


#define UARTH_PARSEBUF_SIZE 2048
#define UARTH_NOTIFBUF_SIZE 512

#if (UARTH_NOTIFBUF_SIZE < 2 * BT_AVRCP_META_LENGTH || UARTH_NOTIFBUF_SIZE < 2 * BT_STATE_STR_LENGTH)
#error "Notification buffers must be large enough to definitely fit readback strings!"
#endif


typedef struct _notif_queue_item {
  uint8_t data[UARTH_NOTIFBUF_SIZE];
  uint32_t length;
  uint8_t retransmit_attempts;
  struct _notif_queue_item* next;
} UARTH_NotificationQueueItem;


//UART receive buffer
static uint8_t _rx_buffer[UARTH_RXBUF_SIZE] = { 0 };
//buffer offset of start of current receive call
static uint32_t _rx_buffer_write_offset = 0;
//buffer offset of the next byte to be read for parsing
static uint32_t _rx_buffer_read_offset = 0;
//whether the next read byte should be escaped
static bool _rx_escape_active = false;
//whether the bytes up to the next start byte should be ignored
static bool _rx_skip_to_start = true;

//command parse buffer holding current command (or part if not complete yet)
static uint8_t _parse_buffer[UARTH_PARSEBUF_SIZE] __attribute__((aligned(2))) = { 0 };
//parse buffer offset of next byte to be written
static uint32_t _parse_buffer_write_offset = 0;
//error to send in case of current command parse failure
static uint16_t _parse_failure_error_code = UARTDEF_BTRX_ERROR_UART_FORMAT_ERROR;

//notification preparation buffer holding the current notification (not link-layer encoded yet, without start and end bytes)
static uint8_t _notif_prep_buffer[UARTH_NOTIFBUF_SIZE] __attribute__((aligned(2))) = { 0 };
//head and tail pointers of response queue
static UARTH_NotificationQueueItem* _notif_queue_head = NULL;
static UARTH_NotificationQueueItem* _notif_queue_tail = NULL;
//transmit buffer for the current notification
static uint8_t _notif_tx_buffer[UARTH_NOTIFBUF_SIZE] = { 0 };
//whether we are ready to transmit the next notification
static bool _notif_transmit_ready = true;

//whether there was a UART error since the last status read
static bool _uart_error_since_last_status = false;

//mask of enabled change notifications
static uint16_t _change_notification_mask = UARTDEF_BTRX_NOTIF_MASK_DEFAULT_VALUE;
//whether a change notification check is forced on the next update
static bool _force_change_notification_check = false;
//state seen at last change notification check
static BT_DriverState _last_check_state;

//whether an interrupt handler has detected an error, requiring a reset
static bool _interrupt_error = false;


/******************************************************************************************
 * CRC CALCULATIONS
 ******************************************************************************************/

static const uint16_t _uart_crc_table[256] = {
  0x0000, 0x1FB7, 0x3F6E, 0x20D9, 0x7EDC, 0x616B, 0x41B2, 0x5E05, 0xFDB8, 0xE20F, 0xC2D6, 0xDD61, 0x8364, 0x9CD3, 0xBC0A, 0xA3BD,
  0xE4C7, 0xFB70, 0xDBA9, 0xC41E, 0x9A1B, 0x85AC, 0xA575, 0xBAC2, 0x197F, 0x06C8, 0x2611, 0x39A6, 0x67A3, 0x7814, 0x58CD, 0x477A,
  0xD639, 0xC98E, 0xE957, 0xF6E0, 0xA8E5, 0xB752, 0x978B, 0x883C, 0x2B81, 0x3436, 0x14EF, 0x0B58, 0x555D, 0x4AEA, 0x6A33, 0x7584,
  0x32FE, 0x2D49, 0x0D90, 0x1227, 0x4C22, 0x5395, 0x734C, 0x6CFB, 0xCF46, 0xD0F1, 0xF028, 0xEF9F, 0xB19A, 0xAE2D, 0x8EF4, 0x9143,
  0xB3C5, 0xAC72, 0x8CAB, 0x931C, 0xCD19, 0xD2AE, 0xF277, 0xEDC0, 0x4E7D, 0x51CA, 0x7113, 0x6EA4, 0x30A1, 0x2F16, 0x0FCF, 0x1078,
  0x5702, 0x48B5, 0x686C, 0x77DB, 0x29DE, 0x3669, 0x16B0, 0x0907, 0xAABA, 0xB50D, 0x95D4, 0x8A63, 0xD466, 0xCBD1, 0xEB08, 0xF4BF,
  0x65FC, 0x7A4B, 0x5A92, 0x4525, 0x1B20, 0x0497, 0x244E, 0x3BF9, 0x9844, 0x87F3, 0xA72A, 0xB89D, 0xE698, 0xF92F, 0xD9F6, 0xC641,
  0x813B, 0x9E8C, 0xBE55, 0xA1E2, 0xFFE7, 0xE050, 0xC089, 0xDF3E, 0x7C83, 0x6334, 0x43ED, 0x5C5A, 0x025F, 0x1DE8, 0x3D31, 0x2286,
  0x783D, 0x678A, 0x4753, 0x58E4, 0x06E1, 0x1956, 0x398F, 0x2638, 0x8585, 0x9A32, 0xBAEB, 0xA55C, 0xFB59, 0xE4EE, 0xC437, 0xDB80,
  0x9CFA, 0x834D, 0xA394, 0xBC23, 0xE226, 0xFD91, 0xDD48, 0xC2FF, 0x6142, 0x7EF5, 0x5E2C, 0x419B, 0x1F9E, 0x0029, 0x20F0, 0x3F47,
  0xAE04, 0xB1B3, 0x916A, 0x8EDD, 0xD0D8, 0xCF6F, 0xEFB6, 0xF001, 0x53BC, 0x4C0B, 0x6CD2, 0x7365, 0x2D60, 0x32D7, 0x120E, 0x0DB9,
  0x4AC3, 0x5574, 0x75AD, 0x6A1A, 0x341F, 0x2BA8, 0x0B71, 0x14C6, 0xB77B, 0xA8CC, 0x8815, 0x97A2, 0xC9A7, 0xD610, 0xF6C9, 0xE97E,
  0xCBF8, 0xD44F, 0xF496, 0xEB21, 0xB524, 0xAA93, 0x8A4A, 0x95FD, 0x3640, 0x29F7, 0x092E, 0x1699, 0x489C, 0x572B, 0x77F2, 0x6845,
  0x2F3F, 0x3088, 0x1051, 0x0FE6, 0x51E3, 0x4E54, 0x6E8D, 0x713A, 0xD287, 0xCD30, 0xEDE9, 0xF25E, 0xAC5B, 0xB3EC, 0x9335, 0x8C82,
  0x1DC1, 0x0276, 0x22AF, 0x3D18, 0x631D, 0x7CAA, 0x5C73, 0x43C4, 0xE079, 0xFFCE, 0xDF17, 0xC0A0, 0x9EA5, 0x8112, 0xA1CB, 0xBE7C,
  0xF906, 0xE6B1, 0xC668, 0xD9DF, 0x87DA, 0x986D, 0xB8B4, 0xA703, 0x04BE, 0x1B09, 0x3BD0, 0x2467, 0x7A62, 0x65D5, 0x450C, 0x5ABB
};


//calculate CRC, starting with given crc state
static uint16_t _UART_CRC_Accumulate(uint8_t* buf, uint32_t length, uint16_t crc) {
  int i;
  for (i = 0; i < length; i++) {
    crc = ((crc << 8) ^ _uart_crc_table[buf[i] ^ (uint8_t)(crc >> 8)]);
  }
  return crc;
}


/******************************************************************************************
 * RESPONSE FUNCTIONS
 ******************************************************************************************/

static UARTH_NotificationQueueItem* _UARTH_CreateNotification(int32_t length) {
  if (length <= 0) {
    //indicated error
    return NULL;
  }

  //calculate notification CRC
  uint16_t crc = _UART_CRC_Accumulate(_notif_prep_buffer, length, 0);
  //pointer for bytewise CRC access
  uint8_t* crc_ptr = (uint8_t*)&crc;

  //check if the notification fits in the buffer, starting with length + start and end bytes + 2 crc bytes
  uint32_t true_length = length + 4;
  int i;
  //count the number of data bytes that need escaping
  for (i = 0; i < length; i++) {
    uint8_t val = _notif_prep_buffer[i];
    if (val == UARTDEF_BTRX_START_BYTE || val == UARTDEF_BTRX_END_BYTE || val == UARTDEF_BTRX_ESCAPE_BYTE) {
      true_length++;
    }
  }
  //count the number of CRC bytes that need escaping
  for (i = 0; i < 2; i++) {
    uint8_t val = crc_ptr[i];
    if (val == UARTDEF_BTRX_START_BYTE || val == UARTDEF_BTRX_END_BYTE || val == UARTDEF_BTRX_ESCAPE_BYTE) {
      true_length++;
    }
  }
  if (true_length > UARTH_NOTIFBUF_SIZE) {
    //notification buffer overflow
    return NULL;
  }

  //allocate queue item
  UARTH_NotificationQueueItem* new_item = malloc(sizeof(UARTH_NotificationQueueItem));
  if (new_item == NULL) {
    //insufficient heap memory
    return NULL;
  }

  //copy notification data, link-layer encoding it
  new_item->data[0] = UARTDEF_BTRX_START_BYTE;
  int j = 1;
  for (i = 0; i < length; i++) {
    uint8_t val = _notif_prep_buffer[i];
    if (val == UARTDEF_BTRX_START_BYTE || val == UARTDEF_BTRX_END_BYTE || val == UARTDEF_BTRX_ESCAPE_BYTE) {
      //reserved byte value: prepend escape byte
      new_item->data[j++] = UARTDEF_BTRX_ESCAPE_BYTE;
    }
    //add data byte itself
    new_item->data[j++] = val;
  }
  //add CRC value, also escaped if necessary, in big endian order so that CRC(data..crc) = 0
  for (i = 1; i >= 0; i--) {
    uint8_t val = crc_ptr[i];
    if (val == UARTDEF_BTRX_START_BYTE || val == UARTDEF_BTRX_END_BYTE || val == UARTDEF_BTRX_ESCAPE_BYTE) {
      new_item->data[j++] = UARTDEF_BTRX_ESCAPE_BYTE;
    }
    new_item->data[j++] = val;
  }
  new_item->data[j] = UARTDEF_BTRX_END_BYTE;

  //populate other queue item values
  new_item->length = true_length;
  new_item->retransmit_attempts = 0;
  new_item->next = NULL;

  return new_item;
}

//put the given notification at the back of the queue
static void _UARTH_QueueNotification(UARTH_NotificationQueueItem* item) {
  if (item == NULL) {
    return;
  }

  if (_notif_queue_head == NULL || _notif_queue_tail == NULL) {
    //queue currently empty: make new item the first and only item
    _notif_queue_head = item;
  } else {
    //queue not empty: append new item to the tail
    _notif_queue_tail->next = item;
  }
  //new item becomes the tail either way
  _notif_queue_tail = item;
  item->next = NULL;
}

//put the given notification at the top of the queue, e.g. in case of an error or high-priority notification
static void _UARTH_QueueNotificationFront(UARTH_NotificationQueueItem* item) {
  if (item == NULL) {
    return;
  }

  if (_notif_queue_head == NULL || _notif_queue_tail == NULL) {
    //queue currently empty: make item the only item, i.e. also the tail
    _notif_queue_tail = item;
    item->next = NULL;
  } else {
    //queue not empty: append current head to the item
    item->next = _notif_queue_head;
  }
  //either way: item becomes the new head
  _notif_queue_head = item;
}

static UARTH_NotificationQueueItem* _UARTH_DequeueNotification() {
  if (_notif_queue_head == NULL || _notif_queue_tail == NULL) {
    //empty queue: nothing to dequeue
    return NULL;
  }

  //take item from the front (head) of the queue, next item becomes the new head
  UARTH_NotificationQueueItem* item = _notif_queue_head;
  _notif_queue_head = item->next;
  if (_notif_queue_head == NULL) {
    //removed item was the last item: clear tail pointer too
    _notif_queue_tail = NULL;
  }

  return item;
}

static void _UARTH_ClearNotificationQueue() {
  //iterate through queue from head, freeing all items
  UARTH_NotificationQueueItem* item = _notif_queue_head;
  while (item != NULL) {
    UARTH_NotificationQueueItem* next_item = item->next;
    free(item);
    item = next_item;
  }

  //clear pointers to mark queue as empty
  _notif_queue_head = NULL;
  _notif_queue_tail = NULL;
}


//insert the given string at the given position in the notification prep buffer, up to given max length, returns string length (including null termination)
static uint32_t _UARTH_NotificationInsertString(uint32_t offset, const char* source, uint32_t max_length) {
  //length of string without null termination
  uint32_t length = strlen(source);

  if (length >= max_length) {
    //limit string length (taking into account extra null byte)
    length = max_length - 1;
  }

  //copy string itself and ensure null termination
  memcpy(_notif_prep_buffer + offset, source, length);
  _notif_prep_buffer[offset + length] = 0;

  //return total length including null termination
  return length + 1;
}


//queue a notification with the given parameters - all parameters must already be prepared in _notif_prep_buffer, starting at index 1
static HAL_StatusTypeDef _UARTH_Notification(uint8_t type, uint32_t param_length, bool high_priority) {
  _notif_prep_buffer[0] = type;

  //allocate queue item
  UARTH_NotificationQueueItem* item = _UARTH_CreateNotification(param_length + 1);
  if (item == NULL) {
    return HAL_ERROR;
  }

  //add to queue
  if (high_priority) {
    _UARTH_QueueNotificationFront(item);
  } else {
    _UARTH_QueueNotification(item);
  }

  return HAL_OK;
}

HAL_StatusTypeDef UARTH_Notification_Event_MCUReset() {
  //clear notification mask only on MCU reset
  _change_notification_mask = UARTDEF_BTRX_NOTIF_MASK_DEFAULT_VALUE;

  _notif_prep_buffer[1] = UARTDEF_BTRX_EVENT_MCU_RESET;
  return _UARTH_Notification(UARTDEF_BTRX_TYPE_EVENT, 1, true);
}

static HAL_StatusTypeDef _UARTH_Notification_Event_WriteAck(uint8_t reg_addr) {
  _notif_prep_buffer[1] = UARTDEF_BTRX_EVENT_WRITE_ACK;
  _notif_prep_buffer[2] = reg_addr;
  return _UARTH_Notification(UARTDEF_BTRX_TYPE_EVENT, 2, false);
}

HAL_StatusTypeDef UARTH_Notification_Event_Error(uint16_t error_code, bool high_priority) {
  _notif_prep_buffer[1] = UARTDEF_BTRX_EVENT_ERROR;
  _notif_prep_buffer[2] = (uint8_t)error_code;
  _notif_prep_buffer[3] = (uint8_t)(error_code >> 8);
  return _UARTH_Notification(UARTDEF_BTRX_TYPE_EVENT, 3, high_priority);
}

HAL_StatusTypeDef UARTH_Notification_Event_BTReset() {
  //if BT driver is reset, initialise last seen state to initial driver state, to avoid change notifications at the start
  memcpy(&_last_check_state, &bt_driver_state, sizeof(BT_DriverState));

  _notif_prep_buffer[1] = UARTDEF_BTRX_EVENT_BT_RESET;
  return _UARTH_Notification(UARTDEF_BTRX_TYPE_EVENT, 1, true);
}

//queue a status notification (read data or change notification) for the given register
static HAL_StatusTypeDef _UARTH_Notification_Status(uint8_t type, uint8_t reg_addr) {
  uint32_t data_length;
  uint16_t temp_value;

  switch (reg_addr) {
    case UARTDEF_BTRX_STATUS:
      temp_value =
          (bt_driver_state.init_complete ? UARTDEF_BTRX_STATUS_INIT_DONE_Msk : 0) |
          (bt_driver_state.connectable ? UARTDEF_BTRX_STATUS_CONNECTABLE_Msk : 0) |
          (bt_driver_state.discoverable ? UARTDEF_BTRX_STATUS_DISCOVERABLE_Msk : 0) |
          (bt_driver_state.connected_device_addr != 0 ? UARTDEF_BTRX_STATUS_CONNECTED_Msk : 0) |
          (bt_driver_state.a2dp_link_id != 0 ? UARTDEF_BTRX_STATUS_A2DP_LINK_Msk : 0) |
          (bt_driver_state.avrcp_link_id != 0 ? UARTDEF_BTRX_STATUS_AVRCP_LINK_Msk : 0) |
          (bt_driver_state.a2dp_stream_active ? UARTDEF_BTRX_STATUS_A2DP_STREAMING_Msk : 0) |
          (bt_driver_state.avrcp_playing ? UARTDEF_BTRX_STATUS_AVRCP_PLAYING_Msk : 0) |
          (bt_driver_state.ota_upgrade_enabled ? UARTDEF_BTRX_STATUS_OTA_UPGRADE_EN_Msk : 0);
      if (type == UARTDEF_BTRX_TYPE_READ_DATA && _uart_error_since_last_status) {
        //only include and reset UART error bit for actual reads
        temp_value |= UARTDEF_BTRX_STATUS_UARTERR_Msk;
        _uart_error_since_last_status = false;
      }
      ((uint16_t*)(_notif_prep_buffer + 2))[0] = temp_value;
      data_length = 2;
      break;
    case UARTDEF_BTRX_VOLUME:
      _notif_prep_buffer[2] = bt_driver_state.abs_volume;
      data_length = 1;
      break;
    case UARTDEF_BTRX_TITLE:
      data_length = _UARTH_NotificationInsertString(2, bt_driver_state.avrcp_title, BT_AVRCP_META_LENGTH);
      break;
    case UARTDEF_BTRX_ARTIST:
      data_length = _UARTH_NotificationInsertString(2, bt_driver_state.avrcp_artist, BT_AVRCP_META_LENGTH);
      break;
    case UARTDEF_BTRX_ALBUM:
      data_length = _UARTH_NotificationInsertString(2, bt_driver_state.avrcp_album, BT_AVRCP_META_LENGTH);
      break;
    case UARTDEF_BTRX_DEVICE_ADDR:
      memcpy(_notif_prep_buffer + 2, &bt_driver_state.connected_device_addr, 6);
      data_length = 6;
      break;
    case UARTDEF_BTRX_DEVICE_NAME:
      data_length = _UARTH_NotificationInsertString(2, bt_driver_state.connected_device_name, BT_STATE_STR_LENGTH);
      break;
    case UARTDEF_BTRX_CONN_STATS:
      ((int16_t*)(_notif_prep_buffer + 2))[0] = bt_driver_state.connected_device_rssi;
      ((uint16_t*)(_notif_prep_buffer + 2))[1] = bt_driver_state.connected_device_link_quality;
      data_length = 4;
      break;
    case UARTDEF_BTRX_CODEC:
      data_length = _UARTH_NotificationInsertString(2, bt_driver_state.a2dp_codec, BT_STATE_STR_LENGTH);
      break;
    case UARTDEF_BTRX_NOTIF_MASK:
      ((uint16_t*)(_notif_prep_buffer + 2))[0] = _change_notification_mask;
      data_length = 2;
      break;
    case UARTDEF_BTRX_MODULE_ID:
      _notif_prep_buffer[2] = UARTDEF_BTRX_MODULE_ID_VALUE;
      data_length = 1;
      break;
    default:
      return HAL_ERROR;
  }

  _notif_prep_buffer[1] = reg_addr;

  if (type == UARTDEF_BTRX_TYPE_READ_DATA) {
    //notification queue failure is an internal UART error
    _parse_failure_error_code = UARTDEF_BTRX_ERROR_INTERNAL_UART_ERROR;
  }

  return _UARTH_Notification(type, data_length + 1, false);
}


//reset host UART driver due to an internal error
static void _UARTH_ErrorReset() {
  if (UARTH_Init() != HAL_OK) {
    //re-init fails completely: reset MCU
    HAL_NVIC_SystemReset();
  }
  _uart_error_since_last_status = true;
  UARTH_Notification_Event_Error(UARTDEF_BTRX_ERROR_INTERNAL_UART_ERROR, true);
}


/******************************************************************************************
 * COMMAND PARSING
 ******************************************************************************************/

static HAL_StatusTypeDef _UARTH_ParseRead(uint8_t reg_addr) {
  return _UARTH_Notification_Status(UARTDEF_BTRX_TYPE_READ_DATA, reg_addr);
}

static HAL_StatusTypeDef _UARTH_ParseWrite(uint8_t reg_addr, uint32_t data_length) {
  uint8_t temp_val;

  switch (reg_addr) {
    case UARTDEF_BTRX_VOLUME:
      if (data_length != 1) {
        return HAL_ERROR;
      }

      //check for valid value
      temp_val = _parse_buffer[2];
      if (temp_val > 127) {
        return HAL_ERROR;
      }

      //volume command not allowed if BT driver is not initialised or not connected to AVRCP
      if (!bt_driver_state.init_complete || bt_driver_state.avrcp_link_id == 0) {
        _parse_failure_error_code = UARTDEF_BTRX_ERROR_UART_COMMAND_NOT_ALLOWED;
        return HAL_ERROR;
      }

      //command failure is an internal (BT command) error
      _parse_failure_error_code = UARTDEF_BTRX_ERROR_INTERNAL_UART_ERROR;
      ReturnOnError(BT_Command_VOLUME(bt_driver_state.avrcp_link_id, temp_val));
      break;
    case UARTDEF_BTRX_NOTIF_MASK:
      if (data_length != 2) {
        return HAL_ERROR;
      }

      _change_notification_mask = ((uint16_t*)(_parse_buffer + 2))[0] & 0x01FFU;
      break;
    case UARTDEF_BTRX_CONTROL:
      if (data_length != 1) {
        return HAL_ERROR;
      }

      //check reset code
      temp_val = (_parse_buffer[2] & UARTDEF_BTRX_CONTROL_RESET_Msk) >> UARTDEF_BTRX_CONTROL_RESET_Pos;
      if (temp_val == UARTDEF_BTRX_CONTROL_RESET_VALUE) {
        //correct value: trigger reset
        HAL_NVIC_SystemReset();
      } else if (temp_val != 0) {
        //incorrect value: error
        return HAL_ERROR;
      }

      //enable OTA firmware upgrade if requested
      if ((_parse_buffer[2] & UARTDEF_BTRX_CONTROL_ENABLE_OTA_UPGRADE_Msk) != 0) {
        //OTA firmware upgrade enable not allowed if BT driver is not initialised or OTA upgrade is already enabled
        if (!bt_driver_state.init_complete || bt_driver_state.ota_upgrade_enabled) {
          _parse_failure_error_code = UARTDEF_BTRX_ERROR_UART_COMMAND_NOT_ALLOWED;
          return HAL_ERROR;
        }

        //command failure is an internal (BT command) error
        _parse_failure_error_code = UARTDEF_BTRX_ERROR_INTERNAL_UART_ERROR;
        ReturnOnError(BT_EnableOTAUpgrade());
      }
      break;
    case UARTDEF_BTRX_CONN_CONTROL:
      if (data_length != 1) {
        return HAL_ERROR;
      }

      //connection control commands not allowed if BT driver is not initialised
      if (!bt_driver_state.init_complete) {
        _parse_failure_error_code = UARTDEF_BTRX_ERROR_UART_COMMAND_NOT_ALLOWED;
        return HAL_ERROR;
      }

      //further failures are internal (BT command) errors
      _parse_failure_error_code = UARTDEF_BTRX_ERROR_INTERNAL_UART_ERROR;

      //turn connectable on or off if requested (off takes precedence)
      if ((_parse_buffer[2] & UARTDEF_BTRX_CONN_CONTROL_CONNECTABLE_OFF_Msk) != 0) {
        ReturnOnError(BT_Command_CONNECTABLE(false));
      } else if ((_parse_buffer[2] & UARTDEF_BTRX_CONN_CONTROL_CONNECTABLE_ON_Msk) != 0) {
        ReturnOnError(BT_Command_CONNECTABLE(true));
      }

      //turn discoverable on or off if requested (off takes precedence)
      if ((_parse_buffer[2] & UARTDEF_BTRX_CONN_CONTROL_DISCOVERABLE_OFF_Msk) != 0) {
        ReturnOnError(BT_Command_DISCOVERABLE(false));
      } else if ((_parse_buffer[2] & UARTDEF_BTRX_CONN_CONTROL_DISCOVERABLE_ON_Msk) != 0) {
        ReturnOnError(BT_Command_DISCOVERABLE(true));
      }

      //disconnect from Bluetooth device if requested
      if ((_parse_buffer[2] & UARTDEF_BTRX_CONN_CONTROL_DISCONNECT_Msk) != 0) {
        ReturnOnError(BT_Command_CLOSE_ALL());
      }
      break;
    case UARTDEF_BTRX_MEDIA_CONTROL:
      if (data_length != 1) {
        return HAL_ERROR;
      }

      //media control command not allowed if BT driver is not initialised or not connected to AVRCP
      if (!bt_driver_state.init_complete || bt_driver_state.avrcp_link_id == 0) {
        _parse_failure_error_code = UARTDEF_BTRX_ERROR_UART_COMMAND_NOT_ALLOWED;
        return HAL_ERROR;
      }

      //most further failures are internal (BT command) errors
      _parse_failure_error_code = UARTDEF_BTRX_ERROR_INTERNAL_UART_ERROR;

      //decode subcommand enum
      switch (_parse_buffer[2]) {
        case UARTDEF_BTRX_MEDIA_CONTROL_PLAY_Val:
          ReturnOnError(BT_Command_MUSIC(bt_driver_state.avrcp_link_id, "PLAY"));
          break;
        case UARTDEF_BTRX_MEDIA_CONTROL_PAUSE_Val:
          ReturnOnError(BT_Command_MUSIC(bt_driver_state.avrcp_link_id, "PAUSE"));
          break;
        case UARTDEF_BTRX_MEDIA_CONTROL_STOP_Val:
          ReturnOnError(BT_Command_MUSIC(bt_driver_state.avrcp_link_id, "STOP"));
          break;
        case UARTDEF_BTRX_MEDIA_CONTROL_FORWARD_Val:
          ReturnOnError(BT_Command_MUSIC(bt_driver_state.avrcp_link_id, "FORWARD"));
          break;
        case UARTDEF_BTRX_MEDIA_CONTROL_BACKWARD_Val:
          ReturnOnError(BT_Command_MUSIC(bt_driver_state.avrcp_link_id, "BACKWARD"));
          break;
        case UARTDEF_BTRX_MEDIA_CONTROL_FF_START_Val:
          ReturnOnError(BT_Command_MUSIC(bt_driver_state.avrcp_link_id, "FF_PRESS"));
          break;
        case UARTDEF_BTRX_MEDIA_CONTROL_FF_END_Val:
          ReturnOnError(BT_Command_MUSIC(bt_driver_state.avrcp_link_id, "FF_RELEASE"));
          break;
        case UARTDEF_BTRX_MEDIA_CONTROL_REW_START_Val:
          ReturnOnError(BT_Command_MUSIC(bt_driver_state.avrcp_link_id, "REW_PRESS"));
          break;
        case UARTDEF_BTRX_MEDIA_CONTROL_REW_END_Val:
          ReturnOnError(BT_Command_MUSIC(bt_driver_state.avrcp_link_id, "REW_RELEASE"));
          break;
        default:
          //invalid subcommand is a format error
          _parse_failure_error_code = UARTDEF_BTRX_ERROR_UART_FORMAT_ERROR;
          return HAL_ERROR;
      }
      break;
    case UARTDEF_BTRX_TONE:
      if (data_length > 240) {
        return HAL_ERROR;
      }

      //tone command not allowed if BT driver is not initialised
      if (!bt_driver_state.init_complete) {
        _parse_failure_error_code = UARTDEF_BTRX_ERROR_UART_COMMAND_NOT_ALLOWED;
        return HAL_ERROR;
      }

      //check that the string is null-terminated
      if (_parse_buffer[2 + data_length - 1] != 0) {
        return HAL_ERROR;
      }

      //command failure is an internal (BT command) error
      _parse_failure_error_code = UARTDEF_BTRX_ERROR_INTERNAL_UART_ERROR;
      ReturnOnError(BT_Command_TONE((const char*)(_parse_buffer + 2)));
      break;
    default:
      return HAL_ERROR;
  }

  //notification queue failure is an internal UART error
  _parse_failure_error_code = UARTDEF_BTRX_ERROR_INTERNAL_UART_ERROR;
  //command processed without error: queue write acknowledgement
  return _UARTH_Notification_Event_WriteAck(reg_addr);
}

static void _UARTH_ParseCommand() {
  uint32_t length = _parse_buffer_write_offset;

  //assume format error on failure, unless specified otherwise by sub-parser
  _parse_failure_error_code = UARTDEF_BTRX_ERROR_UART_FORMAT_ERROR;

  HAL_StatusTypeDef result;
  if (_UART_CRC_Accumulate(_parse_buffer, length, 0) == 0) {
    //CRC check passed: continue parsing
    switch (_parse_buffer[0]) {
      case UARTDEF_BTRX_TYPE_READ:
        if (length == 4) {
          result = _UARTH_ParseRead(_parse_buffer[1]);
        } else {
          result = HAL_ERROR;
        }
        break;
      case UARTDEF_BTRX_TYPE_WRITE:
        if (length > 4) {
          result = _UARTH_ParseWrite(_parse_buffer[1], length - 4);
        } else {
          result = HAL_ERROR;
        }
        break;
      default:
        result = HAL_ERROR;
        break;
    }
  } else {
    //CRC check failed
    _parse_failure_error_code = UARTDEF_BTRX_ERROR_UART_CRC_ERROR;
    result = HAL_ERROR;
  }

  if (result != HAL_OK) {
    _uart_error_since_last_status = true;
    UARTH_Notification_Event_Error(_parse_failure_error_code, false);
  }
}


/******************************************************************************************
 * INTERRUPT CALLBACKS
 ******************************************************************************************/

void UARTH_UARTEx_RxEventCallback(UART_HandleTypeDef* huart, uint16_t Size) {
  //calculate next call's write offset in circular buffer
  _rx_buffer_write_offset += (uint32_t)Size;
  if (_rx_buffer_write_offset >= UARTH_RXBUF_SIZE) {
    _rx_buffer_write_offset = 0;
  }

  //start next reception immediately to avoid receiver overrun (data loss)
  //single transaction limited to remaining buffer space before wrap-around, because the HAL function has no "circular buffer" concept
  if (HAL_UARTEx_ReceiveToIdle_IT(&huart1, _rx_buffer + _rx_buffer_write_offset, UARTH_RXBUF_SIZE - _rx_buffer_write_offset) != HAL_OK) {
    DEBUG_PRINTF("*** UART COMMAND RECEIVE ERROR\n");
    _interrupt_error = true;
  }
}

void UARTH_UART_TxCpltCallback(UART_HandleTypeDef* huart) {
  _notif_transmit_ready = true;
}

void UARTH_UART_ErrorCallback(UART_HandleTypeDef* huart) {
  DEBUG_PRINTF("*** UART1 internal error!\n");
  _interrupt_error = true;
}


/******************************************************************************************
 * MAIN DRIVER FUNCTIONS
 ******************************************************************************************/

static void _UARTH_ProcessReceivedData() {
  //process one byte at a time, until we reach the write pointer
  while (_rx_buffer_read_offset != _rx_buffer_write_offset) {
    //read byte from receive buffer, decode link-layer format (escaping), transfer to parse buffer, and increment offsets
    uint8_t rx_byte = _rx_buffer[_rx_buffer_read_offset++];
    if (_rx_escape_active) {
      //escape current byte: only allowed for reserved bytes
      if (rx_byte == UARTDEF_BTRX_START_BYTE || rx_byte == UARTDEF_BTRX_END_BYTE || rx_byte == UARTDEF_BTRX_ESCAPE_BYTE) {
        //reserved byte escaped: copy it to the parse buffer, unless we're skipping to the next start byte (because it should be ignored then)
        if (!_rx_skip_to_start) {
          _parse_buffer[_parse_buffer_write_offset++] = rx_byte;
        }
      } else {
        //error: non-reserved byte escaped: reset parse buffer, skip to next start
        _parse_buffer_write_offset = 0;
        _rx_skip_to_start = true;
        _uart_error_since_last_status = true;
        UARTH_Notification_Event_Error(UARTDEF_BTRX_ERROR_UART_FORMAT_ERROR, false);
      }
      _rx_escape_active = false;
    } else {
      //not escaping: normal reception logic
      if (rx_byte == UARTDEF_BTRX_ESCAPE_BYTE) {
        //escape byte received: discard it and escape the next byte
        _rx_escape_active = true;
      } else if (_rx_skip_to_start) {
        //skipping to start: ignore all non-start bytes
        if (rx_byte == UARTDEF_BTRX_START_BYTE) {
          //start byte found: ready for actual reception starting with next byte
          _parse_buffer_write_offset = 0;
          _rx_skip_to_start = false;
        }
      } else {
        //normal reception
        if (rx_byte == UARTDEF_BTRX_START_BYTE) {
          //error: unexpected start byte: reset parse buffer, receive new command starting with this byte
          _parse_buffer_write_offset = 0;
          _uart_error_since_last_status = true;
          UARTH_Notification_Event_Error(UARTDEF_BTRX_ERROR_UART_FORMAT_ERROR, false);
        } else if (rx_byte == UARTDEF_BTRX_END_BYTE) {
          //end byte found: parse buffer is done
          if (_parse_buffer_write_offset < 3) {
            //error: parse buffer empty or too short (must have 1B type + 2B crc at least): skip to next start
            _rx_skip_to_start = true;
            _uart_error_since_last_status = true;
            UARTH_Notification_Event_Error(UARTDEF_BTRX_ERROR_UART_FORMAT_ERROR, false);
          } else {
            //parse buffer has data: parse command, then reset parse buffer and skip to next start
            _UARTH_ParseCommand();
            _parse_buffer_write_offset = 0;
            _rx_skip_to_start = true;
          }
        } else {
          //non-reserved byte: copy it to the parse buffer
          _parse_buffer[_parse_buffer_write_offset++] = rx_byte;
        }
      }
    }

    //handle circular receive buffer
    if (_rx_buffer_read_offset >= UARTH_RXBUF_SIZE) {
      _rx_buffer_read_offset = 0;
    }

    if (_parse_buffer_write_offset >= UARTH_PARSEBUF_SIZE) {
      //error: parse buffer overflow: reset parse buffer, skip to next start
      DEBUG_PRINTF("*** ERROR: UART command parse buffer overflow\n");
      _parse_buffer_write_offset = 0;
      _rx_skip_to_start = true;
      _uart_error_since_last_status = true;
      UARTH_Notification_Event_Error(UARTDEF_BTRX_ERROR_UART_FORMAT_ERROR, false);
    }
  }
}

static void _UARTH_CheckChangeNotification() {
  if ((_change_notification_mask & UARTDEF_BTRX_NOTIF_MASK_STATUS_Msk) != 0) {
    //check for main status change
    if (
        bt_driver_state.init_complete != _last_check_state.init_complete ||
        bt_driver_state.connectable != _last_check_state.connectable ||
        bt_driver_state.discoverable != _last_check_state.discoverable ||
        (bt_driver_state.connected_device_addr == 0) != (_last_check_state.connected_device_addr == 0) ||
        (bt_driver_state.a2dp_link_id == 0) != (_last_check_state.a2dp_link_id == 0) ||
        (bt_driver_state.avrcp_link_id == 0) != (_last_check_state.avrcp_link_id == 0) ||
        bt_driver_state.a2dp_stream_active != _last_check_state.a2dp_stream_active ||
        bt_driver_state.avrcp_playing != _last_check_state.avrcp_playing ||
        bt_driver_state.ota_upgrade_enabled != _last_check_state.ota_upgrade_enabled
    ) {
      _UARTH_Notification_Status(UARTDEF_BTRX_TYPE_CHANGE_NOTIFICATION, UARTDEF_BTRX_STATUS);
    }
  }

  if ((_change_notification_mask & UARTDEF_BTRX_NOTIF_MASK_VOLUME_Msk) != 0) {
    //check for volume change
    if (bt_driver_state.abs_volume != _last_check_state.abs_volume) {
      _UARTH_Notification_Status(UARTDEF_BTRX_TYPE_CHANGE_NOTIFICATION, UARTDEF_BTRX_VOLUME);
    }
  }

  if ((_change_notification_mask & UARTDEF_BTRX_NOTIF_MASK_TITLE_Msk) != 0) {
    //check for title change
    if (strncmp(bt_driver_state.avrcp_title, _last_check_state.avrcp_title, BT_AVRCP_META_LENGTH) != 0) {
      _UARTH_Notification_Status(UARTDEF_BTRX_TYPE_CHANGE_NOTIFICATION, UARTDEF_BTRX_TITLE);
    }
  }

  if ((_change_notification_mask & UARTDEF_BTRX_NOTIF_MASK_ARTIST_Msk) != 0) {
    //check for artist change
    if (strncmp(bt_driver_state.avrcp_artist, _last_check_state.avrcp_artist, BT_AVRCP_META_LENGTH) != 0) {
      _UARTH_Notification_Status(UARTDEF_BTRX_TYPE_CHANGE_NOTIFICATION, UARTDEF_BTRX_ARTIST);
    }
  }

  if ((_change_notification_mask & UARTDEF_BTRX_NOTIF_MASK_ALBUM_Msk) != 0) {
    //check for album change
    if (strncmp(bt_driver_state.avrcp_album, _last_check_state.avrcp_album, BT_AVRCP_META_LENGTH) != 0) {
      _UARTH_Notification_Status(UARTDEF_BTRX_TYPE_CHANGE_NOTIFICATION, UARTDEF_BTRX_ALBUM);
    }
  }

  if ((_change_notification_mask & UARTDEF_BTRX_NOTIF_MASK_DEVICE_ADDR_Msk) != 0) {
    //check for device address change
    if (bt_driver_state.connected_device_addr != _last_check_state.connected_device_addr) {
      _UARTH_Notification_Status(UARTDEF_BTRX_TYPE_CHANGE_NOTIFICATION, UARTDEF_BTRX_DEVICE_ADDR);
    }
  }

  if ((_change_notification_mask & UARTDEF_BTRX_NOTIF_MASK_DEVICE_NAME_Msk) != 0) {
    //check for device name change
    if (strncmp(bt_driver_state.connected_device_name, _last_check_state.connected_device_name, BT_STATE_STR_LENGTH) != 0) {
      _UARTH_Notification_Status(UARTDEF_BTRX_TYPE_CHANGE_NOTIFICATION, UARTDEF_BTRX_DEVICE_NAME);
    }
  }

  if ((_change_notification_mask & UARTDEF_BTRX_NOTIF_MASK_CONN_STATS_Msk) != 0) {
    //check for connection stats change
    if (
        bt_driver_state.connected_device_rssi != _last_check_state.connected_device_rssi ||
        bt_driver_state.connected_device_link_quality != _last_check_state.connected_device_link_quality
    ) {
      _UARTH_Notification_Status(UARTDEF_BTRX_TYPE_CHANGE_NOTIFICATION, UARTDEF_BTRX_CONN_STATS);
    }
  }

  if ((_change_notification_mask & UARTDEF_BTRX_NOTIF_MASK_CODEC_Msk) != 0) {
    //check for device name change
    if (strncmp(bt_driver_state.a2dp_codec, _last_check_state.a2dp_codec, BT_STATE_STR_LENGTH) != 0) {
      _UARTH_Notification_Status(UARTDEF_BTRX_TYPE_CHANGE_NOTIFICATION, UARTDEF_BTRX_CODEC);
    }
  }

  //save current state as last checked state for next time
  memcpy(&_last_check_state, &bt_driver_state, sizeof(BT_DriverState));
}


void UARTH_ForceCheckChangeNotification() {
  _force_change_notification_check = true;
}

HAL_StatusTypeDef UARTH_Init() {
  //restore UART peripheral to idle/ready
  HAL_UART_Abort(&huart1);

  //reset all driver state
  _rx_buffer_write_offset = 0;
  _rx_buffer_read_offset = 0;
  _rx_escape_active = false;
  _rx_skip_to_start = true;
  _parse_buffer_write_offset = 0;
  _parse_failure_error_code = UARTDEF_BTRX_ERROR_UART_FORMAT_ERROR;
  _uart_error_since_last_status = false;
  _interrupt_error = false;
  _force_change_notification_check = false;

  _UARTH_ClearNotificationQueue();
  _notif_transmit_ready = true;

  //do not reset change notification mask

  //start reception of commands
  return HAL_UARTEx_ReceiveToIdle_IT(&huart1, _rx_buffer + _rx_buffer_write_offset, UARTH_RXBUF_SIZE - _rx_buffer_write_offset);
}

void UARTH_Update(uint32_t loop_counter) {
  //handle interrupt errors
  if (_interrupt_error) {
    _UARTH_ErrorReset();
    return;
  }

  //handle received data
  _UARTH_ProcessReceivedData();

  //check if a change notification should be sent, if forced or it's time for that
  if (_force_change_notification_check || loop_counter % UARTH_CHANGE_NOTIF_CHECK_PERIOD == 0) {
    _UARTH_CheckChangeNotification();
  }

  //detect ready state if interrupt failed
  if (!_notif_transmit_ready && (HAL_UART_GetState(&huart1) & 0xE5) == HAL_UART_STATE_READY) {
    _notif_transmit_ready = true;
  }

  //send next notification if ready
  if (_notif_transmit_ready) {
    //dequeue notification, if there is one
    UARTH_NotificationQueueItem* item = _UARTH_DequeueNotification();
    if (item != NULL) {
      //notification queued: sanity-check length first
      uint32_t length = item->length;
      if (length <= UARTH_NOTIFBUF_SIZE) {
        //length okay: proceed, extract queue item values
        memcpy(_notif_tx_buffer, item->data, length);

        //start command
        if (HAL_UART_Transmit_IT(&huart1, _notif_tx_buffer, length) == HAL_OK) {
          //send success: free queue item memory, mark as not ready
          free(item);
          _notif_transmit_ready = false;
        } else {
          //error: requeue item to try again next time
          DEBUG_PRINTF("*** UART notification transmit error!\n");
          if (item->retransmit_attempts >= UARTH_NOTIF_RETRANSMIT_ATTEMPTS) {
            //maximum retransmit attempts exceeded: reset
            _UARTH_ErrorReset();
            return;
          } else {
            //attempt retransmit
            item->retransmit_attempts++;
            _UARTH_QueueNotificationFront(item);
          }
        }
      }
    }
  }
}


