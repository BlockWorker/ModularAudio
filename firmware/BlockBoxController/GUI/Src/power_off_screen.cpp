/*
 * power_off_screen.cpp
 *
 *  Created on: Jul 24, 2025
 *      Author: Alex
 */


#include "power_off_screen.h"
#include "bbv2_gui_manager.h"
#include "gui_common_draws.h"
#include "system.h"


//touch tag for power-on button
#define SCREEN_POWEROFF_TAG_PWRON 5

//timeout for automatic calibration, in milliseconds
#define SCREEN_POWEROFF_AUTO_CAL_TIMEOUT_MS 60000
//warning start for automatic calibration
#define SCREEN_POWEROFF_AUTO_CAL_WARNING_MS 10000


PowerOffScreen::PowerOffScreen(BlockBoxV2GUIManager& manager) :
    BlockBoxV2Screen(manager), auto_calibration_scheduled(false), auto_calibration_tick(0), currently_drawn_auto_cal_seconds(0) {
  this->status_text_override = "Standby Mode";
}


void PowerOffScreen::DisplayScreen() {
  //handle auto-calibration timer
  if (this->auto_calibration_scheduled) {
    uint32_t tick_difference = this->auto_calibration_tick - HAL_GetTick();
    if ((int32_t)tick_difference <= 0) {
      //timeout passed: reset this screen and start calibration
      this->auto_calibration_scheduled = false;
      this->auto_calibration_tick = 0;
      this->bbv2_manager.touch_cal_screen.SetReturnScreen(this);
      this->GoToScreen(&this->bbv2_manager.touch_cal_screen);
      return;
    } else if (tick_difference <= SCREEN_POWEROFF_AUTO_CAL_WARNING_MS) {
      //below warning threshold: warning text should be shown, check if we need to update it
      uint8_t warning_seconds = (uint8_t)((tick_difference + 999) / 1000);
      if (warning_seconds != this->currently_drawn_auto_cal_seconds) {
        this->needs_display_list_rebuild = true;
      }
    }
  }

  //base handling
  this->BlockBoxV2Screen::DisplayScreen();
}


void PowerOffScreen::HandleTouch(const GUITouchState& state) noexcept {
  //cancel auto-calibration on any touch release (should avoid cancellation by any "stuck" touches)
  if (this->auto_calibration_scheduled && state.released) {
    DEBUG_PRINTF("PowerOffScreen auto-calibration cancelled\n");
    this->auto_calibration_scheduled = false;
    this->auto_calibration_tick = 0;
    if (this->currently_drawn_auto_cal_seconds > 0) {
      //already changed to warning text: rebuild screen
      this->needs_display_list_rebuild = true;
    }
  }

  //power-on button
  if (state.released && state.tag == SCREEN_POWEROFF_TAG_PWRON && state.initial_tag == SCREEN_POWEROFF_TAG_PWRON) {
    //power on system
    this->bbv2_manager.system.SetPowerState(true, [this](bool success) {
      DEBUG_PRINTF("PowerOffScreen power-on success %u\n", success);
      if (success) {
        //reset this screen and go to main screen
        this->GoToScreen(&this->bbv2_manager.main_screen);
      }
    });
  }

  //allow base handling
  this->BlockBoxV2Screen::HandleTouch(state);
}


void PowerOffScreen::Init() {
  //allow base handling
  this->BlockBoxV2Screen::Init();

  //redraw on RTC minutes update
  this->bbv2_manager.system.rtc_if.RegisterCallback([this](EventSource*, uint32_t) {
    this->needs_display_list_rebuild = true;
  }, MODIF_RTC_EVENT_MINUTE_UPDATE);
}


void PowerOffScreen::ScheduleAutoCalibration() {
  this->auto_calibration_tick = HAL_GetTick() + SCREEN_POWEROFF_AUTO_CAL_TIMEOUT_MS;
  this->auto_calibration_scheduled = true;
}


void PowerOffScreen::BuildScreenContent() {
  this->driver.CmdTag(0);
  this->driver.CmdColorRGB(0xFFFFFF);

  char text_buf[64];

  //main text, or auto-calibration warning) TODO: auto-shutdown warning too
  uint32_t auto_cal_tick_difference = this->auto_calibration_tick - HAL_GetTick();
  if (this->auto_calibration_scheduled && auto_cal_tick_difference <= SCREEN_POWEROFF_AUTO_CAL_WARNING_MS) {
    //show auto-calibration warning
    uint8_t warning_seconds = (uint8_t)((auto_cal_tick_difference + 999) / 1000);
    snprintf(text_buf, 64, "Touch calibration in %u seconds", warning_seconds);
    this->driver.CmdText(160, 165, 28, EVE_OPT_CENTERX, text_buf);
    this->currently_drawn_auto_cal_seconds = warning_seconds;
  } else {
    //normal main text
    this->driver.CmdText(160, 165, 29, EVE_OPT_CENTERX, "BlockBox v2 neo");
    this->currently_drawn_auto_cal_seconds = 0;
  }

  //time+date text
  RTCDateTime datetime = this->bbv2_manager.system.rtc_if.GetDateTime();
  snprintf(text_buf, 64, "%.3s %.3s %u %u - %u:%02u%s", RTCInterface::GetWeekdayName(datetime.weekday), RTCInterface::GetMonthName(datetime.month), datetime.day, datetime.year,
           datetime.hours, datetime.minutes, (datetime.hour_mode == IF_RTC_24H) ? "" : ((datetime.hour_mode == IF_RTC_12H_PM) ? " PM" : " AM"));
  this->driver.CmdText(160, 222, 20, EVE_OPT_CENTERX, text_buf);

  //big power button
  GUIDraws::PowerIconHuge(this->driver, 110, 60, 0xFFFFFF, this->bbv2_manager.GetThemeColorMain(), 0x000000);

  //top status bar
  this->BlockBoxV2Screen::BuildScreenContent();

  //invisible touch area
  this->driver.CmdDL(DL_SAVE_CONTEXT);
  this->driver.CmdBeginDraw(EVE_RECTS);
  this->driver.CmdDL(LINE_WIDTH(16));
  this->driver.CmdDL(COLOR_MASK(0, 0, 0, 0));
  this->driver.CmdTagMask(true);
  this->driver.CmdTag(SCREEN_POWEROFF_TAG_PWRON);
  this->driver.CmdDL(VERTEX2F(16 * 105, 16 * 55));
  this->driver.CmdDL(VERTEX2F(16 * 215, 16 * 165));
  this->driver.CmdDL(DL_END);
  this->driver.CmdDL(DL_RESTORE_CONTEXT);

  //popup overlay, if any
  this->DrawPopupOverlay();
}


void PowerOffScreen::UpdateExistingScreenContent() {
  //nothing to do
}

