/*
 * audio_path_manager.h
 *
 *  Created on: Jul 22, 2025
 *      Author: Alex
 */

#ifndef INC_AUDIO_PATH_MANAGER_H_
#define INC_AUDIO_PATH_MANAGER_H_


#include "cpp_main.h"
#include "event_source.h"
#include "storage.h"
#include "dap_interface.h"
#include "math.h"


//audio path events
#define AUDIO_EVENT_INPUT_UPDATE (1u << 8)
#define AUDIO_EVENT_VOLUME_UPDATE (1u << 9)
#define AUDIO_EVENT_MUTE_UPDATE (1u << 10)

//audio path setting limits
#define AUDIO_LIMIT_MIN_VOLUME_MIN -80.0f
#define AUDIO_LIMIT_MIN_VOLUME_MAX -10.0f
#define AUDIO_LIMIT_MAX_VOLUME_MIN -40.0f
#define AUDIO_LIMIT_MAX_VOLUME_MAX 20.0f
#define AUDIO_LIMIT_VOLUME_RANGE_MIN 20.0f
#define AUDIO_LIMIT_VOLUME_STEP_MIN 1.0f
#define AUDIO_LIMIT_VOLUME_STEP_MAX 5.0f
#define AUDIO_LIMIT_LOUDNESS_GAIN_MIN_ACTIVE -30.0f
#define AUDIO_LIMIT_LOUDNESS_GAIN_MAX 0.0f

//audio path setting lock timeout, in main loop cycles
#define AUDIO_LOCK_TIMEOUT_CYCLES (200 / MAIN_LOOP_PERIOD_MS)


#ifdef __cplusplus


typedef enum {
  AUDIO_INPUT_NONE = IF_DAP_INPUT_NONE,
  AUDIO_INPUT_BLUETOOTH = IF_DAP_INPUT_I2S1,
  AUDIO_INPUT_USB = IF_DAP_INPUT_USB,
  AUDIO_INPUT_SPDIF = IF_DAP_INPUT_SPDIF
} AudioPathInput;


class BlockBoxV2System;


class AudioPathManager : public EventSource {
public:
  BlockBoxV2System& system;

  AudioPathManager(BlockBoxV2System& system);

  void Init(SuccessCallback&& callback);
  void LoopTasks();

  //input functions
  AudioPathInput GetActiveInput();
  bool IsInputAvailable(AudioPathInput input) const;

  void SetActiveInput(AudioPathInput input, SuccessCallback&& callback);

  //current absolute volume functions
  float GetCurrentVolumeDB() const;

  void SetCurrentVolumeDB(float volume_dB, SuccessCallback&& callback);
  void ChangeCurrentVolumeDB(float volume_change_dB, SuccessCallback&& callback);
  void StepCurrentVolumeDB(bool up, SuccessCallback&& callback);

  //current relative volume functions (0 to 1, relative to current min-max range)
  float GetCurrentRelativeVolume() const;

  void SetCurrentRelativeVolume(float relative_volume, SuccessCallback&& callback);

  //volume min/max/step functions
  float GetMinVolumeDB() const;
  float GetMaxVolumeDB() const;
  float GetVolumeStepDB() const;
  bool IsPositiveGainAllowed() const;

  void SetMinVolumeDB(float min_volume_dB, SuccessCallback&& callback);
  void SetMaxVolumeDB(float max_volume_dB, SuccessCallback&& callback);
  void SetVolumeStepDB(float volume_step_dB, SuccessCallback&& callback);
  void SetPositiveGainAllowed(bool pos_gain_allowed, SuccessCallback&& callback);

  //mute functions
  bool IsMute() const;

  void SetMute(bool mute, SuccessCallback&& callback);

  //loudness functions
  float GetLoudnessGainDB() const;
  bool IsLoudnessTrackingMaxVolume() const;

  void SetLoudnessGainDB(float loudness_gain_dB, SuccessCallback&& callback);
  void SetLoudnessTrackingMaxVolume(bool track_max_volume, SuccessCallback&& callback);

  //TODO if desired: different EQ modes, bass/treble, speaker response calibration mode

protected:
  StorageSection non_volatile_config;

  bool initialised;
  uint32_t lock_timer;

  AudioPathInput persistent_active_input;

  float current_volume_dB;

  float min_volume_dB;
  float max_volume_dB;
  float volume_step_dB;

  static void LoadNonVolatileConfigDefaults(StorageSection& section);

  AudioPathInput DetermineActiveInput() const;

  void CheckAndFixVolumeLimits();
  void CheckAndFixVolumeStep();
  void ClampAndApplyVolumeGain(float desired_gain_dB, SuccessCallback&& callback);
};


#endif


#endif /* INC_AUDIO_PATH_MANAGER_H_ */
