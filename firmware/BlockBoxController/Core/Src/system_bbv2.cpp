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
  uint16_t regSizes[2] = { 16, 8 };

  if (!success) {
    DEBUG_PRINTF("* DAP async fail at context %u\n", context);
    return;
  }

  switch (context) {
    case 0:
      DEBUG_PRINTF("DAP async status: 0x%02X\n", (uint8_t)value);
      bbv2_system.dap_if.WriteRegister32Async(0x28, 44100, NULL, 0 /*_AsyncI2CTest, 1*/);
      /*break;
    case 1:
      DEBUG_PRINTF("DAP async write I2S1 SR done\n");*/
      bbv2_system.dap_if.ReadRegister32Async(0x28, _AsyncI2CTest, 2);
      break;
    case 2:
      DEBUG_PRINTF("DAP async new I2S1 SR: %lu\n", value);
      bbv2_system.dap_if.ReadRegisterAsync(0x42, (uint8_t*)loudness_gains, 8, _AsyncI2CTest, 3);
      break;
    case 3:
      DEBUG_PRINTF("DAP async initial loudness: %.1f %.1f\n", loudness_gains[0], loudness_gains[1]);
      loudness_gains[0] = -10.0f;
      loudness_gains[1] = 0.0f;
      bbv2_system.dap_if.WriteRegisterAsync(0x42, (uint8_t*)loudness_gains, 8, _AsyncI2CTest, 4);
      break;
    case 4:
      DEBUG_PRINTF("DAP async write loudness done\n");
      memset(loudness_gains, 0, sizeof(loudness_gains));
      bbv2_system.dap_if.ReadRegisterAsync(0x42, (uint8_t*)loudness_gains, 8, _AsyncI2CTest, 5);
      break;
    case 5:
      DEBUG_PRINTF("DAP async new loudness: %.1f %.1f\n", loudness_gains[0], loudness_gains[1]);
      bbv2_system.dap_if.WriteMultiRegisterAsync(0x40, (uint8_t*)mixerAndVols, regSizes, 2, _AsyncI2CTest, 6);
      break;
    case 6:
      DEBUG_PRINTF("DAP async write mixer+vols done\n");
      memset(mixerAndVols, 0, sizeof(mixerAndVols));
      bbv2_system.dap_if.ReadMultiRegisterAsync(0x40, (uint8_t*)mixerAndVols, regSizes, 2, _AsyncI2CTest, 7);
      break;
    case 7:
      DEBUG_PRINTF("DAP async new mixer: 0x%08lX 0x%08lX 0x%08lX 0x%08lX; vols: %.1f %.1f\n", mixerAndVols[0], mixerAndVols[1], mixerAndVols[2], mixerAndVols[3], ((float*)mixerAndVols)[4], ((float*)mixerAndVols)[5]);
      break;
    default:
      break;
  }
}

static void _AsyncUARTTest(bool success, uintptr_t context, uint32_t value, uint16_t length) {
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
      bbv2_system.btrx_if.ReadRegister16Async(0x20, _AsyncUARTTest, 1);
      break;
    case 1:
      if (length == 2) {
        DEBUG_PRINTF("BTRX initial notif mask: 0x%04X\n", (uint16_t)value);
      } else {
        DEBUG_PRINTF("BTRX wrong length %u at context %u\n", length, context);
      }
      bbv2_system.btrx_if.WriteRegister16Async(0x20, 0x1F, NULL, 0);
      bbv2_system.btrx_if.ReadRegister16Async(0x20, _AsyncUARTTest, 2);
      break;
    case 2:
      if (length == 2) {
        DEBUG_PRINTF("BTRX new notif mask: 0x%04X\n", (uint16_t)value);
      } else {
        DEBUG_PRINTF("BTRX wrong length %u at context %u\n", length, context);
      }
      bbv2_system.btrx_if.ReadRegister16Async(0x00, _AsyncUARTTest, 3);
      break;
    case 3:
      if (length == 2) {
        DEBUG_PRINTF("BTRX initial status: 0x%04X\n", (uint16_t)value);
      } else {
        DEBUG_PRINTF("BTRX wrong length %u at context %u\n", length, context);
      }
      bbv2_system.btrx_if.WriteRegister8Async(0x31, 0x04, NULL, 0);
      bbv2_system.btrx_if.ReadRegister16Async(0x00, _AsyncUARTTest, 4);
      break;
    case 4:
      if (length == 2) {
        DEBUG_PRINTF("BTRX new status: 0x%04X\n", (uint16_t)value);
      } else {
        DEBUG_PRINTF("BTRX wrong length %u at context %u\n", length, context);
      }
      break;
    default:
      break;
  }
}

void BlockBoxV2System::Init() {
  this->main_i2c_hw.Init();
  this->dap_if.Init();
  this->btrx_if.Init();

  DEBUG_PRINTF("DAP module ID: 0x%02X\n", this->dap_if.ReadRegister8(0xFF));
  DEBUG_PRINTF("DAP I2S1 sample rate: %lu\n", this->dap_if.ReadRegister32(0x28));
  DEBUG_PRINTF("DAP initial control: 0x%02X\n", this->dap_if.ReadRegister8(0x08));
  this->dap_if.WriteRegister8(0x08, 0x07);
  DEBUG_PRINTF("DAP new control: 0x%02X\n", this->dap_if.ReadRegister8(0x08));
  uint32_t mixerAndVols[6];
  uint16_t regSizes[2] = { 16, 8 };
  this->dap_if.ReadMultiRegister(0x40, (uint8_t*)mixerAndVols, regSizes, 2);
  DEBUG_PRINTF("DAP initial mixer: 0x%08lX 0x%08lX 0x%08lX 0x%08lX; vols: %.1f %.1f\n", mixerAndVols[0], mixerAndVols[1], mixerAndVols[2], mixerAndVols[3], ((float*)mixerAndVols)[4], ((float*)mixerAndVols)[5]);
  mixerAndVols[0] = 0x41234567;
  mixerAndVols[1] = 0x00000008;
  mixerAndVols[2] = 0x00000009;
  mixerAndVols[3] = 0x4123456A;
  ((float*)mixerAndVols)[4] = -3.5f;
  ((float*)mixerAndVols)[5] = -5.5f;
  this->dap_if.WriteMultiRegister(0x40, (uint8_t*)mixerAndVols, regSizes, 2);
  memset(mixerAndVols, 0, sizeof(mixerAndVols));
  this->dap_if.ReadMultiRegister(0x40, (uint8_t*)mixerAndVols, regSizes, 2);
  DEBUG_PRINTF("DAP new mixer: 0x%08lX 0x%08lX 0x%08lX 0x%08lX; vols: %.1f %.1f\n", mixerAndVols[0], mixerAndVols[1], mixerAndVols[2], mixerAndVols[3], ((float*)mixerAndVols)[4], ((float*)mixerAndVols)[5]);

  HAL_Delay(100);

  //this->dap_if.ReadRegister8Async(0x01, _AsyncI2CTest, 0);
  this->btrx_if.ReadRegister8Async(0xFE, _AsyncUARTTest, 0);
}


void BlockBoxV2System::LoopTasks() {
  this->main_i2c_hw.LoopTasks();
  this->dap_if.LoopTasks();
  this->btrx_if.LoopTasks();
}


BlockBoxV2System::BlockBoxV2System() :
    main_i2c_hw(&BBV2_I2C_MAIN_HANDLE, _BlockBoxV2_I2C_Main_HardwareReset),
    dap_if(this->main_i2c_hw, BBV2_DAP_INT_PORT, BBV2_DAP_INT_PIN, BBV2_DAP_I2C_ADDR, true),
    btrx_if(&BBV2_BTRX_UART_HANDLE, true) {

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


