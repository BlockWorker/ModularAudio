/*
 * module_interface_common.cpp
 *
 *  Created on: May 9, 2025
 *      Author: Alex
 */

#include "module_interface.h"


uint8_t ModuleInterface::ReadRegister8(uint8_t reg_addr) {
  uint8_t value;

  this->ReadRegister(reg_addr, &value, 1);

  return value;
}

uint16_t ModuleInterface::ReadRegister16(uint16_t reg_addr) {
  uint16_t value;

  this->ReadRegister(reg_addr, (uint8_t*)&value, 2);

  return value;
}

uint32_t ModuleInterface::ReadRegister32(uint16_t reg_addr) {
  uint32_t value;

  this->ReadRegister(reg_addr, (uint8_t*)&value, 4);

  return value;
}


//helper function for queueing variable length reads or writes
static inline void _ModuleInterface_QueueVarTransfer(std::deque<ModuleTransferQueueItem*>& queue, ModuleTransferQueueItem* new_transfer, ModuleTransferType type,
                                                     uint8_t reg_addr, uint8_t* buf, uint16_t length, ModuleTransferCallback cb, uintptr_t context) {
  if (buf == NULL || length == 0) {
    delete new_transfer;
    throw std::invalid_argument("ModuleInterface transfers require non-null buffer and nonzero length");
  }

  //configure transfer for the read operation
  new_transfer->type = type;
  new_transfer->reg_addr = reg_addr;
  new_transfer->data_ptr = buf;
  new_transfer->length = length;
  new_transfer->value_buffer = 0;
  new_transfer->success = false;
  new_transfer->callback = cb;
  new_transfer->context = context;
  //add transfer to queue
  try {
    queue.push_back(new_transfer);
  } catch (...) {
    delete new_transfer;
    throw;
  }
}

//helper function for queueing 8/16/32-bit reads or writes
static inline void _ModuleInterface_QueueShortTransfer(std::deque<ModuleTransferQueueItem*>& queue, ModuleTransferQueueItem* new_transfer, ModuleTransferType type,
                                                       uint8_t reg_addr, uint32_t value, uint16_t length, ModuleTransferCallback cb, uintptr_t context) {
  //configure transfer for the short read (reading into the internal value buffer) or write (copy value into the internal value buffer, write from there)
  new_transfer->type = type;
  new_transfer->reg_addr = reg_addr;
  new_transfer->data_ptr = (uint8_t*)&new_transfer->value_buffer;
  new_transfer->length = length;
  new_transfer->value_buffer = value;
  new_transfer->success = false;
  new_transfer->callback = cb;
  new_transfer->context = context;
  //add transfer to queue
  try {
    queue.push_back(new_transfer);
  } catch (...) {
    delete new_transfer;
    throw;
  }
}

void ModuleInterface::ReadRegisterAsync(uint8_t reg_addr, uint8_t* buf, uint16_t length, ModuleTransferCallback cb, uintptr_t context) {
  _ModuleInterface_QueueVarTransfer(this->queued_transfers, this->CreateTransferQueueItem(), TF_READ, reg_addr, buf, length, cb, context);
  this->StartQueuedAsyncTransfer();
}

void ModuleInterface::ReadRegister8Async(uint8_t reg_addr, ModuleTransferCallback cb, uintptr_t context) {
  _ModuleInterface_QueueShortTransfer(this->queued_transfers, this->CreateTransferQueueItem(), TF_READ, reg_addr, 0, 1, cb, context);
  this->StartQueuedAsyncTransfer();
}

void ModuleInterface::ReadRegister16Async(uint8_t reg_addr, ModuleTransferCallback cb, uintptr_t context) {
  _ModuleInterface_QueueShortTransfer(this->queued_transfers, this->CreateTransferQueueItem(), TF_READ, reg_addr, 0, 2, cb, context);
  this->StartQueuedAsyncTransfer();
}

void ModuleInterface::ReadRegister32Async(uint8_t reg_addr, ModuleTransferCallback cb, uintptr_t context) {
  _ModuleInterface_QueueShortTransfer(this->queued_transfers, this->CreateTransferQueueItem(), TF_READ, reg_addr, 0, 4, cb, context);
  this->StartQueuedAsyncTransfer();
}


void ModuleInterface::WriteRegister8(uint8_t reg_addr, uint8_t value) {
  this->WriteRegister(reg_addr, &value, 1);
}

void ModuleInterface::WriteRegister16(uint16_t reg_addr, uint16_t value) {
  this->WriteRegister(reg_addr, (uint8_t*)&value, 2);
}

void ModuleInterface::WriteRegister32(uint16_t reg_addr, uint32_t value) {
  this->WriteRegister(reg_addr, (uint8_t*)&value, 4);
}


void ModuleInterface::WriteRegisterAsync(uint8_t reg_addr, const uint8_t* buf, uint16_t length, ModuleTransferCallback cb, uintptr_t context) {
  _ModuleInterface_QueueVarTransfer(this->queued_transfers, this->CreateTransferQueueItem(), TF_WRITE, reg_addr, (uint8_t*)buf, length, cb, context);
  this->StartQueuedAsyncTransfer();
}

void ModuleInterface::WriteRegister8Async(uint8_t reg_addr, uint8_t value, ModuleTransferCallback cb, uintptr_t context) {
  _ModuleInterface_QueueShortTransfer(this->queued_transfers, this->CreateTransferQueueItem(), TF_WRITE, reg_addr, (uint32_t)value, 1, cb, context);
  this->StartQueuedAsyncTransfer();
}

void ModuleInterface::WriteRegister16Async(uint16_t reg_addr, uint16_t value, ModuleTransferCallback cb, uintptr_t context) {
  _ModuleInterface_QueueShortTransfer(this->queued_transfers, this->CreateTransferQueueItem(), TF_WRITE, reg_addr, (uint32_t)value, 2, cb, context);
  this->StartQueuedAsyncTransfer();
}

void ModuleInterface::WriteRegister32Async(uint16_t reg_addr, uint32_t value, ModuleTransferCallback cb, uintptr_t context) {
  _ModuleInterface_QueueShortTransfer(this->queued_transfers, this->CreateTransferQueueItem(), TF_WRITE, reg_addr, value, 4, cb, context);
  this->StartQueuedAsyncTransfer();
}


void ModuleInterface::RegisterCallback(ModuleEventCallback cb, uint32_t event_mask) {
  if (cb == NULL || event_mask == 0) {
    throw std::invalid_argument("ModuleInterface RegisterCallback requires a non-null function and a non-zero event mask");
  }

  //iterate through existing registrations to see if this function is already registered
  for (auto i = this->registered_callbacks.begin(); i < this->registered_callbacks.end(); i++) {
    auto& reg = *i;
    if (reg.func == cb) {
      //function already registered: update its event mask and return, no need to re-register
      reg.event_mask = event_mask;
      return;
    }
  }

  //new function: add registration for it
  auto& new_reg = this->registered_callbacks.emplace_back();
  new_reg.func = cb;
  new_reg.event_mask = event_mask;
}

void ModuleInterface::UnregisterCallback(ModuleEventCallback cb) {
  //iterate through existing registrations to find the given function
  for (auto i = this->registered_callbacks.begin(); i < this->registered_callbacks.end(); i++) {
    auto& reg = *i;
    if (reg.func == cb) {
      //found: remove callback registration and return
      this->registered_callbacks.erase(i);
      return;
    }
  }
}


void ModuleInterface::Init() {
  //reset state
  this->registered_callbacks.clear();

  for (auto i = this->queued_transfers.begin(); i < this->queued_transfers.end(); i++) {
    delete *i;
  }
  this->queued_transfers.clear();

  for (auto i = this->completed_transfers.begin(); i < this->completed_transfers.end(); i++) {
    delete *i;
  }
  this->completed_transfers.clear();

  this->async_transfer_active = false;
}

void ModuleInterface::LoopTasks() {
  //iterate through completed async transfers, executing their callbacks
  for (auto i = this->completed_transfers.begin(); i < this->completed_transfers.end(); i++) {
    auto transfer = *i;

    if (transfer->callback != NULL) {
      try {
        transfer->callback(transfer->success, transfer->context, transfer->value_buffer, transfer->length);
      } catch (...) {
        //on error: erase successfully executed callbacks from completed list (all before current one), then rethrow
        this->completed_transfers.erase(this->completed_transfers.begin(), i);
        throw;
      }
    }

    delete transfer;
  }
  this->completed_transfers.clear();

  //try to start the next async transfer if possible
  this->StartQueuedAsyncTransfer();
}


ModuleInterface::~ModuleInterface() {
  for (auto i = this->queued_transfers.begin(); i < this->queued_transfers.end(); i++) {
    delete *i;
  }

  for (auto i = this->completed_transfers.begin(); i < this->completed_transfers.end(); i++) {
    delete *i;
  }
}


ModuleTransferQueueItem* ModuleInterface::CreateTransferQueueItem() {
  return new ModuleTransferQueueItem;
}


void ModuleInterface::ExecuteCallbacks(uint32_t event) noexcept {
  //iterate through callback registrations to find all that match the given event
  for (auto i = this->registered_callbacks.begin(); i < this->registered_callbacks.end(); i++) {
    auto& reg = *i;
    if ((reg.event_mask & event) != 0) {
      //event mask matches: call function (in exception-safe way)
      try {
        reg.func(*this, event);
      } catch (const std::exception& err) {
        DEBUG_PRINTF("* ModuleInterface callback exception: %s\n", err.what());
      } catch (...) {
        DEBUG_PRINTF("* Unknown ModuleInterface callback exception\n");
      }
    }
  }
}

