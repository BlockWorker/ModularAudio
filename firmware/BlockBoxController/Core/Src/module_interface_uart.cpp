/*
 * module_interface_uart.cpp
 *
 *  Created on: May 9, 2025
 *      Author: Alex
 */

#include "module_interface.h"


//timeout for async UART commands, in main loop cycles
#define UART_COMMAND_TIMEOUT_CYCLES (200 / MAIN_LOOP_PERIOD_MS)

//number of internal retries for failed UART transfers
#define UART_INTERNAL_RETRIES 3


//async transfer queue item, extended for UART-specific information
class UARTModuleTransferQueueItem : public ModuleTransferQueueItem {
public:
  uint8_t* add_buffer;

  uint16_t timeout_cycles;

  //uint16_t error_code;
  uint8_t retry_count;

  ~UARTModuleTransferQueueItem() override {
    if (this->add_buffer != NULL) {
      delete[] this->add_buffer;
    }
  }
};


//UART CRC data
static const uint16_t _uart_crc_table[256] = {
  0x0000, 0x1FB7, 0x3F6E, 0x20D9, 0x7EDC, 0x616B, 0x41B2, 0x5E05, 0xFDB8, 0xE20F, 0xC2D6, 0xDD61, 0x8364, 0x9CD3, 0xBC0A, 0xA3BD,
  0xE4C7, 0xFB70, 0xDBA9, 0xC41E, 0x9A1B, 0x85AC, 0xA575, 0xBAC2, 0x197F, 0x06C8, 0x2611, 0x39A6, 0x67A3, 0x7814, 0x58CD, 0x477A,
  0xD639, 0xC98E, 0xE957, 0xF6E0, 0xA8E5, 0xB752, 0x978B, 0x883C, 0x2B81, 0x3436, 0x14EF, 0x0B58, 0x555D, 0x4AEA, 0x6A33, 0x7584,
  0x32FE, 0x2D49, 0x0D90, 0x1227, 0x4C22, 0x5395, 0x734C, 0x6CFB, 0xCF46, 0xD0F1, 0xF028, 0xEF9F, 0xB19A, 0xAE2D, 0x8EF4, 0x9143,
  0xB3C5, 0xAC72, 0x8CAB, 0x931C, 0xCD19, 0xD2AE, 0xF277, 0xEDC0, 0x4E7D, 0x51CA, 0x7113, 0x6EA4, 0x30A1, 0x2F16, 0x0FCF, 0x1078,
  0x5702, 0x48B5, 0x686C, 0x77DB, 0x29DE, 0x3669, 0x16B0, 0x0907, 0xAABA, 0xB50D, 0x95D4, 0x8A63, 0xD466, 0xCBD1, 0xEB08, 0xF4BF,
  0x65FC, 0x7A4B, 0x5A92, 0x4525, 0x1B20, 0x0497, 0x244E, 0x3BF9, 0x9844, 0x87F3, 0xA72A, 0xB89D, 0xE698, 0xF92F, 0xD9F6, 0xC641,
  0x813B, 0x9E8C, 0xBE55, 0xA1E2, 0xFFE7, 0xE050, 0xC089, 0xDF3E, 0x7C83, 0x6334, 0x43ED, 0x5C5A, 0x025F, 0x1DE8, 0x3D31, 0x2286,
  0x783D, 0x678A, 0x4753, 0x58E4, 0x06E1, 0x1956, 0x398F, 0x2638, 0x8585, 0x9A32, 0xBAEB, 0xA55C, 0xFB59, 0xE4EE, 0xC437, 0xDB80,
  0x9CFA, 0x834D, 0xA394, 0xBC23, 0xE226, 0xFD91, 0xDD48, 0xC2FF, 0x6142, 0x7EF5, 0x5E2C, 0x419B, 0x1F9E, 0x0029, 0x20F0, 0x3F47,
  0xAE04, 0xB1B3, 0x916A, 0x8EDD, 0xD0D8, 0xCF6F, 0xEFB6, 0xF001, 0x53BC, 0x4C0B, 0x6CD2, 0x7365, 0x2D60, 0x32D7, 0x120E, 0x0DB9,
  0x4AC3, 0x5574, 0x75AD, 0x6A1A, 0x341F, 0x2BA8, 0x0B71, 0x14C6, 0xB77B, 0xA8CC, 0x8815, 0x97A2, 0xC9A7, 0xD610, 0xF6C9, 0xE97E,
  0xCBF8, 0xD44F, 0xF496, 0xEB21, 0xB524, 0xAA93, 0x8A4A, 0x95FD, 0x3640, 0x29F7, 0x092E, 0x1699, 0x489C, 0x572B, 0x77F2, 0x6845,
  0x2F3F, 0x3088, 0x1051, 0x0FE6, 0x51E3, 0x4E54, 0x6E8D, 0x713A, 0xD287, 0xCD30, 0xEDE9, 0xF25E, 0xAC5B, 0xB3EC, 0x9335, 0x8C82,
  0x1DC1, 0x0276, 0x22AF, 0x3D18, 0x631D, 0x7CAA, 0x5C73, 0x43C4, 0xE079, 0xFFCE, 0xDF17, 0xC0A0, 0x9EA5, 0x8112, 0xA1CB, 0xBE7C,
  0xF906, 0xE6B1, 0xC668, 0xD9DF, 0x87DA, 0x986D, 0xB8B4, 0xA703, 0x04BE, 0x1B09, 0x3BD0, 0x2467, 0x7A62, 0x65D5, 0x450C, 0x5ABB
};


//calculate CRC, starting with given crc state
static uint16_t _UART_CRC_Accumulate(uint8_t* buf, uint32_t length, uint16_t crc) {
  uint32_t i;
  for (i = 0; i < length; i++) {
    crc = ((crc << 8) ^ _uart_crc_table[buf[i] ^ (uint8_t)(crc >> 8)]);
  }
  return crc;
}


void UARTModuleInterface::ReadRegister(uint8_t reg_addr, uint8_t* buf, uint16_t length) {
  UNUSED(reg_addr);
  UNUSED(buf);
  UNUSED(length);
  throw std::logic_error("UARTModuleInterface does not support blocking transfers");
}

void UARTModuleInterface::WriteRegister(uint8_t reg_addr, const uint8_t* buf, uint16_t length) {
  UNUSED(reg_addr);
  UNUSED(buf);
  UNUSED(length);
  throw std::logic_error("UARTModuleInterface does not support blocking transfers");
}


void UARTModuleInterface::HandleInterrupt(ModuleInterfaceInterruptType type, uint16_t extra) noexcept {
  //perform interrupt handling under disabled interrupts
  uint32_t primask = __get_PRIMASK();
  __disable_irq();

  switch (type) {
    case IF_RX_COMPLETE:
      //advance receive buffer pointer
      this->rx_buffer_write_offset += (uint32_t)extra;
      if (this->rx_buffer_write_offset >= MODIF_UART_RXBUF_SIZE) {
        this->rx_buffer_write_offset = 0;
      }

      //start next reception immediately to avoid receiver overrun (data loss)
      //single transaction limited to remaining buffer space before wrap-around, because the HAL function has no "circular buffer" concept
      if (HAL_UARTEx_ReceiveToIdle_IT(this->uart_handle, this->rx_buffer + this->rx_buffer_write_offset, MODIF_UART_RXBUF_SIZE - this->rx_buffer_write_offset) == HAL_OK) {
        //success: we're done; otherwise we fall through to the error case
        break;
      }
    case IF_ERROR:
      try {
        this->Reset();
      } catch (const std::exception& exc) {
        DEBUG_PRINTF("*** UARTModuleInterface interrupt handler exception: %s\n", exc.what());
        this->interrupt_error = true;
        //TODO: log event, in both "catch" cases
      } catch (...) {
        DEBUG_PRINTF("*** UARTModuleInterface interrupt handler unknown exception\n");
        this->interrupt_error = true;
      }
      __set_PRIMASK(primask);
      this->ExecuteCallbacks(MODIF_EVENT_ERROR);
      return;
    case IF_TX_COMPLETE:
      //this->async_transfer_active = false;
      break;
    default:
      break;
  }

  __set_PRIMASK(primask);
}


void UARTModuleInterface::Init() {
  this->ModuleInterface::Init();

  this->Reset();
}

void UARTModuleInterface::LoopTasks() {
  if (this->interrupt_error) {
    this->Reset();
  }

  this->ProcessRawReceivedData();

  //transfer timeout handling: to be done under disabled interrupts
  uint32_t primask = __get_PRIMASK();
  __disable_irq();

  //decrement transfer timeout counters
  for (auto i = this->queued_transfers.begin(); i < this->queued_transfers.end(); i++) {
    auto transfer = (UARTModuleTransferQueueItem*)*i;
    if (transfer->timeout_cycles > 0) {
      transfer->timeout_cycles--;
    }
  }
  //check for transfer timeouts
  bool any_timed_out;
  do {
    any_timed_out = false;
    for (auto i = this->queued_transfers.begin(); i < this->queued_transfers.end(); i++) {
      auto transfer = (UARTModuleTransferQueueItem*)*i;
      if (transfer->timeout_cycles == 0) {
        //timed out: unsuccessful completion of transfer
        transfer->success = false;
        this->completed_transfers.push_back(transfer);
        //if we're at the front of the queue: async transfer no longer active (cancelled)
        if (i == this->queued_transfers.begin()) {
          this->async_transfer_active = false;
        }
        this->queued_transfers.erase(i);
        //erase invalidates iterators, so break out of inner loop and do another pass to check for more timeouts
        any_timed_out = true;
        break;
      }
    }
  } while (any_timed_out);

  __set_PRIMASK(primask);

  this->ModuleInterface::LoopTasks();
}


ModuleInterfaceUARTErrorType UARTModuleInterface::GetCurrentError() noexcept {
  return this->current_error;
}


UARTModuleInterface::UARTModuleInterface(UART_HandleTypeDef* uart_handle, bool use_crc) : uart_handle(uart_handle), uses_crc(use_crc) {
  if (uart_handle == NULL) {
    throw std::invalid_argument("UARTModuleInterface UART handle cannot be null");
  }
}


ModuleTransferQueueItem* UARTModuleInterface::CreateTransferQueueItem() {
  //initialise to empty defaults
  auto item = new UARTModuleTransferQueueItem;
  item->add_buffer = NULL;
  item->timeout_cycles = UART_COMMAND_TIMEOUT_CYCLES;
  //item->error_code = IF_UART_ERROR_UNKNOWN;
  item->retry_count = 0;
  return item;
}

void UARTModuleInterface::StartQueuedAsyncTransfer() noexcept {
  //perform start checks and handling under disabled interrupts
  uint32_t primask = __get_PRIMASK();
  __disable_irq();

  if (this->async_transfer_active || this->uart_handle->gState != HAL_UART_STATE_READY || this->queued_transfers.empty()) {
    //busy or nothing queued: don't do anything
    __set_PRIMASK(primask);
    return;
  }

  //whether the transfer should be retried if it failed
  bool retry = true;
  //whether a transfer failure should reset the I2C hardware
  bool reset_on_fail = false;

  auto transfer = (UARTModuleTransferQueueItem*)this->queued_transfers.front();

  //buffer for the un-encoded command to be sent
  std::vector<uint8_t> cmd_buf;
  cmd_buf.reserve(8);

  try {
    try {
      //start assembling the command itself
      switch (transfer->type) {
        case TF_READ:
          //read transfer: just consists of type and address
          cmd_buf.push_back(IF_UART_TYPE_READ);
          cmd_buf.push_back(transfer->reg_addr);
          break;
        case TF_WRITE:
          //write transfer: consists of type, address, and data
          cmd_buf.push_back(IF_UART_TYPE_WRITE);
          cmd_buf.push_back(transfer->reg_addr);
          cmd_buf.insert(cmd_buf.end(), transfer->data_ptr, transfer->data_ptr + transfer->length);
          break;
        default:
          retry = false;
          InlineFormat(throw std::runtime_error(__msg), "UARTModuleInterface async transfer with invalid type %u", transfer->type);
      }

      //append the CRC sum if enabled
      if (this->uses_crc) {
        uint16_t crc = _UART_CRC_Accumulate(cmd_buf.data(), cmd_buf.size(), 0);
        //use big-endian order so that CRC(data..crc) = 0
        cmd_buf.push_back((uint8_t)(crc >> 8));
        cmd_buf.push_back((uint8_t)crc);
      }

      //calculate true length of the encoded command, starting with current buffer size + start and end bytes
      uint16_t true_length = cmd_buf.size() + 2;
      //count number of bytes that need escaping
      for (uint8_t b : cmd_buf) {
        if (b == MODIF_UART_START_BYTE || b == MODIF_UART_END_BYTE || b == MODIF_UART_ESCAPE_BYTE) {
          true_length++;
        }
      }

      //allocate encoded command transmit buffer
      uint8_t* tx_buf = new uint8_t[true_length];

      //encode command into buffer, including start and end bytes, and escaping any data bytes that need it
      uint16_t tx_buf_pos = 0;
      tx_buf[tx_buf_pos++] = MODIF_UART_START_BYTE;
      for (uint8_t b : cmd_buf) {
        if (b == MODIF_UART_START_BYTE || b == MODIF_UART_END_BYTE || b == MODIF_UART_ESCAPE_BYTE) {
          tx_buf[tx_buf_pos++] = MODIF_UART_ESCAPE_BYTE;
        }
        tx_buf[tx_buf_pos++] = b;
      }
      tx_buf[tx_buf_pos] = MODIF_UART_END_BYTE;

      //start command transmission
      try {
        //reset if we encounter a hardware exception
        reset_on_fail = true;
        ThrowOnHALErrorMsg(HAL_UART_Transmit_IT(this->uart_handle, tx_buf, true_length), "UART transmit");
        transfer->add_buffer = tx_buf;
      } catch (...) {
        //on error: free transmit buffer before rethrowing to avoid memory leak
        delete[] tx_buf;
        throw;
      }

      this->async_transfer_active = true;
    } catch (const std::exception& exc) {
      //encountered "readable" exception: just print the message and rethrow for the outer handler
      DEBUG_PRINTF("* UARTModuleInterface async transfer start exception: %s\n", exc.what());
      throw;
    }
  } catch (...) {
    if (retry) {
      //failed with possibility of retry: increment retry counter, check if we're out of retries
      if (transfer->retry_count++ >= UART_INTERNAL_RETRIES) {
        //out of retries: don't retry again
        retry = false;
      }
    }

    if (!retry) {
      //failed without retry: put transfer into the completed list and remove it from the queue
      DEBUG_PRINTF("* UARTModuleInterface async transfer failed to start too many times to retry!\n");
      transfer->success = false;
      try {
        this->completed_transfers.push_back(transfer);
        this->queued_transfers.pop_front();
      } catch (...) {
        //list operation failed: force another retry (should be a very unlikely case)
        DEBUG_PRINTF("*** Retry forced due to exception when trying to mark the failed transfer as done!\n");
      }
    } else {
      DEBUG_PRINTF("UARTModuleInterface async transfer retrying on start\n");
    }

    if (reset_on_fail) {
      try {
        this->Reset();
      } catch (const std::exception& exc) {
        DEBUG_PRINTF("*** UARTModuleInterface failed to reset after fail: %s\n", exc.what());
      } catch (...) {
        DEBUG_PRINTF("*** UARTModuleInterface failed to reset after fail with unknown exception!\n");
      }
    }
  }

  __set_PRIMASK(primask);
}


void UARTModuleInterface::ProcessRawReceivedData() noexcept {
  //process one byte at a time, until we reach the write pointer
  while (this->rx_buffer_read_offset != this->rx_buffer_write_offset) {
    try {
      try {
        //read byte from receive buffer, decode link-layer format (escaping), transfer to parse buffer, and increment offsets
        uint8_t rx_byte = this->rx_buffer[this->rx_buffer_read_offset++];
        if (this->rx_escape_active) {
          //escape current byte: only allowed for reserved bytes
          if (rx_byte == MODIF_UART_START_BYTE || rx_byte == MODIF_UART_END_BYTE || rx_byte == MODIF_UART_ESCAPE_BYTE) {
            //reserved byte escaped: copy it to the parse buffer, unless we're skipping to the next start byte (because it should be ignored then)
            if (!this->rx_skip_to_start) {
              this->parse_buffer.push_back(rx_byte);
            }
          } else {
            //error: non-reserved byte escaped: reset parse buffer, skip to next start
            this->parse_buffer.clear();
            this->rx_skip_to_start = true;
            //TODO: maybe log event or something
          }
          this->rx_escape_active = false;
        } else {
          //not escaping: normal reception logic
          if (rx_byte == MODIF_UART_ESCAPE_BYTE) {
            //escape byte received: discard it and escape the next byte
            this->rx_escape_active = true;
          } else if (this->rx_skip_to_start) {
            //skipping to start: ignore all non-start bytes
            if (rx_byte == MODIF_UART_START_BYTE) {
              //start byte found: ready for actual reception starting with next byte
              this->parse_buffer.clear();
              this->rx_skip_to_start = false;
            }
          } else {
            //normal reception
            if (rx_byte == MODIF_UART_START_BYTE) {
              //error: unexpected start byte: reset parse buffer, receive new command starting with this byte
              this->parse_buffer.clear();
              //TODO: maybe log event or something
            } else if (rx_byte == MODIF_UART_END_BYTE) {
              //end byte found: parse buffer is done
              if (this->parse_buffer.size() < 3) {
                //error: parse buffer empty or too short (must have 1B type + 2B crc at least): skip to next start
                this->rx_skip_to_start = true;
                //TODO: maybe log event or something
              } else {
                //parse buffer has data: parse command, then reset parse buffer and skip to next start
                this->ParseRawNotification();
                this->parse_buffer.clear();
                //TODO: maybe shrink parse buffer if capacity above some threshold
                this->rx_skip_to_start = true;
              }
            } else {
              //non-reserved byte: copy it to the parse buffer
              this->parse_buffer.push_back(rx_byte);
            }
          }
        }
      } catch (const std::exception& exc) {
        //print message and rethrow to outer handler
        DEBUG_PRINTF("* UARTModuleInterface data reception processing exception: %s\n", exc.what());
        throw;
      }
    } catch (...) {
      //data processing or parsing failed: reset parse buffer, skip to next start
      this->parse_buffer.clear();
      this->parse_buffer.shrink_to_fit();
      this->rx_skip_to_start = true;
      //TODO: maybe log event or something
    }

    //handle circular receive buffer
    if (this->rx_buffer_read_offset >= MODIF_UART_RXBUF_SIZE) {
      this->rx_buffer_read_offset = 0;
    }
  }
}

void UARTModuleInterface::ParseRawNotification() {
  //perform parsing under disabled interrupts
  uint32_t primask = __get_PRIMASK();
  __disable_irq();

  uint32_t length = this->parse_buffer.size();

  //whether the notification is related to the current async transfer, assume no by default
  bool transfer_related = false;
  //whether to retry the current transfer if it failed
  bool retry_on_fail = true;

  //error encountered here, if any - default to unknown
  this->current_error = IF_UART_ERROR_UNKNOWN;
  //whether we have encountered any kind of error - default to yes
  bool error = true;

  //get the currently active async transfer, if there is one
  UARTModuleTransferQueueItem* transfer = NULL;
  if (this->async_transfer_active && !this->queued_transfers.empty()) {
    //we have an active transfer
    transfer = (UARTModuleTransferQueueItem*)this->queued_transfers.front();
    //assume transfer fail by default
    transfer->success = false;
  } else {
    //no active transfer
    this->async_transfer_active = false;
    transfer = NULL;
  }

  //how many length bytes to subtract for CRC
  uint32_t crc_length = (this->uses_crc ? 2 : 0);

  //perform CRC check if enabled
  bool crc_success = (!this->uses_crc || _UART_CRC_Accumulate(this->parse_buffer.data(), length, 0) == 0);

  if (crc_success) {
    //CRC check passed (or disabled): continue parsing
    switch (this->parse_buffer[0]) {
      case IF_UART_TYPE_EVENT:
        if (length - crc_length < 2) {
          //too short to be parsed as a known event, abort
          transfer_related = false;
          this->current_error = IF_UART_ERROR_UART_FORMAT_ERROR_LOCAL;
          error = true;
          break;
        }
        switch (this->parse_buffer[1]) {
          case IF_UART_EVENT_MCU_RESET:
            //TODO handle mcu resets if necessary
            transfer_related = false;
            error = false;
            break;
          case IF_UART_EVENT_WRITE_ACK:
            if (transfer == NULL) {
              //write acknowledgment despite no active transfer - nothing to do, but also not really an error? TODO: check if this should be an error after all
              transfer_related = false;
              error = false;
              break;
            }
            //write acknowledgment is definitely transfer-related
            transfer_related = true;
            if (length - crc_length < 3) {
              //too short to contain a register address: write transfer considered unsuccessful, but retry
              transfer->success = false;
              this->current_error = IF_UART_ERROR_UART_FORMAT_ERROR_LOCAL;
              error = true;
              retry_on_fail = true;
              break;
            }
            //if we have a write transfer, and its register address matches the acknowledgment, consider it successful
            transfer->success = (transfer->type == TF_WRITE && transfer->reg_addr == this->parse_buffer[2]);
            this->current_error = IF_UART_ERROR_UART_FORMAT_ERROR_LOCAL;
            //if unsuccessful, report an error, but retry
            error = !transfer->success;
            retry_on_fail = true;
            break;
          case IF_UART_EVENT_ERROR:
            error = true;
            if (length - crc_length < 4) {
              //too short to contain an error code: unknown error
              this->current_error = IF_UART_ERROR_UNKNOWN;
            } else {
              //get error code from event parameter
              this->current_error = (ModuleInterfaceUARTErrorType)((uint16_t)this->parse_buffer[2] | ((uint16_t)this->parse_buffer[3]) << 8);
            }
            //check whether the error is transfer-related and whether a retry should be attempted in that case
            transfer_related = (transfer != NULL && this->IsCommandError(&retry_on_fail));
            if (transfer_related) {
              transfer->success = false;
            }
            break;
          default:
            //unknown event type: assume no error, allow further handling in subclass
            transfer_related = false;
            error = false;
            break;
        }
        break;
      case IF_UART_TYPE_READ_DATA:
        if (transfer == NULL) {
          //read data despite no active transfer - nothing to do, but also not really an error? TODO: check if this should be an error after all, or treated as a change notification
          transfer_related = false;
          error = false;
          break;
        }
        //read data is always transfer-related
        transfer_related = true;
        if (length - crc_length < 3) {
          //too short to contain a register address and data (at least 1B): read transfer failed, allow retry
          transfer->success = false;
          this->current_error = IF_UART_ERROR_UART_FORMAT_ERROR_LOCAL;
          error = true;
          retry_on_fail = true;
          break;
        }
        if (transfer->type == TF_READ && transfer->reg_addr == this->parse_buffer[1]) {
          //we have a read transfer, and its register address matches the data: do a sanity check that we have a non-null destination buffer
          if (transfer->data_ptr != NULL) {
            //transfer successful, copy data to the destination buffer
            transfer->success = true;
            error = false;
            //if we received fewer bytes than requested, update that information in the transfer object
            if (length - crc_length - 2 < transfer->length) {
              transfer->length = length - crc_length - 2;
            }
            //copy whatever data we have, up to the requested number of bytes
            memcpy(transfer->data_ptr, this->parse_buffer.data() + 2, transfer->length);
          } else {
            //we somehow have a null destination pointer: transfer failed, do not retry (corrupted transfer object)
            transfer->success = false;
            this->current_error = IF_UART_ERROR_UART_FORMAT_ERROR_LOCAL;
            error = true;
            retry_on_fail = false;
          }
        } else {
          //wrong type or specification of transfer: fail but allow retry
          transfer->success = false;
          this->current_error = IF_UART_ERROR_UART_FORMAT_ERROR_LOCAL;
          error = true;
          retry_on_fail = true;
        }
        break;
      default:
        //unknown notification type: assume no error, allow further handling in subclass
        transfer_related = false;
        error = false;
        break;
    }
  } else {
    //CRC check failed: can't do anything with this data, discard it
    this->current_error = IF_UART_ERROR_UART_CRC_ERROR_LOCAL;
    transfer_related = false;
    error = true;
  }

  //do transfer handling if related
  if (transfer_related) {
    if (!transfer->success && retry_on_fail) {
      //failed with possibility of retry: increment retry counter, check if we're out of retries
      if (transfer->retry_count++ >= UART_INTERNAL_RETRIES) {
        //out of retries: don't retry again
        retry_on_fail = false;
        DEBUG_PRINTF("* UARTModuleInterface async transfer failed to complete too many times to retry!\n");
      } else {
        DEBUG_PRINTF("UARTModuleInterface async transfer retrying on completion\n");
      }
    }

    if (transfer->success || !retry_on_fail) {
      //successful, or failed without retry: put transfer into the completed list and remove it from the queue
      try {
        this->completed_transfers.push_back(transfer);
        this->queued_transfers.pop_front();
      } catch (...) {
        //list operation failed: force a retry (should be a very unlikely case)
        DEBUG_PRINTF("*** UARTModuleInterface retry forced due to exception when trying to finish a transfer!\n");
        //TODO: log event or something
      }
    }

    //async transfer is no longer active, ready for the next (or retry of current one)
    this->async_transfer_active = false;
    //start next transfer
    this->StartQueuedAsyncTransfer();
  }

  __set_PRIMASK(primask);

  if (error) {
    this->ExecuteCallbacks(MODIF_EVENT_ERROR);
  }

  this->HandleNotificationData(!transfer_related);
}

void UARTModuleInterface::HandleNotificationData(bool unsolicited) {
  //nothing to do in base class implementation
  UNUSED(unsolicited);
  //testing printout for debug - TODO: remove later
  DEBUG_PRINTF("UART notif: unsolicited %u, type %u, sub %u, length %u\n", unsolicited, this->parse_buffer[0], this->parse_buffer[1], this->parse_buffer.size());
}

//return value: whether the current error is attributable to a command
bool UARTModuleInterface::IsCommandError(bool* should_retry) noexcept {
  bool _should_retry = true;

  switch (this->current_error) {
    case IF_UART_ERROR_UART_FORMAT_ERROR_REMOTE:
    case IF_UART_ERROR_UART_COMMAND_NOT_ALLOWED:
      _should_retry = false;
    case IF_UART_ERROR_INTERNAL_UART_ERROR_REMOTE:
    case IF_UART_ERROR_UART_CRC_ERROR_REMOTE:
      break;
    default:
      return false;
  }

  if (should_retry != NULL) {
    *should_retry = _should_retry;
  }
  return true;
}


void UARTModuleInterface::Reset() {
  //perform reset under disabled interrupts
  uint32_t primask = __get_PRIMASK();
  __disable_irq();

  //restore UART peripheral to idle/ready
  HAL_UART_Abort(this->uart_handle);

  //reset all driver state
  this->rx_buffer_write_offset = 0;
  this->rx_buffer_read_offset = 0;
  this->rx_escape_active = false;
  this->rx_skip_to_start = true;
  this->async_transfer_active = false;
  this->current_error = IF_UART_ERROR_UNKNOWN;

  this->parse_buffer.clear();
  this->parse_buffer.shrink_to_fit();

  //start reception of notifications
  HAL_StatusTypeDef res = HAL_UARTEx_ReceiveToIdle_IT(this->uart_handle, this->rx_buffer + this->rx_buffer_write_offset, MODIF_UART_RXBUF_SIZE - this->rx_buffer_write_offset);

  if (res == HAL_OK) {
    this->interrupt_error = false;
  }

  __set_PRIMASK(primask);

  ThrowOnHALErrorMsg(res, "UART receive start");
}

