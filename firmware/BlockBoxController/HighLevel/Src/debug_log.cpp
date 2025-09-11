/*
 * debug_log.cpp
 *
 *  Created on: Sep 2, 2025
 *      Author: Alex
 */


#include "debug_log.h"
#include <cstdarg>


const uint16_t DebugLog::max_firstline_width = DEBUG_WIN_WIDTH - 2;
const uint16_t DebugLog::max_short_width = DebugLog::max_firstline_width - EVEDriver::GetTextWidth(20, "...");
const uint16_t DebugLog::max_extraline_width = DEBUG_WIN_WIDTH - 2 - 8;

DebugLog DebugLog::instance;


//debug level strings for UART printouts
static const char* const _debug_level_strings[] = { "*** (C)", "** (E)", "* (W)", "(I)" };


uint32_t DebugLog::GetEntryMinValid() const noexcept {
  return this->entry_valid_offset;
}

uint32_t DebugLog::GetEntryCount() const noexcept {
  return this->entry_valid_offset + (this->entry_write_offset - this->entry_start_offset) % DEBUG_LOG_ENTRIES_SIZE;
}

const DebugLogEntry& DebugLog::GetEntry(uint32_t index) const noexcept {
  return this->log_entries[index % DEBUG_LOG_ENTRIES_SIZE];
}


void DebugLog::LogEntry(DebugLevel level, const RTCDateTime& datetime, const char* fmt, ...) noexcept {
  if (fmt == NULL || fmt[0] == 0 || level > DEBUG_INFO) {
    return;
  }

  va_list vargs;
  va_start(vargs, fmt);

  char fmt_buffer[256];
  char entry_buffer[512];
  uint32_t length;

  if (!this->enabled) {
    //logging disabled: just print to UART simply and return
#ifdef DEBUG
    snprintf(fmt_buffer, 256, "%s %s\n", _debug_level_strings[level], fmt);
    vprintf(fmt_buffer, vargs);
#endif
    va_end(vargs);
    return;
  }

  //prepare message
  snprintf(fmt_buffer, 256, "%.2s %02u:%02u:%02u %s", RTCInterface::GetWeekdayName(datetime.weekday), datetime.hours, datetime.minutes, datetime.seconds, fmt);
  int32_t print_chars = vsnprintf(entry_buffer, 512, fmt_buffer, vargs);
  if (print_chars < 0) {
#ifdef DEBUG
    snprintf(fmt_buffer, 256, "* Debug log error: %s %s\n", _debug_level_strings[level], fmt);
    vprintf(fmt_buffer, vargs);
#endif
    va_end(vargs);
    return;
  } else if (print_chars >= 512) {
    length = 512;
  } else {
    length = print_chars + 1;
  }
  va_end(vargs);

  //log to console
  DEBUG_PRINTF("%s %s\n", _debug_level_strings[level], entry_buffer);

  //get and update log array pointers (under disabled interrupts)
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  //array pointers and space
  uint32_t text_offset = this->text_write_offset;
  uint32_t text_space = DEBUG_LOG_TEXT_SIZE - (text_offset - this->text_start_offset) % DEBUG_LOG_TEXT_SIZE;
  uint32_t entry_offset = this->entry_write_offset;
  uint32_t entry_space = DEBUG_LOG_ENTRIES_SIZE - (entry_offset - this->entry_start_offset) % DEBUG_LOG_ENTRIES_SIZE;

  //check if string fits contiguously in text array - if not, wrap around to beginning preemptively
  if (text_offset + length > DEBUG_LOG_TEXT_SIZE) {
    text_offset = 0;
    //account for lost space due to wrap-around
    uint32_t space_lost = DEBUG_LOG_TEXT_SIZE - this->text_write_offset;
    if (text_space < space_lost) {
      text_space = 0;
    } else {
      text_space -= space_lost;
    }
  }

  //check for space
  if (text_space <= length) {
    //no space in text array: remove entries until a quarter of the array is clear
    while ((DEBUG_LOG_TEXT_SIZE - (this->text_write_offset - this->text_start_offset) % DEBUG_LOG_TEXT_SIZE) < DEBUG_LOG_TEXT_SIZE / 4) {
      //free up space until enough free
      this->entry_start_offset = (this->entry_start_offset + 1) % DEBUG_LOG_ENTRIES_SIZE;
      this->entry_valid_offset++;
      auto& start_entry = this->log_entries[this->entry_start_offset];
      if (start_entry.text == NULL) {
        DEBUG_PRINTF("* Debug log cleanup found empty starting entry, shouldn't happen\n");
        break;
      }

      this->text_start_offset = (uint32_t)(start_entry.text - this->log_text);
    }
  } else if (entry_space <= 1) {
    //no space in entry array: clear some space by removing the oldest quarter of log entries
    this->entry_start_offset = (this->entry_start_offset + DEBUG_LOG_ENTRIES_SIZE / 4) % DEBUG_LOG_ENTRIES_SIZE;
    this->entry_valid_offset += DEBUG_LOG_ENTRIES_SIZE / 4;
    //free corresponding space in the text array too (no longer needed messages)
    auto& start_entry = this->log_entries[this->entry_start_offset];
    if (start_entry.text != NULL) {
      this->text_start_offset = (uint32_t)(start_entry.text - this->log_text);
    } else {
      DEBUG_PRINTF("* Debug log cleanup found empty starting entry, shouldn't happen - resetting to array start\n");
      this->text_start_offset = 0;
    }
  }

  this->text_write_offset = (text_offset + length) % DEBUG_LOG_TEXT_SIZE;
  this->entry_write_offset = (entry_offset + 1) % DEBUG_LOG_ENTRIES_SIZE;
  __set_PRIMASK(primask);

  //write message to arrays
  memcpy(this->log_text + text_offset, entry_buffer, length);
  auto& entry = this->log_entries[entry_offset];
  entry.level = level;
  entry.text = this->log_text + text_offset;
}


void DebugLog::SetEnabled(bool enabled) noexcept {
  this->enabled = enabled;
}



DebugLog::DebugLog() : enabled(false), text_start_offset(0), text_write_offset(0), entry_start_offset(0), entry_write_offset(0), entry_valid_offset(0) {
  for (uint32_t i = 0; i < DEBUG_LOG_ENTRIES_SIZE; i++) {
    auto& entry = this->log_entries[i];
    entry.level = DEBUG_INFO;
    entry.text = NULL;
  }
}
