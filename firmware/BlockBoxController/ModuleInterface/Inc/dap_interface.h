/*
 * dap_interface.h
 *
 *  Created on: Jul 12, 2025
 *      Author: Alex
 */

#ifndef INC_DAP_INTERFACE_H_
#define INC_DAP_INTERFACE_H_


#include "cpp_main.h"
#include "module_interface_i2c.h"
#include "../../../DigitalAudioProcessor/Core/Inc/i2c_defines_dap.h"
#include "arm_math.h"


//whether the DAP interface uses CRC for communication
#define IF_DAP_USE_CRC true

//DAP interface events
#define MODIF_DAP_EVENT_STATUS_UPDATE (1u << 8)
#define MODIF_DAP_EVENT_INPUTS_UPDATE (1u << 9)
#define MODIF_DAP_EVENT_INPUT_RATE_UPDATE (1u << 10)
#define MODIF_DAP_EVENT_SRC_STATS_UPDATE (1u << 11)

//reset timeout, in main loop cycles
#define IF_DAP_RESET_TIMEOUT (1000 / MAIN_LOOP_PERIOD_MS)

//minimum/maximum volume and loudness gains
#define IF_DAP_VOLUME_GAIN_MIN -120.0f
#define IF_DAP_VOLUME_GAIN_MAX 20.0f
#define IF_DAP_LOUDNESS_GAIN_MAX 0.0f


//DAP status
typedef union {
  struct {
    bool streaming : 1;
    bool src_ready : 1;
    bool usb_connected : 1;
    int : 4;
    bool i2c_error : 1;
  };
  uint8_t value;
} DAPStatus;

//DAP input enum
typedef enum {
  IF_DAP_INPUT_NONE = I2CDEF_DAP_INPUT_ACTIVE_NONE,
  IF_DAP_INPUT_I2S1 = I2CDEF_DAP_INPUT_ACTIVE_I2S1,
  IF_DAP_INPUT_I2S2 = I2CDEF_DAP_INPUT_ACTIVE_I2S2,
  IF_DAP_INPUT_I2S3 = I2CDEF_DAP_INPUT_ACTIVE_I2S3,
  IF_DAP_INPUT_USB = I2CDEF_DAP_INPUT_ACTIVE_USB,
  IF_DAP_INPUT_SPDIF = I2CDEF_DAP_INPUT_ACTIVE_SPDIF
} DAPInput;

//DAP available inputs
typedef union {
  struct {
    bool i2s1 : 1;
    bool i2s2 : 1;
    bool i2s3 : 1;
    bool usb : 1;
    bool spdif : 1;
    int : 3;
  };
  uint8_t value;
} DAPAvailableInputs;

//DAP sample rate
typedef enum {
  IF_DAP_SR_44K = 44100,
  IF_DAP_SR_48K = 48000,
  IF_DAP_SR_96K = 96000
} DAPSampleRate;

//DAP mixer coefficients
typedef struct {
  q31_t ch1_into_ch1;
  q31_t ch2_into_ch1;
  q31_t ch1_into_ch2;
  q31_t ch2_into_ch2;
} DAPMixerConfig;

//DAP gains
typedef struct {
  float ch1;
  float ch2;
} DAPGains;

//DAP channel
typedef enum {
  IF_DAP_CH1 = 1,
  IF_DAP_CH2 = 2
} DAPChannel;

//DAP biquad setup
typedef struct {
  uint8_t ch1_stages;
  uint8_t ch2_stages;
  uint8_t ch1_shift;
  uint8_t ch2_shift;
} DAPBiquadSetup;

//DAP FIR setup
typedef struct {
  uint16_t ch1_length;
  uint16_t ch2_length;
} DAPFIRSetup;


static_assert(sizeof(DAPMixerConfig) == 16);
static_assert(sizeof(DAPGains) == 8);
static_assert(sizeof(DAPBiquadSetup) == 4);
static_assert(sizeof(DAPFIRSetup) == 4);


#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
}


class DAPInterface : public IntRegI2CModuleInterface {
public:
  //whether SRC stats (input rate error and buffer fill error) are continuously monitored or not - may be changed at any time
  bool monitor_src_stats;

  DAPStatus GetStatus() const;

  void GetConfig(bool& sp_enabled, bool& pos_gain_allowed) const;

  DAPInput GetActiveInput() const;
  DAPAvailableInputs GetAvailableInputs() const;
  bool IsInputAvailable(DAPInput input) const;

  DAPSampleRate GetI2SInputSampleRate(DAPInput input) const;

  DAPSampleRate GetSRCInputSampleRate() const;
  float GetSRCInputRateErrorRelative() const;
  float GetSRCBufferFillErrorSamples() const;

  DAPMixerConfig GetMixerConfig() const;
  DAPGains GetVolumeGains() const;
  DAPGains GetLoudnessGains() const;

  const q31_t* GetBiquadCoefficients(DAPChannel channel) const;
  DAPBiquadSetup GetBiquadSetup() const;

  const q31_t* GetFIRCoefficients(DAPChannel channel) const;
  DAPFIRSetup GetFIRSetup() const;


  void SetConfig(bool sp_enabled, bool pos_gain_allowed, SuccessCallback&& callback);

  void ResetModule(SuccessCallback&& callback);

  void SetActiveInput(DAPInput input, SuccessCallback&& callback);

  void SetI2SInputSampleRate(DAPInput input, DAPSampleRate sample_rate, SuccessCallback&& callback);

  void SetMixerConfig(DAPMixerConfig config, SuccessCallback&& callback);
  void SetVolumeGains(DAPGains gains, SuccessCallback&& callback);
  void SetLoudnessGains(DAPGains gains, SuccessCallback&& callback);

  void SetBiquadCoefficients(DAPChannel channel, const q31_t* coeff_buffer, SuccessCallback&& callback);
  void SetBiquadSetup(DAPBiquadSetup setup, SuccessCallback&& callback);

  void SetFIRCoefficients(DAPChannel channel, const q31_t* coeff_buffer, SuccessCallback&& callback);
  void SetFIRSetup(DAPFIRSetup setup, SuccessCallback&& callback);


  void InitModule(SuccessCallback&& callback);
  void LoopTasks() override;


  DAPInterface(I2CHardwareInterface& hw_interface, uint8_t i2c_address, GPIO_TypeDef* int_port, uint16_t int_pin);

protected:
  bool initialised;

  uint32_t reset_wait_timer;
  SuccessCallback reset_callback;

  void OnRegisterUpdate(uint8_t address) override;
  void OnI2CInterrupt(uint16_t interrupt_flags) override;
};


#endif


#endif /* INC_DAP_INTERFACE_H_ */
