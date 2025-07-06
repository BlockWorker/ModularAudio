/*
 * uart_defines_btrx.h
 *
 *  Created on: Mar 8, 2025
 *      Author: Alex
 *
 *  Bluetooth Receiver UART protocol definitions
 *
 *  Message format: START Type Parameters END
 *    - START = 0xF1
 *    - END = 0xFA
 *    - Escape = 0xFF (escapes START, END, and Escape)
 *
 *  Master->Slave types:
 *    - 0x00: Read (Parameters: Register)
 *    - 0x01: Write (Parameters: Register, Data)
 *  Slave->Master types:
 *    - 0x00: Event (Parameters: Event type, Event parameters)
 *    - 0x01: Change notification (Parameters: Register, Data)
 *    - 0x02: Read data (Parameters: Register, Data)
 *
 *  Event definitions:
 *    - 0x00: MCU_RESET: Controller (re)started (No parameters)
 *    - 0x01: WRITE_ACK: Write Acknowledge (Parameter: Register)
 *    - 0x02: ERROR: Error (Parameter: Error code, 2B, hex)
 *    - 0x03: BT_RESET: Bluetooth module restarted, e.g. due to internal error or config change (No parameters)
 *
 *  Error codes:
 *    - 0x0003: UNKNOWN_ERROR: Unknown error
 *    - 0x0011: COMMAND_NOT_ALLOWED_CONFIG: BT command not allowed in the current configuration
 *    - 0x0012: COMMAND_NOT_FOUND: BT command not found
 *    - 0x0013: WRONG_PARAMETER: BT command parameter invalid
 *    - 0x0014: WRONG_NUMBER_OF_PARAMETERS: BT command parameter count wrong
 *    - 0x0015: COMMAND_NOT_ALLOWED_STATE: BT command not allowed in the current state
 *    - 0x0016: DEVICE_ALREADY_CONNECTED: BT device already connected
 *    - 0x0017: DEVICE_NOT_CONNECTED: BT device not connected
 *    - 0x0018: COMMAND_TOO_LONG: BT command too long
 *    - 0x0019: NAME_NOT_FOUND: BT name not found
 *    - 0x001A: CONFIG_NOT_FOUND: BT config not found
 *    - 0x00F0: OPEN_GENERAL_ERROR: BT connection open failed due to generic error
 *    - 0x00F1: OPEN_DEVICE_NOT_PRESENT: BT connection open failed because device could not be found
 *    - 0x00F2: OPEN_DEVICE_NOT_PAIRED: BT connection open failed because device is not paired
 *    - 0x00FF: COMMAND_TIMEOUT: BT command timed out before completion
 *    - 0x8001: UART_FORMAT_ERROR: Received UART command is malformed
 *    - 0x8002: INTERNAL_UART_ERROR: Internal UART communication error
 *    - 0x8003: UART_COMMAND_NOT_ALLOWED: Given UART command is not allowed in the current state
 *    - 0x8004: UART_CRC_ERROR: Received UART command has failed the CRC integrity check
 *
 *
 *  Register map:
 *  * Status registers
 *    - 0x00: STATUS: General status (2B, bit field, r)
 *    - 0x01: VOLUME: Absolute volume (1B, 0-127, rw)
 *    - 0x02: TITLE: Media title (?B, text, r)
 *    - 0x03: ARTIST: Media artist (?B, text, r)
 *    - 0x04: ALBUM: Media album (?B, text, r)
 *    - 0x05: DEVICE_ADDR: Connected Bluetooth device address (6B, hex, r)
 *    - 0x06: DEVICE_NAME: Connected Bluetooth device name (?B, text, r)
 *    - 0x07: CONN_STATS: Bluetooth connection stats (4B, 2B(low) signed RSSI + 2B(high) unsigned quality, r)
 *    - 0x08: CODEC: Active A2DP codec (?B, text, r)
 *  * Notification registers
 *    - 0x20: NOTIF_MASK: Change notification mask (2B, bit field, rw, high = change notification enable), defaults to 0x0000 (no change notifications)
 *  * Control registers
 *    - 0x30: CONTROL: General control (1B, bit field, w)
 *    - 0x31: CONN_CONTROL: Connection control (1B, bit field, w)
 *    - 0x32: MEDIA_CONTROL: Media control (1B, enum, w)
 *    - 0x33: TONE: Play a tone defined by the parameter (see IDC777 manual) (<240B, text, w)
 *  * Misc registers
 *    - 0xFE: MODULE_ID: Module ID (1B, hex, r)
 *
 *  Bit field and enum definitions:
 *  * STATUS (0x00, 2B bit field):
 *    - 15: UARTERR: UART communication error detected since last STATUS read - not included in change notifications
 *    - 8: OTA_UPGRADE_EN: whether OTA firmware upgrades are enabled
 *    - 7: AVRCP_PLAYING: whether AVRCP is in "playing" state
 *    - 6: A2DP_STREAMING: whether A2DP link is streaming audio
 *    - 5: AVRCP_LINK: whether an AVRCP link is established
 *    - 4: A2DP_LINK: whether an A2DP link is established
 *    - 3: CONNECTED: whether a Bluetooth device is connected
 *    - 2: DISCOVERABLE: whether Bluetooth is in pairing mode
 *    - 1: CONNECTABLE: whether Bluetooth connections are accepted
 *    - 0: INIT_DONE: whether module init has been completed
 *  * NOTIF_MASK (0x20, 2B bit field):
 *    Bit X high enables change notification for status register 0x0X
 *  * CONTROL (0x30, 1B bit field):
 *    - 4-7: RESET: controller reset (write 0xA to trigger software reset)
 *    - 0: ENABLE_OTA_UPGRADE: write 1 to enable OTA firmware upgrades (until next controller reset)
 *  * CONN_CONTROL (0x31, 1B bit field):
 *    - 4: DISCONNECT: write 1 to close all current Bluetooth connections
 *    - 3: DISCOVERABLE_OFF: write 1 to end Bluetooth pairing mode (takes precedence over DISCOVERABLE_ON)
 *    - 2: DISCOVERABLE_ON: write 1 to start Bluetooth pairing mode (unless DISCOVERABLE_OFF is also set)
 *    - 1: CONNECTABLE_OFF: write 1 to disallow Bluetooth connections (takes precedence over CONNECTABLE_ON)
 *    - 0: CONNECTABLE_ON: write 1 to allow Bluetooth connections (unless CONNECTABLE_OFF is also set)
 *  * MEDIA_CONTROL (0x32, 1B enum):
 *    - 0x01: PLAY
 *    - 0x02: PAUSE
 *    - 0x03: STOP
 *    - 0x04: FORWARD
 *    - 0x05: BACKWARD
 *    - 0x06: FF_START
 *    - 0x07: FF_END
 *    - 0x08: REW_START
 *    - 0x09: REW_END
 *
 */

#ifndef INC_UART_DEFINES_BTRX_H_
#define INC_UART_DEFINES_BTRX_H_


//Link layer definitions
#define UARTDEF_BTRX_START_BYTE 0xF1
#define UARTDEF_BTRX_END_BYTE 0xFA
#define UARTDEF_BTRX_ESCAPE_BYTE 0xFF


//Message types
//Master -> Slave
#define UARTDEF_BTRX_TYPE_READ 0x00
#define UARTDEF_BTRX_TYPE_WRITE 0x01
//Slave -> Master
#define UARTDEF_BTRX_TYPE_EVENT 0x00
#define UARTDEF_BTRX_TYPE_CHANGE_NOTIFICATION 0x01
#define UARTDEF_BTRX_TYPE_READ_DATA 0x02


//Events
#define UARTDEF_BTRX_EVENT_MCU_RESET 0x00
#define UARTDEF_BTRX_EVENT_WRITE_ACK 0x01
#define UARTDEF_BTRX_EVENT_ERROR 0x02
#define UARTDEF_BTRX_EVENT_BT_RESET 0x03

//Error codes
#define UARTDEF_BTRX_ERROR_UNKNOWN_ERROR              0x0003
#define UARTDEF_BTRX_ERROR_COMMAND_NOT_ALLOWED_CONFIG 0x0011
#define UARTDEF_BTRX_ERROR_COMMAND_NOT_FOUND          0x0012
#define UARTDEF_BTRX_ERROR_WRONG_PARAMETER            0x0013
#define UARTDEF_BTRX_ERROR_WRONG_NUMBER_OF_PARAMETERS 0x0014
#define UARTDEF_BTRX_ERROR_COMMAND_NOT_ALLOWED_STATE  0x0015
#define UARTDEF_BTRX_ERROR_DEVICE_ALREADY_CONNECTED   0x0016
#define UARTDEF_BTRX_ERROR_DEVICE_NOT_CONNECTED       0x0017
#define UARTDEF_BTRX_ERROR_COMMAND_TOO_LONG           0x0018
#define UARTDEF_BTRX_ERROR_NAME_NOT_FOUND             0x0019
#define UARTDEF_BTRX_ERROR_CONFIG_NOT_FOUND           0x001A
#define UARTDEF_BTRX_ERROR_OPEN_GENERAL_ERROR         0x00F0
#define UARTDEF_BTRX_ERROR_OPEN_DEVICE_NOT_PRESENT    0x00F1
#define UARTDEF_BTRX_ERROR_OPEN_DEVICE_NOT_PAIRED     0x00F2
#define UARTDEF_BTRX_ERROR_COMMAND_TIMEOUT            0x00FF
#define UARTDEF_BTRX_ERROR_UART_FORMAT_ERROR          0x8001
#define UARTDEF_BTRX_ERROR_INTERNAL_UART_ERROR        0x8002
#define UARTDEF_BTRX_ERROR_UART_COMMAND_NOT_ALLOWED   0x8003
#define UARTDEF_BTRX_ERROR_UART_CRC_ERROR             0x8004


//Register sizes in bytes - 0 means register is invalid
#define UARTDEF_BTRX_STRINGREG_SIZE 256
#define UARTDEF_BTRX_REG_SIZES {\
  2, 1, UARTDEF_BTRX_STRINGREG_SIZE, UARTDEF_BTRX_STRINGREG_SIZE, UARTDEF_BTRX_STRINGREG_SIZE, 6, UARTDEF_BTRX_STRINGREG_SIZE, 4, UARTDEF_BTRX_STRINGREG_SIZE, 0, 0, 0, 0, 0, 0, 0,\
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  1, 1, 1, UARTDEF_BTRX_STRINGREG_SIZE, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,\
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0 }


//Status registers
#define UARTDEF_BTRX_STATUS 0x00

#define UARTDEF_BTRX_STATUS_INIT_DONE_Pos 0
#define UARTDEF_BTRX_STATUS_INIT_DONE_Msk (0x1 << UARTDEF_BTRX_STATUS_INIT_DONE_Pos)
#define UARTDEF_BTRX_STATUS_CONNECTABLE_Pos 1
#define UARTDEF_BTRX_STATUS_CONNECTABLE_Msk (0x1 << UARTDEF_BTRX_STATUS_CONNECTABLE_Pos)
#define UARTDEF_BTRX_STATUS_DISCOVERABLE_Pos 2
#define UARTDEF_BTRX_STATUS_DISCOVERABLE_Msk (0x1 << UARTDEF_BTRX_STATUS_DISCOVERABLE_Pos)
#define UARTDEF_BTRX_STATUS_CONNECTED_Pos 3
#define UARTDEF_BTRX_STATUS_CONNECTED_Msk (0x1 << UARTDEF_BTRX_STATUS_CONNECTED_Pos)
#define UARTDEF_BTRX_STATUS_A2DP_LINK_Pos 4
#define UARTDEF_BTRX_STATUS_A2DP_LINK_Msk (0x1 << UARTDEF_BTRX_STATUS_A2DP_LINK_Pos)
#define UARTDEF_BTRX_STATUS_AVRCP_LINK_Pos 5
#define UARTDEF_BTRX_STATUS_AVRCP_LINK_Msk (0x1 << UARTDEF_BTRX_STATUS_AVRCP_LINK_Pos)
#define UARTDEF_BTRX_STATUS_A2DP_STREAMING_Pos 6
#define UARTDEF_BTRX_STATUS_A2DP_STREAMING_Msk (0x1 << UARTDEF_BTRX_STATUS_A2DP_STREAMING_Pos)
#define UARTDEF_BTRX_STATUS_AVRCP_PLAYING_Pos 7
#define UARTDEF_BTRX_STATUS_AVRCP_PLAYING_Msk (0x1 << UARTDEF_BTRX_STATUS_AVRCP_PLAYING_Pos)
#define UARTDEF_BTRX_STATUS_OTA_UPGRADE_EN_Pos 8
#define UARTDEF_BTRX_STATUS_OTA_UPGRADE_EN_Msk (0x1 << UARTDEF_BTRX_STATUS_OTA_UPGRADE_EN_Pos)
#define UARTDEF_BTRX_STATUS_UARTERR_Pos 15
#define UARTDEF_BTRX_STATUS_UARTERR_Msk (0x1 << UARTDEF_BTRX_STATUS_UARTERR_Pos)

#define UARTDEF_BTRX_VOLUME 0x01

#define UARTDEF_BTRX_TITLE 0x02

#define UARTDEF_BTRX_ARTIST 0x03

#define UARTDEF_BTRX_ALBUM 0x04

#define UARTDEF_BTRX_DEVICE_ADDR 0x05

#define UARTDEF_BTRX_DEVICE_NAME 0x06

#define UARTDEF_BTRX_CONN_STATS 0x07

#define UARTDEF_BTRX_CODEC 0x08


//Notification registers
#define UARTDEF_BTRX_NOTIF_MASK 0x20

#define UARTDEF_BTRX_NOTIF_MASK_STATUS_Pos UARTDEF_BTRX_STATUS
#define UARTDEF_BTRX_NOTIF_MASK_STATUS_Msk (0x1 << UARTDEF_BTRX_NOTIF_MASK_STATUS_Pos)
#define UARTDEF_BTRX_NOTIF_MASK_VOLUME_Pos UARTDEF_BTRX_VOLUME
#define UARTDEF_BTRX_NOTIF_MASK_VOLUME_Msk (0x1 << UARTDEF_BTRX_NOTIF_MASK_VOLUME_Pos)
#define UARTDEF_BTRX_NOTIF_MASK_TITLE_Pos UARTDEF_BTRX_TITLE
#define UARTDEF_BTRX_NOTIF_MASK_TITLE_Msk (0x1 << UARTDEF_BTRX_NOTIF_MASK_TITLE_Pos)
#define UARTDEF_BTRX_NOTIF_MASK_ARTIST_Pos UARTDEF_BTRX_ARTIST
#define UARTDEF_BTRX_NOTIF_MASK_ARTIST_Msk (0x1 << UARTDEF_BTRX_NOTIF_MASK_ARTIST_Pos)
#define UARTDEF_BTRX_NOTIF_MASK_ALBUM_Pos UARTDEF_BTRX_ALBUM
#define UARTDEF_BTRX_NOTIF_MASK_ALBUM_Msk (0x1 << UARTDEF_BTRX_NOTIF_MASK_ALBUM_Pos)
#define UARTDEF_BTRX_NOTIF_MASK_DEVICE_ADDR_Pos UARTDEF_BTRX_DEVICE_ADDR
#define UARTDEF_BTRX_NOTIF_MASK_DEVICE_ADDR_Msk (0x1 << UARTDEF_BTRX_NOTIF_MASK_DEVICE_ADDR_Pos)
#define UARTDEF_BTRX_NOTIF_MASK_DEVICE_NAME_Pos UARTDEF_BTRX_DEVICE_NAME
#define UARTDEF_BTRX_NOTIF_MASK_DEVICE_NAME_Msk (0x1 << UARTDEF_BTRX_NOTIF_MASK_DEVICE_NAME_Pos)
#define UARTDEF_BTRX_NOTIF_MASK_CONN_STATS_Pos UARTDEF_BTRX_CONN_STATS
#define UARTDEF_BTRX_NOTIF_MASK_CONN_STATS_Msk (0x1 << UARTDEF_BTRX_NOTIF_MASK_CONN_STATS_Pos)
#define UARTDEF_BTRX_NOTIF_MASK_CODEC_Pos UARTDEF_BTRX_CODEC
#define UARTDEF_BTRX_NOTIF_MASK_CODEC_Msk (0x1 << UARTDEF_BTRX_NOTIF_MASK_CODEC_Pos)

#define UARTDEF_BTRX_NOTIF_MASK_DEFAULT_VALUE 0x0000


//Control registers
#define UARTDEF_BTRX_CONTROL 0x30

#define UARTDEF_BTRX_CONTROL_ENABLE_OTA_UPGRADE_Pos 0
#define UARTDEF_BTRX_CONTROL_ENABLE_OTA_UPGRADE_Msk (0x1 << UARTDEF_BTRX_CONTROL_ENABLE_OTA_UPGRADE_Pos)
#define UARTDEF_BTRX_CONTROL_RESET_Pos 4
#define UARTDEF_BTRX_CONTROL_RESET_Msk (0xF << UARTDEF_BTRX_CONTROL_RESET_Pos)
#define UARTDEF_BTRX_CONTROL_RESET_VALUE 0xA

#define UARTDEF_BTRX_CONN_CONTROL 0x31

#define UARTDEF_BTRX_CONN_CONTROL_CONNECTABLE_ON_Pos 0
#define UARTDEF_BTRX_CONN_CONTROL_CONNECTABLE_ON_Msk (0x1 << UARTDEF_BTRX_CONN_CONTROL_CONNECTABLE_ON_Pos)
#define UARTDEF_BTRX_CONN_CONTROL_CONNECTABLE_OFF_Pos 1
#define UARTDEF_BTRX_CONN_CONTROL_CONNECTABLE_OFF_Msk (0x1 << UARTDEF_BTRX_CONN_CONTROL_CONNECTABLE_OFF_Pos)
#define UARTDEF_BTRX_CONN_CONTROL_DISCOVERABLE_ON_Pos 2
#define UARTDEF_BTRX_CONN_CONTROL_DISCOVERABLE_ON_Msk (0x1 << UARTDEF_BTRX_CONN_CONTROL_DISCOVERABLE_ON_Pos)
#define UARTDEF_BTRX_CONN_CONTROL_DISCOVERABLE_OFF_Pos 3
#define UARTDEF_BTRX_CONN_CONTROL_DISCOVERABLE_OFF_Msk (0x1 << UARTDEF_BTRX_CONN_CONTROL_DISCOVERABLE_OFF_Pos)
#define UARTDEF_BTRX_CONN_CONTROL_DISCONNECT_Pos 4
#define UARTDEF_BTRX_CONN_CONTROL_DISCONNECT_Msk (0x1 << UARTDEF_BTRX_CONN_CONTROL_DISCONNECT_Pos)

#define UARTDEF_BTRX_MEDIA_CONTROL 0x32

#define UARTDEF_BTRX_MEDIA_CONTROL_PLAY_Val 0x01
#define UARTDEF_BTRX_MEDIA_CONTROL_PAUSE_Val 0x02
#define UARTDEF_BTRX_MEDIA_CONTROL_STOP_Val 0x03
#define UARTDEF_BTRX_MEDIA_CONTROL_FORWARD_Val 0x04
#define UARTDEF_BTRX_MEDIA_CONTROL_BACKWARD_Val 0x05
#define UARTDEF_BTRX_MEDIA_CONTROL_FF_START_Val 0x06
#define UARTDEF_BTRX_MEDIA_CONTROL_FF_END_Val 0x07
#define UARTDEF_BTRX_MEDIA_CONTROL_REW_START_Val 0x08
#define UARTDEF_BTRX_MEDIA_CONTROL_REW_END_Val 0x09

#define UARTDEF_BTRX_TONE 0x33


//Misc registers
#define UARTDEF_BTRX_MODULE_ID 0xFE
#define UARTDEF_BTRX_MODULE_ID_VALUE 0xB2


#endif /* INC_UART_DEFINES_BTRX_H_ */
