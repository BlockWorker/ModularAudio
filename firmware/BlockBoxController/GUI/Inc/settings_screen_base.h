/*
 * settings_screen_base.h
 *
 *  Created on: Aug 8, 2025
 *      Author: Alex
 */

#ifndef INC_SETTINGS_SCREEN_BASE_H_
#define INC_SETTINGS_SCREEN_BASE_H_


#include "bbv2_screen.h"


//touch tags for tabs
#define SCREEN_SETTINGS_TAG_TAB_0 30
#define SCREEN_SETTINGS_TAG_TAB_1 31
#define SCREEN_SETTINGS_TAG_TAB_2 32
#define SCREEN_SETTINGS_TAG_TAB_3 33
#define SCREEN_SETTINGS_TAG_TAB_4 34
#define SCREEN_SETTINGS_TAG_TAB_5 35
#define SCREEN_SETTINGS_TAG_CLOSE 39


class SettingsScreenBase : public BlockBoxV2Screen {
public:
  const uint8_t tab_index;

  SettingsScreenBase(BlockBoxV2GUIManager& manager, uint8_t tab_index);

  void HandleTouch(const GUITouchState& state) noexcept override;

protected:
  void BuildScreenContent() override;

  void DrawActivePageDot(uint16_t x_pos);

};


#endif /* INC_SETTINGS_SCREEN_BASE_H_ */
