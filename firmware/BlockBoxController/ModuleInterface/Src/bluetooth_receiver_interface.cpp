/*
 * bluetooth_receiver_interface.cpp
 *
 *  Created on: Jul 7, 2025
 *      Author: Alex
 */


#include "bluetooth_receiver_interface.h"


//scratch space for register discard-reads
static uint8_t btrx_scratch[256];


BluetoothReceiverStatus BluetoothReceiverInterface::GetStatus() const {
  BluetoothReceiverStatus status;
  status.value = this->registers.Reg16(UARTDEF_BTRX_STATUS);
  return status;
}

uint8_t BluetoothReceiverInterface::GetAbsoluteVolume() const {
  return this->registers.Reg8(UARTDEF_BTRX_VOLUME) & 0x7f;
}


const char* BluetoothReceiverInterface::GetMediaTitle() const {
  return (const char*)this->registers[UARTDEF_BTRX_TITLE];
}

const char* BluetoothReceiverInterface::GetMediaArtist() const {
  return (const char*)this->registers[UARTDEF_BTRX_ARTIST];
}

const char* BluetoothReceiverInterface::GetMediaAlbum() const {
  return (const char*)this->registers[UARTDEF_BTRX_ALBUM];
}


uint64_t BluetoothReceiverInterface::GetDeviceAddress() const {
  uint64_t address = 0;
  memcpy(&address, this->registers[UARTDEF_BTRX_DEVICE_ADDR], 6);
  return address;
}

const char* BluetoothReceiverInterface::GetDeviceName() const {
  return (const char*)this->registers[UARTDEF_BTRX_DEVICE_NAME];
}


int16_t BluetoothReceiverInterface::GetConnectionRSSI() const {
  return (int16_t)this->registers.Reg32(UARTDEF_BTRX_CONN_STATS);
}

uint16_t BluetoothReceiverInterface::GetConnectionQuality() const {
  return (uint16_t)(this->registers.Reg32(UARTDEF_BTRX_CONN_STATS) >> 16);
}


const char* BluetoothReceiverInterface::GetActiveCodec() const {
  return (const char*)this->registers[UARTDEF_BTRX_CODEC];
}



void BluetoothReceiverInterface::SetAbsoluteVolume(uint8_t volume, SuccessCallback&& callback) {
  if (volume > 127) {
    throw std::invalid_argument("BluetoothReceiver absolute volume must be in range 0-127");
  }

  this->WriteRegister8Async(UARTDEF_BTRX_VOLUME, volume, SuccessToTransferCallback(callback));
}


void BluetoothReceiverInterface::ResetModule(SuccessCallback&& callback) {
  //store reset callback and start timer
  this->reset_callback = std::move(callback);
  this->reset_wait_timer = IF_BTRX_RESET_TIMEOUT;

  //write reset without callback, since we're not getting an acknowledgement for this write
  this->WriteRegister8Async(UARTDEF_BTRX_CONTROL, UARTDEF_BTRX_CONTROL_RESET_VALUE << UARTDEF_BTRX_CONTROL_RESET_Pos, ModuleTransferCallback());
}


void BluetoothReceiverInterface::EnableOTAUpgrade(SuccessCallback&& callback) {
  //set enable bit in control register
  this->WriteRegister8Async(UARTDEF_BTRX_CONTROL, UARTDEF_BTRX_CONTROL_ENABLE_OTA_UPGRADE_Msk, SuccessToTransferCallback(callback));
}


void BluetoothReceiverInterface::SetConnectable(bool connectable, SuccessCallback&& callback) {
  //select bit in connection control register to set (on or off)
  uint8_t value = connectable ? UARTDEF_BTRX_CONN_CONTROL_CONNECTABLE_ON_Msk : UARTDEF_BTRX_CONN_CONTROL_CONNECTABLE_OFF_Msk;

  this->WriteRegister8Async(UARTDEF_BTRX_CONN_CONTROL, value, SuccessToTransferCallback(callback));
}

void BluetoothReceiverInterface::SetDiscoverable(bool discoverable, SuccessCallback&& callback) {
  //select bit in connection control register to set (on or off)
  uint8_t value = discoverable ? UARTDEF_BTRX_CONN_CONTROL_DISCOVERABLE_ON_Msk : UARTDEF_BTRX_CONN_CONTROL_DISCOVERABLE_OFF_Msk;

  this->WriteRegister8Async(UARTDEF_BTRX_CONN_CONTROL, value, SuccessToTransferCallback(callback));
}


void BluetoothReceiverInterface::DisconnectBluetooth(SuccessCallback&& callback) {
  //set disconnect bit in control register
  this->WriteRegister8Async(UARTDEF_BTRX_CONN_CONTROL, UARTDEF_BTRX_CONN_CONTROL_DISCONNECT_Msk, SuccessToTransferCallback(callback));
}



void BluetoothReceiverInterface::CutAndDisableConnections(SuccessCallback&& callback) {
  //set connection control register to connectable off + discoverable off + disconnect
  uint8_t value = UARTDEF_BTRX_CONN_CONTROL_CONNECTABLE_OFF_Msk | UARTDEF_BTRX_CONN_CONTROL_DISCOVERABLE_OFF_Msk | UARTDEF_BTRX_CONN_CONTROL_DISCONNECT_Msk;

  this->WriteRegister8Async(UARTDEF_BTRX_CONN_CONTROL, value, SuccessToTransferCallback(callback));
}


void BluetoothReceiverInterface::SendMediaControl(BluetoothReceiverMediaControl action, SuccessCallback&& callback) {
  //get and verify action value
  uint8_t value = (uint8_t)action;
  if (value < UARTDEF_BTRX_MEDIA_CONTROL_PLAY_Val || value > UARTDEF_BTRX_MEDIA_CONTROL_REW_END_Val) {
    throw std::invalid_argument("SendMediaControl invalid action given");
  }

  this->WriteRegister8Async(UARTDEF_BTRX_MEDIA_CONTROL, value, SuccessToTransferCallback(callback));
}


void BluetoothReceiverInterface::SendTone(std::string& tone, SuccessCallback&& callback) {
  if (tone.length() == 0 || tone.length() >= 239) {
    throw std::invalid_argument("SendTone string length must be 1-238 characters");
  }

  //write tone string, using register-less "base" function to allow variable length
  this->UARTModuleInterface::WriteRegisterAsync(UARTDEF_BTRX_TONE, (const uint8_t*)tone.c_str(), tone.length() + 1, SuccessToTransferCallback(callback));
}



void BluetoothReceiverInterface::InitModule(SuccessCallback&& callback) {
  this->initialised = false;

  this->init_wait_timer = 0;
  this->reset_wait_timer = 0;

  //clear status (in particular, init-done bit)
  this->_registers.Reg16(UARTDEF_BTRX_STATUS) = 0;
  this->OnRegisterUpdate(UARTDEF_BTRX_STATUS);

  //read module ID to check communication
  this->ReadRegister8Async(UARTDEF_BTRX_MODULE_ID, [&, callback = std::move(callback)](bool success, uint32_t value, uint16_t) {
    if (!success) {
      //report failure to external callback
      if (callback) {
        callback(false);
      }
      return;
    }

    //check correctness of module ID
    if ((uint8_t)value != UARTDEF_BTRX_MODULE_ID_VALUE) {
      DEBUG_PRINTF("* BluetoothReceiver module ID incorrect: 0x%02X instead of 0x%02X\n", (uint8_t)value, UARTDEF_BTRX_MODULE_ID_VALUE);
      //report failure to external callback
      if (callback) {
        callback(false);
      }
      return;
    }

    //write notification mask (enable all notifications)
    this->WriteRegister16Async(UARTDEF_BTRX_NOTIF_MASK, 0x1FF, [&, callback = std::move(callback)](bool success, uint32_t, uint16_t) {
      if (!success) {
        //report failure to external callback
        if (callback) {
          callback(false);
        }
        return;
      }

      //create init callback (contains further initialisation steps)
      this->init_callback = [&, callback = std::move(callback)](bool success) {
        if (!success) {
          //report failure to external callback
          if (callback) {
            callback(false);
          }
          return;
        }

        //read all registers once to update registers to their initial values
        this->ReadRegister8Async(UARTDEF_BTRX_VOLUME, ModuleTransferCallback());
        this->ReadRegisterAsync(UARTDEF_BTRX_TITLE, btrx_scratch, ModuleTransferCallback());
        this->ReadRegisterAsync(UARTDEF_BTRX_ARTIST, btrx_scratch, ModuleTransferCallback());
        this->ReadRegisterAsync(UARTDEF_BTRX_ALBUM, btrx_scratch, ModuleTransferCallback());
        this->ReadRegisterAsync(UARTDEF_BTRX_DEVICE_ADDR, btrx_scratch, ModuleTransferCallback());
        this->ReadRegisterAsync(UARTDEF_BTRX_DEVICE_NAME, btrx_scratch, ModuleTransferCallback());
        this->ReadRegister32Async(UARTDEF_BTRX_CONN_STATS, ModuleTransferCallback());
        this->ReadRegisterAsync(UARTDEF_BTRX_CODEC, btrx_scratch, [&, callback = std::move(callback)](bool, uint32_t, uint16_t) {
          //after last read is done: init completed successfully (even if read failed - that's non-critical)
          this->initialised = true;
          if (callback) {
            callback(true);
          }
        });
      };

      //start init wait
      this->init_wait_timer = IF_BTRX_INIT_TIMEOUT;
      //read status register immediately (in case init is already done) - no callback needed, we're only reading to update the register
      this->ReadRegister16Async(UARTDEF_BTRX_STATUS, ModuleTransferCallback());
    });
  });
}


void BluetoothReceiverInterface::LoopTasks() {
  static uint32_t loop_count = 0;

  if (this->initialised && loop_count++ % 50 == 0) {
    //every 50 cycles (500ms), read status - no callback needed, we're only reading to update the register
    this->ReadRegister16Async(UARTDEF_BTRX_STATUS, ModuleTransferCallback());
  }

  //allow base handling
  this->RegUARTModuleInterface::LoopTasks();

  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  if (this->init_wait_timer > 0) {
    //check for init timeout
    if (--this->init_wait_timer == 0) {
      //timed out: report to callback
      __set_PRIMASK(primask);
      if (this->init_callback) {
        this->init_callback(false);
      }
    }
  }
  __set_PRIMASK(primask);

  primask = __get_PRIMASK();
  __disable_irq();
  if (this->reset_wait_timer > 0) {
    //check for reset timeout
    if (--this->reset_wait_timer == 0) {
      //timed out: report to callback
      __set_PRIMASK(primask);
      if (this->reset_callback) {
        this->reset_callback(false);
        //clear reset callback
        this->reset_callback = SuccessCallback();
      }
    }
  }
  __set_PRIMASK(primask);
}



BluetoothReceiverInterface::BluetoothReceiverInterface(UART_HandleTypeDef* uart_handle) :
    RegUARTModuleInterface(uart_handle, UARTDEF_BTRX_REG_SIZES, IF_BTRX_USE_CRC), initialised(false), init_wait_timer(0), reset_wait_timer(0) {}



void BluetoothReceiverInterface::HandleNotificationData(bool error, bool unsolicited) {
  //allow base handling
  this->RegUARTModuleInterface::HandleNotificationData(error, unsolicited);

  if (error) {
    //notifications with errors should be disregarded
    return;
  }

  //check notification type
  switch ((ModuleInterfaceUARTNotificationType)this->parse_buffer[0]) {
    case IF_UART_TYPE_EVENT:
      switch ((BluetoothReceiverInterfaceEventNotifType)this->parse_buffer[1]) {
        case IF_BTRX_EVENT_BT_RESET:
          //only re-initialise if already initialised, or reset is pending
          if (this->initialised || this->reset_wait_timer > 0) {
            if (this->reset_wait_timer == 0) {
              DEBUG_PRINTF("BluetoothReceiver module spurious reset detected\n");
            }
            //perform module re-init
            this->InitModule([&](bool success) {
              //notify system of reset, then call reset callback
              this->ExecuteCallbacks(MODIF_EVENT_MODULE_RESET);
              if (this->reset_callback) {
                this->reset_callback(success);
                //clear reset callback
                this->reset_callback = SuccessCallback();
              }
            });
          }
          break;
        default:
          break;
      }
      break;
    default:
      //other types already handled as desired
      return;
  }
}

bool BluetoothReceiverInterface::IsCommandError(bool* should_retry) noexcept {
  //definitely true for all general UART command errors
  if (this->RegUARTModuleInterface::IsCommandError(should_retry)) {
    return true;
  }

  bool _should_retry = false;

  //check BTRX-specific errors
  switch ((BluetoothReceiverInterfaceErrorType)this->current_error) {
    case IF_BTRX_ERROR_BT_OPEN_GENERAL_ERROR:
    case IF_BTRX_ERROR_BT_COMMAND_TIMEOUT:
      _should_retry = true;
    case IF_BTRX_ERROR_BT_COMMAND_NOT_ALLOWED_CONFIG:
    case IF_BTRX_ERROR_BT_COMMAND_NOT_FOUND:
    case IF_BTRX_ERROR_BT_WRONG_PARAMETER:
    case IF_BTRX_ERROR_BT_WRONG_NUMBER_OF_PARAMETERS:
    case IF_BTRX_ERROR_BT_COMMAND_NOT_ALLOWED_STATE:
    case IF_BTRX_ERROR_BT_DEVICE_ALREADY_CONNECTED:
    case IF_BTRX_ERROR_BT_DEVICE_NOT_CONNECTED:
    case IF_BTRX_ERROR_BT_COMMAND_TOO_LONG:
    case IF_BTRX_ERROR_BT_NAME_NOT_FOUND:
    case IF_BTRX_ERROR_BT_CONFIG_NOT_FOUND:
    case IF_BTRX_ERROR_BT_OPEN_DEVICE_NOT_PRESENT:
    case IF_BTRX_ERROR_BT_OPEN_DEVICE_NOT_PAIRED:
      break;
    default:
      return false;
  }

  if (should_retry != NULL) {
    *should_retry = _should_retry;
  }
  return true;
}

void BluetoothReceiverInterface::OnRegisterUpdate(uint8_t address) {
  //allow base handling
  this->RegUARTModuleInterface::OnRegisterUpdate(address);

  uint32_t event;
  uint32_t primask;

  //select corresponding event
  switch (address) {
    case UARTDEF_BTRX_STATUS:
      event = MODIF_BTRX_EVENT_STATUS_UPDATE;

      //check init-complete bit
      primask = __get_PRIMASK();
      __disable_irq();
      if (this->init_wait_timer > 0) {
        //waiting for init: check if we have an init completion
        if (this->GetStatus().init_done) {
          //init complete: stop wait timer, proceed to init callback
          this->init_wait_timer = 0;
          __set_PRIMASK(primask);
          if (this->init_callback) {
            this->init_callback(true);
          }
        }
      }
      __set_PRIMASK(primask);
      break;
    case UARTDEF_BTRX_VOLUME:
      event = MODIF_BTRX_EVENT_VOLUME_UPDATE;
      break;
    case UARTDEF_BTRX_TITLE:
    case UARTDEF_BTRX_ARTIST:
    case UARTDEF_BTRX_ALBUM:
      event = MODIF_BTRX_EVENT_MEDIA_META_UPDATE;
      break;
    case UARTDEF_BTRX_DEVICE_ADDR:
    case UARTDEF_BTRX_DEVICE_NAME:
      event = MODIF_BTRX_EVENT_DEVICE_UPDATE;
      break;
    case UARTDEF_BTRX_CONN_STATS:
      event = MODIF_BTRX_EVENT_CONN_STATS_UPDATE;
      break;
    case UARTDEF_BTRX_CODEC:
      event = MODIF_BTRX_EVENT_CODEC_UPDATE;
      break;
    default:
      return;
  }

  this->ExecuteCallbacks(event);
}

