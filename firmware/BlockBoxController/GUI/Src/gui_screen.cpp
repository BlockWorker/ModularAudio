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
void GUIScreen::DisplayScreen() {
  this->UpdateDisplayList();

  this->driver.SendSavedDLCmds(this->saved_dl_commands, this->display_timeout);
}


GUIScreen::GUIScreen(GUIManager& manager, uint32_t display_timeout) noexcept :
    driver(manager.driver), manager(manager), display_timeout(display_timeout), needs_display_list_rebuild(true) {}


/**
 * @brief Build the display list for this screen, including common content and screen-specific content, and save it to saved_dl_commands.
 */
void GUIScreen::BuildAndSaveDisplayList() {
  this->saved_dl_commands.clear();
  this->dl_command_offsets.clear();

  this->driver.ClearDLCmdBuffer();
  this->driver.CmdBeginDisplay(true, true, true, 0x000000);

  this->BuildCommonContent();

  this->BuildScreenContent();

  this->driver.CmdEndDisplay();

  this->driver.SaveBufferedDLCmds(this->saved_dl_commands);
}

/**
 * @brief Update this screen's display list to the latest data, or build it if not done yet.
 */
void GUIScreen::UpdateDisplayList() {
  if (this->needs_display_list_rebuild || this->saved_dl_commands.empty()) {
    //full (re)build needed
    this->needs_display_list_rebuild = false;
    this->BuildAndSaveDisplayList();
  } else {
    //full rebuild not needed: just do any necessary modifications to existing display list values
    this->UpdateExistingScreenContent();
  }
}

/**
 * @brief Save the offset of the next display list command, for later modification of the command's parameters using the ModifyDLCommand32/16 functions.
 * @returns Command offset index of the saved command.
 */
uint32_t GUIScreen::SaveNextCommandOffset() {
  this->dl_command_offsets.push_back(this->driver.GetDLBufferSize());
  return this->dl_command_offsets.size() - 1;
}

/**
 * @brief Modify a 32-bit word in the display list, relative to a saved command offset (at given `cmd_offset_index`), offset by `word_offset` 32-bit words.
 */
void GUIScreen::ModifyDLCommand32(uint32_t cmd_offset_index, int32_t word_offset, uint32_t value) noexcept {
  int32_t total_word_offset = (int32_t)this->dl_command_offsets[cmd_offset_index] + word_offset;
  this->saved_dl_commands[total_word_offset] = value;
}

/**
 * @brief Modify a 16-bit half word in the display list, relative to a saved command offset (at given `cmd_offset_index`), offset by `word_offset` 32-bit words and `half_word_offset` 16-bit half words.
 */
void GUIScreen::ModifyDLCommand16(uint32_t cmd_offset_index, int32_t word_offset, uint8_t half_word_offset, uint16_t value) noexcept {
  int32_t total_word_offset = (int32_t)this->dl_command_offsets[cmd_offset_index] + word_offset;
  ((uint16_t*)(this->saved_dl_commands.data() + total_word_offset))[half_word_offset] = value;
}


/**
 * @brief Build common display list content for all screens.
 */
void GUIScreen::BuildCommonContent() {

}

