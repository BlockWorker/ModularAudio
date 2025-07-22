/*
 * test_screen.h
 *
 *  Created on: Feb 28, 2025
 *      Author: Alex
 */

#ifndef INC_TEST_SCREEN_H_
#define INC_TEST_SCREEN_H_


#include "bbv2_screen.h"


class TestScreen : public BlockBoxV2Screen {
public:
  TestScreen(BlockBoxV2GUIManager& manager);

  void HandleTouch(const GUITouchState& state) noexcept override;

protected:
  void BuildScreenContent() override;

  void UpdateExistingScreenContent() override;

private:
  uint32_t slider_offset_index;
  uint16_t slider_pos;
};


#endif /* INC_TEST_SCREEN_H_ */
