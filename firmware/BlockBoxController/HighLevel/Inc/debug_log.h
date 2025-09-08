/*
 * debug_log.h
 *
 *  Created on: Sep 2, 2025
 *      Author: Alex
 */

#ifndef INC_DEBUG_LOG_H_
#define INC_DEBUG_LOG_H_


#include "cpp_main.h"
#include "EVE_commands.h"
#include "rtc_interface.h"


//width in pixels to assume for the debug log window
#define DEBUG_WIN_WIDTH 310

//size of the log text array, in bytes
#define DEBUG_LOG_TEXT_SIZE (uint32_t)131072
//size of the log item array, in entries
#define DEBUG_LOG_ENTRIES_SIZE (uint32_t)2048


typedef enum {
  DEBUG_CRITICAL = 0,
  DEBUG_ERROR = 1,
  DEBUG_WARNING = 2,
  DEBUG_INFO = 3
} DebugLevel;

typedef struct {
  DebugLevel level;
  const char* text;
  uint16_t short_length;
  uint16_t line_count = 0;
  uint16_t line_starts[3];
  uint16_t line_lengths[3];
} DebugScreenMessage;

typedef struct {
  DebugLevel level;
  const char* text;
} DebugLogEntry;


#ifdef __cplusplus

#include <deque>


class DebugLog {
public:
  //derived maximum pixel widths of debug lines
  static const uint16_t max_firstline_width;
  static const uint16_t max_short_width;
  static const uint16_t max_extraline_width;

  //debug message creation function
  static constexpr DebugScreenMessage PrepareMessage(const DebugLogEntry& entry) {
    DebugScreenMessage msg = {};
    msg.level = entry.level;
    msg.text = entry.text;
    uint16_t total_len = strlen(msg.text);
    if (EVEDriver::GetTextWidth(20, msg.text) <= DebugLog::max_firstline_width) {
      msg.short_length = 0;
    } else {
      msg.short_length = EVEDriver::GetMaxFittingTextLength(20, DebugLog::max_short_width, msg.text);
    }
    uint16_t line_start = 0;
    uint16_t max_width = DebugLog::max_firstline_width;
    for (int i = 0; i < 3; i++) {
      if (line_start >= total_len) {
        msg.line_starts[i] = 0;
        msg.line_lengths[i] = 0;
      } else {
        msg.line_starts[i] = line_start;
        uint16_t length = EVEDriver::GetMaxFittingTextLengthWords(20, max_width, msg.text + line_start);
        msg.line_lengths[i] = length;
        line_start += length + 1;
        max_width = DebugLog::max_extraline_width;
        msg.line_count++;
      }
    }
    return msg;
  }

  static DebugLog instance;


  uint32_t GetEntryMinValid() const noexcept;
  uint32_t GetEntryCount() const noexcept;
  const DebugLogEntry& GetEntry(uint32_t index) const noexcept;

  void LogEntry(DebugLevel level, const RTCDateTime& datetime, const char* fmt, ...) noexcept __attribute__((__format__(__printf__, 4, 5)));

  void SetEnabled(bool enabled) noexcept;

private:
  bool enabled;

  char log_text[DEBUG_LOG_TEXT_SIZE];
  DebugLogEntry log_entries[DEBUG_LOG_ENTRIES_SIZE];

  uint32_t text_start_offset;
  uint32_t text_write_offset;

  uint32_t entry_start_offset;
  uint32_t entry_write_offset;
  uint32_t entry_valid_offset;


  DebugLog();
  DebugLog(const DebugLog&) = delete;
};


#endif


#endif /* INC_DEBUG_LOG_H_ */
