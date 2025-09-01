/*
 * settings_screen_base.cpp
 *
 *  Created on: Aug 8, 2025
 *      Author: Alex
 */


#include "settings_screen_base.h"
#include "bbv2_gui_manager.h"
#include "gui_common_draws.h"


SettingsScreenBase::SettingsScreenBase(BlockBoxV2GUIManager& manager, uint8_t tab_index) :
    BlockBoxV2Screen(manager), tab_index(tab_index) {}


void SettingsScreenBase::HandleTouch(const GUITouchState& state) noexcept {
  //handle tab switching
  if (state.released && state.tag == state.initial_tag) {
    switch (state.tag) {
      case SCREEN_SETTINGS_TAG_TAB_0:
        if (this->tab_index != 0) {
          //goto tab 0 (audio)
          this->GoToScreen(&this->bbv2_manager.settings_screen_audio);
        }
        break;
      case SCREEN_SETTINGS_TAG_TAB_1:
        if (this->tab_index != 1) {
          //goto tab 1 (display)
          this->GoToScreen(&this->bbv2_manager.settings_screen_display);
        }
        break;
      case SCREEN_SETTINGS_TAG_TAB_2:
        if (this->tab_index != 2) {
          //TODO: goto tab 2 (light)
        }
        break;
      case SCREEN_SETTINGS_TAG_TAB_3:
        if (this->tab_index != 3) {
          this->GoToScreen(&this->bbv2_manager.settings_screen_power);
        }
        break;
      /*case SCREEN_SETTINGS_TAG_TAB_4:
        if (this->tab_index != 4) {
          //goto tab 4 - currently unused
        }
        break;*/
      case SCREEN_SETTINGS_TAG_TAB_5:
        if (this->tab_index != 5 && state.long_press) {
          this->GoToScreen(&this->bbv2_manager.settings_screen_debug);
        }
        break;
      case SCREEN_SETTINGS_TAG_CLOSE:
        //return to main screen
        this->GoToScreen(&this->bbv2_manager.main_screen);
        break;
      default:
        break;
    }
  }

  //allow base handling
  this->BlockBoxV2Screen::HandleTouch(state);
}


void SettingsScreenBase::BuildScreenContent() {
  //base handling (draw top status bar)
  this->BlockBoxV2Screen::BuildScreenContent();

  this->driver.CmdDL(DL_SAVE_CONTEXT);
  this->driver.CmdTag(0);

  uint32_t theme_colour_main = this->bbv2_manager.GetThemeColorMain();

  //selected tab rectangle
  this->driver.CmdBeginDraw(EVE_RECTS);
  this->driver.CmdDL(LINE_WIDTH(80));
  this->driver.CmdColorRGB(theme_colour_main);
  this->driver.CmdDL(VERTEX2F(16 * (12 + this->tab_index * 45), 16 * 32));
  this->driver.CmdDL(VERTEX2F(16 * (38 + this->tab_index * 45), 16 * 58));
  this->driver.CmdDL(DL_END);

  //submenu icons
  GUIDraws::SpeakerIconMedium(this->driver, 6, 25, 0xFFFFFF, (this->tab_index == 0) ? theme_colour_main : 0x000000);
  GUIDraws::ScreenIconMedium(this->driver, 50, 25, 0xFFFFFF);
  GUIDraws::BulbIconMedium(this->driver, 95, 25, 0xFFFFFF, (this->tab_index == 2) ? theme_colour_main : 0x000000, true);
  GUIDraws::LightningIconMedium(this->driver, 140, 25, 0xFFFFFF);
  if (this->tab_index == 5) {
    //cog icon on tab 5 (debug) only visible in debug menu itself
    GUIDraws::CogIconMedium(this->driver, 230, 25, 0xFFFFFF, theme_colour_main);
  }

  //close icon lines
  this->driver.CmdBeginDraw(EVE_LINES);
  this->driver.CmdDL(LINE_WIDTH(16));
  this->driver.CmdColorRGB(0xFFFFFF);
  this->driver.CmdDL(VERTEX2F(16 * 283, 16 * 36));
  this->driver.CmdDL(VERTEX2F(16 * 307, 16 * 60));
  this->driver.CmdDL(VERTEX2F(16 * 283, 16 * 60));
  this->driver.CmdDL(VERTEX2F(16 * 307, 16 * 36));
  //separator lines
  this->driver.CmdColorRGB(0x808080);
  this->driver.CmdDL(VERTEX2F(47 * 16 + 8, 26 * 16));
  this->driver.CmdDL(VERTEX2F(47 * 16 + 8, 70 * 16));
  this->driver.CmdDL(VERTEX2F(92 * 16 + 8, 26 * 16));
  this->driver.CmdDL(VERTEX2F(92 * 16 + 8, 70 * 16));
  this->driver.CmdDL(VERTEX2F(137 * 16 + 8, 26 * 16));
  this->driver.CmdDL(VERTEX2F(137 * 16 + 8, 70 * 16));
  this->driver.CmdDL(VERTEX2F(182 * 16 + 8, 26 * 16));
  this->driver.CmdDL(VERTEX2F(182 * 16 + 8, 70 * 16));
  this->driver.CmdDL(VERTEX2F(227 * 16 + 8, 26 * 16));
  this->driver.CmdDL(VERTEX2F(227 * 16 + 8, 70 * 16));
  this->driver.CmdDL(VERTEX2F(272 * 16 + 8, 26 * 16));
  this->driver.CmdDL(VERTEX2F(272 * 16 + 8, 70 * 16));
  this->driver.CmdDL(DL_END);

  //page dots
  this->driver.CmdBeginDraw(EVE_POINTS);
  this->driver.CmdDL(POINT_SIZE(40));
  //tab 0 (audio): 3 pages
  this->driver.CmdDL(VERTEX2F(16 * 17, 16 * 68));
  this->driver.CmdDL(VERTEX2F(16 * 25, 16 * 68));
  this->driver.CmdDL(VERTEX2F(16 * 33, 16 * 68));
  //tab 1 (display): 2 pages
  this->driver.CmdDL(VERTEX2F(16 * 66, 16 * 68));
  this->driver.CmdDL(VERTEX2F(16 * 74, 16 * 68));
  //tab 2 (light): 1 page
  this->driver.CmdDL(VERTEX2F(16 * 115, 16 * 68));
  //tab 3 (power): 2 pages
  this->driver.CmdDL(VERTEX2F(16 * 156, 16 * 68));
  this->driver.CmdDL(VERTEX2F(16 * 164, 16 * 68));
  //tab 5 (debug): 2 pages, only visible in debug menu itself
  if (this->tab_index == 5) {
    this->driver.CmdDL(VERTEX2F(16 * 246, 16 * 68));
    this->driver.CmdDL(VERTEX2F(16 * 254, 16 * 68));
  }
  this->driver.CmdDL(DL_END);

  //invisible touch areas
  this->driver.CmdDL(DL_SAVE_CONTEXT);
  this->driver.CmdBeginDraw(EVE_RECTS);
  this->driver.CmdDL(LINE_WIDTH(16));
  this->driver.CmdTagMask(true);
  this->driver.CmdDL(COLOR_MASK(0, 0, 0, 0));
  this->driver.CmdTag(SCREEN_SETTINGS_TAG_TAB_0); //tab 0
  this->driver.CmdDL(VERTEX2F(16 * 5, 16 * 25));
  this->driver.CmdDL(VERTEX2F(16 * 45, 16 * 70));
  this->driver.CmdTag(SCREEN_SETTINGS_TAG_TAB_1); //tab 1
  this->driver.CmdDL(VERTEX2F(16 * 50, 16 * 25));
  this->driver.CmdDL(VERTEX2F(16 * 90, 16 * 70));
  this->driver.CmdTag(SCREEN_SETTINGS_TAG_TAB_2); //tab 2
  this->driver.CmdDL(VERTEX2F(16 * 95, 16 * 25));
  this->driver.CmdDL(VERTEX2F(16 * 135, 16 * 70));
  this->driver.CmdTag(SCREEN_SETTINGS_TAG_TAB_3); //tab 3
  this->driver.CmdDL(VERTEX2F(16 * 140, 16 * 25));
  this->driver.CmdDL(VERTEX2F(16 * 180, 16 * 70));
  /*this->driver.CmdTag(SCREEN_SETTINGS_TAG_TAB_4); //tab 4
  this->driver.CmdDL(VERTEX2F(16 * 185, 16 * 25));
  this->driver.CmdDL(VERTEX2F(16 * 225, 16 * 70));*/
  this->driver.CmdTag(SCREEN_SETTINGS_TAG_TAB_5); //tab 5
  this->driver.CmdDL(VERTEX2F(16 * 230, 16 * 25));
  this->driver.CmdDL(VERTEX2F(16 * 270, 16 * 70));
  this->driver.CmdTag(SCREEN_SETTINGS_TAG_CLOSE); //close menu
  this->driver.CmdDL(VERTEX2F(16 * 275, 16 * 25));
  this->driver.CmdDL(VERTEX2F(16 * 315, 16 * 70));
  this->driver.CmdDL(DL_END);
  this->driver.CmdDL(DL_RESTORE_CONTEXT);

  this->driver.CmdDL(DL_RESTORE_CONTEXT);
}


void SettingsScreenBase::DrawActivePageDot(uint16_t x_pos) {
  this->driver.CmdDL(DL_SAVE_CONTEXT);

  //don't change existing tags
  this->driver.CmdTagMask(false);
  //large black dot to erase inactive page dot
  this->driver.CmdBeginDraw(EVE_POINTS);
  this->driver.CmdDL(POINT_SIZE(48));
  this->driver.CmdColorRGB(0x000000);
  this->driver.CmdDL(VERTEX2F(16 * x_pos, 16 * 68));
  //active page dot
  this->driver.CmdDL(POINT_SIZE(40));
  this->driver.CmdColorRGB(this->bbv2_manager.GetThemeColorMain());
  this->driver.CmdDL(VERTEX2F(16 * x_pos, 16 * 68));
  this->driver.CmdDL(DL_END);

  this->driver.CmdDL(DL_RESTORE_CONTEXT);
}

