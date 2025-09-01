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

  void HandleTouch(const GUITouchState& state) noexcept override;

  void Init() override;

protected:
  void BuildScreenContent() override;
  void UpdateExistingScreenContent() override;

  void OnScreenExit() override;

private:
  uint8_t page_index;

};


#endif /* INC_SETTINGS_SCREEN_DEBUG_H_ */
