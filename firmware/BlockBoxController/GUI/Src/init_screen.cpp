/*
 * init_screen.cpp
 *
 *  Created on: Jul 20, 2025
 *      Author: Alex
 */


#include "init_screen.h"


InitScreen::InitScreen(BlockBoxV2GUIManager& manager) :
    BlockBoxV2Screen(manager), init_progress_string("Initialising GUI..."), init_error(false) {
  this->status_text_override = "BlockBox v2 neo";
}


void InitScreen::UpdateProgressString(const char* progress_string, bool error) {
  this->init_progress_string = progress_string;
  this->init_error = error;
  this->needs_display_list_rebuild = true;
}


void InitScreen::BuildScreenContent() {
  this->driver.CmdTag(0);

  //progress string
  this->driver.CmdColorRGB(0xFFFFFF);
  this->driver.CmdText(160, 170, 27, EVE_OPT_CENTERX, this->init_progress_string);

  //top status bar
  this->BlockBoxV2Screen::BuildScreenContent();

  //spinner
  if (this->init_error) {
    //error colour if needed
    this->driver.CmdColorRGB(0xFF4040);
  }
  this->driver.CmdSpinner(160, 120, 0, 0);
}

void InitScreen::UpdateExistingScreenContent() {
  //nothing to do
}
