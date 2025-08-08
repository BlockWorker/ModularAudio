/*
 * settings_screen_audio.cpp
 *
 *  Created on: Aug 8, 2025
 *      Author: Alex
 */


#include "settings_screen_audio.h"


//settings tab index
#define SCREEN_AUDIO_TAB_INDEX 0


SettingsScreenAudio::SettingsScreenAudio(BlockBoxV2GUIManager& manager) :
    SettingsScreenBase(manager, SCREEN_AUDIO_TAB_INDEX), page_index(0) {}


void SettingsScreenAudio::HandleTouch(const GUITouchState& state) noexcept {
  if (state.released && state.tag == state.initial_tag) {
    switch (state.tag) {
      case SCREEN_SETTINGS_TAG_TAB_0:
        //go to next page
        this->page_index = (this->page_index + 1) % 3;
        this->needs_display_list_rebuild = true;
        return;
      default:
        break;
    }
  }

  //base handling: tab switching etc
  this->SettingsScreenBase::HandleTouch(state);
}



void SettingsScreenAudio::BuildScreenContent() {
  //TODO controls etc

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
  //TODO update controls etc
}


void SettingsScreenAudio::OnScreenExit() {
  //TODO: save settings
  this->page_index = 0;
  DEBUG_PRINTF("Audio screen exiting\n");
}


