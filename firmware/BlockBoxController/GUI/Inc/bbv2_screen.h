/*
 * bbv2_screen.h
 *
 *  Created on: Jul 19, 2025
 *      Author: Alex
 */

#ifndef INC_BBV2_SCREEN_H_
#define INC_BBV2_SCREEN_H_


#include "gui_screen.h"


class BlockBoxV2GUIManager;


//common screen base for BlockBox v2, with functions for the global status bar
class BlockBoxV2Screen : public GUIScreen {
public:
  BlockBoxV2GUIManager& bbv2_manager;

  BlockBoxV2Screen(BlockBoxV2GUIManager& manager, uint32_t display_timeout);

  virtual void Init();

protected:
  const char* status_text_override; //if set to something non-null, overrides the usual input-based status bar text

  virtual void BuildScreenContent() override;

};


#endif /* INC_BBV2_SCREEN_H_ */
