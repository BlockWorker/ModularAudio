/*
 * uart_host.c
 *
 *  Created on: Mar 8, 2025
 *      Author: Alex
 */

#include "uart_host.h"
#include "bms.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>


#define UARTH_PARSEBUF_SIZE 512
#define UARTH_NOTIFBUF_SIZE 512


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
static uint16_t _parse_failure_error_code = UARTDEF_BMS_ERROR_UART_FORMAT_ERROR;

//notification preparation buffer holding the current notification (not link-layer encoded yet, without start and end bytes)
static uint8_t _notif_prep_buffer[UARTH_NOTIFBUF_SIZE] __attribute__((aligned(2))) = { 0 };
//head and tail pointers of response queue
static UARTH_NotificationQueueItem* _notif_queue_head = NULL;
static UARTH_NotificationQueueItem* _notif_queue_tail = NULL;
//transmit buffer for the current notification
static uint8_t _notif_tx_buffer[UARTH_NOTIFBUF_SIZE] = { 0 };
//whether we are ready to transmit the next notification
static bool _notif_transmit_ready = true;

//whether there was a BMS I2C/checksum error since the last status read
static bool _bms_error_since_last_status = false;
//whether there was a Flash error since the last status read
static bool _flash_error_since_last_status = false;
//whether there was a UART error since the last status read
static bool _uart_error_since_last_status = false;

//mask of enabled change notifications
static uint16_t _change_notification_mask = UARTDEF_BMS_NOTIF_MASK_DEFAULT_VALUE;
//whether a change notification check is forced on the next update
static bool _force_change_notification_check = false;
//state seen at last change notification check
static BMS_Status _last_check_status;
static BMS_Measurements _last_check_measurements;
static float _last_check_battery_health;


//whether an interrupt handler has detected an error, requiring a reset
static bool _interrupt_error = false;


bool uarth_ignore_hardware_errors = false;
bool uarth_ignore_one_hardware_error = false;


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
    if (val == UARTDEF_BMS_START_BYTE || val == UARTDEF_BMS_END_BYTE || val == UARTDEF_BMS_ESCAPE_BYTE) {
      true_length++;
    }
  }
  //count the number of CRC bytes that need escaping
  for (i = 0; i < 2; i++) {
    uint8_t val = crc_ptr[i];
    if (val == UARTDEF_BMS_START_BYTE || val == UARTDEF_BMS_END_BYTE || val == UARTDEF_BMS_ESCAPE_BYTE) {
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
  new_item->data[0] = UARTDEF_BMS_START_BYTE;
  int j = 1;
  for (i = 0; i < length; i++) {
    uint8_t val = _notif_prep_buffer[i];
    if (val == UARTDEF_BMS_START_BYTE || val == UARTDEF_BMS_END_BYTE || val == UARTDEF_BMS_ESCAPE_BYTE) {
      //reserved byte value: prepend escape byte
      new_item->data[j++] = UARTDEF_BMS_ESCAPE_BYTE;
    }
    //add data byte itself
    new_item->data[j++] = val;
  }
  //add CRC value, also escaped if necessary, in big endian order so that CRC(data..crc) = 0
  for (i = 1; i >= 0; i--) {
    uint8_t val = crc_ptr[i];
    if (val == UARTDEF_BMS_START_BYTE || val == UARTDEF_BMS_END_BYTE || val == UARTDEF_BMS_ESCAPE_BYTE) {
      new_item->data[j++] = UARTDEF_BMS_ESCAPE_BYTE;
    }
    new_item->data[j++] = val;
  }
  new_item->data[j] = UARTDEF_BMS_END_BYTE;

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
  _change_notification_mask = UARTDEF_BMS_NOTIF_MASK_DEFAULT_VALUE;

  _notif_prep_buffer[1] = UARTDEF_BMS_EVENT_MCU_RESET;
  return _UARTH_Notification(UARTDEF_BMS_TYPE_EVENT, 1, true);
}

static HAL_StatusTypeDef _UARTH_Notification_Event_WriteAck(uint8_t reg_addr) {
  _notif_prep_buffer[1] = UARTDEF_BMS_EVENT_WRITE_ACK;
  _notif_prep_buffer[2] = reg_addr;
  return _UARTH_Notification(UARTDEF_BMS_TYPE_EVENT, 2, false);
}

HAL_StatusTypeDef UARTH_Notification_Event_Error(uint16_t error_code, bool high_priority) {
  switch(error_code) {
    case UARTDEF_BMS_ERROR_BMS_I2C_ERROR:
      _bms_error_since_last_status = true;
      break;
    case UARTDEF_BMS_ERROR_FLASH_ERROR:
      _flash_error_since_last_status = true;
      break;
    case UARTDEF_BMS_ERROR_UART_FORMAT_ERROR:
    case UARTDEF_BMS_ERROR_INTERNAL_UART_ERROR:
    case UARTDEF_BMS_ERROR_UART_COMMAND_NOT_ALLOWED:
    case UARTDEF_BMS_ERROR_UART_CRC_ERROR:
      _uart_error_since_last_status = true;
      break;
  }

  _notif_prep_buffer[1] = UARTDEF_BMS_EVENT_ERROR;
  _notif_prep_buffer[2] = (uint8_t)error_code;
  _notif_prep_buffer[3] = (uint8_t)(error_code >> 8);
  return _UARTH_Notification(UARTDEF_BMS_TYPE_EVENT, 3, high_priority);
}

//queue a status notification (read data or change notification) for the given register
static HAL_StatusTypeDef _UARTH_Notification_Status(uint8_t type, uint8_t reg_addr) {
  uint32_t data_length;
  uint8_t temp_value_8;
  uint16_t temp_value_16;
  uint32_t temp_value_32;
  int i;

  switch (reg_addr) {
    case UARTDEF_BMS_STATUS:
      temp_value_8 =
          ((bms_status.safety_faults._all != 0) ? UARTDEF_BMS_STATUS_FAULT_Msk : 0) |
          ((bms_status.safety_alerts._all != 0) ? UARTDEF_BMS_STATUS_ALERT_Msk : 0) |
          ((bms_status.chg_force_off) ? UARTDEF_BMS_STATUS_CHG_FAULT_Msk : 0) |
          ((bms_status.timed_shutdown_type != BMSSHDN_NONE) ? UARTDEF_BMS_STATUS_SHUTDOWN_Msk : 0);
      if (type == UARTDEF_BMS_TYPE_READ_DATA) {
        //only include and reset error bits for actual reads
        if (_bms_error_since_last_status) {
          temp_value_8 |= UARTDEF_BMS_STATUS_BMS_I2C_ERR_Msk;
          _bms_error_since_last_status = false;
        }
        if (_flash_error_since_last_status) {
          temp_value_8 |= UARTDEF_BMS_STATUS_FLASH_ERR_Msk;
          _flash_error_since_last_status = false;
        }
        if (_uart_error_since_last_status) {
          temp_value_8 |= UARTDEF_BMS_STATUS_UARTERR_Msk;
          _uart_error_since_last_status = false;
        }
      }
      _notif_prep_buffer[2] = temp_value_8;
      data_length = 1;
      break;
    case UARTDEF_BMS_STACK_VOLTAGE:
      ((uint16_t*)(_notif_prep_buffer + 2))[0] = bms_measurements.voltage_stack;
      data_length = 2;
      break;
    case UARTDEF_BMS_CELL_VOLTAGES:
      for (i = 0; i < BMS_CELLS_SERIES; i++) {
        ((int16_t*)(_notif_prep_buffer + 2))[i] = bms_measurements.voltage_cells[i];
      }
      data_length = 2 * BMS_CELLS_SERIES;
      break;
    case UARTDEF_BMS_CURRENT:
      memcpy(_notif_prep_buffer + 2, &bms_measurements.current, sizeof(int32_t));
      data_length = 4;
      break;
    case UARTDEF_BMS_SOC_FRACTION:
      memcpy(_notif_prep_buffer + 2, &bms_measurements.soc_fraction, sizeof(float));
      if (isnanf(bms_measurements.soc_fraction)) {
        //no valid estimation
        _notif_prep_buffer[6] = UARTDEF_BMS_SOC_CONFIDENCE_INVALID;
      } else {
        //estimation valid: return confidence level converted to UARTDEF enum value
        _notif_prep_buffer[6] = (uint8_t)bms_measurements.soc_precisionlevel + 1;
      }
      data_length = 5;
      break;
    case UARTDEF_BMS_SOC_ENERGY:
      memcpy(_notif_prep_buffer + 2, &bms_measurements.soc_energy, sizeof(float));
      if (isnanf(bms_measurements.soc_energy)) {
        //no valid estimation
        _notif_prep_buffer[6] = UARTDEF_BMS_SOC_CONFIDENCE_INVALID;
      } else {
        //estimation valid: return confidence level converted to UARTDEF enum value
        _notif_prep_buffer[6] = (uint8_t)bms_measurements.soc_precisionlevel + 1;
      }
      data_length = 5;
      break;
    case UARTDEF_BMS_HEALTH:
      memcpy(_notif_prep_buffer + 2, bms_soc_battery_health_ptr, sizeof(float));
      data_length = 4;
      break;
    case UARTDEF_BMS_BAT_TEMP:
      ((int16_t*)(_notif_prep_buffer + 2))[0] = bms_measurements.bat_temp;
      data_length = 2;
      break;
    case UARTDEF_BMS_INT_TEMP:
      ((int16_t*)(_notif_prep_buffer + 2))[0] = bms_measurements.internal_temp;
      data_length = 2;
      break;
    case UARTDEF_BMS_ALERTS:
      ((uint16_t*)(_notif_prep_buffer + 2))[0] = bms_status.safety_alerts._all;
      data_length = 2;
      break;
    case UARTDEF_BMS_FAULTS:
      ((uint16_t*)(_notif_prep_buffer + 2))[0] = bms_status.safety_faults._all;
      data_length = 2;
      break;
    case UARTDEF_BMS_SHUTDOWN:
      _notif_prep_buffer[2] = (uint8_t)bms_status.timed_shutdown_type;
      temp_value_16 = bms_status.timed_shutdown_time * MAIN_LOOP_PERIOD_MS;
      memcpy(_notif_prep_buffer + 3, &temp_value_16, sizeof(uint16_t));
      data_length = 3;
      break;
    case UARTDEF_BMS_NOTIF_MASK:
      ((uint16_t*)(_notif_prep_buffer + 2))[0] = _change_notification_mask;
      data_length = 2;
      break;
    case UARTDEF_BMS_CONTROL:
      _notif_prep_buffer[2] = BMS_GetHostRequestedShutdown() ? UARTDEF_BMS_CONTROL_REQ_SHUTDOWN_Msk : 0;
      data_length = 1;
      break;
    case UARTDEF_BMS_CELLS_SERIES:
      _notif_prep_buffer[2] = BMS_CELLS_SERIES;
      data_length = 1;
      break;
    case UARTDEF_BMS_CELLS_PARALLEL:
      _notif_prep_buffer[2] = BMS_CELLS_PARALLEL;
      data_length = 1;
      break;
    case UARTDEF_BMS_MIN_VOLTAGE:
      ((uint16_t*)(_notif_prep_buffer + 2))[0] = BMS_MIN_DSG_VOLTAGE;
      data_length = 2;
      break;
    case UARTDEF_BMS_MAX_VOLTAGE:
      ((uint16_t*)(_notif_prep_buffer + 2))[0] = BMS_MAX_CHG_VOLTAGE;
      data_length = 2;
      break;
    case UARTDEF_BMS_MAX_DSG_CURRENT:
      temp_value_32 = BMS_MAX_DSG_CURRENT;
      memcpy(_notif_prep_buffer + 2, &temp_value_32, sizeof(uint32_t));
      data_length = 4;
      break;
    case UARTDEF_BMS_PEAK_DSG_CURRENT:
      temp_value_32 = BMS_PEAK_DSG_CURRENT;
      memcpy(_notif_prep_buffer + 2, &temp_value_32, sizeof(uint32_t));
      data_length = 4;
      break;
    case UARTDEF_BMS_MAX_CHG_CURRENT:
      temp_value_32 = BMS_MAX_CHG_CURRENT;
      memcpy(_notif_prep_buffer + 2, &temp_value_32, sizeof(uint32_t));
      data_length = 4;
      break;
    case UARTDEF_BMS_MODULE_ID:
      _notif_prep_buffer[2] = UARTDEF_BMS_MODULE_ID_VALUE;
      data_length = 1;
      break;
    default:
      return HAL_ERROR;
  }

  _notif_prep_buffer[1] = reg_addr;

  if (type == UARTDEF_BMS_TYPE_READ_DATA) {
    //notification queue failure is an internal UART error
    _parse_failure_error_code = UARTDEF_BMS_ERROR_INTERNAL_UART_ERROR;
  }

  return _UARTH_Notification(type, data_length + 1, false);
}


//reset host UART driver due to an internal error
static void _UARTH_ErrorReset() {
  if (UARTH_Init() != HAL_OK) {
    //re-init fails completely: reset MCU
    HAL_NVIC_SystemReset();
  }
  UARTH_Notification_Event_Error(UARTDEF_BMS_ERROR_INTERNAL_UART_ERROR, true);
}


/******************************************************************************************
 * COMMAND PARSING
 ******************************************************************************************/

static HAL_StatusTypeDef _UARTH_ParseRead(uint8_t reg_addr) {
  return _UARTH_Notification_Status(UARTDEF_BMS_TYPE_READ_DATA, reg_addr);
}

static HAL_StatusTypeDef _UARTH_ParseWrite(uint8_t reg_addr, uint32_t data_length) {
  uint8_t temp_val_8;
  float temp_val_f;

  switch (reg_addr) {
    case UARTDEF_BMS_HEALTH:
      if (data_length != 4) {
        return HAL_ERROR;
      }

      //parse float
      memcpy(&temp_val_f, _parse_buffer + 2, sizeof(float));

      //check for valid value
      if (isnanf(temp_val_f) || temp_val_f < 0.1f || temp_val_f > 1.0f) {
        return HAL_ERROR;
      }

      if (BMS_SetBatteryHealth(temp_val_f) != HAL_OK) {
        //write error is a flash error
        _parse_failure_error_code = UARTDEF_BMS_ERROR_FLASH_ERROR;
        return HAL_ERROR;
      }
      break;
    case UARTDEF_BMS_NOTIF_MASK:
      if (data_length != 2) {
        return HAL_ERROR;
      }

      _change_notification_mask = ((uint16_t*)(_parse_buffer + 2))[0] & 0x0FFFU;
      break;
    case UARTDEF_BMS_CONTROL:
      if (data_length != 1) {
        return HAL_ERROR;
      }

      //check reset code
      temp_val_8 = (_parse_buffer[2] & UARTDEF_BMS_CONTROL_RESET_Msk) >> UARTDEF_BMS_CONTROL_RESET_Pos;
      if (temp_val_8 == UARTDEF_BMS_CONTROL_RESET_VALUE) {
        //correct value: trigger reset
        HAL_NVIC_SystemReset();
      } else if (temp_val_8 != 0) {
        //incorrect value: error
        return HAL_ERROR;
      }

      //clear latched charge fault if requested
      if ((_parse_buffer[2] & UARTDEF_BMS_CONTROL_CLEAR_CHG_FAULT_Msk) != 0) {
        bms_status.chg_force_off = false;
      }

      //start or cancel host-requested timed shutdown, based on bit state
      if ((_parse_buffer[2] & UARTDEF_BMS_CONTROL_REQ_SHUTDOWN_Msk) != 0) {
        BMS_StartHostRequestedShutdown();
      } else {
        BMS_CancelHostRequestedShutdown();
      }

      //initiate full shutdown if requested
      if ((_parse_buffer[2] & UARTDEF_BMS_CONTROL_FULL_SHUTDOWN_Msk) != 0) {
        if (BMS_EnterShutdown(false) != HAL_OK) {
          _parse_failure_error_code = UARTDEF_BMS_ERROR_INTERNAL_UART_ERROR;
          return HAL_ERROR;
        }
      }
      break;
    default:
      return HAL_ERROR;
  }

  //notification queue failure is an internal UART error
  _parse_failure_error_code = UARTDEF_BMS_ERROR_INTERNAL_UART_ERROR;
  //command processed without error: queue write acknowledgement
  return _UARTH_Notification_Event_WriteAck(reg_addr);
}

static void _UARTH_ParseCommand() {
  uint32_t length = _parse_buffer_write_offset;

  //assume format error on failure, unless specified otherwise by sub-parser
  _parse_failure_error_code = UARTDEF_BMS_ERROR_UART_FORMAT_ERROR;

  HAL_StatusTypeDef result;
  if (_UART_CRC_Accumulate(_parse_buffer, length, 0) == 0) {
    //CRC check passed: continue parsing
    switch (_parse_buffer[0]) {
      case UARTDEF_BMS_TYPE_READ:
        if (length == 4) {
          result = _UARTH_ParseRead(_parse_buffer[1]);
        } else {
          result = HAL_ERROR;
        }
        break;
      case UARTDEF_BMS_TYPE_WRITE:
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
    _parse_failure_error_code = UARTDEF_BMS_ERROR_UART_CRC_ERROR;
    result = HAL_ERROR;
  }

  if (result != HAL_OK) {
    UARTH_Notification_Event_Error(_parse_failure_error_code, false);
  }
}


/******************************************************************************************
 * INTERRUPT CALLBACKS
 ******************************************************************************************/

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef* huart, uint16_t Size) {
  if (huart != &huart1) {
    return;
  }

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

void HAL_UART_ErrorCallback(UART_HandleTypeDef* huart) {
  if (huart != &huart1) {
    return;
  }

  if (uarth_ignore_hardware_errors) {
    return;
  } else if (uarth_ignore_one_hardware_error) {
    uarth_ignore_one_hardware_error = false;
    return;
  }

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
      if (rx_byte == UARTDEF_BMS_START_BYTE || rx_byte == UARTDEF_BMS_END_BYTE || rx_byte == UARTDEF_BMS_ESCAPE_BYTE) {
        //reserved byte escaped: copy it to the parse buffer, unless we're skipping to the next start byte (because it should be ignored then)
        if (!_rx_skip_to_start) {
          _parse_buffer[_parse_buffer_write_offset++] = rx_byte;
        }
      } else {
        //error: non-reserved byte escaped: reset parse buffer, skip to next start
        _parse_buffer_write_offset = 0;
        _rx_skip_to_start = true;
        UARTH_Notification_Event_Error(UARTDEF_BMS_ERROR_UART_FORMAT_ERROR, false);
      }
      _rx_escape_active = false;
    } else {
      //not escaping: normal reception logic
      if (rx_byte == UARTDEF_BMS_ESCAPE_BYTE) {
        //escape byte received: discard it and escape the next byte
        _rx_escape_active = true;
      } else if (_rx_skip_to_start) {
        //skipping to start: ignore all non-start bytes
        if (rx_byte == UARTDEF_BMS_START_BYTE) {
          //start byte found: ready for actual reception starting with next byte
          _parse_buffer_write_offset = 0;
          _rx_skip_to_start = false;
        }
      } else {
        //normal reception
        if (rx_byte == UARTDEF_BMS_START_BYTE) {
          //error: unexpected start byte: reset parse buffer, receive new command starting with this byte
          _parse_buffer_write_offset = 0;
          UARTH_Notification_Event_Error(UARTDEF_BMS_ERROR_UART_FORMAT_ERROR, false);
        } else if (rx_byte == UARTDEF_BMS_END_BYTE) {
          //end byte found: parse buffer is done
          if (_parse_buffer_write_offset < 3) {
            //error: parse buffer empty or too short (must have 1B type + 2B crc at least): skip to next start
            _rx_skip_to_start = true;
            UARTH_Notification_Event_Error(UARTDEF_BMS_ERROR_UART_FORMAT_ERROR, false);
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
      UARTH_Notification_Event_Error(UARTDEF_BMS_ERROR_UART_FORMAT_ERROR, false);
    }
  }
}

static void _UARTH_CheckChangeNotification() {
  if ((_change_notification_mask & UARTDEF_BMS_NOTIF_MASK_STATUS_Msk) != 0) {
    //check for main status change
    if (
        (bms_status.safety_faults._all == 0) != (_last_check_status.safety_faults._all == 0) ||
        (bms_status.safety_alerts._all == 0) != (_last_check_status.safety_alerts._all == 0) ||
        bms_status.chg_force_off != _last_check_status.chg_force_off ||
        (bms_status.timed_shutdown_type == BMSSHDN_NONE) != (_last_check_status.timed_shutdown_type == BMSSHDN_NONE)
    ) {
      _UARTH_Notification_Status(UARTDEF_BMS_TYPE_CHANGE_NOTIFICATION, UARTDEF_BMS_STATUS);
    }
  }

  if ((_change_notification_mask & UARTDEF_BMS_NOTIF_MASK_STACK_VOLTAGE_Msk) != 0) {
    //check for stack voltage change
    if (bms_measurements.voltage_stack != _last_check_measurements.voltage_stack) {
      _UARTH_Notification_Status(UARTDEF_BMS_TYPE_CHANGE_NOTIFICATION, UARTDEF_BMS_STACK_VOLTAGE);
    }
  }

  if ((_change_notification_mask & UARTDEF_BMS_NOTIF_MASK_CELL_VOLTAGES_Msk) != 0) {
    //check for cell voltage change
    bool change = false;
    int i;
    for (i = 0; i < BMS_CELLS_SERIES; i++) {
      if (bms_measurements.voltage_cells[i] != _last_check_measurements.voltage_cells[i]) {
        change = true;
        break;
      }
    }
    if (change) {
      _UARTH_Notification_Status(UARTDEF_BMS_TYPE_CHANGE_NOTIFICATION, UARTDEF_BMS_CELL_VOLTAGES);
    }
  }

  if ((_change_notification_mask & UARTDEF_BMS_NOTIF_MASK_CURRENT_Msk) != 0) {
    //check for current change
    if (bms_measurements.current != _last_check_measurements.current) {
      _UARTH_Notification_Status(UARTDEF_BMS_TYPE_CHANGE_NOTIFICATION, UARTDEF_BMS_CURRENT);
    }
  }

  if ((_change_notification_mask & UARTDEF_BMS_NOTIF_MASK_SOC_FRACTION_Msk) != 0) {
    //check for SoC fraction change
    if (
        bms_measurements.soc_precisionlevel != _last_check_measurements.soc_precisionlevel ||
        isnanf(bms_measurements.soc_fraction) != isnanf(_last_check_measurements.soc_fraction) ||
        (!isnanf(bms_measurements.soc_fraction) && (bms_measurements.soc_fraction != _last_check_measurements.soc_fraction))
    ) {
      _UARTH_Notification_Status(UARTDEF_BMS_TYPE_CHANGE_NOTIFICATION, UARTDEF_BMS_SOC_FRACTION);
    }
  }

  if ((_change_notification_mask & UARTDEF_BMS_NOTIF_MASK_SOC_ENERGY_Msk) != 0) {
    //check for SoC energy change
    if (
        bms_measurements.soc_precisionlevel != _last_check_measurements.soc_precisionlevel ||
        isnanf(bms_measurements.soc_energy) != isnanf(_last_check_measurements.soc_energy) ||
        (!isnanf(bms_measurements.soc_energy) && (bms_measurements.soc_energy != _last_check_measurements.soc_energy))
    ) {
      _UARTH_Notification_Status(UARTDEF_BMS_TYPE_CHANGE_NOTIFICATION, UARTDEF_BMS_SOC_ENERGY);
    }

    if ((_change_notification_mask & UARTDEF_BMS_NOTIF_MASK_HEALTH_Msk) != 0) {
      //check for health change
      if (*bms_soc_battery_health_ptr != _last_check_battery_health) {
        _UARTH_Notification_Status(UARTDEF_BMS_TYPE_CHANGE_NOTIFICATION, UARTDEF_BMS_HEALTH);
      }
    }
  }

  if ((_change_notification_mask & UARTDEF_BMS_NOTIF_MASK_BAT_TEMP_Msk) != 0) {
    //check for battery temp change
    if (bms_measurements.bat_temp != _last_check_measurements.bat_temp) {
      _UARTH_Notification_Status(UARTDEF_BMS_TYPE_CHANGE_NOTIFICATION, UARTDEF_BMS_BAT_TEMP);
    }
  }

  if ((_change_notification_mask & UARTDEF_BMS_NOTIF_MASK_INT_TEMP_Msk) != 0) {
    //check for internal temp change
    if (bms_measurements.internal_temp != _last_check_measurements.internal_temp) {
      _UARTH_Notification_Status(UARTDEF_BMS_TYPE_CHANGE_NOTIFICATION, UARTDEF_BMS_INT_TEMP);
    }
  }

  if ((_change_notification_mask & UARTDEF_BMS_NOTIF_MASK_ALERTS_Msk) != 0) {
    //check for safety alert change
    if (bms_status.safety_alerts._all != _last_check_status.safety_alerts._all) {
      _UARTH_Notification_Status(UARTDEF_BMS_TYPE_CHANGE_NOTIFICATION, UARTDEF_BMS_ALERTS);
    }
  }

  if ((_change_notification_mask & UARTDEF_BMS_NOTIF_MASK_FAULTS_Msk) != 0) {
    //check for safety fault change
    if (bms_status.safety_faults._all != _last_check_status.safety_faults._all) {
      _UARTH_Notification_Status(UARTDEF_BMS_TYPE_CHANGE_NOTIFICATION, UARTDEF_BMS_FAULTS);
    }
  }

  if ((_change_notification_mask & UARTDEF_BMS_NOTIF_MASK_SHUTDOWN_Msk) != 0) {
    //check for timed shutdown change
    if (
        bms_status.timed_shutdown_type != _last_check_status.timed_shutdown_type ||
        bms_status.timed_shutdown_time != _last_check_status.timed_shutdown_time
    ) {
      _UARTH_Notification_Status(UARTDEF_BMS_TYPE_CHANGE_NOTIFICATION, UARTDEF_BMS_SHUTDOWN);
    }
  }

  //save current state as last checked state for next time
  memcpy(&_last_check_status, &bms_status, sizeof(BMS_Status));
  memcpy(&_last_check_measurements, &bms_measurements, sizeof(BMS_Measurements));
  _last_check_battery_health = *bms_soc_battery_health_ptr;
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
  _parse_failure_error_code = UARTDEF_BMS_ERROR_UART_FORMAT_ERROR;
  _bms_error_since_last_status = false;
  _flash_error_since_last_status = false;
  _uart_error_since_last_status = false;
  _interrupt_error = false;
  _force_change_notification_check = false;

  uarth_ignore_hardware_errors = false;
  uarth_ignore_one_hardware_error = false;

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


