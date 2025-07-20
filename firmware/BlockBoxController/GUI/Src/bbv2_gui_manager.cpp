/*
 * bbv2_gui_manager.cpp
 *
 *  Created on: Jul 20, 2025
 *      Author: Alex
 */


#include "bbv2_gui_manager.h"
#include "system.h"


BlockBoxV2GUIManager::BlockBoxV2GUIManager(BlockBoxV2System& system) noexcept :
    GUIManager(system.eve_drv), system(system), init_screen(*this), test_screen(*this) {}


void BlockBoxV2GUIManager::Init() {
  //initialise screens
  this->init_screen.Init();
  this->test_screen.Init();

  //pre-set initial screen
  this->SetScreen(&this->init_screen);

  //base call to initialise the base manager itself
  this->GUIManager::Init();
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
