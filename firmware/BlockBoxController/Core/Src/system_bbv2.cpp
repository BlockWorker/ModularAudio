/*
 * system_bbv2.cpp
 *
 *  Created on: May 10, 2025
 *      Author: Alex
 */

#include "system.h"
#include "retarget.h"


/***************************************************/
/*        BlockBox v2 system configuration         */
/***************************************************/

#define BBV2_I2C_MAIN_HANDLE hi2c5
#define BBV2_I2C_MAIN_FORCE_RESET() __HAL_RCC_I2C5_FORCE_RESET()
#define BBV2_I2C_MAIN_RELEASE_RESET() __HAL_RCC_I2C5_RELEASE_RESET()

#define BBV2_DAP_INT_PORT I2C5_INT3_N_GPIO_Port
#define BBV2_DAP_INT_PIN I2C5_INT3_N_Pin
#define BBV2_DAP_I2C_ADDR 0x4A

#define BBV2_HIFIDAC_INT_PORT I2C5_INT2_N_GPIO_Port
#define BBV2_HIFIDAC_INT_PIN I2C5_INT2_N_Pin
#define BBV2_HIFIDAC_I2C_ADDR 0x1D

#define BBV2_BTRX_UART_HANDLE huart1


static void _BlockBoxV2_I2C_Main_HardwareReset() {
  BBV2_I2C_MAIN_FORCE_RESET();
  int i;
  for (i = 0; i < 10; i++) __NOP();
  BBV2_I2C_MAIN_RELEASE_RESET();
}


/***************************************************/
/*          System class implementation            */
/***************************************************/

static void _AsyncI2CTest(bool success, uintptr_t context, uint32_t value, uint16_t length) {
  static float loudness_gains[2];

  static uint32_t mixerAndVols[6] = { 0x40000001, 0, 0, 0x40000002, 0, 0x80000000 };
  //uint16_t regSizes[2] = { 16, 8 };
  uint32_t reg_tmp;

  if (!success) {
    DEBUG_PRINTF("* DAP async fail at context %u\n", context);
    return;
  }

  switch (context) {
    case 0:
      DEBUG_PRINTF("DAP async status: 0x%02X reg 0x%02X\n", (uint8_t)value, bbv2_system.dap_if.registers.Reg8(0x01));
      bbv2_system.dap_if.WriteRegister32Async(0x28, 44100, std::bind(_AsyncI2CTest, std::placeholders::_1, 1, std::placeholders::_2, std::placeholders::_3));
      break;
    case 1:
      DEBUG_PRINTF("DAP async write I2S1 SR done, reg %lu\n", bbv2_system.dap_if.registers.Reg32(0x28));
      bbv2_system.dap_if.ReadRegister32Async(0x28, std::bind(_AsyncI2CTest, std::placeholders::_1, 2, std::placeholders::_2, std::placeholders::_3));
      break;
    case 2:
      DEBUG_PRINTF("DAP async new I2S1 SR: %lu reg %lu\n", value, bbv2_system.dap_if.registers.Reg32(0x28));
      bbv2_system.dap_if.ReadRegisterAsync(0x42, (uint8_t*)loudness_gains, std::bind(_AsyncI2CTest, std::placeholders::_1, 3, std::placeholders::_2, std::placeholders::_3));
      break;
    case 3:
      reg_tmp = *(uint32_t*)bbv2_system.dap_if.registers[0x42];
      DEBUG_PRINTF("DAP async initial loudness: %.1f %.1f; reg1 %.1f\n", loudness_gains[0], loudness_gains[1], *(float*)&reg_tmp);
      loudness_gains[0] = -10.0f;
      loudness_gains[1] = 0.0f;
      bbv2_system.dap_if.WriteRegisterAsync(0x42, (uint8_t*)loudness_gains, std::bind(_AsyncI2CTest, std::placeholders::_1, 4, std::placeholders::_2, std::placeholders::_3));
      break;
    case 4:
      reg_tmp = *(uint32_t*)bbv2_system.dap_if.registers[0x42];
      DEBUG_PRINTF("DAP async write loudness done, reg1 %.1f\n", *(float*)&reg_tmp);
      memset(loudness_gains, 0, sizeof(loudness_gains));
      bbv2_system.dap_if.ReadRegisterAsync(0x42, (uint8_t*)loudness_gains, std::bind(_AsyncI2CTest, std::placeholders::_1, 5, std::placeholders::_2, std::placeholders::_3));
      break;
    case 5:
      reg_tmp = *(uint32_t*)bbv2_system.dap_if.registers[0x42];
      DEBUG_PRINTF("DAP async new loudness: %.1f %.1f; reg1 %.1f\n", loudness_gains[0], loudness_gains[1], *(float*)&reg_tmp);
      bbv2_system.dap_if.WriteMultiRegisterAsync(0x40, (uint8_t*)mixerAndVols, 2, std::bind(_AsyncI2CTest, std::placeholders::_1, 6, std::placeholders::_2, std::placeholders::_3));
      break;
    case 6:
      reg_tmp = *(uint32_t*)bbv2_system.dap_if.registers[0x41];
      DEBUG_PRINTF("DAP async write mixer+vols done, reg1 0x%08lX %.1f\n", *(uint32_t*)bbv2_system.dap_if.registers[0x40], *(float*)&reg_tmp);
      memset(mixerAndVols, 0, sizeof(mixerAndVols));
      bbv2_system.dap_if.ReadMultiRegisterAsync(0x40, (uint8_t*)mixerAndVols, 2, std::bind(_AsyncI2CTest, std::placeholders::_1, 7, std::placeholders::_2, std::placeholders::_3));
      break;
    case 7:
      reg_tmp = *(uint32_t*)bbv2_system.dap_if.registers[0x41];
      DEBUG_PRINTF("DAP async new mixer: 0x%08lX 0x%08lX 0x%08lX 0x%08lX; vols: %.1f %.1f; reg1 0x%08lX %.1f\n", mixerAndVols[0], mixerAndVols[1], mixerAndVols[2], mixerAndVols[3], ((float*)mixerAndVols)[4], ((float*)mixerAndVols)[5], *(uint32_t*)bbv2_system.dap_if.registers[0x40], *(float*)&reg_tmp);
      break;
    default:
      break;
  }
}

/*static void _AsyncUARTTest(bool success, uintptr_t context, uint32_t value, uint16_t length) {
  if (!success) {
    DEBUG_PRINTF("* BTRX fail at context %u\n", context);
    return;
  }

  switch (context) {
    case 0:
      if (length == 1) {
        DEBUG_PRINTF("BTRX module ID: 0x%02X\n", (uint8_t)value);
      } else {
        DEBUG_PRINTF("BTRX wrong length %u at context %u\n", length, context);
      }
      bbv2_system.btrx_if.ReadRegister16Async(0x20, std::bind(_AsyncUARTTest, std::placeholders::_1, 1, std::placeholders::_2, std::placeholders::_3));
      break;
    case 1:
      if (length == 2) {
        DEBUG_PRINTF("BTRX initial notif mask: 0x%04X reg 0x%04X\n", (uint16_t)value, bbv2_system.btrx_if.registers.Reg16(0x20));
      } else {
        DEBUG_PRINTF("BTRX wrong length %u at context %u\n", length, context);
      }
      bbv2_system.btrx_if.WriteRegister16Async(0x20, 0x1F, ModuleTransferCallback());
      bbv2_system.btrx_if.ReadRegister16Async(0x20, std::bind(_AsyncUARTTest, std::placeholders::_1, 2, std::placeholders::_2, std::placeholders::_3));
      break;
    case 2:
      if (length == 2) {
        DEBUG_PRINTF("BTRX new notif mask: 0x%04X reg 0x%04X\n", (uint16_t)value, bbv2_system.btrx_if.registers.Reg16(0x20));
      } else {
        DEBUG_PRINTF("BTRX wrong length %u at context %u\n", length, context);
      }
      bbv2_system.btrx_if.ReadRegister16Async(0x00, std::bind(_AsyncUARTTest, std::placeholders::_1, 3, std::placeholders::_2, std::placeholders::_3));
      break;
    case 3:
      if (length == 2) {
        DEBUG_PRINTF("BTRX initial status: 0x%04X reg 0x%04X\n", (uint16_t)value, bbv2_system.btrx_if.registers.Reg16(0x00));
      } else {
        DEBUG_PRINTF("BTRX wrong length %u at context %u\n", length, context);
      }
      if ((value & UARTDEF_BTRX_STATUS_INIT_DONE_Msk) != 0) {
        //init done - proceed
        bbv2_system.btrx_if.WriteRegister8Async(0x31, 0x04, ModuleTransferCallback());
        bbv2_system.btrx_if.ReadRegister16Async(0x00, std::bind(_AsyncUARTTest, std::placeholders::_1, 4, std::placeholders::_2, std::placeholders::_3));
      } else {
        //init not done yet - stay in this stage
        bbv2_system.btrx_if.ReadRegister16Async(0x00, std::bind(_AsyncUARTTest, std::placeholders::_1, 3, std::placeholders::_2, std::placeholders::_3));
      }
      break;
    case 4:
      if (length == 2) {
        DEBUG_PRINTF("BTRX new status: 0x%04X reg 0x%04X\n", (uint16_t)value, bbv2_system.btrx_if.registers.Reg16(0x00));
      } else {
        DEBUG_PRINTF("BTRX wrong length %u at context %u\n", length, context);
      }
      break;
    default:
      break;
  }
}*/

void BlockBoxV2System::Init() {
  this->main_i2c_hw.Init();
  this->dap_if.Init();
  this->dac_if.Init();
  this->btrx_if.Init();

  /*DEBUG_PRINTF("DAP module ID: 0x%02X reg 0x%02X\n", this->dap_if.ReadRegister8(0xFF), this->dap_if.registers.Reg8(0xFF));
  DEBUG_PRINTF("DAP I2S1 sample rate: %lu reg %lu\n", this->dap_if.ReadRegister32(0x28), this->dap_if.registers.Reg32(0x28));
  DEBUG_PRINTF("DAP initial control: 0x%02X reg 0x%02X\n", this->dap_if.ReadRegister8(0x08), this->dap_if.registers.Reg8(0x08));
  this->dap_if.WriteRegister8(0x08, 0x07);
  uint8_t reg_val = this->dap_if.registers.Reg8(0x08);
  DEBUG_PRINTF("DAP new control: 0x%02X reg 0x%02X/0x%02X\n", this->dap_if.ReadRegister8(0x08), reg_val, this->dap_if.registers.Reg8(0x08));
  uint32_t mixerAndVols[6];
  //uint16_t regSizes[2] = { 16, 8 };
  this->dap_if.ReadMultiRegister(0x40, (uint8_t*)mixerAndVols, 2);
  uint32_t reg_tmp = *(uint32_t*)this->dap_if.registers[0x41];
  DEBUG_PRINTF("DAP initial mixer: 0x%08lX 0x%08lX 0x%08lX 0x%08lX; vols: %.1f %.1f; reg1 0x%08lX %.1f\n", mixerAndVols[0], mixerAndVols[1], mixerAndVols[2], mixerAndVols[3], ((float*)mixerAndVols)[4], ((float*)mixerAndVols)[5], *(uint32_t*)this->dap_if.registers[0x40], *(float*)&reg_tmp);
  mixerAndVols[0] = 0x41234567;
  mixerAndVols[1] = 0x00000008;
  mixerAndVols[2] = 0x00000009;
  mixerAndVols[3] = 0x4123456A;
  ((float*)mixerAndVols)[4] = -3.5f;
  ((float*)mixerAndVols)[5] = -5.5f;
  this->dap_if.WriteMultiRegister(0x40, (uint8_t*)mixerAndVols, 2);
  reg_tmp = *(uint32_t*)this->dap_if.registers[0x41];
  DEBUG_PRINTF("DAP mixer/vol regs after write: 0x%08lX %.1f\n", *(uint32_t*)this->dap_if.registers[0x40], *(float*)&reg_tmp);
  memset(mixerAndVols, 0, sizeof(mixerAndVols));
  this->dap_if.ReadMultiRegister(0x40, (uint8_t*)mixerAndVols, 2);
  reg_tmp = *(uint32_t*)this->dap_if.registers[0x41];
  DEBUG_PRINTF("DAP new mixer: 0x%08lX 0x%08lX 0x%08lX 0x%08lX; vols: %.1f %.1f; reg1 0x%08lX %.1f\n", mixerAndVols[0], mixerAndVols[1], mixerAndVols[2], mixerAndVols[3], ((float*)mixerAndVols)[4], ((float*)mixerAndVols)[5], *(uint32_t*)this->dap_if.registers[0x40], *(float*)&reg_tmp);

  HAL_Delay(100);

  this->dap_if.SetInterruptMask(0xF, [&](bool success) {
    DEBUG_PRINTF("DAP interrupt mask set: 0x%02X\n", this->dap_if.GetInterruptMask());
  });*/

  //this->dap_if.ReadRegister8Async(0x01, std::bind(_AsyncI2CTest, std::placeholders::_1, 0, std::placeholders::_2, std::placeholders::_3));
  //this->btrx_if.ReadRegister8Async(0xFE, std::bind(_AsyncUARTTest, std::placeholders::_1, 0, std::placeholders::_2, std::placeholders::_3));

  this->dap_if.RegisterCallback([&](ModuleInterface&, uint32_t event) {
    switch (event) {
      case MODIF_DAP_EVENT_STATUS_UPDATE:
        DEBUG_PRINTF("DAP status update: 0x%02X\n", this->dap_if.GetStatus().value);
        break;
      case MODIF_DAP_EVENT_INPUTS_UPDATE:
        DEBUG_PRINTF("DAP inputs update: active 0x%02X, available 0x%02X\n", this->dap_if.GetActiveInput(), this->dap_if.GetAvailableInputs().value);
        break;
      case MODIF_DAP_EVENT_INPUT_RATE_UPDATE:
        DEBUG_PRINTF("DAP input rate update: %u\n", this->dap_if.GetSRCInputSampleRate());
        break;
      case MODIF_DAP_EVENT_SRC_STATS_UPDATE:
        DEBUG_PRINTF("DAP SRC stats update: rate error %.4f, fill error %.1f\n", this->dap_if.GetSRCInputRateErrorRelative(), this->dap_if.GetSRCBufferFillErrorSamples());
        break;
      default:
        break;
    }
  }, MODIF_DAP_EVENT_STATUS_UPDATE | MODIF_DAP_EVENT_INPUTS_UPDATE | MODIF_DAP_EVENT_INPUT_RATE_UPDATE | MODIF_DAP_EVENT_SRC_STATS_UPDATE);

  this->dac_if.RegisterCallback([&](ModuleInterface&, uint32_t event) {
    DEBUG_PRINTF("HiFiDAC status update: 0x%02X\n", this->dac_if.GetStatus().value);
  }, MODIF_HIFIDAC_EVENT_STATUS_UPDATE);

  this->btrx_if.RegisterCallback([&](ModuleInterface&, uint32_t event) {
    switch (event) {
      case MODIF_BTRX_EVENT_STATUS_UPDATE:
        DEBUG_PRINTF("BTRX status update: 0x%04X\n", this->btrx_if.GetStatus().value);
        break;
      case MODIF_BTRX_EVENT_VOLUME_UPDATE:
        DEBUG_PRINTF("BTRX volume update: %u\n", this->btrx_if.GetAbsoluteVolume());
        break;
      case MODIF_BTRX_EVENT_MEDIA_META_UPDATE:
        DEBUG_PRINTF("BTRX media meta update: %s - %s (%s)\n", this->btrx_if.GetMediaArtist(), this->btrx_if.GetMediaTitle(), this->btrx_if.GetMediaAlbum());
        break;
      case MODIF_BTRX_EVENT_DEVICE_UPDATE:
        DEBUG_PRINTF("BTRX device update: %s (0x%012llX)\n", this->btrx_if.GetDeviceName(), this->btrx_if.GetDeviceAddress());
        break;
      case MODIF_BTRX_EVENT_CONN_STATS_UPDATE:
        DEBUG_PRINTF("BTRX conn stats update: RSSI %d, quality %u\n", this->btrx_if.GetConnectionRSSI(), this->btrx_if.GetConnectionQuality());
        break;
      case MODIF_BTRX_EVENT_CODEC_UPDATE:
        DEBUG_PRINTF("BTRX codec update: %s\n", this->btrx_if.GetActiveCodec());
        break;
      default:
        break;
    }
  }, MODIF_BTRX_EVENT_STATUS_UPDATE | MODIF_BTRX_EVENT_VOLUME_UPDATE | MODIF_BTRX_EVENT_MEDIA_META_UPDATE | MODIF_BTRX_EVENT_DEVICE_UPDATE | MODIF_BTRX_EVENT_CONN_STATS_UPDATE | MODIF_BTRX_EVENT_CODEC_UPDATE);

  HAL_Delay(500);

  this->dap_if.ResetModule([&](bool success) {
    DEBUG_PRINTF("DAP reset/init complete, success %u\n", success);
    if (success) {
      //this->dap_if.monitor_src_stats = true;
      this->dap_if.SetI2SInputSampleRate(IF_DAP_INPUT_I2S1, IF_DAP_SR_96K, [&](bool success) {
        DEBUG_PRINTF("DAP I2S1 sample rate set to 96K, success %u\n", success);
      });
      this->dap_if.SetVolumeGains({ -6.0f, -6.0f }, [&](bool success) {
        DEBUG_PRINTF("DAP vol gains set, success %u\n", success);
      });
    }
  });

  this->dac_if.ResetModule([&](bool success) {
    DEBUG_PRINTF("HiFiDAC reset/init complete, success %u\n", success);
    if (success) {
      HiFiDACConfig cfg;
      cfg.dac_enable = true;
      cfg.sync_mode = true;
      cfg.master_mode = false;
      this->dac_if.SetConfig(cfg, [&](bool success) {
        DEBUG_PRINTF("HiFiDAC config set to enabled/sync/slave, success %u\n", success);
      });
    }
  });

  this->btrx_if.ResetModule([&](bool success) {
    DEBUG_PRINTF("BTRX reset/init complete, success %u\n", success);
    if (success) {
      this->btrx_if.SetDiscoverable(true, [&](bool success) {
        DEBUG_PRINTF("BTRX set discoverable, success %u\n", success);
      });
    }
  });
}


void BlockBoxV2System::LoopTasks() {
  this->main_i2c_hw.LoopTasks();
  this->dap_if.LoopTasks();
  this->dac_if.LoopTasks();
  this->btrx_if.LoopTasks();
}


BlockBoxV2System::BlockBoxV2System() :
    main_i2c_hw(&BBV2_I2C_MAIN_HANDLE, _BlockBoxV2_I2C_Main_HardwareReset),
    dap_if(this->main_i2c_hw, BBV2_DAP_I2C_ADDR, BBV2_DAP_INT_PORT, BBV2_DAP_INT_PIN),
    dac_if(this->main_i2c_hw, BBV2_HIFIDAC_I2C_ADDR, BBV2_HIFIDAC_INT_PORT, BBV2_HIFIDAC_INT_PIN),
    btrx_if(&BBV2_BTRX_UART_HANDLE) {

}


/***************************************************/
/*              Interrupt forwarding               */
/***************************************************/

void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *hi2c) {
  if (hi2c == &BBV2_I2C_MAIN_HANDLE) {
    bbv2_system.main_i2c_hw.HandleInterrupt(IF_TX_COMPLETE);
  }
}

void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c) {
  if (hi2c == &BBV2_I2C_MAIN_HANDLE) {
    bbv2_system.main_i2c_hw.HandleInterrupt(IF_RX_COMPLETE);
  }
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c) {
  if (hi2c == &BBV2_I2C_MAIN_HANDLE) {
    bbv2_system.main_i2c_hw.HandleInterrupt(IF_ERROR);
  }
}


void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
  switch (GPIO_Pin) {
    case BBV2_DAP_INT_PIN:
      bbv2_system.dap_if.HandleInterrupt(IF_EXTI, GPIO_Pin);
      break;
    case BBV2_HIFIDAC_INT_PIN:
      bbv2_system.dac_if.HandleInterrupt(IF_EXTI, GPIO_Pin);
      break;
    default:
      break;
  }
}


void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef* huart, uint16_t Size) {
  if (huart == &BBV2_BTRX_UART_HANDLE) {
    bbv2_system.btrx_if.HandleInterrupt(IF_RX_COMPLETE, Size);
  }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef* huart) {
  if (huart == &BBV2_BTRX_UART_HANDLE) {
    bbv2_system.btrx_if.HandleInterrupt(IF_TX_COMPLETE, 0);
  } else {
    Retarget_UART_TxCpltCallback(huart);
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef* huart) {
  if (huart == &BBV2_BTRX_UART_HANDLE) {
    bbv2_system.btrx_if.HandleInterrupt(IF_ERROR, 0);
  }
}


