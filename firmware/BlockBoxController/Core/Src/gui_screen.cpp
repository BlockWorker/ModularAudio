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
  this->dl_command_offsets.clear();

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
 * @brief Save the offset of the next display list command, for later modification of the command's parameters using the ModifyDLCommand32/16 functions.
 * @returns Command offset index of the saved command.
 */
uint32_t GUI_Screen::SaveNextCommandOffset() {
  this->dl_command_offsets.push_back(eve_drv.GetDLBufferSize());
  return this->dl_command_offsets.size() - 1;
}

/**
 * @brief Modify a 32-bit word in the display list, relative to a saved command offset (at given `cmd_offset_index`), offset by `word_offset` 32-bit words.
 */
void GUI_Screen::ModifyDLCommand32(uint32_t cmd_offset_index, int32_t word_offset, uint32_t value) {
  int32_t total_word_offset = (int32_t)this->dl_command_offsets[cmd_offset_index] + word_offset;
  this->saved_dl_commands[total_word_offset] = value;
}

/**
 * @brief Modify a 16-bit half word in the display list, relative to a saved command offset (at given `cmd_offset_index`), offset by `word_offset` 32-bit words and `half_word_offset` 16-bit half words.
 */
void GUI_Screen::ModifyDLCommand16(uint32_t cmd_offset_index, int32_t word_offset, uint8_t half_word_offset, uint16_t value) {
  int32_t total_word_offset = (int32_t)this->dl_command_offsets[cmd_offset_index] + word_offset;
  ((uint16_t*)(this->saved_dl_commands.data() + total_word_offset))[half_word_offset] = value;
}


/**
 * @brief Build common display list content for all screens.
 */
void GUI_Screen::BuildCommonContent() {

}

