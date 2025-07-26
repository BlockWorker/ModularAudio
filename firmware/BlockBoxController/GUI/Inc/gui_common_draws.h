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

  //tiny in-battery (~9x12px)
  static void LightningIconTiny(EVEDriver& drv, uint16_t x, uint16_t y, uint32_t color);

  //small (~14x18px)
  static void BluetoothIconSmall(EVEDriver& drv, uint16_t x, uint16_t y, uint32_t color);
  static void USBIconSmall(EVEDriver& drv, uint16_t x, uint16_t y, uint32_t color);
  static void SPDIFIconSmall(EVEDriver& drv, uint16_t x, uint16_t y, uint32_t color, uint32_t bg_color);

  //small-ish (28x28px)
  static void SpeakerIconSmallish(EVEDriver& drv, uint16_t x, uint16_t y, uint32_t color, uint32_t bg_color);

  //medium (40x40px)
  static void BluetoothIconMedium(EVEDriver& drv, uint16_t x, uint16_t y, uint32_t color, bool dots);
  static void BulbIconMedium(EVEDriver& drv, uint16_t x, uint16_t y, uint32_t color, uint32_t bg_color, bool rays);
  static void CogIconMedium(EVEDriver& drv, uint16_t x, uint16_t y, uint32_t color, uint32_t bg_color);
  static void PowerIconMedium(EVEDriver& drv, uint16_t x, uint16_t y, uint32_t color, uint32_t bg_color);

  //large (50x50px)
  static void PlayIconLarge(EVEDriver& drv, uint16_t x, uint16_t y, uint32_t outline_color, uint32_t fill_color, uint32_t bg_color);
  static void PauseIconLarge(EVEDriver& drv, uint16_t x, uint16_t y, uint32_t outline_color, uint32_t fill_color);
  static void BackIconLarge(EVEDriver& drv, uint16_t x, uint16_t y, uint32_t outline_color, uint32_t fill_color, uint32_t bg_color);
  static void ForwardIconLarge(EVEDriver& drv, uint16_t x, uint16_t y, uint32_t outline_color, uint32_t fill_color, uint32_t bg_color);
  static void BWLogoLarge(EVEDriver& drv, uint16_t x, uint16_t y, uint32_t outline_color, uint32_t fill_color);

  //huge (100x100px)
  static void PowerIconHuge(EVEDriver& drv, uint16_t x, uint16_t y, uint32_t outline_color, uint32_t fill_color, uint32_t bg_color);
};


#endif /* INC_GUI_COMMON_DRAWS_H_ */
