/*
 * main_screen.cpp
 *
 *  Created on: Jul 25, 2025
 *      Author: Alex
 */


#include "main_screen.h"
#include "bbv2_gui_manager.h"
#include "gui_common_draws.h"
#include "system.h"


//maximum width of info labels, in pixels
#define SCREEN_MAIN_INFO_MAX_WIDTH 300
//maximum width of dropdown Bluetooth button label, in pixels
#define SCREEN_MAIN_DROPDOWN_BT_BUTTON_MAX_WIDTH 260
//maximum width of dropdown Bluetooth codec name, in pixels
#define SCREEN_MAIN_DROPDOWN_BT_CODEC_MAX_WIDTH 95

//touch tags
//media back
#define SCREEN_MAIN_TAG_MEDIA_BACK 10
//media play/pause
#define SCREEN_MAIN_TAG_MEDIA_PLAYPAUSE 11
//media forward
#define SCREEN_MAIN_TAG_MEDIA_FORWARD 12
//volume down
#define SCREEN_MAIN_TAG_VOL_DOWN 13
//mute/unmute
#define SCREEN_MAIN_TAG_MUTE_UNMUTE 14
//volume up
#define SCREEN_MAIN_TAG_VOL_UP 15
//bluetooth button
#define SCREEN_MAIN_TAG_BLUETOOTH 16
//light button
#define SCREEN_MAIN_TAG_LIGHT 17
//settings button
#define SCREEN_MAIN_TAG_SETTINGS 18
//power-off button
#define SCREEN_MAIN_TAG_POWER_OFF 19
//input dropdown open/close
#define SCREEN_MAIN_TAG_DROPDOWN_TRIGGER 250
//input dropdown Bluetooth select
#define SCREEN_MAIN_TAG_DROPDOWN_BT_SELECT 251
//input dropdown Bluetooth pair/stop/disconnect
#define SCREEN_MAIN_TAG_DROPDOWN_BT_ACTION 252
//input dropdown USB select
#define SCREEN_MAIN_TAG_DROPDOWN_USB_SELECT 253
//input dropdown SPDIF select
#define SCREEN_MAIN_TAG_DROPDOWN_SPDIF_SELECT 254


//TODO temporary bool
static bool _temp_light_on = false;


MainScreen::MainScreen(BlockBoxV2GUIManager& manager) :
    BlockBoxV2Screen(manager), currently_drawn_input(AUDIO_INPUT_NONE), currently_drawn_streaming(false), currently_drawn_input_rate(0),
    currently_drawn_mute(true), currently_drawn_volume_dB(0), dropdown_open(false) {
  this->currently_drawn_bt_status.value = 0;
}


void MainScreen::DisplayScreen() {
  //TODO update handling

  //base handling
  this->BlockBoxV2Screen::DisplayScreen();
}


void MainScreen::HandleTouch(const GUITouchState& state) noexcept {
  //potential button press
  if (state.released && state.tag == state.initial_tag) {
    BluetoothReceiverStatus bt_status = this->bbv2_manager.system.btrx_if.GetStatus();
    if (this->dropdown_open) {
      //dropdown touch inputs
      AudioPathInput active_input = this->bbv2_manager.system.audio_mgr.GetActiveInput();
      switch (state.tag) {
        case SCREEN_MAIN_TAG_DROPDOWN_TRIGGER:
          //close input dropdown
          this->dropdown_open = false;
          this->needs_display_list_rebuild = true;
          break;
        case SCREEN_MAIN_TAG_DROPDOWN_BT_SELECT:
          //activate Bluetooth input, if available and not already active
          if (this->bbv2_manager.system.audio_mgr.IsInputAvailable(AUDIO_INPUT_BLUETOOTH) && active_input != AUDIO_INPUT_BLUETOOTH) {
            this->bbv2_manager.system.audio_mgr.SetActiveInput(AUDIO_INPUT_BLUETOOTH, [](bool success) {
              if (!success) {
                DEBUG_PRINTF("MainScreen dropdown Bluetooth select failed, might be unavailable\n");
              }
            });
          }
          break;
        case SCREEN_MAIN_TAG_DROPDOWN_BT_ACTION:
          if (this->bbv2_manager.system.audio_mgr.IsInputAvailable(AUDIO_INPUT_BLUETOOTH)) {
            //disconnect active connection
            this->bbv2_manager.system.btrx_if.DisconnectBluetooth([](bool success) {
              if (!success) {
                DEBUG_PRINTF("* MainScreen dropdown Bluetooth disconnect failed\n");
              }
            });

          } else if (bt_status.discoverable) {
            //cancel active pairing
            this->bbv2_manager.system.btrx_if.SetDiscoverable(false, [](bool success) {
              if (!success) {
                DEBUG_PRINTF("* MainScreen dropdown Bluetooth pairing cancel failed\n");
              }
            });
          } else {
            //start pairing
            this->bbv2_manager.system.btrx_if.SetDiscoverable(true, [](bool success) {
              if (!success) {
                DEBUG_PRINTF("* MainScreen dropdown Bluetooth pairing start failed\n");
              }
            });
          }
          break;
        case SCREEN_MAIN_TAG_DROPDOWN_USB_SELECT:
          //activate USB input, if available and not already active
          if (this->bbv2_manager.system.audio_mgr.IsInputAvailable(AUDIO_INPUT_USB) && active_input != AUDIO_INPUT_USB) {
            this->bbv2_manager.system.audio_mgr.SetActiveInput(AUDIO_INPUT_USB, [](bool success) {
              if (!success) {
                DEBUG_PRINTF("MainScreen dropdown USB select failed, might be unavailable\n");
              }
            });
          }
          break;
        case SCREEN_MAIN_TAG_DROPDOWN_SPDIF_SELECT:
          //activate SPDIF input, if available and not already active
          if (this->bbv2_manager.system.audio_mgr.IsInputAvailable(AUDIO_INPUT_SPDIF) && active_input != AUDIO_INPUT_SPDIF) {
            this->bbv2_manager.system.audio_mgr.SetActiveInput(AUDIO_INPUT_SPDIF, [](bool success) {
              if (!success) {
                DEBUG_PRINTF("MainScreen dropdown SPDIF select failed, might be unavailable\n");
              }
            });
          }
          break;
        default:
          break;
      }
    } else {
      //non-dropdown touch inputs
      bool audio_mute = this->bbv2_manager.system.audio_mgr.IsMute();
      switch (state.tag) {
        case SCREEN_MAIN_TAG_MEDIA_BACK:
          //send back command, if AVRCP connected
          if (bt_status.avrcp_link) {
            this->bbv2_manager.system.btrx_if.SendMediaControl(IF_BTRX_MEDIA_BACKWARD, [](bool success) {
              if (!success) {
                DEBUG_PRINTF("* MainScreen media back action failed\n");
              }
            });
          }
          break;
        case SCREEN_MAIN_TAG_MEDIA_PLAYPAUSE:
          //send play/pause command (toggle), if AVRCP connected
          if (bt_status.avrcp_link) {
            if (bt_status.avrcp_playing) {
              this->bbv2_manager.system.btrx_if.SendMediaControl(IF_BTRX_MEDIA_PAUSE, [](bool success) {
                if (!success) {
                  DEBUG_PRINTF("* MainScreen media pause action failed\n");
                }
              });
            } else {
              this->bbv2_manager.system.btrx_if.SendMediaControl(IF_BTRX_MEDIA_PLAY, [](bool success) {
                if (!success) {
                  DEBUG_PRINTF("* MainScreen media play action failed\n");
                }
              });
            }
          }
          break;
        case SCREEN_MAIN_TAG_MEDIA_FORWARD:
          //send forward command, if AVRCP connected
          if (bt_status.avrcp_link) {
            this->bbv2_manager.system.btrx_if.SendMediaControl(IF_BTRX_MEDIA_FORWARD, [](bool success) {
              if (!success) {
                DEBUG_PRINTF("* MainScreen media forward action failed\n");
              }
            });
          }
          break;
        case SCREEN_MAIN_TAG_VOL_DOWN:
          //step volume down, if not muted and not long pressed
          if (!audio_mute && !state.long_press) {
            this->bbv2_manager.system.audio_mgr.StepCurrentVolumeDB(false, [](bool success) {
              if (!success) {
                DEBUG_PRINTF("* MainScreen volume decrease failed\n");
              }
            });
          }
          break;
        case SCREEN_MAIN_TAG_MUTE_UNMUTE:
          //toggle mute status
          this->bbv2_manager.system.audio_mgr.SetMute(!audio_mute, [](bool success) {
            if (!success) {
              DEBUG_PRINTF("* MainScreen mute toggle failed\n");
            }
          });
          break;
        case SCREEN_MAIN_TAG_VOL_UP:
          //step volume up, if not muted and not long pressed
          if (!audio_mute && !state.long_press) {
            this->bbv2_manager.system.audio_mgr.StepCurrentVolumeDB(true, [](bool success) {
              if (!success) {
                DEBUG_PRINTF("* MainScreen volume increase failed\n");
              }
            });
          }
          break;
        case SCREEN_MAIN_TAG_BLUETOOTH:
          if (bt_status.discoverable) {
            //cancel active pairing
            this->bbv2_manager.system.btrx_if.SetDiscoverable(false, [](bool success) {
              if (!success) {
                DEBUG_PRINTF("* MainScreen Bluetooth pairing cancel failed\n");
              }
            });
          } else if (bt_status.connected) {
            //disconnect active connection if long-pressed
            if (state.long_press) {
              this->bbv2_manager.system.btrx_if.DisconnectBluetooth([](bool success) {
                if (!success) {
                  DEBUG_PRINTF("* MainScreen Bluetooth disconnect failed\n");
                }
              });
            }
          } else {
            //start pairing
            this->bbv2_manager.system.btrx_if.SetDiscoverable(true, [](bool success) {
              if (!success) {
                DEBUG_PRINTF("* MainScreen Bluetooth pairing start failed\n");
              }
            });
          }
          break;
        case SCREEN_MAIN_TAG_LIGHT:
          //TODO manage actual light
          _temp_light_on = !_temp_light_on;
          this->needs_display_list_rebuild = true;
          break;
        case SCREEN_MAIN_TAG_SETTINGS:
          //TODO enter settings screen
          break;
        case SCREEN_MAIN_TAG_POWER_OFF:
          //TODO actual power-off, just mock-up for now
          this->bbv2_manager.system.audio_mgr.HandlePowerStateChange(false, [&](bool success) {
            DEBUG_PRINTF("MainScreen power-off: AudioPathManager success %u\n", success);
            if (success) {
              this->bbv2_manager.system.amp_if.SetManualShutdownActive(true, [&](bool success) {
                DEBUG_PRINTF("MainScreen power-off: PowerAmp success %u\n", success);
                if (success) {
                  //reset this screen and go to power-off screen
                  this->bbv2_manager.SetScreen(&this->bbv2_manager.power_off_screen);
                }
              });
            }
          });
          break;
        case SCREEN_MAIN_TAG_DROPDOWN_TRIGGER:
          //open input dropdown
          this->dropdown_open = true;
          this->needs_display_list_rebuild = true;
          break;
        default:
          break;
      }
    }
  }

  //long-press tick: volume adjustments (allows holding the +/- buttons), only if not in dropdown and not muted
  if (state.touched && state.long_press_tick && state.tag == state.initial_tag && !this->dropdown_open && !this->bbv2_manager.system.audio_mgr.IsMute()) {
    switch (state.tag) {
      case SCREEN_MAIN_TAG_VOL_DOWN:
        //step volume down
        this->bbv2_manager.system.audio_mgr.StepCurrentVolumeDB(false, [](bool success) {
          if (!success) {
            DEBUG_PRINTF("* MainScreen volume hold decrease failed\n");
          }
        });
        break;
      case SCREEN_MAIN_TAG_VOL_UP:
        //step volume up
        this->bbv2_manager.system.audio_mgr.StepCurrentVolumeDB(true, [](bool success) {
          if (!success) {
            DEBUG_PRINTF("* MainScreen volume hold increase failed\n");
          }
        });
        break;
      default:
        break;
    }
  }
}


void MainScreen::Init() {
  //allow base handling
  this->BlockBoxV2Screen::Init();

  //redraw on input and volume/mute changes
  this->bbv2_manager.system.audio_mgr.RegisterCallback([&](EventSource*, uint32_t event) {
    switch (event) {
      case AUDIO_EVENT_INPUT_UPDATE:
        if (this->dropdown_open || this->bbv2_manager.system.audio_mgr.GetActiveInput() != this->currently_drawn_input) {
          this->needs_display_list_rebuild = true;
        }
        break;
      case AUDIO_EVENT_VOLUME_UPDATE:
        if (!this->bbv2_manager.system.audio_mgr.IsMute() && (int8_t)roundf(this->bbv2_manager.system.audio_mgr.GetCurrentVolumeDB()) != this->currently_drawn_volume_dB) {
          this->needs_display_list_rebuild = true;
        }
        break;
      case AUDIO_EVENT_MUTE_UPDATE:
        if (this->bbv2_manager.system.audio_mgr.IsMute() != this->currently_drawn_mute) {
          this->needs_display_list_rebuild = true;
        }
        break;
    }
  }, AUDIO_EVENT_INPUT_UPDATE | AUDIO_EVENT_VOLUME_UPDATE | AUDIO_EVENT_MUTE_UPDATE);

  //redraw in dropdown on any Bluetooth changes, or in main screen on connection/playback/metadata changes
  this->bbv2_manager.system.btrx_if.RegisterCallback([&](EventSource*, uint32_t event) {
    if (this->dropdown_open) {
      this->needs_display_list_rebuild = true;
    } else {
      switch (event) {
        case MODIF_BTRX_EVENT_STATUS_UPDATE:
        {
          BluetoothReceiverStatus status = this->bbv2_manager.system.btrx_if.GetStatus();
          if (status.value != this->currently_drawn_bt_status.value) {
            this->needs_display_list_rebuild = true;
          }
          break;
        }
        case MODIF_BTRX_EVENT_MEDIA_META_UPDATE:
          if (currently_drawn_input == AUDIO_INPUT_BLUETOOTH) {
            this->needs_display_list_rebuild = true;
          }
          break;
        default:
          break;
      }
    }
  }, MODIF_BTRX_EVENT_STATUS_UPDATE | MODIF_BTRX_EVENT_MEDIA_META_UPDATE | MODIF_BTRX_EVENT_DEVICE_UPDATE | MODIF_BTRX_EVENT_CONN_STATS_UPDATE | MODIF_BTRX_EVENT_CODEC_UPDATE);

  //redraw in dropdown on any DAP changes, or on streaming status change if any input is active, or on input rate change if USB or SPDIF is active
  this->bbv2_manager.system.dap_if.RegisterCallback([&](EventSource*, uint32_t event) {
    if (this->dropdown_open) {
      this->needs_display_list_rebuild = true;
    } else {
      switch (event) {
        case MODIF_DAP_EVENT_STATUS_UPDATE:
          if (this->currently_drawn_input != AUDIO_INPUT_NONE && this->bbv2_manager.system.dap_if.GetStatus().streaming != this->currently_drawn_streaming) {
            this->needs_display_list_rebuild = true;
          }
          break;
        case MODIF_DAP_EVENT_INPUT_RATE_UPDATE:
          if ((this->currently_drawn_input == AUDIO_INPUT_USB || this->currently_drawn_input == AUDIO_INPUT_SPDIF) && this->currently_drawn_streaming &&
              (uint32_t)this->bbv2_manager.system.dap_if.GetSRCInputSampleRate() != this->currently_drawn_input_rate) {
            this->needs_display_list_rebuild = true;
          }
          break;
      }
    }
  }, MODIF_DAP_EVENT_STATUS_UPDATE | MODIF_DAP_EVENT_INPUT_RATE_UPDATE);

  //TODO: register update handlers for RTC, as well as maybe others if needed?
}


static bool _IsMediaInfoValid(const char* str) {
  if (str == NULL) {
    //no string: not valid
    return false;
  }

  if (str[0] == 0) {
    //empty string: not valid
    return false;
  }

  //direct placeholder string comparisons
  const char* cmp = "unavailable";
  uint16_t i;
  for (i = 0; i < 12; i++) {
    if ((char)std::tolower((unsigned char)str[i]) != cmp[i]) {
      break;
    }
  }
  if (i >= 12) {
    //"unavailable": not valid
    return false;
  }

  cmp = "not provided";
  for (i = 0; i < 13; i++) {
    if ((char)std::tolower((unsigned char)str[i]) != cmp[i]) {
      break;
    }
  }
  if (i >= 13) {
    //"not provided": not valid
    return false;
  }

  //otherwise, valid
  return true;
}

void MainScreen::BuildScreenContent() {
  this->driver.CmdTag(0);

  DAPStatus dap_status = this->bbv2_manager.system.dap_if.GetStatus();
  uint32_t theme_colour_main = this->bbv2_manager.GetThemeColorMain();
  uint32_t theme_colour_dark = this->bbv2_manager.GetThemeColorDark();

  this->currently_drawn_input = this->bbv2_manager.system.audio_mgr.GetActiveInput();
  this->currently_drawn_bt_status = this->bbv2_manager.system.btrx_if.GetStatus();


  //prepare info label text
  char info_lines[3][96] = { { 0 } };
  this->currently_drawn_streaming = dap_status.streaming;
  switch (this->currently_drawn_input) {
    case AUDIO_INPUT_BLUETOOTH:
    {
      //check if media metadata is valid
      const char* title = this->bbv2_manager.system.btrx_if.GetMediaTitle();
      const char* artist = this->bbv2_manager.system.btrx_if.GetMediaArtist();
      const char* album = this->bbv2_manager.system.btrx_if.GetMediaAlbum();
      if (_IsMediaInfoValid(title) && _IsMediaInfoValid(artist)) {
        //at least title and artist valid: use that for info labels
        strncpy(info_lines[0], title, 95);
        strncpy(info_lines[1], artist, 95);
        //use album too if it's valid, otherwise leave blank
        if (_IsMediaInfoValid(album)) {
          strncpy(info_lines[2], album, 95);
        } else {
          memset(info_lines[2], 0, 95);
        }
      } else {
        //title or artist invalid: use default labels with status
        strncpy(info_lines[0], "BlockBox v2 neo", 95);
        strncpy(info_lines[1], "Bluetooth Audio Connected", 95);
        strncpy(info_lines[2], (this->currently_drawn_streaming && this->currently_drawn_bt_status.a2dp_streaming) ? "Streaming" : "Idle", 95);
      }
      break;
    }
    case AUDIO_INPUT_USB:
    {
      //default labels with status
      strncpy(info_lines[0], "BlockBox v2 neo", 95);
      if (this->currently_drawn_streaming) {
        strncpy(info_lines[1], "USB Audio Streaming", 95);
        this->currently_drawn_input_rate = (uint32_t)this->bbv2_manager.system.dap_if.GetSRCInputSampleRate();
        snprintf(info_lines[2], 95, "%lu Hz", this->currently_drawn_input_rate);
      } else {
        strncpy(info_lines[1], "USB Audio Connected", 95);
        strncpy(info_lines[2], "Idle", 95);
      }
      break;
    }
    case AUDIO_INPUT_SPDIF:
    {
      //default labels with status
      strncpy(info_lines[0], "BlockBox v2 neo", 95);
      if (this->currently_drawn_streaming) {
        strncpy(info_lines[1], "S/PDIF Audio Streaming", 95);
        this->currently_drawn_input_rate = (uint32_t)this->bbv2_manager.system.dap_if.GetSRCInputSampleRate();
        snprintf(info_lines[2], 95, "%lu Hz", this->currently_drawn_input_rate);
      } else {
        strncpy(info_lines[1], "S/PDIF Audio Connected", 95);
        strncpy(info_lines[2], "Idle", 95);
      }
      break;
    }
    default:
      //default labels
      strncpy(info_lines[0], "BlockBox v2 neo", 95);
      strncpy(info_lines[1], "No Inputs Connected", 95);
      strncpy(info_lines[2], "Idle", 95);
      break;
  }
  info_lines[0][95] = 0;
  info_lines[1][95] = 0;
  info_lines[2][95] = 0;

  //draw info labels
  this->driver.CmdColorRGB(0xFFFFFF);
  char shortened_text[96] = { 0 };
  //first line: choose biggest possible font (29 -> 28 -> 27 with shortening if needed)
  if (EVEDriver::GetTextWidth(29, info_lines[0]) <= SCREEN_MAIN_INFO_MAX_WIDTH) {
    //fits in font 29: draw directly
    this->driver.CmdText(160, 45, 29, EVE_OPT_CENTER, info_lines[0]);
  } else if (EVEDriver::GetTextWidth(28, info_lines[0]) <= SCREEN_MAIN_INFO_MAX_WIDTH) {
    //fits in font 28: draw directly
    this->driver.CmdText(160, 45, 28, EVE_OPT_CENTER, info_lines[0]);
  } else {
    //doesn't fit in fonts 28 or 29: shorten if necessary and draw in font 27
    BlockBoxV2Screen::CopyOrShortenText(shortened_text, info_lines[0], 95, SCREEN_MAIN_INFO_MAX_WIDTH, 27);
    shortened_text[95] = 0;
    this->driver.CmdText(160, 45, 27, EVE_OPT_CENTER, shortened_text);
  }
  //second and third lines: shorten if necessary and draw in font 26
  BlockBoxV2Screen::CopyOrShortenText(shortened_text, info_lines[1], 95, SCREEN_MAIN_INFO_MAX_WIDTH, 26);
  shortened_text[95] = 0;
  this->driver.CmdText(160, 74, 26, EVE_OPT_CENTER, shortened_text);
  BlockBoxV2Screen::CopyOrShortenText(shortened_text, info_lines[2], 95, SCREEN_MAIN_INFO_MAX_WIDTH, 26);
  shortened_text[95] = 0;
  this->driver.CmdText(160, 95, 26, EVE_OPT_CENTER, shortened_text);


  //media control buttons (or placeholders)
  uint32_t button_fill;
  if (this->currently_drawn_input == AUDIO_INPUT_BLUETOOTH && this->currently_drawn_bt_status.avrcp_link) {
    //Bluetooth connected with AVRCP: draw active media controls
    if (this->currently_drawn_bt_status.avrcp_playing) {
      GUIDraws::PauseIconLarge(this->driver, 135, 111, 0xFFFFFF, theme_colour_main);
    } else {
      GUIDraws::PlayIconLarge(this->driver, 136, 111, 0xFFFFFF, theme_colour_main, 0x000000);
    }
    button_fill = theme_colour_main;
  } else {
    //placeholders: draw logo and inactive controls
    GUIDraws::BWLogoLarge(this->driver, 134, 112, 0xFFFFFF, theme_colour_main);
    button_fill = 0x808080;
  }
  GUIDraws::BackIconLarge(this->driver, 60, 111, 0xFFFFFF, button_fill, 0x000000);
  GUIDraws::ForwardIconLarge(this->driver, 210, 111, 0xFFFFFF, button_fill, 0x000000);


  //separator lines
  this->driver.CmdBeginDraw(EVE_LINES);
  this->driver.CmdDL(LINE_WIDTH(16));
  this->driver.CmdColorRGB(0x808080);
  this->driver.CmdDL(VERTEX2F(129 * 16 + 8, 192 * 16));
  this->driver.CmdDL(VERTEX2F(129 * 16 + 8, 230 * 16));
  this->driver.CmdDL(VERTEX2F(176 * 16 + 8, 192 * 16));
  this->driver.CmdDL(VERTEX2F(176 * 16 + 8, 230 * 16));
  this->driver.CmdDL(VERTEX2F(223 * 16 + 8, 192 * 16));
  this->driver.CmdDL(VERTEX2F(223 * 16 + 8, 230 * 16));
  this->driver.CmdDL(VERTEX2F(270 * 16 + 8, 192 * 16));
  this->driver.CmdDL(VERTEX2F(270 * 16 + 8, 230 * 16));
  this->driver.CmdDL(VERTEX2F(5 * 16, 181 * 16 + 8));
  this->driver.CmdDL(VERTEX2F(314 * 16, 181 * 16 + 8));
  //+,- buttons
  this->driver.CmdColorRGB(0xFFFFFF);
  this->driver.CmdDL(VERTEX2F(12 * 16, 211 * 16));
  this->driver.CmdDL(VERTEX2F(38 * 16, 211 * 16));
  this->driver.CmdDL(VERTEX2F(94 * 16, 211 * 16));
  this->driver.CmdDL(VERTEX2F(120 * 16, 211 * 16));
  this->driver.CmdDL(VERTEX2F(107 * 16, 198 * 16));
  this->driver.CmdDL(VERTEX2F(107 * 16, 224 * 16));
  this->driver.CmdDL(DL_END);


  //speaker symbol and volume/mute status
  GUIDraws::SpeakerIconSmallish(this->driver, 54, 191, 0xFFFFFF, 0x000000);
  this->currently_drawn_mute = this->bbv2_manager.system.audio_mgr.IsMute();
  if (this->currently_drawn_mute) {
    strncpy(shortened_text, "Mute", 95);
  } else {
    this->currently_drawn_volume_dB = (int8_t)roundf(this->bbv2_manager.system.audio_mgr.GetCurrentVolumeDB());
    snprintf(shortened_text, 95, "%+ddB", this->currently_drawn_volume_dB);
  }
  shortened_text[95] = 0;
  this->driver.CmdText(66, 217, 26, EVE_OPT_CENTERX, shortened_text);

  //remaining button icons
  GUIDraws::BluetoothIconMedium(this->driver, 133, 191, this->currently_drawn_bt_status.connected ? theme_colour_main : 0xFFFFFF, this->currently_drawn_bt_status.discoverable);
  GUIDraws::BulbIconMedium(this->driver, 180, 191, 0xFFFFFF, 0x000000, _temp_light_on); //TODO rays based on actual light status
  GUIDraws::CogIconMedium(this->driver, 227, 191, 0xFFFFFF, 0x000000);
  GUIDraws::PowerIconMedium(this->driver, 274, 191, 0xFFFFFF, 0x000000);


  //time+date text - TODO actual time+date
  this->driver.CmdText(160, 167, 20, EVE_OPT_CENTERX, "Fri Jul 18 2025 - 06:23 PM");


  //input dropdown, if open
  if (this->dropdown_open) {
    this->driver.CmdDL(DL_SAVE_CONTEXT);

    //main screen shadow (= dropdown close trigger)
    this->driver.CmdBeginDraw(EVE_RECTS);
    this->driver.CmdDL(LINE_WIDTH(16));
    this->driver.CmdTagMask(true);
    this->driver.CmdTag(SCREEN_MAIN_TAG_DROPDOWN_TRIGGER);
    this->driver.CmdColorA(196);
    this->driver.CmdColorRGB(0x000000);
    this->driver.CmdDL(VERTEX2F(0, 0));
    this->driver.CmdDL(VERTEX2F(16 * 320, 16 * 240));
    //dropdown background
    this->driver.CmdTag(0);
    this->driver.CmdColorA(255);
    this->driver.CmdColorRGB(0x404040);
    this->driver.CmdDL(LINE_WIDTH(16 * 10));
    this->driver.CmdDL(VERTEX2F(16 * 9, 0));
    this->driver.CmdDL(VERTEX2F(16 * 310, 16 * 117));
    this->driver.CmdDL(DL_END);

    //Bluetooth section
    const char* action_button_text = "";
    bool bt_available = this->bbv2_manager.system.audio_mgr.IsInputAvailable(AUDIO_INPUT_BLUETOOTH);
    if (bt_available) {
      //Bluetooth connected (active or inactive): get device name
      const char* device_name = this->bbv2_manager.system.btrx_if.GetDeviceName();
      if (device_name[0] == 0) {
        //empty name: use default button label
        strncpy(info_lines[0], "     Bluetooth - Connected", 95);
      } else {
        //non-empty name: format into button label
        snprintf(info_lines[0], 95, "     Bluetooth - %s", device_name);
      }

      //get and format device + connection info
      uint64_t bt_addr = this->bbv2_manager.system.btrx_if.GetDeviceAddress();
      const char* bt_codec = this->bbv2_manager.system.btrx_if.GetActiveCodec();
      int16_t bt_rssi = this->bbv2_manager.system.btrx_if.GetConnectionRSSI();
      uint8_t bt_quality_percent = (uint8_t)roundf(100.0f * (float)this->bbv2_manager.system.btrx_if.GetConnectionQuality() / (float)UINT16_MAX);
      BlockBoxV2Screen::CopyOrShortenText(shortened_text, bt_codec, 95, SCREEN_MAIN_DROPDOWN_BT_CODEC_MAX_WIDTH, 20);
#pragma GCC diagnostic ignored "-Wformat-truncation="
      snprintf(info_lines[1], 95, "0x%012llX | %s | %+ddB / %u%% | %c", (bt_addr & 0xFFFFFFFFFFFFllu), shortened_text, bt_rssi, bt_quality_percent, this->currently_drawn_bt_status.avrcp_link ? '2' : '1');
#pragma GCC diagnostic pop

      //button colour depending on active or not
      this->driver.CmdFGColor((this->currently_drawn_input == AUDIO_INPUT_BLUETOOTH) ? theme_colour_main : theme_colour_dark);
    } else if (this->currently_drawn_bt_status.discoverable) {
      //not connected, pairing
      strncpy(info_lines[0], "     Bluetooth - Pairing...", 95);
      strncpy(info_lines[1], "Waiting for device to pair", 95);
      action_button_text = "Stop";
      this->driver.CmdFGColor(0x404040);
    } else {
      //not connected, not pairing
      strncpy(info_lines[0], "     Bluetooth - Not Connected", 95);
      strncpy(info_lines[1], "Not pairing", 95);
      action_button_text = "Pair";
      this->driver.CmdFGColor(0x404040);
    }
    info_lines[0][95] = 0;
    info_lines[1][95] = 0;
    //main button
    this->driver.CmdTag(SCREEN_MAIN_TAG_DROPDOWN_BT_SELECT);
    this->driver.CmdBeginDraw(EVE_RECTS);
    this->driver.CmdDL(LINE_WIDTH(40));
    this->driver.CmdColorRGB(0xFFFFFF);
    this->driver.CmdDL(VERTEX2F(16 * 10, 16 * 29));
    this->driver.CmdDL(VERTEX2F(16 * (bt_available ? 278 : 258), 16 * 50));
    this->driver.CmdDL(DL_END);
    BlockBoxV2Screen::CopyOrShortenText(shortened_text, info_lines[0], 95, SCREEN_MAIN_DROPDOWN_BT_BUTTON_MAX_WIDTH, 26);
    this->driver.CmdButton(9, 28, (bt_available ? 270 : 250), 23, 26, EVE_OPT_FLAT, shortened_text);
    //action button
    this->driver.CmdFGColor(theme_colour_dark);
    this->driver.CmdTag(SCREEN_MAIN_TAG_DROPDOWN_BT_ACTION);
    this->driver.CmdBeginDraw(EVE_RECTS);
    this->driver.CmdDL(VERTEX2F(16 * (bt_available ? 288 : 268), 16 * 29));
    this->driver.CmdDL(VERTEX2F(16 * 309, 16 * 50));
    this->driver.CmdDL(DL_END);
    this->driver.CmdButton((bt_available ? 287 : 267), 28, (bt_available ? 23 : 43), 23, 26, EVE_OPT_FLAT, action_button_text);
    //status text
    this->driver.CmdTag(0);
    this->driver.CmdText(160, 55, 20, EVE_OPT_CENTERX, info_lines[1]);

    //USB section
    bool usb_available = this->bbv2_manager.system.audio_mgr.IsInputAvailable(AUDIO_INPUT_USB);
    if (usb_available) {
      //USB connected
      strncpy(info_lines[0], "      USB - Connected", 95);
      //we know USB is streaming audio if DAP says USB is available
      bool usb_streaming = this->bbv2_manager.system.dap_if.IsInputAvailable(IF_DAP_INPUT_USB);
      if (usb_streaming) {
        if (this->currently_drawn_input == AUDIO_INPUT_USB) {
          //streaming, active input: include sample rate in status
          snprintf(info_lines[1], 95, "Audio streaming @ %lu Hz", (uint32_t)this->bbv2_manager.system.dap_if.GetSRCInputSampleRate());
        } else {
          //streaming, passive input: we don't know the sample rate
          strncpy(info_lines[1], "Audio stream active", 95);
        }
      } else {
        strncpy(info_lines[1], "Audio stream inactive", 95);
      }

      this->driver.CmdFGColor((this->currently_drawn_input == AUDIO_INPUT_USB) ? theme_colour_main : theme_colour_dark);
    } else {
      //USB not connected
      strncpy(info_lines[0], "      USB - Inactive", 95);
      strncpy(info_lines[1], "Not connected", 95);
      this->driver.CmdFGColor(0x404040);
    }
    info_lines[0][95] = 0;
    info_lines[1][95] = 0;
    //button
    this->driver.CmdTag(SCREEN_MAIN_TAG_DROPDOWN_USB_SELECT);
    this->driver.CmdBeginDraw(EVE_RECTS);
    this->driver.CmdDL(VERTEX2F(16 * 10, 16 * 82));
    this->driver.CmdDL(VERTEX2F(16 * 148, 16 * 103));
    this->driver.CmdDL(DL_END);
    this->driver.CmdButton(9, 81, 140, 23, 26, EVE_OPT_FLAT, info_lines[0]);
    //status text
    this->driver.CmdTag(0);
    this->driver.CmdText(80, 108, 20, EVE_OPT_CENTERX, info_lines[1]);

    //SPDIF section
    bool spdif_available = this->bbv2_manager.system.audio_mgr.IsInputAvailable(AUDIO_INPUT_SPDIF);
    uint32_t spdif_button_color;
    if (spdif_available) {
      //SPDIF connected
      strncpy(info_lines[0], "      S/PDIF - Active", 95);
      //we know SPDIF is streaming audio if DAP says SPDIF is available
      bool spdif_streaming = this->bbv2_manager.system.dap_if.IsInputAvailable(IF_DAP_INPUT_SPDIF);
      if (spdif_streaming) {
        if (this->currently_drawn_input == AUDIO_INPUT_SPDIF) {
          //streaming, active input: include sample rate in status
          snprintf(info_lines[1], 95, "Audio streaming @ %lu Hz", (uint32_t)this->bbv2_manager.system.dap_if.GetSRCInputSampleRate());
        } else {
          //streaming, passive input: we don't know the sample rate
          strncpy(info_lines[1], "Audio stream active", 95);
        }
      } else {
        strncpy(info_lines[1], "Audio stream inactive", 95);
      }

      spdif_button_color = (this->currently_drawn_input == AUDIO_INPUT_SPDIF) ? theme_colour_main : theme_colour_dark;
    } else {
      //SPDIF not connected
      strncpy(info_lines[0], "      S/PDIF - Inactive", 95);
      strncpy(info_lines[1], "Not connected", 95);
      spdif_button_color = 0x404040;
    }
    info_lines[0][95] = 0;
    info_lines[1][95] = 0;
    //button
    this->driver.CmdFGColor(spdif_button_color);
    this->driver.CmdTag(SCREEN_MAIN_TAG_DROPDOWN_SPDIF_SELECT);
    this->driver.CmdBeginDraw(EVE_RECTS);
    this->driver.CmdDL(VERTEX2F(16 * 171, 16 * 82));
    this->driver.CmdDL(VERTEX2F(16 * 309, 16 * 103));
    this->driver.CmdDL(DL_END);
    this->driver.CmdButton(170, 81, 140, 23, 26, EVE_OPT_FLAT, info_lines[0]);
    //status text
    this->driver.CmdTag(0);
    this->driver.CmdText(240, 108, 20, EVE_OPT_CENTERX, info_lines[1]);

    //icons
    this->driver.CmdTagMask(false);
    //cross on Bluetooth action button, if needed
    if (bt_available) {
      this->driver.CmdBeginDraw(EVE_LINES);
      this->driver.CmdDL(LINE_WIDTH(16));
      this->driver.CmdDL(VERTEX2F(16 * 293, 16 * 34));
      this->driver.CmdDL(VERTEX2F(16 * 304, 16 * 45));
      this->driver.CmdDL(VERTEX2F(16 * 304, 16 * 34));
      this->driver.CmdDL(VERTEX2F(16 * 293, 16 * 45));
      this->driver.CmdDL(DL_END);
    }
    //Bluetooth button icon
    GUIDraws::BluetoothIconSmall(this->driver, 11, 29, 0xFFFFFF);
    //USB button icon
    GUIDraws::USBIconSmall(this->driver, 11, 82, 0xFFFFFF);
    //SPDIF button icon
    GUIDraws::SPDIFIconSmall(this->driver, 172, 82, 0xFFFFFF, spdif_button_color);

    this->driver.CmdDL(DL_RESTORE_CONTEXT);

    this->status_text_override = "BlockBox v2 neo - Inputs";
  } else {
    this->status_text_override = NULL;
  }


  //top status bar
  this->BlockBoxV2Screen::BuildScreenContent();


  //invisible touch areas, if dropdown is closed
  if (!this->dropdown_open) {
    this->driver.CmdDL(DL_SAVE_CONTEXT);
    this->driver.CmdBeginDraw(EVE_RECTS);
    this->driver.CmdDL(LINE_WIDTH(16));
    this->driver.CmdDL(COLOR_MASK(0, 0, 0, 0));
    this->driver.CmdTagMask(true);
    this->driver.CmdTag(SCREEN_MAIN_TAG_MEDIA_BACK); //back button
    this->driver.CmdDL(VERTEX2F(16 * 60, 16 * 111));
    this->driver.CmdDL(VERTEX2F(16 * 110, 16 * 161));
    this->driver.CmdTag(SCREEN_MAIN_TAG_MEDIA_PLAYPAUSE); //play/pause button
    this->driver.CmdDL(VERTEX2F(16 * 135, 16 * 111));
    this->driver.CmdDL(VERTEX2F(16 * 185, 16 * 161));
    this->driver.CmdTag(SCREEN_MAIN_TAG_MEDIA_FORWARD); //forward button
    this->driver.CmdDL(VERTEX2F(16 * 210, 16 * 111));
    this->driver.CmdDL(VERTEX2F(16 * 260, 16 * 161));
    this->driver.CmdTag(SCREEN_MAIN_TAG_VOL_DOWN); //volume -
    this->driver.CmdDL(VERTEX2F(16 * 5, 16 * 191));
    this->driver.CmdDL(VERTEX2F(16 * 44, 16 * 231));
    this->driver.CmdTag(SCREEN_MAIN_TAG_MUTE_UNMUTE); //mute/unmute
    this->driver.CmdDL(VERTEX2F(16 * 47, 16 * 191));
    this->driver.CmdDL(VERTEX2F(16 * 85, 16 * 231));
    this->driver.CmdTag(SCREEN_MAIN_TAG_VOL_UP); //volume +
    this->driver.CmdDL(VERTEX2F(16 * 88, 16 * 191));
    this->driver.CmdDL(VERTEX2F(16 * 127, 16 * 231));
    this->driver.CmdTag(SCREEN_MAIN_TAG_BLUETOOTH); //bluetooth button
    this->driver.CmdDL(VERTEX2F(16 * 133, 16 * 191));
    this->driver.CmdDL(VERTEX2F(16 * 173, 16 * 231));
    this->driver.CmdTag(SCREEN_MAIN_TAG_LIGHT); //light button
    this->driver.CmdDL(VERTEX2F(16 * 180, 16 * 191));
    this->driver.CmdDL(VERTEX2F(16 * 220, 16 * 231));
    this->driver.CmdTag(SCREEN_MAIN_TAG_SETTINGS); //settings button
    this->driver.CmdDL(VERTEX2F(16 * 227, 16 * 191));
    this->driver.CmdDL(VERTEX2F(16 * 267, 16 * 231));
    this->driver.CmdTag(SCREEN_MAIN_TAG_POWER_OFF); //power off button
    this->driver.CmdDL(VERTEX2F(16 * 274, 16 * 191));
    this->driver.CmdDL(VERTEX2F(16 * 314, 16 * 231));
    this->driver.CmdTag(SCREEN_MAIN_TAG_DROPDOWN_TRIGGER); //input dropdown open trigger
    this->driver.CmdDL(VERTEX2F(0, 0));
    this->driver.CmdDL(VERTEX2F(16 * 320, 16 * 45));
    this->driver.CmdDL(DL_END);
    this->driver.CmdDL(DL_RESTORE_CONTEXT);
  }
}


void MainScreen::UpdateExistingScreenContent() {
  //TODO non-redraw updates
}

