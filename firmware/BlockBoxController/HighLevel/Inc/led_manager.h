/*
 * led_manager.h
 *
 *  Created on: Sep 10, 2025
 *      Author: Alex
 */

#ifndef INC_LED_MANAGER_H_
#define INC_LED_MANAGER_H_


#include "cpp_main.h"
#include "event_source.h"
#include "storage.h"


typedef struct {
  float red;
  float green;
  float blue;
} LEDColor;


#ifdef __cplusplus


class BlockBoxV2System;


class LEDManager : public EventSource {
public:
  BlockBoxV2System& system;


  static LEDColor ColorHSVToRGB(float hue, float saturation, float value);


  LEDManager(BlockBoxV2System& system);

  void Init(SuccessCallback&& callback);
  void LoopTasks();

  //void HandlePowerStateChange(bool on);

  //LED config functions
  bool IsOn() const;
  float GetBrightness() const;
  float GetHueDegrees() const;

  void SetOn(bool on);
  void SetBrightness(float brightness);
  void SetHueDegrees(float hue_degrees);

  //TODO: sound to light?

protected:
  StorageSection non_volatile_config;

  bool initialised;
  bool led_on;


  static void LoadNonVolatileConfigDefaults(StorageSection& section);

  //void HandleEvent(EventSource* source, uint32_t event);
};


#endif


#endif /* INC_LED_MANAGER_H_ */
