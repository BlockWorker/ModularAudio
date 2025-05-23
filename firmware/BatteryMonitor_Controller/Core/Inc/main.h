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
#include "stm32l0xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <stdbool.h>
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */
extern CRC_HandleTypeDef hcrc;

extern I2C_HandleTypeDef hi2c3;

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;


extern bool pwrsw_status;
extern bool low_voltage_powerdown;
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
#define PWR_SWITCH_Pin GPIO_PIN_14
#define PWR_SWITCH_GPIO_Port GPIOC
#define PWR_SWITCH_EXTI_IRQn EXTI4_15_IRQn
#define I2C_ISO_EN_Pin GPIO_PIN_9
#define I2C_ISO_EN_GPIO_Port GPIOA
#define BMS_ALERT_N_Pin GPIO_PIN_5
#define BMS_ALERT_N_GPIO_Port GPIOB
#define BMS_ALERT_N_EXTI_IRQn EXTI4_15_IRQn

/* USER CODE BEGIN Private defines */
//period of main program loop in milliseconds
#define MAIN_LOOP_PERIOD_MS 10

//main loop cycles for power switch debouncing
#define MAIN_PWRSW_DEBOUNCE_CYCLES 20

//cell voltage threshold for low-voltage power off, in mV
#define MAIN_LVOFF_THRESHOLD 3000
//timeout in main loop cycles for low-voltage power off
#define MAIN_LVOFF_TIMEOUT (30100 / MAIN_LOOP_PERIOD_MS)
//period for low-voltage power off system notifications, in main loop cycles
#define MAIN_LVOFF_NOTIFY_PERIOD (1000 / MAIN_LOOP_PERIOD_MS)

//current calibration mode - disable for actual use!
#undef MAIN_CURRENT_CALIBRATION
//#define MAIN_CURRENT_CALIBRATION
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
