/*
 * bbv2_screen.h
 *
 *  Created on: Jul 19, 2025
 *      Author: Alex
 */

#ifndef INC_BBV2_SCREEN_H_
#define INC_BBV2_SCREEN_H_


#include "gui_screen.h"
#include "audio_path_manager.h"


class BlockBoxV2GUIManager;


//common screen base for BlockBox v2, with functions for the global status bar
class BlockBoxV2Screen : public GUIScreen {
public:
  //widths of "..." in pixels, for various fonts
  static const uint16_t dots_width_20, dots_width_26, dots_width_27, dots_width_28;

  BlockBoxV2GUIManager& bbv2_manager;

  BlockBoxV2Screen(BlockBoxV2GUIManager& manager);

  virtual void Init();

  //functions like strncpy(dest, src, max_chars), but limits the pixel-width of the text (in the given font) to the given maximum, shortening with "..." if necessary.
  static void CopyOrShortenText(char* dest, const char* src, uint16_t max_chars, uint16_t max_width, uint8_t font);

protected:
  const char* status_text_override; //if set to something non-null, overrides the usual input-based status bar text

  virtual void BuildScreenContent() override;

private:
  //values currently drawn on screen - used to check if we need to redraw on update events
  AudioPathInput currently_drawn_input;
  bool currently_drawn_battery_valid;
  uint8_t currently_drawn_battery_percent;
  bool currently_drawn_battery_precise;

};


#endif /* INC_BBV2_SCREEN_H_ */
