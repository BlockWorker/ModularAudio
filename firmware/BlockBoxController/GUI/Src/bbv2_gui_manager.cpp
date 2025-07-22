/*
 * bbv2_gui_manager.cpp
 *
 *  Created on: Jul 20, 2025
 *      Author: Alex
 */


#include "bbv2_gui_manager.h"
#include "system.h"


//GUI config storage definitions
//touch transform matrix offset in bytes (size: 6 * 32-bit int = 24B)
#define GUI_CONFIG_TOUCH_TRANSFORM 0
//theme colour main (size: 4B)
#define GUI_CONFIG_THEME_COLOR_MAIN 24
//theme colour dark (size: 4B)
#define GUI_CONFIG_THEME_COLOR_DARK 28
//total storage bytes
#define GUI_CONFIG_SIZE_BYTES 32


BlockBoxV2GUIManager::BlockBoxV2GUIManager(BlockBoxV2System& system) noexcept :
    GUIManager(system.eve_drv), system(system), gui_config(system.eeprom_if, GUI_CONFIG_SIZE_BYTES, BlockBoxV2GUIManager::LoadConfigDefaults),
    init_screen(*this), test_screen(*this) {}


void BlockBoxV2GUIManager::Init() {
  //initialise screens
  this->init_screen.Init();
  this->test_screen.Init();

  //pre-set initial screen
  this->SetScreen(&this->init_screen);

  //base call to initialise the base manager itself
  this->GUIManager::Init();
}


TouchTransformMatrix BlockBoxV2GUIManager::GetTouchMatrix() const {
  TouchTransformMatrix matrix;
  memcpy(&matrix, this->gui_config[GUI_CONFIG_TOUCH_TRANSFORM], sizeof(TouchTransformMatrix));
  return matrix;
}

uint32_t BlockBoxV2GUIManager::GetThemeColorMain() const {
  return this->gui_config.GetValue32(GUI_CONFIG_THEME_COLOR_MAIN);
}

uint32_t BlockBoxV2GUIManager::GetThemeColorDark() const {
  return this->gui_config.GetValue32(GUI_CONFIG_THEME_COLOR_DARK);
}


void BlockBoxV2GUIManager::SetTouchMatrix(TouchTransformMatrix matrix) {
  this->gui_config.SetData(GUI_CONFIG_TOUCH_TRANSFORM, (const uint8_t*)&matrix, sizeof(TouchTransformMatrix));
}

void BlockBoxV2GUIManager::SetThemeColors(uint32_t main, uint32_t dark) {
  this->gui_config.SetValue32(GUI_CONFIG_THEME_COLOR_MAIN, main);
  this->gui_config.SetValue32(GUI_CONFIG_THEME_COLOR_DARK, dark);
}


//updates the init progress message, or proceeds past init if given null pointer
void BlockBoxV2GUIManager::SetInitProgress(const char* progress_string, bool error) {
  if (this->current_screen != &this->init_screen) {
    //not on init screen: this function shouldn't be called, ignore
    return;
  }

  if (progress_string == NULL) {
    //null string signals that init is done - proceed past init screen
    this->SetScreen(&this->test_screen);
  } else {
    //update init screen
    this->init_screen.UpdateProgressString(progress_string, error);
  }
}


void BlockBoxV2GUIManager::LoadConfigDefaults(StorageSection& section) {
  //write all zeros to the touch transform matrix (signals that touch calibration hasn't occurred yet)
  uint8_t zeros[sizeof(TouchTransformMatrix)] = { 0 };
  section.SetData(GUI_CONFIG_TOUCH_TRANSFORM, zeros, sizeof(TouchTransformMatrix));

  //write default theme colours
  section.SetValue32(GUI_CONFIG_THEME_COLOR_MAIN, 0x0060FF);
  section.SetValue32(GUI_CONFIG_THEME_COLOR_DARK, 0x002060);
}


void BlockBoxV2GUIManager::InitTouchCalibration() {
  //TODO testing only: default behaviour
  this->GUIManager::InitTouchCalibration();
}
