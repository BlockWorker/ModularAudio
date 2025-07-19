/*
 * gui_screens.h
 *
 *  Created on: Feb 28, 2025
 *      Author: Alex
 */

#ifndef INC_GUI_SCREENS_H_
#define INC_GUI_SCREENS_H_


#include "bbv2_screen.h"
#include "gui_common_draws.h"


class GUIScreenTest : public BlockBoxV2Screen {
public:
  GUIScreenTest(GUIManager& manager) : BlockBoxV2Screen(manager, 30), slider_pos(0) {}

  virtual void HandleTouch(const GUITouchState& state) noexcept override {
    if (state.touched && state.tag == 1) {
      this->slider_pos = state.tracker_value;
    }
  }

protected:
  virtual void BuildScreenContent() override {
    this->driver.CmdTag(1);
    this->slider_offset_index = this->SaveNextCommandOffset();
    this->driver.CmdSlider(50, 60, 150, 10, 0, slider_pos, 0xFFFFU);
    this->driver.CmdTrack(45, 55, 160, 20, 1);

    this->BlockBoxV2Screen::BuildScreenContent();

    //testing icons
    GUIDraws::BluetoothIconSmall(this->driver, 20, 100);
    GUIDraws::USBIconSmall(this->driver, 40, 100);
    GUIDraws::SPDIFIconSmall(this->driver, 60, 100, 0x000000);
  }

  virtual void UpdateExistingScreenContent() override {
    this->ModifyDLCommand16(this->slider_offset_index, 3, 1, slider_pos);
  }

private:
  uint32_t slider_offset_index;
  uint16_t slider_pos;
};


#endif /* INC_GUI_SCREENS_H_ */
