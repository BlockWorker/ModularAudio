/*
 * cpp_main.cpp
 *
 *  Created on: Feb 23, 2025
 *      Author: Alex
 */

#include "cpp_main.h"
#include "retarget.h"
#include <string.h>
#include "EVE.h"
#include "gui_manager.h"


static GUI_Screen_Test test_screen;
static GUI_Manager gui_mgr(test_screen);


int cpp_main() {
  RetargetInit(&huart2);

  HAL_Delay(1000);

  DEBUG_PRINTF("Controller started\n");

  HAL_Delay(100);

  if (gui_mgr.Init() == HAL_OK) {
    DEBUG_PRINTF("GUI Init complete\n");
  } else {
    DEBUG_PRINTF("*** GUI Init failed!\n");
  }

  while (1) {
    HAL_Delay(50);
    gui_mgr.Update();
  }
}


//old code for OSPI testing
#if 0
  HAL_Delay(100);

  if (eve_phy.SendHostCommand(0x49, 0x40) == HAL_OK) {
    DEBUG_PRINTF("Host command sent\n");
  } else {
    DEBUG_PRINTF("*** Host command send failed!!\n");
  }

  HAL_Delay(100);

  if (eve_phy.EnsureMMapMode(MMAP_FUNC_RAM) == HAL_OK) {
    DEBUG_PRINTF("MMap enabled\n");
  } else {
    DEBUG_PRINTF("*** MMap enable failed!!\n");
  }

  HAL_Delay(10);

  uint32_t temp = MMAP_REG_PWM_DUTY;

  HAL_Delay(10);

  *(uint32_t*)&MMAP_REG_PWM_DUTY = 0x12345678;
  HAL_Delay(1);
  MMAP_REG_PWM_DUTY = 0x55;
  HAL_Delay(1);
  *(uint16_t*)&MMAP_REG_PWM_DUTY = 0xAABB;

  HAL_Delay(10);

  temp = MMAP_REG_ID;

  HAL_Delay(10);

  uint32_t buf[] = { 0x00112233, 0x44556677, 0x8899AABB, 0xCCDDEEFF, 0x00112233, 0x44556677, 0x8899AABB, 0xCCDDEEFF };
  memcpy((void*)(0x90308000UL), buf, 32);

  HAL_Delay(10);

  for (int i = 0; i < 10; i++) {
    if (eve_phy.DirectRead8(REG_ID, (uint8_t*)&temp) != HAL_OK) {
      DEBUG_PRINTF("*** Direct Read 8 failed!!\n");
    }
  }

  if (eve_phy.SetTransferMode(TRANSFERMODE_QUAD) == HAL_OK) {
    DEBUG_PRINTF("Quad mode enabled\n");
  } else {
    DEBUG_PRINTF("*** Quad mode enable failed!!\n");
  }

  if (eve_phy.DirectRead16(REG_ID, (uint16_t*)&temp) != HAL_OK) {
    DEBUG_PRINTF("*** Direct Read 16 failed!!\n");
  }

  if (eve_phy.EnsureMMapMode(MMAP_FUNC_RAM) == HAL_OK) {
    DEBUG_PRINTF("MMap re-enabled\n");
  } else {
    DEBUG_PRINTF("*** MMap re-enable failed!!\n");
  }

  HAL_Delay(10);

  MMAP_REG_PWM_DUTY = 2;
  *(&MMAP_REG_PWM_DUTY + 1) = 3;
  *(&MMAP_REG_PWM_DUTY + 2) = temp;
  temp = *(&MMAP_REG_ID + 1);
  if (eve_phy.DirectRead32(REG_TOUCH_TRANSFORM_A, &temp) == HAL_OK) {
    DEBUG_PRINTF("Direct Read 32 success\n");
  } else {
    DEBUG_PRINTF("*** Direct Read 32 failed!!\n");
  }
  if (eve_phy.EnsureMMapMode(MMAP_FUNC_RAM) == HAL_OK) {
    DEBUG_PRINTF("MMap re-enabled\n");
  } else {
    DEBUG_PRINTF("*** MMap re-enable failed!!\n");
  }
  temp = *(&MMAP_REG_ID + 2);

  if (eve_phy.SetTransferMode(TRANSFERMODE_SINGLE) == HAL_OK) {
    DEBUG_PRINTF("Quad mode disabled\n");
  } else {
    DEBUG_PRINTF("*** Quad mode disable failed!!\n");
  }

  temp = MMAP_REG_PWM_DUTY;

  MMAP_REG_PWM_DUTY = 1;
  MMAP_REG_ID = 5;
  MMAP_REG_PWM_DUTY = 22;

  if (eve_phy.EndMMap() == HAL_OK) {
    DEBUG_PRINTF("MMap aborted\n");
  } else {
    DEBUG_PRINTF("*** MMap abort failed!!\n");
  }

  if (eve_phy.DirectRead8(REG_ID, (uint8_t*)&temp) == HAL_OK) {
    DEBUG_PRINTF("Direct Read 8 success\n");
  } else {
    DEBUG_PRINTF("*** Direct Read 8 failed!!\n");
  }

  if (eve_phy.DirectWrite16(REG_ID, 0x1234) == HAL_OK) {
    DEBUG_PRINTF("Direct Write 16 success\n");
  } else {
    DEBUG_PRINTF("*** Direct Write 16 failed!!\n");
  }
#endif
