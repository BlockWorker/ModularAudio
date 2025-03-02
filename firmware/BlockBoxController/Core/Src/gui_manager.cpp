/*
 * gui_manager.cpp
 *
 *  Created on: Feb 28, 2025
 *      Author: Alex
 */

#include "gui_manager.h"


HAL_StatusTypeDef GUI_Manager::Init() {
  //start by initialising the display
  uint8_t init_result;
  ReturnOnError(eve_drv.Init(&init_result));
  if (init_result != E_OK) {
    return HAL_ERROR;
  }

  //set up quad transfer mode - TODO: enable when logic analyser debug not required anymore
  //ReturnOnError(eve_drv.SetTransferMode(TRANSFERMODE_QUAD));

  //TODO: increase SPI speed above 11MHz here if desired

  //calibrate touch - TODO: save to flash and restore if saved
  eve_drv.ClearDLCmdBuffer();
  eve_drv.CmdBeginDisplay(true, true, true, 0x000000);
  eve_drv.CmdCalibrate();
  eve_drv.CmdEndDisplay();
  eve_drv.SendBufferedDLCmds(HAL_MAX_DELAY); //TODO: is this wait okay in an overall system context? if yes, keep in mind for other init, if no, implement alternative approach

  //reset touch state
  this->touch_state.touched = false;
  this->touch_state.initial = false;
  this->touch_state.long_press = false;
  this->touch_state.long_press_tick = false;
  this->touch_state.released = false;
  this->touch_state._next_tick_at = HAL_MAX_DELAY;
  this->touch_state.tag = 0;
  this->touch_state.tracker_value = 0;

  //display initial screen
  return this->current_screen.DisplayScreen();
}

void GUI_Manager::Update() {
  uint32_t tick = HAL_GetTick();

  //read touch info
  uint8_t tag_read_buf;
  uint32_t touch_read_buf;
  if (eve_drv.phy.DirectRead32(REG_TOUCH_DIRECT_XY, &touch_read_buf) == HAL_OK) {
    bool new_touched = (touch_read_buf & 0x80000000U) == 0;

    //read tag and tracker info if we have a touch
    if (new_touched) {
      if (eve_drv.phy.DirectRead8(REG_TOUCH_TAG, &tag_read_buf) == HAL_OK) {
        //store tag in touch state
        this->touch_state.tag = tag_read_buf;

        if (eve_drv.phy.DirectRead32(REG_TRACKER, &touch_read_buf) == HAL_OK) {
          //get tracker tag and compare it against directly reported tag
          uint16_t tracker_tag = (uint16_t)touch_read_buf;
          if (tracker_tag == this->touch_state.tag) {
            //tag matches: store tracker value
            this->touch_state.tracker_value = (uint16_t)(touch_read_buf >> 16);
          } else {
            //tag doesn't match: discard tracker value
            this->touch_state.tracker_value = 0;
          }
        }
      }
    }

    //clear initial, release, and tick flags - only set for one update if applicable
    this->touch_state.initial = false;
    this->touch_state.long_press_tick = false;
    this->touch_state.released = false;

    //check if we already had a touch during the last update
    if (this->touch_state.touched) {
      //touch before: check for hold or release
      if (new_touched) {
        //still touched: check for long-press/tick
        if (tick > this->touch_state._next_tick_at) {
          this->touch_state.long_press = true;
          this->touch_state.long_press_tick = true;
          this->touch_state._next_tick_at = tick + GUI_TOUCH_TICK_PERIOD; //set up delay until next tick
        }
      } else {
        //no longer touched: release, keep long_press flag intact (to differentiate short-press release and long-press release)
        this->touch_state.touched = false;
        this->touch_state.released = true;
      }
    } else if (new_touched) {
      //no touch before, but there is one now: mark as initial touch
      this->touch_state.touched = true;
      this->touch_state.initial = true;
      this->touch_state.long_press = false;
      this->touch_state._next_tick_at = tick + GUI_TOUCH_LONG_DELAY; //set up delay until long press
    }

    //send touch update to screen if there's anything to report
    if (this->touch_state.touched || this->touch_state.released) {
      this->current_screen.HandleTouch(touch_state);
    }
  }

  //perform screen update and redraw
  this->current_screen.DisplayScreen();
}


void GUI_Manager::SetScreen(GUI_Screen& screen) {
  //TODO
}
