/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "stm32h7xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */
extern ADC_HandleTypeDef hadc1;

extern I2C_HandleTypeDef hi2c1;
extern SMBUS_HandleTypeDef hsmbus3;
extern I2C_HandleTypeDef hi2c4;
extern I2C_HandleTypeDef hi2c5;

extern I2S_HandleTypeDef hi2s6;

extern OSPI_HandleTypeDef hospi1;

extern SPI_HandleTypeDef hspi2;

extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim4;

extern UART_HandleTypeDef huart2;
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

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define B1_VOL_UP_Pin GPIO_PIN_2
#define B1_VOL_UP_GPIO_Port GPIOE
#define B1_VOL_DOWN_Pin GPIO_PIN_4
#define B1_VOL_DOWN_GPIO_Port GPIOE
#define B1_BACK_Pin GPIO_PIN_5
#define B1_BACK_GPIO_Port GPIOE
#define B1_PLAY_Pin GPIO_PIN_13
#define B1_PLAY_GPIO_Port GPIOC
#define B1_MFB_Pin GPIO_PIN_14
#define B1_MFB_GPIO_Port GPIOC
#define B1_FWD_Pin GPIO_PIN_15
#define B1_FWD_GPIO_Port GPIOC
#define LCD_GPIO3_Pin GPIO_PIN_4
#define LCD_GPIO3_GPIO_Port GPIOA
#define LCD_GPIO2_Pin GPIO_PIN_5
#define LCD_GPIO2_GPIO_Port GPIOA
#define LCD_INT_N_Pin GPIO_PIN_4
#define LCD_INT_N_GPIO_Port GPIOC
#define LCD_PD_N_Pin GPIO_PIN_5
#define LCD_PD_N_GPIO_Port GPIOC
#define LCD_B1_D3_Pin GPIO_PIN_7
#define LCD_B1_D3_GPIO_Port GPIOE
#define LCD_BL_DRIVE_Pin GPIO_PIN_8
#define LCD_BL_DRIVE_GPIO_Port GPIOE
#define B1_LCD_SWITCH_Pin GPIO_PIN_9
#define B1_LCD_SWITCH_GPIO_Port GPIOD
#define B1_LEDMODE_1_Pin GPIO_PIN_10
#define B1_LEDMODE_1_GPIO_Port GPIOD
#define B1_LEDMODE_2_Pin GPIO_PIN_11
#define B1_LEDMODE_2_GPIO_Port GPIOD
#define CHG_ACOK_Pin GPIO_PIN_8
#define CHG_ACOK_GPIO_Port GPIOC
#define AMP_CLIP_OTW_N_Pin GPIO_PIN_10
#define AMP_CLIP_OTW_N_GPIO_Port GPIOA
#define AMP_FAULT_N_Pin GPIO_PIN_11
#define AMP_FAULT_N_GPIO_Port GPIOA
#define AMP_RESET_N_Pin GPIO_PIN_12
#define AMP_RESET_N_GPIO_Port GPIOA
#define I2C5_INT0_N_Pin GPIO_PIN_12
#define I2C5_INT0_N_GPIO_Port GPIOC
#define I2C5_INT1_N_Pin GPIO_PIN_0
#define I2C5_INT1_N_GPIO_Port GPIOD
#define I2C5_INT2_N_Pin GPIO_PIN_1
#define I2C5_INT2_N_GPIO_Port GPIOD
#define I2C5_INT3_N_Pin GPIO_PIN_2
#define I2C5_INT3_N_GPIO_Port GPIOD
#define I2C5_INT4_N_Pin GPIO_PIN_3
#define I2C5_INT4_N_GPIO_Port GPIOD
#define I2C1_INT0_N_Pin GPIO_PIN_5
#define I2C1_INT0_N_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
