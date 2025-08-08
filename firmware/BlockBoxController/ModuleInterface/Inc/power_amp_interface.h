/*
 * power_amp_interface.h
 *
 *  Created on: Jul 14, 2025
 *      Author: Alex
 */

#ifndef INC_POWER_AMP_INTERFACE_H_
#define INC_POWER_AMP_INTERFACE_H_


#include "cpp_main.h"
#include "module_interface_i2c.h"
#include "../../../PowerAmpController/Core/Inc/i2c_defines_poweramp.h"


//whether the power amp interface uses CRC for communication
#define IF_POWERAMP_USE_CRC true

//DAP interface events
#define MODIF_POWERAMP_EVENT_STATUS_UPDATE (1u << 8)
#define MODIF_POWERAMP_EVENT_SAFETY_UPDATE (1u << 9)
#define MODIF_POWERAMP_EVENT_PVDD_UPDATE (1u << 10)
#define MODIF_POWERAMP_EVENT_MEASUREMENT_UPDATE (1u << 11)

//reset timeout, in main loop cycles
#define IF_POWERAMP_RESET_TIMEOUT (1000 / MAIN_LOOP_PERIOD_MS)

//minimum and maximum requestable PVDD targets
#define IF_POWERAMP_PVDD_TARGET_MIN 18.7f
#define IF_POWERAMP_PVDD_TARGET_MAX 53.5f


//Power amp status
typedef union {
  struct {
    bool amp_fault : 1;
    bool amp_clip_otw : 1;
    bool amp_shutdown : 1;
    bool pvdd_valid : 1;
    bool pvdd_reducing : 1;
    bool pvdd_offset_nonzero : 1;
    bool pvdd_offset_limit : 1;
    bool safety_warning : 1;
    bool clip_detected : 1;
    bool otw_detected : 1;
    int : 5;
    bool i2c_error : 1;
  };
  uint16_t value;
} PowerAmpStatus;

//Power amp per-channel values
typedef struct {
  float ch_a;
  float ch_b;
  float ch_c;
  float ch_d;
} PowerAmpChannelValues;

//Power amp measurement values
typedef struct {
  PowerAmpChannelValues voltage_rms;
  PowerAmpChannelValues current_rms;
  PowerAmpChannelValues power_average;
  PowerAmpChannelValues power_apparent;
} PowerAmpMeasurements;

//Power amp per-channel and sum thresholds
typedef struct {
  float ch_a;
  float ch_b;
  float ch_c;
  float ch_d;
  float sum;
} PowerAmpChannelThresholds;

//Power amp threshold set for a single measurement
typedef struct {
  PowerAmpChannelThresholds instantaneous;
  PowerAmpChannelThresholds fast;
  PowerAmpChannelThresholds slow;
} PowerAmpThresholdSet;

//Power amp threshold types
typedef enum {
  IF_POWERAMP_THR_IRMS_ERROR,
  IF_POWERAMP_THR_PAVG_ERROR,
  IF_POWERAMP_THR_PAPP_ERROR,
  IF_POWERAMP_THR_IRMS_WARNING,
  IF_POWERAMP_THR_PAVG_WARNING,
  IF_POWERAMP_THR_PAPP_WARNING
} PowerAmpThresholdType;

//Power amp error/warning source
typedef union {
  struct {
    bool channel_sum : 1;
    bool channel_d : 1;
    bool channel_c : 1;
    bool channel_b : 1;
    bool channel_a : 1;
    bool power_apparent_slow : 1;
    bool power_apparent_fast : 1;
    bool power_apparent_instantaneous : 1;
    bool power_average_slow : 1;
    bool power_average_fast : 1;
    bool power_average_instantaneous : 1;
    bool current_rms_slow : 1;
    bool current_rms_fast : 1;
    bool current_rms_instantaneous : 1;
    int : 2;
  };
  uint16_t value;
} PowerAmpErrWarnSource;


static_assert(sizeof(PowerAmpMeasurements) == 16 * sizeof(float));
static_assert(sizeof(PowerAmpThresholdSet) == 15 * sizeof(float));


#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
}


class PowerAmpInterface : public IntRegI2CModuleInterface {
public:
  //whether output measurements (voltage, current, power) are continuously monitored or not - may be changed at any time
  bool monitor_measurements;

  PowerAmpStatus GetStatus() const;

  bool IsManualShutdownActive() const;

  float GetPVDDTargetVoltage() const;
  float GetPVDDRequestedVoltage() const;
  float GetPVDDMeasuredVoltage() const;

  //fast = true: 0.1s smoothing; fast = false: 1s smoothing
  PowerAmpMeasurements GetOutputMeasurements(bool fast) const;

  bool IsSafetyShutdownActive() const;
  PowerAmpErrWarnSource GetSafetyErrorSource() const;
  PowerAmpErrWarnSource GetSafetyWarningSource() const;

  PowerAmpThresholdSet GetSafetyThresholds(PowerAmpThresholdType type) const;


  void SetManualShutdownActive(bool manual_shutdown, SuccessCallback&& callback);

  void ResetModule(SuccessCallback&& callback);

  void SetPVDDTargetVoltage(float voltage, SuccessCallback&& callback);

  void SetSafetyThresholds(PowerAmpThresholdType type, const PowerAmpThresholdSet* thresholds, SuccessCallback&& callback);


  void InitModule(SuccessCallback&& callback);
  void LoopTasks() override;


  PowerAmpInterface(I2CHardwareInterface& hw_interface, uint8_t i2c_address, GPIO_TypeDef* int_port, uint16_t int_pin);

protected:
  bool initialised;

  uint32_t reset_wait_timer;
  SuccessCallback reset_callback;

  void OnRegisterUpdate(uint8_t address) override;
  void OnI2CInterrupt(uint16_t interrupt_flags) override;
};


#endif


#endif /* INC_POWER_AMP_INTERFACE_H_ */
