/*
 * settings_screen_audio.h
 *
 *  Created on: Aug 8, 2025
 *      Author: Alex
 */

#ifndef INC_SETTINGS_SCREEN_AUDIO_H_
#define INC_SETTINGS_SCREEN_AUDIO_H_


#include "settings_screen_base.h"


class SettingsScreenAudio : public SettingsScreenBase {
public:
  SettingsScreenAudio(BlockBoxV2GUIManager& manager);

  void DisplayScreen() override;

  void HandleTouch(const GUITouchState& state) noexcept override;

  void Init() override;

protected:
  void BuildScreenContent() override;
  void UpdateExistingScreenContent() override;

  void OnScreenExit() override;

private:
  uint8_t page_index;

  //command offsets for controls etc
  uint32_t min_value_oidx;
  uint32_t max_value_oidx;
  uint32_t step_value_oidx;
  uint32_t loudness_value_oidx;
  uint32_t power_target_value_oidx;
  uint32_t min_slider_oidx;
  uint32_t max_slider_oidx;
  uint32_t step_slider_oidx;
  uint32_t loudness_slider_oidx;
  uint32_t power_target_slider_oidx;

  //local min/max/step values for fast slider adjustments
  float local_min_volume;
  float local_max_volume;
  float local_volume_step;
  float local_loudness;
  float local_power_target;
};



#endif /* INC_SETTINGS_SCREEN_AUDIO_H_ */
