/*
 * gui_manager.h
 *
 *  Created on: Feb 27, 2025
 *      Author: Alex
 */

#ifndef INC_GUI_MANAGER_H_
#define INC_GUI_MANAGER_H_


#include "EVE.h"
#include "gui_screens.h"


//time of touch long-press, in milliseconds
#define GUI_TOUCH_LONG_DELAY 1500
//period of long-press touch "ticks", in milliseconds
#define GUI_TOUCH_TICK_PERIOD 500


#ifdef __cplusplus

class GUI_Manager {
public:
  GUI_Manager(GUI_Screen& init_screen) noexcept : current_screen(init_screen) {}

  void Init();
  void Update() noexcept;

  void SetScreen(GUI_Screen& screen);

private:
  GUI_Screen& current_screen;
  GUI_TouchState touch_state;

};

#endif


#endif /* INC_GUI_MANAGER_H_ */
