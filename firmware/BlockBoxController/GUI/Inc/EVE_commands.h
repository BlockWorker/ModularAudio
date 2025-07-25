/*
@file    EVE_commands.h
@brief   contains FT8xx / BT8xx function prototypes
@version 5.0
@date    2023-12-29
@author  Rudolph Riedel

@section LICENSE

MIT License

Copyright (c) 2016-2023 Rudolph Riedel

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the Software
is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

@section History

5.0
- added prototype for EVE_cmd_plkfreq()
- replaced BT81X_ENABLE with "EVE_GEN > 2"
- removed FT81X_ENABLE as FT81x already is the lowest supported chip revision now
- removed the formerly as deprected marked EVE_get_touch_tag()
- changed EVE_color_rgb() to use a 32 bit value like the rest of the color commands
- removed the meta-commands EVE_cmd_point(), EVE_cmd_line() and EVE_cmd_rect()
- removed obsolete functions EVE_get_cmdoffset(void) and EVE_report_cmdoffset(void) - cmdoffset is gone
- renamed EVE_LIB_GetProps() back to EVE_cmd_getprops() since it does not do anything special to justify a special name
- added prototype for helper function EVE_memWrite_sram_buffer()
- added prototypes for EVE_cmd_bitmap_transform() and EVE_cmd_bitmap_transform_burst()
- added prototype for EVE_cmd_playvideo()
- added prototypes for EVE_cmd_setfont_burst() and EVE_cmd_setfont2_burst()
- added prototype for EVE_cmd_videoframe()
- restructured: functions are sorted by chip-generation and within their group in alphabetical order
- reimplementedEVE_cmd_getmatrix() again, it needs to read values, not write them
- added prototypes for EVE_cmd_fontcache() and EVE_cmd_fontcachequery()
- added prototype for EVE_cmd_flashprogram()
- added prototype for EVE_cmd_calibratesub()
- added prototypes for EVE_cmd_animframeram(), EVE_cmd_animframeram_burst(), EVE_cmd_animstartram(),
EVE_cmd_animstartram_burst()
- added prototypes for EVE_cmd_apilevel(), EVE_cmd_apilevel_burst()
- added prototypes for EVE_cmd_calllist(), EVE_cmd_calllist_burst()
- added prototype for EVE_cmd_getimage()
- added prototypes for EVE_cmd_hsf(), EVE_cmd_hsf_burst()
- added prototype for EVE_cmd_linetime()
- added prototypes for EVE_cmd_newlist(), EVE_cmd_newlist_burst()
- added prototypes for EVE_cmd_runanim(), EVE_cmd_runanim_burst()
- added prototype for EVE_cmd_wait()
- removed the history from before 4.0
- added an enum with return codes to have the functions return something more meaningfull
- finally removed EVE_cmd_start() after setting it to deprecatd with the first 5.0 release
- renamed EVE_cmd_execute() to EVE_execute_cmd() to be more consistent, this is is not an EVE command
- added the return-value of EVE_FIFO_HALF_EMPTY to EVE_busy() to indicate there is more than 2048 bytes available
- removed the 4.0 history
- added parameter width to EVE_calibrate_manual()
- changed the varargs versions of cmd_button, cmd_text and cmd_toggle to use an array of uint32_t values to comply with MISRA-C
- fixed some MISRA-C issues
- basic maintenance: checked for violations of white space and indent rules
- more linter fixes for minor issues like variables shorter than 3 characters
- added EVE_color_a() / EVE_color_a_burst()
- removed EVE_cmd_newlist_burst() prototype as the function got removed earlier
- added prototype for EVE_write_display_parameters()
- added EVE_memRead_sram_buffer()
- added EVE_FAULT_RECOVERED to the list of return codes
- added defines for the state of the external flash
- added protype for EVE_get_and_reset_fault_state()
- put E_OK and E_NOT_OK in #ifndef/#endif guards as these are usually defined
  already in AUTOSAR projects
- renamed EVE_FAIL_CHIPID_TIMEOUT to EVE_FAIL_REGID_TIMEOUT as suggested by #93 on github
- changed a number of function parameters from signed to unsigned following the
    updated BT81x series programming guide V2.4
- commented out EVE_cmd_regread() prototype
- removed prototype for EVE_cmd_hsf_burst()

*/

#ifndef EVE_COMMANDS_H
#define EVE_COMMANDS_H

#include "EVE.h"

#ifdef __cplusplus

#include <vector>

extern "C"
{
#endif

#if !defined E_OK
#define E_OK 0U
#endif

#if !defined E_NOT_OK
#define E_NOT_OK 1U
#endif

#define EVE_FAIL_REGID_TIMEOUT 2U
#define EVE_FAIL_RESET_TIMEOUT 3U
#define EVE_FAIL_PCLK_FREQ 4U
#define EVE_FAIL_FLASH_STATUS_INIT 5U
#define EVE_FAIL_FLASH_STATUS_DETACHED 6U
#define EVE_FAIL_FLASHFAST_NOT_SUPPORTED 7U
#define EVE_FAIL_FLASHFAST_NO_HEADER_DETECTED 8U
#define EVE_FAIL_FLASHFAST_SECTOR0_FAILED 9U
#define EVE_FAIL_FLASHFAST_BLOB_MISMATCH 10U
#define EVE_FAIL_FLASHFAST_SPEED_TEST 11U
#define EVE_IS_BUSY 12U
#define EVE_FIFO_HALF_EMPTY 13U
#define EVE_FAULT_RECOVERED 14U

#define EVE_FLASH_STATUS_INIT 0U
#define EVE_FLASH_STATUS_DETACHED 1U
#define EVE_FLASH_STATUS_BASIC 2U
#define EVE_FLASH_STATUS_FULL 3U

#define EVE_DLBUFFER_DEFAULT_SIZE 128U

#ifdef __cplusplus
}


extern const uint8_t eve_font_char_widths[15][96];


class EVEDriver {
public:
  EVETargetPHY phy;

  EVEDriver();

/* ##################################################################
    helper functions
##################################################################### */

  uint8_t IsBusy();
  void WaitUntilNotBusy(uint32_t timeout);
  uint8_t GetAndResetFaultState() noexcept;

  void ClearDLCmdBuffer() noexcept;
  void SendBufferedDLCmds(uint32_t timeout);

  void SaveBufferedDLCmds(std::vector<uint32_t>& target);
  void SendSavedDLCmds(const std::vector<uint32_t>& source, uint32_t timeout);
  uint32_t GetDLBufferSize() noexcept;

/* ##################################################################
    commands and functions to be used outside of display-lists
##################################################################### */

  void CmdGetProps(uint32_t *p_pointer, uint32_t *p_width, uint32_t *p_height);
  uint32_t CmdGetPtr();
  void CmdInflate(uint32_t ptr, const uint8_t *p_data, uint32_t len);
  void CmdInterrupt(uint32_t msec);
  void CmdLoadImage(uint32_t ptr, uint32_t options, const uint8_t *p_data, uint32_t len);
  void CmdMediaFifo(uint32_t ptr, uint32_t size);
  void CmdMemcpy(uint32_t dest, uint32_t src, uint32_t num);
  uint32_t CmdMemcrc(uint32_t ptr, uint32_t num);
  void CmdMemset(uint32_t ptr, uint8_t value, uint32_t num);
  void CmdMemzero(uint32_t ptr, uint32_t num);
  void CmdPlayVideo(uint32_t options, const uint8_t *p_data, uint32_t len);
  void CmdSetRotate(uint32_t rotation);
  void CmdSnapshot(uint32_t ptr);
  void CmdSnapshot2(uint32_t fmt, uint32_t ptr, int16_t xc0, int16_t yc0, uint16_t wid, uint16_t hgt);
  //void CmdTrack(int16_t xc0, int16_t yc0, uint16_t wid, uint16_t hgt, uint16_t tag);
  void CmdVideoFrame(uint32_t dest, uint32_t result_ptr);

/* ##################################################################
    patching and initialization
##################################################################### */

  void WriteDisplayParameters();
  uint8_t Init();
  void SetTransferMode(EVETransferMode mode);
  EVETransferMode GetTransferMode() noexcept;

/* ##################################################################
    functions for display lists
##################################################################### */

  void CmdDL(uint32_t command);

  void CmdAppend(uint32_t ptr, uint32_t num);
  void CmdBeginDraw(uint32_t primitive);
  void CmdBGColor(uint32_t color);
  void CmdButton(int16_t xc0, int16_t yc0, uint16_t wid, uint16_t hgt, uint16_t font, uint16_t options, const char *p_text);
  void CmdCalibrate();
  void CmdClock(int16_t xc0, int16_t yc0, uint16_t rad, uint16_t options, uint16_t hours, uint16_t mins, uint16_t secs, uint16_t msecs);
  void CmdDial(int16_t xc0, int16_t yc0, uint16_t rad, uint16_t options, uint16_t val);
  void CmdFGColor(uint32_t color);
  void CmdGauge(int16_t xc0, int16_t yc0, uint16_t rad, uint16_t options, uint16_t major, uint16_t minor, uint16_t val, uint16_t range);
  //void CmdGetMatrix(int32_t *p_a, int32_t *p_b, int32_t *p_c, int32_t *p_d, int32_t *p_e, int32_t *p_f);
  void CmdGradColor(uint32_t color);
  void CmdGradient(int16_t xc0, int16_t yc0, uint32_t rgb0, int16_t xc1, int16_t yc1, uint32_t rgb1);
  void CmdKeys(int16_t xc0, int16_t yc0, uint16_t wid, uint16_t hgt, uint16_t font, uint16_t options, const char *p_text);
  void CmdNumber(int16_t xc0, int16_t yc0, uint16_t font, uint16_t options, int32_t number);
  void CmdProgress(int16_t xc0, int16_t yc0, uint16_t wid, uint16_t hgt, uint16_t options, uint16_t val, uint16_t range);
  void CmdRomFont(uint32_t font, uint32_t romslot);
  void CmdRotate(uint32_t angle);
  void CmdScale(int32_t scx, int32_t scy);
  void CmdScrollBar(int16_t xc0, int16_t yc0, uint16_t wid, uint16_t hgt, uint16_t options, uint16_t val, uint16_t size, uint16_t range);
  void CmdSetBase(uint32_t base);
  void CmdSetBitmap(uint32_t addr, uint16_t fmt, uint16_t width, uint16_t height);
  void CmdSetFont(uint32_t font, uint32_t ptr);
  void CmdSetFont2(uint32_t font, uint32_t ptr, uint32_t firstchar);
  void CmdSetScratch(uint32_t handle);
  void CmdSketch(int16_t xc0, int16_t yc0, uint16_t wid, uint16_t hgt, uint32_t ptr, uint16_t format);
  void CmdSlider(int16_t xc0, int16_t yc0, uint16_t wid, uint16_t hgt, uint16_t options, uint16_t val, uint16_t range);
  void CmdSpinner(int16_t xc0, int16_t yc0, uint16_t style, uint16_t scale);
  void CmdText(int16_t xc0, int16_t yc0, uint16_t font, uint16_t options, const char *p_text);
  void CmdToggle(int16_t xc0, int16_t yc0, uint16_t wid, uint16_t font, uint16_t options, uint16_t state, const char *p_text);
  void CmdTrack(int16_t xc0, int16_t yc0, uint16_t wid, uint16_t hgt, uint16_t tag);
  void CmdTranslate(int32_t tr_x, int32_t tr_y);

  void CmdColorRGB(uint32_t color);
  void CmdColorA(uint8_t alpha);

/* ##################################################################
    special purpose functions
##################################################################### */

  //HAL_StatusTypeDef CalibrateManual(uint16_t width, uint16_t height);

/* ##################################################################
    meta-commands (commonly used sequences of display-list entries)
##################################################################### */

  void CmdBeginDisplay(bool color, bool stencil, bool tag, uint32_t clear_color);
  void CmdBeginDisplayLimited(bool color, bool stencil, bool tag, uint32_t clear_color, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
  void CmdPoint(int16_t x0, int16_t y0, uint16_t size);
  void CmdLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t width);
  void CmdRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t corner);
  void CmdEndDisplay();
  void CmdTag(uint8_t tag);
  void CmdTagMask(bool enable_tag);
  void CmdScissor(uint16_t x, uint16_t y, uint16_t w, uint16_t h);

/* ##################################################################
    text width helper functions
##################################################################### */

  //calculates the width of the given text in pixels, using the given font
  static constexpr uint16_t GetTextWidth(uint8_t font, const char* text) {
    if (font < 16 || font > 34 || text == NULL) {
      throw std::invalid_argument("EVEDriver GetTextWidth given invalid font or null pointer");
    }

    if (font < 20) {
      //constant-width fonts 16-19 (8px per character): easy case
      return 8 * strlen(text);
    }

    //non-constant-width font: calculate from corresponding character widths
    const uint8_t* char_widths = eve_font_char_widths[font - 20];
    uint16_t width = 0;
    //iterate through characters until null termination is hit
    const char* c_ptr = text;
    while (*c_ptr != 0) {
      char c = *c_ptr;
      //only ASCII 32-127 are printable
      if (c >= 32 && c < 128) {
        width += char_widths[c - 32];
      }
      c_ptr++;
    }
    return width;
  }

  //calculates the maximum number of characters of the given text (from the start) would fit in the given maximum width (in pixels)
  static constexpr uint16_t GetMaxFittingTextLength(uint8_t font, uint16_t max_width, const char* text) {
    if (font < 16 || font > 34 || text == NULL) {
      throw std::invalid_argument("EVEDriver GetMaxFittingLength given invalid font or null pointer");
    }

    if (font < 20) {
      //constant-width fonts 16-19 (8px per character): easy case
      uint16_t total_len = strlen(text);
      return MIN(total_len, max_width / 8);
    }

    //non-constant-width font: calculate from corresponding character widths
    const uint8_t* char_widths = eve_font_char_widths[font - 20];
    uint16_t width = 0;
    uint16_t count = 0;
    //iterate through characters until null termination or max width is hit
    const char* c_ptr = text;
    while (*c_ptr != 0) {
      char c = *c_ptr;
      //only ASCII 32-127 are printable
      if (c >= 32 && c < 128) {
        width += char_widths[c - 32];
      }
      if (width > max_width) {
        return count;
      }
      c_ptr++;
      count++;
    }
    return count;
  }

private:
  uint8_t fault_recovered;
  std::vector<uint32_t> dl_cmd_buffer;

  void CoprocessorFaultRecover();
  void SendCmdBlockTransfer(const uint8_t* p_data, uint32_t length, uint32_t timeout);
  uint8_t WaitRegID();
  uint8_t WaitReset();
  void WriteString(const char *p_text);

};

#endif /* __cplusplus */

/* ##################################################################
    old prototypes of EVE>2 functions
##################################################################### */

/* EVE4: BT817 / BT818 */
#if EVE_GEN > 3

void EVE_cmd_flashprogram(uint32_t dest, uint32_t src, uint32_t num);
void EVE_cmd_fontcache(uint32_t font, uint32_t ptr, uint32_t num);
void EVE_cmd_fontcachequery(uint32_t *p_total, uint32_t *p_used);
void EVE_cmd_getimage(uint32_t *p_source, uint32_t *p_fmt, uint32_t *p_width, uint32_t *p_height, uint32_t *p_palette);
void EVE_cmd_linetime(uint32_t dest);
void EVE_cmd_newlist(uint32_t adr);
uint32_t EVE_cmd_pclkfreq(uint32_t ftarget, int32_t rounding);
void EVE_cmd_wait(uint32_t usec);

#endif /* EVE_GEN > 3 */

/* EVE3: BT815 / BT816 */
#if EVE_GEN > 2

void EVE_cmd_clearcache(void);
void EVE_cmd_flashattach(void);
void EVE_cmd_flashdetach(void);
void EVE_cmd_flasherase(void);
uint32_t EVE_cmd_flashfast(void);
void EVE_cmd_flashspidesel(void);
void EVE_cmd_flashread(uint32_t dest, uint32_t src, uint32_t num);
void EVE_cmd_flashsource(uint32_t ptr);
void EVE_cmd_flashspirx(uint32_t dest, uint32_t num);
void EVE_cmd_flashspitx(uint32_t num, const uint8_t *p_data);
void EVE_cmd_flashupdate(uint32_t dest, uint32_t src, uint32_t num);
void EVE_cmd_flashwrite(uint32_t ptr, uint32_t num, const uint8_t *p_data);
void EVE_cmd_inflate2(uint32_t ptr, uint32_t options, const uint8_t *p_data, uint32_t len);

#endif /* EVE_GEN > 2 */

#if EVE_GEN > 2
uint8_t EVE_init_flash(void);
#endif /* EVE_GEN > 2 */

/* EVE4: BT817 / BT818 */
#if EVE_GEN > 3

void EVE_cmd_animframeram(int16_t xc0, int16_t yc0, uint32_t aoptr, uint32_t frame);
void EVE_cmd_animframeram_burst(int16_t xc0, int16_t yc0, uint32_t aoptr, uint32_t frame);
void EVE_cmd_animstartram(int32_t chnl, uint32_t aoptr, uint32_t loop);
void EVE_cmd_animstartram_burst(int32_t chnl, uint32_t aoptr, uint32_t loop);
void EVE_cmd_apilevel(uint32_t level);
void EVE_cmd_apilevel_burst(uint32_t level);
void EVE_cmd_calibratesub(uint16_t xc0, uint16_t yc0, uint16_t width, uint16_t height);
void EVE_cmd_calllist(uint32_t adr);
void EVE_cmd_calllist_burst(uint32_t adr);
void EVE_cmd_hsf(uint32_t hsf);
void EVE_cmd_runanim(uint32_t waitmask, uint32_t play);
void EVE_cmd_runanim_burst(uint32_t waitmask, uint32_t play);

#endif /* EVE_GEN > 3 */

/* EVE3: BT815 / BT816 */
#if EVE_GEN > 2

void EVE_cmd_animdraw(int32_t chnl);
void EVE_cmd_animdraw_burst(int32_t chnl);
void EVE_cmd_animframe(int16_t xc0, int16_t yc0, uint32_t aoptr, uint32_t frame);
void EVE_cmd_animframe_burst(int16_t xc0, int16_t yc0, uint32_t aoptr, uint32_t frame);
void EVE_cmd_animstart(int32_t chnl, uint32_t aoptr, uint32_t loop);
void EVE_cmd_animstart_burst(int32_t chnl, uint32_t aoptr, uint32_t loop);
void EVE_cmd_animstop(int32_t chnl);
void EVE_cmd_animstop_burst(int32_t chnl);
void EVE_cmd_animxy(int32_t chnl, int16_t xc0, int16_t yc0);
void EVE_cmd_animxy_burst(int32_t chnl, int16_t xc0, int16_t yc0);
void EVE_cmd_appendf(uint32_t ptr, uint32_t num);
void EVE_cmd_appendf_burst(uint32_t ptr, uint32_t num);
uint16_t EVE_cmd_bitmap_transform(int32_t xc0, int32_t yc0, int32_t xc1, int32_t yc1, int32_t xc2, int32_t yc2, int32_t tx0,
                                  int32_t ty0, int32_t tx1, int32_t ty1, int32_t tx2, int32_t ty2);
void EVE_cmd_bitmap_transform_burst(int32_t xc0, int32_t yc0, int32_t xc1, int32_t yc1, int32_t xc2, int32_t yc2, int32_t tx0,
                                    int32_t ty0, int32_t tx1, int32_t ty1, int32_t tx2, int32_t ty2);
void EVE_cmd_fillwidth(uint32_t pixel);
void EVE_cmd_fillwidth_burst(uint32_t pixel);
void EVE_cmd_gradienta(int16_t xc0, int16_t yc0, uint32_t argb0, int16_t xc1, int16_t yc1, uint32_t argb1);
void EVE_cmd_gradienta_burst(int16_t xc0, int16_t yc0, uint32_t argb0, int16_t xc1, int16_t yc1, uint32_t argb1);
void EVE_cmd_rotatearound(int32_t xc0, int32_t yc0, uint32_t angle, int32_t scale);
void EVE_cmd_rotatearound_burst(int32_t xc0, int32_t yc0, uint32_t angle, int32_t scale);

void EVE_cmd_button_var(int16_t xc0, int16_t yc0, uint16_t wid, uint16_t hgt, uint16_t font, uint16_t options, const char *p_text, uint8_t num_args, const uint32_t p_arguments[]);
void EVE_cmd_button_var_burst(int16_t xc0, int16_t yc0, uint16_t wid, uint16_t hgt, uint16_t font, uint16_t options, const char *p_text, uint8_t num_args, const uint32_t p_arguments[]);
void EVE_cmd_text_var(int16_t xc0, int16_t yc0, uint16_t font, uint16_t options, const char *p_text, uint8_t num_args, const uint32_t p_arguments[]);
void EVE_cmd_text_var_burst(int16_t xc0, int16_t yc0, uint16_t font, uint16_t options, const char *p_text, uint8_t num_args, const uint32_t p_arguments[]);
void EVE_cmd_toggle_var(int16_t xc0, int16_t yc0, uint16_t wid, uint16_t font, uint16_t options, uint16_t state, const char *p_text, uint8_t num_args, const uint32_t p_arguments[]);
void EVE_cmd_toggle_var_burst(int16_t xc0, int16_t yc0, uint16_t wid, uint16_t font, uint16_t options, uint16_t state, const char *p_text, uint8_t num_args, const uint32_t p_arguments[]);

#endif /* EVE_GEN > 2 */

#endif /* EVE_COMMANDS_H */
