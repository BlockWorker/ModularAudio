/*
 * dac_control.c
 *
 *  Created on: Sep 22, 2024
 *      Author: Alex
 */

#include "dac_control.h"
#include "dac_spi.h"


DAC_Status dac_status;


#ifdef DEBUG
static void PrintReg8(DAC_SPI_Register reg) {
  uint8_t i8 = 0;
  HAL_StatusTypeDef res = DAC_SPI_Read8(reg, &i8);
  if (res != HAL_OK) printf("0x%02X: Read error %u\n", (uint8_t)reg, (uint8_t)res);
  else printf("0x%02X: 0x%02X\n", (uint8_t)reg, i8);
}

static void PrintReg16(DAC_SPI_Register reg) {
  uint16_t i16 = 0;
  HAL_StatusTypeDef res = DAC_SPI_Read16(reg, &i16);
  if (res != HAL_OK) printf("0x%02X: Read error %u\n", (uint8_t)reg, (uint8_t)res);
  else printf("0x%02X: 0x%04X\n", (uint8_t)reg, i16);
}

static void PrintReg24(DAC_SPI_Register reg) {
  uint32_t i32 = 0;
  HAL_StatusTypeDef res = DAC_SPI_Read24(reg, &i32);
  if (res != HAL_OK) printf("0x%02X: Read error %u\n", (uint8_t)reg, (uint8_t)res);
  else printf("0x%02X: 0x%06lX\n", (uint8_t)reg, i32);
}

static void PrintAllRegisters() {
  PrintReg8(REG_SYSTEM_CONFIG);
  PrintReg8(REG_SYS_MODE_CONFIG);
  PrintReg8(REG_DAC_CLOCK_CONFIG);
  PrintReg8(REG_CLOCK_CONFIG);
  PrintReg8(REG_CLK_GEAR_SELECT);
  PrintReg16(REG_INTERRUPT_MASKP);
  PrintReg16(REG_INTERRUPT_MASKN);
  PrintReg16(REG_INTERRUPT_CLEAR);
  PrintReg8(REG_DPLL_BW);
  PrintReg8(REG_DATA_PATH_CONFIG);
  PrintReg8(REG_PCM_4X_GAIN);
  PrintReg8(REG_GPIO12_CONFIG);
  PrintReg8(REG_GPIO34_CONFIG);
  PrintReg8(REG_GPIO56_CONFIG);
  PrintReg8(REG_GPIO78_CONFIG);
  PrintReg8(REG_GPIO_OUTPUT_ENABLE);
  PrintReg8(REG_GPIO_INPUT);
  PrintReg8(REG_GPIO_WK_EN);
  PrintReg8(REG_INVERT_GPIO);
  PrintReg8(REG_GPIO_READ_EN);
  PrintReg16(REG_GPIO_OUTPUT_LOGIC);
  PrintReg8(REG_PWM1_COUNT);
  PrintReg16(REG_PWM1_FREQUENCY);
  PrintReg8(REG_PWM2_COUNT);
  PrintReg16(REG_PWM2_FREQUENCY);
  PrintReg8(REG_PWM3_COUNT);
  PrintReg16(REG_PWM3_FREQUENCY);
  PrintReg8(REG_INPUT_SELECTION);
  PrintReg8(REG_MASTER_ENCODER_CONFIG);
  PrintReg8(REG_TDM_CONFIG);
  PrintReg8(REG_TDM_CONFIG1);
  PrintReg8(REG_TDM_CONFIG2);
  PrintReg8(REG_BCKWS_MONITOR_CONFIG);
  PrintReg8(REG_CH1_SLOT_CONFIG);
  PrintReg8(REG_CH2_SLOT_CONFIG);
  PrintReg8(REG_VOLUME_CH1);
  PrintReg8(REG_VOLUME_CH2);
  PrintReg8(REG_DAC_VOL_UP_RATE);
  PrintReg8(REG_DAC_VOL_DOWN_RATE);
  PrintReg8(REG_DAC_VOL_DOWN_RATE_FAST);
  PrintReg8(REG_DAC_MUTE);
  PrintReg8(REG_DAC_INVERT);
  PrintReg8(REG_FILTER_SHAPE);
  PrintReg8(REG_IIR_BANDWIDTH_SPDIF_SEL);
  PrintReg8(REG_DAC_PATH_CONFIG);
  PrintReg16(REG_THD_C2_CH1);
  PrintReg16(REG_THD_C2_CH2);
  PrintReg16(REG_THD_C3_CH1);
  PrintReg16(REG_THD_C3_CH2);
  PrintReg8(REG_AUTOMUTE_ENABLE);
  PrintReg16(REG_AUTOMUTE_TIME);
  PrintReg16(REG_AUTOMUTE_LEVEL);
  PrintReg16(REG_AUTOMUTE_OFF_LEVEL);
  PrintReg8(REG_SOFT_RAMP_CONFIG);
  PrintReg8(REG_PROGRAM_RAM_CONTROL);
  PrintReg8(REG_SPDIF_READ_CONTROL);
  PrintReg8(REG_PROGRAM_RAM_ADDRESS);
  PrintReg24(REG_PROGRAM_RAM_DATA);
  PrintReg8(REG_CHIP_ID_READ);
  PrintReg16(REG_INTERRUPT_STATES);
  PrintReg16(REG_INTERRUPT_SOURCES);
  PrintReg8(REG_RATIO_VALID_READ);
  PrintReg8(REG_GPIO_READ);
  PrintReg8(REG_VOL_MIN_READ);
  PrintReg8(REG_AUTOMUTE_READ);
  PrintReg8(REG_SOFT_RAMP_UP_READ);
  PrintReg8(REG_SOFT_RAMP_DOWN_READ);
  PrintReg8(REG_INPUT_STREAM_READBACK);
  PrintReg24(REG_PROG_COEFF_OUT_READ);
  PrintReg8(REG_SPDIF_DATA_READ);
}
#endif


//write value to given register and read back true value - 8 bit or 16 bit
static HAL_StatusTypeDef _DAC_WriteAndCheckRegister(DAC_SPI_Register reg, bool _16bit, uint16_t value, uint16_t* read_value) {
  //perform write
  if (_16bit) {
    ReturnOnError(DAC_SPI_Write16(reg, value));
  } else {
    ReturnOnError(DAC_SPI_Write8(reg, (uint8_t)value));
  }

  //read back
  if (_16bit) {
    ReturnOnError(DAC_SPI_Read16(reg, read_value));
  } else {
    *read_value = 0;
    ReturnOnError(DAC_SPI_Read8(reg, (uint8_t*)read_value));
  }

  //check for correct readback
  if (*read_value != value) {
    if (_16bit) {
      //incorrect readback in 16 bit mode: error
      return HAL_ERROR;
    } else {
      //incorrect readback in 8 bit mode: check for correctness of relevant byte
      if (*(uint8_t*)read_value != (uint8_t)value) {
        return HAL_ERROR;
      }
    }
  }

  return HAL_OK;
}


//Check that the DAC chip ID is correct, confirming that the chip functions and can communicate
HAL_StatusTypeDef DAC_CheckChipID() {
  uint8_t chip_id = 0;

  ReturnOnError(DAC_SPI_Read8(REG_CHIP_ID_READ, &chip_id));

  DEBUG_PRINTF("Chip ID: 0x%02X\n", chip_id);

  if (chip_id != DAC_EXPECTED_CHIP_ID) return HAL_ERROR;

  return HAL_OK;
}

HAL_StatusTypeDef DAC_WriteSysConfig() {
  uint16_t readback;
  uint16_t value = dac_status.enabled ? 0x02 : 0x00;

  HAL_StatusTypeDef result = _DAC_WriteAndCheckRegister(REG_SYSTEM_CONFIG, false, value, &readback);

  dac_status.enabled = ((readback & 0x02) != 0);

  return result;
}

HAL_StatusTypeDef DAC_WriteSysModeConfig() {
  uint16_t readback;
  uint16_t value = DAC_SYS_MODE_CFG_BASE_VALUE | (dac_status.sync ? 0x40 : 0x00);

  HAL_StatusTypeDef result = _DAC_WriteAndCheckRegister(REG_SYS_MODE_CONFIG, false, value, &readback);

  dac_status.sync = ((readback & 0x40) != 0);

  return result;
}

HAL_StatusTypeDef DAC_WriteDACClockConfig() {
  uint16_t readback;
  uint16_t value = dac_status.clk_config;

  HAL_StatusTypeDef result = _DAC_WriteAndCheckRegister(REG_DAC_CLOCK_CONFIG, false, value, &readback);

  dac_status.clk_config = (uint8_t)readback;

  return result;
}

HAL_StatusTypeDef DAC_WriteMasterClockConfig() {
  uint16_t readback;
  uint16_t value = dac_status.master_div;

  HAL_StatusTypeDef result = _DAC_WriteAndCheckRegister(REG_CLOCK_CONFIG, false, value, &readback);

  dac_status.master_div = (uint8_t)readback;

  return result;
}

HAL_StatusTypeDef DAC_Write4XGains() {
  uint16_t readback;
  uint16_t value = (dac_status.en4xgain_ch1 ? 0x01 : 0x00) | (dac_status.en4xgain_ch2 ? 0x02 : 0x00);

  HAL_StatusTypeDef result = _DAC_WriteAndCheckRegister(REG_PCM_4X_GAIN, false, value, &readback);

  dac_status.en4xgain_ch1 = ((readback & 0x01) != 0);
  dac_status.en4xgain_ch2 = ((readback & 0x02) != 0);

  return result;
}

HAL_StatusTypeDef DAC_WriteInputSelection() {
  uint16_t readback;
  uint16_t value = dac_status.master ? 0x10 : 0x00;

  HAL_StatusTypeDef result = _DAC_WriteAndCheckRegister(REG_INPUT_SELECTION, false, value, &readback);

  dac_status.master = ((readback & 0x10) != 0);

  return result;
}

HAL_StatusTypeDef DAC_WriteTDMSlotNum() {
  uint16_t readback;
  uint16_t value = dac_status.tdm_slot_num & 0x1F;

  HAL_StatusTypeDef result = _DAC_WriteAndCheckRegister(REG_TDM_CONFIG, false, value, &readback);

  dac_status.tdm_slot_num = (uint8_t)readback;

  return result;
}

HAL_StatusTypeDef DAC_WriteChannelTDMSlots() {
  uint16_t readback;
  uint16_t value = dac_status.tdm_slot_ch1 & 0x1F;

  HAL_StatusTypeDef result = _DAC_WriteAndCheckRegister(REG_CH1_SLOT_CONFIG, false, value, &readback);

  dac_status.tdm_slot_ch1 = (uint8_t)readback;

  if (result != HAL_OK) {
    return result;
  }

  value = dac_status.tdm_slot_ch2 & 0x1F;

  result = _DAC_WriteAndCheckRegister(REG_CH2_SLOT_CONFIG, false, value, &readback);

  dac_status.tdm_slot_ch2 = (uint8_t)readback;

  return result;
}

HAL_StatusTypeDef DAC_WriteChannelVolumes() {
  //set volume hold
  ReturnOnError(DAC_SPI_WriteConfirm8(REG_IIR_BANDWIDTH_SPDIF_SEL, DAC_IIRBW_SPDIFSEL_BASE_VALUE | 0x08));

  uint16_t readback1, readback2;
  uint16_t value = dac_status.volume_ch1 + DAC_VOL_CAL_CH1;
  if (value > 0xFF) {
    value = 0xFF;
  }

  HAL_StatusTypeDef result = _DAC_WriteAndCheckRegister(REG_VOLUME_CH1, false, value, &readback1);

  if (result != HAL_OK) {
    return result;
  }

  value = dac_status.volume_ch2 + DAC_VOL_CAL_CH2;
  if (value > 0xFF) {
    value = 0xFF;
  }

  result = _DAC_WriteAndCheckRegister(REG_VOLUME_CH2, false, value, &readback2);

  if (result != HAL_OK) {
    return result;
  }

  //remove volume hold
  result = DAC_SPI_WriteConfirm8(REG_IIR_BANDWIDTH_SPDIF_SEL, DAC_IIRBW_SPDIFSEL_BASE_VALUE);

  if (result == HAL_OK) {
    //store new volumes if all succeeded
    dac_status.volume_ch1 = (uint8_t)readback1 - DAC_VOL_CAL_CH1;
    dac_status.volume_ch2 = (uint8_t)readback2 - DAC_VOL_CAL_CH2;
  }

  return result;
}

HAL_StatusTypeDef DAC_WriteChannelMutes() {
  uint16_t readback;
  uint16_t value = (dac_status.manual_mute_ch1 ? 0x01 : 0x00) | (dac_status.manual_mute_ch2 ? 0x02 : 0x00);

  HAL_StatusTypeDef result = _DAC_WriteAndCheckRegister(REG_DAC_MUTE, false, value, &readback);

  dac_status.manual_mute_ch1 = ((readback & 0x01) != 0);
  dac_status.manual_mute_ch2 = ((readback & 0x02) != 0);

  return result;
}

HAL_StatusTypeDef DAC_WriteChannelInverts() {
  uint16_t readback;
  uint16_t value = (dac_status.invert_ch1 ? 0x01 : 0x00) | (dac_status.invert_ch2 ? 0x02 : 0x00);

  HAL_StatusTypeDef result = _DAC_WriteAndCheckRegister(REG_DAC_INVERT, false, value, &readback);

  dac_status.invert_ch1 = ((readback & 0x01) != 0);
  dac_status.invert_ch2 = ((readback & 0x02) != 0);

  return result;
}

HAL_StatusTypeDef DAC_WriteFilterShape() {
  uint16_t readback;
  uint16_t value = DAC_FILTER_SHAPE_BASE_VALUE | (dac_status.filter_shape & 0x07);

  HAL_StatusTypeDef result = _DAC_WriteAndCheckRegister(REG_FILTER_SHAPE, false, value, &readback);

  dac_status.filter_shape = (uint8_t)readback & 0x07;

  return result;
}

HAL_StatusTypeDef DAC_WriteTHDC2() {
  uint16_t readback;
  uint16_t value = (uint16_t)dac_status.thd_c2_ch1;

  HAL_StatusTypeDef result = _DAC_WriteAndCheckRegister(REG_THD_C2_CH1, true, value, &readback);

  dac_status.thd_c2_ch1 = (int16_t)readback;

  if (result != HAL_OK) {
    return result;
  }

  value = (uint16_t)dac_status.thd_c2_ch2;

  HAL_StatusTypeDef result = _DAC_WriteAndCheckRegister(REG_THD_C2_CH2, true, value, &readback);

  dac_status.thd_c2_ch2 = (int16_t)readback;

  return result;
}

HAL_StatusTypeDef DAC_WriteTHDC3() {
  uint16_t readback;
  uint16_t value = (uint16_t)dac_status.thd_c3_ch1;

  HAL_StatusTypeDef result = _DAC_WriteAndCheckRegister(REG_THD_C3_CH1, true, value, &readback);

  dac_status.thd_c3_ch1 = (int16_t)readback;

  if (result != HAL_OK) {
    return result;
  }

  value = (uint16_t)dac_status.thd_c3_ch2;

  HAL_StatusTypeDef result = _DAC_WriteAndCheckRegister(REG_THD_C3_CH2, true, value, &readback);

  dac_status.thd_c3_ch2 = (int16_t)readback;

  return result;
}

HAL_StatusTypeDef DAC_WriteChannelAutomuteEnables() {
  uint16_t readback;
  uint16_t value = DAC_AUTOMUTE_EN_BASE_VALUE | (dac_status.automute_enabled_ch1 ? 0x01 : 0x00) | (dac_status.automute_enabled_ch2 ? 0x02 : 0x00);

  HAL_StatusTypeDef result = _DAC_WriteAndCheckRegister(REG_AUTOMUTE_ENABLE, false, value, &readback);

  dac_status.automute_enabled_ch1 = ((readback & 0x01) != 0);
  dac_status.automute_enabled_ch2 = ((readback & 0x02) != 0);

  return result;
}

HAL_StatusTypeDef DAC_WriteAutomuteTimeAndRamp() {
  uint16_t readback;
  uint16_t value = DAC_AUTOMUTE_TIME_BASE_VALUE | (dac_status.mute_gnd_ramp ? 0x0800 : 0x0000);

  HAL_StatusTypeDef result = _DAC_WriteAndCheckRegister(REG_AUTOMUTE_TIME, true, value, &readback);

  dac_status.mute_gnd_ramp = ((readback & 0x0800) != 0);

  return result;
}


HAL_StatusTypeDef DAC_Init() {

  ReturnOnError(DAC_SPI_Write8(REG_SYSTEM_CONFIG, 0x80)); //perform soft reset

  HAL_Delay(10);

  ReturnOnError(DAC_SPI_Write8(REG_SYS_MODE_CONFIG, DAC_SYS_MODE_CFG_BASE_VALUE)); //reset sys mode config to the default base value

  ReturnOnError(DAC_SPI_WriteConfirm8(REG_DAC_CLOCK_CONFIG, 0x80)); //keep auto FS detection on, init CLK_IDAC = SYS_CLK
  ReturnOnError(DAC_SPI_WriteConfirm8(REG_CLOCK_CONFIG, 0x07)); //set master BCK divider to default value
  ReturnOnError(DAC_SPI_WriteConfirm8(REG_CLK_GEAR_SELECT, 0x00)); //keep clock gear as SYS_CLK = MCLK - should be the default already

  ReturnOnError(DAC_SPI_WriteConfirm16(REG_INTERRUPT_MASKP, 0x00BC)); //enable interrupts for monitor fail, ramps, and automute changes - each in both polarities
  ReturnOnError(DAC_SPI_WriteConfirm16(REG_INTERRUPT_MASKN, 0x00BC));

  ReturnOnError(DAC_SPI_Write8(REG_DPLL_BW, 0x40)); //default DPLL bandwidth

  ReturnOnError(DAC_SPI_WriteConfirm8(REG_DATA_PATH_CONFIG, 0x40)); //mono mode off, keep calibration resistor on - no reason not to - should be the default already
  ReturnOnError(DAC_SPI_WriteConfirm8(REG_PCM_4X_GAIN, 0x00)); //no 4x gains - should be the default already

  ReturnOnError(DAC_SPI_WriteConfirm8(REG_GPIO12_CONFIG, 0x7D)); //configure GPIO1 as automute status and GPIO2 as lock status - should be the default already
  ReturnOnError(DAC_SPI_WriteConfirm8(REG_GPIO34_CONFIG, 0x4E)); //configure GPIO3 as soft ramp status and GPIO4 as interrupt
  ReturnOnError(DAC_SPI_WriteConfirm8(REG_GPIO_OUTPUT_ENABLE, 0x0F)); //configure GPIO1-4 as outputs
  ReturnOnError(DAC_SPI_WriteConfirm8(REG_GPIO_INPUT, 0x00)); //no GPIO inputs - should be the default already
  ReturnOnError(DAC_SPI_WriteConfirm8(REG_INVERT_GPIO, 0x00)); //no GPIOs inverted - should be the default already
  ReturnOnError(DAC_SPI_WriteConfirm16(REG_GPIO_OUTPUT_LOGIC, 0x0007)); //GPIO output logic ANDed - should be the default already

  ReturnOnError(DAC_SPI_WriteConfirm8(REG_INPUT_SELECTION, 0x00)); //fixed PCM input in slave mode
  ReturnOnError(DAC_SPI_WriteConfirm8(REG_MASTER_ENCODER_CONFIG, 0x00)); //master encoder in default config but without any inverts

  ReturnOnError(DAC_SPI_WriteConfirm8(REG_TDM_CONFIG, 0x01)); //init to 2 channels - should be the default already
  ReturnOnError(DAC_SPI_WriteConfirm8(REG_TDM_CONFIG1, 0x00)); //reset TDM config to the defaults
  ReturnOnError(DAC_SPI_WriteConfirm8(REG_TDM_CONFIG2, 0x80));

  ReturnOnError(DAC_SPI_WriteConfirm8(REG_BCKWS_MONITOR_CONFIG, 0x30)); //enable BCK/WS monitor and all related features - should be the default already

  ReturnOnError(DAC_SPI_WriteConfirm8(REG_CH1_SLOT_CONFIG, 0x00)); //channel 1 on slot 0 - should be the default already
  ReturnOnError(DAC_SPI_WriteConfirm8(REG_CH2_SLOT_CONFIG, 0x01)); //channel 2 on slot 1

  ReturnOnError(DAC_SPI_WriteConfirm8(REG_VOLUME_CH1, DAC_VOL_INIT_CH1 + DAC_VOL_CAL_CH1)); //write initial volume to configured value with calibration offset
  ReturnOnError(DAC_SPI_WriteConfirm8(REG_VOLUME_CH2, DAC_VOL_INIT_CH2 + DAC_VOL_CAL_CH2));

  ReturnOnError(DAC_SPI_WriteConfirm8(REG_DAC_VOL_UP_RATE, 0x04)); //default volume increase and decrease rates
  ReturnOnError(DAC_SPI_WriteConfirm8(REG_DAC_VOL_DOWN_RATE, 0x04));
  ReturnOnError(DAC_SPI_WriteConfirm8(REG_DAC_VOL_DOWN_RATE_FAST, 0xFF));

  ReturnOnError(DAC_SPI_WriteConfirm8(REG_DAC_MUTE, 0x00)); //no muted channels - should be the default already
  ReturnOnError(DAC_SPI_WriteConfirm8(REG_DAC_INVERT, 0x00)); //no inverted channels - should be the default already

  ReturnOnError(DAC_SPI_WriteConfirm8(REG_FILTER_SHAPE, DAC_FILTER_SHAPE_BASE_VALUE)); //default filter shape
  ReturnOnError(DAC_SPI_WriteConfirm8(REG_IIR_BANDWIDTH_SPDIF_SEL, DAC_IIRBW_SPDIFSEL_BASE_VALUE)); //default IIR bandwidth, SPDIF selection, and no volume hold
  ReturnOnError(DAC_SPI_WriteConfirm8(REG_DAC_PATH_CONFIG, 0x00)); //no filter bypass - should be the default already

  ReturnOnError(DAC_SPI_WriteConfirm16(REG_THD_C2_CH1, 0x0000)); //reset THD correction coefficients to default (none)
  ReturnOnError(DAC_SPI_WriteConfirm16(REG_THD_C2_CH2, 0x0000));
  ReturnOnError(DAC_SPI_WriteConfirm16(REG_THD_C3_CH1, 0x0000));
  ReturnOnError(DAC_SPI_WriteConfirm16(REG_THD_C3_CH2, 0x0000));

  ReturnOnError(DAC_SPI_WriteConfirm8(REG_AUTOMUTE_ENABLE, DAC_AUTOMUTE_EN_BASE_VALUE | 0x03)); //enable both automutes - should be the default already
  ReturnOnError(DAC_SPI_WriteConfirm16(REG_AUTOMUTE_TIME, DAC_AUTOMUTE_TIME_BASE_VALUE | 0x0800)); //enable mute ramp to ground with default time - should be the default already
  ReturnOnError(DAC_SPI_WriteConfirm16(REG_AUTOMUTE_LEVEL, 0x0008)); //default automute on and off levels
  ReturnOnError(DAC_SPI_WriteConfirm16(REG_AUTOMUTE_OFF_LEVEL, 0x000A)); //default automute on and off levels

  ReturnOnError(DAC_SPI_WriteConfirm8(REG_SOFT_RAMP_CONFIG, 0x03)); //default soft ramp time

  ReturnOnError(DAC_SPI_WriteConfirm8(REG_PROGRAM_RAM_CONTROL, 0x00)); //no program RAM functions - should be the default already

  HAL_Delay(10);

  ReturnOnError(DAC_SPI_WriteConfirm8(REG_SYSTEM_CONFIG, 0x02)); //enable DAC analog section

  dac_status.enabled = true;
  dac_status.sync = false;
  dac_status.master = false;
  dac_status.mute_gnd_ramp = true;

  dac_status.automute_ch1 = false;
  dac_status.automute_ch2 = false;
  dac_status.full_ramp_ch1 = true;
  dac_status.full_ramp_ch2 = true;
  dac_status.monitor_error = false;

  dac_status.manual_mute_ch1 = false;
  dac_status.manual_mute_ch2 = false;
  dac_status.automute_enabled_ch1 = true;
  dac_status.automute_enabled_ch2 = true;
  dac_status.en4xgain_ch1 = false;
  dac_status.en4xgain_ch2 = false;
  dac_status.invert_ch1 = false;
  dac_status.invert_ch2 = false;

  dac_status.volume_ch1 = DAC_VOL_INIT_CH1;
  dac_status.volume_ch2 = DAC_VOL_INIT_CH2;
  dac_status.clk_config = 0x80;
  dac_status.master_div = 0x07;
  dac_status.tdm_slot_num = 0x01;
  dac_status.tdm_slot_ch1 = 0x00;
  dac_status.tdm_slot_ch2 = 0x01;

  dac_status.filter_shape = 0x00;
  dac_status.thd_c2_ch1 = 0;
  dac_status.thd_c2_ch2 = 0;
  dac_status.thd_c3_ch1 = 0;
  dac_status.thd_c3_ch2 = 0;

#ifdef DEBUG
  PrintAllRegisters();
#endif

  return HAL_OK;
}

void DAC_LoopUpdate() {

}
