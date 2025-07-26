/*
 * power_off_screen.h
 *
 *  Created on: Jul 24, 2025
 *      Author: Alex
 */

#ifndef INC_POWER_OFF_SCREEN_H_
#define INC_POWER_OFF_SCREEN_H_


#include "bbv2_screen.h"


class PowerOffScreen : public BlockBoxV2Screen {
public:
  PowerOffScreen(BlockBoxV2GUIManager& manager);

  void DisplayScreen() override;

  void HandleTouch(const GUITouchState& state) noexcept override;

  void Init() override;

  void ScheduleAutoCalibration();

protected:
  bool auto_calibration_scheduled;
  uint32_t auto_calibration_tick;

  void BuildScreenContent() override;

  void UpdateExistingScreenContent() override;

private:
  //values currently drawn on screen - used to check if we need to redraw on update events
  uint8_t currently_drawn_auto_cal_seconds;

};


#endif /* INC_POWER_OFF_SCREEN_H_ */
