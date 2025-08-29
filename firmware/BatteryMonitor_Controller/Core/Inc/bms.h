/*
 * bms.h
 *
 *  Created on: Jan 19, 2025
 *      Author: Alex
 */

#ifndef INC_BMS_H_
#define INC_BMS_H_

#include "main.h"
#include "bms_i2c.h"


//default max number of tries for BMS commands and reads/writes
#define BMS_COMM_MAX_TRIES 3
//max number of tries for BMS init procedure
#define BMS_INIT_MAX_TRIES 2

//maximum wait ticks for init-complete alert
#define BMS_INITCOMP_WAIT_MAX 5

//expected return value of the DEVICE_NUMBER subcommand
#define BMS_EXPECTED_DEVICE_NUMBER 0x7605

//measurement values indicating read errors
#define BMS_ERROR_VOLTAGE INT16_MIN
#define BMS_ERROR_STACKVOLTAGE 0
#define BMS_ERROR_CURRENT INT32_MIN
#define BMS_ERROR_TEMPERATURE INT16_MIN
#define BMS_ERROR_CHARGE INT64_MIN
#define BMS_ERROR_TIME UINT32_MAX

//number of cells in the pack (series and parallel)
#define BMS_CELLS_SERIES 4
#define BMS_CELLS_PARALLEL 4
#define BMS_CELLS_TOTAL (BMS_CELLS_SERIES * BMS_CELLS_PARALLEL)

//current measurement to mA conversion multiplier - based on the CC2 current gain config
#define BMS_CONV_CUR_TO_MA_MULT 2
//charge measurement to mAs conversion multiplier and divisor - multiplier is based on the CC1 current gain config; divisor set to 4 to match 250ms integration period
#define BMS_CONV_CHG_TO_MAS_MULT 1
#define BMS_CONV_CHG_TO_MAS_DIV 4

//IC-integrated pull-up resistor value for thermistor measurement in ohms
#define BMS_THERM_PULLUP 20000.0f
//value of a thermistor measurement LSB, as a fraction of the pull-up reference voltage
#define BMS_THERM_LSB_VALUE (5.0f / 3.0f / 32768.0f)
//Steinhart-Hart coefficients for thermistor temperature calculations - need to match the thermistor used
#define BMS_THERM_COEFF_A 8.211303e-4f
#define BMS_THERM_COEFF_B 2.736018e-4f
#define BMS_THERM_COEFF_C -2.443654e-6f
#define BMS_THERM_COEFF_D 2.818534e-7f

//period of status updates, in main loop cycles
#define BMS_LOOP_PERIOD_STATUS (100 / MAIN_LOOP_PERIOD_MS)
//period of measurement updates, in main loop cycles
#define BMS_LOOP_PERIOD_MEASUREMENTS (1000 / MAIN_LOOP_PERIOD_MS)
//period of temperature measurement updates, in main loop cycles - must be a multiple of BMS_LOOP_PERIOD_MEASUREMENTS!
#define BMS_LOOP_PERIOD_TEMPERATURES (2 * BMS_LOOP_PERIOD_MEASUREMENTS)

//state-of-charge calculation constants
//minimum and maximum cell voltages to be considered acceptable for estimations
#define BMS_SOC_CELL_VOLTAGE_MIN 2.5f
#define BMS_SOC_CELL_VOLTAGE_MAX 4.25f
//voltage above which a cell is considered "fully charged"
#define BMS_SOC_CELL_FULL_CHARGE_VOLTAGE_MIN 4.15f
//cell current and voltage smoothing parameter - approx 30s time constant, given measurements once per second
#define BMS_SOC_SMOOTHING_ALPHA 0.03f
#define BMS_SOC_SMOOTHING_1MALPHA (1.0f - BMS_SOC_SMOOTHING_ALPHA)
//current initialisation factor for start delay before allowing reference point setting - 1.4 with alpha 0.03 results in approx. 10 measurement cycles of delay
#define BMS_SOC_CURRENT_INIT_FACTOR 1.4f
//maximum allowable difference between tracked charge and estimated charge, in Ah
#define BMS_SOC_CHARGE_DIFFERENCE_MAX 0.15f

//cell balancing configuration
//minimum voltage difference between two cells to be considered "unbalanced", in mV - TODO maybe higher in production? might also be fine like this? not sure
#define BMS_BAL_DIFF_START 10
//voltage difference between two cells at/below which balancing should be stopped, in mV - TODO test whether this is too low
#define BMS_BAL_DIFF_STOP 2

//maximum cell charge voltage in mV
#define BMS_MAX_CHG_VOLTAGE 4200
//end-of-discharge voltage in mV
#define BMS_MIN_DSG_VOLTAGE 3000
//end-of-discharge voltage hysteresis in mV (voltage needs to rise this much above the threshold to cancel auto-shutdown)
#define BMS_MIN_DSG_HYSTERESIS 50

//maximum continuous discharge current in mA
#define BMS_MAX_DSG_CURRENT 11000
//maximum peak discharge current in mA
#define BMS_PEAK_DSG_CURRENT 32000
//maximum charge current in mA
#define BMS_MAX_CHG_CURRENT 6000

//shutdown timer settings for the different types, in main loop cycles (not precise for forced shutdown)
#define BMS_SHUTDOWN_TIME_FULL (8500 / MAIN_LOOP_PERIOD_MS)
#define BMS_SHUTDOWN_TIME_EOD (30100 / MAIN_LOOP_PERIOD_MS)
#define BMS_SHUTDOWN_TIME_HOST (10100 / MAIN_LOOP_PERIOD_MS)

//minimum current in mA to be considered "charging" (preventing end-of-discharge shutdown) (on/off values for hysteresis)
#define BMS_CHARGING_CURRENT_THRESHOLD_ON 300
#define BMS_CHARGING_CURRENT_THRESHOLD_OFF 150


//BMS IC mode - shutdown is not an option here, since controller would be unpowered in that mode
typedef enum {
  BMSMODE_UNKNOWN = 0,
  BMSMODE_NORMAL = 1,
  BMSMODE_SLEEP = 2,
  BMSMODE_DEEPSLEEP = 3,
  BMSMODE_CFGUPDATE = 4
} BMS_Mode;

//timed shutdown types
typedef enum {
  BMSSHDN_NONE = 0,
  BMSSHDN_FULL_SHUTDOWN = 1,
  BMSSHDN_END_OF_DISCHARGE = 2,
  BMSSHDN_HOST_REQUEST = 3
} BMS_ShutdownType;

//precision level ("quality") of the state-of-charge calculation
typedef enum {
  SOC_VOLTAGE_ONLY = 0,
  SOC_CHARGE_ESTIMATED = 1,
  SOC_CHARGE_FULL = 2
} BMS_SoC_PrecisionLevel;

//safety alerts/faults bit field
typedef union {
  struct {
    bool regout : 1;
    bool curlatch : 1;
    bool occ : 1;
    bool ocd2 : 1;
    bool ocd1 : 1;
    bool scd : 1;
    bool cuv : 1;
    bool cov : 1;
    bool vss : 1;
    bool vref : 1;
    bool hwd : 1;
    bool otint : 1;
    bool utc : 1;
    bool utd : 1;
    bool otc : 1;
    bool otd : 1;
  };
  struct {
    uint8_t _a : 8;
    uint8_t _b : 8;
  };
  uint16_t _all;
} BMS_SafetyStatus;

//BMS operation status
typedef struct {
  bool _sleep_bit;
  bool _deepsleep_bit;
  bool _sleep_en_bit;
  bool _cfgupdate_bit;

  BMS_Mode mode;
  bool sealed;
  BMS_SafetyStatus safety_alerts;
  BMS_SafetyStatus safety_faults;
  bool fets_enabled;
  bool dsg_state;
  bool chg_state;

  //whether charge fet is forced off until recovered by software or deepsleep
  bool chg_force_off;

  //type of timed shutdown (if one is scheduled)
  BMS_ShutdownType timed_shutdown_type;
  //time until timed shutdown (if one is scheduled) in main loop cycles
  uint16_t timed_shutdown_time;
} BMS_Status;

//BMS voltage, current, temperature, and charge measurements and state-of-charge estimations
typedef struct {
  int16_t voltage_cells[5];           //all in mV
  uint16_t voltage_stack;             //in mV
  int32_t current;                    //in mA
  int16_t bat_temp;                   //in °C
  int16_t internal_temp;              //in °C
  int64_t accumulated_charge;         //in mAs
  uint32_t charge_accumulation_time;  //in 250ms

  //state-of-charge values
  BMS_SoC_PrecisionLevel soc_precisionlevel;
  float soc_energy;    //in Wh
  float soc_fraction;  //in [0, 1]
} BMS_Measurements;


extern BMS_Status bms_status;
extern BMS_Measurements bms_measurements;

//state-of-charge calculation status and results
//pointer to battery health fraction (in (0, 1]) - do not modify directly (EEPROM value), use BMS_SetBatteryHealth function instead
extern float* const bms_soc_battery_health_ptr;

//desired status: may be set externally, BMS_LoopUpdate logic will aim to achieve and keep that status
extern bool bms_should_deepsleep;


//update general BMS status (bms_status variable)
HAL_StatusTypeDef BMS_UpdateStatus();
//update battery measurements (bms_measurements variable)
HAL_StatusTypeDef BMS_UpdateMeasurements(bool include_temps);

//set the FET control status: false = FETs will not turn on, true = FETs may turn on if there are no faults and the FETs aren't set to be forced off
HAL_StatusTypeDef BMS_SetFETControl(bool fets_enabled);
//set which FETs are forced off or not: false = FET may turn on if there are no faults and FET control is enabled, true = FET will be forced off
HAL_StatusTypeDef BMS_SetFETForceOff(bool force_off_dsg, bool force_off_chg);

HAL_StatusTypeDef BMS_EnterDeepSleep();
HAL_StatusTypeDef BMS_ExitDeepSleep();

//enter BMS shutdown mode, which will also power down the controller. instant: false = shutdown after BMS-internal ~10s delay, true = shutdown immediately
HAL_StatusTypeDef BMS_EnterShutdown(bool instant);

//set the estimated battery health value
HAL_StatusTypeDef BMS_SetBatteryHealth(float health);

//start a timed shutdown (deepsleep) by host request
void BMS_StartHostRequestedShutdown();
//cancel a previously host-requested timed shutdown (deepsleep)
void BMS_CancelHostRequestedShutdown();
//whether the host has requested a shutdown
bool BMS_GetHostRequestedShutdown();
//reset internal timed shutdown state (e.g. after user turns off switch)
void BMS_ResetTimedShutdownState();

//get whether the BMS is trying to be in deepsleep
bool BMS_GetDeepSleepDesiredState();

//interrupt handler for BMS alert pin falling edge
void BMS_AlertInterrupt();

//standard initialisation and loop update functions
HAL_StatusTypeDef BMS_Init();
void BMS_LoopUpdate(uint32_t loop_count);


#endif /* INC_BMS_H_ */
