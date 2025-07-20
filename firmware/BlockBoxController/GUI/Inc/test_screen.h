/*
 * test_screen.h
 *
 *  Created on: Feb 28, 2025
 *      Author: Alex
 */

#ifndef INC_TEST_SCREEN_H_
#define INC_TEST_SCREEN_H_


#include "bbv2_screen.h"
#include "gui_common_draws.h"


class TestScreen : public BlockBoxV2Screen {
public:
  TestScreen(BlockBoxV2GUIManager& manager) : BlockBoxV2Screen(manager, 40), slider_pos(0) {}

  void HandleTouch(const GUITouchState& state) noexcept override {
    if (state.touched && state.tag == 1) {
      this->slider_pos = state.tracker_value;
    }
  }

protected:
  void BuildScreenContent() override {
    this->driver.CmdTag(1);
    this->slider_offset_index = this->SaveNextCommandOffset();
    this->driver.CmdSlider(50, 60, 150, 10, 0, slider_pos, 0xFFFFU);
    this->driver.CmdTrack(45, 55, 160, 20, 1);
    this->driver.CmdTag(0);

    //top status bar
    this->BlockBoxV2Screen::BuildScreenContent();

    //testing icons
    GUIDraws::BluetoothIconSmall(this->driver, 20, 100);
    GUIDraws::USBIconSmall(this->driver, 40, 100);
    GUIDraws::SPDIFIconSmall(this->driver, 60, 100, 0x000000);
  }

  void UpdateExistingScreenContent() override {
    this->ModifyDLCommand16(this->slider_offset_index, 3, 1, slider_pos);
  }

private:
  uint32_t slider_offset_index;
  uint16_t slider_pos;
};


#endif /* INC_TEST_SCREEN_H_ */
