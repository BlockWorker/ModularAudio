/*
 * cpp_main.cpp
 *
 *  Created on: Feb 23, 2025
 *      Author: Alex
 */

#include "cpp_main.h"
#include "retarget.h"
#include "EVE.h"
#include "gui_manager.h"
#include "module_interface.h"
#include "system.h"


#undef MAIN_LOOP_PERFORMANCE_MONITOR
//#define MAIN_LOOP_PERFORMANCE_MONITOR

#undef MAIN_HEAP_MONITOR
//#define MAIN_HEAP_MONITOR


BlockBoxV2System bbv2_system;
System& main_system = bbv2_system;


//memory monitoring helpers
extern "C" {
ptrdiff_t _mem_get_max_heap_size();
ptrdiff_t _mem_get_free_heap_size();
}


static uint32_t single_err_count = 0;
static uint32_t double_err_count = 0;


static inline void _RefreshWatchdogs() {

}


int cpp_main() {
  //init block - on exception, fail to error handler
  try {
    RetargetInit(&huart2);

    HAL_Delay(1000);

    DEBUG_PRINTF("Controller started\n");

    HAL_Delay(100);

    main_system.Init();

  } catch (const std::exception& err) {
    DEBUG_PRINTF("*** Init failed with exception: %s\n", err.what());
    HAL_Delay(100);
    Error_Handler();
  } catch (...) {
    DEBUG_PRINTF("*** Unknown exception on init!\n");
    HAL_Delay(100);
    Error_Handler();
  }

#ifdef MAIN_LOOP_PERFORMANCE_MONITOR
  float performance_mean_ticks = 0.0f;
#endif

  //infinite loop
  //iteration counter; may overflow eventually but shouldn't be a problem
  uint32_t loop_count = 0;
  while (1) {
    uint32_t iteration_start_tick = HAL_GetTick();

    //main loop block - on exception, continue to next cycle
    try {
      main_system.LoopTasks();
    } catch (const std::exception& err) {
      DEBUG_LOG(DEBUG_ERROR, "Exception in main loop: %s", err.what());
    } catch (...) {
      DEBUG_LOG(DEBUG_ERROR, "Unknown exception in main loop");
    }


#ifdef MAIN_LOOP_PERFORMANCE_MONITOR
    if (loop_count % 200 == 0) {
      DEBUG_PRINTF("Mean main loop ticks: %.4f\n", performance_mean_ticks);
    }

    performance_mean_ticks = .005f * (float)(HAL_GetTick() - iteration_start_tick) + .995f * performance_mean_ticks;
#endif

#ifdef MAIN_HEAP_MONITOR
    if (loop_count % 500 == 0) {
      ptrdiff_t heap_size = _mem_get_max_heap_size();
      ptrdiff_t heap_free = _mem_get_free_heap_size();
      DEBUG_PRINTF("Free heap: %tu / %tu (%.2f used)\n", heap_free, heap_size, 1.0f - (float)heap_free / (float)heap_size);
    }
#endif

    if (loop_count % 100 == 0) {
      if (single_err_count > 0 || double_err_count > 0) {
        DEBUG_LOG(DEBUG_ERROR, "ECC errors detected: single %lu double %lu", single_err_count, double_err_count);
        single_err_count = 0;
        double_err_count = 0;
      }
    }

    loop_count++;
    _RefreshWatchdogs();
    __enable_irq(); //catch-all, in case interrupts are disabled somewhere and the re-enable is missed
    while((HAL_GetTick() - iteration_start_tick) < MAIN_LOOP_PERIOD_MS); //replaces HAL_Delay() to not wait any unnecessary extra ticks
  }
}


//RAMECC handling
void HAL_RAMECC_DetectErrorCallback(RAMECC_HandleTypeDef *hramecc) {
  if (hramecc->RAMECCErrorCode == HAL_RAMECC_NO_ERROR) {
    return;
  }

  uintptr_t base_address;
  uintptr_t addr_step;
  uintptr_t data_size = 4;
  switch ((uintptr_t)hramecc->Instance) {
    case RAMECC1_Monitor1_BASE:
      base_address = 0x24000000U;
      addr_step = 8;
      data_size = 8;
      break;
    case RAMECC1_Monitor2_BASE:
      base_address = 0x00000000U;
      addr_step = 8;
      data_size = 8;
      break;
    case RAMECC1_Monitor3_BASE:
      base_address = 0x20000000U;
      addr_step = 8;
      break;
    case RAMECC1_Monitor4_BASE:
      base_address = 0x20000004U;
      addr_step = 8;
      break;
    case RAMECC1_Monitor6_BASE:
      base_address = 0x24020000U;
      addr_step = 8;
      break;
    case RAMECC2_Monitor1_BASE:
      base_address = 0x30000000U;
      addr_step = 4;
      break;
    case RAMECC2_Monitor2_BASE:
      base_address = 0x30004000U;
      addr_step = 4;
      break;
    case RAMECC3_Monitor1_BASE:
      base_address = 0x38000000U;
      addr_step = 4;
      break;
    default:
      return;
  }

  uintptr_t address = base_address + addr_step * HAL_RAMECC_GetFailingAddress(hramecc);

  if ((hramecc->RAMECCErrorCode & HAL_RAMECC_DOUBLEERROR_DETECTED) != 0) {
    DEBUG_PRINTF("***** ECC Double Error %p\n", (void*)address);
    double_err_count++;
  } else if ((hramecc->RAMECCErrorCode & HAL_RAMECC_SINGLEERROR_DETECTED) != 0) {
    //DEBUG_PRINTF("*** ECC Single Error %p\n", (void*)address);
    single_err_count++;

    //write back corrected value, if possible
    if (address >= 0x20000000) {
      switch (data_size) {
        case 4:
        default:
        {
          uint32_t read_data = *(uint32_t*)address;
          *(uint32_t volatile*)address = read_data;
          break;
        }
        case 8:
        {
          uint64_t read_data = *(uint64_t*)address;
          *(uint64_t volatile*)address = read_data;
          break;
        }
      }
    }
  }

  hramecc->RAMECCErrorCode = HAL_RAMECC_NO_ERROR;
}
