/*
 * touch_cal_screen.cpp
 *
 *  Created on: Jul 22, 2025
 *      Author: Alex
 */


#include "touch_cal_screen.h"
#include "bbv2_gui_manager.h"
#include "system.h"


//touch lockout after calibration/test, in main loop cycles
#define SCREEN_TOUCH_CAL_LOCKOUT_CYCLES (200 / MAIN_LOOP_PERIOD_MS)

//touch tags for calibration test
#define SCREEN_TOUCH_CAL_TAG_TEST_HIT 2
#define SCREEN_TOUCH_CAL_TAG_TEST_MISS 1


TouchCalScreen::TouchCalScreen(BlockBoxV2GUIManager& manager) :
    BlockBoxV2Screen(manager), state(CAL_START), return_screen(&manager.test_screen), touch_lockout_counter(0) {}


void TouchCalScreen::DisplayScreen() {
  if (this->touch_lockout_counter > 0) {
    this->touch_lockout_counter--;
  }

  switch (this->state) {
    case CAL_WAITING:
    {
      //don't actually draw anything, just check busy state to see if calibration command finished
      uint8_t busy_result = this->driver.IsBusy();
      if (busy_result == EVE_FAULT_RECOVERED) {
        //fault: go back to start state to retry calibration
        this->state = CAL_START;
      } else if (busy_result == E_OK) {
        //not busy: calibration command done, continue to test
        this->touch_lockout_counter = SCREEN_TOUCH_CAL_LOCKOUT_CYCLES;
        this->needs_display_list_rebuild = true;
        this->state = CAL_TEST_1;
      }
      break;
    }
    case CAL_TEST_1:
    case CAL_TEST_2:
    case CAL_TEST_FAILED:
    case CAL_SAVING:
      //base handling
      this->BlockBoxV2Screen::DisplayScreen();
      break;
    default:
    {
      //default = start: prepare calibration command
      this->driver.ClearDLCmdBuffer();
      this->driver.CmdBeginDisplay(true, true, true, 0x000000);
      this->driver.CmdText(160, 120, 27, EVE_OPT_CENTER, "Touch the dot");
      this->driver.CmdCalibrate();
      this->driver.CmdEndDisplay();

      //save commands to temporary vector
      std::vector<uint32_t> cmds;
      this->driver.SaveBufferedDLCmds(cmds);

      //force screen to be awake while calibrating
      this->manager.SetDisplayForceWake(true);

      //send commands to command buffer, without waiting for completion
      this->driver.phy.DirectWriteBuffer(REG_CMDB_WRITE, (const uint8_t*)cmds.data(), cmds.size() * sizeof(uint32_t), 10);

      //go to waiting state
      this->state = CAL_WAITING;
      break;
    }
  }
}


void TouchCalScreen::HandleTouch(const GUITouchState& state) noexcept {
  if (this->state != CAL_TEST_1 && this->state != CAL_TEST_2 && this->state != CAL_TEST_FAILED) {
    //nothing to do in other states
    return;
  }

  if (this->touch_lockout_counter > 0) {
    return;
  }

  if (state.initial) {
    if (state.initial_tag == 2) {
      //button hit: proceed
      switch (this->state) {
        case CAL_TEST_1:
          this->touch_lockout_counter = SCREEN_TOUCH_CAL_LOCKOUT_CYCLES;
          this->needs_display_list_rebuild = true;
          this->state = CAL_TEST_2;
          break;
        case CAL_TEST_2:
        {
          this->needs_display_list_rebuild = true;
          this->state = CAL_SAVING;

          //get touch transform matrix from EVE and save it in the config
          TouchTransformMatrix matrix;
          this->driver.phy.DirectReadBuffer(REG_TOUCH_TRANSFORM_A, (uint8_t*)&matrix, sizeof(TouchTransformMatrix), 5);
          this->bbv2_manager.SetTouchMatrix(matrix);

          //stop forcing the screen to be awake
          this->manager.SetDisplayForceWake(false);

          //start EEPROM write
          this->bbv2_manager.system.eeprom_if.WriteAllDirtySections([&](bool) {
            //once EEPROM write completes (regardless of success): go to specified return screen and reset this screen
            this->GoToScreen(this->return_screen);
            this->state = CAL_START;
          });
          break;
        }
        default:
          break;
      }
    } else {
      //button missed: test failed, restart calibration after touch released
      this->needs_display_list_rebuild = true;
      this->state = CAL_TEST_FAILED;
    }
  } else if (state.released && this->state == CAL_TEST_FAILED) {
    this->state = CAL_START;
  }
}


void TouchCalScreen::Init() {
  //nothing to do - no status bar init either, because we don't draw it
}


void TouchCalScreen::SetReturnScreen(BlockBoxV2Screen* return_screen) {
  if (return_screen == NULL) {
    throw std::invalid_argument("TouchCalScreen SetReturnScreen given null pointer");
  }

  this->return_screen = return_screen;
  this->state = CAL_START;
}


void TouchCalScreen::BuildScreenContent() {
  int16_t button_x, button_y;
  const char* text;

  switch (this->state) {
    case CAL_TEST_1:
      button_x = 20;
      button_y = 20;
      text = "Touch the square";
      break;
    case CAL_TEST_2:
      button_x = 280;
      button_y = 200;
      text = "Touch the square";
      break;
    case CAL_TEST_FAILED:
      button_x = button_y = 0;
      text = "Try again";
      break;
    case CAL_SAVING:
      button_x = button_y = 0;
      text = "Saving calibration...";
      break;
    default:
      return;
  }

  //states after calibration command itself (test1/2, saving): draw text and button if needed
  this->driver.CmdColorRGB(0xFFFFFF);
  this->driver.CmdText(160, 120, 27, EVE_OPT_CENTER, text);
  if (button_x > 0 && button_y > 0) {
    //invisible background touch area (for when you "miss" the button)
    this->driver.CmdDL(CLEAR_TAG(SCREEN_TOUCH_CAL_TAG_TEST_MISS));
    this->driver.CmdDL(CLEAR(0, 0, 1));
    //actual button
    this->driver.CmdFGColor(0x8080FF);
    this->driver.CmdTag(SCREEN_TOUCH_CAL_TAG_TEST_HIT);
    this->driver.CmdButton(button_x, button_y, 20, 20, 26, EVE_OPT_FLAT, "");
  }
}

void TouchCalScreen::UpdateExistingScreenContent() {
  //nothing to do
}
