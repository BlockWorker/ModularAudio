/*
 * bms_i2c.c
 *
 *  Created on: Jan 12, 2025
 *      Author: Alex
 */

#include "bms_i2c.h"
#include <string.h>


//size of send/receive buffer, chosen for at least a full 2+32+2-byte read/write with CRC
#define BMS_I2C_BUFSIZE 128


//sizes of data memory registers in bytes, index = lower byte of address
// _0 _1 _2 _3 _4 _5 _6 _7 _8 _9 _A _B _C _D _E _F
const uint8_t bms_i2c_data_reg_sizes[115] = {
    2, 0, 2, 0, 1, 1, 2, 0, 2, 0, 2, 0, 2, 0, 2, 0, //0_
    2, 0, 2, 0, 1, 1, 1, 2, 0, 2, 0, 1, 2, 0, 1, 1, //1_
    1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 0, 1, 1, 1, 2, 0, //2_
    1, 1, 2, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, //3_
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, //4_
    0, 1, 1, 2, 0, 2, 0, 1, 1, 1, 2, 0, 2, 0, 0, 0, //5_
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, //6_
    0, 1, 1                                         //7_
};

//expected state of the config data memory, excluding calibration (0x9014 up to and including 0x905D)
//calibration values (0x9000-0x9013 and 0x9071-0x9072) are preprogrammed by TI and omitted from here, do potential adjustments separately based on factory calibration
const uint8_t bms_config_main_array[74] = {
    BMS_SET_POWER_CONFIG,
    BMS_SET_REGOUT_CONFIG,
    BMS_SET_I2C_ADDRESS,
    TwoBytes(BMS_SET_I2C_CONFIG),
    TwoBytes(BMS_SET_DA_CONFIG),
    BMS_SET_VCELL_MODE,
    TwoBytes(BMS_SET_DEFAULT_ALARM_MASK),
    BMS_SET_FET_OPTIONS,
    BMS_SET_CHGDET_TIME,
    BMS_SET_BAL_CONFIG,
    BMS_SET_BAL_MINTEMP_TS,
    BMS_SET_BAL_MAXTEMP_TS,
    BMS_SET_BAL_MAXTEMP_INT,
    BMS_PROT_ENABLE_A,
    BMS_PROT_ENABLE_B,
    BMS_PROT_DSGFET_A,
    BMS_PROT_CHGFET_A,
    BMS_PROT_BOTHFET_B,
    TwoBytes(BMS_PROT_BODYDIODE_THRESHOLD),
    BMS_PROT_COW_NORMAL_TIME,
    BMS_PROT_COW_SLEEP_CONFIG,
    BMS_PROT_HWD_TIMEOUT,
    TwoBytes(BMS_PROT_CUV_THRESHOLD),
    BMS_PROT_CUV_DELAY,
    BMS_PROT_CUV_HYSTERESIS,
    TwoBytes(BMS_PROT_COV_THRESHOLD),
    BMS_PROT_COV_DELAY,
    BMS_PROT_COV_HYSTERESIS,
    BMS_PROT_OCC_THRESHOLD,
    BMS_PROT_OCC_DELAY,
    BMS_PROT_OCD1_THRESHOLD,
    BMS_PROT_OCD1_DELAY,
    BMS_PROT_OCD2_THRESHOLD,
    BMS_PROT_OCD2_DELAY,
    BMS_PROT_SCD_THRESHOLD,
    BMS_PROT_SCD_DELAY,
    BMS_PROT_CURR_LATCH_LIMIT,
    BMS_PROT_CURR_RECOVERY_TIME,
    BMS_PROT_OTC_THRESHOLD,
    BMS_PROT_OTC_DELAY,
    BMS_PROT_OTC_RECOVERY,
    BMS_PROT_UTC_THRESHOLD,
    BMS_PROT_UTC_DELAY,
    BMS_PROT_UTC_RECOVERY,
    BMS_PROT_OTD_THRESHOLD,
    BMS_PROT_OTD_DELAY,
    BMS_PROT_OTD_RECOVERY,
    BMS_PROT_UTD_THRESHOLD,
    BMS_PROT_UTD_DELAY,
    BMS_PROT_UTD_RECOVERY,
    BMS_PROT_OTINT_THRESHOLD,
    BMS_PROT_OTINT_DELAY,
    BMS_PROT_OTINT_RECOVERY,
    TwoBytes(BMS_PWR_SLEEP_CURRENT),
    BMS_PWR_SLEEP_VOLTAGE_TIME,
    BMS_PWR_WAKEUP_CURRENT,
    TwoBytes(BMS_PWR_SHUTDOWN_CELL_VOLTAGE),
    TwoBytes(BMS_PWR_SHUTDOWN_STACK_VOLTAGE),
    BMS_PWR_SHUTDOWN_INT_TEMP,
    BMS_PWR_SHUTDOWN_AUTO_TIME,
    BMS_SEC_CONFIG,
    TwoBytes(BMS_SEC_FULLACCESS_KEY_1),
    TwoBytes(BMS_SEC_FULLACCESS_KEY_2)
};


uint8_t bms_i2c_crc_active = 0;

static uint8_t bms_i2c_buffer[BMS_I2C_BUFSIZE] = { 0 };


/*void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *hi2c) {

}

void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c) {

}*/

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c) {
  if (hi2c == &hi2c3) {
    SET_BIT(hi2c3.Instance->ICR, I2C_ICR_BERRCF); //workaround for STM32L07xxx device errata, 2.14.4 (spurious bus error detection in master mode) - shouldn't matter since interrupt disabled
  }
}


HAL_StatusTypeDef BMS_I2C_DirectCommandRead(BMS_I2C_DirectCommand command, uint8_t* buffer, uint8_t length, uint8_t max_tries) {
  if (length < 1 || length > (BMS_I2C_BUFSIZE / 2)) return HAL_ERROR;

  uint8_t data_size = bms_i2c_crc_active ? 2 * length : length; //size doubled if CRC is active
  HAL_StatusTypeDef res;

  //retry loop
  int try;
  for (try = 0; try < max_tries; try++) {
    res = HAL_I2C_Mem_Read(&hi2c3, BMS_I2C_ADDR, command, 1, bms_i2c_buffer, data_size, BMS_I2C_TIMEOUT);
    if (res != HAL_OK) continue;

    if (bms_i2c_crc_active) {
      //insert initial CRC data before first byte
      uint8_t initial_crc_buf[] = { BMS_I2C_ADDR, command, BMS_I2C_ADDR | 1 };
      HAL_CRC_Calculate(&hcrc, (uint32_t*)initial_crc_buf, sizeof(initial_crc_buf));

      //handle data bytes one by one
      int i;
      for (i = 0; i < length; i++) {
        //calculate crc from data byte and received crc, should be 0 if correct
        uint8_t crc_check = HAL_CRC_Accumulate(&hcrc, (uint32_t*)(bms_i2c_buffer + 2 * i), 2);
        if (crc_check != 0) {
          //wrong crc detected: abort
          DEBUG_PRINTF("ERROR: BMS I2C CRC check failed on read command 0x%02X offset %u (check value should be 0, is 0x%02X)\n", command, i, crc_check);
          res = HAL_ERROR;
          break;
        }

        //output data byte and reset crc calculation for next byte
        buffer[i] = bms_i2c_buffer[2 * i];
        __HAL_CRC_DR_RESET(&hcrc);
      }
    } else { //basic non-crc read
      memcpy(buffer, bms_i2c_buffer, length);
    }

    if (res == HAL_OK) return HAL_OK;
  }

  //out of tries: return whatever error status we had
  return res;
}

HAL_StatusTypeDef BMS_I2C_DirectCommandWrite(BMS_I2C_DirectCommand command, uint8_t* data, uint8_t length, uint8_t max_tries) {
  if (length < 1 || length > (BMS_I2C_BUFSIZE / 2)) return HAL_ERROR;

  if (bms_i2c_crc_active) {
    //insert initial CRC data before first byte
    uint8_t initial_crc_buf[] = { BMS_I2C_ADDR, command };
    HAL_CRC_Calculate(&hcrc, (uint32_t*)initial_crc_buf, sizeof(initial_crc_buf));

    //handle data bytes one by one
    int i;
    for (i = 0; i < length; i++) {
      //calculate crc from data byte
      uint8_t byte_crc = HAL_CRC_Accumulate(&hcrc, (uint32_t*)(data + i), 1);

      //put data byte and crc in buffer and reset crc calculation for next byte
      bms_i2c_buffer[2 * i] = data[i];
      bms_i2c_buffer[2 * i + 1] = byte_crc;
      __HAL_CRC_DR_RESET(&hcrc);
    }
  } else { //basic non-crc write
    memcpy(bms_i2c_buffer, data, length);
  }

  uint8_t data_size = bms_i2c_crc_active ? 2 * length : length; //size doubled if CRC is active
  HAL_StatusTypeDef res;

  //retry loop
  int try;
  for (try = 0; try < max_tries; try++) {
    res = HAL_I2C_Mem_Write(&hi2c3, BMS_I2C_ADDR, command, 1, bms_i2c_buffer, data_size, BMS_I2C_TIMEOUT);
    if (res == HAL_OK) return HAL_OK;
  }

  //out of tries: return whatever error status we had
  return res;
}

HAL_StatusTypeDef BMS_I2C_SubcommandOnly(BMS_I2C_SubCommand command, uint8_t max_tries) {
  uint8_t command_bytes[] = { TwoBytes((uint16_t)command) }; //separate command into bytes

  //write to subcommand address - process is equivalent to direct command write
  return BMS_I2C_DirectCommandWrite((BMS_I2C_DirectCommand)BMS_I2C_REG_SUBCOMMAND, command_bytes, 2, max_tries);
}

HAL_StatusTypeDef BMS_I2C_SubcommandRead(BMS_I2C_SubCommand command, uint8_t* buffer, uint8_t length, uint8_t max_tries) {
  if (length < 1) return HAL_ERROR;

  uint8_t read_buffer[36] = { 0 };

  HAL_StatusTypeDef res;

  //retry loop
  int try;
  for (try = 0; try < max_tries; try++) {
    //write command itself
    res = BMS_I2C_SubcommandOnly(command, 1);
    if (res != HAL_OK) continue;

    //read chunks of 32 bytes
    int i, j;
    for (i = 0; i < length; i += 32) {
      //read back subcommand, data, checksum, and length (which also causes auto-increment)
      res = BMS_I2C_DirectCommandRead((BMS_I2C_DirectCommand)BMS_I2C_REG_SUBCOMMAND, read_buffer, 36, 1);
      if (res != HAL_OK) break;

      //check that the command is correct
      uint16_t command_readback = ((uint16_t*)read_buffer)[0];
      if (command_readback != (uint16_t)command + i) {
        DEBUG_PRINTF("ERROR: BMS I2C subcommand 0x%04X read failed: unexpected command readback 0x%04X at offset %u\n", command, command_readback, i);
        res = HAL_ERROR;
        break;
      }

      //check that we got at least the expected amount of data
      uint8_t chunk_length = read_buffer[35];
      if (chunk_length < 36 && (length - i + 4) > chunk_length) {
        DEBUG_PRINTF("ERROR: BMS I2C subcommand 0x%04X read failed: need length of at least %u + 4 bytes at offset %u, but only got %u bytes\n", command, length - i, i, chunk_length);
        res = HAL_ERROR;
        break;
      }

      //calculate checksum and compare with received checksum
      uint8_t checksum = 0;
      for (j = 0; j < chunk_length - 2; j++) {
        checksum += read_buffer[j];
      }
      checksum = ~checksum;
      if (checksum != read_buffer[34]) {
        DEBUG_PRINTF("ERROR: BMS I2C subcommand 0x%04X read failed: chunk checksum failed at offset %u: calculated 0x%02X, received 0x%02X\n", command, i, checksum, read_buffer[34]);
        res = HAL_ERROR;
        break;
      }

      //all seems okay with this chunk - copy to the output buffer and continue
      memcpy(buffer + i, read_buffer + 2, MIN(chunk_length - 4, length - i));
    }

    if (res == HAL_OK) return HAL_OK;
  }

  //out of tries: return whatever error status we had
  return res;
}

HAL_StatusTypeDef BMS_I2C_SubcommandWrite(BMS_I2C_SubCommand command, uint8_t* data, uint8_t length, uint8_t max_tries) {
  if (length < 1 || length > 32) return HAL_ERROR;

  //calculate checksum from command and data, prepare final two bytes
  uint8_t checksum = (uint8_t)((uint16_t)command & 0xFF) + (uint8_t)((uint16_t)command >> 8);
  int i;
  for (i = 0; i < length; i++) {
    checksum += data[i];
  }
  checksum = ~checksum;
  uint8_t final_bytes[2] = { checksum, length + 4 };

  HAL_StatusTypeDef res;

  //retry loop
  int try;
  for (try = 0; try < max_tries; try++) {
    //write command itself
    res = BMS_I2C_SubcommandOnly(command, 1);
    if (res != HAL_OK) continue;

    //write data bytes
    res = BMS_I2C_DirectCommandWrite((BMS_I2C_DirectCommand)BMS_I2C_REG_DATA, data, length, 1);
    if (res != HAL_OK) continue;

    //write final bytes (checksum and length)
    res = BMS_I2C_DirectCommandWrite((BMS_I2C_DirectCommand)BMS_I2C_REG_CHECKSUM, final_bytes, 2, 1);
    if (res == HAL_OK) return HAL_OK;
  }

  //out of tries: return whatever error status we had
  return res;
}

HAL_StatusTypeDef BMS_I2C_DataMemoryRead(BMS_I2C_DataMemAddress address, uint8_t* buffer, uint8_t length, uint8_t max_tries) {
  //data memory read operation is identical to subcommand read
  return BMS_I2C_SubcommandRead((BMS_I2C_SubCommand)address, buffer, length, max_tries);
}

HAL_StatusTypeDef BMS_I2C_DataMemoryWrite(BMS_I2C_DataMemAddress address, uint8_t* data, uint8_t length, uint8_t max_tries) {
  //data memory write operation is identical to subcommand write
  return BMS_I2C_SubcommandWrite((BMS_I2C_SubCommand)address, data, length, max_tries);
}
