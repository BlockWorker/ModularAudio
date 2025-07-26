/*
 * gui_common_draws.cpp
 *
 *  Created on: Jul 19, 2025
 *      Author: Alex
 */


#include "gui_common_draws.h"


/**********************************************/
/*         Tiny in-battery (~9x12px)          */
/**********************************************/

void GUIDraws::LightningIconTiny(EVEDriver& drv, uint16_t x, uint16_t y, uint32_t color) {
  drv.CmdDL(DL_SAVE_CONTEXT);
  drv.CmdBeginDraw(EVE_LINE_STRIP);
  drv.CmdDL(LINE_WIDTH(18));
  drv.CmdColorRGB(color);
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


/**********************************************/
/*              Small (~14x18px)              */
/**********************************************/

void GUIDraws::BluetoothIconSmall(EVEDriver& drv, uint16_t x, uint16_t y, uint32_t color) {
  drv.CmdDL(DL_SAVE_CONTEXT);
  drv.CmdDL(VERTEX_TRANSLATE_X(16 * x));
  drv.CmdDL(VERTEX_TRANSLATE_Y(16 * y));
  drv.CmdBeginDraw(EVE_LINE_STRIP);
  drv.CmdDL(LINE_WIDTH(16));
  drv.CmdColorRGB(color);
  drv.CmdDL(VERTEX2F(16 * 4, 16 * 6 + 12));
  drv.CmdDL(VERTEX2F(16 * 12, 16 * 14 + 4));
  drv.CmdDL(VERTEX2F(16 * 8, 16 * 18));
  drv.CmdDL(VERTEX2F(16 * 8, 16 * 3));
  drv.CmdDL(VERTEX2F(16 * 12, 16 * 6 + 12));
  drv.CmdDL(VERTEX2F(16 * 4, 16 * 14 + 4));
  drv.CmdDL(DL_END);
  drv.CmdDL(DL_RESTORE_CONTEXT);
}

void GUIDraws::USBIconSmall(EVEDriver& drv, uint16_t x, uint16_t y, uint32_t color) {
  drv.CmdDL(DL_SAVE_CONTEXT);
  drv.CmdDL(VERTEX_TRANSLATE_X(16 * x));
  drv.CmdDL(VERTEX_TRANSLATE_Y(16 * y));
  drv.CmdDL(LINE_WIDTH(16));
  drv.CmdColorRGB(color);
  drv.CmdBeginDraw(EVE_LINE_STRIP);
  drv.CmdDL(VERTEX2F(16 * 8, 16 * 3));
  drv.CmdDL(VERTEX2F(16 * 9 + 4, 16 * 5));
  drv.CmdDL(VERTEX2F(16 * 6 + 12, 16 * 5));
  drv.CmdDL(VERTEX2F(16 * 8, 16 * 3));
  drv.CmdDL(VERTEX2F(16 * 8, 16 * 16));
  drv.CmdDL(DL_END);
  drv.CmdBeginDraw(EVE_RECTS);
  drv.CmdDL(VERTEX2F(16 * 12 + 4, 16 * 6 + 4));
  drv.CmdDL(VERTEX2F(16 * 14, 16 * 8));
  drv.CmdDL(DL_END);
  drv.CmdBeginDraw(EVE_POINTS);
  drv.CmdDL(POINT_SIZE(32));
  drv.CmdDL(VERTEX2F(16 * 3 + 4, 16 * 9));
  drv.CmdDL(POINT_SIZE(48));
  drv.CmdDL(VERTEX2F(16 * 8, 16 * 16));
  drv.CmdDL(DL_END);
  drv.CmdBeginDraw(EVE_LINES);
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

void GUIDraws::SPDIFIconSmall(EVEDriver& drv, uint16_t x, uint16_t y, uint32_t color, uint32_t bg_color) {
  drv.CmdDL(DL_SAVE_CONTEXT);
  drv.CmdDL(VERTEX_TRANSLATE_X(16 * x));
  drv.CmdDL(VERTEX_TRANSLATE_Y(16 * y));
  drv.CmdBeginDraw(EVE_LINE_STRIP);
  drv.CmdDL(LINE_WIDTH(16));
  drv.CmdColorRGB(color);
  drv.CmdDL(VERTEX2F(16 * 3, 16 * 15));
  drv.CmdDL(VERTEX2F(16 * 13, 16 * 15));
  drv.CmdDL(VERTEX2F(16 * 13, 16 * 7));
  drv.CmdDL(VERTEX2F(16 * 11, 16 * 5));
  drv.CmdDL(VERTEX2F(16 * 5, 16 * 5));
  drv.CmdDL(VERTEX2F(16 * 3, 16 * 7));
  drv.CmdDL(VERTEX2F(16 * 3, 16 * 15));
  drv.CmdDL(DL_END);
  drv.CmdBeginDraw(EVE_POINTS);
  drv.CmdDL(POINT_SIZE(36));
  drv.CmdDL(VERTEX2F(16 * 8, 16 * 10));
  drv.CmdColorRGB(bg_color);
  drv.CmdDL(POINT_SIZE(12));
  drv.CmdDL(VERTEX2F(16 * 8, 16 * 10));
  drv.CmdDL(DL_END);
  drv.CmdDL(DL_RESTORE_CONTEXT);
}


/**********************************************/
/*            Small-ish (28x28px)             */
/**********************************************/

void GUIDraws::SpeakerIconSmallish(EVEDriver& drv, uint16_t x, uint16_t y, uint32_t color, uint32_t bg_color) {
  //setup
  drv.CmdDL(DL_SAVE_CONTEXT);
  drv.CmdDL(VERTEX_TRANSLATE_X(16 * x));
  drv.CmdDL(VERTEX_TRANSLATE_Y(16 * y));
  drv.CmdScissor(x, y, 28, 28);
  //right wave: outer
  drv.CmdBeginDraw(EVE_POINTS);
  drv.CmdDL(POINT_SIZE(168));
  drv.CmdColorRGB(color);
  drv.CmdDL(VERTEX2F(224, 224));
  //right wave: inner
  drv.CmdDL(POINT_SIZE(146));
  drv.CmdColorRGB(bg_color);
  drv.CmdDL(VERTEX2F(224, 224));
  //middle wave: outer
  drv.CmdDL(POINT_SIZE(129));
  drv.CmdColorRGB(color);
  drv.CmdDL(VERTEX2F(212, 224));
  //middle wave: inner
  drv.CmdDL(POINT_SIZE(107));
  drv.CmdColorRGB(bg_color);
  drv.CmdDL(VERTEX2F(212, 224));
  //left wave: outer
  drv.CmdDL(POINT_SIZE(90));
  drv.CmdColorRGB(color);
  drv.CmdDL(VERTEX2F(197, 224));
  //left wave: inner
  drv.CmdDL(POINT_SIZE(68));
  drv.CmdColorRGB(bg_color);
  drv.CmdDL(VERTEX2F(197, 224));
  drv.CmdDL(DL_END);
  //wave cutoff
  drv.CmdBeginDraw(EVE_EDGE_STRIP_L);
  drv.CmdDL(LINE_WIDTH(6));
  drv.CmdDL(VERTEX2F(325, 40));
  drv.CmdDL(VERTEX2F(182, 224));
  drv.CmdDL(VERTEX2F(325, 404));
  drv.CmdDL(DL_END);
  //body rect
  drv.CmdBeginDraw(EVE_RECTS);
  drv.CmdDL(LINE_WIDTH(16));
  drv.CmdColorRGB(color);
  drv.CmdDL(VERTEX2F(64, 98));
  drv.CmdDL(VERTEX2F(170, 350));
  drv.CmdDL(DL_END);
  //body cutouts
  drv.CmdBeginDraw(EVE_EDGE_STRIP_L);
  drv.CmdDL(LINE_WIDTH(6));
  drv.CmdColorRGB(bg_color);
  drv.CmdDL(VERTEX2F(185, 72));
  drv.CmdDL(VERTEX2F(118, 139));
  drv.CmdDL(VERTEX2F(45, 139));
  drv.CmdDL(VERTEX2F(45, 308));
  drv.CmdDL(VERTEX2F(118, 308));
  drv.CmdDL(VERTEX2F(185, 375));
  drv.CmdDL(DL_END);
  //cleanup
  drv.CmdDL(DL_RESTORE_CONTEXT);
}


/**********************************************/
/*              Medium (40x40px)              */
/**********************************************/

void GUIDraws::BluetoothIconMedium(EVEDriver& drv, uint16_t x, uint16_t y, uint32_t color, bool dots) {
  //setup
  drv.CmdDL(DL_SAVE_CONTEXT);
  drv.CmdDL(VERTEX_TRANSLATE_X(16 * x));
  drv.CmdDL(VERTEX_TRANSLATE_Y(16 * y));
  //lines
  drv.CmdBeginDraw(EVE_LINE_STRIP);
  drv.CmdDL(LINE_WIDTH(16));
  drv.CmdColorRGB(color);
  drv.CmdDL(VERTEX2F(207, 207));
  drv.CmdDL(VERTEX2F(435, 435));
  drv.CmdDL(VERTEX2F(320, 550));
  drv.CmdDL(VERTEX2F(320, 89));
  drv.CmdDL(VERTEX2F(435, 204));
  drv.CmdDL(VERTEX2F(207, 432));
  drv.CmdDL(DL_END);
  //pairing dots if desired
  if (dots) {
    drv.CmdBeginDraw(EVE_POINTS);
    drv.CmdDL(POINT_SIZE(32));
    drv.CmdDL(VERTEX2F(188, 320));
    drv.CmdDL(VERTEX2F(452, 320));
    drv.CmdDL(DL_END);
  }
  //cleanup
  drv.CmdDL(DL_RESTORE_CONTEXT);
}

void GUIDraws::BulbIconMedium(EVEDriver& drv, uint16_t x, uint16_t y, uint32_t color, uint32_t bg_color, bool rays) {
  //setup
  drv.CmdDL(DL_SAVE_CONTEXT);
  drv.CmdDL(VERTEX_TRANSLATE_X(16 * x));
  drv.CmdDL(VERTEX_TRANSLATE_Y(16 * y));
  //outer circle
  drv.CmdBeginDraw(EVE_POINTS);
  drv.CmdDL(POINT_SIZE(129));
  drv.CmdColorRGB(color);
  drv.CmdDL(VERTEX2F(320, 321));
  //inner cirle
  drv.CmdDL(POINT_SIZE(97));
  drv.CmdColorRGB(bg_color);
  drv.CmdDL(VERTEX2F(320, 321));
  drv.CmdDL(DL_END);
  //circle cut
  drv.CmdBeginDraw(EVE_LINES);
  drv.CmdDL(LINE_WIDTH(40));
  drv.CmdDL(VERTEX2F(320, 392));
  drv.CmdDL(VERTEX2F(320, 446));
  drv.CmdColorRGB(color);
  //rays if desired
  if (rays) {
    drv.CmdDL(LINE_WIDTH(16));
    drv.CmdDL(VERTEX2F(156, 268));
    drv.CmdDL(VERTEX2F(105, 241));
    drv.CmdDL(VERTEX2F(218, 180));
    drv.CmdDL(VERTEX2F(182, 134));
    drv.CmdDL(VERTEX2F(320, 150));
    drv.CmdDL(VERTEX2F(320, 92));
    drv.CmdDL(VERTEX2F(421, 180));
    drv.CmdDL(VERTEX2F(457, 134));
    drv.CmdDL(VERTEX2F(483, 268));
    drv.CmdDL(VERTEX2F(534, 241));
  }
  drv.CmdDL(DL_END);
  //socket edge
  drv.CmdBeginDraw(EVE_LINE_STRIP);
  drv.CmdDL(LINE_WIDTH(20));
  drv.CmdDL(VERTEX2F(273, 424));
  drv.CmdDL(VERTEX2F(273, 495));
  drv.CmdDL(VERTEX2F(288, 526));
  drv.CmdDL(VERTEX2F(350, 526));
  drv.CmdDL(VERTEX2F(366, 495));
  drv.CmdDL(VERTEX2F(366, 424));
  drv.CmdDL(DL_END);
  //socket fill
  drv.CmdBeginDraw(EVE_RECTS);
  drv.CmdDL(VERTEX2F(296, 444));
  drv.CmdDL(VERTEX2F(344, 510));
  drv.CmdDL(DL_END);
  //cleanup
  drv.CmdDL(DL_RESTORE_CONTEXT);
}

void GUIDraws::CogIconMedium(EVEDriver& drv, uint16_t x, uint16_t y, uint32_t color, uint32_t bg_color) {
  //setup
  drv.CmdDL(DL_SAVE_CONTEXT);
  drv.CmdDL(VERTEX_TRANSLATE_X(16 * x));
  drv.CmdDL(VERTEX_TRANSLATE_Y(16 * y));
  //center circle outer
  drv.CmdBeginDraw(EVE_POINTS);
  drv.CmdDL(POINT_SIZE(94));
  drv.CmdColorRGB(color);
  drv.CmdDL(VERTEX2F(320, 320));
  //center circle inner
  drv.CmdDL(POINT_SIZE(62));
  drv.CmdColorRGB(bg_color);
  drv.CmdDL(VERTEX2F(320, 320));
  drv.CmdDL(DL_END);
  //outer line
  drv.CmdBeginDraw(EVE_LINE_STRIP);
  drv.CmdDL(LINE_WIDTH(16));
  drv.CmdColorRGB(color);
  drv.CmdDL(VERTEX2F(291, 112));
  drv.CmdDL(VERTEX2F(276, 163));
  drv.CmdDL(VERTEX2F(241, 177));
  drv.CmdDL(VERTEX2F(194, 152));
  drv.CmdDL(VERTEX2F(153, 192));
  drv.CmdDL(VERTEX2F(178, 239));
  drv.CmdDL(VERTEX2F(164, 274));
  drv.CmdDL(VERTEX2F(112, 290));
  drv.CmdDL(VERTEX2F(112, 346));
  drv.CmdDL(VERTEX2F(162, 362));
  drv.CmdDL(VERTEX2F(177, 399));
  drv.CmdDL(VERTEX2F(152, 446));
  drv.CmdDL(VERTEX2F(192, 486));
  drv.CmdDL(VERTEX2F(239, 461));
  drv.CmdDL(VERTEX2F(275, 476));
  drv.CmdDL(VERTEX2F(291, 527));
  drv.CmdDL(VERTEX2F(348, 527));
  drv.CmdDL(VERTEX2F(363, 477));
  drv.CmdDL(VERTEX2F(400, 461));
  drv.CmdDL(VERTEX2F(447, 486));
  drv.CmdDL(VERTEX2F(488, 445));
  drv.CmdDL(VERTEX2F(462, 398));
  drv.CmdDL(VERTEX2F(478, 361));
  drv.CmdDL(VERTEX2F(527, 346));
  drv.CmdDL(VERTEX2F(527, 290));
  drv.CmdDL(VERTEX2F(476, 274));
  drv.CmdDL(VERTEX2F(461, 239));
  drv.CmdDL(VERTEX2F(486, 192));
  drv.CmdDL(VERTEX2F(446, 152));
  drv.CmdDL(VERTEX2F(398, 177));
  drv.CmdDL(VERTEX2F(364, 163));
  drv.CmdDL(VERTEX2F(348, 112));
  drv.CmdDL(VERTEX2F(291, 112));
  drv.CmdDL(DL_END);
  //cleanup
  drv.CmdDL(DL_RESTORE_CONTEXT);
}

void GUIDraws::PowerIconMedium(EVEDriver& drv, uint16_t x, uint16_t y, uint32_t color, uint32_t bg_color) {
  //setup
  drv.CmdDL(DL_SAVE_CONTEXT);
  drv.CmdDL(VERTEX_TRANSLATE_X(16 * x));
  drv.CmdDL(VERTEX_TRANSLATE_Y(16 * y));
  //outer circle
  drv.CmdBeginDraw(EVE_POINTS);
  drv.CmdDL(POINT_SIZE(208));
  drv.CmdColorRGB(color);
  drv.CmdDL(VERTEX2F(320, 349));
  //inner circle
  drv.CmdDL(POINT_SIZE(176));
  drv.CmdColorRGB(bg_color);
  drv.CmdDL(VERTEX2F(320, 349));
  drv.CmdDL(DL_END);
  //circle cut
  drv.CmdBeginDraw(EVE_LINES);
  drv.CmdDL(LINE_WIDTH(56));
  drv.CmdDL(VERTEX2F(320, 120));
  drv.CmdDL(VERTEX2F(320, 192));
  //line
  drv.CmdDL(LINE_WIDTH(16));
  drv.CmdColorRGB(color);
  drv.CmdDL(VERTEX2F(320, 97));
  drv.CmdDL(VERTEX2F(320, 273));
  drv.CmdDL(DL_END);
  //cleanup
  drv.CmdDL(DL_RESTORE_CONTEXT);
}


/**********************************************/
/*              Large (50x50px)               */
/**********************************************/

void GUIDraws::PlayIconLarge(EVEDriver& drv, uint16_t x, uint16_t y, uint32_t outline_color, uint32_t fill_color, uint32_t bg_color) {
  //setup
  drv.CmdDL(DL_SAVE_CONTEXT);
  drv.CmdDL(VERTEX_TRANSLATE_X(16 * x));
  drv.CmdDL(VERTEX_TRANSLATE_Y(16 * y));
  drv.CmdScissor(x, y, 50, 50);
  //inner rect
  drv.CmdBeginDraw(EVE_RECTS);
  drv.CmdDL(LINE_WIDTH(16));
  drv.CmdColorRGB(fill_color);
  drv.CmdDL(VERTEX2F(172, 127));
  drv.CmdDL(VERTEX2F(648, 673));
  drv.CmdDL(DL_END);
  //cutout
  drv.CmdBeginDraw(EVE_EDGE_STRIP_R);
  drv.CmdDL(LINE_WIDTH(8));
  drv.CmdColorRGB(bg_color);
  drv.CmdDL(VERTEX2F(150, 100));
  drv.CmdDL(VERTEX2F(665, 400));
  drv.CmdDL(VERTEX2F(150, 699));
  drv.CmdDL(DL_END);
  //outline
  drv.CmdBeginDraw(EVE_LINE_STRIP);
  drv.CmdDL(LINE_WIDTH(16));
  drv.CmdColorRGB(outline_color);
  drv.CmdDL(VERTEX2F(167, 110));
  drv.CmdDL(VERTEX2F(665, 400));
  drv.CmdDL(VERTEX2F(167, 689));
  drv.CmdDL(VERTEX2F(167, 110));
  drv.CmdDL(DL_END);
  //cleanup
  drv.CmdDL(DL_RESTORE_CONTEXT);
}

void GUIDraws::PauseIconLarge(EVEDriver& drv, uint16_t x, uint16_t y, uint32_t outline_color, uint32_t fill_color) {
  //setup
  drv.CmdDL(DL_SAVE_CONTEXT);
  drv.CmdDL(VERTEX_TRANSLATE_X(16 * x));
  drv.CmdDL(VERTEX_TRANSLATE_Y(16 * y));
  //outer rects
  drv.CmdBeginDraw(EVE_RECTS);
  drv.CmdDL(LINE_WIDTH(48));
  drv.CmdColorRGB(outline_color);
  drv.CmdDL(VERTEX2F(207, 126));
  drv.CmdDL(VERTEX2F(305, 674));
  drv.CmdDL(VERTEX2F(495, 126));
  drv.CmdDL(VERTEX2F(593, 674));
  //inner rects
  drv.CmdDL(LINE_WIDTH(16));
  drv.CmdColorRGB(fill_color);
  drv.CmdDL(VERTEX2F(207, 126));
  drv.CmdDL(VERTEX2F(305, 674));
  drv.CmdDL(VERTEX2F(495, 126));
  drv.CmdDL(VERTEX2F(593, 674));
  drv.CmdDL(DL_END);
  //cleanup
  drv.CmdDL(DL_RESTORE_CONTEXT);
}

void GUIDraws::BackIconLarge(EVEDriver& drv, uint16_t x, uint16_t y, uint32_t outline_color, uint32_t fill_color, uint32_t bg_color) {
  //setup
  drv.CmdDL(DL_SAVE_CONTEXT);
  drv.CmdDL(VERTEX_TRANSLATE_X(16 * x));
  drv.CmdDL(VERTEX_TRANSLATE_Y(16 * y));
  drv.CmdScissor(x, y, 50, 50);
  //inner rect
  drv.CmdBeginDraw(EVE_RECTS);
  drv.CmdDL(LINE_WIDTH(16));
  drv.CmdColorRGB(fill_color);
  drv.CmdDL(VERTEX2F(91, 218));
  drv.CmdDL(VERTEX2F(691, 582));
  drv.CmdDL(DL_END);
  //top cutout
  drv.CmdBeginDraw(EVE_EDGE_STRIP_A);
  drv.CmdDL(LINE_WIDTH(8));
  drv.CmdColorRGB(bg_color);
  drv.CmdDL(VERTEX2F(86, 209));
  drv.CmdDL(VERTEX2F(175, 209));
  drv.CmdDL(VERTEX2F(175, 361));
  drv.CmdDL(VERTEX2F(435, 206));
  drv.CmdDL(VERTEX2F(435, 360));
  drv.CmdDL(VERTEX2F(708, 195));
  drv.CmdDL(DL_END);
  //bottom cutout
  drv.CmdBeginDraw(EVE_EDGE_STRIP_B);
  drv.CmdDL(VERTEX2F(86, 590));
  drv.CmdDL(VERTEX2F(175, 590));
  drv.CmdDL(VERTEX2F(175, 438));
  drv.CmdDL(VERTEX2F(435, 593));
  drv.CmdDL(VERTEX2F(435, 439));
  drv.CmdDL(VERTEX2F(708, 604));
  drv.CmdDL(DL_END);
  //outline
  drv.CmdBeginDraw(EVE_LINE_STRIP);
  drv.CmdDL(LINE_WIDTH(16));
  drv.CmdColorRGB(outline_color);
  drv.CmdDL(VERTEX2F(86, 209));
  drv.CmdDL(VERTEX2F(172, 209));
  drv.CmdDL(VERTEX2F(172, 361));
  drv.CmdDL(VERTEX2F(435, 206));
  drv.CmdDL(VERTEX2F(435, 360));
  drv.CmdDL(VERTEX2F(695, 203));
  drv.CmdDL(VERTEX2F(695, 596));
  drv.CmdDL(VERTEX2F(435, 439));
  drv.CmdDL(VERTEX2F(435, 593));
  drv.CmdDL(VERTEX2F(172, 438));
  drv.CmdDL(VERTEX2F(172, 590));
  drv.CmdDL(VERTEX2F(86, 590));
  drv.CmdDL(VERTEX2F(86, 209));
  drv.CmdDL(DL_END);
  //cleanup
  drv.CmdDL(DL_RESTORE_CONTEXT);
}

void GUIDraws::ForwardIconLarge(EVEDriver& drv, uint16_t x, uint16_t y, uint32_t outline_color, uint32_t fill_color, uint32_t bg_color) {
  //setup
  drv.CmdDL(DL_SAVE_CONTEXT);
  drv.CmdDL(VERTEX_TRANSLATE_X(16 * x));
  drv.CmdDL(VERTEX_TRANSLATE_Y(16 * y));
  drv.CmdScissor(x, y, 50, 50);
  //inner rect
  drv.CmdBeginDraw(EVE_RECTS);
  drv.CmdDL(LINE_WIDTH(16));
  drv.CmdColorRGB(fill_color);
  drv.CmdDL(VERTEX2F(109, 218));
  drv.CmdDL(VERTEX2F(709, 582));
  drv.CmdDL(DL_END);
  //top cutout
  drv.CmdBeginDraw(EVE_EDGE_STRIP_A);
  drv.CmdDL(LINE_WIDTH(8));
  drv.CmdColorRGB(bg_color);
  drv.CmdDL(VERTEX2F(713, 209));
  drv.CmdDL(VERTEX2F(624, 209));
  drv.CmdDL(VERTEX2F(624, 361));
  drv.CmdDL(VERTEX2F(364, 206));
  drv.CmdDL(VERTEX2F(364, 360));
  drv.CmdDL(VERTEX2F(91, 195));
  drv.CmdDL(DL_END);
  //bottom cutout
  drv.CmdBeginDraw(EVE_EDGE_STRIP_B);
  drv.CmdDL(VERTEX2F(713, 590));
  drv.CmdDL(VERTEX2F(624, 590));
  drv.CmdDL(VERTEX2F(624, 438));
  drv.CmdDL(VERTEX2F(364, 593));
  drv.CmdDL(VERTEX2F(364, 439));
  drv.CmdDL(VERTEX2F(91, 604));
  drv.CmdDL(DL_END);
  //outline
  drv.CmdBeginDraw(EVE_LINE_STRIP);
  drv.CmdDL(LINE_WIDTH(16));
  drv.CmdColorRGB(outline_color);
  drv.CmdDL(VERTEX2F(713, 209));
  drv.CmdDL(VERTEX2F(624, 209));
  drv.CmdDL(VERTEX2F(624, 361));
  drv.CmdDL(VERTEX2F(364, 206));
  drv.CmdDL(VERTEX2F(364, 360));
  drv.CmdDL(VERTEX2F(104, 203));
  drv.CmdDL(VERTEX2F(104, 596));
  drv.CmdDL(VERTEX2F(364, 439));
  drv.CmdDL(VERTEX2F(364, 593));
  drv.CmdDL(VERTEX2F(624, 438));
  drv.CmdDL(VERTEX2F(624, 590));
  drv.CmdDL(VERTEX2F(713, 590));
  drv.CmdDL(VERTEX2F(713, 209));
  drv.CmdDL(DL_END);
  //cleanup
  drv.CmdDL(DL_RESTORE_CONTEXT);
}

void GUIDraws::BWLogoLarge(EVEDriver& drv, uint16_t x, uint16_t y, uint32_t outline_color, uint32_t fill_color) {
  //setup
  drv.CmdDL(DL_SAVE_CONTEXT);
  drv.CmdDL(VERTEX_TRANSLATE_X(16 * x));
  drv.CmdDL(VERTEX_TRANSLATE_Y(16 * y));
  //inner lines
  drv.CmdBeginDraw(EVE_LINE_STRIP);
  drv.CmdDL(LINE_WIDTH(35));
  drv.CmdColorRGB(fill_color);
  drv.CmdDL(VERTEX2F(35 * 3, 35 * 6));
  drv.CmdDL(VERTEX2F(35 * 3, 35 * 16));
  drv.CmdDL(VERTEX2F(35 * 8, 35 * 16));
  drv.CmdDL(VERTEX2F(35 * 9, 35 * 15));
  drv.CmdDL(VERTEX2F(35 * 9, 35 * 12));
  drv.CmdDL(VERTEX2F(35 * 8, 35 * 11));
  drv.CmdDL(VERTEX2F(35 * 9, 35 * 10));
  drv.CmdDL(VERTEX2F(35 * 9, 35 * 7));
  drv.CmdDL(VERTEX2F(35 * 8, 35 * 6));
  drv.CmdDL(VERTEX2F(35 * 3, 35 * 6));
  drv.CmdDL(DL_END);
  drv.CmdBeginDraw(EVE_LINE_STRIP);
  drv.CmdDL(VERTEX2F(35 * 13, 35 * 6));
  drv.CmdDL(VERTEX2F(35 * 13, 35 * 14 + 17));
  drv.CmdDL(LINE_WIDTH(43));
  drv.CmdDL(VERTEX2F(35 * 14 + 16, 35 * 16));
  drv.CmdDL(VERTEX2F(35 * 17, 35 * 14));
  drv.CmdDL(VERTEX2F(35 * 19 + 19, 35 * 16));
  drv.CmdDL(VERTEX2F(35 * 21, 35 * 14 + 20));
  drv.CmdDL(LINE_WIDTH(35));
  drv.CmdDL(VERTEX2F(35 * 21, 35 * 6));
  drv.CmdDL(DL_END);
  drv.CmdBeginDraw(EVE_LINES);
  drv.CmdDL(VERTEX2F(35 * 8, 35 * 11));
  drv.CmdDL(VERTEX2F(35 * 3, 35 * 11));
  drv.CmdDL(VERTEX2F(35 * 17, 35 * 14));
  drv.CmdDL(VERTEX2F(35 * 17, 35 * 6));
  drv.CmdDL(DL_END);
  //outline
  drv.CmdBeginDraw(EVE_LINE_STRIP);
  drv.CmdDL(LINE_WIDTH(14));
  drv.CmdColorRGB(outline_color);
  drv.CmdDL(VERTEX2F(35 * 2, 35 * 5));
  drv.CmdDL(VERTEX2F(35 * 2, 35 * 17));
  drv.CmdDL(VERTEX2F(35 * 9, 35 * 17));
  drv.CmdDL(VERTEX2F(35 * 10, 35 * 16));
  drv.CmdDL(VERTEX2F(35 * 10, 35 * 12));
  drv.CmdDL(VERTEX2F(35 * 9, 35 * 11));
  drv.CmdDL(VERTEX2F(35 * 10, 35 * 10));
  drv.CmdDL(VERTEX2F(35 * 10, 35 * 6));
  drv.CmdDL(VERTEX2F(35 * 9, 35 * 5));
  drv.CmdDL(VERTEX2F(35 * 2, 35 * 5));
  drv.CmdDL(DL_END);
  drv.CmdBeginDraw(EVE_LINE_STRIP);
  drv.CmdDL(VERTEX2F(35 * 4, 35 * 7));
  drv.CmdDL(VERTEX2F(35 * 4, 35 * 10));
  drv.CmdDL(VERTEX2F(35 * 7, 35 * 10));
  drv.CmdDL(VERTEX2F(35 * 8, 35 * 9));
  drv.CmdDL(VERTEX2F(35 * 8, 35 * 8));
  drv.CmdDL(VERTEX2F(35 * 7, 35 * 7));
  drv.CmdDL(VERTEX2F(35 * 4, 35 * 7));
  drv.CmdDL(DL_END);
  drv.CmdBeginDraw(EVE_LINE_STRIP);
  drv.CmdDL(VERTEX2F(35 * 4, 35 * 12));
  drv.CmdDL(VERTEX2F(35 * 4, 35 * 15));
  drv.CmdDL(VERTEX2F(35 * 7, 35 * 15));
  drv.CmdDL(VERTEX2F(35 * 8, 35 * 14));
  drv.CmdDL(VERTEX2F(35 * 8, 35 * 13));
  drv.CmdDL(VERTEX2F(35 * 7, 35 * 12));
  drv.CmdDL(VERTEX2F(35 * 4, 35 * 12));
  drv.CmdDL(DL_END);
  drv.CmdBeginDraw(EVE_LINE_STRIP);
  drv.CmdDL(VERTEX2F(35 * 12, 35 * 6));
  drv.CmdDL(VERTEX2F(35 * 12, 35 * 16));
  drv.CmdDL(VERTEX2F(35 * 13, 35 * 17));
  drv.CmdDL(VERTEX2F(35 * 16, 35 * 17));
  drv.CmdDL(VERTEX2F(35 * 17, 35 * 16));
  drv.CmdDL(VERTEX2F(35 * 18, 35 * 17));
  drv.CmdDL(VERTEX2F(35 * 21, 35 * 17));
  drv.CmdDL(VERTEX2F(35 * 22, 35 * 16));
  drv.CmdDL(VERTEX2F(35 * 22, 35 * 6));
  drv.CmdDL(VERTEX2F(35 * 21, 35 * 5));
  drv.CmdDL(VERTEX2F(35 * 20, 35 * 5));
  drv.CmdDL(VERTEX2F(35 * 20, 35 * 13));
  drv.CmdDL(VERTEX2F(35 * 19, 35 * 14));
  drv.CmdDL(VERTEX2F(35 * 18, 35 * 13));
  drv.CmdDL(VERTEX2F(35 * 18, 35 * 6));
  drv.CmdDL(VERTEX2F(35 * 17, 35 * 5));
  drv.CmdDL(VERTEX2F(35 * 16, 35 * 6));
  drv.CmdDL(VERTEX2F(35 * 16, 35 * 13));
  drv.CmdDL(VERTEX2F(35 * 15, 35 * 14));
  drv.CmdDL(VERTEX2F(35 * 14, 35 * 13));
  drv.CmdDL(VERTEX2F(35 * 14, 35 * 5));
  drv.CmdDL(VERTEX2F(35 * 13, 35 * 5));
  drv.CmdDL(VERTEX2F(35 * 12, 35 * 6));
  drv.CmdDL(DL_END);
  //cleanup
  drv.CmdDL(DL_RESTORE_CONTEXT);
}


/**********************************************/
/*              Huge (100x100px)              */
/**********************************************/

void GUIDraws::PowerIconHuge(EVEDriver& drv, uint16_t x, uint16_t y, uint32_t outline_color, uint32_t fill_color, uint32_t bg_color) {
  //setup
  drv.CmdDL(DL_SAVE_CONTEXT);
  drv.CmdDL(VERTEX_TRANSLATE_X(16 * x));
  drv.CmdDL(VERTEX_TRANSLATE_Y(16 * y));
  //outer circle
  drv.CmdBeginDraw(EVE_POINTS);
  drv.CmdDL(POINT_SIZE(8 * 84));
  drv.CmdColorRGB(outline_color);
  drv.CmdDL(VERTEX2F(16 * 49 + 8, 16 * 54 + 8));
  //fill circle
  drv.CmdDL(POINT_SIZE(8 * 80));
  drv.CmdColorRGB(fill_color);
  drv.CmdDL(VERTEX2F(16 * 49 + 8, 16 * 54 + 8));
  //inner circle
  drv.CmdDL(POINT_SIZE(8 * 60));
  drv.CmdColorRGB(outline_color);
  drv.CmdDL(VERTEX2F(16 * 49 + 8, 16 * 54 + 8));
  drv.CmdDL(DL_END);
  //cut outline
  drv.CmdBeginDraw(EVE_LINES);
  drv.CmdDL(LINE_WIDTH(8 * 29));
  drv.CmdDL(VERTEX2F(16 * 49 + 8, 16 * 14 + 8));
  drv.CmdDL(VERTEX2F(16 * 49 + 8, 16 * 30 + 8));
  //outline cutoff
  drv.CmdDL(LINE_WIDTH(8 * 15));
  drv.CmdColorRGB(bg_color);
  drv.CmdDL(VERTEX2F(16 * 35 + 8, 16 * 7 + 6));
  drv.CmdDL(VERTEX2F(16 * 63 + 8, 16 * 7 + 6));
  drv.CmdDL(DL_END);
  //background circle
  drv.CmdBeginDraw(EVE_POINTS);
  drv.CmdDL(POINT_SIZE(8 * 56));
  drv.CmdDL(VERTEX2F(16 * 49 + 8, 16 * 54 + 8));
  drv.CmdDL(DL_END);
  //line cut
  drv.CmdBeginDraw(EVE_LINE_STRIP);
  drv.CmdDL(LINE_WIDTH(8 * 25));
  drv.CmdDL(VERTEX2F(16 * 49 + 8, 16 * 12 + 8));
  drv.CmdDL(VERTEX2F(16 * 49 + 8, 16 * 38 + 8));
  //line outline
  drv.CmdDL(LINE_WIDTH(8 * 14));
  drv.CmdColorRGB(outline_color);
  drv.CmdDL(VERTEX2F(16 * 49 + 8, 16 * 7 + 8));
  //line fill
  drv.CmdDL(LINE_WIDTH(8 * 10));
  drv.CmdColorRGB(fill_color);
  drv.CmdDL(VERTEX2F(16 * 49 + 8, 16 * 38 + 8));
  drv.CmdDL(DL_END);
  //cleanup
  drv.CmdDL(DL_RESTORE_CONTEXT);
}

