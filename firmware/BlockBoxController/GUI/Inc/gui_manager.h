/*
 * gui_manager.h
 *
 *  Created on: Feb 27, 2025
 *      Author: Alex
 */

#ifndef INC_GUI_MANAGER_H_
#define INC_GUI_MANAGER_H_


#include "EVE.h"


//time of touch long-press, in milliseconds
#define GUI_TOUCH_LONG_DELAY 1500
//period of long-press touch "ticks", in milliseconds
#define GUI_TOUCH_TICK_PERIOD 500


#ifdef __cplusplus


typedef struct {
  bool touched;
  bool initial;
  bool long_press;
  bool long_press_tick;
  bool released;

  uint8_t initial_tag;
  uint8_t tag;
  uint16_t tracker_value;

  uint32_t _next_tick_at;
} GUITouchState;


class GUIScreen;


class GUIManager {
public:
  EVEDriver& driver;

  GUIManager(EVEDriver& driver) noexcept;

  void Init();
  void Update() noexcept;

  void SetScreen(GUIScreen* screen);

private:
  bool initialised;
  GUIScreen* current_screen;
  GUITouchState touch_state;

};


#endif


#endif /* INC_GUI_MANAGER_H_ */
