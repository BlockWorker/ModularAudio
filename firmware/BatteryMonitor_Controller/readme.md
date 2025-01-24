# BatteryMonitor Controller

The following code changes may be necessary, depending on the application and the cells used:

- bms_config.h: All configuration values should be adjusted to fit the application and cell specifications, as well as the factory calibration defaults of the specific IC sample
- bms.h: Thermistor Steinhart-Hart coefficients need to be adjusted for the thermistor used (alternatively, redesign temperature calculation function in bms.c)
- bms.c: Voltage-based state-of-charge approximation function should be designed to fit the cells' voltage-charge curve
- If BMS sleep mode is desired: May need additional handling logic (not implemented yet)