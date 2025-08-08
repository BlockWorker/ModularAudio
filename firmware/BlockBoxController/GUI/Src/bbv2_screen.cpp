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


void BlockBoxV2Screen::Init() {
  //rebuild on active input change
  this->bbv2_manager.system.audio_mgr.RegisterCallback([&](EventSource*, uint32_t) {
    if (this->status_text_override == NULL && this->bbv2_manager.system.audio_mgr.GetActiveInput() != this->currently_drawn_input) {
      this->needs_display_list_rebuild = true;
    }
  }, AUDIO_EVENT_INPUT_UPDATE);

  //rebuild on Bluetooth device name change
  this->bbv2_manager.system.btrx_if.RegisterCallback([&](EventSource*, uint32_t) {
    if (this->status_text_override == NULL && this->currently_drawn_input == AUDIO_INPUT_BLUETOOTH) {
      this->needs_display_list_rebuild = true;
    }
  }, MODIF_BTRX_EVENT_DEVICE_UPDATE);

  //TODO register event handler with battery and charger to force list re-build as well
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

  //battery icon fill - TODO: use actual battery status
  bool battery_present = true;
  bool battery_soc_valid = true;
  bool battery_soc_precise = false;
  float battery_soc_fraction = 0.8556f;
  bool display_battery_soc = battery_present && battery_soc_valid && !isnanf(battery_soc_fraction) && battery_soc_fraction >= 0.0f && battery_soc_fraction <= 1.0f;
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

  //icons for missing battery or charging
  bool charging = false; //TODO: use actual charging status
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
  } else if (charging) {
    //battery charging: lightning icon
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
  //TODO: on event, draw tagless translucent cover and popup with message
}


void BlockBoxV2Screen::GoToScreen(GUIScreen* screen) {
  this->OnScreenExit();
  this->bbv2_manager.SetScreen(screen);
}

void BlockBoxV2Screen::OnScreenExit() {
  //TODO I think nothing to do in the base version, but double-check this
}


