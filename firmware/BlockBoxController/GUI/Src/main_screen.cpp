/*
 * main_screen.cpp
 *
 *  Created on: Jul 25, 2025
 *      Author: Alex
 */


#include "main_screen.h"
#include "bbv2_gui_manager.h"
#include "gui_common_draws.h"





MainScreen::MainScreen(BlockBoxV2GUIManager& manager) :
    BlockBoxV2Screen(manager, 50) {}


void MainScreen::DisplayScreen() {
  //TODO update handling

  //base handling
  this->BlockBoxV2Screen::DisplayScreen();
}


void MainScreen::HandleTouch(const GUITouchState& state) noexcept {
  //potential button press
  if (state.released && state.tag == state.initial_tag) {

  }
}


void MainScreen::Init() {
  //allow base handling
  this->BlockBoxV2Screen::Init();

  //TODO: register update handlers to redraw screen
}


void MainScreen::BuildScreenContent() {
  this->driver.CmdTag(0);



  //top status bar
  this->BlockBoxV2Screen::BuildScreenContent();

  //invisible touch areas
  this->driver.CmdDL(DL_SAVE_CONTEXT);
  this->driver.CmdBeginDraw(EVE_RECTS);
  this->driver.CmdDL(LINE_WIDTH(16));
  this->driver.CmdDL(COLOR_MASK(0, 0, 0, 0));
  this->driver.CmdTagMask(true);

  this->driver.CmdDL(DL_END);
  this->driver.CmdDL(DL_RESTORE_CONTEXT);
}


void MainScreen::UpdateExistingScreenContent() {
  //TODO non-redraw updates
}

