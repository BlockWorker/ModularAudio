/*
 * settings_screen_display.cpp
 *
 *  Created on: Aug 10, 2025
 *      Author: Alex
 */


#include "settings_screen_display.h"
#include "bbv2_gui_manager.h"
#include "system.h"
#include "gui_common_draws.h"


//settings tab index
#define SCREEN_DISPLAY_TAB_INDEX 1

//min and max brightness selectable
#define SCREEN_DISPLAY_BRIGHTNESS_MIN 5
#define SCREEN_DISPLAY_BRIGHTNESS_MAX 127

//chroma and luma for main and dark theme colours
#define SCREEN_DISPLAY_THEME_MAIN_CHROMA 1.0f
#define SCREEN_DISPLAY_THEME_MAIN_LUMA 0.335f
#define SCREEN_DISPLAY_THEME_DARK_CHROMA 0.3765f
#define SCREEN_DISPLAY_THEME_DARK_LUMA 0.1166f

//touch tags
//backlight brightness
#define SCREEN_DISPLAY_TAG_BRIGHTNESS 60
//display sleep delay
#define SCREEN_DISPLAY_TAG_SLEEP_TIME 61
//display theme colour
#define SCREEN_DISPLAY_TAG_THEME 62
//touch calibration button
#define SCREEN_DISPLAY_TAG_TOUCH_CAL 63
//datetime year +
#define SCREEN_DISPLAY_TAG_DT_YEAR_UP 64
//datetime year -
#define SCREEN_DISPLAY_TAG_DT_YEAR_DN 65
//datetime month +
#define SCREEN_DISPLAY_TAG_DT_MONTH_UP 66
//datetime month -
#define SCREEN_DISPLAY_TAG_DT_MONTH_DN 67
//datetime day +
#define SCREEN_DISPLAY_TAG_DT_DAY_UP 68
//datetime day -
#define SCREEN_DISPLAY_TAG_DT_DAY_DN 69
//datetime hour +
#define SCREEN_DISPLAY_TAG_DT_HOUR_UP 70
//datetime hour -
#define SCREEN_DISPLAY_TAG_DT_HOUR_DN 71
//datetime minute +
#define SCREEN_DISPLAY_TAG_DT_MIN_UP 72
//datetime minute -
#define SCREEN_DISPLAY_TAG_DT_MIN_DN 73
//datetime second +
#define SCREEN_DISPLAY_TAG_DT_SEC_UP 74
//datetime second -
#define SCREEN_DISPLAY_TAG_DT_SEC_DN 75
//datetime hour mode (24h/AM/PM)
#define SCREEN_DISPLAY_TAG_DT_HOUR_MODE 76
//datetime apply edit
#define SCREEN_DISPLAY_TAG_DT_APPLY 77
//datetime cancel edit
#define SCREEN_DISPLAY_TAG_DT_CANCEL 78


SettingsScreenDisplay::SettingsScreenDisplay(BlockBoxV2GUIManager& manager) :
    SettingsScreenBase(manager, SCREEN_DISPLAY_TAB_INDEX), page_index(0) {}


void SettingsScreenDisplay::EnsureEditingDateTime() {
  if (!this->editing_datetime) {
    this->edit_dt = this->bbv2_manager.system.rtc_if.GetDateTime();
    this->editing_datetime = true;
  }
}

void SettingsScreenDisplay::ClampEditingDayToMonth() {
  uint8_t new_month_length = RTCInterface::GetMonthLength(this->edit_dt.month, this->edit_dt.year);
  if (this->edit_dt.day > new_month_length) {
    this->edit_dt.day = new_month_length;
  }
}

void SettingsScreenDisplay::HandleTouch(const GUITouchState& state) noexcept {
  switch (state.initial_tag) {
    case SCREEN_SETTINGS_TAG_TAB_1:
      if (state.released && state.tag == state.initial_tag) {
        //go to next page
        this->page_index = (this->page_index + 1) % 2;
        this->needs_display_list_rebuild = true;
      }
      return;
    case SCREEN_DISPLAY_TAG_BRIGHTNESS:
    {
      //interpolate slider value
      uint8_t brightness = SCREEN_DISPLAY_BRIGHTNESS_MIN + (uint8_t)roundf(((float)state.tracker_value / (float)UINT16_MAX) * (float)(SCREEN_DISPLAY_BRIGHTNESS_MAX - SCREEN_DISPLAY_BRIGHTNESS_MIN));
      if (brightness > SCREEN_DISPLAY_BRIGHTNESS_MAX) {
        brightness = SCREEN_DISPLAY_BRIGHTNESS_MAX;
      }
      this->bbv2_manager.SetDisplayBrightness(brightness);
      this->needs_existing_list_update = true;
      return;
    }
    case SCREEN_DISPLAY_TAG_SLEEP_TIME:
    {
      //interpret slider value
      uint8_t sleep_time_step = (uint8_t)roundf(((float)state.tracker_value / (float)UINT16_MAX) * 7.0f);
      uint32_t sleep_time;
      switch (sleep_time_step) {
        case 0:
          sleep_time = 5000;
          break;
        case 1:
          sleep_time = 10000;
          break;
        case 2:
          sleep_time = 20000;
          break;
        case 3:
          sleep_time = 30000;
          break;
        case 4:
          sleep_time = 60000;
          break;
        case 5:
          sleep_time = 120000;
          break;
        case 6:
          sleep_time = 300000;
          break;
        default:
          sleep_time = UINT32_MAX;
          break;
      }
      this->bbv2_manager.SetDisplaySleepTimeoutMS(sleep_time);
      this->needs_existing_list_update = true;
      return;
    }
    case SCREEN_DISPLAY_TAG_THEME:
    {
      //interpolate slider value
      float hue = ((float)state.tracker_value / (float)UINT16_MAX) * 360.0f;
      //calculate and apply theme colours
      uint32_t theme_main = BlockBoxV2GUIManager::ColorHCLToRGB(hue, SCREEN_DISPLAY_THEME_MAIN_CHROMA, SCREEN_DISPLAY_THEME_MAIN_LUMA);
      uint32_t theme_dark = BlockBoxV2GUIManager::ColorHCLToRGB(hue, SCREEN_DISPLAY_THEME_DARK_CHROMA, SCREEN_DISPLAY_THEME_DARK_LUMA);
      this->bbv2_manager.SetThemeColors(theme_main, theme_dark);
      this->needs_display_list_rebuild = true;
      return;
    }
    case SCREEN_DISPLAY_TAG_TOUCH_CAL:
      if (state.released && state.tag == state.initial_tag) {
        //start touch calibration, return back to this screen afterwards
        this->bbv2_manager.touch_cal_screen.SetReturnScreen(this);
        this->GoToScreen(&this->bbv2_manager.touch_cal_screen);
      }
      return;
    case SCREEN_DISPLAY_TAG_DT_YEAR_UP:
      if (state.tag == state.initial_tag && (state.long_press ? state.long_press_tick : state.released)) {
        this->EnsureEditingDateTime();
        this->edit_dt.year++;
        if (this->edit_dt.year > 2199) {
          this->edit_dt.year = 2000;
        }
        this->ClampEditingDayToMonth();
        this->edit_dt.weekday = RTCInterface::GetWeekday(this->edit_dt.day, this->edit_dt.month, this->edit_dt.year);
        this->needs_display_list_rebuild = true;
      }
      return;
    case SCREEN_DISPLAY_TAG_DT_YEAR_DN:
      if (state.tag == state.initial_tag && (state.long_press ? state.long_press_tick : state.released)) {
        this->EnsureEditingDateTime();
        this->edit_dt.year--;
        if (this->edit_dt.year < 2000) {
          this->edit_dt.year = 2199;
        }
        this->ClampEditingDayToMonth();
        this->edit_dt.weekday = RTCInterface::GetWeekday(this->edit_dt.day, this->edit_dt.month, this->edit_dt.year);
        this->needs_display_list_rebuild = true;
      }
      return;
    case SCREEN_DISPLAY_TAG_DT_MONTH_UP:
      if (state.tag == state.initial_tag && (state.long_press ? state.long_press_tick : state.released)) {
        this->EnsureEditingDateTime();
        this->edit_dt.month++;
        if (this->edit_dt.month > 12) {
          this->edit_dt.month = 1;
        }
        this->ClampEditingDayToMonth();
        this->edit_dt.weekday = RTCInterface::GetWeekday(this->edit_dt.day, this->edit_dt.month, this->edit_dt.year);
        this->needs_display_list_rebuild = true;
      }
      return;
    case SCREEN_DISPLAY_TAG_DT_MONTH_DN:
      if (state.tag == state.initial_tag && (state.long_press ? state.long_press_tick : state.released)) {
        this->EnsureEditingDateTime();
        this->edit_dt.month--;
        if (this->edit_dt.month < 1) {
          this->edit_dt.month = 12;
        }
        this->ClampEditingDayToMonth();
        this->edit_dt.weekday = RTCInterface::GetWeekday(this->edit_dt.day, this->edit_dt.month, this->edit_dt.year);
        this->needs_display_list_rebuild = true;
      }
      return;
    case SCREEN_DISPLAY_TAG_DT_DAY_UP:
      if (state.tag == state.initial_tag && (state.long_press ? state.long_press_tick : state.released)) {
        this->EnsureEditingDateTime();
        this->edit_dt.day++;
        if (this->edit_dt.day > RTCInterface::GetMonthLength(this->edit_dt.month, this->edit_dt.year)) {
          this->edit_dt.day = 1;
        }
        this->edit_dt.weekday = RTCInterface::GetWeekday(this->edit_dt.day, this->edit_dt.month, this->edit_dt.year);
        this->needs_display_list_rebuild = true;
      }
      return;
    case SCREEN_DISPLAY_TAG_DT_DAY_DN:
      if (state.tag == state.initial_tag && (state.long_press ? state.long_press_tick : state.released)) {
        this->EnsureEditingDateTime();
        this->edit_dt.day--;
        if (this->edit_dt.day < 1) {
          this->edit_dt.day = RTCInterface::GetMonthLength(this->edit_dt.month, this->edit_dt.year);
        }
        this->edit_dt.weekday = RTCInterface::GetWeekday(this->edit_dt.day, this->edit_dt.month, this->edit_dt.year);
        this->needs_display_list_rebuild = true;
      }
      return;
    case SCREEN_DISPLAY_TAG_DT_HOUR_UP:
      if (state.tag == state.initial_tag && (state.long_press ? state.long_press_tick : state.released)) {
        this->EnsureEditingDateTime();
        this->edit_dt.hours++;
        if (this->edit_dt.hour_mode == IF_RTC_24H) {
          if (this->edit_dt.hours > 23) {
            this->edit_dt.hours = 0;
          }
        } else {
          if (this->edit_dt.hours == 12) {
            this->edit_dt.hour_mode = (this->edit_dt.hour_mode == IF_RTC_12H_AM) ? IF_RTC_12H_PM : IF_RTC_12H_AM;
          } else if (this->edit_dt.hours > 12) {
            this->edit_dt.hours = 1;
          }
        }
        this->needs_display_list_rebuild = true;
      }
      return;
    case SCREEN_DISPLAY_TAG_DT_HOUR_DN:
      if (state.tag == state.initial_tag && (state.long_press ? state.long_press_tick : state.released)) {
        this->EnsureEditingDateTime();
        if (this->edit_dt.hour_mode == IF_RTC_24H) {
          if (this->edit_dt.hours == 0) {
            this->edit_dt.hours = 23;
          } else {
            this->edit_dt.hours--;
          }
        } else {
          this->edit_dt.hours--;
          if (this->edit_dt.hours == 11) {
            this->edit_dt.hour_mode = (this->edit_dt.hour_mode == IF_RTC_12H_AM) ? IF_RTC_12H_PM : IF_RTC_12H_AM;
          } else if (this->edit_dt.hours < 1) {
            this->edit_dt.hours = 12;
          }
        }
        this->needs_display_list_rebuild = true;
      }
      return;
    case SCREEN_DISPLAY_TAG_DT_MIN_UP:
      if (state.tag == state.initial_tag && (state.long_press ? state.long_press_tick : state.released)) {
        this->EnsureEditingDateTime();
        this->edit_dt.minutes++;
        if (this->edit_dt.minutes > 59) {
          this->edit_dt.minutes = 0;
        }
        this->needs_display_list_rebuild = true;
      }
      return;
    case SCREEN_DISPLAY_TAG_DT_MIN_DN:
      if (state.tag == state.initial_tag && (state.long_press ? state.long_press_tick : state.released)) {
        this->EnsureEditingDateTime();
        if (this->edit_dt.minutes == 0) {
          this->edit_dt.minutes = 59;
        } else {
          this->edit_dt.minutes--;
        }
        this->needs_display_list_rebuild = true;
      }
      return;
    case SCREEN_DISPLAY_TAG_DT_SEC_UP:
      if (state.tag == state.initial_tag && (state.long_press ? state.long_press_tick : state.released)) {
        this->EnsureEditingDateTime();
        this->edit_dt.seconds++;
        if (this->edit_dt.seconds > 59) {
          this->edit_dt.seconds = 0;
        }
        this->needs_display_list_rebuild = true;
      }
      return;
    case SCREEN_DISPLAY_TAG_DT_SEC_DN:
      if (state.tag == state.initial_tag && (state.long_press ? state.long_press_tick : state.released)) {
        this->EnsureEditingDateTime();
        if (this->edit_dt.seconds == 0) {
          this->edit_dt.seconds = 59;
        } else {
          this->edit_dt.seconds--;
        }
        this->needs_display_list_rebuild = true;
      }
      return;
    case SCREEN_DISPLAY_TAG_DT_HOUR_MODE:
      if (state.tag == state.initial_tag && (state.long_press ? state.long_press_tick : state.released)) {
        this->EnsureEditingDateTime();
        if (this->edit_dt.hour_mode == IF_RTC_24H) {
          //convert from 24h to 12h
          this->edit_dt.hour_mode = (this->edit_dt.hours >= 12) ? IF_RTC_12H_PM : IF_RTC_12H_AM;
          if (this->edit_dt.hours == 0) {
            this->edit_dt.hours = 12;
          } else {
            this->edit_dt.hours = (this->edit_dt.hours - 1) % 12 + 1;
          }
        } else {
          //convert from 12h to 24h
          this->edit_dt.hours = (this->edit_dt.hours % 12) + ((this->edit_dt.hour_mode == IF_RTC_12H_PM) ? 12 : 0);
          this->edit_dt.hour_mode = IF_RTC_24H;
        }
        this->needs_display_list_rebuild = true;
      }
      return;
    case SCREEN_DISPLAY_TAG_DT_APPLY:
      if (this->editing_datetime && state.released && state.tag == state.initial_tag) {
        //no longer editing
        this->editing_datetime = false;
        //apply new date+time
        this->bbv2_manager.system.rtc_if.SetDateTime(this->edit_dt, [&](bool success) {
          DEBUG_PRINTF("RTC settings date+time apply success %u\n", success);
          //once done: refresh screen
          this->needs_display_list_rebuild = true;
        });
      }
      return;
    case SCREEN_DISPLAY_TAG_DT_CANCEL:
      if (this->editing_datetime && state.released && state.tag == state.initial_tag) {
        //no longer editing, refresh screen
        this->editing_datetime = false;
        this->needs_display_list_rebuild = true;
      }
      return;
  }

  //base handling: tab switching etc
  this->SettingsScreenBase::HandleTouch(state);
}


void SettingsScreenDisplay::Init() {
  //allow base handling
  this->SettingsScreenBase::Init();

  //redraw second page (datetime) on RTC seconds update, if not editing
  this->bbv2_manager.system.rtc_if.RegisterCallback([&](EventSource*, uint32_t) {
    if (this->page_index == 1 && !this->editing_datetime) {
      this->needs_display_list_rebuild = true;
    }
  }, MODIF_RTC_EVENT_SECOND_UPDATE);
}



void SettingsScreenDisplay::BuildScreenContent() {
  this->driver.CmdTag(0);

  uint32_t theme_colour_main = this->bbv2_manager.GetThemeColorMain();
  uint32_t theme_colour_dark = this->bbv2_manager.GetThemeColorDark();

  this->driver.CmdColorRGB(0xFFFFFF);
  this->driver.CmdFGColor(theme_colour_main);
  this->driver.CmdBGColor(0x808080);

  switch (this->page_index) {
    case 0:
    {
      //display settings page
      //labels
      this->driver.CmdText(7, 76, 28, 0, "Display");
      this->driver.CmdText(47, 154, 27, EVE_OPT_CENTERY | EVE_OPT_RIGHTX, "Sleep");
      this->driver.CmdText(108, 174, 26, EVE_OPT_CENTERX, "Theme");
      this->driver.CmdText(264, 174, 26, EVE_OPT_CENTERX, "Touch");
      //brightness icon instead of label
      GUIDraws::BrightnessIconMedium(this->driver, 11, 101, 0xFFFFFF, 0x000000);

      //interpret sleep delay
      uint32_t sleep_delay_ms = this->bbv2_manager.GetDisplaySleepTimeoutMS();
      const char* sleep_delay_text;
      uint8_t sleep_delay_step;
      if (sleep_delay_ms == UINT32_MAX) {
        sleep_delay_text = "Never";
        sleep_delay_step = 7;
      } else if (sleep_delay_ms > 120000) {
        sleep_delay_text = "5 min";
        sleep_delay_step = 6;
      } else if (sleep_delay_ms > 60000) {
        sleep_delay_text = "2 min";
        sleep_delay_step = 5;
      } else if (sleep_delay_ms > 30000) {
        sleep_delay_text = "1 min";
        sleep_delay_step = 4;
      } else if (sleep_delay_ms > 20000) {
        sleep_delay_text = "30 s";
        sleep_delay_step = 3;
      } else if (sleep_delay_ms > 10000) {
        sleep_delay_text = "20 s";
        sleep_delay_step = 2;
      } else if (sleep_delay_ms > 5000) {
        sleep_delay_text = "10 s";
        sleep_delay_step = 1;
      } else {
        sleep_delay_text = "5 s ";
        sleep_delay_step = 0;
      }

      //sleep delay slider value display
      this->sleep_time_value_oidx = this->SaveNextCommandOffset();
      this->driver.CmdText(272, 154, 26, EVE_OPT_CENTERY, sleep_delay_text);

      //theme colour gradient
      this->driver.CmdDL(DL_SAVE_CONTEXT);
      this->driver.CmdScissor(15, 194, 32, 34);
      this->driver.CmdGradient(15, 0, 0xFF0000, 47, 0, 0x717100);
      this->driver.CmdScissor(47, 194, 20, 34);
      this->driver.CmdGradient(47, 0, 0x717100, 67, 0, 0x00A200);
      this->driver.CmdScissor(67, 194, 11, 34);
      this->driver.CmdGradient(67, 0, 0x00A200, 78, 0, 0x00BD00);
      this->driver.CmdScissor(78, 194, 9, 34);
      this->driver.CmdGradient(78, 0, 0x00BD00, 87, 0, 0x00B500);
      this->driver.CmdScissor(87, 194, 23, 34);
      this->driver.CmdGradient(87, 0, 0x00B500, 110, 0, 0x00A0A0);
      this->driver.CmdScissor(110, 194, 20, 34);
      this->driver.CmdGradient(110, 0, 0x00A0A0, 130, 0, 0x0060FF);
      this->driver.CmdScissor(130, 194, 12, 34);
      this->driver.CmdGradient(130, 0, 0x0060FF, 142, 0, 0x3737FF);
      this->driver.CmdScissor(142, 194, 22, 34);
      this->driver.CmdGradient(142, 0, 0x3737FF, 164, 0, 0xB700FF);
      this->driver.CmdScissor(164, 194, 9, 34);
      this->driver.CmdGradient(164, 0, 0xB700FF, 173, 0, 0xEA00EA);
      this->driver.CmdScissor(173, 194, 32, 34);
      this->driver.CmdGradient(173, 0, 0xEA00EA, 205, 0, 0xFF0000);
      this->driver.CmdDL(DL_RESTORE_CONTEXT);
      this->driver.CmdBeginDraw(EVE_LINE_STRIP);
      this->driver.CmdDL(LINE_WIDTH(18));
      this->driver.CmdDL(VERTEX2F(16 * 14, 16 * 194));
      this->driver.CmdDL(VERTEX2F(16 * 205, 16 * 194));
      this->driver.CmdDL(VERTEX2F(16 * 205, 16 * 227));
      this->driver.CmdDL(VERTEX2F(16 * 14, 16 * 227));
      this->driver.CmdDL(VERTEX2F(16 * 14, 16 * 194));
      this->driver.CmdDL(DL_END);

      //calculate brightness slider step
      uint8_t brightness_slider_step = this->bbv2_manager.GetDisplayBrightness();
      if (brightness_slider_step < SCREEN_DISPLAY_BRIGHTNESS_MIN) {
        brightness_slider_step = 0;
      } else if (brightness_slider_step > SCREEN_DISPLAY_BRIGHTNESS_MAX) {
        brightness_slider_step = SCREEN_DISPLAY_BRIGHTNESS_MAX - SCREEN_DISPLAY_BRIGHTNESS_MIN;
      } else {
        brightness_slider_step -= SCREEN_DISPLAY_BRIGHTNESS_MIN;
      }

      //controls
      this->driver.CmdTagMask(true);
      //brightness slider
      this->driver.CmdTag(SCREEN_DISPLAY_TAG_BRIGHTNESS);
      this->brightness_slider_oidx = this->SaveNextCommandOffset();
      this->driver.CmdSlider(63, 115, 193, 11, 0, brightness_slider_step, SCREEN_DISPLAY_BRIGHTNESS_MAX - SCREEN_DISPLAY_BRIGHTNESS_MIN);
      this->driver.CmdTrack(61, 110, 198, 21, SCREEN_DISPLAY_TAG_BRIGHTNESS);
      this->driver.CmdInvisibleRect(50, 109, 219, 23);
      //sleep delay slider
      this->driver.CmdTag(SCREEN_DISPLAY_TAG_SLEEP_TIME);
      this->sleep_time_slider_oidx = this->SaveNextCommandOffset();
      this->driver.CmdSlider(63, 148, 193, 11, 0, sleep_delay_step, 7);
      this->driver.CmdTrack(61, 143, 198, 21, SCREEN_DISPLAY_TAG_SLEEP_TIME);
      this->driver.CmdInvisibleRect(50, 142, 219, 23);
      //theme track (invisible)
      this->driver.CmdTag(SCREEN_DISPLAY_TAG_THEME);
      this->driver.CmdInvisibleRect(3, 192, 213, 37);
      this->driver.CmdTrack(14, 194, 192, 34, SCREEN_DISPLAY_TAG_THEME);
      //calibrate button
      this->driver.CmdFGColor(theme_colour_dark);
      this->driver.CmdTag(SCREEN_DISPLAY_TAG_TOUCH_CAL);
      this->driver.CmdBeginDraw(EVE_RECTS);
      this->driver.CmdDL(LINE_WIDTH(40));
      this->driver.CmdDL(VERTEX2F(16 * 226, 16 * 194));
      this->driver.CmdDL(VERTEX2F(16 * 304, 16 * 227));
      this->driver.CmdDL(DL_END);
      this->driver.CmdButton(225, 193, 80, 35, 27, EVE_OPT_FLAT, "Calibrate");
      break;
    }
    case 1:
    {
      //datetime settings page
      //labels
      this->driver.CmdText(7, 76, 28, 0, "Date & Time");
      this->driver.CmdText(85, 140, 28, EVE_OPT_CENTER, "-");
      this->driver.CmdText(123, 140, 28, EVE_OPT_CENTER, "-");
      this->driver.CmdText(85, 195, 28, EVE_OPT_CENTER, ":");
      this->driver.CmdText(123, 195, 28, EVE_OPT_CENTER, ":");
      //date+time display
      RTCDateTime datetime = this->editing_datetime ? this->edit_dt : this->bbv2_manager.system.rtc_if.GetDateTime();
      this->driver.CmdNumber(54, 140, 28, EVE_OPT_CENTER, datetime.year);
      this->driver.CmdNumber(104, 140, 28, EVE_OPT_CENTER | 2, datetime.month);
      this->driver.CmdNumber(142, 140, 28, EVE_OPT_CENTER | 2, datetime.day);
      this->driver.CmdText(171, 140, 28, EVE_OPT_CENTERY, RTCInterface::GetWeekdayName(datetime.weekday));
      this->driver.CmdNumber(66, 195, 28, EVE_OPT_CENTER | 2, datetime.hours);
      this->driver.CmdNumber(104, 195, 28, EVE_OPT_CENTER | 2, datetime.minutes);
      this->driver.CmdNumber(142, 195, 28, EVE_OPT_CENTER | 2, datetime.seconds);
      this->driver.CmdText(171, 195, 28, EVE_OPT_CENTERY, (datetime.hour_mode == IF_RTC_24H) ? "24h" : ((datetime.hour_mode == IF_RTC_12H_PM) ? "PM" : "AM"));
      //button indicators
      this->driver.CmdBeginDraw(EVE_LINES);
      this->driver.CmdDL(LINE_WIDTH(16));
      this->driver.CmdColorRGB(theme_colour_main);
      this->driver.CmdDL(VERTEX2F(16 * 50, 16 * 127));
      this->driver.CmdDL(VERTEX2F(16 * 54, 16 * 123));
      this->driver.CmdDL(VERTEX2F(16 * 54, 16 * 123));
      this->driver.CmdDL(VERTEX2F(16 * 58, 16 * 127));
      this->driver.CmdDL(VERTEX2F(16 * 50, 16 * 153));
      this->driver.CmdDL(VERTEX2F(16 * 54, 16 * 157));
      this->driver.CmdDL(VERTEX2F(16 * 54, 16 * 157));
      this->driver.CmdDL(VERTEX2F(16 * 58, 16 * 153));
      this->driver.CmdDL(VERTEX2F(16 * 100, 16 * 127));
      this->driver.CmdDL(VERTEX2F(16 * 104, 16 * 123));
      this->driver.CmdDL(VERTEX2F(16 * 104, 16 * 123));
      this->driver.CmdDL(VERTEX2F(16 * 108, 16 * 127));
      this->driver.CmdDL(VERTEX2F(16 * 100, 16 * 153));
      this->driver.CmdDL(VERTEX2F(16 * 104, 16 * 157));
      this->driver.CmdDL(VERTEX2F(16 * 104, 16 * 157));
      this->driver.CmdDL(VERTEX2F(16 * 108, 16 * 153));
      this->driver.CmdDL(VERTEX2F(16 * 138, 16 * 127));
      this->driver.CmdDL(VERTEX2F(16 * 142, 16 * 123));
      this->driver.CmdDL(VERTEX2F(16 * 142, 16 * 123));
      this->driver.CmdDL(VERTEX2F(16 * 146, 16 * 127));
      this->driver.CmdDL(VERTEX2F(16 * 138, 16 * 153));
      this->driver.CmdDL(VERTEX2F(16 * 142, 16 * 157));
      this->driver.CmdDL(VERTEX2F(16 * 142, 16 * 157));
      this->driver.CmdDL(VERTEX2F(16 * 146, 16 * 153));
      this->driver.CmdDL(VERTEX2F(16 * 62, 16 * 182));
      this->driver.CmdDL(VERTEX2F(16 * 66, 16 * 178));
      this->driver.CmdDL(VERTEX2F(16 * 66, 16 * 178));
      this->driver.CmdDL(VERTEX2F(16 * 70, 16 * 182));
      this->driver.CmdDL(VERTEX2F(16 * 62, 16 * 208));
      this->driver.CmdDL(VERTEX2F(16 * 66, 16 * 212));
      this->driver.CmdDL(VERTEX2F(16 * 66, 16 * 212));
      this->driver.CmdDL(VERTEX2F(16 * 70, 16 * 208));
      this->driver.CmdDL(VERTEX2F(16 * 100, 16 * 182));
      this->driver.CmdDL(VERTEX2F(16 * 104, 16 * 178));
      this->driver.CmdDL(VERTEX2F(16 * 104, 16 * 178));
      this->driver.CmdDL(VERTEX2F(16 * 108, 16 * 182));
      this->driver.CmdDL(VERTEX2F(16 * 100, 16 * 208));
      this->driver.CmdDL(VERTEX2F(16 * 104, 16 * 212));
      this->driver.CmdDL(VERTEX2F(16 * 104, 16 * 212));
      this->driver.CmdDL(VERTEX2F(16 * 108, 16 * 208));
      this->driver.CmdDL(VERTEX2F(16 * 138, 16 * 182));
      this->driver.CmdDL(VERTEX2F(16 * 142, 16 * 178));
      this->driver.CmdDL(VERTEX2F(16 * 142, 16 * 178));
      this->driver.CmdDL(VERTEX2F(16 * 146, 16 * 182));
      this->driver.CmdDL(VERTEX2F(16 * 138, 16 * 208));
      this->driver.CmdDL(VERTEX2F(16 * 142, 16 * 212));
      this->driver.CmdDL(VERTEX2F(16 * 142, 16 * 212));
      this->driver.CmdDL(VERTEX2F(16 * 146, 16 * 208));
      this->driver.CmdDL(VERTEX2F(16 * 212, 16 * 191));
      this->driver.CmdDL(VERTEX2F(16 * 216, 16 * 195));
      this->driver.CmdDL(VERTEX2F(16 * 216, 16 * 195));
      this->driver.CmdDL(VERTEX2F(16 * 212, 16 * 199));
      this->driver.CmdDL(VERTEX2F(16 * 218, 16 * 191));
      this->driver.CmdDL(VERTEX2F(16 * 222, 16 * 195));
      this->driver.CmdDL(VERTEX2F(16 * 222, 16 * 195));
      this->driver.CmdDL(VERTEX2F(16 * 218, 16 * 199));
      this->driver.CmdDL(DL_END);

      //editing icon
      if (this->editing_datetime) {
        GUIDraws::EditIconSmallish(this->driver, 15, 182, 0xFFFFFF);
      }

      //controls
      this->driver.CmdTagMask(true);
      //apply button
      this->driver.CmdColorRGB(0xFFFFFF);
      this->driver.CmdFGColor(this->editing_datetime ? theme_colour_dark : 0x808080);
      this->driver.CmdTag(SCREEN_DISPLAY_TAG_DT_APPLY);
      this->driver.CmdBeginDraw(EVE_RECTS);
      this->driver.CmdDL(LINE_WIDTH(40));
      this->driver.CmdDL(VERTEX2F(16 * 181, 16 * 79));
      this->driver.CmdDL(VERTEX2F(16 * 239, 16 * 100));
      this->driver.CmdDL(DL_END);
      this->driver.CmdButton(180, 78, 60, 23, 27, EVE_OPT_FLAT, "Apply");
      //cancel button
      this->driver.CmdTag(SCREEN_DISPLAY_TAG_DT_CANCEL);
      this->driver.CmdBeginDraw(EVE_RECTS);
      this->driver.CmdDL(LINE_WIDTH(40));
      this->driver.CmdDL(VERTEX2F(16 * 253, 16 * 79));
      this->driver.CmdDL(VERTEX2F(16 * 311, 16 * 100));
      this->driver.CmdDL(DL_END);
      this->driver.CmdButton(252, 78, 60, 23, 27, EVE_OPT_FLAT, "Cancel");
      //edit touch areas (invisible)
      this->driver.CmdBeginDraw(EVE_RECTS);
      this->driver.CmdDL(LINE_WIDTH(16));
      this->driver.CmdDL(COLOR_MASK(0, 0, 0, 0));
      //year up
      this->driver.CmdTag(SCREEN_DISPLAY_TAG_DT_YEAR_UP);
      this->driver.CmdDL(VERTEX2F(16 * 30, 16 * 115));
      this->driver.CmdDL(VERTEX2F(16 * 80, 16 * 139));
      //year down
      this->driver.CmdTag(SCREEN_DISPLAY_TAG_DT_YEAR_DN);
      this->driver.CmdDL(VERTEX2F(16 * 30, 16 * 142));
      this->driver.CmdDL(VERTEX2F(16 * 80, 16 * 165));
      //month up
      this->driver.CmdTag(SCREEN_DISPLAY_TAG_DT_MONTH_UP);
      this->driver.CmdDL(VERTEX2F(16 * 91, 16 * 115));
      this->driver.CmdDL(VERTEX2F(16 * 117, 16 * 139));
      //month down
      this->driver.CmdTag(SCREEN_DISPLAY_TAG_DT_MONTH_DN);
      this->driver.CmdDL(VERTEX2F(16 * 91, 16 * 142));
      this->driver.CmdDL(VERTEX2F(16 * 117, 16 * 165));
      //day up
      this->driver.CmdTag(SCREEN_DISPLAY_TAG_DT_DAY_UP);
      this->driver.CmdDL(VERTEX2F(16 * 129, 16 * 115));
      this->driver.CmdDL(VERTEX2F(16 * 155, 16 * 139));
      //day down
      this->driver.CmdTag(SCREEN_DISPLAY_TAG_DT_DAY_DN);
      this->driver.CmdDL(VERTEX2F(16 * 129, 16 * 142));
      this->driver.CmdDL(VERTEX2F(16 * 155, 16 * 165));
      //hour up
      this->driver.CmdTag(SCREEN_DISPLAY_TAG_DT_HOUR_UP);
      this->driver.CmdDL(VERTEX2F(16 * 54, 16 * 170));
      this->driver.CmdDL(VERTEX2F(16 * 80, 16 * 194));
      //hour down
      this->driver.CmdTag(SCREEN_DISPLAY_TAG_DT_HOUR_DN);
      this->driver.CmdDL(VERTEX2F(16 * 54, 16 * 197));
      this->driver.CmdDL(VERTEX2F(16 * 80, 16 * 220));
      //minute up
      this->driver.CmdTag(SCREEN_DISPLAY_TAG_DT_MIN_UP);
      this->driver.CmdDL(VERTEX2F(16 * 91, 16 * 170));
      this->driver.CmdDL(VERTEX2F(16 * 117, 16 * 194));
      //minute down
      this->driver.CmdTag(SCREEN_DISPLAY_TAG_DT_MIN_DN);
      this->driver.CmdDL(VERTEX2F(16 * 91, 16 * 197));
      this->driver.CmdDL(VERTEX2F(16 * 117, 16 * 220));
      //second up
      this->driver.CmdTag(SCREEN_DISPLAY_TAG_DT_SEC_UP);
      this->driver.CmdDL(VERTEX2F(16 * 129, 16 * 170));
      this->driver.CmdDL(VERTEX2F(16 * 155, 16 * 194));
      //second down
      this->driver.CmdTag(SCREEN_DISPLAY_TAG_DT_SEC_DN);
      this->driver.CmdDL(VERTEX2F(16 * 129, 16 * 197));
      this->driver.CmdDL(VERTEX2F(16 * 155, 16 * 220));
      //24h/AM/PM
      this->driver.CmdTag(SCREEN_DISPLAY_TAG_DT_HOUR_MODE);
      this->driver.CmdDL(VERTEX2F(16 * 170, 16 * 180));
      this->driver.CmdDL(VERTEX2F(16 * 230, 16 * 210));
      this->driver.CmdDL(DL_END);
      this->driver.CmdDL(COLOR_MASK(1, 1, 1, 1));
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
      this->DrawActivePageDot(66);
      break;
    case 1:
      this->DrawActivePageDot(74);
      break;
    default:
      break;
  }

  //popup overlay, if any
  this->DrawPopupOverlay();
}

void SettingsScreenDisplay::UpdateExistingScreenContent() {
  switch (this->page_index) {
    case 0:
    {
      //display settings page
      //interpret sleep delay
      uint32_t sleep_delay_ms = this->bbv2_manager.GetDisplaySleepTimeoutMS();
      const char* sleep_delay_text;
      uint8_t sleep_delay_step;
      if (sleep_delay_ms == UINT32_MAX) {
        sleep_delay_text = "Never";
        sleep_delay_step = 7;
      } else if (sleep_delay_ms > 120000) {
        sleep_delay_text = "5 min";
        sleep_delay_step = 6;
      } else if (sleep_delay_ms > 60000) {
        sleep_delay_text = "2 min";
        sleep_delay_step = 5;
      } else if (sleep_delay_ms > 30000) {
        sleep_delay_text = "1 min";
        sleep_delay_step = 4;
      } else if (sleep_delay_ms > 20000) {
        sleep_delay_text = "30 s";
        sleep_delay_step = 3;
      } else if (sleep_delay_ms > 10000) {
        sleep_delay_text = "20 s";
        sleep_delay_step = 2;
      } else if (sleep_delay_ms > 5000) {
        sleep_delay_text = "10 s";
        sleep_delay_step = 1;
      } else {
        sleep_delay_text = "5 s ";
        sleep_delay_step = 0;
      }

      //calculate brightness slider step
      uint8_t brightness_slider_step = this->bbv2_manager.GetDisplayBrightness();
      if (brightness_slider_step < SCREEN_DISPLAY_BRIGHTNESS_MIN) {
        brightness_slider_step = 0;
      } else if (brightness_slider_step > SCREEN_DISPLAY_BRIGHTNESS_MAX) {
        brightness_slider_step = SCREEN_DISPLAY_BRIGHTNESS_MAX - SCREEN_DISPLAY_BRIGHTNESS_MIN;
      } else {
        brightness_slider_step -= SCREEN_DISPLAY_BRIGHTNESS_MIN;
      }

      //update sleep delay slider value display
      snprintf((char*)this->GetDLCommandPointer(this->sleep_time_value_oidx, 3), 6, "%s", sleep_delay_text);

      //update sliders
      //brightness
      this->ModifyDLCommand16(this->brightness_slider_oidx, 3, 1, brightness_slider_step);
      //sleep delay
      this->ModifyDLCommand16(this->sleep_time_slider_oidx, 3, 1, sleep_delay_step);
      break;
    }
    default:
      break;
  }
}


void SettingsScreenDisplay::OnScreenExit() {
  //reset to first page
  this->page_index = 0;

  //save settings
  this->bbv2_manager.system.eeprom_if.WriteAllDirtySections([](bool success) {
    DEBUG_PRINTF("Display screen exit EEPROM write success %u\n", success);
  });
}

