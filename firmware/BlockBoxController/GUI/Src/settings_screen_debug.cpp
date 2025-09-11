/*
 * settings_screen_debug.cpp
 *
 *  Created on: Sep 1, 2025
 *      Author: Alex
 */


#include "settings_screen_debug.h"
#include "bbv2_gui_manager.h"
#include "system.h"


//settings tab index
#define SCREEN_DEBUG_TAB_INDEX 5

//touch tags
//log scrolling area
#define SCREEN_DEBUG_TAG_LOG_SCROLL 200
//log first item (12 more after this)
#define SCREEN_DEBUG_TAG_LOG_FIRST_ITEM 201
//speaker calibration channel 1 (tweeter)
#define SCREEN_DEBUG_TAG_CAL_CH1 220
//speaker calibration channel 2 (woofer)
#define SCREEN_DEBUG_TAG_CAL_CH2 221
//bluetooth OTA upgrade
#define SCREEN_DEBUG_TAG_BT_OTA 222
//battery learn mode
#define SCREEN_DEBUG_TAG_BAT_LEARN 223
//battery full shutdown
#define SCREEN_DEBUG_TAG_BAT_FULL_SD 224

//scroll velocity EMA coefficients
#define SCREEN_DEBUG_SCROLL_EMA_ALPHA 0.125f
#define SCREEN_DEBUG_SCROLL_EMA_1MALPHA (1.0f - SCREEN_DEBUG_SCROLL_EMA_ALPHA)
//scroll velocity slowing factor
#define SCREEN_DEBUG_SCROLL_SLOW_FACTOR 0.95f;


SettingsScreenDebug::SettingsScreenDebug(BlockBoxV2GUIManager& manager) :
    SettingsScreenBase(manager, SCREEN_DEBUG_TAB_INDEX), page_index(0), scroll_last_y(0), scroll_velocity(0.0f), scroll_velocity_active(false), log_start_index(INT32_MAX),
    log_pixel_offset(0), log_expanded_index(-1) {}


void SettingsScreenDebug::DisplayScreen() {
  if (this->page_index == 0) {
    //log page: check for invalid indices
    int32_t min_valid_index = (int32_t)DebugLog::instance.GetEntryMinValid();
    if (this->log_start_index < min_valid_index) {
      this->log_start_index = min_valid_index;
      this->log_pixel_offset = 13;
    }
    if (this->log_expanded_index < min_valid_index) {
      this->log_expanded_index = -1;
    }
    //process scrolling velocity
    if (this->scroll_velocity_active) {
      int16_t int_velocity = (int16_t)roundf(this->scroll_velocity);
      if (int_velocity == 0) {
        //slowed down enough: stop
        this->scroll_velocity_active = false;
        this->scroll_velocity = 0.0f;
      } else {
        //active: scroll and slow down
        this->ApplyScrollDelta(int_velocity);
        this->scroll_velocity *= SCREEN_DEBUG_SCROLL_SLOW_FACTOR;
      }
    }
  }

  //allow base handling
  this->SettingsScreenBase::DisplayScreen();
}


void SettingsScreenDebug::ApplyScrollDelta(int16_t delta) {
  int16_t new_pixel_offset = this->log_pixel_offset + delta;
  int32_t log_end_index = (int32_t)DebugLog::instance.GetEntryCount() - 13;
  if (new_pixel_offset >= 13) {
    //scrolled up beyond item boundary: decrease log start index
    int32_t item_count = new_pixel_offset / 13;
    if (this->log_start_index >= log_end_index) {
      //was at end of log - scroll from actual end
      this->log_start_index = log_end_index - item_count;
      new_pixel_offset %= 13;
    } else if (this->log_start_index - item_count < (int32_t)DebugLog::instance.GetEntryMinValid()) {
      //reached start of log - can't scroll further
      this->log_start_index = DebugLog::instance.GetEntryMinValid();
      new_pixel_offset = 13;
    } else {
      //not at end or start of log - just decrement
      this->log_start_index -= item_count;
      new_pixel_offset %= 13;
    }
  } else if (new_pixel_offset < 0) {
    //scrolled down beyond item boundary: increase log start index
    int32_t item_count = 1 + (new_pixel_offset / -13);
    if (this->log_start_index + item_count >= log_end_index) {
      //reached end of log: enable auto-scrolling
      this->log_start_index = INT32_MAX;
      new_pixel_offset = 0;
    } else if (this->log_start_index < log_end_index) {
      //not reached end of log - just increment
      this->log_start_index += item_count;
      new_pixel_offset %= 13;
      if (new_pixel_offset < 0) {
        new_pixel_offset += 13;
      }
    } else {
      //at end of log: fix offset
      new_pixel_offset = 0;
    }
  }
  this->log_pixel_offset = new_pixel_offset;
  this->needs_display_list_rebuild = true;
}

void SettingsScreenDebug::HandleTouch(const GUITouchState& state) noexcept {
  switch (state.initial_tag) {
    case SCREEN_SETTINGS_TAG_TAB_5:
      if (state.released && state.tag == state.initial_tag) {
        //go to next page
        this->page_index = (this->page_index + 1) % 2;
        //reset log scrolling and expanded message
        this->scroll_velocity = 0.0f;
        this->scroll_velocity_active = false;
        this->log_start_index = INT32_MAX;
        this->log_pixel_offset = 0;
        this->log_expanded_index = -1;
        this->needs_display_list_rebuild = true;
      }
      return;
    case SCREEN_DEBUG_TAG_LOG_SCROLL:
      if (state.y < 0 || state.y > 240) {
        return;
      }
      if (state.initial) {
        //start scrolling: remember initial y position
        this->scroll_last_y = state.y;
        this->scroll_velocity_active = false;
      } else if (state.touched) {
        //scroll: change values accordingly
        this->scroll_velocity_active = false;
        int16_t diff = state.y - this->scroll_last_y;
        this->scroll_last_y = state.y;
        if (diff != 0) {
          this->ApplyScrollDelta(diff);
        }
        this->scroll_velocity = SCREEN_DEBUG_SCROLL_EMA_ALPHA * (float)diff + SCREEN_DEBUG_SCROLL_EMA_1MALPHA * this->scroll_velocity;
      } else if (state.released) {
        //activate velocity-based scroll
        this->scroll_velocity_active = true;
      }
      return;
    case SCREEN_DEBUG_TAG_CAL_CH1:
      if (state.released && state.tag == state.initial_tag) {
        if (this->bbv2_manager.system.audio_mgr.GetCalibrationMode() == AUDIO_CAL_CH1) {
          //disable calibration mode
          this->bbv2_manager.system.audio_mgr.SetCalibrationMode(AUDIO_CAL_NONE, [this](bool success) {
            if (!success) {
              DEBUG_LOG(DEBUG_WARNING, "Calibration mode disable failed");
            }
            this->needs_display_list_rebuild = true;
          });
        } else {
          //set to ch1 calibration mode
          this->bbv2_manager.system.audio_mgr.SetCalibrationMode(AUDIO_CAL_CH1, [this](bool success) {
            if (!success) {
              DEBUG_LOG(DEBUG_WARNING, "Calibration mode set to ch1 failed");
            }
            this->needs_display_list_rebuild = true;
          });
        }
      }
      return;
    case SCREEN_DEBUG_TAG_CAL_CH2:
      if (state.released && state.tag == state.initial_tag) {
        if (this->bbv2_manager.system.audio_mgr.GetCalibrationMode() == AUDIO_CAL_CH2) {
          //disable calibration mode
          this->bbv2_manager.system.audio_mgr.SetCalibrationMode(AUDIO_CAL_NONE, [this](bool success) {
            if (!success) {
              DEBUG_LOG(DEBUG_WARNING, "Calibration mode disable failed");
            }
            this->needs_display_list_rebuild = true;
          });
        } else {
          //set to ch2 calibration mode
          this->bbv2_manager.system.audio_mgr.SetCalibrationMode(AUDIO_CAL_CH2, [this](bool success) {
            if (!success) {
              DEBUG_LOG(DEBUG_WARNING, "Calibration mode set to ch2 failed");
            }
            this->needs_display_list_rebuild = true;
          });
        }
      }
      return;
    case SCREEN_DEBUG_TAG_BT_OTA:
      if (state.released && state.tag == state.initial_tag) {
        //check state - OTA FW upgrade can only be enabled, not disabled (other than by system restart)
        if (!this->bbv2_manager.system.btrx_if.GetStatus().ota_upgrade_enabled) {
          //enable OTA FW upgrade
          this->bbv2_manager.system.btrx_if.EnableOTAUpgrade([this](bool success) {
            if (!success) {
              DEBUG_LOG(DEBUG_WARNING, "OTA FW upgrade enable failed");
            }
            this->needs_display_list_rebuild = true;
          });
        }
      }
      return;
    case SCREEN_DEBUG_TAG_BAT_LEARN:
      if (state.released && state.tag == state.initial_tag) {
        //TODO toggle learn mode once implemented
      }
      return;
    case SCREEN_DEBUG_TAG_BAT_FULL_SD:
      if (state.released && state.tag == state.initial_tag && state.long_press) {
        //on long press: initiate battery full shutdown
        this->bbv2_manager.system.bat_if.TriggerFullShutdown([this](bool success) {
          if (!success) {
            DEBUG_LOG(DEBUG_WARNING, "Battery full shutdown trigger failed");
          }
          this->needs_display_list_rebuild = true;
        });
      }
      return;
    default:
      break;
  }

  if (state.initial_tag >= SCREEN_DEBUG_TAG_LOG_FIRST_ITEM && state.initial_tag < SCREEN_DEBUG_TAG_LOG_FIRST_ITEM + 13) {
    if (state.released && state.tag == state.initial_tag) {
      int32_t index_offset = state.initial_tag - SCREEN_DEBUG_TAG_LOG_FIRST_ITEM;
      int32_t index;
      int32_t log_end_index = (int32_t)DebugLog::instance.GetEntryCount() - 13;
      if (this->log_start_index >= log_end_index) {
        //at end of log: base on log end
        index = log_end_index + index_offset;
      } else {
        //not at end: base on offset
        index = this->log_start_index + index_offset;
      }
      if (this->log_expanded_index == index) {
        //unexpand
        this->log_expanded_index = -1;
      } else {
        //expand
        this->log_expanded_index = index;
      }
      this->needs_display_list_rebuild = true;
    }
    return;
  }

  //base handling: tab switching etc
  this->SettingsScreenBase::HandleTouch(state);
}


void SettingsScreenDebug::Init() {
  //allow base handling
  this->SettingsScreenBase::Init();

  //TODO event handlers if needed
}



void SettingsScreenDebug::BuildScreenContent() {
  this->driver.CmdTag(0);

  uint32_t theme_colour_main = this->bbv2_manager.GetThemeColorMain();
  uint32_t theme_colour_dark = this->bbv2_manager.GetThemeColorDark();

  this->driver.CmdColorRGB(0xFFFFFF);
  this->driver.CmdFGColor(theme_colour_main);
  this->driver.CmdBGColor(0x808080);

  switch (this->page_index) {
    case 0:
    {
      //event log page
      //frame
      this->driver.CmdBeginDraw(EVE_LINE_STRIP);
      this->driver.CmdDL(LINE_WIDTH(8));
      this->driver.CmdDL(VERTEX2F(16 * 4, 16 * 76));
      this->driver.CmdDL(VERTEX2F(16 * 4, 16 * 235));
      this->driver.CmdDL(VERTEX2F(16 * 315, 16 * 235));
      this->driver.CmdDL(VERTEX2F(16 * 315, 16 * 76));
      this->driver.CmdDL(VERTEX2F(16 * 4, 16 * 76));
      this->driver.CmdDL(DL_END);

      //content scissor
      this->driver.CmdTagMask(true);
      this->driver.CmdDL(DL_SAVE_CONTEXT);
      this->driver.CmdScissor(5, 77, 310, 158);

      this->driver.CmdDL(VERTEX_TRANSLATE_Y(16 * (64 + this->log_pixel_offset)));
      this->driver.CmdDL(LINE_WIDTH(16));

      //draw items
      char item_buffer[128] = { 0 };
      int32_t first_entry = (int32_t)DebugLog::instance.GetEntryCount() - 13; //default to end of log
      if (this->log_start_index < INT32_MAX && this->log_start_index < first_entry) {
        //fixed log point
        first_entry = this->log_start_index;
      }
      int32_t first_valid_entry = (int32_t)DebugLog::instance.GetEntryMinValid();
      for (int i = 12; i >= 0; i--) {
        int32_t index = first_entry + i;
        if (index < first_valid_entry) {
          break;
        }

        DebugScreenMessage msg = DebugLog::PrepareMessage(DebugLog::instance.GetEntry(index));

        bool expanded = (index == this->log_expanded_index);

        //background rect
        switch (msg.level) {
          case DEBUG_CRITICAL:
            this->driver.CmdColorRGB(0x800000);
            break;
          case DEBUG_ERROR:
            this->driver.CmdColorRGB(0x804000);
            break;
          case DEBUG_WARNING:
            this->driver.CmdColorRGB(0x505000);
            break;
          case DEBUG_INFO:
          default:
            this->driver.CmdColorRGB(0x101020);
            break;
        }
        this->driver.CmdTag(SCREEN_DEBUG_TAG_LOG_FIRST_ITEM + i);
        this->driver.CmdBeginDraw(EVE_RECTS);
        this->driver.CmdDL(VERTEX2F(16 * 5, 16 * (13 * (i - (expanded ? msg.line_count - 1 : 0)) + 1)));
        this->driver.CmdDL(VERTEX2F(16 * 315, 16 * (13 * (i + 1))));
        this->driver.CmdDL(DL_END);
        this->driver.CmdColorRGB(0xFFFFFF);

        if (expanded) {
          //expanded message text
          for (int j = msg.line_count - 1; j >= 0; j--) {
            uint32_t len = MIN(msg.line_lengths[j], 127);
            strncpy(item_buffer, msg.text + msg.line_starts[j], len);
            item_buffer[len] = 0;
            this->driver.CmdText((j == 0 ? 7 : 15), 13 * i--, 20, 0, item_buffer);
          }
          i++;
          first_entry += (msg.line_count - 1);
        } else {
          //short message text
          if (msg.short_length == 0) {
            strncpy(item_buffer, msg.text, 127);
          } else {
            uint32_t len = MIN(msg.short_length, 124);
            strncpy(item_buffer, msg.text, len);
            strncpy(item_buffer + len, "...", 127 - len);
          }
          item_buffer[127] = 0;
          this->driver.CmdText(7, 13 * i, 20, 0, item_buffer);
        }
      }
      this->driver.CmdDL(DL_RESTORE_CONTEXT);

      //invisible scroll touch area
      this->driver.CmdTagMask(true);
      this->driver.CmdTag(SCREEN_DEBUG_TAG_LOG_SCROLL);
      this->driver.CmdBeginDraw(EVE_RECTS);
      this->driver.CmdDL(LINE_WIDTH(16));
      this->driver.CmdDL(COLOR_MASK(0, 0, 0, 0));
      this->driver.CmdDL(VERTEX2F(16 * 160, 16 * 77));
      this->driver.CmdDL(VERTEX2F(16 * 315, 16 * 235));
      this->driver.CmdDL(DL_END);
      this->driver.CmdDL(COLOR_MASK(1, 1, 1, 1));
      break;
    }
    case 1:
    {
      //debug settings/actions page
      //labels
      this->driver.CmdText(160, 80, 27, EVE_OPT_CENTERX, "Speaker calibration mode");
      this->driver.CmdText(90, 115, 27, EVE_OPT_CENTERY | EVE_OPT_RIGHTX, "Tweeter");
      this->driver.CmdText(230, 115, 27, EVE_OPT_CENTERY | EVE_OPT_RIGHTX, "Woofer");
      this->driver.CmdText(235, 147, 27, EVE_OPT_CENTERY | EVE_OPT_RIGHTX, "Bluetooth OTA FW Upgrade");
      this->driver.CmdText(205, 177, 27, EVE_OPT_CENTERY | EVE_OPT_RIGHTX, "Battery Learn Mode");

      AudioPathCalibrationMode cal_mode = this->bbv2_manager.system.audio_mgr.GetCalibrationMode();
      bool ota_enabled = this->bbv2_manager.system.btrx_if.GetStatus().ota_upgrade_enabled;

      //controls
      this->driver.CmdTagMask(true);
      //calibration mode toggle ch1 (tweeter)
      this->driver.CmdTag(SCREEN_DEBUG_TAG_CAL_CH1);
      this->driver.CmdToggle(107, 110, 29, 26, 0, (cal_mode == AUDIO_CAL_CH1) ? 0xFFFF : 0, "Off\xFFOn ");
      //calibration mode toggle ch2 (woofer)
      this->driver.CmdTag(SCREEN_DEBUG_TAG_CAL_CH2);
      this->driver.CmdToggle(247, 110, 29, 26, 0, (cal_mode == AUDIO_CAL_CH2) ? 0xFFFF : 0, "Off\xFFOn ");
      //bluetooth OTA FW toggle
      this->driver.CmdTag(SCREEN_DEBUG_TAG_BT_OTA);
      if (ota_enabled) {
        this->driver.CmdFGColor(0x606060);
      }
      this->driver.CmdToggle(252, 142, 29, 26, 0, ota_enabled ? 0xFFFF : 0, "Off\xFFOn ");
      if (ota_enabled) {
        this->driver.CmdFGColor(theme_colour_main);
      }
      //battery learn mode toggle - TODO use true state once implemented
      this->driver.CmdTag(SCREEN_DEBUG_TAG_BAT_LEARN);
      this->driver.CmdFGColor(0x606060);
      this->driver.CmdToggle(252, 172, 29, 26, 0, 0, "Off\xFFOn ");
      //battery full shutdown button
      this->driver.CmdTag(SCREEN_DEBUG_TAG_BAT_FULL_SD);
      this->driver.CmdFGColor(theme_colour_dark);
      this->driver.CmdBeginDraw(EVE_RECTS);
      this->driver.CmdDL(LINE_WIDTH(40));
      this->driver.CmdDL(VERTEX2F(16 * 16, 16 * 201));
      this->driver.CmdDL(VERTEX2F(16 * 304, 16 * 224));
      this->driver.CmdDL(DL_END);
      this->driver.CmdButton(15, 200, 290, 25, 27, EVE_OPT_FLAT, "Battery Full Shutdown (long press)");
      break;
    }
    default:
      break;
  }

  this->driver.CmdTag(0);

  //base handling (top status bar and settings tabs)
  this->SettingsScreenBase::BuildScreenContent();
  //add active page dot depending on page index
  switch (this->page_index) {
    case 0:
      this->DrawActivePageDot(246);
      break;
    case 1:
      this->DrawActivePageDot(254);
      break;
    default:
      break;
  }

  //popup overlay, if any
  this->DrawPopupOverlay();
}

void SettingsScreenDebug::UpdateExistingScreenContent() {
  //nothing to do for now
}


void SettingsScreenDebug::OnScreenExit() {
  //reset to first page
  this->page_index = 0;
  //reset log scrolling and expanded message
  this->scroll_velocity = 0.0f;
  this->scroll_velocity_active = false;
  this->log_start_index = INT32_MAX;
  this->log_pixel_offset = 0;
  this->log_expanded_index = -1;
}


