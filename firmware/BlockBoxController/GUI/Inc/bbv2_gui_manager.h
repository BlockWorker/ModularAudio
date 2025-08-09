/*
 * bbv2_gui_manager.h
 *
 *  Created on: Jul 20, 2025
 *      Author: Alex
 */

#ifndef INC_BBV2_GUI_MANAGER_H_
#define INC_BBV2_GUI_MANAGER_H_


#include "gui_manager.h"
#include "storage.h"
#include "touch_cal_screen.h"
#include "init_screen.h"
#include "power_off_screen.h"
#include "main_screen.h"
#include "settings_screen_audio.h"
#include "test_screen.h"


#ifdef __cplusplus


//EVE touch transform matrix
typedef struct {
  int32_t a;
  int32_t b;
  int32_t c;
  int32_t d;
  int32_t e;
  int32_t f;
} TouchTransformMatrix;

static_assert(sizeof(TouchTransformMatrix) == 24);


class BlockBoxV2System;


class BlockBoxV2GUIManager : public GUIManager {
public:
  BlockBoxV2System& system;

  TouchCalScreen touch_cal_screen;
  InitScreen init_screen;

  PowerOffScreen power_off_screen;
  MainScreen main_screen;

  SettingsScreenAudio settings_screen_audio;

  TestScreen test_screen;


  BlockBoxV2GUIManager(BlockBoxV2System& system) noexcept;

  void Init() override;


  TouchTransformMatrix GetTouchMatrix() const;
  uint32_t GetThemeColorMain() const;
  uint32_t GetThemeColorDark() const;

  void SetTouchMatrix(TouchTransformMatrix matrix);
  void SetThemeColors(uint32_t main, uint32_t dark);

  void SetDisplayBrightness(uint8_t brightness) noexcept override;
  void SetDisplaySleepTimeoutMS(uint32_t timeout_ms) noexcept override;

  //updates the init progress message, or proceeds past init if given null pointer
  void SetInitProgress(const char* progress_string, bool error);

protected:
  StorageSection gui_config;

  static void LoadConfigDefaults(StorageSection& section);

  void InitTouchCalibration() override;
};


#endif


#endif /* INC_BBV2_GUI_MANAGER_H_ */
