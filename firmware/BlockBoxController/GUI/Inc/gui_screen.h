/*
 * gui_screen.h
 *
 *  Created on: Feb 27, 2025
 *      Author: Alex
 */

#ifndef INC_GUI_SCREEN_H_
#define INC_GUI_SCREEN_H_

#include "cpp_main.h"
#include "EVE.h"
#include "gui_manager.h"


#ifdef __cplusplus

#include <vector>

extern "C" {
#endif


#ifdef __cplusplus
}


class GUIScreen {
public:
  EVEDriver& driver;
  GUIManager& manager;

  virtual void DisplayScreen();

  virtual void HandleTouch(const GUITouchState& state) noexcept = 0;

  GUIScreen(GUIManager& manager, uint32_t display_timeout) noexcept;

protected:
  std::vector<uint32_t> saved_dl_commands;
  std::vector<uint32_t> dl_command_offsets;
  uint32_t display_timeout;
  bool needs_display_list_rebuild;

  void BuildAndSaveDisplayList();
  virtual void BuildScreenContent() = 0;
  virtual void UpdateExistingScreenContent() = 0;
  virtual void UpdateDisplayList();

  uint32_t SaveNextCommandOffset();
  void ModifyDLCommand32(uint32_t cmd_offset_index, int32_t word_offset, uint32_t value) noexcept;
  void ModifyDLCommand16(uint32_t cmd_offset_index, int32_t word_offset, uint8_t half_word_offset, uint16_t value) noexcept;

private:
  void BuildCommonContent();
};


#endif

#endif /* INC_GUI_SCREEN_H_ */
