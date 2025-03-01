/*
 * gui_screen.h
 *
 *  Created on: Feb 27, 2025
 *      Author: Alex
 */

#ifndef INC_GUI_SCREEN_H_
#define INC_GUI_SCREEN_H_

#include "EVE.h"


#ifdef __cplusplus

#include <vector>

extern "C" {
#endif


#ifdef __cplusplus
}

typedef struct {
  bool touched;
  bool initial;
  bool long_press;
  bool long_press_tick;
  bool released;

  uint8_t tag;
  uint16_t tracker_value;

  uint32_t _next_tick_at;
} GUI_TouchState;

class GUI_Screen {
public:
  virtual HAL_StatusTypeDef DisplayScreen();

  virtual void HandleTouch(const GUI_TouchState& state) = 0;

  GUI_Screen(uint32_t display_timeout) : display_timeout(display_timeout) {}

protected:
  std::vector<uint32_t> saved_dl_commands;
  std::vector<uint32_t> command_variable_offsets;
  uint32_t display_timeout;

  void BuildAndSaveDisplayList();
  virtual void BuildScreenContent() = 0;
  virtual void UpdateDisplayList();

private:
  void BuildCommonContent();
};

#endif

#endif /* INC_GUI_SCREEN_H_ */
