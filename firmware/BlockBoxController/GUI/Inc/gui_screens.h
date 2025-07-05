/*
 * gui_screens.h
 *
 *  Created on: Feb 28, 2025
 *      Author: Alex
 */

#ifndef INC_GUI_SCREENS_H_
#define INC_GUI_SCREENS_H_


#include "gui_screen.h"


class GUI_Screen_Test : public GUI_Screen {
public:
  GUI_Screen_Test() : GUI_Screen(30), slider_pos(0) {}

  virtual void HandleTouch(const GUI_TouchState& state) noexcept override {
    if (state.touched && state.tag == 1) {
      this->slider_pos = state.tracker_value;
    }
  }

protected:
  virtual void BuildScreenContent() override {
    eve_drv.CmdTag(1);
    this->slider_offset_index = this->SaveNextCommandOffset();
    eve_drv.CmdSlider(50, 50, 150, 10, 0, slider_pos, 0xFFFFU);
    eve_drv.CmdTrack(45, 45, 160, 20, 1);
  }

  virtual void UpdateDisplayList() override {
    if (this->saved_dl_commands.empty()) {
      this->GUI_Screen::UpdateDisplayList();
      return;
    }

    this->ModifyDLCommand16(this->slider_offset_index, 3, 1, slider_pos);
  }

private:
  uint32_t slider_offset_index;
  uint16_t slider_pos;
};


#endif /* INC_GUI_SCREENS_H_ */
