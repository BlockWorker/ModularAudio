/*
 * module_interface.h
 *
 *  Created on: May 8, 2025
 *      Author: Alex
 */

#ifndef INC_MODULE_INTERFACE_H_
#define INC_MODULE_INTERFACE_H_

#include "cpp_main.h"
#include "event_source.h"

//common constants
//blocking operation timeout
#define MODIF_BLOCKING_TIMEOUT_MS 20
//common interface events
#define MODIF_EVENT_INTERRUPT (1u << 0)
#define MODIF_EVENT_ERROR (1u << 1)
#define MODIF_EVENT_REGISTER_UPDATE (1u << 2)
#define MODIF_EVENT_MODULE_RESET (1u << 3)


#ifdef __cplusplus

#include <vector>
#include <deque>

extern "C" {
#endif

typedef enum {
  IF_EXTI,
  IF_TX_COMPLETE,
  IF_RX_COMPLETE,
  IF_ERROR
} ModuleInterfaceInterruptType;

#ifdef __cplusplus
}


//transfer types
typedef enum {
  TF_READ = 0x1,
  TF_WRITE = 0x2
} ModuleTransferType;


//callback type for module register transfers - arguments: success, value where applicable, length in bytes
typedef std::function<void(bool, uint32_t, uint16_t)> ModuleTransferCallback;

//macro for converting a success-or-fail callback reference to a transfer callback (which discards everything except for the success bool)
#define SuccessToTransferCallback(cb) ((cb) ? [cb = std::move(cb)](bool success, uint32_t, uint16_t) { cb(success); } : ModuleTransferCallback())


class ModuleTransferQueueItem {
public:
  ModuleTransferType type;

  uint16_t reg_addr;
  uint8_t* data_ptr;
  uint16_t length;

  uint32_t value_buffer;
  bool success;

  ModuleTransferCallback callback;

  virtual ~ModuleTransferQueueItem() = default;
};


//abstract interface for interacting with other system modules
class ModuleInterface : public EventSource {
public:
  virtual void ReadRegister(uint16_t reg_addr, uint8_t* buf, uint16_t length) = 0;
  uint8_t ReadRegister8(uint16_t reg_addr);
  uint16_t ReadRegister16(uint16_t reg_addr);
  uint32_t ReadRegister32(uint16_t reg_addr);

  void ReadRegisterAsync(uint16_t reg_addr, uint8_t* buf, uint16_t length, ModuleTransferCallback&& callback);
  void ReadRegister8Async(uint16_t reg_addr, ModuleTransferCallback&& callback);
  void ReadRegister16Async(uint16_t reg_addr, ModuleTransferCallback&& callback);
  void ReadRegister32Async(uint16_t reg_addr, ModuleTransferCallback&& callback);

  virtual void WriteRegister(uint16_t reg_addr, const uint8_t* buf, uint16_t length) = 0;
  void WriteRegister8(uint16_t reg_addr, uint8_t value);
  void WriteRegister16(uint16_t reg_addr, uint16_t value);
  void WriteRegister32(uint16_t reg_addr, uint32_t value);

  void WriteRegisterAsync(uint16_t reg_addr, const uint8_t* buf, uint16_t length, ModuleTransferCallback&& callback);
  void WriteRegister8Async(uint16_t reg_addr, uint8_t value, ModuleTransferCallback&& callback);
  void WriteRegister16Async(uint16_t reg_addr, uint16_t value, ModuleTransferCallback&& callback);
  void WriteRegister32Async(uint16_t reg_addr, uint32_t value, ModuleTransferCallback&& callback);

  virtual void HandleInterrupt(ModuleInterfaceInterruptType type, uint16_t extra) noexcept = 0;

  virtual void Init();
  virtual void LoopTasks();

  virtual ~ModuleInterface();

protected:
  std::deque<ModuleTransferQueueItem*> queued_transfers;
  std::vector<ModuleTransferQueueItem*> completed_transfers;
  bool async_transfer_active = false;

  virtual ModuleTransferQueueItem* CreateTransferQueueItem();
  virtual void StartQueuedAsyncTransfer() noexcept = 0;
};


#endif


#endif /* INC_MODULE_INTERFACE_H_ */
