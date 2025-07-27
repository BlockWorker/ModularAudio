/*
 * test_screen.cpp
 *
 *  Created on: Jul 22, 2025
 *      Author: Alex
 */


#include "test_screen.h"
#include "bbv2_gui_manager.h"
#include "gui_common_draws.h"


TestScreen::TestScreen(BlockBoxV2GUIManager& manager) :
    BlockBoxV2Screen(manager), slider_pos(0) {}


void TestScreen::HandleTouch(const GUITouchState& state) noexcept {
  if (state.touched && state.tag == 1) {
    this->slider_pos = state.tracker_value;
    this->needs_existing_list_update = true;
  }

  if (state.released && state.tag == 2 && state.initial_tag == 2) {
    this->bbv2_manager.touch_cal_screen.SetReturnScreen(this);
    this->bbv2_manager.SetScreen(&this->bbv2_manager.touch_cal_screen);
  }
}


void TestScreen::BuildScreenContent() {
  this->driver.CmdTag(1);
  this->driver.CmdFGColor(this->bbv2_manager.GetThemeColorMain());
  this->slider_offset_index = this->SaveNextCommandOffset();
  this->driver.CmdSlider(50, 60, 150, 10, 0, this->slider_pos, 0xFFFFU);
  this->driver.CmdTrack(45, 55, 160, 20, 1);
  this->driver.CmdTag(2);
  this->driver.CmdButton(220, 60, 70, 30, 26, EVE_OPT_FLAT, "Calibrate");
  this->driver.CmdTag(0);

  //top status bar
  this->BlockBoxV2Screen::BuildScreenContent();

  //testing icons
  GUIDraws::BluetoothIconSmall(this->driver, 20, 100, 0x8080FF);
  GUIDraws::USBIconSmall(this->driver, 40, 100, 0xFFFF80);
  GUIDraws::SPDIFIconSmall(this->driver, 60, 100, 0xFF80FF, 0x000000);
}


void TestScreen::UpdateExistingScreenContent() {
  this->ModifyDLCommand16(this->slider_offset_index, 3, 1, this->slider_pos);
}
