/*
 * settings_screen_power.cpp
 *
 *  Created on: Aug 30, 2025
 *      Author: Alex
 */


#include "settings_screen_power.h"
#include "bbv2_gui_manager.h"
#include "system.h"


//settings tab index
#define SCREEN_POWER_TAB_INDEX 3

//touch tags
//auto-shutdown time
#define SCREEN_POWER_TAG_ASD_TIME 100
//max adapter input current
#define SCREEN_POWER_TAG_INPUT_CURRENT 101
//max charging current
#define SCREEN_POWER_TAG_CHG_CURRENT 102
//charging target voltage state
#define SCREEN_POWER_TAG_CHG_TARGET 103


//auto-shutdown time steps
#define SCREEN_POWER_ASD_STEP_COUNT 6
static const uint32_t _screen_power_asd_steps_ms[SCREEN_POWER_ASD_STEP_COUNT] = {
  60000, //1 min
  300000, //5 min
  600000, //10 min
  1800000, //30 min
  3600000, //60 min
  UINT32_MAX //Never
};
static const char* _screen_power_asd_step_titles[SCREEN_POWER_ASD_STEP_COUNT] = {
  "1 min",
  "5 min",
  "10 min",
  "30 min",
  "1 hour",
  "Never"
};

//input current steps
#define SCREEN_POWER_INPUT_CUR_STEP_SIZE 0.32f
#define SCREEN_POWER_INPUT_CUR_STEPS_MIN 16
#define SCREEN_POWER_INPUT_CUR_STEPS_MAX 63

//charge current steps
#define SCREEN_POWER_CHG_CUR_STEP_SIZE 0.128f
#define SCREEN_POWER_CHG_CUR_STEPS_MIN 4
#define SCREEN_POWER_CHG_CUR_STEPS_MAX 46

//charge voltage target step labels
static const char* _screen_power_chg_target_titles[_PWR_CHG_TARGET_COUNT] = { "~70%", "~75%", "~80%", "~85%", "~90%", "~95%", "100%" };


SettingsScreenPower::SettingsScreenPower(BlockBoxV2GUIManager& manager) :
    SettingsScreenBase(manager, SCREEN_POWER_TAB_INDEX), page_index(0), local_in_current(6.0f), local_chg_current(4.0f), local_chg_target(PWR_CHG_TARGET_70) {}


void SettingsScreenPower::DisplayScreen() {
  //if not actively touching screen, update local slider values to true values
  if (!this->manager.GetTouchState().touched) {
    switch (this->page_index) {
      case 1:
      {
        //max adapter input current
        float in_current = this->bbv2_manager.system.power_mgr.GetAdapterMaxCurrentA();
        if (this->local_in_current != in_current) {
          this->local_in_current = in_current;
          this->needs_existing_list_update = true;
        }
        //max charging current
        float chg_current = this->bbv2_manager.system.power_mgr.GetChargingTargetCurrentA();
        if (this->local_chg_current != chg_current) {
          this->local_chg_current = chg_current;
          this->needs_existing_list_update = true;
        }
        //charging target state
        PowerChargingTargetState chg_target = this->bbv2_manager.system.power_mgr.GetChargingTargetState();
        if (this->local_chg_target != chg_target) {
          this->local_chg_target = chg_target;
          this->needs_existing_list_update = true;
        }
        break;
      }
      default:
        break;
    }
  }

  //base handling
  this->SettingsScreenBase::DisplayScreen();
}


void SettingsScreenPower::HandleTouch(const GUITouchState& state) noexcept {
  switch (state.initial_tag) {
    case SCREEN_SETTINGS_TAG_TAB_3:
      if (state.released && state.tag == state.initial_tag) {
        //go to next page
        this->page_index = (this->page_index + 1) % 2;
        this->needs_display_list_rebuild = true;
      }
      return;
    case SCREEN_POWER_TAG_ASD_TIME:
    {
      //interpolate slider value, rounding to nearest step
      uint8_t step = (uint8_t)roundf(((float)state.tracker_value / (float)UINT16_MAX) * (float)(SCREEN_POWER_ASD_STEP_COUNT - 1));
      //clamp to valid range
      if (step >= SCREEN_POWER_ASD_STEP_COUNT) {
        step = SCREEN_POWER_ASD_STEP_COUNT - 1;
      }
      //update
      this->bbv2_manager.system.power_mgr.SetAutoShutdownDelayMS(_screen_power_asd_steps_ms[step]);
      this->needs_existing_list_update = true;
      return;
    }
    case SCREEN_POWER_TAG_INPUT_CURRENT:
      //interpolate slider value, rounding to nearest step
      this->local_in_current = roundf((float)SCREEN_POWER_INPUT_CUR_STEPS_MIN + (((float)state.tracker_value / (float)UINT16_MAX) * (float)(SCREEN_POWER_INPUT_CUR_STEPS_MAX - SCREEN_POWER_INPUT_CUR_STEPS_MIN))) * SCREEN_POWER_INPUT_CUR_STEP_SIZE;
      //clamp to valid range
      if (this->local_in_current < PWR_ADAPTER_CURRENT_A_MIN) {
        this->local_in_current = PWR_ADAPTER_CURRENT_A_MIN;
      } else if (this->local_in_current > PWR_ADAPTER_CURRENT_A_MAX) {
        this->local_in_current = PWR_ADAPTER_CURRENT_A_MAX;
      }
      this->needs_existing_list_update = true;

      if (state.released) {
        //released: check if any change
        if (this->local_in_current != this->bbv2_manager.system.power_mgr.GetAdapterMaxCurrentA()) {
          //changed: apply value globally
          this->bbv2_manager.system.power_mgr.SetAdapterMaxCurrentA(this->local_in_current, [this](bool success) {
            DEBUG_PRINTF("Max input current apply success %u\n", success);
            //update local slider value to true values
            this->local_in_current = this->bbv2_manager.system.power_mgr.GetAdapterMaxCurrentA();
            this->needs_existing_list_update = true;
          });
        }
      }
      return;
    case SCREEN_POWER_TAG_CHG_CURRENT:
    {
      //interpolate slider value, rounding to nearest step
      float step = roundf((((float)state.tracker_value / (float)UINT16_MAX) * (float)(SCREEN_POWER_CHG_CUR_STEPS_MAX - SCREEN_POWER_CHG_CUR_STEPS_MIN + 3)));
      if (step < 3.0f) {
        this->local_chg_current = 0.0f;
      } else {
        this->local_chg_current = ((float)SCREEN_POWER_CHG_CUR_STEPS_MIN + step - 3.0f) * SCREEN_POWER_CHG_CUR_STEP_SIZE;
      }
      //clamp to valid range
      if (this->local_chg_current > PWR_CHG_CURRENT_A_MAX) {
        this->local_chg_current = PWR_CHG_CURRENT_A_MAX;
      }
      this->needs_existing_list_update = true;

      if (state.released) {
        //released: check if any change
        if (this->local_chg_current != this->bbv2_manager.system.power_mgr.GetChargingTargetCurrentA()) {
          //changed: apply value globally
          this->bbv2_manager.system.power_mgr.SetChargingTargetCurrentA(this->local_chg_current, [this](bool success) {
            DEBUG_PRINTF("Charging current apply success %u\n", success);
            //update local slider value to true values
            this->local_chg_current = this->bbv2_manager.system.power_mgr.GetChargingTargetCurrentA();
            this->needs_existing_list_update = true;
          });
        }
      }
      return;
    }
    case SCREEN_POWER_TAG_CHG_TARGET:
      //interpolate slider value, rounding to nearest step
      this->local_chg_target = (PowerChargingTargetState)roundf(((float)state.tracker_value / (float)UINT16_MAX) * (float)(_PWR_CHG_TARGET_COUNT - 1));
      //clamp to valid range
      if (this->local_chg_target >= _PWR_CHG_TARGET_COUNT) {
        this->local_chg_target = (PowerChargingTargetState)(_PWR_CHG_TARGET_COUNT - 1);
      }
      this->needs_existing_list_update = true;

      if (state.released) {
        //released: check if any change
        if (this->local_chg_target != this->bbv2_manager.system.power_mgr.GetChargingTargetState()) {
          //changed: apply value globally
          this->bbv2_manager.system.power_mgr.SetChargingTargetState(this->local_chg_target, [this](bool success) {
            DEBUG_PRINTF("Charging voltage target state apply success %u\n", success);
            //update local slider value to true values
            this->local_chg_target = this->bbv2_manager.system.power_mgr.GetChargingTargetState();
            this->needs_existing_list_update = true;
          });
        }
      }
      return;
    default:
      break;
  }

  //base handling: tab switching etc
  this->SettingsScreenBase::HandleTouch(state);
}


void SettingsScreenPower::Init() {
  //allow base handling
  this->SettingsScreenBase::Init();

  //redraw first page (battery) on battery info updates
  this->bbv2_manager.system.bat_if.RegisterCallback([this](EventSource*, uint32_t) {
    if (this->page_index == 0) {
      this->needs_display_list_rebuild = true;
    }
  }, MODIF_BMS_EVENT_PRESENCE_UPDATE | MODIF_BMS_EVENT_VOLTAGE_UPDATE | MODIF_BMS_EVENT_CURRENT_UPDATE | MODIF_BMS_EVENT_SOC_UPDATE | MODIF_BMS_EVENT_HEALTH_UPDATE | MODIF_BMS_EVENT_TEMP_UPDATE);

  //redraw second page (charger) on charger presence updates
  this->bbv2_manager.system.chg_if.RegisterCallback([this](EventSource*, uint32_t) {
    if (this->page_index == 1) {
      this->needs_display_list_rebuild = true;
    }
  }, MODIF_CHG_EVENT_PRESENCE_UPDATE);

  //redraw second page (charger) on charging activity updates
  this->bbv2_manager.system.power_mgr.RegisterCallback([this](EventSource*, uint32_t) {
    if (this->page_index == 1) {
      this->needs_display_list_rebuild = true;
    }
  }, PWR_EVENT_CHARGING_ACTIVE_CHANGE);
}



void SettingsScreenPower::BuildScreenContent() {
  this->driver.CmdTag(0);

  uint32_t theme_colour_main = this->bbv2_manager.GetThemeColorMain();

  this->driver.CmdColorRGB(0xFFFFFF);
  this->driver.CmdFGColor(theme_colour_main);
  this->driver.CmdBGColor(0x808080);

  switch (this->page_index) {
    case 0:
    {
      //battery settings page
      //labels
      this->driver.CmdText(7, 76, 28, 0, "Battery");
      this->driver.CmdText(80, 214, 27, EVE_OPT_CENTERY | EVE_OPT_RIGHTX, "Auto-Off");

      //battery info
      if (this->bbv2_manager.system.bat_if.IsBatteryPresent()) {
        //battery present: draw divider lines for info first
        this->driver.CmdBeginDraw(EVE_LINES);
        this->driver.CmdDL(LINE_WIDTH(16));
        this->driver.CmdColorRGB(0x808080);
        this->driver.CmdDL(VERTEX2F(16 * 106 + 8, 16 * 107));
        this->driver.CmdDL(VERTEX2F(16 * 106 + 8, 16 * 126));
        this->driver.CmdDL(VERTEX2F(16 * 213 + 8, 16 * 107));
        this->driver.CmdDL(VERTEX2F(16 * 213 + 8, 16 * 126));
        this->driver.CmdDL(VERTEX2F(16 * 160 + 8, 16 * 132));
        this->driver.CmdDL(VERTEX2F(16 * 160 + 8, 16 * 151));
        this->driver.CmdDL(DL_END);
        this->driver.CmdColorRGB(0xFFFFFF);

        //get battery data
        float voltage_V = (float)this->bbv2_manager.system.bat_if.GetStackVoltageMV() / 1000.0f;
        float current_A = (float)this->bbv2_manager.system.bat_if.GetCurrentMA() / 1000.0f;
        float power_W = voltage_V * current_A;
        BatterySoCLevel soc_lvl = this->bbv2_manager.system.bat_if.GetSoCConfidenceLevel();
        float soc_Wh = (float)this->bbv2_manager.system.bat_if.GetSoCEnergyWh();
        uint32_t time_sec = this->bbv2_manager.system.power_mgr.GetEstimatedBatteryTimeSeconds();
        uint32_t time_hours = time_sec / 3600;
        uint32_t time_mins = ((time_sec % 3600) / 600) * 10; //rounded down to nearest 10min
        const int16_t* cell_voltages_mV = this->bbv2_manager.system.bat_if.GetCellVoltagesMV();
        float health = this->bbv2_manager.system.bat_if.GetBatteryHealth();
        int16_t temp = this->bbv2_manager.system.bat_if.GetBatteryTemperatureC();

        //format and draw battery data text
        char format_buf[64] = { 0 };
        //voltage
        snprintf(format_buf, 64, "%.3f V", voltage_V); // @suppress("Float formatting support")
        this->driver.CmdText(53, 117, 27, EVE_OPT_CENTER, format_buf);
        //current
        snprintf(format_buf, 64, "%+.3f A", current_A); // @suppress("Float formatting support")
        this->driver.CmdText(160, 117, 27, EVE_OPT_CENTER, format_buf);
        //power
        snprintf(format_buf, 64, "%+.2f W", power_W); // @suppress("Float formatting support")
        this->driver.CmdText(266, 117, 27, EVE_OPT_CENTER, format_buf);
        //state of charge
        if (soc_lvl == IF_BMS_SOCLVL_INVALID || isnanf(soc_Wh) || soc_Wh < 0.0f) {
          snprintf(format_buf, 64, "(unknown) Wh");
        } else if (soc_lvl != IF_BMS_SOCLVL_MEASURED_REF) {
          snprintf(format_buf, 64, "~%.1f Wh", soc_Wh); // @suppress("Float formatting support")
        } else {
          snprintf(format_buf, 64, "%.2f Wh", soc_Wh); // @suppress("Float formatting support")
        }
        this->driver.CmdText(80, 142, 28, EVE_OPT_CENTER, format_buf);
        //remaining time estimate
        if (this->bbv2_manager.system.chg_if.IsAdapterPresent()) {
          //adapter connected - no discharge, show charging state
          snprintf(format_buf, 64, this->bbv2_manager.system.power_mgr.IsCharging() ? "Charging" : "Idle");
        } else if (time_sec < 60 || time_hours > 99) {
          //outside of reasonable range: state as "unknown"
          snprintf(format_buf, 64, "Time unknown");
        } else {
          //show valid estimate
          snprintf(format_buf, 64, "~%lu h %lu min", time_hours, time_mins);
        }
        this->driver.CmdText(239, 142, 28, EVE_OPT_CENTER, format_buf);
        //cell voltages
        snprintf(format_buf, 64, "Cells: %.3f V | %.3f V | %.3f V | %.3f V", // @suppress("Float formatting support")
                 (float)cell_voltages_mV[0] / 1000.0f,
                 (float)cell_voltages_mV[1] / 1000.0f,
                 (float)cell_voltages_mV[2] / 1000.0f,
                 (float)cell_voltages_mV[3] / 1000.0f);
        this->driver.CmdText(160, 167, 26, EVE_OPT_CENTER, format_buf);
        //health and temp
        snprintf(format_buf, 64, "Health: %.1f%% | Temperature: %d C", health * 100.0f, temp); // @suppress("Float formatting support")
        this->driver.CmdText(160, 187, 26, EVE_OPT_CENTER, format_buf);
      } else {
        //battery not present: draw message
        this->driver.CmdText(160, 137, 28, EVE_OPT_CENTER, "Battery is off or disconnected");
        this->driver.CmdText(160, 165, 27, EVE_OPT_CENTER, "No data available");
      }

      //interpret auto-shutdown delay
      uint32_t asd_delay = this->bbv2_manager.system.power_mgr.GetAutoShutdownDelayMS();
      uint8_t asd_delay_step;
      for (asd_delay_step = SCREEN_POWER_ASD_STEP_COUNT - 1; asd_delay_step > 0; asd_delay_step--) {
        if (asd_delay >= _screen_power_asd_steps_ms[asd_delay_step]) {
          break;
        }
      }

      //auto-shutdown delay slider value
      this->asd_time_value_oidx = this->SaveNextCommandOffset();
      this->driver.CmdText(270, 214, 26, EVE_OPT_CENTERY, _screen_power_asd_step_titles[asd_delay_step]);

      //controls
      this->driver.CmdTagMask(true);
      //auto-shutdown delay slider
      this->driver.CmdTag(SCREEN_POWER_TAG_ASD_TIME);
      this->asd_time_slider_oidx = this->SaveNextCommandOffset();
      this->driver.CmdSlider(97, 208, 151, 11, 0, asd_delay_step, SCREEN_POWER_ASD_STEP_COUNT - 1);
      this->driver.CmdTrack(95, 203, 156, 21, SCREEN_POWER_TAG_ASD_TIME);
      this->driver.CmdInvisibleRect(84, 202, 177, 23);
      break;
    }
    case 1:
    {
      //charger settings page
      //labels
      this->driver.CmdText(7, 76, 28, 0, "Charger");
      this->driver.CmdText(106, 148, 26, EVE_OPT_CENTERY | EVE_OPT_RIGHTX, "Max In Current");
      this->driver.CmdText(106, 181, 26, EVE_OPT_CENTERY | EVE_OPT_RIGHTX, "Charge Current");
      this->driver.CmdText(106, 214, 26, EVE_OPT_CENTERY | EVE_OPT_RIGHTX, "Charge Target");

      //charger info
      const char* info_text = this->bbv2_manager.system.chg_if.IsAdapterPresent() ?
          (this->bbv2_manager.system.power_mgr.IsCharging() ? "Connected - Charging" : "Connected - Not Charging") :
          "Not Connected";
      this->driver.CmdText(160, 117, 27, EVE_OPT_CENTER, info_text);

      //slider value displays
      char val_buffer[8];
      //input current
      snprintf(val_buffer, 8, "%.2f A", this->local_in_current); // @suppress("Float formatting support")
      this->in_current_value_oidx = this->SaveNextCommandOffset();
      this->driver.CmdText(265, 148, 26, EVE_OPT_CENTERY, val_buffer);
      //charge current
      if (this->local_chg_current < PWR_CHG_CURRENT_A_MIN) {
        snprintf(val_buffer, 8, "Off ");
      } else {
        snprintf(val_buffer, 8, "%.2f A", this->local_chg_current); // @suppress("Float formatting support")
      }
      this->chg_current_value_oidx = this->SaveNextCommandOffset();
      this->driver.CmdText(265, 181, 26, EVE_OPT_CENTERY, val_buffer);
      //charge target
      strncpy(val_buffer, _screen_power_chg_target_titles[this->local_chg_target], 7);
      val_buffer[7] = 0;
      this->chg_target_value_oidx = this->SaveNextCommandOffset();
      this->driver.CmdText(265, 214, 26, EVE_OPT_CENTERY, val_buffer);

      //controls
      this->driver.CmdTagMask(true);
      //input current slider
      this->driver.CmdTag(SCREEN_POWER_TAG_INPUT_CURRENT);
      this->in_current_slider_oidx = this->SaveNextCommandOffset();
      this->driver.CmdSlider(122, 142, 126, 11, 0, (uint16_t)roundf(this->local_in_current / SCREEN_POWER_INPUT_CUR_STEP_SIZE) - SCREEN_POWER_INPUT_CUR_STEPS_MIN, SCREEN_POWER_INPUT_CUR_STEPS_MAX - SCREEN_POWER_INPUT_CUR_STEPS_MIN);
      this->driver.CmdTrack(120, 137, 131, 21, SCREEN_POWER_TAG_INPUT_CURRENT);
      this->driver.CmdInvisibleRect(109, 136, 152, 23);
      //charge current slider
      this->driver.CmdTag(SCREEN_POWER_TAG_CHG_CURRENT);
      this->chg_current_slider_oidx = this->SaveNextCommandOffset();
      this->driver.CmdSlider(122, 175, 126, 11, 0, this->local_chg_current >= PWR_CHG_CURRENT_A_MIN ? 3 + (uint16_t)roundf(this->local_chg_current / SCREEN_POWER_CHG_CUR_STEP_SIZE) - SCREEN_POWER_CHG_CUR_STEPS_MIN : 0,
                             SCREEN_POWER_CHG_CUR_STEPS_MAX - SCREEN_POWER_CHG_CUR_STEPS_MIN + 3);
      this->driver.CmdTrack(120, 170, 131, 21, SCREEN_POWER_TAG_CHG_CURRENT);
      this->driver.CmdInvisibleRect(109, 169, 152, 23);
      //charge target slider
      this->driver.CmdTag(SCREEN_POWER_TAG_CHG_TARGET);
      this->chg_target_slider_oidx = this->SaveNextCommandOffset();
      this->driver.CmdSlider(122, 208, 126, 11, 0, (uint16_t)this->local_chg_target, (uint16_t)(_PWR_CHG_TARGET_COUNT - 1));
      this->driver.CmdTrack(120, 203, 131, 21, SCREEN_POWER_TAG_CHG_TARGET);
      this->driver.CmdInvisibleRect(109, 202, 152, 23);
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
      this->DrawActivePageDot(156);
      break;
    case 1:
      this->DrawActivePageDot(164);
      break;
    default:
      break;
  }

  //popup overlay, if any
  this->DrawPopupOverlay();
}

void SettingsScreenPower::UpdateExistingScreenContent() {
  switch (this->page_index) {
    case 0:
    {
      //battery settings page
      //interpret auto-shutdown delay
      uint32_t asd_delay = this->bbv2_manager.system.power_mgr.GetAutoShutdownDelayMS();
      uint8_t asd_delay_step;
      for (asd_delay_step = SCREEN_POWER_ASD_STEP_COUNT - 1; asd_delay_step > 0; asd_delay_step--) {
        if (asd_delay >= _screen_power_asd_steps_ms[asd_delay_step]) {
          break;
        }
      }

      //update auto-shutdown delay slider value display
      snprintf((char*)this->GetDLCommandPointer(this->asd_time_value_oidx, 3), 8, _screen_power_asd_step_titles[asd_delay_step]);

      //update auto-shutdown delay slider
      this->ModifyDLCommand16(this->asd_time_slider_oidx, 3, 1, asd_delay_step);
      break;
    }
    case 1:
    {
      //charger settings page
      //update slider value displays
      //input current
      snprintf((char*)this->GetDLCommandPointer(this->in_current_value_oidx, 3), 8, "%.2f A", this->local_in_current); // @suppress("Float formatting support")
      //charge current
      if (this->local_chg_current < PWR_CHG_CURRENT_A_MIN) {
        snprintf((char*)this->GetDLCommandPointer(this->chg_current_value_oidx, 3), 8, "Off ");
      } else {
        snprintf((char*)this->GetDLCommandPointer(this->chg_current_value_oidx, 3), 8, "%.2f A", this->local_chg_current); // @suppress("Float formatting support")
      }
      //charge target
      strncpy((char*)this->GetDLCommandPointer(this->chg_target_value_oidx, 3), _screen_power_chg_target_titles[this->local_chg_target], 7);

      //update sliders
      //input current
      this->ModifyDLCommand16(this->in_current_slider_oidx, 3, 1, (uint16_t)roundf(this->local_in_current / SCREEN_POWER_INPUT_CUR_STEP_SIZE) - SCREEN_POWER_INPUT_CUR_STEPS_MIN);
      //charge current
      this->ModifyDLCommand16(this->chg_current_slider_oidx, 3, 1, this->local_chg_current >= PWR_CHG_CURRENT_A_MIN ? 3 + (uint16_t)roundf(this->local_chg_current / SCREEN_POWER_CHG_CUR_STEP_SIZE) - SCREEN_POWER_CHG_CUR_STEPS_MIN : 0);
      //charge target
      this->ModifyDLCommand16(this->chg_target_slider_oidx, 3, 1, (uint16_t)this->local_chg_target);
      break;
    }
    default:
      break;
  }
}


void SettingsScreenPower::OnScreenExit() {
  //reset to first page
  this->page_index = 0;

  //save settings
  this->bbv2_manager.system.eeprom_if.WriteAllDirtySections([](bool success) {
    DEBUG_PRINTF("Power screen exit EEPROM write success %u\n", success);
  });
}

