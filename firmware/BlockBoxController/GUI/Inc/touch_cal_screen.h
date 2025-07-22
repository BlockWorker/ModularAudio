/*
 * touch_cal_screen.h
 *
 *  Created on: Jul 22, 2025
 *      Author: Alex
 */

#ifndef INC_TOUCH_CAL_SCREEN_H_
#define INC_TOUCH_CAL_SCREEN_H_


#include "bbv2_screen.h"


typedef enum {
  CAL_START,
  CAL_WAITING,
  CAL_TEST_1,
  CAL_TEST_2,
  CAL_TEST_FAILED,
  CAL_SAVING
} TouchCalState;


class TouchCalScreen : public BlockBoxV2Screen {
public:
  TouchCalScreen(BlockBoxV2GUIManager& manager);

  void DisplayScreen() override;

  void HandleTouch(const GUITouchState& state) noexcept override;

  void Init() override;

  void SetReturnScreen(BlockBoxV2Screen* return_screen);

protected:
  void BuildScreenContent() override;
  void UpdateExistingScreenContent() override;

private:
  TouchCalState state;
  BlockBoxV2Screen* return_screen;
  uint32_t touch_lockout_counter;
};


#endif /* INC_TOUCH_CAL_SCREEN_H_ */
