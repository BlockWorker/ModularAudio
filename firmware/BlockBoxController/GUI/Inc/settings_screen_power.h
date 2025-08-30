/*
 * settings_screen_power.h
 *
 *  Created on: Aug 30, 2025
 *      Author: Alex
 */

#ifndef INC_SETTINGS_SCREEN_POWER_H_
#define INC_SETTINGS_SCREEN_POWER_H_


#include "settings_screen_base.h"
#include "power_manager.h"


class SettingsScreenPower : public SettingsScreenBase {
public:
  SettingsScreenPower(BlockBoxV2GUIManager& manager);

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
  uint32_t asd_time_value_oidx;
  uint32_t in_current_value_oidx;
  uint32_t chg_current_value_oidx;
  uint32_t chg_target_value_oidx;
  uint32_t asd_time_slider_oidx;
  uint32_t in_current_slider_oidx;
  uint32_t chg_current_slider_oidx;
  uint32_t chg_target_slider_oidx;

  //local values for fast slider adjustments
  float local_in_current;
  float local_chg_current;
  PowerChargingTargetState local_chg_target;
};


#endif /* INC_SETTINGS_SCREEN_POWER_H_ */
