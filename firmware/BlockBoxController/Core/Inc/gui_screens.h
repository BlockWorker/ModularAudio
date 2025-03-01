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

  virtual void HandleTouch(const GUI_TouchState& state) override {
    if (state.touched && state.tag == 1) {
      this->slider_pos = state.tracker_value;
    }
  }

protected:
  virtual void BuildScreenContent() override {
    eve_drv.CmdTag(1);
    this->command_variable_offsets.push_back(eve_drv.GetDLBufferSize() + 3);
    eve_drv.CmdSlider(50, 50, 150, 10, 0, slider_pos, 0xFFFFU);
    eve_drv.CmdTrack(45, 45, 160, 20, 1);
  }

  virtual void UpdateDisplayList() override {
    this->GUI_Screen::UpdateDisplayList();

    uint32_t* slider_pos_ptr32 = this->saved_dl_commands.data() + this->command_variable_offsets[0];
    ((uint16_t*)slider_pos_ptr32)[1] = slider_pos;
  }

private:
  uint16_t slider_pos;
};


#endif /* INC_GUI_SCREENS_H_ */
