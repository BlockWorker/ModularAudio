/*
 * event_source.cpp
 *
 *  Created on: Jul 19, 2025
 *      Author: Alex
 */


#include "event_source.h"
#include "system.h"


//registers the given callback with the given event mask and identifier. identifier 0 is "unidentifiable". if the given (non-zero) identifier already exists, the old callback is replaced.
void EventSource::RegisterCallback(EventCallback&& cb, uint32_t event_mask, uint64_t identifier) {
  if (!cb || event_mask == 0) {
    throw std::invalid_argument("EventSource RegisterCallback requires a non-empty function and a non-zero event mask");
  }

  //iterate through existing registrations to see if this identifier is already registered
  if (identifier != 0) {
    for (auto& reg : this->registered_callbacks) {
      if (reg.identifier == identifier) {
        //function already registered: replace old callback and event mask with new ones
        reg.func = std::move(cb);
        reg.event_mask = event_mask;
        return;
      }
    }
  }

  //new function: add registration for it
  auto& new_reg = this->registered_callbacks.emplace_back();
  new_reg.func = std::move(cb);
  new_reg.event_mask = event_mask;
  new_reg.identifier = identifier;
}

void EventSource::UnregisterCallback(uint64_t identifier) {
  if (identifier == 0) {
    throw std::invalid_argument("EventSource UnregisterCallback only works for nonzero identifiers");
  }

  //iterate through existing registrations to find the function with the given identifier
  for (auto i = this->registered_callbacks.begin(); i < this->registered_callbacks.end(); i++) {
    auto& reg = *i;
    if (reg.identifier == identifier) {
      //found: remove callback registration and return
      this->registered_callbacks.erase(i);
      return;
    }
  }
}

void EventSource::ClearCallbacks() noexcept {
  this->registered_callbacks.clear();
}


void EventSource::ExecuteCallbacks(uint32_t event) noexcept {
  //iterate through callback registrations to find all that match the given event
  for (auto i = this->registered_callbacks.begin(); i < this->registered_callbacks.end(); i++) {
    auto& reg = *i;
    if ((reg.event_mask & event) != 0) {
      //event mask matches: call function (in exception-safe way)
      try {
        reg.func(this, event);
      } catch (const std::exception& err) {
        DEBUG_LOG(DEBUG_ERROR, "EventSource callback exception: %s", err.what());
      } catch (...) {
        DEBUG_LOG(DEBUG_ERROR, "Unknown EventSource callback exception");
      }
    }
  }
}

