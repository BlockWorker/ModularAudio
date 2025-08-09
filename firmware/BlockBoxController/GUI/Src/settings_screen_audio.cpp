/*
 * settings_screen_audio.cpp
 *
 *  Created on: Aug 8, 2025
 *      Author: Alex
 */


#include "settings_screen_audio.h"
#include "bbv2_gui_manager.h"
#include "system.h"


//settings tab index
#define SCREEN_AUDIO_TAB_INDEX 0

//touch tags
//allow positive gain
#define SCREEN_AUDIO_TAG_ALLOW_POS_GAIN 40
//mono mix mode
#define SCREEN_AUDIO_TAG_MIXER_MODE 41
//min volume
#define SCREEN_AUDIO_TAG_MIN_VOLUME 42
//max volume
#define SCREEN_AUDIO_TAG_MAX_VOLUME 43
//volume step
#define SCREEN_AUDIO_TAG_VOLUME_STEP 44
//EQ mode
#define SCREEN_AUDIO_TAG_EQ_MODE 45
//loudness track max volume
#define SCREEN_AUDIO_TAG_LOUDNESS_TRACKMAX 46
//loudness gain
#define SCREEN_AUDIO_TAG_LOUDNESS_GAIN 47
//bass gain
#define SCREEN_AUDIO_TAG_BASS_GAIN 48
//treble gain
#define SCREEN_AUDIO_TAG_TREBLE_GAIN 49
//amplifier power target
#define SCREEN_AUDIO_TAG_AMP_POWER_TARGET 50


SettingsScreenAudio::SettingsScreenAudio(BlockBoxV2GUIManager& manager) :
    SettingsScreenBase(manager, SCREEN_AUDIO_TAB_INDEX), page_index(0), local_power_target(1.0f) {}


void SettingsScreenAudio::DisplayScreen() {
  //if not actively touching screen, update local slider values to true values
  if (!this->manager.GetTouchState().touched) {
    switch (this->page_index) {
      case 0:
      {
        //min volume
        float min_vol = this->bbv2_manager.system.audio_mgr.GetMinVolumeDB();
        if (this->local_min_volume != min_vol) {
          this->local_min_volume = min_vol;
          this->needs_existing_list_update = true;
        }
        //max volume
        float max_vol = this->bbv2_manager.system.audio_mgr.GetMaxVolumeDB();
        if (this->local_max_volume != max_vol) {
          this->local_max_volume = max_vol;
          this->needs_existing_list_update = true;
        }
        //volume step
        float vol_step = this->bbv2_manager.system.audio_mgr.GetVolumeStepDB();
        if (this->local_volume_step != vol_step) {
          this->local_volume_step = vol_step;
          this->needs_existing_list_update = true;
        }
        break;
      }
      case 1:
      {
        //loudness gain
        float loudness_gain = this->bbv2_manager.system.audio_mgr.GetLoudnessGainDB();
        if (this->local_loudness != loudness_gain) {
          this->local_loudness = loudness_gain;
          this->needs_existing_list_update = true;
        }
        break;
      }
      case 2:
      {
        //TODO power target once implemented
        break;
      }
      default:
        break;
    }
  }

  //base handling
  this->SettingsScreenBase::DisplayScreen();
}


void SettingsScreenAudio::HandleTouch(const GUITouchState& state) noexcept {
  switch (state.initial_tag) {
    case SCREEN_SETTINGS_TAG_TAB_0:
      if (state.released && state.tag == state.initial_tag) {
        //go to next page
        this->page_index = (this->page_index + 1) % 3;
        //enable page-specific monitoring
        switch (this->page_index) {
          case 1:
            this->bbv2_manager.system.dap_if.monitor_src_stats = true;
            this->bbv2_manager.system.amp_if.monitor_measurements = false;
            break;
          case 2:
            this->bbv2_manager.system.dap_if.monitor_src_stats = false;
            this->bbv2_manager.system.amp_if.monitor_measurements = true;
            break;
          default:
            this->bbv2_manager.system.dap_if.monitor_src_stats = false;
            this->bbv2_manager.system.amp_if.monitor_measurements = false;
            break;
        }
        this->needs_display_list_rebuild = true;
      }
      return;
    case SCREEN_AUDIO_TAG_ALLOW_POS_GAIN:
      if (state.released && state.tag == state.initial_tag) {
        //toggle positive gain allowed
        this->bbv2_manager.system.audio_mgr.SetPositiveGainAllowed(!this->bbv2_manager.system.audio_mgr.IsPositiveGainAllowed(), [&](bool success) {
          DEBUG_PRINTF("Pos gain allowed toggle success %u\n", success);
          this->needs_display_list_rebuild = true;
        });
      }
      return;
    case SCREEN_AUDIO_TAG_MIXER_MODE:
      if (state.released && state.tag == state.initial_tag) {
        //toggle mixer mode
        AudioPathMixerMode mode = (this->bbv2_manager.system.audio_mgr.GetMixerMode() == AUDIO_MIXER_AVG) ? AUDIO_MIXER_LEFT : AUDIO_MIXER_AVG;
        this->bbv2_manager.system.audio_mgr.SetMixerMode(mode, [&](bool success) {
          DEBUG_PRINTF("Mixer mode toggle success %u\n", success);
          this->needs_display_list_rebuild = true;
        });
      }
      return;
    case SCREEN_AUDIO_TAG_MIN_VOLUME:
    {
      //calculate top end of slider
      float min_vol_max = MIN(this->local_max_volume - AUDIO_LIMIT_VOLUME_RANGE_MIN, AUDIO_LIMIT_MIN_VOLUME_MAX);

      //interpolate slider value, rounding to nearest step
      this->local_min_volume = roundf((AUDIO_LIMIT_MIN_VOLUME_MIN + ((float)state.tracker_value / (float)UINT16_MAX) * (min_vol_max - AUDIO_LIMIT_MIN_VOLUME_MIN)) / this->local_volume_step) * this->local_volume_step;
      //clamp to valid range
      if (this->local_min_volume < AUDIO_LIMIT_MIN_VOLUME_MIN) {
        this->local_min_volume = AUDIO_LIMIT_MIN_VOLUME_MIN;
      } else if (this->local_min_volume > min_vol_max) {
        this->local_min_volume = min_vol_max;
      }
      this->needs_existing_list_update = true;

      if (state.released) {
        //released: check if any change
        if (this->local_min_volume != this->bbv2_manager.system.audio_mgr.GetMinVolumeDB()) {
          //changed: apply value globally
          this->bbv2_manager.system.audio_mgr.SetMinVolumeDB(this->local_min_volume, [&](bool success) {
            DEBUG_PRINTF("Min volume apply success %u\n", success);
            //update local slider values to true values
            this->local_min_volume = this->bbv2_manager.system.audio_mgr.GetMinVolumeDB();
            this->local_max_volume = this->bbv2_manager.system.audio_mgr.GetMaxVolumeDB();
            this->local_volume_step = this->bbv2_manager.system.audio_mgr.GetVolumeStepDB();
            this->needs_existing_list_update = true;
          });
        }
      }
      return;
    }
    case SCREEN_AUDIO_TAG_MAX_VOLUME:
    {
      //calculate top end of slider
      float max_vol_max = this->bbv2_manager.system.audio_mgr.IsPositiveGainAllowed() ? AUDIO_LIMIT_MAX_VOLUME_MAX : 0.0f;

      //interpolate slider value, rounding to nearest step
      this->local_max_volume = roundf((AUDIO_LIMIT_MAX_VOLUME_MIN + ((float)state.tracker_value / (float)UINT16_MAX) * (max_vol_max - AUDIO_LIMIT_MAX_VOLUME_MIN)) / this->local_volume_step) * this->local_volume_step;
      //clamp to valid range
      if (this->local_max_volume < AUDIO_LIMIT_MAX_VOLUME_MIN) {
        this->local_max_volume = AUDIO_LIMIT_MAX_VOLUME_MIN;
      } else if (this->local_max_volume > max_vol_max) {
        this->local_max_volume = max_vol_max;
      }
      this->needs_existing_list_update = true;

      if (state.released) {
        //released: check if any change
        if (this->local_max_volume != this->bbv2_manager.system.audio_mgr.GetMaxVolumeDB()) {
          //changed: apply value globally
          this->bbv2_manager.system.audio_mgr.SetMaxVolumeDB(this->local_max_volume, [&](bool success) {
            DEBUG_PRINTF("Max volume apply success %u\n", success);
            //update local slider values to true values
            this->local_min_volume = this->bbv2_manager.system.audio_mgr.GetMinVolumeDB();
            this->local_max_volume = this->bbv2_manager.system.audio_mgr.GetMaxVolumeDB();
            this->local_volume_step = this->bbv2_manager.system.audio_mgr.GetVolumeStepDB();
            this->needs_existing_list_update = true;
          });
        }
      }
      return;
    }
    case SCREEN_AUDIO_TAG_VOLUME_STEP:
      //interpolate slider value
      this->local_volume_step = roundf(AUDIO_LIMIT_VOLUME_STEP_MIN + ((float)state.tracker_value / (float)UINT16_MAX) * (AUDIO_LIMIT_VOLUME_STEP_MAX - AUDIO_LIMIT_VOLUME_STEP_MIN));
      this->needs_existing_list_update = true;

      if (state.released) {
        //released: check if any change
        if (this->local_volume_step != this->bbv2_manager.system.audio_mgr.GetVolumeStepDB()) {
          //changed: apply value globally
          this->bbv2_manager.system.audio_mgr.SetVolumeStepDB(this->local_volume_step, [&](bool success) {
            DEBUG_PRINTF("Volume step apply success %u\n", success);
            //update local slider values to true values
            this->local_min_volume = this->bbv2_manager.system.audio_mgr.GetMinVolumeDB();
            this->local_max_volume = this->bbv2_manager.system.audio_mgr.GetMaxVolumeDB();
            this->local_volume_step = this->bbv2_manager.system.audio_mgr.GetVolumeStepDB();
            this->needs_existing_list_update = true;
          });
        }
      }
      return;
    case SCREEN_AUDIO_TAG_EQ_MODE:
      if (state.released && state.tag == state.initial_tag) {
        //toggle EQ mode
        AudioPathEQMode mode = (this->bbv2_manager.system.audio_mgr.GetEQMode() == AUDIO_EQ_HIFI) ? AUDIO_EQ_POWER : AUDIO_EQ_HIFI;
        this->bbv2_manager.system.audio_mgr.SetEQMode(mode, [&](bool success) {
          DEBUG_PRINTF("EQ mode toggle success %u\n", success);
          this->needs_display_list_rebuild = true;
        });
      }
      return;
    case SCREEN_AUDIO_TAG_LOUDNESS_TRACKMAX:
      if (state.released && state.tag == state.initial_tag) {
        //toggle loudness track max volume
        this->bbv2_manager.system.audio_mgr.SetLoudnessTrackingMaxVolume(!this->bbv2_manager.system.audio_mgr.IsLoudnessTrackingMaxVolume(), [&](bool success) {
          DEBUG_PRINTF("Loudness track max vol toggle success %u\n", success);
          this->needs_display_list_rebuild = true;
        });
      }
      return;
    case SCREEN_AUDIO_TAG_LOUDNESS_GAIN:
      {
        //interpolate slider value
        float step_index = roundf(((float)state.tracker_value / (float)UINT16_MAX) * (3.0f + AUDIO_LIMIT_LOUDNESS_GAIN_MAX - AUDIO_LIMIT_LOUDNESS_GAIN_MIN_ACTIVE));
        if (step_index < 3.0f) {
          //disable loudness
          this->local_loudness = -INFINITY;
        } else {
          //enable loudness with corresponding gain
          this->local_loudness = AUDIO_LIMIT_LOUDNESS_GAIN_MIN_ACTIVE + step_index - 3.0f;
        }
        this->needs_existing_list_update = true;

        if (state.released) {
          //released: check if any change
          if (this->local_loudness != this->bbv2_manager.system.audio_mgr.GetLoudnessGainDB()) {
            //changed: apply value globally
            this->bbv2_manager.system.audio_mgr.SetLoudnessGainDB(this->local_loudness, [&](bool success) {
              DEBUG_PRINTF("Loudness gain apply success %u\n", success);
              //update local slider values to true values
              this->local_loudness = this->bbv2_manager.system.audio_mgr.GetLoudnessGainDB();
              this->needs_existing_list_update = true;
            });
          }
        }
        return;
      }
    case SCREEN_AUDIO_TAG_AMP_POWER_TARGET:
      //interpolate slider value
      this->local_power_target = 0.5f + roundf((float)state.tracker_value / (float)UINT16_MAX * 5.0f) / 10.0f;
      this->needs_existing_list_update = true;

      //TODO add release handling once implemented
      /*if (state.released) {
        //released: check if any change
        if (this->local_power_target != ...) {
          //changed: apply value globally
          ...(this->local_power_target, [&](bool success) {
            DEBUG_PRINTF("Power target apply success %u\n", success);
            //update local slider values to true values
            this->local_power_target = ...;
            this->needs_existing_list_update = true;
          });
        }
      }*/
      return;
    default:
      break;
  }

  //base handling: tab switching etc
  this->SettingsScreenBase::HandleTouch(state);
}


void SettingsScreenAudio::Init() {
  //allow base handling
  this->SettingsScreenBase::Init();

  //redraw second page (sound) on SRC stats update
  this->bbv2_manager.system.dap_if.RegisterCallback([&](EventSource*, uint32_t) {
    if (this->page_index == 1) {
      this->needs_display_list_rebuild = true;
    }
  }, MODIF_DAP_EVENT_SRC_STATS_UPDATE);

  //redraw third page (amp) on PVDD or amp measurement update
  this->bbv2_manager.system.amp_if.RegisterCallback([&](EventSource*, uint32_t) {
    if (this->page_index == 2) {
      this->needs_display_list_rebuild = true;
    }
  }, MODIF_POWERAMP_EVENT_PVDD_UPDATE | MODIF_POWERAMP_EVENT_MEASUREMENT_UPDATE);

  //TODO register update handlers
}



void SettingsScreenAudio::BuildScreenContent() {
  this->driver.CmdTag(0);

  uint32_t theme_colour_main = this->bbv2_manager.GetThemeColorMain();

  this->driver.CmdColorRGB(0xFFFFFF);
  this->driver.CmdFGColor(theme_colour_main);
  this->driver.CmdBGColor(0x808080);

  switch (this->page_index) {
    case 0:
    {
      //volume settings page
      //labels
      this->driver.CmdText(7, 76, 28, 0, "Volume & Mix");
      this->driver.CmdText(87, 121, 26, EVE_OPT_CENTERY | EVE_OPT_RIGHTX, "Allow >0dB");
      this->driver.CmdText(239, 121, 26, EVE_OPT_CENTERY | EVE_OPT_RIGHTX, "Mono Mode");
      this->driver.CmdText(41, 153, 27, EVE_OPT_CENTERY | EVE_OPT_RIGHTX, "Min");
      this->driver.CmdText(41, 186, 27, EVE_OPT_CENTERY | EVE_OPT_RIGHTX, "Max");
      this->driver.CmdText(41, 219, 27, EVE_OPT_CENTERY | EVE_OPT_RIGHTX, "Step");

      bool pos_gain_allowed = this->bbv2_manager.system.audio_mgr.IsPositiveGainAllowed();

      //calculate (and enforce) min/max volume upper limits
      float min_vol_max = MIN(this->local_max_volume - AUDIO_LIMIT_VOLUME_RANGE_MIN, AUDIO_LIMIT_MIN_VOLUME_MAX);
      if (this->local_min_volume > min_vol_max) {
        this->local_min_volume = min_vol_max;
      }
      float max_vol_max = pos_gain_allowed ? AUDIO_LIMIT_MAX_VOLUME_MAX : 0.0f;
      if (this->local_max_volume > max_vol_max) {
        this->local_max_volume = max_vol_max;
      }

#pragma GCC diagnostic ignored "-Wformat-truncation="
      //slider value displays
      char val_buffer[6];
      //min
      snprintf(val_buffer, 6, "%+ddB", (int8_t)this->local_min_volume);
      this->min_value_oidx = this->SaveNextCommandOffset();
      this->driver.CmdText(272, 153, 26, EVE_OPT_CENTERY, val_buffer);
      //max
      snprintf(val_buffer, 6, "%+ddB", (int8_t)this->local_max_volume);
      this->max_value_oidx = this->SaveNextCommandOffset();
      this->driver.CmdText(272, 186, 26, EVE_OPT_CENTERY, val_buffer);
      //step
      snprintf(val_buffer, 4, "%udB", (uint8_t)this->local_volume_step);
      this->step_value_oidx = this->SaveNextCommandOffset();
      this->driver.CmdText(272, 219, 26, EVE_OPT_CENTERY, val_buffer);
#pragma GCC diagnostic pop

      //controls
      this->driver.CmdTagMask(true);
      //allow positive gain toggle
      this->driver.CmdTag(SCREEN_AUDIO_TAG_ALLOW_POS_GAIN);
      this->driver.CmdToggle(104, 115, 31, 26, 0, pos_gain_allowed ? 0xFFFF : 0, " No\xFFYes");
      //mono mixer mode toggle
      this->driver.CmdTag(SCREEN_AUDIO_TAG_MIXER_MODE);
      this->driver.CmdToggle(256, 115, 35, 26, 0, (this->bbv2_manager.system.audio_mgr.GetMixerMode() == AUDIO_MIXER_LEFT) ? 0xFFFF : 0, " Mix\xFFLeft");
      //min volume slider
      this->driver.CmdTag(SCREEN_AUDIO_TAG_MIN_VOLUME);
      this->min_slider_oidx = this->SaveNextCommandOffset();
      this->driver.CmdSlider(57, 147, 200, 11, 0, (uint16_t)roundf(this->local_min_volume - AUDIO_LIMIT_MIN_VOLUME_MIN), (uint16_t)roundf(min_vol_max - AUDIO_LIMIT_MIN_VOLUME_MIN));
      this->driver.CmdTrack(55, 142, 205, 21, SCREEN_AUDIO_TAG_MIN_VOLUME);
      this->driver.CmdInvisibleRect(44, 141, 226, 23);
      //max volume slider
      this->driver.CmdTag(SCREEN_AUDIO_TAG_MAX_VOLUME);
      this->max_slider_oidx = this->SaveNextCommandOffset();
      this->driver.CmdSlider(57, 180, 200, 11, 0, (uint16_t)roundf(this->local_max_volume - AUDIO_LIMIT_MAX_VOLUME_MIN), (uint16_t)roundf(max_vol_max - AUDIO_LIMIT_MAX_VOLUME_MIN));
      this->driver.CmdTrack(55, 175, 205, 21, SCREEN_AUDIO_TAG_MAX_VOLUME);
      this->driver.CmdInvisibleRect(44, 174, 226, 23);
      //volume step slider
      this->driver.CmdTag(SCREEN_AUDIO_TAG_VOLUME_STEP);
      this->step_slider_oidx = this->SaveNextCommandOffset();
      this->driver.CmdSlider(57, 213, 200, 11, 0, (uint16_t)roundf(this->local_volume_step - AUDIO_LIMIT_VOLUME_STEP_MIN), (uint16_t)roundf(AUDIO_LIMIT_VOLUME_STEP_MAX - AUDIO_LIMIT_VOLUME_STEP_MIN));
      this->driver.CmdTrack(55, 208, 205, 21, SCREEN_AUDIO_TAG_VOLUME_STEP);
      this->driver.CmdInvisibleRect(44, 207, 226, 23);
      break;
    }
    case 1:
    {
      //sound settings page
      //labels
      this->driver.CmdText(7, 76, 28, 0, "Sound");
      this->driver.CmdText(62, 121, 26, EVE_OPT_CENTERY | EVE_OPT_RIGHTX, "EQ Mode");
      this->driver.CmdText(232, 121, 26, EVE_OPT_CENTERY | EVE_OPT_RIGHTX, "Loudness Ref.");
      this->driver.CmdText(81, 153, 27, EVE_OPT_CENTERY | EVE_OPT_RIGHTX, "Loudness");
      this->driver.CmdText(81, 186, 27, EVE_OPT_CENTERY | EVE_OPT_RIGHTX, "Bass");
      this->driver.CmdText(81, 219, 27, EVE_OPT_CENTERY | EVE_OPT_RIGHTX, "Treble");

      //ASRC stats display
      char stats_buffer[64];
      snprintf(stats_buffer, 64, "ASRC: Rate %+.3f%%, Buffer %+.1f", this->bbv2_manager.system.dap_if.GetSRCInputRateErrorRelative() * 100.0f, this->bbv2_manager.system.dap_if.GetSRCBufferFillErrorSamples()); // @suppress("Float formatting support")
      this->driver.CmdText(310, 88, 20, EVE_OPT_CENTERY | EVE_OPT_RIGHTX, stats_buffer);

#pragma GCC diagnostic ignored "-Wformat-truncation="
      //slider value displays
      char val_buffer[6];
      //loudness
      bool loudness_on = (this->local_loudness >= AUDIO_LIMIT_LOUDNESS_GAIN_MIN_ACTIVE);
      if (loudness_on) {
        snprintf(val_buffer, 6, "%+ddB", (int8_t)this->local_loudness);
      } else {
        snprintf(val_buffer, 6, "Off ");
      }
      this->loudness_value_oidx = this->SaveNextCommandOffset();
      this->driver.CmdText(272, 153, 26, EVE_OPT_CENTERY, val_buffer);
      //bass (disabled)
      this->driver.CmdText(272, 186, 26, EVE_OPT_CENTERY, "+0dB");
      //treble (disabled)
      this->driver.CmdText(272, 219, 26, EVE_OPT_CENTERY, "+0dB");
#pragma GCC diagnostic pop

      //controls
      this->driver.CmdTagMask(true);
      //EQ mode toggle
      this->driver.CmdTag(SCREEN_AUDIO_TAG_EQ_MODE);
      this->driver.CmdToggle(78, 115, 38, 26, 0, (this->bbv2_manager.system.audio_mgr.GetEQMode() == AUDIO_EQ_POWER) ? 0xFFFF : 0, " HiFi\xFFLoud");
      //loudness track max volume toggle
      this->driver.CmdTag(SCREEN_AUDIO_TAG_LOUDNESS_TRACKMAX);
      this->driver.CmdToggle(248, 115, 53, 26, 0, this->bbv2_manager.system.audio_mgr.IsLoudnessTrackingMaxVolume() ? 0xFFFF : 0, "   0dB\xFFMaxVol");
      //loudness gain slider
      this->driver.CmdTag(SCREEN_AUDIO_TAG_LOUDNESS_GAIN);
      this->loudness_slider_oidx = this->SaveNextCommandOffset();
      this->driver.CmdSlider(97, 147, 160, 11, 0, loudness_on ? 3 + (uint16_t)roundf(this->local_loudness - AUDIO_LIMIT_LOUDNESS_GAIN_MIN_ACTIVE) : 0, 3 + (uint16_t)roundf(AUDIO_LIMIT_LOUDNESS_GAIN_MAX - AUDIO_LIMIT_LOUDNESS_GAIN_MIN_ACTIVE));
      this->driver.CmdTrack(95, 142, 165, 21, SCREEN_AUDIO_TAG_LOUDNESS_GAIN);
      this->driver.CmdInvisibleRect(84, 141, 186, 23);
      //bass gain slider (disabled)
      this->driver.CmdFGColor(0x606060);
      this->driver.CmdTag(SCREEN_AUDIO_TAG_BASS_GAIN);
      //... = this->SaveNextCommandOffset();
      this->driver.CmdSlider(97, 180, 160, 11, 0, 6, 12);
      this->driver.CmdTrack(95, 175, 165, 21, SCREEN_AUDIO_TAG_BASS_GAIN);
      this->driver.CmdInvisibleRect(84, 174, 186, 23);
      //treble gain slider (disabled)
      this->driver.CmdTag(SCREEN_AUDIO_TAG_TREBLE_GAIN);
      //... = this->SaveNextCommandOffset();
      this->driver.CmdSlider(97, 213, 160, 11, 0, 6, 12);
      this->driver.CmdTrack(95, 208, 165, 21, SCREEN_AUDIO_TAG_TREBLE_GAIN);
      this->driver.CmdInvisibleRect(84, 207, 186, 23);
      break;
    }
    case 2:
    {
      //amp settings page
      //labels
      this->driver.CmdText(7, 76, 28, 0, "Amplifier");
      this->driver.CmdText(125, 125, 27, EVE_OPT_CENTER, "V(rms)");
      this->driver.CmdText(195, 125, 27, EVE_OPT_CENTER, "A(rms)");
      this->driver.CmdText(265, 125, 27, EVE_OPT_CENTER, "W(avg)");
      this->driver.CmdText(85, 150, 27, EVE_OPT_CENTERY | EVE_OPT_RIGHTX, "Woofer:");
      this->driver.CmdText(85, 175, 27, EVE_OPT_CENTERY | EVE_OPT_RIGHTX, "Tweeter:");
      this->driver.CmdText(110, 214, 27, EVE_OPT_CENTERY | EVE_OPT_RIGHTX, "Power Target");

      //amp info and stats display
      char stats_buffer[64];
      //PVDD
      snprintf(stats_buffer, 64, "PVDD: %.2f V (Target: %.2f V)", this->bbv2_manager.system.amp_if.GetPVDDMeasuredVoltage(), this->bbv2_manager.system.amp_if.GetPVDDTargetVoltage()); // @suppress("Float formatting support")
      this->driver.CmdText(300, 88, 20, EVE_OPT_CENTERY | EVE_OPT_RIGHTX, stats_buffer);
      //measurements
      PowerAmpMeasurements amp_m = this->bbv2_manager.system.amp_if.GetOutputMeasurements(false);
      snprintf(stats_buffer, 64, "%.3f", amp_m.voltage_rms.ch_a); // @suppress("Float formatting support")
      this->driver.CmdText(125, 150, 27, EVE_OPT_CENTER, stats_buffer);
      snprintf(stats_buffer, 64, "%.3f", amp_m.current_rms.ch_a); // @suppress("Float formatting support")
      this->driver.CmdText(195, 150, 27, EVE_OPT_CENTER, stats_buffer);
      snprintf(stats_buffer, 64, "%.3f", amp_m.power_average.ch_a); // @suppress("Float formatting support")
      this->driver.CmdText(265, 150, 27, EVE_OPT_CENTER, stats_buffer);
      snprintf(stats_buffer, 64, "%.3f", amp_m.voltage_rms.ch_c); // @suppress("Float formatting support")
      this->driver.CmdText(125, 175, 27, EVE_OPT_CENTER, stats_buffer);
      snprintf(stats_buffer, 64, "%.3f", amp_m.current_rms.ch_c); // @suppress("Float formatting support")
      this->driver.CmdText(195, 175, 27, EVE_OPT_CENTER, stats_buffer);
      snprintf(stats_buffer, 64, "%.3f", amp_m.power_average.ch_c); // @suppress("Float formatting support")
      this->driver.CmdText(265, 175, 27, EVE_OPT_CENTER, stats_buffer);

      //power target slider value display
      char val_buffer[5];
      snprintf(val_buffer, 5, "%3u%%", (uint8_t)(this->local_power_target * 100.0f));
      this->power_target_value_oidx = this->SaveNextCommandOffset();
      this->driver.CmdText(278, 214, 26, EVE_OPT_CENTERY, val_buffer);

      //controls
      this->driver.CmdTagMask(true);
      //power target slider
      this->driver.CmdTag(SCREEN_AUDIO_TAG_AMP_POWER_TARGET);
      this->power_target_slider_oidx = this->SaveNextCommandOffset();
      this->driver.CmdSlider(126, 208, 137, 11, 0, (uint16_t)roundf((this->local_power_target - 0.5f) * 10.0f), 5);
      this->driver.CmdTrack(124, 203, 142, 21, SCREEN_AUDIO_TAG_AMP_POWER_TARGET);
      this->driver.CmdInvisibleRect(113, 202, 163, 23);
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
      this->DrawActivePageDot(17);
      break;
    case 1:
      this->DrawActivePageDot(25);
      break;
    case 2:
      this->DrawActivePageDot(33);
      break;
    default:
      break;
  }

  //popup overlay, if any
  this->DrawPopupOverlay();
}

void SettingsScreenAudio::UpdateExistingScreenContent() {
  switch (this->page_index) {
    case 0:
    {
      //volume settings page
      //calculate (and enforce) min/max volume upper limits
      float min_vol_max = MIN(this->local_max_volume - AUDIO_LIMIT_VOLUME_RANGE_MIN, AUDIO_LIMIT_MIN_VOLUME_MAX);
      if (this->local_min_volume > min_vol_max) {
        this->local_min_volume = min_vol_max;
      }
      float max_vol_max = this->bbv2_manager.system.audio_mgr.IsPositiveGainAllowed() ? AUDIO_LIMIT_MAX_VOLUME_MAX : 0.0f;
      if (this->local_max_volume > max_vol_max) {
        this->local_max_volume = max_vol_max;
      }

#pragma GCC diagnostic ignored "-Wformat-truncation="
      //update slider value displays
      //min volume
      snprintf((char*)this->GetDLCommandPointer(this->min_value_oidx, 3), 6, "%+ddB", (int8_t)this->local_min_volume);
      //max volume
      snprintf((char*)this->GetDLCommandPointer(this->max_value_oidx, 3), 6, "%+ddB", (int8_t)this->local_max_volume);
      //volume step
      snprintf((char*)this->GetDLCommandPointer(this->step_value_oidx, 3), 4, "%udB", (uint8_t)this->local_volume_step);
#pragma GCC diagnostic pop

      //update sliders
      //min volume (including range)
      this->ModifyDLCommand16(this->min_slider_oidx, 3, 1, (uint16_t)roundf(this->local_min_volume - AUDIO_LIMIT_MIN_VOLUME_MIN));
      this->ModifyDLCommand16(this->min_slider_oidx, 4, 0, (uint16_t)roundf(min_vol_max - AUDIO_LIMIT_MIN_VOLUME_MIN));
      //max volume (including range)
      this->ModifyDLCommand16(this->max_slider_oidx, 3, 1, (uint16_t)roundf(this->local_max_volume - AUDIO_LIMIT_MAX_VOLUME_MIN));
      this->ModifyDLCommand16(this->max_slider_oidx, 4, 0, (uint16_t)roundf(max_vol_max - AUDIO_LIMIT_MAX_VOLUME_MIN));
      //volume step
      this->ModifyDLCommand16(this->step_slider_oidx, 3, 1, (uint16_t)roundf(this->local_volume_step - AUDIO_LIMIT_VOLUME_STEP_MIN));
      break;
    }
    case 1:
    {
      //sound settings page
#pragma GCC diagnostic ignored "-Wformat-truncation="
      //update slider value displays
      //loudness
      bool loudness_on = (this->local_loudness >= AUDIO_LIMIT_LOUDNESS_GAIN_MIN_ACTIVE);
      if (loudness_on) {
        snprintf((char*)this->GetDLCommandPointer(this->loudness_value_oidx, 3), 6, "%+ddB", (int8_t)this->local_loudness);
      } else {
        snprintf((char*)this->GetDLCommandPointer(this->loudness_value_oidx, 3), 6, "Off ");
      }
#pragma GCC diagnostic pop

      //update sliders
      //loudness
      this->ModifyDLCommand16(this->loudness_slider_oidx, 3, 1, loudness_on ? 3 + (uint16_t)roundf(this->local_loudness - AUDIO_LIMIT_LOUDNESS_GAIN_MIN_ACTIVE) : 0);
      break;
    }
    case 2:
    {
      //amp settings page

      //update power target slider value display
      snprintf((char*)this->GetDLCommandPointer(this->power_target_value_oidx, 3), 5, "%3u%%", (uint8_t)(this->local_power_target * 100.0f));

      //update power target slider
      this->ModifyDLCommand16(this->power_target_slider_oidx, 3, 1, (uint16_t)roundf((this->local_power_target - 0.5f) * 10.0f));
      break;
      break;
    }
    default:
      break;
  }
}


void SettingsScreenAudio::OnScreenExit() {
  //reset to first page
  this->page_index = 0;

  //reset monitoring
  this->bbv2_manager.system.dap_if.monitor_src_stats = false;
  this->bbv2_manager.system.amp_if.monitor_measurements = false;

  //save settings
  this->bbv2_manager.system.eeprom_if.WriteAllDirtySections([](bool success) {
    DEBUG_PRINTF("Audio screen exit EEPROM write success %u\n", success);
  });
}


