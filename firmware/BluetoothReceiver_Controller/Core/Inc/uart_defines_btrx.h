/*
 * uart_defines_btrx.h
 *
 *  Created on: Mar 8, 2025
 *      Author: Alex
 *
 *  Bluetooth Receiver UART protocol definitions
 *
 *  Message format: START Type Parameters END
 *  START = 0xF1
 *  END = 0xFA
 *  Escape = 0xFF (escapes START, END, and Escape)
 *
 *  Master->Slave types:
 *    - 0x00: Read (Parameters: Register)
 *    - 0x01: Write (Parameters: Register, Data)
 *  Slave->Master types:
 *    - 0x00: Event (Parameters: Event type, Event parameters)
 *    - 0x01: Change notification (Parameters: Register, Data)
 *    - 0x02: Read response (Parameters: Register, Data)
 *
 *  Event definitions:
 *    - 0x00: Controller (re)started (No parameters)
 *    - 0x01: Write Acknowledge (No parameters)
 *    - 0x02: Error (Parameter: Error code, 2B, hex)
 *
 *  Error codes:
 *    - 0x0003: UNKNOWN_ERROR
 *    - 0x0011: COMMAND_NOT_ALLOWED_CONFIG
 *    - 0x0012: COMMAND_NOT_FOUND
 *    - 0x0013: WRONG_PARAMETER
 *    - 0x0014: WRONG_NUMBER_OF_PARAMETERS
 *    - 0x0015: COMMAND_NOT_ALLOWED_STATE
 *    - 0x0016: DEVICE_ALREADY_CONNECTED
 *    - 0x0017: DEVICE_NOT_CONNECTED
 *    - 0x0018: COMMAND_TOO_LONG
 *    - 0x0019: NAME_NOT_FOUND
 *    - 0x001A: CONFIG_NOT_FOUND
 *    - 0x00F0: OPEN_GENERAL_ERROR
 *    - 0x00F1: OPEN_DEVICE_NOT_PRESENT
 *    - 0x00F2: OPEN_DEVICE_NOT_PAIRED
 *    - 0x00FF: COMMAND_TIMEOUT
 *
 *  Register map:
 *  * Status registers
 *    - 0x00: STATUS: General status (2B, bit field, r)
 *    - 0x01: VOLUME: Absolute volume (1B, 0-127, rw)
 *    - 0x02: CONN_STATS: Bluetooth connection stats (4B, 2B(high) signed RSSI + 2B(low) unsigned quality, r)
 *    - 0x03: DEVICE_NAME: Bluetooth device name (?B, ASCII, r)
 *    - 0x04: CODEC: Bluetooth codec used (?B, ASCII, r)
 *    - 0x05: TITLE: Media title (?B, ASCII, r)
 *    - 0x06: ARTIST: Media artist (?B, ASCII, r)
 *    - 0x07: ALBUM: Media album (?B, ASCII, r)
 *  * Notification registers
 *    - 0x20: NOTIF_MASK: Change notification mask (1B, bit field, rw, high = change notification enable)
 *  * Control registers
 *    - 0x30: CONTROL: General control (1B, bit field, w)
 *    - 0x31: CONN_CONTROL: Connection control (1B, bit field, w)
 *    - 0x32: MEDIA_CONTROL: Media control (1B, enum, w)
 *  * Misc registers
 *    - 0xFE: MODULE_ID: Module ID (1B, hex, r)
 *
 *  Bit field and enum definitions:
 *  * STATUS (0x00, 2B bit field):
 *    - 15: UARTERR: UART communication error detected since last STATUS read
 *    - 8: OTA_UPGRADE_EN: whether OTA firmware upgrades are enabled
 *    - 7: AVRCP_PLAYING: whether AVRCP is in "playing" state
 *    - 6: A2DP_STREAMING: whether A2DP link is streaming audio
 *    - 5: AVRCP_LINK: whether an AVRCP link is established
 *    - 4: A2DP_LINK: whether an A2DP link is established
 *    - 3: CONNECTED: whether a Bluetooth device is connected
 *    - 2: DISCOVERABLE: whether Bluetooth is in pairing mode
 *    - 1: CONNECTABLE: whether Bluetooth connections are accepted
 *    - 0: INIT_DONE: whether module init has been completed
 *  * NOTIF_MASK (0x20, 1B bit field):
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


#define UARTDEF_BTRX_START_BYTE 0xF1
#define UARTDEF_BTRX_END_BYTE 0xFA
#define UARTDEF_BTRX_ESCAPE_BYTE 0xFF





#endif /* INC_UART_DEFINES_BTRX_H_ */
