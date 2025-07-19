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


void BlockBoxV2Screen::Init(BlockBoxV2System* system) {
  if (system == NULL) {
    throw std::invalid_argument("BlockBoxV2Screen Init given null pointer");
  }

  this->system = system;

  //TODO register event handler (with battery, charger, and all input-related modules) to force update
}


//draws the global top status bar with its associated info
void BlockBoxV2Screen::BuildScreenContent() {
  uint32_t theme_color = 0x0060FF; //TODO: use actual theme colour

  this->driver.CmdDL(DL_SAVE_CONTEXT);
  this->driver.CmdTag(0);

  //status bar and battery icon outline
  this->driver.CmdDL(DL_BEGIN | EVE_RECTS);
  this->driver.CmdDL(LINE_WIDTH(16));
  this->driver.CmdDL(DL_COLOR_RGB | 0x303030);
  this->driver.CmdDL(VERTEX2F(0, 0));
  this->driver.CmdDL(VERTEX2F(16 * 320, 16 * 21));
  this->driver.CmdDL(DL_COLOR_RGB | 0xFFFFFF);
  this->driver.CmdDL(VERTEX2F(16 * 278, 16 * 7));
  this->driver.CmdDL(VERTEX2F(16 * 282, 16 * 13));
  this->driver.CmdDL(VERTEX2F(16 * 282, 16 * 3));
  this->driver.CmdDL(VERTEX2F(16 * 316, 16 * 18));

  //battery icon fill - TODO: use actual battery status
  bool battery_soc_precise = false;
  float battery_soc_fraction = 0.8556f;
  bool battery_valid = true && !isnanf(battery_soc_fraction) && battery_soc_fraction >= 0.0f && battery_soc_fraction <= 1.0f;
  if (battery_valid) {
    //valid: split battery fill
    uint16_t split_x = (uint16_t)roundf(16.0f * 314.0f - 16.0f * 30.0f * battery_soc_fraction);
    //black (empty) section
    if (split_x > 16 * 284) {
      this->driver.CmdDL(DL_COLOR_RGB | 0x000000);
      this->driver.CmdDL(VERTEX2F(16 * 284, 16 * 5));
      this->driver.CmdDL(VERTEX2F(split_x, 16 * 16));
    }
    //themed (full) section
    this->driver.CmdDL(DL_COLOR_RGB | (theme_color & 0xFFFFFF));
    this->driver.CmdDL(VERTEX2F(split_x, 16 * 5));
    this->driver.CmdDL(VERTEX2F(16 * 314, 16 * 16));
  } else {
    //invalid: grey battery fill
    this->driver.CmdDL(DL_COLOR_RGB | 0x808080);
    this->driver.CmdDL(VERTEX2F(16 * 284, 16 * 5));
    this->driver.CmdDL(VERTEX2F(16 * 314, 16 * 16));
  }

  this->driver.CmdDL(DL_END);

  bool charging = false; //TODO: use actual charging status
  if (charging) {
    GUIDraws::LightningIconTiny(this->driver, 296, 6);
  }

  //battery text
  this->driver.CmdDL(DL_COLOR_RGB | 0xFFFFFF);
  char bat_text[6] = { 0 };
  if (battery_valid) {
    uint8_t battery_percentage = (uint8_t)roundf(100.0f * battery_soc_fraction);
    snprintf(bat_text, 6, "%s%u%%", battery_soc_precise ? "" : "~", battery_percentage);
  } else {
    strncpy(bat_text, "---%", 5);
  }
  bat_text[5] = 0;
  this->driver.CmdText(275, 3, 26, EVE_OPT_RIGHTX, bat_text);

  //active input icon and info - TODO use actual input information
  uint8_t active_input = 0;
  char input_text[32] = { 0 };
  switch (active_input) {
    case 1:
      //bluetooth - TODO get real device name, with width limitation
      GUIDraws::BluetoothIconSmall(this->driver, 0, 0);
      strncpy(input_text, "Bluetooth Device Name", 31);
      break;
    case 2:
      //USB
      GUIDraws::USBIconSmall(this->driver, 0, 0);
      strncpy(input_text, "USB Audio", 31);
      break;
    case 3:
      //SPDIF
      GUIDraws::SPDIFIconSmall(this->driver, 0, 0, 0x303030);
      strncpy(input_text, "Optical S/PDIF", 31);
      break;
    default:
      //no input: draw X mark
      this->driver.CmdDL(DL_BEGIN | EVE_LINES);
      this->driver.CmdDL(LINE_WIDTH(16));
      this->driver.CmdDL(VERTEX2F(16 * 4, 16 * 6));
      this->driver.CmdDL(VERTEX2F(16 * 13, 16 * 15));
      this->driver.CmdDL(VERTEX2F(16 * 13, 16 * 6));
      this->driver.CmdDL(VERTEX2F(16 * 4, 16 * 15));
      this->driver.CmdDL(DL_END);
      strncpy(input_text, "No input", 31);
      break;
  }
  input_text[31] = 0;
  this->driver.CmdText(18, 3, 26, 0, input_text);

  this->driver.CmdDL(DL_RESTORE_CONTEXT);
}


