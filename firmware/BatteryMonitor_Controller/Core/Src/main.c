/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "retarget.h"
#include <stdio.h>
#include <string.h>
#include "bms.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
CRC_HandleTypeDef hcrc;

I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c3;

IWDG_HandleTypeDef hiwdg;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
//power switch status and debouncing logic variables
static uint8_t pwrsw_debounce_count = 0;
static GPIO_PinState pwrsw_last_state = GPIO_PIN_SET;
bool pwrsw_status = false;

//whether the current main loop cycle follows stop mode
static bool wakeup_from_stop = false;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_I2C3_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_CRC_Init(void);
static void MX_IWDG_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static __always_inline void _RefreshWatchdogs() {
  //HAL_WWDG_Refresh(&hwwdg);
  HAL_IWDG_Refresh(&hiwdg);
}

static void _TestReadDataMem() {
  uint8_t data_mem_values[115] = { 0 };

  DEBUG_PRINTF("Reading contiguous data memory...\n");
  if (BMS_I2C_DataMemoryRead(MEMADDR_CAL_CELL1_GAIN, data_mem_values, 0x5E, 3) == HAL_OK) {
    DEBUG_PRINTF("Success: Contiguous memory read\n");
  } else {
    DEBUG_PRINTF("Contiguous data memory read failed!\n");
  }

  DEBUG_PRINTF("Reading extra data memory...\n");
  if (BMS_I2C_DataMemoryRead(MEMADDR_CAL_CELL4_DELTA, data_mem_values + 0x71, 2, 3) == HAL_OK) {
    DEBUG_PRINTF("Success: Extra memory read\n");
  } else {
    DEBUG_PRINTF("Extra data memory read failed!\n");
  }

  HAL_Delay(10);

  uint8_t configured_mem_values[115] __attribute__((aligned (2))) = { 0 };
  memcpy(configured_mem_values + 0x14, bms_config_main_array, sizeof(bms_config_main_array));
  *(uint16_t*)(configured_mem_values + 0x06) = BMS_CAL_CURR_GAIN;
  *(uint16_t*)(configured_mem_values + 0x0A) = BMS_CAL_CC1_GAIN;

  int i, j;
  DEBUG_PRINTF("\n     _0 _1 _2 _3 _4 _5 _6 _7 _8 _9 _A _B _C _D _E _F\n");
  for (i = 0; i < 8; i++) {
    int end = (i == 7) ? 3 : 16;

    DEBUG_PRINTF("\n r%X_", i);
    for (j = 0; j < end; j++) {
      DEBUG_PRINTF(" %02X", data_mem_values[16 * i + j]);
    }
    DEBUG_PRINTF("\n c%X_", i);
    for (j = 0; j < end; j++) {
      DEBUG_PRINTF(" %02X", configured_mem_values[16 * i + j]);
    }
    DEBUG_PRINTF("\n");
  }
}


void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
  switch (GPIO_Pin) {
    case BMS_ALERT_N_Pin:
      BMS_AlertInterrupt();
      break;
    case PWR_SWITCH_Pin:
      pwrsw_last_state = HAL_GPIO_ReadPin(PWR_SWITCH_GPIO_Port, PWR_SWITCH_Pin);
      pwrsw_debounce_count = MAIN_PWRSW_DEBOUNCE_CYCLES;
      break;
  }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_I2C3_Init();
  MX_USART2_UART_Init();
  MX_CRC_Init();
  MX_IWDG_Init();
  /* USER CODE BEGIN 2 */
  RetargetInit(&huart2);

  HAL_Delay(10);
  _RefreshWatchdogs();

  DEBUG_PRINTF("Controller started\n");

  HAL_Delay(1000);

  pwrsw_debounce_count = 0;
  pwrsw_status = (HAL_GPIO_ReadPin(PWR_SWITCH_GPIO_Port, PWR_SWITCH_Pin) == GPIO_PIN_RESET);
  wakeup_from_stop = false;

  //configure for lowest power - not sure if compatible with stop mode, but won't hurt
  HAL_PWREx_EnableUltraLowPower();
  HAL_PWREx_EnableFastWakeUp();

  if (BMS_Init() != HAL_OK) {
    DEBUG_PRINTF("ERROR: BMS init failed!\n");
    //Error_Handler();
  }
  DEBUG_PRINTF("BMS init completed\n");

  HAL_Delay(100);

  _TestReadDataMem();

  HAL_Delay(100);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  //loop iteration counter - even if extremely unlikely wrap-around happens, it shouldn't be critical
  uint32_t loop_count = 0;
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    uint32_t iteration_start_tick = HAL_GetTick();

    //check power switch: handle debouncing or see if it changed outside of debouncing somehow
    GPIO_PinState switch_state = HAL_GPIO_ReadPin(PWR_SWITCH_GPIO_Port, PWR_SWITCH_Pin);
    if (pwrsw_debounce_count > 0) {
      //if state changed: reset debounce counter
      if (switch_state != pwrsw_last_state) {
        pwrsw_last_state = switch_state;
        pwrsw_debounce_count = MAIN_PWRSW_DEBOUNCE_CYCLES;
      } else if (pwrsw_debounce_count-- == 1) { //same state as before: decrement counter and check if we're at the last cycle
        //last cycle: debounce is done, write switch status based on state
        pwrsw_status = (switch_state == GPIO_PIN_RESET);
      }
    } else if (pwrsw_status != (switch_state == GPIO_PIN_RESET)) {
      //this shouldn't really ever happen
      DEBUG_PRINTF("WARNING: Power switch change detected that somehow wasn't caught by an interrupt\n");
      pwrsw_last_state = switch_state;
      pwrsw_debounce_count = MAIN_PWRSW_DEBOUNCE_CYCLES;
    }

    //apply power switch status to desired BMS state
    bms_should_disable_fets = bms_should_deepsleep = !pwrsw_status;

    if (wakeup_from_stop) {
      //exit deepsleep after wakeup, to allow measurements
      BMS_ExitDeepSleep();
    }

    BMS_LoopUpdate(loop_count);

    _RefreshWatchdogs();

    //check whether to stop or continue to next loop, stop conditions: power switch off, no ongoing debounce, BMS in deepsleep, BMS alert pin is high (no alert)
    if (!pwrsw_status && pwrsw_debounce_count == 0 && bms_status.mode == BMSMODE_DEEPSLEEP && HAL_GPIO_ReadPin(BMS_ALERT_N_GPIO_Port, BMS_ALERT_N_Pin) == GPIO_PIN_SET) {
      //prepare everything for a check cycle after wakeup
      loop_count = 0;
      wakeup_from_stop = true;

      //enter stop mode
      __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);
      HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);

      _RefreshWatchdogs();
      DEBUG_PRINTF("Woke up from STOP, loop before stop took %lu ms\n", HAL_GetTick() - iteration_start_tick);
    } else {
      //just go to next loop after corresponding delay
      loop_count++;
      wakeup_from_stop = false;
      while((HAL_GetTick() - iteration_start_tick) < MAIN_LOOP_PERIOD_MS); //replaces HAL_Delay() to not wait any unnecessary extra ticks
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART2|RCC_PERIPHCLK_I2C1
                              |RCC_PERIPHCLK_I2C3;
  PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
  PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_PCLK1;
  PeriphClkInit.I2c3ClockSelection = RCC_I2C3CLKSOURCE_PCLK1;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief CRC Initialization Function
  * @param None
  * @retval None
  */
static void MX_CRC_Init(void)
{

  /* USER CODE BEGIN CRC_Init 0 */

  /* USER CODE END CRC_Init 0 */

  /* USER CODE BEGIN CRC_Init 1 */

  /* USER CODE END CRC_Init 1 */
  hcrc.Instance = CRC;
  hcrc.Init.DefaultPolynomialUse = DEFAULT_POLYNOMIAL_DISABLE;
  hcrc.Init.DefaultInitValueUse = DEFAULT_INIT_VALUE_DISABLE;
  hcrc.Init.GeneratingPolynomial = 7;
  hcrc.Init.CRCLength = CRC_POLYLENGTH_8B;
  hcrc.Init.InitValue = 0;
  hcrc.Init.InputDataInversionMode = CRC_INPUTDATA_INVERSION_NONE;
  hcrc.Init.OutputDataInversionMode = CRC_OUTPUTDATA_INVERSION_DISABLE;
  hcrc.InputDataFormat = CRC_INPUTDATA_FORMAT_BYTES;
  if (HAL_CRC_Init(&hcrc) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CRC_Init 2 */

  /* USER CODE END CRC_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x00100E16;
  hi2c1.Init.OwnAddress1 = 22;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief I2C3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C3_Init(void)
{

  /* USER CODE BEGIN I2C3_Init 0 */

  /* USER CODE END I2C3_Init 0 */

  /* USER CODE BEGIN I2C3_Init 1 */

  /* USER CODE END I2C3_Init 1 */
  hi2c3.Instance = I2C3;
  hi2c3.Init.Timing = 0x00100E16;
  hi2c3.Init.OwnAddress1 = 0;
  hi2c3.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c3.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c3.Init.OwnAddress2 = 0;
  hi2c3.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c3.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c3.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c3) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c3, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c3, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C3_Init 2 */

  /* USER CODE END I2C3_Init 2 */

}

/**
  * @brief IWDG Initialization Function
  * @param None
  * @retval None
  */
static void MX_IWDG_Init(void)
{

  /* USER CODE BEGIN IWDG_Init 0 */

  /* USER CODE END IWDG_Init 0 */

  /* USER CODE BEGIN IWDG_Init 1 */

  /* USER CODE END IWDG_Init 1 */
  hiwdg.Instance = IWDG;
  hiwdg.Init.Prescaler = IWDG_PRESCALER_256;
  hiwdg.Init.Window = 4095;
  hiwdg.Init.Reload = 4095;
  if (HAL_IWDG_Init(&hiwdg) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN IWDG_Init 2 */

  /* USER CODE END IWDG_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(I2C_ISO_EN_GPIO_Port, I2C_ISO_EN_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : PWR_SWITCH_Pin */
  GPIO_InitStruct.Pin = PWR_SWITCH_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(PWR_SWITCH_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PC15 */
  GPIO_InitStruct.Pin = GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PA0 PA1 PA4 PA5
                           PA6 PA7 PA10 PA11
                           PA12 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_4|GPIO_PIN_5
                          |GPIO_PIN_6|GPIO_PIN_7|GPIO_PIN_10|GPIO_PIN_11
                          |GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB0 PB1 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : I2C_ISO_EN_Pin */
  GPIO_InitStruct.Pin = I2C_ISO_EN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(I2C_ISO_EN_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : BMS_ALERT_N_Pin */
  GPIO_InitStruct.Pin = BMS_ALERT_N_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BMS_ALERT_N_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI4_15_IRQn, 1, 0);
  HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
