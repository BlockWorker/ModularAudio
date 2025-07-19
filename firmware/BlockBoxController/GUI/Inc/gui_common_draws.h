/*
 * gui_common_draws.h
 *
 *  Created on: Jul 19, 2025
 *      Author: Alex
 */

#ifndef INC_GUI_COMMON_DRAWS_H_
#define INC_GUI_COMMON_DRAWS_H_


#include "EVE.h"


class GUIDraws final {
public:
  GUIDraws() = delete;

  static void LightningIconTiny(EVEDriver& drv, uint16_t x, uint16_t y);

  static void BluetoothIconSmall(EVEDriver& drv, uint16_t x, uint16_t y);
  static void USBIconSmall(EVEDriver& drv, uint16_t x, uint16_t y);
  static void SPDIFIconSmall(EVEDriver& drv, uint16_t x, uint16_t y, uint32_t bg_color);
};


#endif /* INC_GUI_COMMON_DRAWS_H_ */
