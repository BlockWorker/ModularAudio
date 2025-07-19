/*
 * bbv2_screen.h
 *
 *  Created on: Jul 19, 2025
 *      Author: Alex
 */

#ifndef INC_BBV2_SCREEN_H_
#define INC_BBV2_SCREEN_H_


#include "gui_screen.h"


class BlockBoxV2System;


//common screen base for BlockBox v2, with functions for the global status bar
class BlockBoxV2Screen : public GUIScreen {
public:
  BlockBoxV2Screen(GUIManager& manager, uint32_t display_timeout) : GUIScreen(manager, display_timeout), system(NULL) {}

  virtual void Init(BlockBoxV2System* system);

protected:
  BlockBoxV2System* system;

  virtual void BuildScreenContent() override;
};


#endif /* INC_BBV2_SCREEN_H_ */
