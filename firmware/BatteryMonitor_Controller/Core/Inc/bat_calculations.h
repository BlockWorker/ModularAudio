/*
 * bat_calculations.h
 *
 *  Created on: Jan 25, 2025
 *      Author: Alex
 */

#ifndef INC_BAT_CALCULATIONS_H_
#define INC_BAT_CALCULATIONS_H_


#include <stdint.h>


//maximum per-cell current in A for which the voltage-to-charge approximation is valid
extern const float bat_calc_voltageToCharge_max_valid_current;
//maximum per-cell current in A for which the voltage-to-energy approximation is valid
extern const float bat_calc_voltageToEnergy_max_valid_current;


//Estimate energy of a single cell in Wh, given its charge in Ah
float BAT_CALC_CellChargeToEnergy(float cell_charge_ah);

//Approximately estimate charge of a single cell in Ah, given its voltage in V - valid up to bat_calc_voltageToCharge_max_valid_current
float BAT_CALC_CellVoltageToCharge(float cell_voltage_v);

//Approximately estimate energy of a single cell in Wh, given its voltage in V - valid up to bat_calc_voltageToEnergy_max_valid_current
float BAT_CALC_CellVoltageToEnergy(float cell_voltage_v);


#endif /* INC_BAT_CALCULATIONS_H_ */
