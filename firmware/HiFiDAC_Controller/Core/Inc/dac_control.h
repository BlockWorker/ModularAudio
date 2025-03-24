/*
 * dac_control.h
 *
 *  Created on: Sep 22, 2024
 *      Author: Alex
 */

#ifndef INC_DAC_CONTROL_H_
#define INC_DAC_CONTROL_H_

#include "main.h"

//expected value of chip ID register
#define DAC_EXPECTED_CHIP_ID 0x63


//base value for the sys mode config register - assuming sync = 0
#define DAC_SYS_MODE_CFG_BASE_VALUE 0xB1
//base value for the filter shape register - assuming filter shape = 0
#define DAC_FILTER_SHAPE_BASE_VALUE 0xB8
//base value for the IIRBW/SPDIFSEL register - assuming volume hold = 0
#define DAC_IIRBW_SPDIFSEL_BASE_VALUE 0x04
//base value for the automute enable register - assuming both automutes disabled
#define DAC_AUTOMUTE_EN_BASE_VALUE 0xFC
//base value for the automute time register - assuming mute ramp to ground = 0
#define DAC_AUTOMUTE_TIME_BASE_VALUE 0x000F

//calibrated volume offsets (reduction) in 0.5dB steps - TODO: perform tests with pcb changes
#define DAC_VOL_CAL_CH1 0
#define DAC_VOL_CAL_CH2 0
//initial volume setting (reduction) after reset/init in 0.5dB steps - here: set to -20dB
#define DAC_VOL_INIT_CH1 40
#define DAC_VOL_INIT_CH2 40


typedef struct {
  bool enabled; //whether the DAC is enabled overall

  bool sync; //whether synchronous mode is enabled (otherwise asynchronous mode)

  bool master; //whether master mode is enabled (otherwise slave mode)

  bool mute_gnd_ramp; //whether outputs ramp to ground when muted


  bool automute_ch1; //whether channels are automuted
  bool automute_ch2;

  bool full_ramp_ch1; //whether channels have completed their ramps
  bool full_ramp_ch2;

  bool monitor_error; //whether the BCK/WS monitor has detected any errors


  bool manual_mute_ch1; //whether channels are manually muted
  bool manual_mute_ch2;

  bool automute_enabled_ch1; //whether channels have automute enabled
  bool automute_enabled_ch2;

  bool en4xgain_ch1; //whether channels have 4x gain enabled
  bool en4xgain_ch2;

  bool invert_ch1; //whether channels are inverted
  bool invert_ch2;


  uint8_t volume_ch1; //channel volumes
  uint8_t volume_ch2;

  uint8_t clk_config; //DAC clock config

  uint8_t master_div; //master mode BCK divider
  uint8_t tdm_slot_num; //TDM slot number

  uint8_t tdm_slot_ch1; //channel TDM slots
  uint8_t tdm_slot_ch2;


  uint8_t filter_shape; //digital filter shape

  int16_t thd_c2_ch1; //THD correction coefficients
  int16_t thd_c2_ch2;
  int16_t thd_c3_ch1;
  int16_t thd_c3_ch2;
} DAC_Status;


extern DAC_Status dac_status;


//Check that the DAC chip ID is correct, confirming that the chip functions and can communicate
HAL_StatusTypeDef DAC_CheckChipID();

HAL_StatusTypeDef DAC_WriteSysConfig();
HAL_StatusTypeDef DAC_WriteSysModeConfig();
HAL_StatusTypeDef DAC_WriteDACClockConfig();
HAL_StatusTypeDef DAC_WriteMasterClockConfig();
HAL_StatusTypeDef DAC_Write4XGains();
HAL_StatusTypeDef DAC_WriteInputSelection();
HAL_StatusTypeDef DAC_WriteTDMSlotNum();
HAL_StatusTypeDef DAC_WriteChannelTDMSlots();
HAL_StatusTypeDef DAC_WriteChannelVolumes();
HAL_StatusTypeDef DAC_WriteChannelMutes();
HAL_StatusTypeDef DAC_WriteChannelInverts();
HAL_StatusTypeDef DAC_WriteFilterShape();
HAL_StatusTypeDef DAC_WriteTHDC2();
HAL_StatusTypeDef DAC_WriteTHDC3();
HAL_StatusTypeDef DAC_WriteChannelAutomuteEnables();
HAL_StatusTypeDef DAC_WriteAutomuteTimeAndRamp();

HAL_StatusTypeDef DAC_Init();
void DAC_LoopUpdate();


#endif /* INC_DAC_CONTROL_H_ */
