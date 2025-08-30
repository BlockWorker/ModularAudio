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
#include "settings_screen_display.h"
#include "settings_screen_power.h"


//message pop-up reasons (higher bit = higher priority)
#define GUI_POPUP_NONE 0
#define GUI_POPUP_CHG_FAULT ((uint32_t)(1u << 4))
#define GUI_POPUP_AMP_FAULT ((uint32_t)(1u << 8))
#define GUI_POPUP_AMP_SAFETY_ERR ((uint32_t)(1u << 9))
#define GUI_POPUP_AUTO_SHUTDOWN ((uint32_t)(1u << 16))
#define GUI_POPUP_EOD_SHUTDOWN ((uint32_t)(1u << 17))
#define GUI_POPUP_FULL_SHUTDOWN ((uint32_t)(1u << 18))


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
  SettingsScreenDisplay settings_screen_display;
  SettingsScreenPower settings_screen_power;


  static uint32_t ColorHCLToRGB(float hue, float chroma, float luma);


  BlockBoxV2GUIManager(BlockBoxV2System& system) noexcept;

  void Init() override;
  void Update() noexcept override;


  TouchTransformMatrix GetTouchMatrix() const;
  uint32_t GetThemeColorMain() const;
  uint32_t GetThemeColorDark() const;

  void SetTouchMatrix(TouchTransformMatrix matrix);
  void SetThemeColors(uint32_t main, uint32_t dark);

  void SetDisplayBrightness(uint8_t brightness) noexcept override;
  void SetDisplaySleepTimeoutMS(uint32_t timeout_ms) noexcept override;

  //updates the init progress message, or proceeds past init if given null pointer
  void SetInitProgress(const char* progress_string, bool error);

  uint32_t GetPopupStatus() const;
  uint32_t GetHighestPopup() const;
  void ActivatePopups(uint32_t bits);
  void DeactivatePopups(uint32_t bits);

protected:
  StorageSection gui_config;

  uint32_t popup_status;

  static void LoadConfigDefaults(StorageSection& section);

  void InitTouchCalibration() override;
};


#endif


#endif /* INC_BBV2_GUI_MANAGER_H_ */
