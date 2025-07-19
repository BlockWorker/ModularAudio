/*
 * event_source.h
 *
 *  Created on: Jul 19, 2025
 *      Author: Alex
 */

#ifndef INC_EVENT_SOURCE_H_
#define INC_EVENT_SOURCE_H_


#include "cpp_main.h"


#ifdef __cplusplus

#include <vector>

extern "C" {
#endif


#ifdef __cplusplus
}


class EventSource;

//callback type for events - arguments: reference to event source, event type
typedef std::function<void(EventSource&, uint32_t)> EventCallback;


typedef struct {
  EventCallback func;
  uint32_t event_mask;
  uint64_t identifier;
} EventCallbackRegistration;


class EventSource {
public:
  void RegisterCallback(EventCallback&& cb, uint32_t event_mask, uint64_t identifier = 0);
  void UnregisterCallback(uint64_t identifier);
  void ClearCallbacks() noexcept;

protected:
  std::vector<EventCallbackRegistration> registered_callbacks;

  void ExecuteCallbacks(uint32_t event) noexcept;
};


#endif


#endif /* INC_EVENT_SOURCE_H_ */
