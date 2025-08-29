/*
 * init_screen.h
 *
 *  Created on: Jul 20, 2025
 *      Author: Alex
 */

#ifndef INC_INIT_SCREEN_H_
#define INC_INIT_SCREEN_H_


#include "bbv2_screen.h"


class InitScreen : public BlockBoxV2Screen {
public:
  InitScreen(BlockBoxV2GUIManager& manager);

  void UpdateProgressString(const char* progress_string, bool error);

protected:
  void BuildScreenContent() override;
  void UpdateExistingScreenContent() override;

private:
  const char* init_progress_string;
  bool init_error;
};


#endif /* INC_INIT_SCREEN_H_ */
