/*
 * hifidac_interface.h
 *
 *  Created on: Jul 13, 2025
 *      Author: Alex
 */

#ifndef INC_HIFIDAC_INTERFACE_H_
#define INC_HIFIDAC_INTERFACE_H_


#include "cpp_main.h"
#include "module_interface_i2c.h"
#include "../../../HiFiDAC_Controller/Core/Inc/i2c_defines_hifidac.h"


//whether the HiFiDAC interface uses CRC for communication
#define IF_HIFIDAC_USE_CRC true

//HiFiDAC interface events
#define MODIF_HIFIDAC_EVENT_STATUS_UPDATE (1u << 8)

//reset timeout, in main loop cycles
#define IF_HIFIDAC_RESET_TIMEOUT (1000 / MAIN_LOOP_PERIOD_MS)

//HiFiDAC volume gain per step
#define IF_HIFIDAC_VOLUME_GAIN_PER_STEP -0.5f


//HiFiDAC status
typedef union {
  struct {
    bool lock : 1;
    bool automute_ch1 : 1;
    bool automute_ch2 : 1;
    bool ramp_done_ch1 : 1;
    bool ramp_done_ch2 : 1;
    bool monitor_error : 1;
    int : 1;
    bool i2c_error : 1;
  };
  uint8_t value;
} HiFiDACStatus;

//HiFiDAC config
typedef union {
  struct {
    int : 1;
    bool dac_enable : 1;
    bool sync_mode : 1;
    bool master_mode : 1;
    int : 4;
  };
  uint8_t value;
} HiFiDACConfig;

//HiFiDAC signal path setup
typedef union {
  struct {
    bool automute_ch1 : 1;
    bool automute_ch2 : 1;
    bool invert_ch1 : 1;
    bool invert_ch2 : 1;
    bool gain4x_ch1 : 1;
    bool gain4x_ch2 : 1;
    bool mute_gnd_ramp : 1;
    int : 1;
  };
  uint8_t value;
} HiFiDACSignalPathSetup;

//HiFiDAC internal clock config
typedef union {
  struct {
    uint8_t select_idac_num : 6;
    bool select_idac_half : 1;
    bool auto_fs_detect : 1;
  };
  uint8_t value;
} HiFiDACInternalClockConfig;

//HiFiDAC FIR interpolation filter shape
typedef enum {
  IF_HIFIDAC_FILTER_MIN_PHASE = 0,
  IF_HIFIDAC_FILTER_LIN_PHASE_FRO_APO = 1,
  IF_HIFIDAC_FILTER_LIN_PHASE_FRO = 2,
  IF_HIFIDAC_FILTER_LIN_PHASE_FRO_LR = 3,
  IF_HIFIDAC_FILTER_LIN_PHASE_SRO = 4,
  IF_HIFIDAC_FILTER_MIN_PHASE_FRO = 5,
  IF_HIFIDAC_FILTER_MIN_PHASE_SRO = 6,
  IF_HIFIDAC_FILTER_MIN_PHASE_SRO_LD = 7
} HiFiDACFilterShape;


#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
}


class HiFiDACInterface : public IntRegI2CModuleInterface {
public:
  HiFiDACStatus GetStatus() const;

  HiFiDACConfig GetConfig() const;

  void GetVolumes(uint8_t& volume_ch1, uint8_t& volume_ch2) const;
  void GetVolumesAsGains(float& gain_ch1, float& gain_ch2) const;
  void GetMutes(bool& mute_ch1, bool& mute_ch2) const;

  HiFiDACSignalPathSetup GetSignalPathSetup() const;

  HiFiDACInternalClockConfig GetInternalClockConfig() const;
  uint16_t GetI2SMasterClockDivider() const;

  uint8_t GetTDMSlotCount() const;
  void GetChannelTDMSlots(uint8_t& slot_ch1, uint8_t& slot_ch2) const;

  HiFiDACFilterShape GetInterpolationFilterShape() const;

  void GetSecondHarmonicCorrectionCoefficients(int16_t& thd_c2_ch1, int16_t& thd_c2_ch2) const;
  void GetThirdHarmonicCorrectionCoefficients(int16_t& thd_c3_ch1, int16_t& thd_c3_ch2) const;


  void SetConfig(HiFiDACConfig config, SuccessCallback&& callback);

  void ResetModule(SuccessCallback&& callback);

  void SetVolumes(uint8_t volume_ch1, uint8_t volume_ch2, SuccessCallback&& callback);
  void SetVolumesFromGains(float gain_ch1, float gain_ch2, SuccessCallback&& callback);
  void SetMutes(bool mute_ch1, bool mute_ch2, SuccessCallback&& callback);

  void SetSignalPathSetup(HiFiDACSignalPathSetup setup, SuccessCallback&& callback);

  void SetInternalClockConfig(HiFiDACInternalClockConfig config, SuccessCallback&& callback);
  void SetI2SMasterClockDivider(uint16_t divider, SuccessCallback&& callback);

  void SetTDMSlotCount(uint8_t slot_count, SuccessCallback&& callback);
  void SetChannelTDMSlots(uint8_t slot_ch1, uint8_t slot_ch2, SuccessCallback&& callback);

  void SetInterpolationFilterShape(HiFiDACFilterShape filter_shape, SuccessCallback&& callback);

  void SetSecondHarmonicCorrectionCoefficients(int16_t thd_c2_ch1, int16_t thd_c2_ch2, SuccessCallback&& callback);
  void SetThirdHarmonicCorrectionCoefficients(int16_t thd_c3_ch1, int16_t thd_c3_ch2, SuccessCallback&& callback);


  void InitModule(SuccessCallback&& callback);
  void LoopTasks() override;


  HiFiDACInterface(I2CHardwareInterface& hw_interface, uint8_t i2c_address, GPIO_TypeDef* int_port, uint16_t int_pin);

protected:
  bool initialised;

  uint32_t reset_wait_timer;
  SuccessCallback reset_callback;

  void OnRegisterUpdate(uint8_t address) override;
  void OnI2CInterrupt(uint16_t interrupt_flags) override;
};


#endif


#endif /* INC_HIFIDAC_INTERFACE_H_ */
