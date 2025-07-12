/*
 * bluetooth_receiver_interface.h
 *
 *  Created on: Jul 7, 2025
 *      Author: Alex
 */

#ifndef INC_BLUETOOTH_RECEIVER_INTERFACE_H_
#define INC_BLUETOOTH_RECEIVER_INTERFACE_H_


#include "cpp_main.h"
#include "module_interface_uart.h"
#include "../../../BluetoothReceiver_Controller/Core/Inc/uart_defines_btrx.h"


//whether the bluetooth receiver interface uses CRC for communication
#define IF_BTRX_USE_CRC true

//bluetooth receiver interface events
#define MODIF_BTRX_EVENT_STATUS_UPDATE (1u << 8)
#define MODIF_BTRX_EVENT_VOLUME_UPDATE (1u << 9)
#define MODIF_BTRX_EVENT_MEDIA_META_UPDATE (1u << 10)
#define MODIF_BTRX_EVENT_DEVICE_UPDATE (1u << 11)
#define MODIF_BTRX_EVENT_CONN_STATS_UPDATE (1u << 12)
#define MODIF_BTRX_EVENT_CODEC_UPDATE (1u << 13)

//init timeout, in main loop cycles
#define IF_BTRX_INIT_TIMEOUT (4000 / MAIN_LOOP_PERIOD_MS)
//reset timeout, in main loop cycles
#define IF_BTRX_RESET_TIMEOUT (500 / MAIN_LOOP_PERIOD_MS)


//BTRX specific event notification types
typedef enum {
  IF_BTRX_EVENT_MCU_RESET = IF_UART_EVENT_MCU_RESET,
  IF_BTRX_EVENT_WRITE_ACK = IF_UART_EVENT_WRITE_ACK,
  IF_BTRX_EVENT_ERROR = IF_UART_EVENT_ERROR,
  IF_BTRX_EVENT_BT_RESET = UARTDEF_BTRX_EVENT_BT_RESET
} BluetoothReceiverInterfaceEventNotifType;

//BTRX specific error types
typedef enum {
  IF_BTRX_ERROR_UNKNOWN = IF_UART_ERROR_UNKNOWN,
  IF_BTRX_ERROR_BT_UNKNOWN_ERROR = UARTDEF_BTRX_ERROR_UNKNOWN_ERROR,
  IF_BTRX_ERROR_BT_COMMAND_NOT_ALLOWED_CONFIG = UARTDEF_BTRX_ERROR_COMMAND_NOT_ALLOWED_CONFIG,
  IF_BTRX_ERROR_BT_COMMAND_NOT_FOUND = UARTDEF_BTRX_ERROR_COMMAND_NOT_FOUND,
  IF_BTRX_ERROR_BT_WRONG_PARAMETER = UARTDEF_BTRX_ERROR_WRONG_PARAMETER,
  IF_BTRX_ERROR_BT_WRONG_NUMBER_OF_PARAMETERS = UARTDEF_BTRX_ERROR_WRONG_NUMBER_OF_PARAMETERS,
  IF_BTRX_ERROR_BT_COMMAND_NOT_ALLOWED_STATE = UARTDEF_BTRX_ERROR_COMMAND_NOT_ALLOWED_STATE,
  IF_BTRX_ERROR_BT_DEVICE_ALREADY_CONNECTED = UARTDEF_BTRX_ERROR_DEVICE_ALREADY_CONNECTED,
  IF_BTRX_ERROR_BT_DEVICE_NOT_CONNECTED = UARTDEF_BTRX_ERROR_DEVICE_NOT_CONNECTED,
  IF_BTRX_ERROR_BT_COMMAND_TOO_LONG = UARTDEF_BTRX_ERROR_COMMAND_TOO_LONG,
  IF_BTRX_ERROR_BT_NAME_NOT_FOUND = UARTDEF_BTRX_ERROR_NAME_NOT_FOUND,
  IF_BTRX_ERROR_BT_CONFIG_NOT_FOUND = UARTDEF_BTRX_ERROR_CONFIG_NOT_FOUND,
  IF_BTRX_ERROR_BT_OPEN_GENERAL_ERROR = UARTDEF_BTRX_ERROR_OPEN_GENERAL_ERROR,
  IF_BTRX_ERROR_BT_OPEN_DEVICE_NOT_PRESENT = UARTDEF_BTRX_ERROR_OPEN_DEVICE_NOT_PRESENT,
  IF_BTRX_ERROR_BT_OPEN_DEVICE_NOT_PAIRED = UARTDEF_BTRX_ERROR_OPEN_DEVICE_NOT_PAIRED,
  IF_BTRX_ERROR_BT_COMMAND_TIMEOUT = UARTDEF_BTRX_ERROR_COMMAND_TIMEOUT,
  IF_BTRX_ERROR_UART_FORMAT_ERROR_REMOTE = IF_UART_ERROR_UART_FORMAT_ERROR_REMOTE,
  IF_BTRX_ERROR_INTERNAL_UART_ERROR_REMOTE = IF_UART_ERROR_INTERNAL_UART_ERROR_REMOTE,
  IF_BTRX_ERROR_UART_COMMAND_NOT_ALLOWED = IF_UART_ERROR_UART_COMMAND_NOT_ALLOWED,
  IF_BTRX_ERROR_UART_CRC_ERROR_REMOTE = IF_UART_ERROR_UART_CRC_ERROR_REMOTE,
  IF_BTRX_ERROR_UART_FORMAT_ERROR_LOCAL = IF_UART_ERROR_UART_FORMAT_ERROR_LOCAL,
  IF_BTRX_ERROR_UART_CRC_ERROR_LOCAL = IF_UART_ERROR_UART_CRC_ERROR_LOCAL
} BluetoothReceiverInterfaceErrorType;

//BTRX status
typedef union {
  struct {
    bool init_done : 1;
    bool connectable : 1;
    bool discoverable : 1;
    bool connected : 1;
    bool a2dp_link : 1;
    bool avrcp_link : 1;
    bool a2dp_streaming : 1;
    bool avrcp_playing : 1;
    bool ota_upgrade_enabled : 1;
    int : 6;
    bool uart_error : 1;
  };
  uint16_t value;
} BluetoothReceiverStatus;

//BTRX media control actions
typedef enum {
  IF_BTRX_MEDIA_PLAY = UARTDEF_BTRX_MEDIA_CONTROL_PLAY_Val,
  IF_BTRX_MEDIA_PAUSE = UARTDEF_BTRX_MEDIA_CONTROL_PAUSE_Val,
  IF_BTRX_MEDIA_STOP = UARTDEF_BTRX_MEDIA_CONTROL_STOP_Val,
  IF_BTRX_MEDIA_FORWARD = UARTDEF_BTRX_MEDIA_CONTROL_FORWARD_Val,
  IF_BTRX_MEDIA_BACKWARD = UARTDEF_BTRX_MEDIA_CONTROL_BACKWARD_Val,
  IF_BTRX_MEDIA_FF_START = UARTDEF_BTRX_MEDIA_CONTROL_FF_START_Val,
  IF_BTRX_MEDIA_FF_END = UARTDEF_BTRX_MEDIA_CONTROL_FF_END_Val,
  IF_BTRX_MEDIA_REW_START = UARTDEF_BTRX_MEDIA_CONTROL_REW_START_Val,
  IF_BTRX_MEDIA_REW_END = UARTDEF_BTRX_MEDIA_CONTROL_REW_END_Val
} BluetoothReceiverMediaControl;


#ifdef __cplusplus
extern "C" {
#endif


#ifdef __cplusplus
}


class BluetoothReceiverInterface : public RegUARTModuleInterface {
public:
  BluetoothReceiverStatus GetStatus() const;

  uint8_t GetAbsoluteVolume() const;

  const char* GetMediaTitle() const;
  const char* GetMediaArtist() const;
  const char* GetMediaAlbum() const;

  uint64_t GetDeviceAddress() const;
  const char* GetDeviceName() const;

  int16_t GetConnectionRSSI() const;
  uint16_t GetConnectionQuality() const;

  const char* GetActiveCodec() const;


  void SetAbsoluteVolume(uint8_t volume, SuccessCallback&& callback);

  void ResetModule(SuccessCallback&& callback);

  void EnableOTAUpgrade(SuccessCallback&& callback);

  void SetConnectable(bool connectable, SuccessCallback&& callback);
  void SetDiscoverable(bool discoverable, SuccessCallback&& callback);

  void DisconnectBluetooth(SuccessCallback&& callback);

  void SendMediaControl(BluetoothReceiverMediaControl action, SuccessCallback&& callback);

  void SendTone(std::string& tone, SuccessCallback&& callback);


  void InitModule(SuccessCallback&& callback);
  void LoopTasks() override;


  BluetoothReceiverInterface(UART_HandleTypeDef* uart_handle);

protected:
  bool initialised;
  uint32_t init_wait_timer;
  SuccessCallback init_callback;

  uint32_t reset_wait_timer;
  SuccessCallback reset_callback;

  void HandleNotificationData(bool error, bool unsolicited) override;
  bool IsCommandError(bool* should_retry) noexcept override;
  void OnRegisterUpdate(uint8_t address) override;
};


#endif


#endif /* INC_BLUETOOTH_RECEIVER_INTERFACE_H_ */
