/*
 * gui_common_draws.cpp
 *
 *  Created on: Jul 19, 2025
 *      Author: Alex
 */


#include "gui_common_draws.h"


void GUIDraws::LightningIconTiny(EVEDriver& drv, uint16_t x, uint16_t y) {
  drv.CmdDL(DL_SAVE_CONTEXT);
  drv.CmdDL(DL_BEGIN | EVE_LINE_STRIP);
  drv.CmdDL(LINE_WIDTH(18));
  drv.CmdDL(DL_COLOR_RGB | 0xFFFFFF);
  drv.CmdDL(VERTEX_TRANSLATE_X(16 * x + 8));
  drv.CmdDL(VERTEX_TRANSLATE_Y(16 * y));
  drv.CmdDL(VERTEX2F(16 * 1 + 8, 8));
  drv.CmdDL(VERTEX2F(16 * 4, 8));
  drv.CmdDL(VERTEX2F(16 * 1 + 8, 16 * 4 + 8));
  drv.CmdDL(VERTEX2F(16 * 5, 16 * 4 + 8));
  drv.CmdDL(VERTEX2F(16 * 1 + 8, 16 * 8 + 8));
  drv.CmdDL(VERTEX2F(16 * 2 + 8, 16 * 4 + 8));
  drv.CmdDL(VERTEX2F(0, 16 * 4 + 8));
  drv.CmdDL(VERTEX2F(16 * 1 + 8, 8));
  drv.CmdDL(DL_END);
  drv.CmdDL(DL_RESTORE_CONTEXT);
}

void GUIDraws::BluetoothIconSmall(EVEDriver& drv, uint16_t x, uint16_t y) {
  drv.CmdDL(DL_SAVE_CONTEXT);
  drv.CmdDL(VERTEX_TRANSLATE_X(16 * x));
  drv.CmdDL(VERTEX_TRANSLATE_Y(16 * y));
  drv.CmdDL(DL_BEGIN | EVE_LINE_STRIP);
  drv.CmdDL(LINE_WIDTH(16));
  drv.CmdDL(DL_COLOR_RGB | 0xFFFFFF);
  drv.CmdDL(VERTEX2F(16 * 4, 16 * 6 + 12));
  drv.CmdDL(VERTEX2F(16 * 12, 16 * 14 + 4));
  drv.CmdDL(VERTEX2F(16 * 8, 16 * 18));
  drv.CmdDL(VERTEX2F(16 * 8, 16 * 3));
  drv.CmdDL(VERTEX2F(16 * 12, 16 * 6 + 12));
  drv.CmdDL(VERTEX2F(16 * 4, 16 * 14 + 4));
  drv.CmdDL(DL_END);
  drv.CmdDL(DL_RESTORE_CONTEXT);
}

void GUIDraws::USBIconSmall(EVEDriver& drv, uint16_t x, uint16_t y) {
  drv.CmdDL(DL_SAVE_CONTEXT);
  drv.CmdDL(VERTEX_TRANSLATE_X(16 * x));
  drv.CmdDL(VERTEX_TRANSLATE_Y(16 * y));
  drv.CmdDL(LINE_WIDTH(16));
  drv.CmdDL(DL_COLOR_RGB | 0xFFFFFF);
  drv.CmdDL(DL_BEGIN | EVE_LINE_STRIP);
  drv.CmdDL(VERTEX2F(16 * 8, 16 * 3));
  drv.CmdDL(VERTEX2F(16 * 9 + 4, 16 * 5));
  drv.CmdDL(VERTEX2F(16 * 6 + 12, 16 * 5));
  drv.CmdDL(VERTEX2F(16 * 8, 16 * 3));
  drv.CmdDL(VERTEX2F(16 * 8, 16 * 16));
  drv.CmdDL(DL_END);
  drv.CmdDL(DL_BEGIN | EVE_RECTS);
  drv.CmdDL(VERTEX2F(16 * 12 + 4, 16 * 6 + 4));
  drv.CmdDL(VERTEX2F(16 * 14, 16 * 8));
  drv.CmdDL(DL_END);
  drv.CmdDL(DL_BEGIN | EVE_POINTS);
  drv.CmdDL(POINT_SIZE(32));
  drv.CmdDL(VERTEX2F(16 * 3 + 4, 16 * 9));
  drv.CmdDL(POINT_SIZE(48));
  drv.CmdDL(VERTEX2F(16 * 8, 16 * 16));
  drv.CmdDL(DL_END);
  drv.CmdDL(DL_BEGIN | EVE_LINES);
  drv.CmdDL(VERTEX2F(16 * 3 + 4, 16 * 9));
  drv.CmdDL(VERTEX2F(16 * 3 + 4, 16 * 12));
  drv.CmdDL(VERTEX2F(16 * 3 + 4, 16 * 12));
  drv.CmdDL(VERTEX2F(16 * 8, 16 * 16));
  drv.CmdDL(VERTEX2F(16 * 13 + 12, 16 * 7));
  drv.CmdDL(VERTEX2F(16 * 13, 16 * 10));
  drv.CmdDL(VERTEX2F(16 * 13, 16 * 10));
  drv.CmdDL(VERTEX2F(16 * 8, 16 * 14));
  drv.CmdDL(DL_END);
  drv.CmdDL(DL_RESTORE_CONTEXT);
}

void GUIDraws::SPDIFIconSmall(EVEDriver& drv, uint16_t x, uint16_t y, uint32_t bg_color) {
  drv.CmdDL(DL_SAVE_CONTEXT);
  drv.CmdDL(VERTEX_TRANSLATE_X(16 * x));
  drv.CmdDL(VERTEX_TRANSLATE_Y(16 * y));
  drv.CmdDL(DL_BEGIN | EVE_LINE_STRIP);
  drv.CmdDL(LINE_WIDTH(16));
  drv.CmdDL(DL_COLOR_RGB | 0xFFFFFF);
  drv.CmdDL(VERTEX2F(16 * 3, 16 * 15));
  drv.CmdDL(VERTEX2F(16 * 13, 16 * 15));
  drv.CmdDL(VERTEX2F(16 * 13, 16 * 7));
  drv.CmdDL(VERTEX2F(16 * 11, 16 * 5));
  drv.CmdDL(VERTEX2F(16 * 5, 16 * 5));
  drv.CmdDL(VERTEX2F(16 * 3, 16 * 7));
  drv.CmdDL(VERTEX2F(16 * 3, 16 * 15));
  drv.CmdDL(DL_END);
  drv.CmdDL(DL_BEGIN | EVE_POINTS);
  drv.CmdDL(POINT_SIZE(36));
  drv.CmdDL(VERTEX2F(16 * 8, 16 * 10));
  drv.CmdDL(DL_COLOR_RGB | (bg_color & 0xFFFFFF));
  drv.CmdDL(POINT_SIZE(12));
  drv.CmdDL(VERTEX2F(16 * 8, 16 * 10));
  drv.CmdDL(DL_END);
  drv.CmdDL(DL_RESTORE_CONTEXT);
}


