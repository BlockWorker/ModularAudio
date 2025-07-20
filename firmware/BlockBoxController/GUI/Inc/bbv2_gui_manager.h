/*
 * bbv2_gui_manager.h
 *
 *  Created on: Jul 20, 2025
 *      Author: Alex
 */

#ifndef INC_BBV2_GUI_MANAGER_H_
#define INC_BBV2_GUI_MANAGER_H_


#include "gui_manager.h"
#include "init_screen.h"
#include "test_screen.h"


#ifdef __cplusplus


class BlockBoxV2System;


class BlockBoxV2GUIManager : public GUIManager {
public:
  BlockBoxV2System& system;

  InitScreen init_screen;
  TestScreen test_screen;


  BlockBoxV2GUIManager(BlockBoxV2System& system) noexcept;

  void Init() override;


  //updates the init progress message, or proceeds past init if given null pointer
  void SetInitProgress(const char* progress_string, bool error);

};


#endif


#endif /* INC_BBV2_GUI_MANAGER_H_ */
