/*
 * bat_calculations.c
 *
 *  Created on: Jan 25, 2025
 *      Author: Alex
 */


#include "bat_calculations.h"
#include <math.h>


/********************************************************/
/*  PARAMETERS - need to be updated based on cell type  */
/********************************************************/

//coefficients for quadratic charge-to-energy approximation (constant, linear, quadratic)
static const float bat_chargeToEnergy_coeffs[3] = { -0.02124167f, 3.2197858f, 0.15655184f };
//correction coefficients for charge-to-energy approximation, depending on battery health fraction (linear, quadratic)
static const float bat_chargeToEnergy_healthCorrectionCoeffs[2] = { 0.3f, 0.1f };

//B-spline knots, knot count, control points, and degree for voltage-to-charge estimation
static const float bat_voltageToCharge_t[] = {
    3.0f, 3.0f, 3.0f, 3.0f, 3.15f, 3.29f, 3.33f, 3.37f, 3.44f, 3.51f,
    3.58f, 3.66f, 3.73f, 3.87f, 3.94f, 4.01f, 4.15f, 4.15f, 4.15f, 4.15f
};
static const uint8_t bat_voltageToCharge_t_count = 20;
static const float bat_voltageToCharge_c[] = {
    -0.00355509f, 0.02089707f, 0.00316649f, 0.1122161f, 0.31671303f, 0.37414911f,
    0.51521049f, 0.7574718f, 1.21999862f, 1.53247019f, 1.75656568f, 2.0588198f,
    2.38977397f, 2.56745534f, 2.85180031f, 2.86877747f
};
static const uint8_t bat_voltageToCharge_p = 3;

//B-spline knots, knot count, control points, and degree for voltage-to-energy estimation
static const float bat_voltageToEnergy_t[] = {
    3.0f, 3.0f, 3.0f, 3.0f, 3.15f, 3.22f, 3.26f, 3.29f, 3.44f, 3.51f, 3.58f, 3.62f,
    3.66f, 3.73f, 3.8f, 3.87f, 3.94f, 4.01f, 4.08f, 4.12f, 4.15f, 4.15f, 4.15f, 4.15f
};
static const uint8_t bat_voltageToEnergy_t_count = 24;
static const float bat_voltageToEnergy_c[] = {
    -2.87436676e-03f, 1.81383671e-02f, 5.72841288e-02f, 1.91419519e-01f,
    3.76504645e-01f, 1.05089374e+00f, 1.28755469e+00f, 2.51121639e+00f,
    3.87105895e+00f, 4.65901708e+00f, 5.38827784e+00f, 5.97712740e+00f,
    6.62516653e+00f, 7.65197748e+00f, 8.52844471e+00f, 9.11782949e+00f,
    9.92152850e+00f, 1.03378326e+01f, 1.04702242e+01f, 1.05222716e+01f
};
static const uint8_t bat_voltageToEnergy_p = 3;

//maximum charge per cell in Ah at 100% health 100% charge
const float bat_calc_cellCharge_max = 2.874f;
//maximum energy per cell in Wh at 100% health 100% charge
const float bat_calc_cellEnergy_max = 10.52f;
//maximum per-cell current in A for which the voltage-to-charge approximation is valid
const float bat_calc_voltageToCharge_max_valid_current = 0.2f;
//maximum per-cell current in A for which the voltage-to-energy approximation is valid
const float bat_calc_voltageToEnergy_max_valid_current = 0.2f;


/***************/
/*  FUNCTIONS  */
/***************/

//De Boor's algorithm to calculate value of B-spline with t_count knots given in t, control points given in c, and degree p, at position x
//see also https://en.wikipedia.org/wiki/De_Boor%27s_algorithm
static float _BAT_CALC_DeBoor(float x, const float* t, uint8_t t_count, const float* c, uint8_t p) {
  float d[p + 1];
  int i, j;
  uint8_t k; //index of knot interval that contains x

  if (x <= t[0]) { //at or before range start: clip to range start
    x = t[0];
    k = p; //first "real" knot interval that's not in padding
  } else if (x >= t[t_count - 1]) { //at or after range end: clip to range end
    x = t[t_count - 1];
    k = t_count - p - 2; //last "real" knot interval that's not in padding
  } else { //within range: keep x unchanged, find knot interval index k that contains x
    for (k = p; k < t_count - p - 2; k++) {
      if (x <= t[k + 1]) break;
    }
  }

  //initialise intermediates
  for (i = 0; i <= p; i++) d[i] = c[i + k - p];

  //main algorithm loop - see wikipedia or other article for explanation
  for (i = 1; i <= p; i++) {
    for (j = p; j >= i; j--) {
      float alpha = (x - t[j + k - p]) / (t[j + 1 + k - i] - t[j + k - p]);
      d[j] = (1.0f - alpha) * d[j - 1] + alpha * d[j];
    }
  }

  //return result, clipped to positive numbers
  if (d[p] < 0.0f) return 0.0f;
  else return d[p];
}


//Estimate energy of a single cell in Wh, given its charge in Ah and the battery health fraction
float BAT_CALC_CellChargeToEnergy(float cell_charge_ah, float battery_health) {
  //clamp battery health between 0 and 1
  if (isnanf(battery_health) || battery_health <= 0.0f) return 0.0f;
  else if (battery_health > 1.0f) battery_health = 1.0f;

  //clamp charge between 0 and maximum
  if (cell_charge_ah <= 0.0f) return 0.0f;
  else if (cell_charge_ah > bat_calc_cellCharge_max * battery_health) cell_charge_ah = bat_calc_cellCharge_max * battery_health;

  //calculate quadratic approximation, with correction for battery health
  float battery_health_inverse = 1.0f - battery_health;
  float constant_coeff = bat_chargeToEnergy_coeffs[0];
  float linear_coeff = bat_chargeToEnergy_coeffs[1] + battery_health_inverse * bat_chargeToEnergy_healthCorrectionCoeffs[0];
  float quadratic_coeff = bat_chargeToEnergy_coeffs[2] + battery_health_inverse * bat_chargeToEnergy_healthCorrectionCoeffs[1];
  float res = constant_coeff + cell_charge_ah * (linear_coeff + cell_charge_ah * quadratic_coeff);

  //return result, clamping negative results to 0
  if (res < 0.0f) return 0.0f;
  else return res;
}

//Approximately estimate charge of a single cell in Ah, given its voltage in V and the battery health fraction - valid up to bat_calc_voltageToCharge_max_valid_current
float BAT_CALC_CellVoltageToCharge(float cell_voltage_v, float battery_health) {
  //clamp battery health between 0 and 1
  if (isnanf(battery_health) || battery_health <= 0.0f) return 0.0f;
  else if (battery_health > 1.0f) battery_health = 1.0f;

  //calculate and return B-spline approximation, scaled by battery health
  return battery_health * _BAT_CALC_DeBoor(cell_voltage_v, bat_voltageToCharge_t, bat_voltageToCharge_t_count, bat_voltageToCharge_c, bat_voltageToCharge_p);
}

//Approximately estimate energy of a single cell in Wh, given its voltage in V and the battery health fraction - valid up to bat_calc_voltageToEnergy_max_valid_current
float BAT_CALC_CellVoltageToEnergy(float cell_voltage_v, float battery_health) {
  //clamp battery health between 0 and 1
  if (isnanf(battery_health) || battery_health <= 0.0f) return 0.0f;
  else if (battery_health > 1.0f) battery_health = 1.0f;

  //calculate and return B-spline approximation, scaled by battery health
  return battery_health * _BAT_CALC_DeBoor(cell_voltage_v, bat_voltageToEnergy_t, bat_voltageToEnergy_t_count, bat_voltageToEnergy_c, bat_voltageToEnergy_p);
}
