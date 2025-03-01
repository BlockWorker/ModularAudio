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
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */
extern I2C_HandleTypeDef hi2c1;

extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart6;
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
#define BT_PIO32_Pin GPIO_PIN_12
#define BT_PIO32_GPIO_Port GPIOB
#define BT_PIO29_Pin GPIO_PIN_13
#define BT_PIO29_GPIO_Port GPIOB
#define BT_PIO31_Pin GPIO_PIN_14
#define BT_PIO31_GPIO_Port GPIOB
#define BT_PIO26_Pin GPIO_PIN_15
#define BT_PIO26_GPIO_Port GPIOB
#define BT_PIO30_Pin GPIO_PIN_8
#define BT_PIO30_GPIO_Port GPIOA
#define BT_SYS_CTRL_Pin GPIO_PIN_9
#define BT_SYS_CTRL_GPIO_Port GPIOA
#define BT_RST_N_Pin GPIO_PIN_10
#define BT_RST_N_GPIO_Port GPIOA
#define BT_PIO5_Pin GPIO_PIN_15
#define BT_PIO5_GPIO_Port GPIOA
#define BT_PIO6_Pin GPIO_PIN_3
#define BT_PIO6_GPIO_Port GPIOB
#define BT_PIO3_Pin GPIO_PIN_4
#define BT_PIO3_GPIO_Port GPIOB
#define I2C_INT_N_Pin GPIO_PIN_5
#define I2C_INT_N_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
