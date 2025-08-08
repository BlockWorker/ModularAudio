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

  void HandleTouch(const GUITouchState& state) noexcept override;

protected:
  void BuildScreenContent() override;
  void UpdateExistingScreenContent() override;

  void OnScreenExit() override;

private:
  uint8_t page_index;

};



#endif /* INC_SETTINGS_SCREEN_AUDIO_H_ */
