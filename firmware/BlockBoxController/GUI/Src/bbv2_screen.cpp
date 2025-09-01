/*
 * bbv2_screen.cpp
 *
 *  Created on: Jul 19, 2025
 *      Author: Alex
 */


#include "bbv2_screen.h"
#include "system.h"
#include "gui_common_draws.h"
#include "math.h"


//maximum widths of bluetooth device name text and override text, in pixels
#define SCREEN_TOPBAR_BT_DEVICE_NAME_MAX_WIDTH 194
#define SCREEN_TOPBAR_OVERRIDE_TEXT_MAX_WIDTH 206

//touch tags
#define SCREEN_POPUP_TAG 240


//widths of "..." in pixels, for various fonts
const uint16_t BlockBoxV2Screen::dots_width_20 = EVEDriver::GetTextWidth(20, "...");
const uint16_t BlockBoxV2Screen::dots_width_26 = EVEDriver::GetTextWidth(26, "...");
const uint16_t BlockBoxV2Screen::dots_width_27 = EVEDriver::GetTextWidth(27, "...");
const uint16_t BlockBoxV2Screen::dots_width_28 = EVEDriver::GetTextWidth(28, "...");


//functions like strncpy(dest, src, max_chars), but limits the pixel-width of the text (in the given font) to the given maximum, shortening with "..." if necessary.
void BlockBoxV2Screen::CopyOrShortenText(char* dest, const char* src, uint16_t max_chars, uint16_t max_width, uint8_t font) {
  if (dest == NULL || src == NULL) {
    throw std::invalid_argument("BlockBoxV2Screen CopyOrShortenText given null pointer");
  }

  if (max_chars == 0) {
    //nothing to do
    return;
  }

  //get width of "..."
  uint16_t dots_width;
  switch (font) {
    case 20:
      dots_width = BlockBoxV2Screen::dots_width_20;
      break;
    case 26:
      dots_width = BlockBoxV2Screen::dots_width_26;
      break;
    case 27:
      dots_width = BlockBoxV2Screen::dots_width_27;
      break;
    case 28:
      dots_width = BlockBoxV2Screen::dots_width_28;
      break;
    default:
      throw std::invalid_argument("BlockBoxV2Screen CopyOrShortenText given unsupported font");
  }

  //check if text fits into the provided width
  if (EVEDriver::GetTextWidth(font, src) <= max_width) {
    //fits fully: copy directly
    strncpy(dest, src, max_chars);
  } else {
    //doesn't fit fully: shorten with "..."
    uint16_t max_src_chars = EVEDriver::GetMaxFittingTextLength(font, max_width - dots_width, src);
    if (max_src_chars > max_chars - 3) {
      //ensure enough array space for dots
      max_src_chars = max_chars - 3;
    }
    strncpy(dest, src, max_src_chars);
    strncpy(dest + max_src_chars, "...", max_chars - max_src_chars);
  }
}


BlockBoxV2Screen::BlockBoxV2Screen(BlockBoxV2GUIManager& manager) :
    GUIScreen(manager), bbv2_manager(manager), status_text_override(NULL),
    currently_drawn_input(AUDIO_INPUT_NONE), currently_drawn_battery_percent(0), currently_drawn_battery_precise(false) {}


void BlockBoxV2Screen::HandleTouch(const GUITouchState& state) noexcept {
  //check for pop-up tap
  if (state.released && state.tag == SCREEN_POPUP_TAG && state.tag == state.initial_tag) {
    uint32_t popup = this->bbv2_manager.GetHighestPopup();
    switch (popup) {
      case GUI_POPUP_AMP_FAULT:
      case GUI_POPUP_AMP_SAFETY_ERR:
        //amp fault or safety error: can dismiss by going back to standby/power-off state
        this->bbv2_manager.system.SetPowerState(false, [this](bool success) {
          DEBUG_PRINTF("Pop-up dismissal power-off success %u\n", success);
          if (success) {
            //go to power-off screen and clear corresponding pop-up
            this->GoToScreen(&this->bbv2_manager.power_off_screen);
            this->bbv2_manager.DeactivatePopups(GUI_POPUP_AMP_FAULT | GUI_POPUP_AMP_SAFETY_ERR);
          }
        });
        break;
      default:
        //other pop-ups can't be dismissed manually - ignore
        break;
    }
  }
}


void BlockBoxV2Screen::Init() {
  //rebuild on active input change
  this->bbv2_manager.system.audio_mgr.RegisterCallback([this](EventSource*, uint32_t) {
    if (this->status_text_override == NULL && this->bbv2_manager.system.audio_mgr.GetActiveInput() != this->currently_drawn_input) {
      this->needs_display_list_rebuild = true;
    }
  }, AUDIO_EVENT_INPUT_UPDATE);

  //rebuild on Bluetooth device name change
  this->bbv2_manager.system.btrx_if.RegisterCallback([this](EventSource*, uint32_t) {
    if (this->status_text_override == NULL && this->currently_drawn_input == AUDIO_INPUT_BLUETOOTH) {
      this->needs_display_list_rebuild = true;
    }
  }, MODIF_BTRX_EVENT_DEVICE_UPDATE);

  //rebuild on battery presence or SoC change, or on shutdown update (pop-up update)
  this->bbv2_manager.system.bat_if.RegisterCallback([this](EventSource*, uint32_t) {
    this->needs_display_list_rebuild = true;
  }, MODIF_BMS_EVENT_PRESENCE_UPDATE | MODIF_BMS_EVENT_SOC_UPDATE | MODIF_BMS_EVENT_SHUTDOWN_UPDATE);

  //rebuild on adapter presence change
  this->bbv2_manager.system.chg_if.RegisterCallback([this](EventSource*, uint32_t) {
    this->needs_display_list_rebuild = true;
  }, MODIF_CHG_EVENT_PRESENCE_UPDATE);
}


//draws the global top status bar with its associated info
void BlockBoxV2Screen::BuildScreenContent() {
  this->driver.CmdDL(DL_SAVE_CONTEXT);
  this->driver.CmdTag(0);

  //status bar and battery icon outline
  this->driver.CmdBeginDraw(EVE_RECTS);
  this->driver.CmdDL(LINE_WIDTH(16));
  this->driver.CmdColorRGB(0x303030);
  this->driver.CmdDL(VERTEX2F(0, 0));
  this->driver.CmdDL(VERTEX2F(16 * 320, 16 * 21));
  this->driver.CmdColorRGB(0xFFFFFF);
  this->driver.CmdDL(VERTEX2F(16 * 278, 16 * 7));
  this->driver.CmdDL(VERTEX2F(16 * 282, 16 * 13));
  this->driver.CmdDL(VERTEX2F(16 * 282, 16 * 3));
  this->driver.CmdDL(VERTEX2F(16 * 316, 16 * 18));

  //process battery status
  bool battery_present = this->bbv2_manager.system.bat_if.IsBatteryPresent();
  BatterySoCLevel battery_soc_level = this->bbv2_manager.system.bat_if.GetSoCConfidenceLevel();
  bool battery_soc_valid = (battery_soc_level != IF_BMS_SOCLVL_INVALID) && (battery_soc_level <= IF_BMS_SOCLVL_MEASURED_REF);
  bool battery_soc_precise = (battery_soc_level == IF_BMS_SOCLVL_MEASURED_REF);
  float battery_soc_fraction = this->bbv2_manager.system.bat_if.GetSoCFraction();
  bool display_battery_soc = battery_present && battery_soc_valid && !isnanf(battery_soc_fraction) && battery_soc_fraction >= 0.0f && battery_soc_fraction <= 1.0f;
  bool adapter_present = this->bbv2_manager.system.chg_if.IsAdapterPresent();

  //battery icon fill
  if (display_battery_soc) {
    //valid: split battery fill
    uint16_t split_x = (uint16_t)roundf(16.0f * 314.0f - 16.0f * 30.0f * battery_soc_fraction);
    //black (empty) section
    if (split_x > 16 * 284) {
      this->driver.CmdColorRGB(0x000000);
      this->driver.CmdDL(VERTEX2F(16 * 284, 16 * 5));
      this->driver.CmdDL(VERTEX2F(split_x, 16 * 16));
    }
    //themed (full) section
    this->driver.CmdColorRGB(this->bbv2_manager.GetThemeColorMain());
    this->driver.CmdDL(VERTEX2F(split_x, 16 * 5));
    this->driver.CmdDL(VERTEX2F(16 * 314, 16 * 16));
  } else {
    //invalid: grey battery fill
    this->driver.CmdColorRGB(0x808080);
    this->driver.CmdDL(VERTEX2F(16 * 284, 16 * 5));
    this->driver.CmdDL(VERTEX2F(16 * 314, 16 * 16));
  }
  this->driver.CmdDL(DL_END);

  //icons for missing battery or adapter
  this->driver.CmdColorRGB(0xFFFFFF);
  if (!battery_present) {
    //battery missing: draw X
    this->driver.CmdBeginDraw(EVE_LINES);
    this->driver.CmdDL(LINE_WIDTH(16));
    this->driver.CmdDL(VERTEX2F(16 * 295 + 8, 16 * 7));
    this->driver.CmdDL(VERTEX2F(16 * 302 + 8, 16 * 14));
    this->driver.CmdDL(VERTEX2F(16 * 302 + 8, 16 * 7));
    this->driver.CmdDL(VERTEX2F(16 * 295 + 8, 16 * 14));
    this->driver.CmdDL(DL_END);
  } else if (adapter_present) {
    //charger/adapter present: lightning icon
    GUIDraws::LightningIconTiny(this->driver, 296, 6, 0xFFFFFF);
  }

  //battery text
  char bat_text[6] = { 0 };
  if (display_battery_soc) {
    uint8_t battery_percentage = (uint8_t)roundf(100.0f * battery_soc_fraction);
    snprintf(bat_text, 6, "%s%u%%", battery_soc_precise ? "" : "~", battery_percentage);
    //remember currently drawn stats
    this->currently_drawn_battery_percent = battery_percentage;
    this->currently_drawn_battery_precise = battery_soc_precise;
  } else {
    strncpy(bat_text, "---%", 5);
    //remember currently drawn stats (invalid defaults)
    this->currently_drawn_battery_percent = 255;
    this->currently_drawn_battery_precise = false;
  }
  this->currently_drawn_battery_valid = display_battery_soc;
  bat_text[5] = 0;
  this->driver.CmdText(275, 3, 26, EVE_OPT_RIGHTX, bat_text);

  char status_text[32] = { 0 };
  if (this->status_text_override == NULL) {
    //active input icon and info
    AudioPathInput input = this->bbv2_manager.system.audio_mgr.GetActiveInput();
    switch (input) {
      case AUDIO_INPUT_BLUETOOTH:
      {
        //bluetooth
        GUIDraws::BluetoothIconSmall(this->driver, 0, 0, 0xFFFFFF);
        //get device name
        const char* device_name = this->bbv2_manager.system.btrx_if.GetDeviceName();
        if (device_name[0] == 0) {
          //empty name: use default
          strncpy(status_text, "Bluetooth Audio", 31);
        } else {
          //non-empty name: copy or shorten to fit
          BlockBoxV2Screen::CopyOrShortenText(status_text, device_name, 31, SCREEN_TOPBAR_BT_DEVICE_NAME_MAX_WIDTH, 26);
        }
        break;
      }
      case AUDIO_INPUT_USB:
        //USB
        GUIDraws::USBIconSmall(this->driver, 0, 0, 0xFFFFFF);
        strncpy(status_text, "USB Audio", 31);
        break;
      case AUDIO_INPUT_SPDIF:
        //SPDIF
        GUIDraws::SPDIFIconSmall(this->driver, 0, 0, 0xFFFFFF, 0x303030);
        strncpy(status_text, "Optical S/PDIF", 31);
        break;
      default:
        //no input: draw X mark
        this->driver.CmdBeginDraw(EVE_LINES);
        this->driver.CmdDL(LINE_WIDTH(16));
        this->driver.CmdDL(VERTEX2F(16 * 4, 16 * 6));
        this->driver.CmdDL(VERTEX2F(16 * 13, 16 * 15));
        this->driver.CmdDL(VERTEX2F(16 * 13, 16 * 6));
        this->driver.CmdDL(VERTEX2F(16 * 4, 16 * 15));
        this->driver.CmdDL(DL_END);
        strncpy(status_text, "No input", 31);
        break;
    }
    this->currently_drawn_input = input;
    status_text[31] = 0;
    this->driver.CmdText(18, 3, 26, 0, status_text);
  } else {
    //overridden status text: copy or shorten to fit
    BlockBoxV2Screen::CopyOrShortenText(status_text, this->status_text_override, 31, SCREEN_TOPBAR_OVERRIDE_TEXT_MAX_WIDTH, 26);
    status_text[31] = 0;
    this->driver.CmdText(4, 3, 26, 0, status_text);
  }

  this->driver.CmdDL(DL_RESTORE_CONTEXT);
}


void BlockBoxV2Screen::DrawPopupOverlay() {
  uint32_t popup = this->bbv2_manager.GetHighestPopup();

  char popup_title[64] = { 0 };
  char popup_info[64] = { 0 };;
  char popup_detail[64] = { 0 };

  switch (popup) {
    case GUI_POPUP_CHG_FAULT:
    {
      strncpy(popup_title, "Charging Fault", 63);
      strncpy(popup_info, "Please remove charger", 63);
      BatterySafetyStatus faults = this->bbv2_manager.system.bat_if.GetSafetyFaults();
      snprintf(popup_detail, 63, "Fault:%s%s%s%s",
               faults.cell_overvoltage ? " COV" : "",
               faults.overcurrent_charge ? " OCC" : "",
               faults.overtemperature_charge ? " OTC" : "",
               faults.undertemperature_charge ? " UTC" : "");
      break;
    }
    case GUI_POPUP_AMP_FAULT:
      strncpy(popup_title, "Amplifier Fault", 63);
      strncpy(popup_info, "Tap to return to standby", 63);
      strncpy(popup_detail, "(Should clear the fault)", 63);
      break;
    case GUI_POPUP_AMP_SAFETY_ERR:
    {
      strncpy(popup_title, "Amplifier Safety Error", 63);
      strncpy(popup_info, "Tap to return to standby", 63);
      PowerAmpErrWarnSource source = this->bbv2_manager.system.amp_if.GetSafetyErrorSource();
      snprintf(popup_detail, 63, "Type:%s%s%s%s%s%s%s%s%s, Source:%s%s%s%s%s",
               source.current_rms_instantaneous ? " Ii" : "",
               source.current_rms_fast ? " If" : "",
               source.current_rms_slow ? " Is" : "",
               source.power_average_instantaneous ? " Pi" : "",
               source.power_average_fast ? " Pf" : "",
               source.power_average_slow ? " Ps" : "",
               source.power_apparent_instantaneous ? " Qi" : "",
               source.power_apparent_fast ? " Qf" : "",
               source.power_apparent_slow ? " Qs" : "",
               source.channel_a ? " A" : "",
               source.channel_b ? " B" : "",
               source.channel_c ? " C" : "",
               source.channel_d ? " D" : "",
               source.channel_sum ? " S" : "");
      break;
    }
    case GUI_POPUP_AUTO_SHUTDOWN:
      snprintf(popup_title, 63, "Auto-Shutdown in %us", (this->bbv2_manager.system.bat_if.GetShutdownCountdownMS() + 999) / 1000);
      strncpy(popup_info, "Tap to cancel", 63);
      strncpy(popup_detail, "Due to inactivity - configurable in settings", 63);
      break;
    case GUI_POPUP_EOD_SHUTDOWN:
      strncpy(popup_title, "Battery Low", 63);
      snprintf(popup_info, 63, "Shutting down in %us", (this->bbv2_manager.system.bat_if.GetShutdownCountdownMS() + 999) / 1000);
      strncpy(popup_detail, "Please charge the battery as soon as possible", 63);
      break;
    case GUI_POPUP_FULL_SHUTDOWN:
      snprintf(popup_title, 63, "Full Shutdown in %us", (this->bbv2_manager.system.bat_if.GetShutdownCountdownMS() + 999) / 1000);
      strncpy(popup_info, "Cannot be cancelled", 63);
      strncpy(popup_detail, "Battery will be unavailable until manually restarted", 63);
      break;
    default:
      //no pop-up
      return;
  }
  popup_title[63] = 0;
  popup_info[63] = 0;
  popup_detail[63] = 0;

  //draw pop-up
  this->driver.CmdDL(DL_SAVE_CONTEXT);
  this->driver.CmdDL(COLOR_MASK(1, 1, 1, 1));
  this->driver.CmdTagMask(true);

  //background overlay
  this->driver.CmdTag(SCREEN_POPUP_TAG);
  this->driver.CmdBeginDraw(EVE_RECTS);
  this->driver.CmdDL(LINE_WIDTH(16));
  this->driver.CmdColorRGB(0x000000);
  this->driver.CmdColorA(160);
  this->driver.CmdDL(VERTEX2F(0, 0));
  this->driver.CmdDL(VERTEX2F(16 * 320, 16 * 240));

  //pop-up outline
  this->driver.CmdDL(LINE_WIDTH(80));
  this->driver.CmdColorRGB(0xFF0000);
  this->driver.CmdColorA(255);
  this->driver.CmdDL(VERTEX2F(16 * 40, 16 * 80));
  this->driver.CmdDL(VERTEX2F(16 * 279, 16 * 159));

  //pop-up background
  this->driver.CmdDL(LINE_WIDTH(32));
  this->driver.CmdColorRGB(0x402020);
  this->driver.CmdDL(VERTEX2F(16 * 40, 16 * 80));
  this->driver.CmdDL(VERTEX2F(16 * 279, 16 * 159));
  this->driver.CmdDL(DL_END);

  //text
  this->driver.CmdColorRGB(0xFFFFFF);
  this->driver.CmdText(160, 100, 28, EVE_OPT_CENTER, popup_title);
  this->driver.CmdText(160, 125, 27, EVE_OPT_CENTER, popup_info);
  this->driver.CmdText(160, 142, 20, EVE_OPT_CENTER, popup_detail);

  this->driver.CmdDL(DL_RESTORE_CONTEXT);
}


void BlockBoxV2Screen::GoToScreen(GUIScreen* screen) {
  this->OnScreenExit();
  this->bbv2_manager.SetScreen(screen);
}

void BlockBoxV2Screen::OnScreenExit() {
  //nothing to do in the base version
}


