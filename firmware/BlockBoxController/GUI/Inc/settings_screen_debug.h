/*
 * settings_screen_debug.h
 *
 *  Created on: Sep 1, 2025
 *      Author: Alex
 */

#ifndef INC_SETTINGS_SCREEN_DEBUG_H_
#define INC_SETTINGS_SCREEN_DEBUG_H_


#include "settings_screen_base.h"


class SettingsScreenDebug : public SettingsScreenBase {
public:
  SettingsScreenDebug(BlockBoxV2GUIManager& manager);

  void DisplayScreen() override;

  void HandleTouch(const GUITouchState& state) noexcept override;

  void Init() override;

protected:
  void BuildScreenContent() override;
  void UpdateExistingScreenContent() override;

  void OnScreenExit() override;

private:
  uint8_t page_index;

  //scroll touch tracking coordinate
  int16_t scroll_last_y;
  float scroll_velocity;
  bool scroll_velocity_active;
  //log scrolling parameters
  int32_t log_start_index;
  int16_t log_pixel_offset;
  //log expanded message
  int32_t log_expanded_index;


  void ApplyScrollDelta(int16_t delta);
};


#endif /* INC_SETTINGS_SCREEN_DEBUG_H_ */
