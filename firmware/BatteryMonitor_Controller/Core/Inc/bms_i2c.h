/*
 * bms_i2c.h
 *
 *  Created on: Jan 12, 2025
 *      Author: Alex
 */

#ifndef INC_BMS_I2C_H_
#define INC_BMS_I2C_H_

#include "main.h"
#include "bms_config.h"


//I2C address of the BMS chip, left shifted for HAL functions and CRC calculations
#define BMS_I2C_ADDR (BMS_SET_I2C_ADDRESS << 1)

//Register addresses for subcommand and data memory transfers
#define BMS_I2C_REG_SUBCOMMAND 0x3E //2 bytes
#define BMS_I2C_REG_DATA 0x40       //up to 32 bytes
#define BMS_I2C_REG_CHECKSUM 0x60   //1 byte
#define BMS_I2C_REG_LENGTH 0x61     //1 byte

//I2C transaction timeout in milliseconds
#define BMS_I2C_TIMEOUT 100


//I2C direct commands
typedef enum {
  DIRCMD_SAFETY_ALERT_A = 0x02,
  DIRCMD_SAFETY_STATUS_A = 0x03,
  DIRCMD_SAFETY_ALERT_B = 0x04,
  DIRCMD_SAFETY_STATUS_B = 0x05,
  DIRCMD_BATTERY_STATUS = 0x12,
  DIRCMD_CELL1_VOLTAGE = 0x14,
  DIRCMD_CELL2_VOLTAGE = 0x16,
  DIRCMD_CELL3_VOLTAGE = 0x18,
  DIRCMD_CELL4_VOLTAGE = 0x1A,
  DIRCMD_CELL5_VOLTAGE = 0x1C,
  DIRCMD_REG18_VOLTAGE = 0x22,
  DIRCMD_VSS_VOLTAGE = 0x24,
  DIRCMD_STACK_VOLTAGE = 0x26,
  DIRCMD_INT_TEMP = 0x28,
  DIRCMD_TS_MEASUREMENT = 0x2A,
  DIRCMD_RAW_CURRENT = 0x36,
  DIRCMD_CURRENT = 0x3A,
  DIRCMD_CC1_CURRENT = 0x3C,
  DIRCMD_ALARM_STATUS = 0x62,
  DIRCMD_ALARM_RAW_STATUS = 0x64,
  DIRCMD_ALARM_ENABLE = 0x66,
  DIRCMD_FET_CONTROL = 0x68,
  DIRCMD_REGOUT_CONTROL = 0x69,
  DIRCMD_DSGFET_PWM_CONTROL = 0x6A,
  DIRCMD_CHGFET_PWM_CONTROL = 0x6C
} BMS_I2C_DirectCommand;

//I2C subcommands
typedef enum {
  //command-only
  SUBCMD_RESET_PASSQ = 0x0005,
  SUBCMD_EXIT_DEEPSLEEP = 0x000E,
  SUBCMD_DEEPSLEEP = 0x000F,
  SUBCMD_SHUTDOWN = 0x0010,
  SUBCMD_RESET = 0x0012,
  SUBCMD_FET_ENABLE = 0x0022,
  SUBCMD_SEAL = 0x0030,
  SUBCMD_SET_CFGUPDATE = 0x0090,
  SUBCMD_EXIT_CFGUPDATE = 0x0092,
  SUBCMD_SLEEP_ENABLE = 0x0099,
  SUBCMD_SLEEP_DISABLE = 0x009A,
  //with data
  SUBCMD_DEVICE_NUMBER = 0x0001,
  SUBCMD_FW_VERSION = 0x0002,
  SUBCMD_HW_VERSION = 0x0003,
  SUBCMD_PASSQ = 0x0004,
  SUBCMD_SECURITY_KEYS = 0x0035,
  SUBCMD_CB_ACTIVE_CELLS = 0x0083,
  SUBCMD_PROG_TIMER = 0x0094,
  SUBCMD_PROT_RECOVERY = 0x009B,
  //additional value to force underlying type to 16 bits
  __SUBCMD_INVALID = 0xFFFF
} BMS_I2C_SubCommand;

//I2C data memory register addresses
typedef enum {
  MEMADDR_CAL_CELL1_GAIN = 0x9000,
  MEMADDR_CAL_STACK_GAIN = 0x9002,
  MEMADDR_CAL_CELL2_DELTA = 0x9004,
  MEMADDR_CAL_CELL3_DELTA = 0x9005,
  MEMADDR_CAL_CELL4_DELTA = 0x9071,
  MEMADDR_CAL_CELL5_DELTA = 0x9072,
  MEMADDR_CAL_CURR_GAIN = 0x9006,
  MEMADDR_CAL_CURR_OFFSET = 0x9008,
  MEMADDR_CAL_CC1_GAIN = 0x900A,
  MEMADDR_CAL_CC1_OFFSET = 0x900C,
  MEMADDR_CAL_TS_OFFSET = 0x900E,
  MEMADDR_CAL_INT_TEMP_GAIN = 0x9010,
  MEMADDR_CAL_INT_TEMP_OFFSET = 0x9012,
  MEMADDR_SET_POWER_CONFIG = 0x9014,
  MEMADDR_SET_REGOUT_CONFIG = 0x9015,
  MEMADDR_SET_I2C_ADDRESS = 0x9016,
  MEMADDR_SET_I2C_CONFIG = 0x9017,
  MEMADDR_SET_DA_CONFIG = 0x9019,
  MEMADDR_SET_VCELL_MODE = 0x901B,
  MEMADDR_SET_DEFAULT_ALARM_MASK = 0x901C,
  MEMADDR_SET_FET_OPTIONS = 0x901E,
  MEMADDR_SET_CHGDET_TIME = 0x901F,
  MEMADDR_SET_BAL_CONFIG = 0x9020,
  MEMADDR_SET_BAL_MINTEMP_TS = 0x9021,
  MEMADDR_SET_BAL_MAXTEMP_TS = 0x9022,
  MEMADDR_SET_BAL_MAXTEMP_INT = 0x9023,
  MEMADDR_PROT_ENABLE_A = 0x9024,
  MEMADDR_PROT_ENABLE_B = 0x9025,
  MEMADDR_PROT_DSGFET_A = 0x9026,
  MEMADDR_PROT_CHGFET_A = 0x9027,
  MEMADDR_PROT_BOTHFET_B = 0x9028,
  MEMADDR_PROT_BODYDIODE_THRESHOLD = 0x9029,
  MEMADDR_PROT_COW_NORMAL_TIME = 0x902B,
  MEMADDR_PROT_COW_SLEEP_CONFIG = 0x902C,
  MEMADDR_PROT_HWD_TIMEOUT = 0x902D,
  MEMADDR_PROT_CUV_THRESHOLD = 0x902E,
  MEMADDR_PROT_CUV_DELAY = 0x9030,
  MEMADDR_PROT_CUV_HYSTERESIS = 0x9031,
  MEMADDR_PROT_COV_THRESHOLD = 0x9032,
  MEMADDR_PROT_COV_DELAY = 0x9034,
  MEMADDR_PROT_COV_HYSTERESIS = 0x9035,
  MEMADDR_PROT_OCC_THRESHOLD = 0x9036,
  MEMADDR_PROT_OCC_DELAY = 0x9037,
  MEMADDR_PROT_OCD1_THRESHOLD = 0x9038,
  MEMADDR_PROT_OCD1_DELAY = 0x9039,
  MEMADDR_PROT_OCD2_THRESHOLD = 0x903A,
  MEMADDR_PROT_OCD2_DELAY = 0x903B,
  MEMADDR_PROT_SCD_THRESHOLD = 0x903C,
  MEMADDR_PROT_SCD_DELAY = 0x903D,
  MEMADDR_PROT_CURR_LATCH_LIMIT = 0x903E,
  MEMADDR_PROT_CURR_RECOVERY_TIME = 0x903F,
  MEMADDR_PROT_OTC_THRESHOLD = 0x9040,
  MEMADDR_PROT_OTC_DELAY = 0x9041,
  MEMADDR_PROT_OTC_RECOVERY = 0x9042,
  MEMADDR_PROT_UTC_THRESHOLD = 0x9043,
  MEMADDR_PROT_UTC_DELAY = 0x9044,
  MEMADDR_PROT_UTC_RECOVERY = 0x9045,
  MEMADDR_PROT_OTD_THRESHOLD = 0x9046,
  MEMADDR_PROT_OTD_DELAY = 0x9047,
  MEMADDR_PROT_OTD_RECOVERY = 0x9048,
  MEMADDR_PROT_UTD_THRESHOLD = 0x9049,
  MEMADDR_PROT_UTD_DELAY = 0x904A,
  MEMADDR_PROT_UTD_RECOVERY = 0x904B,
  MEMADDR_PROT_OTINT_THRESHOLD = 0x904C,
  MEMADDR_PROT_OTINT_DELAY = 0x904D,
  MEMADDR_PROT_OTINT_RECOVERY = 0x904E,
  MEMADDR_PWR_SLEEP_CURRENT = 0x904F,
  MEMADDR_PWR_SLEEP_VOLTAGE_TIME = 0x9051,
  MEMADDR_PWR_WAKEUP_CURRENT = 0x9052,
  MEMADDR_PWR_SHUTDOWN_CELL_VOLTAGE = 0x9053,
  MEMADDR_PWR_SHUTDOWN_STACK_VOLTAGE = 0x9055,
  MEMADDR_PWR_SHUTDOWN_INT_TEMP = 0x9057,
  MEMADDR_PWR_SHUTDOWN_AUTO_TIME = 0x9058,
  MEMADDR_PWR_SEC_CONFIG = 0x9059,
  MEMADDR_PWR_SEC_FULLACCESS_KEY_1 = 0x905A,
  MEMADDR_PWR_SEC_FULLACCESS_KEY_2 = 0x905C
} BMS_I2C_DataMemAddress;


//sizes of data memory registers in bytes, index = lower byte of address
extern const uint8_t bms_i2c_data_reg_sizes[115];
/*const uint8_t bms_i2c_data_reg_sizes[115] = {
    2, 0, 2, 0, 1, 1, 2, 0, 2, 0, 2, 0, 2, 0, 2, 0, //0_
    2, 0, 2, 0, 1, 1, 1, 2, 0, 2, 0, 1, 2, 0, 1, 1, //1_
    1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 0, 1, 1, 1, 2, 0, //2_
    1, 1, 2, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //3_
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, //4_
    0, 1, 1, 2, 0, 2, 0, 1, 1, 1, 2, 0, 2, 0, 0, 0, //5_
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //6_
    0, 1, 1                                         //7_
};*/


//whether CRC calculation is currently enabled for I2C communication
extern bool bms_i2c_crc_active;

//whether error notifications are suppressed
extern bool bms_i2c_suppress_error_notifs;


HAL_StatusTypeDef BMS_I2C_DirectCommandRead(BMS_I2C_DirectCommand command, uint8_t* buffer, uint8_t length, uint8_t max_tries);
HAL_StatusTypeDef BMS_I2C_DirectCommandWrite(BMS_I2C_DirectCommand command, const uint8_t* data, uint8_t length, uint8_t max_tries);

HAL_StatusTypeDef BMS_I2C_SubcommandOnly(BMS_I2C_SubCommand command, uint8_t max_tries);
HAL_StatusTypeDef BMS_I2C_SubcommandRead(BMS_I2C_SubCommand command, uint8_t* buffer, uint8_t length, uint8_t max_tries);
HAL_StatusTypeDef BMS_I2C_SubcommandWrite(BMS_I2C_SubCommand command, const uint8_t* data, uint8_t length, uint8_t max_tries);

HAL_StatusTypeDef BMS_I2C_DataMemoryRead(BMS_I2C_DataMemAddress address, uint8_t* buffer, uint8_t length, uint8_t max_tries);
HAL_StatusTypeDef BMS_I2C_DataMemoryWrite(BMS_I2C_DataMemAddress address, const uint8_t* data, uint8_t length, uint8_t max_tries);


#endif /* INC_BMS_I2C_H_ */
