/*
 * gui_screen.cpp
 *
 *  Created on: Feb 27, 2025
 *      Author: Alex
 */


#include "gui_screen.h"


/**
 * @brief Write this screen to the display (updated to the latest data).
 */
HAL_StatusTypeDef GUI_Screen::DisplayScreen() {
  this->UpdateDisplayList();

  return eve_drv.SendSavedDLCmds(this->saved_dl_commands, this->display_timeout);
}


/**
 * @brief Build the display list for this screen, including common content and screen-specific content, and save it to saved_dl_commands.
 */
void GUI_Screen::BuildAndSaveDisplayList() {
  this->saved_dl_commands.clear();
  this->command_variable_offsets.clear();

  eve_drv.ClearDLCmdBuffer();
  eve_drv.CmdBeginDisplay(true, true, true, 0x000000);

  this->BuildCommonContent();

  this->BuildScreenContent();

  eve_drv.CmdEndDisplay();

  eve_drv.SaveBufferedDLCmds(this->saved_dl_commands);
}

/**
 * @brief Update this screen's display list to the latest data, or build it if not done yet.
 */
void GUI_Screen::UpdateDisplayList() {
  if (this->saved_dl_commands.empty()) {
    this->BuildAndSaveDisplayList();
  }
}


/**
 * @brief Build common display list content for all screens.
 */
void GUI_Screen::BuildCommonContent() {

}

