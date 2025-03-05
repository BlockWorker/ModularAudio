/*
 * iot_driver.c
 *
 *  Created on: Mar 2, 2025
 *      Author: Alex
 */

#include "iot_driver.h"
#include <stdlib.h>


#define IOT_PARSEBUF_SIZE 2048
#define IOT_CMDBUF_SIZE 256


typedef struct _cmd_queue_item {
  IOT_Command command;
  char cmd_text[IOT_CMDBUF_SIZE];
  uint32_t cmd_length;
  struct _cmd_queue_item* next;
} IOT_CommandQueueItem;


//arrays of definition strings for configuration, commands, and notification parsing
static const char* _config_array[] = IOT_CONFIG_ARRAY;

//UART receive buffer (byte and char/text views)
static uint8_t _rx_buffer[IOT_RXBUF_SIZE] = { 0 };
static char* _rx_text = (char*)_rx_buffer;
//buffer offset of start of current receive call
static uint32_t _rx_buffer_write_offset = 0;
//buffer offset of the next character to be read for parsing
static uint32_t _rx_buffer_read_offset = 0;

//notification parse buffer holding current notification (or part if not complete yet)
static char _parse_buffer[IOT_PARSEBUF_SIZE] = { 0 };
//parse buffer offset of next character to be written
static uint32_t _parse_buffer_write_offset = 0;
//smaller text buffers for notification parameters
static char _parse_str_0[256] = { 0 };
static char _parse_str_1[256] = { 0 };
static char _parse_str_2[256] = { 0 };
static char _parse_str_3[256] = { 0 };

//command preparation buffer holding the current command
static char _cmd_prep_buffer[IOT_CMDBUF_SIZE] = { 0 };
//head and tail pointers of command queue
static IOT_CommandQueueItem* _cmd_queue_head = NULL;
static IOT_CommandQueueItem* _cmd_queue_tail = NULL;

//command that is currently being executed (and has not been confirmed as completed yet)
static IOT_Command _current_command = CMD_NONE;
//transmit buffer for the current command
static char _cmd_tx_buffer[IOT_CMDBUF_SIZE] = { 0 };
//tick after which the next command may be sent
static uint32_t _next_cmd_tick = 0;
//tick after which the current command times out
static uint32_t _cmd_timeout_tick = HAL_MAX_DELAY;

//whether any config changes had to be made during init, requiring a write and reset
bool _init_config_changed = false;

//whether the driver has been initialised successfully
bool iot_driver_init_complete = false;


/******************************************************************************************
 * COMMAND FUNCTIONS
 ******************************************************************************************/

static HAL_StatusTypeDef _IOT_QueueCommand(IOT_Command command, int32_t length) {
  if (length <= 0 || length >= IOT_CMDBUF_SIZE) {
    //command buffer overflow or other error
    return HAL_ERROR;
  }

  //allocate queue item
  IOT_CommandQueueItem* new_item = malloc(sizeof(IOT_CommandQueueItem));
  if (new_item == NULL) {
    //insufficient heap memory
    return HAL_ERROR;
  }

  //populate queue item values
  new_item->command = command;
  memcpy(new_item->cmd_text, _cmd_prep_buffer, length + 1);
  new_item->cmd_length = length;
  new_item->next = NULL;

  if (_cmd_queue_head == NULL || _cmd_queue_tail == NULL) {
    //queue currently empty: make new item the first and only item
    _cmd_queue_head = new_item;
  } else {
    //queue not empty: append new item to the tail
    _cmd_queue_tail->next = new_item;
  }
  //new item becomes the tail either way
  _cmd_queue_tail = new_item;

  return HAL_OK;
}

static IOT_CommandQueueItem* _IOT_DequeueCommand() {
  if (_cmd_queue_head == NULL || _cmd_queue_tail == NULL) {
    //empty queue: nothing to dequeue
    return NULL;
  }

  //take item from the front (head) of the queue, next item becomes the new head
  IOT_CommandQueueItem* item = _cmd_queue_head;
  _cmd_queue_head = item->next;
  if (_cmd_queue_head == NULL) {
    //removed item was the last item: clear tail pointer too
    _cmd_queue_tail = NULL;
  }

  return item;
}

//put the given command back at the top of the queue, e.g. in case of an error
static void _IOT_RequeueCommand(IOT_CommandQueueItem* item) {
  if (item == NULL) {
    return;
  }

  if (_cmd_queue_head == NULL || _cmd_queue_tail == NULL) {
    //queue currently empty: make item the only item, i.e. also the tail
    _cmd_queue_tail = item;
  } else {
    //queue not empty: append current head to the item
    item->next = _cmd_queue_head;
  }
  //either way: item becomes the new head
  _cmd_queue_head = item;
}

static void _IOT_ClearCommandQueue() {
  //iterate through queue from head, freeing all items
  IOT_CommandQueueItem* item = _cmd_queue_head;
  while (item != NULL) {
    item = item->next;
    free(item);
  }

  //clear pointers to mark queue as empty
  _cmd_queue_head = NULL;
  _cmd_queue_tail = NULL;
}

HAL_StatusTypeDef IOT_Command_AVRCP_META_DATA(uint8_t link_id) {
  int32_t res = snprintf(_cmd_prep_buffer, IOT_CMDBUF_SIZE, IOT_CMD_AVRCP_META_DATA, link_id);
  return _IOT_QueueCommand(CMD_AVRCP_META_DATA, res);
}

HAL_StatusTypeDef IOT_Command_BROADCAST(bool start, uint8_t source, const char* pin) {
  int32_t res = snprintf(_cmd_prep_buffer, IOT_CMDBUF_SIZE, IOT_CMD_BROADCAST, start ? "ON" : "OFF", source, pin);
  return _IOT_QueueCommand(CMD_BROADCAST, res);
}

HAL_StatusTypeDef IOT_Command_BROADCODE(uint8_t link_id, const char* pin) {
  int32_t res = snprintf(_cmd_prep_buffer, IOT_CMDBUF_SIZE, IOT_CMD_BROADCODE, link_id, pin);
  return _IOT_QueueCommand(CMD_BROADCODE, res);
}

HAL_StatusTypeDef IOT_Command_CLOSE(uint8_t link_id) {
  int32_t res = snprintf(_cmd_prep_buffer, IOT_CMDBUF_SIZE, IOT_CMD_CLOSE, link_id);
  return _IOT_QueueCommand(CMD_CLOSE, res);
}

HAL_StatusTypeDef IOT_Command_CLOSE_ALL() {
  int32_t res = snprintf(_cmd_prep_buffer, IOT_CMDBUF_SIZE, IOT_CMD_CLOSE_ALL);
  return _IOT_QueueCommand(CMD_CLOSE_ALL, res);
}

static HAL_StatusTypeDef IOT_Command_CONFIG() {
  int32_t res = snprintf(_cmd_prep_buffer, IOT_CMDBUF_SIZE, IOT_CMD_CONFIG);
  return _IOT_QueueCommand(CMD_CONFIG, res);
}

HAL_StatusTypeDef IOT_Command_CONNECTABLE(bool connectable) {
  int32_t res = snprintf(_cmd_prep_buffer, IOT_CMDBUF_SIZE, IOT_CMD_CONNECTABLE, connectable ? "ON" : "OFF");
  return _IOT_QueueCommand(CMD_CONNECTABLE, res);
}

HAL_StatusTypeDef IOT_Command_DISCOVERABLE(bool discoverable) {
  int32_t res = snprintf(_cmd_prep_buffer, IOT_CMDBUF_SIZE, IOT_CMD_DISCOVERABLE, discoverable ? "ON" : "OFF");
  return _IOT_QueueCommand(CMD_DISCOVERABLE, res);
}

HAL_StatusTypeDef IOT_Command_MUSIC(uint8_t link_id, const char* command) {
  int32_t res = snprintf(_cmd_prep_buffer, IOT_CMDBUF_SIZE, IOT_CMD_MUSIC, link_id, command);
  return _IOT_QueueCommand(CMD_MUSIC, res);
}

HAL_StatusTypeDef IOT_Command_NAME(uint64_t bt_addr) {
  int32_t res = snprintf(_cmd_prep_buffer, IOT_CMDBUF_SIZE, IOT_CMD_NAME, bt_addr);
  return _IOT_QueueCommand(CMD_NAME, res);
}

HAL_StatusTypeDef IOT_Command_OPEN(uint64_t bt_addr, const char* profile) {
  int32_t res = snprintf(_cmd_prep_buffer, IOT_CMDBUF_SIZE, IOT_CMD_OPEN, bt_addr, profile);
  return _IOT_QueueCommand(CMD_OPEN, res);
}

HAL_StatusTypeDef IOT_Command_OPEN_BCAST(uint64_t bt_addr, const char* profile, uint8_t adv_sid, uint32_t bcast_id, bool pa_sync) {
  int32_t res = snprintf(_cmd_prep_buffer, IOT_CMDBUF_SIZE, IOT_CMD_OPEN_LONG, bt_addr, profile, 2, adv_sid, bcast_id, pa_sync ? 1 : 0);
  return _IOT_QueueCommand(CMD_OPEN_LONG, res);
}

//static HAL_StatusTypeDef IOT_Command_RESET() {
//  int32_t res = snprintf(_cmd_prep_buffer, IOT_CMDBUF_SIZE, IOT_CMD_RESET);
//  return _IOT_QueueCommand(CMD_RESET, res);
//}

HAL_StatusTypeDef IOT_Command_RSSI(uint64_t bt_addr) {
  int32_t res = snprintf(_cmd_prep_buffer, IOT_CMDBUF_SIZE, IOT_CMD_RSSI, bt_addr);
  return _IOT_QueueCommand(CMD_RSSI, res);
}

HAL_StatusTypeDef IOT_Command_QUALITY(uint64_t bt_addr) {
  int32_t res = snprintf(_cmd_prep_buffer, IOT_CMDBUF_SIZE, IOT_CMD_QUALITY, bt_addr);
  return _IOT_QueueCommand(CMD_QUALITY, res);
}

HAL_StatusTypeDef IOT_Command_SCAN_BCAST(uint8_t timeout) {
  int32_t res = snprintf(_cmd_prep_buffer, IOT_CMDBUF_SIZE, IOT_CMD_SCAN_BCAST, timeout);
  return _IOT_QueueCommand(CMD_SCAN_BCAST, res);
}

HAL_StatusTypeDef IOT_Command_SEND(uint8_t link_id, const char* payload) {
  int32_t res = snprintf(_cmd_prep_buffer, IOT_CMDBUF_SIZE, IOT_CMD_SEND, link_id, payload);
  return _IOT_QueueCommand(CMD_SEND, res);
}

static HAL_StatusTypeDef IOT_Command_SET(const char* key, const char* value) {
  int32_t res = snprintf(_cmd_prep_buffer, IOT_CMDBUF_SIZE, IOT_CMD_SET, key, value);
  return _IOT_QueueCommand(CMD_SET, res);
}

HAL_StatusTypeDef IOT_Command_STATUS() {
  int32_t res = snprintf(_cmd_prep_buffer, IOT_CMDBUF_SIZE, IOT_CMD_STATUS);
  return _IOT_QueueCommand(CMD_STATUS, res);
}

HAL_StatusTypeDef IOT_Command_TONE(const char* tone_string) {
  int32_t res = snprintf(_cmd_prep_buffer, IOT_CMDBUF_SIZE, IOT_CMD_TONE, tone_string);
  return _IOT_QueueCommand(CMD_TONE, res);
}

HAL_StatusTypeDef IOT_Command_VOLUME(uint8_t link_id, uint8_t volume) {
  int32_t res = snprintf(_cmd_prep_buffer, IOT_CMDBUF_SIZE, IOT_CMD_VOLUME, link_id, volume);
  return _IOT_QueueCommand(CMD_VOLUME, res);
}

HAL_StatusTypeDef IOT_Command_VOLUME_GET() {
  int32_t res = snprintf(_cmd_prep_buffer, IOT_CMDBUF_SIZE, IOT_CMD_VOLUME_GET);
  return _IOT_QueueCommand(CMD_VOLUME_GET, res);
}

static HAL_StatusTypeDef IOT_Command_WRITE() {
  int32_t res = snprintf(_cmd_prep_buffer, IOT_CMDBUF_SIZE, IOT_CMD_WRITE);
  return _IOT_QueueCommand(CMD_WRITE, res);
}


static void _IOT_Command_Finish(IOT_Error error) {
  _current_command = CMD_NONE;
  _next_cmd_tick = HAL_GetTick() + IOT_CMD_DELAY;

  if (error == IOTERR_NONE) {
    DEBUG_PRINTF("Command finished successfully\n");
    return;
  }

  DEBUG_PRINTF("* IOT reported error 0x%04X\n", (uint16_t)error);

  if (_current_command == CMD_INIT || _current_command == CMD_CONFIG || _current_command == CMD_SET || _current_command == CMD_WRITE) {
    //error in init procedure: reset
    IOT_Init(); //TODO: consider full MCU reset?
  }

  //TODO: handle other error cases
}

static void _IOT_Command_Timeout() {
  DEBUG_PRINTF("* Command timed out!\n");
  if (_current_command == CMD_INIT || _current_command == CMD_CONFIG || _current_command == CMD_SET || _current_command == CMD_WRITE) {
    //timeout in init procedure: reset
    IOT_Init(); //TODO: consider full MCU reset?
  } else {
    _IOT_Command_Finish(IOTERR_COMMAND_TIMEOUT);
  }
}


/******************************************************************************************
 * NOTIFICATION PARSING FUNCTIONS
 ******************************************************************************************/

//check correctness of given config key-value pair - if incorrect, queue a correcting SET command, and mark the config as changed
static void _IOT_CheckConfigItem(const char* key, const char* value) {
  //iterate through all config array entries to check whether the given key is in there
  int i;
  for (i = 0; i < IOT_CONFIG_ARRAY_ENTRIES; i++) {
    const char* arr_key = _config_array[2 * i];
    const char* arr_value = _config_array[2 * i + 1];
    //check given key against config entry key
    if (strcmp(key, arr_key) == 0) {
      //key matches config entry key: check value
      if (strcmp(value, arr_value) != 0) {
        //value doesn't match: queue command to correct it and mark the config as changed
        _init_config_changed = true;
        if (IOT_Command_SET(arr_key, arr_value) != HAL_OK) {
          //failed to queue set command: reset
          IOT_Init(); //TODO: consider full MCU reset?
        }
      }
      return;
    }
  }
}


static bool _IOT_Parse_OK() {
  if (strcmp(_parse_buffer, IOT_NOTIF_OK) != 0) {
    return false;
  }
  if (_current_command == CMD_CONFIG) {
    if (_init_config_changed) {
      //config will be changed (set commands queued), so queue a write command at the end
      if (IOT_Command_WRITE() != HAL_OK) {
        //failed to queue write command: reset
        IOT_Init(); //TODO: consider full MCU reset?
        return true;
      }
    } else {
      //config is correct, so init is done
      iot_driver_init_complete = true;
      DEBUG_PRINTF("IOT driver init completed successfully\n");
      //TODO some kind of init complete notification? if necessary
    }
  } else if (_current_command == CMD_WRITE) {
    //config write succeeded, so reset to (hopefully) correct config
    IOT_Init();
    return true;
  }
  _IOT_Command_Finish(IOTERR_NONE);
  //DEBUG_PRINTF("OK response parsed\n");
  return true;
}

static bool _IOT_Parse_CONFIG_Item() {
  if (sscanf(_parse_buffer, IOT_NOTIF_CONFIG_ITEM, _parse_str_0, _parse_str_1) < 2) {
    return false;
  }
  _IOT_CheckConfigItem(_parse_str_0, _parse_str_1);
  //DEBUG_PRINTF("Config item parsed: %s = %s\n", _parse_str_0, _parse_str_1);
  return true;
}

static bool _IOT_Parse_NAME() {
  uint64_t bt_addr;
  if (sscanf(_parse_buffer, IOT_NOTIF_NAME, &bt_addr, _parse_str_0) < 2) {
    return false;
  }
  //TODO: process data
  _IOT_Command_Finish(IOTERR_NONE);
  DEBUG_PRINTF("NAME response parsed\n");
  return true;
}

static bool _IOT_Parse_RSSI() {
  int16_t rssi;
  if (sscanf(_parse_buffer, IOT_NOTIF_RSSI, &rssi) < 1) {
    return false;
  }
  //TODO: process data
  DEBUG_PRINTF("RSSI response parsed\n");
  return true;
}

static bool _IOT_Parse_QUALITY() {
  uint16_t quality;
  if (sscanf(_parse_buffer, IOT_NOTIF_QUALITY, &quality) < 1) {
    return false;
  }
  //TODO: process data
  DEBUG_PRINTF("QUALITY response parsed\n");
  return true;
}

static bool _IOT_Parse_SCAN() {
  uint64_t bt_addr;
  uint32_t bcast_id;
  uint8_t adv_id;
  int16_t rssi;
  if (sscanf(_parse_buffer, IOT_NOTIF_SCAN_BCAST, &bt_addr, &bcast_id, &adv_id, _parse_str_0, &rssi) == 5) {
    //TODO: process broadcast scan result
    DEBUG_PRINTF("SCAN broadcast response parsed\n");
    return true;
  } else if (strcmp(_parse_buffer, IOT_NOTIF_SCAN_OK) == 0) {
    _IOT_Command_Finish(IOTERR_NONE);
    DEBUG_PRINTF("SCAN_OK response parsed\n");
    return true;
  }
  return false;
}

static bool _IOT_Parse_STATE() {
  if (sscanf(_parse_buffer, IOT_NOTIF_STATE, _parse_str_0, _parse_str_1, _parse_str_2, _parse_str_3) < 4) {
    return false;
  }
  //TODO: process data
  DEBUG_PRINTF("STATE response parsed\n");
  return true;
}

static bool _IOT_Parse_LINK() {
  uint8_t link_id;
  uint64_t bt_addr;
  uint32_t sample_rate;
  uint32_t bcast_code;
  uint32_t bcast_pdel;
  uint8_t bcast_encr;
  uint8_t bcast_subgroup;
  uint8_t bcast_biss;

  if (sscanf(_parse_buffer, IOT_NOTIF_LINK_A2DP, &link_id, &bt_addr, _parse_str_0, _parse_str_1, _parse_str_2, &sample_rate) == 6) {
    //TODO: process data
    DEBUG_PRINTF("LINK A2DP response parsed\n");
    return true;
  } else if (sscanf(_parse_buffer, IOT_NOTIF_LINK_AVRCP, &link_id, &bt_addr, _parse_str_0) == 3) {
    //TODO: process data
    DEBUG_PRINTF("LINK AVRCP response parsed\n");
    return true;
  } else if (sscanf(_parse_buffer, IOT_NOTIF_LINK_BRX1, &link_id, &bt_addr, &bcast_code, _parse_str_0, _parse_str_1, &bcast_pdel, &sample_rate, &bcast_encr, &bcast_subgroup) == 9) {
    //TODO: process data
    DEBUG_PRINTF("LINK BRX1 response parsed\n");
    return true;
  } else if (sscanf(_parse_buffer, IOT_NOTIF_LINK_BTX1, &link_id, &bt_addr, _parse_str_0, &bcast_pdel, &sample_rate, &bcast_subgroup, &bcast_biss) == 7) {
    //TODO: process data
    DEBUG_PRINTF("LINK BTX1 response parsed\n");
    return true;
  }

  return false;
}

static bool _IOT_Parse_VOLUME_READ() {
  uint8_t link_id;
  uint8_t volume;
  if (sscanf(_parse_buffer, IOT_NOTIF_VOLUME_READ, &link_id, _parse_str_0, &volume) < 3) {
    return false;
  }
  //TODO: process volume readback
  DEBUG_PRINTF("VOLUME readback response parsed\n");
  return true;
}

static bool _IOT_Parse_Ready() {
  if (strcmp(_parse_buffer, IOT_NOTIF_READY) != 0) {
    return false;
  }
  if (_current_command == CMD_INIT) {
    //module started up - read config next
    if (IOT_Command_CONFIG() == HAL_OK) {
      //config command queued: finish init "command"
      _IOT_Command_Finish(IOTERR_NONE);
    } else {
      //failed to queue config command: reset
      IOT_Init(); //TODO: consider full MCU reset?
    }
  }
  //DEBUG_PRINTF("Ready notification parsed\n");
  return true;
}

static bool _IOT_Parse_ERROR() {
  uint16_t error_code;
  if (sscanf(_parse_buffer, IOT_NOTIF_ERROR, &error_code) < 1) {
    return false;
  }
  _IOT_Command_Finish((IOT_Error)error_code);
  DEBUG_PRINTF("ERROR notification parsed: 0x%04X\n", error_code);
  return true;
}

static bool _IOT_Parse_A2DP_Event() {
  uint8_t link_id;
  if (sscanf(_parse_buffer, IOT_NOTIF_A2DP_STREAM_START, &link_id) == 1) {
    //TODO: process event
    DEBUG_PRINTF("A2DP_STREAM_START notification parsed\n");
    return true;
  } else if (sscanf(_parse_buffer, IOT_NOTIF_A2DP_STREAM_SUSPEND, &link_id) == 1) {
    //TODO: process event
    DEBUG_PRINTF("A2DP_STREAM_SUSPEND notification parsed\n");
    return true;
  }
  return false;
}

static bool _IOT_Parse_ABS_VOL() {
  uint8_t link_id;
  uint8_t volume;
  if (sscanf(_parse_buffer, IOT_NOTIF_ABS_VOL, &link_id, &volume) < 2) {
    return false;
  }
  //TODO: process data
  DEBUG_PRINTF("ABS_VOL notification parsed\n");
  return true;
}

static bool _IOT_Parse_AVRCP_Command() {
  uint8_t link_id;
  if (sscanf(_parse_buffer, IOT_NOTIF_AVRCP_BACKWARD, &link_id) == 1) {
    //TODO: process command
    DEBUG_PRINTF("AVRCP_BACKWARD notification parsed\n");
    return true;
  } else if (sscanf(_parse_buffer, IOT_NOTIF_AVRCP_FORWARD, &link_id) == 1) {
    //TODO: process command
    DEBUG_PRINTF("AVRCP_FORWARD notification parsed\n");
    return true;
  } else if (sscanf(_parse_buffer, IOT_NOTIF_AVRCP_PAUSE, &link_id) == 1) {
    //TODO: process command
    DEBUG_PRINTF("AVRCP_PAUSE notification parsed\n");
    return true;
  } else if (sscanf(_parse_buffer, IOT_NOTIF_AVRCP_PLAY, &link_id) == 1) {
    //TODO: process command
    DEBUG_PRINTF("AVRCP_PLAY notification parsed\n");
    return true;
  } else if (sscanf(_parse_buffer, IOT_NOTIF_AVRCP_STOP, &link_id) == 1) {
    //TODO: process command
    DEBUG_PRINTF("AVRCP_STOP notification parsed\n");
    return true;
  }
  return false;
}

static bool _IOT_Parse_AVRCP_MediaInfo() {
  //uint8_t link_id;
  if (sscanf(_parse_buffer, IOT_NOTIF_AVRCP_MEDIA_TITLE, _parse_str_0) == 1) {
    //TODO: process data
    DEBUG_PRINTF("AVRCP_MEDIA title notification parsed\n");
    return true;
  } else if (sscanf(_parse_buffer, IOT_NOTIF_AVRCP_MEDIA_ARTIST, _parse_str_0) == 1) {
    //TODO: process data
    DEBUG_PRINTF("AVRCP_MEDIA artist notification parsed\n");
    return true;
  } else if (sscanf(_parse_buffer, IOT_NOTIF_AVRCP_MEDIA_ALBUM, _parse_str_0) == 1) {
    //TODO: process data
    DEBUG_PRINTF("AVRCP_MEDIA album notification parsed\n");
    return true;
  }
  return false;
}

static bool _IOT_Parse_CLOSE_LOSS() {
  uint8_t link_id;
  if (sscanf(_parse_buffer, IOT_NOTIF_CLOSE_OK, &link_id, _parse_str_0) == 2) {
    //TODO: process close
    DEBUG_PRINTF("CLOSE_OK notification parsed\n");
    return true;
  } else if (sscanf(_parse_buffer, IOT_NOTIF_LINK_LOSS, &link_id, _parse_str_0) == 2) {
    //TODO: process loss
    DEBUG_PRINTF("LINK_LOSS notification parsed\n");
    return true;
  }
  return false;
}

static bool _IOT_Parse_OPEN_OK() {
  uint8_t link_id;
  uint64_t bt_addr;
  if (sscanf(_parse_buffer, IOT_NOTIF_OPEN_OK, &link_id, _parse_str_0, &bt_addr) < 3) {
    return false;
  }
  //TODO: process open
  if (_current_command == CMD_OPEN || _current_command == CMD_OPEN_LONG) {
    _IOT_Command_Finish(IOTERR_NONE);
  }
  DEBUG_PRINTF("OPEN_OK notification parsed\n");
  return true;
}

static bool _IOT_Parse_OPEN_ERROR() {
  uint64_t bt_addr;
  uint8_t reason;
  int scan_res = sscanf(_parse_buffer, IOT_NOTIF_OPEN_ERROR, &bt_addr, _parse_str_0, &reason);
  if (scan_res == 3) {
    //TODO: process error with reason
    if (_current_command == CMD_OPEN || _current_command == CMD_OPEN_LONG) {
      _IOT_Command_Finish((IOT_Error)(0xFFF0U | reason));
    }
    DEBUG_PRINTF("OPEN_ERROR with reason notification parsed\n");
    return true;
  } else if (scan_res == 2) {
    //TODO: process error without reason
    if (_current_command == CMD_OPEN || _current_command == CMD_OPEN_LONG) {
      _IOT_Command_Finish(IOTERR_UNKNOWN);
    }
    DEBUG_PRINTF("OPEN_ERROR without reason notification parsed\n");
    return true;
  }
  return false;
}

static bool _IOT_Parse_PAIR_ERROR() {
  uint64_t bt_addr;
  if (sscanf(_parse_buffer, IOT_NOTIF_PAIR_ERROR, &bt_addr) < 1) {
    return false;
  }
  //TODO: process error
  DEBUG_PRINTF("PAIR_ERROR notification parsed\n");
  return true;
}

static bool _IOT_Parse_PAIR_OK() {
  uint64_t bt_addr;
  if (sscanf(_parse_buffer, IOT_NOTIF_PAIR_OK, &bt_addr) < 1) {
    return false;
  }
  //TODO: process pair
  DEBUG_PRINTF("PAIR_OK notification parsed\n");
  return true;
}

static bool _IOT_Parse_PAIR_PENDING() {
  if (strcmp(_parse_buffer, IOT_NOTIF_PAIR_PENDING) != 0) {
    return false;
  }
  //TODO: process pending pair
  //DEBUG_PRINTF("PAIR_PENDING notification parsed\n");
  return true;
}

static bool _IOT_Parse_RECV() {
  uint8_t link_id;
  uint16_t size;
  if (sscanf(_parse_buffer, IOT_NOTIF_RECV, &link_id, &size, _parse_str_0) < 3) {
    return false;
  }
  //TODO: process received data
  DEBUG_PRINTF("RECV notification parsed\n");
  return true;
}

static const bool (*_unprompted_parsers[])() = {
  &_IOT_Parse_Ready,
  &_IOT_Parse_ERROR,
  &_IOT_Parse_A2DP_Event,
  &_IOT_Parse_ABS_VOL,
  &_IOT_Parse_AVRCP_Command,
  &_IOT_Parse_AVRCP_MediaInfo,
  &_IOT_Parse_CLOSE_LOSS,
  &_IOT_Parse_OPEN_OK,
  &_IOT_Parse_OPEN_ERROR,
  &_IOT_Parse_PAIR_ERROR,
  &_IOT_Parse_PAIR_OK,
  &_IOT_Parse_PAIR_PENDING,
  &_IOT_Parse_RECV
};

static const int _unprompted_parser_count = 13;

//parse the notification in the parse buffer (must be completely received at this point)
static void _IOT_ParseNotification() {
  //try to parse command responses if there's a corresponding active command
  if (_current_command != CMD_NONE) {
    if (_IOT_Parse_OK()) return;
  }

  if (_current_command == CMD_CONFIG) {
    if (_IOT_Parse_CONFIG_Item()) return;
  } else if (_current_command == CMD_NAME) {
    if (_IOT_Parse_NAME()) return;
  } else if (_current_command == CMD_RSSI) {
    if (_IOT_Parse_RSSI()) return;
  } else if (_current_command == CMD_QUALITY) {
    if (_IOT_Parse_QUALITY()) return;
  } else if (_current_command == CMD_SCAN_BCAST) {
    if (_IOT_Parse_SCAN()) return;
  } else if (_current_command == CMD_STATUS) {
    if (_IOT_Parse_STATE()) return;
    if (_IOT_Parse_LINK()) return;
  } else if (_current_command == CMD_VOLUME_GET) {
    if (_IOT_Parse_VOLUME_READ()) return;
  }

  //try to parse notifications that may come up unprompted
  int i;
  for (i = 0; i < _unprompted_parser_count; i++) {
    if (_unprompted_parsers[i]()) return;
  }

  DEBUG_PRINTF("Notification parse failed\n");
  //DEBUG_PRINTF("Notification parse failed: %s\n", _parse_buffer);
}


/******************************************************************************************
 * INTERRUPT CALLBACKS
 ******************************************************************************************/

void IOT_UARTEx_RxEventCallback(UART_HandleTypeDef* huart, uint16_t Size) {
  //calculate next call's write offset in circular buffer
  _rx_buffer_write_offset += (uint32_t)Size;
  if (_rx_buffer_write_offset >= IOT_RXBUF_SIZE) {
    _rx_buffer_write_offset = 0;
  }

  //start next reception immediately to avoid receiver overrun (data loss)
  //single transaction limited to remaining buffer space before wrap-around, because the HAL function has no "circular buffer" concept
  if (HAL_UARTEx_ReceiveToIdle_IT(&huart6, _rx_buffer + _rx_buffer_write_offset, IOT_RXBUF_SIZE - _rx_buffer_write_offset) != HAL_OK) {
    DEBUG_PRINTF("*** NOTIFICATION RECEIVE ERROR\n");
    //TODO: handle error!! probably just a full reset?
  }
}

void IOT_UART_TxCpltCallback(UART_HandleTypeDef* huart) {
  DEBUG_PRINTF("[CMD] %s\n", _cmd_tx_buffer);
}


/******************************************************************************************
 * MAIN DRIVER FUNCTIONS
 ******************************************************************************************/

HAL_StatusTypeDef IOT_Init() {
  bool factory_reset = false; //TODO: determine based on multiple consecutive resets or something?
  HAL_StatusTypeDef res = HAL_OK;

  //reset module physically
  HAL_GPIO_WritePin(BT_RST_N_GPIO_Port, BT_RST_N_Pin, GPIO_PIN_RESET);

  HAL_UART_Abort(&huart6);

  //reset all driver state
  iot_driver_init_complete = false;

  _rx_buffer_write_offset = 0;
  _rx_buffer_read_offset = 0;
  _parse_buffer_write_offset = 0;
  _current_command = CMD_INIT;
  _next_cmd_tick = 0;
  _cmd_timeout_tick = HAL_MAX_DELAY;
  _init_config_changed = false;

  _IOT_ClearCommandQueue();

  //take module out of reset after a small delay, or long delay for factory reset
  HAL_Delay(factory_reset ? 1800 : 1);
  //start reception of notifications
  if (HAL_UARTEx_ReceiveToIdle_IT(&huart6, _rx_buffer + _rx_buffer_write_offset, IOT_RXBUF_SIZE - _rx_buffer_write_offset) != HAL_OK) {
    res = HAL_ERROR;
  }
  HAL_GPIO_WritePin(BT_RST_N_GPIO_Port, BT_RST_N_Pin, GPIO_PIN_SET);

  //schedule init timeout
  _cmd_timeout_tick = HAL_GetTick() + IOT_CMD_TIMEOUT;

  return res;
}

void IOT_Update() {
  uint32_t tick = HAL_GetTick();

  //handle received data
  while (_rx_buffer_read_offset != _rx_buffer_write_offset) {
    //read character from receive buffer, transfer to parse buffer, and increment offsets
    char rx_char = _rx_text[_rx_buffer_read_offset++];
    _parse_buffer[_parse_buffer_write_offset++] = rx_char;
    //handle circular receive buffer
    if (_rx_buffer_read_offset >= IOT_RXBUF_SIZE) {
      _rx_buffer_read_offset = 0;
    }

    if (rx_char == '\r') {
      //received CR character: notification complete, so null-terminate parse buffer, reset write offset, and parse the notification
      _parse_buffer[_parse_buffer_write_offset] = '\0';
      _parse_buffer_write_offset = 0;

      DEBUG_PRINTF("[NOTIF] %s\n", _parse_buffer);

      _IOT_ParseNotification();
    } else if (_parse_buffer_write_offset >= IOT_PARSEBUF_SIZE - 1) {
      //parse buffer overflow: reset parse buffer
      DEBUG_PRINTF("*** ERROR: Notification parse buffer overflow\n");
      _parse_buffer_write_offset = 0;
    }
  }

  //check for command timeout
  if (_current_command != CMD_NONE && (int32_t)(tick - _cmd_timeout_tick) >= 0) {
    _IOT_Command_Timeout();
  }

  //send next command if it's time for that
  if (_current_command == CMD_NONE && (int32_t)(tick - _next_cmd_tick) >= 0) {
    //conditions for next command are met: dequeue command, if there is one
    IOT_CommandQueueItem* item = _IOT_DequeueCommand();
    if (item == NULL) {
      //no command queued: update next command tick (to avoid eventual underflows), try again next time
      _next_cmd_tick = tick;
    } else {
      //command queued: sanity-check length first
      uint32_t length = item->cmd_length;
      if (length < IOT_CMDBUF_SIZE) {
        //length okay: proceed, extract queue item values
        _current_command = item->command;
        memcpy(_cmd_tx_buffer, item->cmd_text, length);
        _cmd_tx_buffer[length + 1] = '\0';

        //start command
        if (HAL_UART_Transmit_IT(&huart6, (uint8_t*)_cmd_tx_buffer, length) == HAL_OK) {
          //send success: free queue item memory, schedule timeout
          free(item);
          _cmd_timeout_tick = tick + IOT_CMD_TIMEOUT;
        } else {
          //error: reset to no command, update next command tick, requeue item to try again next time
          _current_command = CMD_NONE;
          _next_cmd_tick = tick;
          _IOT_RequeueCommand(item);
          DEBUG_PRINTF("*** Command transmit error on command %s\n", _cmd_tx_buffer);
        }
      }
    }
  }
}

