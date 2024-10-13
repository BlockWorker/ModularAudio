/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f3xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */
extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern ADC_HandleTypeDef hadc3;
extern ADC_HandleTypeDef hadc4;
extern DMA_HandleTypeDef hdma_adc1;
extern DMA_HandleTypeDef hdma_adc2;
extern DMA_HandleTypeDef hdma_adc3;
extern DMA_HandleTypeDef hdma_adc4;

extern DAC_HandleTypeDef hdac1;

extern I2C_HandleTypeDef hi2c3;

extern UART_HandleTypeDef huart3;
/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */
//quick error-return macro
#define ReturnOnError(x) do { HAL_StatusTypeDef __res = (x); if (__res != HAL_OK) return __res; } while (0)

//debug printout, disabled outside of debug mode
#ifdef DEBUG
#define DEBUG_PRINTF(...) do { printf(__VA_ARGS__); } while (0)
#else
#define DEBUG_PRINTF(...) do { } while (0)
#endif

//allow define to select four-channel (SE) mode, keep undefined to select two-channel (BTL) mode
#undef MAIN_FOUR_CHANNEL
//#define MAIN_FOUR_CHANNEL
/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define I2C3_INT_N_Pin GPIO_PIN_9
#define I2C3_INT_N_GPIO_Port GPIOA
#define AMP_RESET_N_Pin GPIO_PIN_10
#define AMP_RESET_N_GPIO_Port GPIOA
#define AMP_FAULT_N_Pin GPIO_PIN_11
#define AMP_FAULT_N_GPIO_Port GPIOA
#define AMP_FAULT_N_EXTI_IRQn EXTI15_10_IRQn
#define AMP_CLIP_OTW_N_Pin GPIO_PIN_12
#define AMP_CLIP_OTW_N_GPIO_Port GPIOA
#define AMP_CLIP_OTW_N_EXTI_IRQn EXTI15_10_IRQn

/* USER CODE BEGIN Private defines */
//main loop period in milliseconds
#define MAIN_LOOP_PERIOD_MS 10
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
