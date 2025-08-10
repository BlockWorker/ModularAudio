/*
 * settings_screen_display.h
 *
 *  Created on: Aug 10, 2025
 *      Author: Alex
 */

#ifndef INC_SETTINGS_SCREEN_DISPLAY_H_
#define INC_SETTINGS_SCREEN_DISPLAY_H_


#include "settings_screen_base.h"
#include "rtc_interface.h"


class SettingsScreenDisplay : public SettingsScreenBase {
public:
  SettingsScreenDisplay(BlockBoxV2GUIManager& manager);

  void HandleTouch(const GUITouchState& state) noexcept override;

  void Init() override;

protected:
  void BuildScreenContent() override;
  void UpdateExistingScreenContent() override;

  void OnScreenExit() override;

private:
  uint8_t page_index;

  //command offsets for controls etc
  uint32_t sleep_time_value_oidx;
  uint32_t brightness_slider_oidx;
  uint32_t sleep_time_slider_oidx;

  //local date/time for editing
  RTCDateTime edit_dt;
  bool editing_datetime;

  void EnsureEditingDateTime();
  void ClampEditingDayToMonth();
};


#endif /* INC_SETTINGS_SCREEN_DISPLAY_H_ */
