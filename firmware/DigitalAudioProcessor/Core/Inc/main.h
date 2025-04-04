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
#include "stm32h7xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */
extern I2C_HandleTypeDef hi2c1;

extern I2S_HandleTypeDef hi2s1;
extern I2S_HandleTypeDef hi2s2;
extern I2S_HandleTypeDef hi2s3;
extern DMA_HandleTypeDef hdma_spi1_rx;
extern DMA_HandleTypeDef hdma_spi2_rx;
extern DMA_HandleTypeDef hdma_spi3_rx;

extern SAI_HandleTypeDef hsai_BlockB4;
extern DMA_HandleTypeDef hdma_sai4_b;

extern SPDIFRX_HandleTypeDef hspdif1;
extern DMA_HandleTypeDef hdma_spdif_rx_cs;
extern DMA_HandleTypeDef hdma_spdif_rx_dt;

extern UART_HandleTypeDef huart4;

extern MDMA_HandleTypeDef hmdma_mdma_channel40_sw_0;
/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */
//quick error-return macro
#define ReturnOnError(x) do { HAL_StatusTypeDef __res = (x); if (__res != HAL_OK) return __res; } while (0)

//debug printout, disabled outside of debug mode
#ifdef DEBUG
#define DEBUG_PRINTF(...) do { /*printf(__VA_ARGS__);*/ } while (0)
#else
#define DEBUG_PRINTF(...)
#endif

#ifndef MIN
  #define MIN(x,y) ((x) < (y) ? (x) : (y))
#endif

#ifndef MAX
  #define MAX(x,y) ((x) > (y) ? (x) : (y))
#endif

//memory allocation macros
#define __D2_BSS __attribute__((section(".d2_bss")))
#define __D2_DATA __attribute__((section(".d2_data")))
#define __D3_BSS __attribute__((section(".d3_bss")))
#define __D3_DATA __attribute__((section(".d3_data")))
#define __DTCM_BSS __attribute__((section(".dtcm_bss")))
#define __DTCM_DATA __attribute__((section(".dtcm_data")))
#define __ITCM_DATA __attribute__((section(".itcm_data")))
/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define I2C_INT_N_Pin GPIO_PIN_5
#define I2C_INT_N_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */
//period of main program loop in milliseconds
#define MAIN_LOOP_PERIOD_MS 10
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
