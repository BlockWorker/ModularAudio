/*
 * uart_defines_BMS.h
 *
 *  Created on: Mar 10, 2025
 *      Author: Alex
 *
 *  Battery Monitor UART protocol definitions
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
 *
 *  Event definitions:
 *    - 0x00: MCU_RESET: Controller (re)started (No parameters)
 *    - 0x01: WRITE_ACK: Write Acknowledge (Parameter: Register)
 *    - 0x02: ERROR: Error (Parameter: Error code, 2B, hex)
 *
 *  Error codes:
 *    - 0x0010: BMS_I2C_ERROR: BMS I2C communication or checksum error
 *    - 0x3001: FLASH_ERROR: Flash/EEPROM read/write error
 *    - 0x8001: UART_FORMAT_ERROR: Received UART command is malformed
 *    - 0x8002: INTERNAL_UART_ERROR: Internal UART communication error
 *    - 0x8003: UART_COMMAND_NOT_ALLOWED: Given UART command is not allowed in the current state
 *
 *
 *  Register map:
 *  * Status registers
 *    - 0x00: STATUS: General status (1B, bit field, r)
 *    - 0x01: STACK_VOLTAGE: Total battery stack voltage (2B, unsigned in mV, r)
 *    - 0x02: CELL_VOLTAGES: Individual cell voltages (cellcount * 2B, each signed in mV, r)
 *    - 0x03: CURRENT: Pack current (4B, signed in mA, positive = charge, r)
 *    - 0x04: SOC_FRACTION: State-of-charge as a fraction of full (5B, 4B float in [0, 1] + 1B confidence level, r)
 *    - 0x05: SOC_ENERGY: State-of-charge in terms of pack energy (5B, 4B float in Wh + 1B confidence level, r)
 *    - 0x06: HEALTH: Estimated battery health as a fraction of factory-new (4B, float in [0, 1], rw)
 *    - 0x07: BAT_TEMP: Battery pack temperature (2B, signed in °C, r)
 *    - 0x08: INT_TEMP: Battery monitor IC temperature (2B, signed in °C, r)
 *    - 0x09: ALERTS: Active safety alerts/warnings (2B, bit field, r)
 *    - 0x0A: FAULTS: Active safety faults (2B, bit field, r)
 *    - 0x0B: SHUTDOWN: Timed shutdown information (3B, 1B shutdown type enum + 2B time until shutdown unsigned in ms, r)
 *  * Notification registers
 *    - 0x20: NOTIF_MASK: Change notification mask (2B, bit field, rw, high = change notification enable), defaults to 0x0C00 (faults and shutdown)
 *  * Control registers
 *    - 0x30: CONTROL: General control (1B, bit field, rw)
 *  * Constant registers
 *    - 0xE0: CELLS_SERIES: Number of series cells in the pack, corresponds to the number of values returned by the CELL_VOLTAGES register (1B, unsigned, r)
 *    - 0xE1: CELLS_PARALLEL: Number of parallel cells in the pack (1B, unsigned, r)
 *    - 0xE2: MIN_VOLTAGE: End-of-discharge cell voltage (2B, unsigned in mV, r)
 *    - 0xE3: MAX_VOLTAGE: Maximum full-charge cell voltage (2B, unsigned in mV, r)
 *    - 0xE4: MAX_DSG_CURRENT: Maximum sustained discharge current (4B, unsigned in mA, r)
 *    - 0xE5: PEAK_DSG_CURRENT: Maximum peak discharge current (4B, unsigned in mA, r)
 *    - 0xE6: MAX_CHG_CURRENT: Maximum charge current (4B, unsigned in mA, r)
 *    - 0xFE: MODULE_ID: Module ID (1B, hex, r)
 *
 *  Bit field and enum definitions:
 *  * STATUS (0x00, 1B bit field):
 *    - 7: UARTERR: UART communication error detected since last STATUS read - not included in change notifications
 *    - 6: FLASH_ERR: Flash/EEPROM read or write error has occurred since the last STATUS read - not included in change notifications
 *    - 5: BMS_I2C_ERR: BMS I2C communication or checksum error has occurred since the last STATUS read - not included in change notifications
 *    - 3: SHUTDOWN: A timed shutdown is scheduled, more details in the SHUTDOWN register
 *    - 2: CHG_FAULT: A charge fault has been latched and must be reset by software before charging is allowed
 *    - 1: ALERT: Any safety alert is present
 *    - 0: FAULT: Any safety fault is present
 *  * SoC confidence level (0x04 and 0x05 fifth bytes, 1B enum):
 *    - 0x00: INVALID: No valid state-of-charge estimation available
 *    - 0x01: VOLTAGE_ONLY: State-of-charge estimated directly based on voltage; not precise
 *    - 0x02: ESTIMATED_REF: State-of-charge estimated using counted charge based on a voltage-estimated reference; good relative precision but may have a significant absolute offset
 *    - 0x03: MEASURED_REF: State-of-charge estimated using counted charge based on a previous full charge as a reference; good relative and absolute precision
 *  * SHUTDOWN type (0x0B first byte, 1B enum):
 *    - 0x00: NONE: No shutdown scheduled (time field is meaningless)
 *    - 0x01: FULL_SHUTDOWN: Full shutdown (MCU power off)
 *    - 0x02: END_OF_DISCHARGE: Preemptive end-of-discharge shutdown
 *    - 0x03: HOST_REQUEST: Host-requested shutdown
 *  * NOTIF_MASK (0x20, 2B bit field):
 *    Bit X high enables change notification for status register 0x0X
 *  * CONTROL (0x30, 1B bit field):
 *    - 4-7: RESET: controller reset (write 0xA to trigger software reset), reads as 0
 *    - 2: FULL_SHUTDOWN: Write 1 to request full battery pack shutdown in 10 seconds, not cancellable
 *    - 1: REQ_SHUTDOWN: Write 1 to request shutdown in 10 seconds, reads back written bit, can write 0 again before shutdown to cancel it
 *    - 0: CLEAR_CHG_FAULT: Write 1 to clear a latched charge fault and re-enable charging (see STATUS CHG_FAULT bit), reads as 0
 *
 */

#ifndef INC_UART_DEFINES_BMS_H_
#define INC_UART_DEFINES_BMS_H_


//Link layer definitions
#define UARTDEF_BMS_START_BYTE 0xF1
#define UARTDEF_BMS_END_BYTE 0xFA
#define UARTDEF_BMS_ESCAPE_BYTE 0xFF


//Message types
//Master -> Slave
#define UARTDEF_BMS_TYPE_READ 0x00
#define UARTDEF_BMS_TYPE_WRITE 0x01
//Slave -> Master
#define UARTDEF_BMS_TYPE_EVENT 0x00
#define UARTDEF_BMS_TYPE_CHANGE_NOTIFICATION 0x01
#define UARTDEF_BMS_TYPE_READ_DATA 0x02


//Events
#define UARTDEF_BMS_EVENT_MCU_RESET 0x00
#define UARTDEF_BMS_EVENT_WRITE_ACK 0x01
#define UARTDEF_BMS_EVENT_ERROR 0x02

//Error codes
#define UARTDEF_BMS_ERROR_BMS_I2C_ERROR               0x0010
#define UARTDEF_BMS_ERROR_FLASH_ERROR                 0x3001
#define UARTDEF_BMS_ERROR_UART_FORMAT_ERROR           0x8001
#define UARTDEF_BMS_ERROR_INTERNAL_UART_ERROR         0x8002
#define UARTDEF_BMS_ERROR_UART_COMMAND_NOT_ALLOWED    0x8003


//Status registers
#define UARTDEF_BMS_STATUS 0x00

#define UARTDEF_BMS_STATUS_FAULT_Pos 0
#define UARTDEF_BMS_STATUS_FAULT_Msk (0x1 << UARTDEF_BMS_STATUS_FAULT_Pos)
#define UARTDEF_BMS_STATUS_ALERT_Pos 1
#define UARTDEF_BMS_STATUS_ALERT_Msk (0x1 << UARTDEF_BMS_STATUS_ALERT_Pos)
#define UARTDEF_BMS_STATUS_CHG_FAULT_Pos 2
#define UARTDEF_BMS_STATUS_CHG_FAULT_Msk (0x1 << UARTDEF_BMS_STATUS_CHG_FAULT_Pos)
#define UARTDEF_BMS_STATUS_SHUTDOWN_Pos 3
#define UARTDEF_BMS_STATUS_SHUTDOWN_Msk (0x1 << UARTDEF_BMS_STATUS_SHUTDOWN_Pos)
#define UARTDEF_BMS_STATUS_BMS_I2C_ERR_Pos 5
#define UARTDEF_BMS_STATUS_BMS_I2C_ERR_Msk (0x1 << UARTDEF_BMS_STATUS_BMS_I2C_ERR_Pos)
#define UARTDEF_BMS_STATUS_FLASH_ERR_Pos 6
#define UARTDEF_BMS_STATUS_FLASH_ERR_Msk (0x1 << UARTDEF_BMS_STATUS_FLASH_ERR_Pos)
#define UARTDEF_BMS_STATUS_UARTERR_Pos 7
#define UARTDEF_BMS_STATUS_UARTERR_Msk (0x1 << UARTDEF_BMS_STATUS_UARTERR_Pos)

#define UARTDEF_BMS_STACK_VOLTAGE 0x01

#define UARTDEF_BMS_CELL_VOLTAGES 0x02

#define UARTDEF_BMS_CURRENT 0x03

#define UARTDEF_BMS_SOC_FRACTION 0x04
#define UARTDEF_BMS_SOC_ENERGY 0x05

#define UARTDEF_BMS_SOC_CONFIDENCE_INVALID 0x00
#define UARTDEF_BMS_SOC_CONFIDENCE_VOLTAGE_ONLY 0x01
#define UARTDEF_BMS_SOC_CONFIDENCE_ESTIMATED_REF 0x02
#define UARTDEF_BMS_SOC_CONFIDENCE_MEASURED_REF 0x03

#define UARTDEF_BMS_HEALTH 0x06

#define UARTDEF_BMS_BAT_TEMP 0x07

#define UARTDEF_BMS_INT_TEMP 0x08

#define UARTDEF_BMS_ALERTS 0x09

#define UARTDEF_BMS_FAULTS 0x0A

#define UARTDEF_BMS_SHUTDOWN 0x0B

#define UARTDEF_BMS_SHUTDOWN_TYPE_NONE 0x00
#define UARTDEF_BMS_SHUTDOWN_TYPE_FULL_SHUTDOWN 0x01
#define UARTDEF_BMS_SHUTDOWN_TYPE_END_OF_DISCHARGE 0x02
#define UARTDEF_BMS_SHUTDOWN_TYPE_HOST_REQUEST 0x03


//Notification registers
#define UARTDEF_BMS_NOTIF_MASK 0x20

#define UARTDEF_BMS_NOTIF_MASK_STATUS_Pos UARTDEF_BMS_STATUS
#define UARTDEF_BMS_NOTIF_MASK_STATUS_Msk (0x1 << UARTDEF_BMS_NOTIF_MASK_STATUS_Pos)
#define UARTDEF_BMS_NOTIF_MASK_STACK_VOLTAGE_Pos UARTDEF_BMS_STACK_VOLTAGE
#define UARTDEF_BMS_NOTIF_MASK_STACK_VOLTAGE_Msk (0x1 << UARTDEF_BMS_NOTIF_MASK_STACK_VOLTAGE_Pos)
#define UARTDEF_BMS_NOTIF_MASK_CELL_VOLTAGES_Pos UARTDEF_BMS_CELL_VOLTAGES
#define UARTDEF_BMS_NOTIF_MASK_CELL_VOLTAGES_Msk (0x1 << UARTDEF_BMS_NOTIF_MASK_CELL_VOLTAGES_Pos)
#define UARTDEF_BMS_NOTIF_MASK_CURRENT_Pos UARTDEF_BMS_CURRENT
#define UARTDEF_BMS_NOTIF_MASK_CURRENT_Msk (0x1 << UARTDEF_BMS_NOTIF_MASK_CURRENT_Pos)
#define UARTDEF_BMS_NOTIF_MASK_SOC_FRACTION_Pos UARTDEF_BMS_SOC_FRACTION
#define UARTDEF_BMS_NOTIF_MASK_SOC_FRACTION_Msk (0x1 << UARTDEF_BMS_NOTIF_MASK_SOC_FRACTION_Pos)
#define UARTDEF_BMS_NOTIF_MASK_SOC_ENERGY_Pos UARTDEF_BMS_SOC_ENERGY
#define UARTDEF_BMS_NOTIF_MASK_SOC_ENERGY_Msk (0x1 << UARTDEF_BMS_NOTIF_MASK_SOC_ENERGY_Pos)
#define UARTDEF_BMS_NOTIF_MASK_HEALTH_Pos UARTDEF_BMS_HEALTH
#define UARTDEF_BMS_NOTIF_MASK_HEALTH_Msk (0x1 << UARTDEF_BMS_NOTIF_MASK_HEALTH_Pos)
#define UARTDEF_BMS_NOTIF_MASK_BAT_TEMP_Pos UARTDEF_BMS_BAT_TEMP
#define UARTDEF_BMS_NOTIF_MASK_BAT_TEMP_Msk (0x1 << UARTDEF_BMS_NOTIF_MASK_BAT_TEMP_Pos)
#define UARTDEF_BMS_NOTIF_MASK_INT_TEMP_Pos UARTDEF_BMS_INT_TEMP
#define UARTDEF_BMS_NOTIF_MASK_INT_TEMP_Msk (0x1 << UARTDEF_BMS_NOTIF_MASK_INT_TEMP_Pos)
#define UARTDEF_BMS_NOTIF_MASK_ALERTS_Pos UARTDEF_BMS_ALERTS
#define UARTDEF_BMS_NOTIF_MASK_ALERTS_Msk (0x1 << UARTDEF_BMS_NOTIF_MASK_ALERTS_Pos)
#define UARTDEF_BMS_NOTIF_MASK_FAULTS_Pos UARTDEF_BMS_FAULTS
#define UARTDEF_BMS_NOTIF_MASK_FAULTS_Msk (0x1 << UARTDEF_BMS_NOTIF_MASK_FAULTS_Pos)
#define UARTDEF_BMS_NOTIF_MASK_SHUTDOWN_Pos UARTDEF_BMS_SHUTDOWN
#define UARTDEF_BMS_NOTIF_MASK_SHUTDOWN_Msk (0x1 << UARTDEF_BMS_NOTIF_MASK_SHUTDOWN_Pos)

#define UARTDEF_BMS_NOTIF_MASK_DEFAULT_VALUE 0x0C00


//Control registers
#define UARTDEF_BMS_CONTROL 0x30

#define UARTDEF_BMS_CONTROL_CLEAR_CHG_FAULT_Pos 0
#define UARTDEF_BMS_CONTROL_CLEAR_CHG_FAULT_Msk (0x1 << UARTDEF_BMS_CONTROL_CLEAR_CHG_FAULT_Pos)
#define UARTDEF_BMS_CONTROL_REQ_SHUTDOWN_Pos 1
#define UARTDEF_BMS_CONTROL_REQ_SHUTDOWN_Msk (0x1 << UARTDEF_BMS_CONTROL_REQ_SHUTDOWN_Pos)
#define UARTDEF_BMS_CONTROL_FULL_SHUTDOWN_Pos 2
#define UARTDEF_BMS_CONTROL_FULL_SHUTDOWN_Msk (0x1 << UARTDEF_BMS_CONTROL_FULL_SHUTDOWN_Pos)
#define UARTDEF_BMS_CONTROL_RESET_Pos 4
#define UARTDEF_BMS_CONTROL_RESET_Msk (0xF << UARTDEF_BMS_CONTROL_RESET_Pos)
#define UARTDEF_BMS_CONTROL_RESET_VALUE 0xA


//Constant registers
#define UARTDEF_BMS_CELLS_SERIES 0xE0

#define UARTDEF_BMS_CELLS_PARALLEL 0xE1

#define UARTDEF_BMS_MIN_VOLTAGE 0xE2

#define UARTDEF_BMS_MAX_VOLTAGE 0xE3

#define UARTDEF_BMS_MAX_DSG_CURRENT 0xE4

#define UARTDEF_BMS_PEAK_DSG_CURRENT 0xE5

#define UARTDEF_BMS_MAX_CHG_CURRENT 0xE6

#define UARTDEF_BMS_MODULE_ID 0xFE
#define UARTDEF_BMS_MODULE_ID_VALUE 0xBD


#endif /* INC_UART_DEFINES_BMS_H_ */
