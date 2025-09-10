/*
 * settings_screen_led.h
 *
 *  Created on: Sep 10, 2025
 *      Author: Alex
 */

#ifndef INC_SETTINGS_SCREEN_LED_H_
#define INC_SETTINGS_SCREEN_LED_H_


#include "settings_screen_base.h"


class SettingsScreenLED : public SettingsScreenBase {
public:
  SettingsScreenLED(BlockBoxV2GUIManager& manager);

  void HandleTouch(const GUITouchState& state) noexcept override;

  void Init() override;

protected:
  void BuildScreenContent() override;
  void UpdateExistingScreenContent() override;

  void OnScreenExit() override;

private:
  //uint8_t page_index;

  //command offsets for controls etc
  uint32_t brightness_slider_oidx;
};


#endif /* INC_SETTINGS_SCREEN_LED_H_ */
