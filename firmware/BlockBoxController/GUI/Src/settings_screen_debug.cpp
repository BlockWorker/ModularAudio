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
//speaker calibration channel 1 (tweeter)
#define SCREEN_DEBUG_TAG_CAL_CH1 210
//speaker calibration channel 2 (woofer)
#define SCREEN_DEBUG_TAG_CAL_CH2 211
//bluetooth OTA upgrade
#define SCREEN_DEBUG_TAG_BT_OTA 212
//battery learn mode
#define SCREEN_DEBUG_TAG_BAT_LEARN 213
//battery full shutdown
#define SCREEN_DEBUG_TAG_BAT_FULL_SD 214


SettingsScreenDebug::SettingsScreenDebug(BlockBoxV2GUIManager& manager) :
    SettingsScreenBase(manager, SCREEN_DEBUG_TAB_INDEX), page_index(0) {}


void SettingsScreenDebug::HandleTouch(const GUITouchState& state) noexcept {
  switch (state.initial_tag) {
    case SCREEN_SETTINGS_TAG_TAB_5:
      if (state.released && state.tag == state.initial_tag) {
        //go to next page
        this->page_index = (this->page_index + 1) % 2;
        this->needs_display_list_rebuild = true;
      }
      return;
    case SCREEN_DEBUG_TAG_CAL_CH1:
      if (state.released && state.tag == state.initial_tag) {
        if (this->bbv2_manager.system.audio_mgr.GetCalibrationMode() == AUDIO_CAL_CH1) {
          //disable calibration mode
          this->bbv2_manager.system.audio_mgr.SetCalibrationMode(AUDIO_CAL_NONE, [this](bool success) {
            DEBUG_PRINTF("Calibration mode disable success %u\n", success);
            this->needs_display_list_rebuild = true;
          });
        } else {
          //set to ch1 calibration mode
          this->bbv2_manager.system.audio_mgr.SetCalibrationMode(AUDIO_CAL_CH1, [this](bool success) {
            DEBUG_PRINTF("Calibration mode set to ch1 success %u\n", success);
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
            DEBUG_PRINTF("Calibration mode disable success %u\n", success);
            this->needs_display_list_rebuild = true;
          });
        } else {
          //set to ch2 calibration mode
          this->bbv2_manager.system.audio_mgr.SetCalibrationMode(AUDIO_CAL_CH2, [this](bool success) {
            DEBUG_PRINTF("Calibration mode set to ch2 success %u\n", success);
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
            DEBUG_PRINTF("OTA FW upgrade enable success %u\n", success);
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
          DEBUG_PRINTF("Battery full shutdown trigger success %u\n", success);
          this->needs_display_list_rebuild = true;
        });
      }
      return;
    default:
      break;
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
      this->driver.CmdDL(DL_SAVE_CONTEXT);
      this->driver.CmdScissor(5, 77, 310, 158);

      //expanded index - TODO dynamic
      int32_t expanded_index = 15;

      //offsets - TODO dynamic
      uint32_t scroll_offset = 0;
      uint32_t item_offset = scroll_offset / 13;
      uint16_t pixel_offset = scroll_offset % 13;

      this->driver.CmdDL(VERTEX_TRANSLATE_Y(16 * (64 + pixel_offset)));
      this->driver.CmdDL(LINE_WIDTH(16));

      //draw items - TODO expansion of items
      char item_buffer[128] = { 0 };
      int32_t first_entry = (int32_t)(DebugLog::instance.GetEntryCount() - item_offset) - 13;
      int32_t first_valid_entry = (int32_t)DebugLog::instance.GetEntryMinValid();
      for (int i = 12; i >= 0; i--) {
        int32_t index = first_entry + i;
        if (index < first_valid_entry) {
          break;
        }

        DebugScreenMessage msg = DebugLog::PrepareMessage(DebugLog::instance.GetEntry(index));

        bool expanded = (index == expanded_index);

        //background rect for non-info levels
        if (msg.level < DEBUG_INFO) {
          switch (msg.level) {
            case DEBUG_WARNING:
              this->driver.CmdColorRGB(0x505000);
              break;
            case DEBUG_ERROR:
              this->driver.CmdColorRGB(0x804000);
              break;
            case DEBUG_CRITICAL:
            default:
              this->driver.CmdColorRGB(0x800000);
              break;
          }
          this->driver.CmdBeginDraw(EVE_RECTS);
          this->driver.CmdDL(VERTEX2F(16 * 5, 16 * (13 * (i - (expanded ? msg.line_count - 1 : 0)) + 1)));
          this->driver.CmdDL(VERTEX2F(16 * 315, 16 * (13 * (i + 1))));
          this->driver.CmdDL(DL_END);
          this->driver.CmdColorRGB(0xFFFFFF);
        }

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
        this->driver.CmdFGColor(0x808080);
      }
      this->driver.CmdToggle(252, 142, 29, 26, 0, ota_enabled ? 0xFFFF : 0, "Off\xFFOn ");
      if (ota_enabled) {
        this->driver.CmdFGColor(theme_colour_main);
      }
      //battery learn mode toggle - TODO use true state once implemented
      this->driver.CmdTag(SCREEN_DEBUG_TAG_BAT_LEARN);
      this->driver.CmdFGColor(0x808080);
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
}


