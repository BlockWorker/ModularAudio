/*
 * gui_manager.cpp
 *
 *  Created on: Feb 28, 2025
 *      Author: Alex
 */

#include "gui_manager.h"
#include "gui_screen.h"


GUIManager::GUIManager(EVEDriver& driver) noexcept :
    driver(driver), initialised(false), current_screen(NULL), cmd_busy_waiting(false) {}


void GUIManager::InitTouchCalibration() {
  //default behaviour: calibrate touch on every init
  this->driver.ClearDLCmdBuffer();
  this->driver.CmdBeginDisplay(true, true, true, 0x000000);
  this->driver.CmdCalibrate();
  this->driver.CmdEndDisplay();
  this->driver.SendBufferedDLCmds(HAL_MAX_DELAY);
}

void GUIManager::Init() {
  this->initialised = false;

  //start by initialising the display
  uint8_t init_result = this->driver.Init();
  if (init_result != E_OK) {
    InlineFormat(throw DriverError(DRV_FAILED, __msg), "EVE driver init failed with result %d", init_result);
  }

  //set up quad transfer mode
  this->driver.SetTransferMode(TRANSFERMODE_QUAD);

  //increase SPI speed above 11MHz
  this->driver.phy.SetTransferSpeed(TRANSFERSPEED_MAX);

  //make sure the touch panel is calibrated
  this->InitTouchCalibration();

  //reset touch state
  this->touch_state.touched = false;
  this->touch_state.initial = false;
  this->touch_state.long_press = false;
  this->touch_state.long_press_tick = false;
  this->touch_state.released = false;
  this->touch_state._next_tick_at = HAL_MAX_DELAY;
  this->touch_state._debounce_tick = HAL_MAX_DELAY;
  this->touch_state.initial_tag = 0;
  this->touch_state.tag = 0;
  this->touch_state.tracker_tag = 0;
  this->touch_state.tracker_value = 0;

  this->initialised = true;

  //display initial screen, if set
  if (this->current_screen != NULL) {
    this->current_screen->DisplayScreen();
  }
}

void GUIManager::Update() noexcept {
  if (!this->initialised) {
    //uninitialised: nothing to do
    return;
  }

  //check for coprocessor busy state, if we're waiting for that
  if (this->cmd_busy_waiting && this->driver.IsBusy() == E_OK) {
    //no longer busy: check if we have any queued transfers to do
    if (this->queued_cmd_transfers.empty()) {
      //no queued transfers: mark as not busy anymore
      this->cmd_busy_waiting = false;
    } else {
      //we have a queued transfer: remove it from the queue, send it, remain in busy waiting state
      GUICMDTransfer transfer = this->queued_cmd_transfers.front();
      this->queued_cmd_transfers.pop_front();
      this->driver.phy.DirectWriteBuffer(REG_CMDB_WRITE, (const uint8_t*)transfer.data, transfer.length_words * sizeof(uint32_t), 10);
    }
  }

  uint32_t tick = HAL_GetTick();

  //read touch info
  uint8_t tag_read_buf;
  uint32_t touch_read_buf;
  try {
    this->driver.phy.DirectRead32(REG_TOUCH_DIRECT_XY, &touch_read_buf);
    bool new_touched_raw = (touch_read_buf & 0x80000000U) == 0;

    //apply debouncing logic
    bool new_touched;
    if (this->touch_state.touched == new_touched_raw) {
      //touch state same as stored: propagate directly, ensure next change is debounced
      new_touched = new_touched_raw;
      this->touch_state._debounce_tick = tick + GUI_TOUCH_DEBOUNCE_DELAY;
    } else {
      //touch state different from stored: propagate new value only if debounce time elapsed, otherwise old value
      new_touched = ((int32_t)(tick - this->touch_state._debounce_tick) > 0) ? new_touched_raw : this->touch_state.touched;
    }

    //read tag and tracker info if we have a touch
    if (new_touched && new_touched_raw) {
      //get tag and store in touch state
      this->driver.phy.DirectRead8(REG_TOUCH_TAG, &tag_read_buf);
      this->touch_state.tag = tag_read_buf;

      //get tracker tag
      this->driver.phy.DirectRead32(REG_TRACKER, &touch_read_buf);
      this->touch_state.tracker_tag = (uint16_t)touch_read_buf;
      if (this->touch_state.tracker_tag != 0) {
        //tag nonzero: update tracker value
        this->touch_state.tracker_value = (uint16_t)(touch_read_buf >> 16);
      } else {
        //tag zero: discard tracker value
        this->touch_state.tracker_value = 0;
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
        if ((int32_t)(tick - this->touch_state._next_tick_at) > 0) {
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
      this->touch_state.initial_tag = tag_read_buf;
      this->touch_state._next_tick_at = tick + GUI_TOUCH_LONG_DELAY; //set up delay until long press
    }

    //send touch update to screen if there's anything to report
    if (this->current_screen != NULL && (this->touch_state.touched || this->touch_state.released)) {
      this->current_screen->HandleTouch(touch_state);
    }
  } catch (const std::exception& exc) {
    DEBUG_PRINTF("* GUI manager touch update failed: %s\n", exc.what());
  } catch (...) {
    DEBUG_PRINTF("* GUI manager touch update failed with unknown exception\n");
  }

  //perform screen update and redraw, if coprocessor is not busy
  if (this->current_screen != NULL && !this->cmd_busy_waiting) {
    try {
      this->current_screen->DisplayScreen();
    } catch (const std::exception& exc) {
      DEBUG_PRINTF("* GUI manager screen update failed: %s\n", exc.what());
    } catch (...) {
      DEBUG_PRINTF("* GUI manager screen update failed with unknown exception\n");
    }
  }
}


const GUITouchState& GUIManager::GetTouchState() const noexcept {
  return this->touch_state;
}


void GUIManager::SetScreen(GUIScreen* screen) {
  if (screen == NULL) {
    throw std::invalid_argument("GUIManager SetScreen given null pointer");
  }

  this->current_screen = screen;

  //ensure the screen gets updated and drawn
  this->current_screen->needs_display_list_rebuild = true;

  //immediately update and draw screen, if we're initialised and coprocessor is not busy
  if (this->initialised && !this->cmd_busy_waiting) {
    this->current_screen->DisplayScreen();
  }
}


void GUIManager::SendCmdTransferWhenNotBusy(const uint32_t* data, uint32_t length_words) {
  if (data == NULL) {
    throw std::invalid_argument("GUIManager SendCmdTransferWhenNotBusy given null pointer");
  }

  if (length_words == 0) {
    //nothing to do
    return;
  }

  //check if coprocessor is currently busy, to see if we need to wait for the first block
  if (this->driver.IsBusy() != E_OK) {
    this->cmd_busy_waiting = true;
  }

  //process data block-by-block, queueing the blocks if we're busy
  uint32_t words_left = length_words;
  uint32_t offset_words = 0;

  while (words_left > 0) {
    uint32_t block_words = MIN(words_left, 960);

    if (this->cmd_busy_waiting) {
      //already busy: queue block for later transfer
      auto& transfer = this->queued_cmd_transfers.emplace_back();
      transfer.data = data + offset_words;
      transfer.length_words = block_words;
    } else {
      //not busy: transfer directly and set as busy
      this->driver.phy.DirectWriteBuffer(REG_CMDB_WRITE, (const uint8_t*)(data + offset_words), block_words * sizeof(uint32_t), 10);
      this->cmd_busy_waiting = true;
    }

    offset_words += block_words;
    words_left -= block_words;
  }
}
