/*
 * led_manager.cpp
 *
 *  Created on: Sep 10, 2025
 *      Author: Alex
 */


#include "led_manager.h"
#include "system.h"


//non-volatile storage layout
//brightness, float (4B)
#define LED_NVM_BRIGHTNESS 0
//hue degrees, float (4B)
#define LED_NVM_HUE 4
//total storage size in bytes
#define LED_NVM_TOTAL_BYTES 8

//config default values
#define LED_BRIGHTNESS_DEFAULT 0.125f
#define LED_HUE_DEFAULT 220.0f

//hue shift amplitude, in degrees
#define LED_HUE_SHIFT_AMPLITUDE 20.0f
//hue shift period, in milliseconds
#define LED_HUE_SHIFT_PERIOD 10000

//RGB LED PWM timer and channels
#define LED_TIMER htim4
#define LED_RED_CH TIM_CHANNEL_1
#define LED_GREEN_CH TIM_CHANNEL_2
#define LED_BLUE_CH TIM_CHANNEL_3
#define LED_RED_REG LED_TIMER.Instance->CCR1
#define LED_GREEN_REG LED_TIMER.Instance->CCR2
#define LED_BLUEN_REG LED_TIMER.Instance->CCR3
#define LED_BLUEP_REG LED_TIMER.Instance->CCR4

//PWM maximum duty cycle register value
#define LED_PWM_MAX_VALUE UINT16_MAX


LEDColor LEDManager::ColorHSVToRGB(float hue, float saturation, float value) {
  float chroma = value * saturation;
  float hp = hue / 60.0f;
  float x = chroma * (1.0f - fabsf(fmodf(hp, 2.0f) - 1.0f));

  float r1 = 0.0f, g1 = 0.0f, b1 = 0.0f;
  if (hp < 1) {
      r1 = chroma;
      g1 = x;
  } else if (hp < 2) {
      r1 = x;
      g1 = chroma;
  } else if (hp < 3) {
      g1 = chroma;
      b1 = x;
  } else if (hp < 4) {
      g1 = x;
      b1 = chroma;
  } else if (hp < 5) {
      b1 = chroma;
      r1 = x;
  } else {
      b1 = x;
      r1 = chroma;
  }

  float m = value - chroma;

  float r = r1 + m;
  if (r < 0.0f) {
    r = 0.0f;
  } else if (r > 1.0f) {
    r = 1.0f;
  }

  float g = g1 + m;
  if (g < 0.0f) {
    g = 0.0f;
  } else if (g > 1.0f) {
    g = 1.0f;
  }

  float b = b1 + m;
  if (b < 0.0f) {
    b = 0.0f;
  } else if (b > 1.0f) {
    b = 1.0f;
  }

  LEDColor color;
  color.red = r;
  color.green = g;
  color.blue = b;

  return color;
}


/******************************************************/
/*                   Initialisation                   */
/******************************************************/

LEDManager::LEDManager(BlockBoxV2System& system) :
    system(system), non_volatile_config(system.eeprom_if, LED_NVM_TOTAL_BYTES, LEDManager::LoadNonVolatileConfigDefaults), initialised(false), led_on(false) {}


void LEDManager::Init(SuccessCallback&& callback) {
  this->initialised = false;
  this->led_on = true; //default to on (when system is on)

  //LEDs off initially
  LED_RED_REG = 0;
  LED_GREEN_REG = LED_PWM_MAX_VALUE;
  LED_BLUEN_REG = LED_PWM_MAX_VALUE;
  LED_BLUEP_REG = 0;
  //set up RGB LED timer
  if (HAL_TIM_Base_Start(&LED_TIMER) != HAL_OK) {
    if (callback) {
      callback(false);
    }
    return;
  }
  if (HAL_TIM_PWM_Start(&LED_TIMER, LED_RED_CH) != HAL_OK) {
    if (callback) {
      callback(false);
    }
    return;
  }
  if (HAL_TIM_PWM_Start(&LED_TIMER, LED_GREEN_CH) != HAL_OK) {
    if (callback) {
      callback(false);
    }
    return;
  }
  LED_GREEN_REG = LED_PWM_MAX_VALUE;
  if (HAL_TIM_PWM_Start(&LED_TIMER, LED_BLUE_CH) != HAL_OK) {
    if (callback) {
      callback(false);
    }
    return;
  }

  this->initialised = true;
  if (callback) {
    callback(true);
  }
}



/******************************************************/
/*           Basic Tasks and Event Handling           */
/******************************************************/

void LEDManager::LoopTasks() {
  if (!this->initialised) {
    return;
  }

  if (this->led_on && this->system.IsPoweredOn()) {
    //LEDs on: calculate hue shift, and resulting hue clamped to [0, 360)
    float hue_shift = LED_HUE_SHIFT_AMPLITUDE * sinf(2.0f * M_PI * (float)(HAL_GetTick() % LED_HUE_SHIFT_PERIOD) / (float)LED_HUE_SHIFT_PERIOD);
    float hue = fmodf(this->GetHueDegrees() + hue_shift, 360.0f);
    if (hue < 0.0f) {
      hue += 360.0f;
    }

    //calculate resulting colour and apply it to PWM
    LEDColor color = LEDManager::ColorHSVToRGB(hue, 1.0f, this->GetBrightness());
    LED_RED_REG = (uint32_t)roundf(color.red * (float)LED_PWM_MAX_VALUE);
    LED_GREEN_REG = (uint32_t)roundf((1.0f - color.green) * (float)LED_PWM_MAX_VALUE);
    LED_BLUEN_REG = (uint32_t)roundf(0.5f * (1.0f - color.blue) * (float)LED_PWM_MAX_VALUE);
    LED_BLUEP_REG = (uint32_t)roundf(0.5f * (1.0f + color.blue) * (float)LED_PWM_MAX_VALUE);
  } else {
    //LEDs off
    LED_RED_REG = 0;
    LED_GREEN_REG = LED_PWM_MAX_VALUE;
    LED_BLUEN_REG = LED_PWM_MAX_VALUE;
    LED_BLUEP_REG = 0;
  }
}


/*void LEDManager::HandlePowerStateChange(bool on) {
  //disable LEDs on system turn off; enable LEDs by default on system turn on
  this->led_on = on;
}*/


void LEDManager::LoadNonVolatileConfigDefaults(StorageSection& section) {
  //write default brightness
  float default_brightness = LED_BRIGHTNESS_DEFAULT;
  section.SetValue32(LED_NVM_BRIGHTNESS, *(uint32_t*)&default_brightness);

  //write default hue
  float default_hue = LED_HUE_DEFAULT;
  section.SetValue32(LED_NVM_HUE, *(uint32_t*)&default_hue);
}



/******************************************************/
/*                LED Config Functions                */
/******************************************************/

bool LEDManager::IsOn() const {
  return this->initialised && this->led_on;
}

float LEDManager::GetBrightness() const {
  uint32_t int_val = this->non_volatile_config.GetValue32(LED_NVM_BRIGHTNESS);
  return *(float*)&int_val;
}

float LEDManager::GetHueDegrees() const {
  uint32_t int_val = this->non_volatile_config.GetValue32(LED_NVM_HUE);
  return *(float*)&int_val;
}


void LEDManager::SetOn(bool on) {
  if (this->initialised) {
    this->led_on = on;
  }
}

void LEDManager::SetBrightness(float brightness) {
  if (isnanf(brightness) || brightness < 0.0f || brightness > 1.0f) {
    throw std::invalid_argument("LEDManager SetBrightness given invalid brightness, must be in [0, 1]");
  }

  //save new brightness
  this->non_volatile_config.SetValue32(LED_NVM_BRIGHTNESS, *(uint32_t*)&brightness);
}

void LEDManager::SetHueDegrees(float hue_degrees) {
  if (isnanf(hue_degrees) || hue_degrees < 0.0f || hue_degrees >= 360.0f) {
    throw std::invalid_argument("LEDManager SetHueDegrees given invalid hue, must be in [0, 360)");
  }

  //save new hue
  this->non_volatile_config.SetValue32(LED_NVM_HUE, *(uint32_t*)&hue_degrees);
}

