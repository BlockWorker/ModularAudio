/*
 * gui_manager.h
 *
 *  Created on: Feb 27, 2025
 *      Author: Alex
 */

#ifndef INC_GUI_MANAGER_H_
#define INC_GUI_MANAGER_H_


#include "EVE.h"
#include <deque>


//time of touch long-press, in milliseconds
#define GUI_TOUCH_LONG_DELAY 1000
//period of long-press touch "ticks", in milliseconds
#define GUI_TOUCH_TICK_PERIOD 300

//debounce time for touches, in milliseconds
#define GUI_TOUCH_DEBOUNCE_DELAY 20


#ifdef __cplusplus


typedef struct {
  const uint32_t* data;
  uint32_t length_words;
} GUICMDTransfer;

typedef struct {
  bool touched;
  bool initial;
  bool long_press;
  bool long_press_tick;
  bool released;

  uint8_t initial_tag;
  uint8_t tag;
  uint16_t tracker_value;

  uint32_t _next_tick_at;
  uint32_t _debounce_tick;
} GUITouchState;


class GUIScreen;


class GUIManager {
public:
  EVEDriver& driver;

  GUIManager(EVEDriver& driver) noexcept;

  virtual void Init();
  virtual void Update() noexcept;

  const GUITouchState& GetTouchState() const noexcept;

  void SetScreen(GUIScreen* screen);

  void SendCmdTransferWhenNotBusy(const uint32_t* data, uint32_t length_words);

  uint8_t GetDisplayBrightness() const noexcept;
  virtual void SetDisplayBrightness(uint8_t brightness) noexcept;

  uint32_t GetDisplaySleepTimeoutMS() const noexcept;
  bool IsDisplayAsleep() const noexcept;
  virtual void SetDisplaySleepTimeoutMS(uint32_t timeout_ms) noexcept;

  bool IsDisplayForceWake() const noexcept;
  void SetDisplayForceWake(bool force_wake) noexcept;

protected:
  bool initialised;
  GUIScreen* current_screen;
  GUITouchState touch_state;

  bool cmd_busy_waiting;
  std::deque<GUICMDTransfer> queued_cmd_transfers;

  uint8_t display_brightness;

  bool display_sleep;
  uint8_t fade_brightness;
  bool touch_sleep_locked;
  uint32_t display_sleep_timeout_ms;
  uint32_t last_touched_tick;
  bool display_force_wake;
  bool display_force_wake_internal;

  virtual void InitTouchCalibration();

  void ForceScreenRedraw() noexcept;

};


#endif


#endif /* INC_GUI_MANAGER_H_ */
