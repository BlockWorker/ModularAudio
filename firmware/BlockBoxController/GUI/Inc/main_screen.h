/*
 * main_screen.h
 *
 *  Created on: Jul 25, 2025
 *      Author: Alex
 */

#ifndef INC_MAIN_SCREEN_H_
#define INC_MAIN_SCREEN_H_


#include "bbv2_screen.h"
#include "audio_path_manager.h"
#include "bluetooth_receiver_interface.h"


class MainScreen : public BlockBoxV2Screen {
public:
  MainScreen(BlockBoxV2GUIManager& manager);

  void DisplayScreen() override;

  void HandleTouch(const GUITouchState& state) noexcept override;

  void Init() override;

protected:
  void BuildScreenContent() override;

  void UpdateExistingScreenContent() override;

private:
  //values currently drawn on screen - used to check if we need to redraw on update events
  AudioPathInput currently_drawn_input;
  bool currently_drawn_streaming;
  uint32_t currently_drawn_input_rate;
  BluetoothReceiverStatus currently_drawn_bt_status;
  bool currently_drawn_mute;
  int8_t currently_drawn_volume_dB;

  bool dropdown_open;
};


#endif /* INC_MAIN_SCREEN_H_ */
