/*
 * bt_config.h
 *
 *  Created on: Mar 2, 2025
 *      Author: Alex
 */

#ifndef INC_BT_CONFIG_H_
#define INC_BT_CONFIG_H_


//array of required config key-value pairs that we care about - always come in pairs, even indices are keys, odd indices are values
#define BT_CONFIG_ARRAY {\
  "AUDIO", "1 1",\
  "AUDIO_DIGITAL", "0 96000 64 01180101 00000000",\
  "AUTOCONN", "0 3",\
  "AUTO_DATA", "OFF OFF",\
  "BATT_CONFIG", "OFF OFF OFF 3000 4200 10 1470 45 200",\
  "BCAST_CONFIG", "223344 48 2 2 40000 OFF 2 1",\
  "BT_CONFIG", "1 0 0",\
  "BLE_CONFIG", "250 ON OFF 2 0 1A 0 245DFC020000",\
  "COD", "240404",\
  "CODEC", "ON ON ON ON ON ON ON ON ON",\
  "DEEP_SLEEP", "OFF ON",\
  "GPIO_CONFIG", "OFF OFF 00",\
  "MUSIC_META_DATA", "ON",\
  "NAME", "BlockBox v2 neo",\
  "NAME_BCAST", "BlockBox v2n",\
  "NAME_SHORT", "BlockBox v2n",\
  "POWERMAX", "11",\
  "PROFILES", "OFF 0 0 1 0 1 0 1",\
  "SSP_CAPS", "3 OFF",\
  "UART_CONFIG", "9600 ON 0",\
  "UI_CONFIG", "OFF OFF OFF OFF %s %s 0 ON",\
  "VOLUME_CONTROL", "OFF 127 ON"\
}

#define BT_CONFIG_ARRAY_ENTRIES 22


#endif /* INC_BT_CONFIG_H_ */
