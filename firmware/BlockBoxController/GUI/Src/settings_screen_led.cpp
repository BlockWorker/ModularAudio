/*
 * settings_screen_led.cpp
 *
 *  Created on: Sep 10, 2025
 *      Author: Alex
 */


#include "settings_screen_led.h"
#include "bbv2_gui_manager.h"
#include "system.h"
#include "gui_common_draws.h"


//settings tab index
#define SCREEN_LED_TAB_INDEX 2

//min and max brightness selectable
#define SCREEN_LED_BRIGHTNESS_MIN 0.01f
#define SCREEN_LED_BRIGHTNESS_MAX 0.25f

//touch tags
//LED on/off
#define SCREEN_LED_TAG_ON_OFF 80
//LED sound-to-light on/off
#define SCREEN_LED_TAG_STL 81
//LED brightness
#define SCREEN_LED_TAG_BRIGHTNESS 82
//LED hue
#define SCREEN_LED_TAG_HUE 83


SettingsScreenLED::SettingsScreenLED(BlockBoxV2GUIManager& manager) :
    SettingsScreenBase(manager, SCREEN_LED_TAB_INDEX) {}


void SettingsScreenLED::HandleTouch(const GUITouchState& state) noexcept {
  switch (state.initial_tag) {
    case SCREEN_SETTINGS_TAG_TAB_2:
      //only have one page - nothing to do
      return;
    case SCREEN_LED_TAG_ON_OFF:
      if (state.released && state.tag == state.initial_tag) {
        //toggle on/off
        this->bbv2_manager.system.led_mgr.SetOn(!this->bbv2_manager.system.led_mgr.IsOn());
        this->needs_display_list_rebuild = true;
      }
      return;
    case SCREEN_LED_TAG_STL:
      if (state.released && state.tag == state.initial_tag) {
        //TODO: toggle sound-to-light once implemented
      }
      return;
    case SCREEN_LED_TAG_BRIGHTNESS:
    {
      //interpolate slider value
      float brightness = SCREEN_LED_BRIGHTNESS_MIN + (((float)state.tracker_value / (float)UINT16_MAX) * (float)(SCREEN_LED_BRIGHTNESS_MAX - SCREEN_LED_BRIGHTNESS_MIN));
      if (brightness > SCREEN_LED_BRIGHTNESS_MAX) {
        brightness = SCREEN_LED_BRIGHTNESS_MAX;
      }
      this->bbv2_manager.system.led_mgr.SetBrightness(brightness);
      this->needs_existing_list_update = true;
      return;
    }
    case SCREEN_LED_TAG_HUE:
    {
      //interpolate slider value
      float hue = ((float)state.tracker_value / (float)UINT16_MAX) * 360.0f;
      //clamp and apply hue
      if (hue < 0.0f || hue >= 360.0f) {
        hue = 0.0f;
      }
      this->bbv2_manager.system.led_mgr.SetHueDegrees(hue);
      return;
    }
    default:
      break;
  }

  //base handling: tab switching etc
  this->SettingsScreenBase::HandleTouch(state);
}


void SettingsScreenLED::Init() {
  //allow base handling
  this->SettingsScreenBase::Init();

  //TODO: register event handlers if needed
}



void SettingsScreenLED::BuildScreenContent() {
  this->driver.CmdTag(0);

  uint32_t theme_colour_main = this->bbv2_manager.GetThemeColorMain();

  this->driver.CmdColorRGB(0xFFFFFF);
  this->driver.CmdFGColor(theme_colour_main);
  this->driver.CmdBGColor(0x808080);

  //labels
  this->driver.CmdText(7, 76, 28, 0, "LED Lighting");
  this->driver.CmdText(55, 121, 27, EVE_OPT_CENTERY | EVE_OPT_RIGHTX, "Light");
  this->driver.CmdText(242, 121, 27, EVE_OPT_CENTERY | EVE_OPT_RIGHTX, "Sound to Light");
  this->driver.CmdText(160, 174, 26, EVE_OPT_CENTERX, "Hue");
  //brightness icon instead of label
  GUIDraws::BrightnessIconMedium(this->driver, 5, 134, 0xFFFFFF, 0x000000);

  //hue gradient
  this->driver.CmdDL(DL_SAVE_CONTEXT);
  this->driver.CmdScissor(15, 194, 49, 34);
  this->driver.CmdGradient(15, 0, 0xFF0000, 63, 0, 0xFFFF00);
  this->driver.CmdDL(SCISSOR_XY(63, 194));
  this->driver.CmdGradient(63, 0, 0xFFFF00, 112, 0, 0x00FF00);
  this->driver.CmdDL(SCISSOR_XY(112, 194));
  this->driver.CmdGradient(112, 0, 0x00FF00, 160, 0, 0x00FFFF);
  this->driver.CmdDL(SCISSOR_XY(160, 194));
  this->driver.CmdGradient(160, 0, 0x00FFFF, 208, 0, 0x0000FF);
  this->driver.CmdDL(SCISSOR_XY(208, 194));
  this->driver.CmdGradient(208, 0, 0x0000FF, 257, 0, 0xFF00FF);
  this->driver.CmdDL(SCISSOR_XY(257, 194));
  this->driver.CmdGradient(257, 0, 0xFF00FF, 305, 0, 0xFF0000);
  this->driver.CmdDL(DL_RESTORE_CONTEXT);
  this->driver.CmdBeginDraw(EVE_LINE_STRIP);
  this->driver.CmdDL(LINE_WIDTH(18));
  this->driver.CmdDL(VERTEX2F(16 * 14, 16 * 194));
  this->driver.CmdDL(VERTEX2F(16 * 305, 16 * 194));
  this->driver.CmdDL(VERTEX2F(16 * 305, 16 * 227));
  this->driver.CmdDL(VERTEX2F(16 * 14, 16 * 227));
  this->driver.CmdDL(VERTEX2F(16 * 14, 16 * 194));
  this->driver.CmdDL(DL_END);

  //calculate brightness slider step
  uint16_t brightness_slider_step;
  float brightness = this->bbv2_manager.system.led_mgr.GetBrightness();
  if (isnanf(brightness) || brightness < SCREEN_LED_BRIGHTNESS_MIN) {
    brightness_slider_step = 0;
  } else if (brightness > SCREEN_LED_BRIGHTNESS_MAX) {
    brightness_slider_step = UINT16_MAX;
  } else {
    brightness_slider_step = (uint16_t)roundf((brightness - SCREEN_LED_BRIGHTNESS_MIN) / (SCREEN_LED_BRIGHTNESS_MAX - SCREEN_LED_BRIGHTNESS_MIN) * (float)UINT16_MAX);
  }

  //controls
  this->driver.CmdTagMask(true);
  //on/off toggle
  this->driver.CmdTag(SCREEN_LED_TAG_ON_OFF);
  this->driver.CmdToggle(73, 115, 29, 26, 0, this->bbv2_manager.system.led_mgr.IsOn() ? UINT16_MAX : 0, "Off\xFFOn ");
  //sound-to-light toggle - TODO: enable once implemented
  this->driver.CmdTag(SCREEN_LED_TAG_STL);
  this->driver.CmdFGColor(0x606060);
  this->driver.CmdToggle(260, 115, 29, 26, 0, 0, "Off\xFFOn ");
  this->driver.CmdFGColor(theme_colour_main);
  //brightness slider
  this->driver.CmdTag(SCREEN_LED_TAG_BRIGHTNESS);
  this->brightness_slider_oidx = this->SaveNextCommandOffset();
  this->driver.CmdSlider(60, 148, 235, 11, 0, brightness_slider_step, UINT16_MAX);
  this->driver.CmdTrack(58, 143, 240, 21, SCREEN_LED_TAG_BRIGHTNESS);
  this->driver.CmdInvisibleRect(47, 142, 261, 23);
  //hue track (invisible)
  this->driver.CmdTag(SCREEN_LED_TAG_HUE);
  this->driver.CmdInvisibleRect(3, 192, 313, 37);
  this->driver.CmdTrack(14, 194, 292, 34, SCREEN_LED_TAG_HUE);

  this->driver.CmdTag(0);

  //base handling (top status bar and settings tabs)
  this->SettingsScreenBase::BuildScreenContent();
  //add active page dot (only have one page)
  this->DrawActivePageDot(115);

  //popup overlay, if any
  this->DrawPopupOverlay();
}

void SettingsScreenLED::UpdateExistingScreenContent() {
  //calculate brightness slider step
  uint16_t brightness_slider_step;
  float brightness = this->bbv2_manager.system.led_mgr.GetBrightness();
  if (isnanf(brightness) || brightness < SCREEN_LED_BRIGHTNESS_MIN) {
    brightness_slider_step = 0;
  } else if (brightness > SCREEN_LED_BRIGHTNESS_MAX) {
    brightness_slider_step = UINT16_MAX;
  } else {
    brightness_slider_step = (uint16_t)roundf((brightness - SCREEN_LED_BRIGHTNESS_MIN) / (SCREEN_LED_BRIGHTNESS_MAX - SCREEN_LED_BRIGHTNESS_MIN) * (float)UINT16_MAX);
  }

  //update brightness slider
  this->ModifyDLCommand16(this->brightness_slider_oidx, 3, 1, brightness_slider_step);
}


void SettingsScreenLED::OnScreenExit() {
  //save settings
  this->bbv2_manager.system.eeprom_if.WriteAllDirtySections([](bool success) {
    DEBUG_PRINTF("LED screen exit EEPROM write success %u\n", success);
  });
}

