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
#include "stm32c0xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */
extern I2C_HandleTypeDef hi2c1;

extern SPI_HandleTypeDef hspi1;

extern UART_HandleTypeDef huart1;
/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

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

#ifndef MIN
  #define MIN(x,y) ((x) < (y) ? (x) : (y))
#endif

#ifndef MAX
  #define MAX(x,y) ((x) > (y) ? (x) : (y))
#endif
/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define DAC_EN_Pin GPIO_PIN_3
#define DAC_EN_GPIO_Port GPIOA
#define SPI1_NSS_Pin GPIO_PIN_4
#define SPI1_NSS_GPIO_Port GPIOA
#define GPIO1_Pin GPIO_PIN_7
#define GPIO1_GPIO_Port GPIOA
#define GPIO2_Pin GPIO_PIN_0
#define GPIO2_GPIO_Port GPIOB
#define GPIO3_Pin GPIO_PIN_1
#define GPIO3_GPIO_Port GPIOB
#define GPIO4_Pin GPIO_PIN_2
#define GPIO4_GPIO_Port GPIOB
#define I2C_INT_N_Pin GPIO_PIN_8
#define I2C_INT_N_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */
//main loop period in milliseconds
#define MAIN_LOOP_PERIOD_MS 10
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
