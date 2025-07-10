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

uint16_t ModuleInterface::ReadRegister16(uint8_t reg_addr) {
  uint16_t value;

  this->ReadRegister(reg_addr, (uint8_t*)&value, 2);

  return value;
}

uint32_t ModuleInterface::ReadRegister32(uint8_t reg_addr) {
  uint32_t value;

  this->ReadRegister(reg_addr, (uint8_t*)&value, 4);

  return value;
}


//helper function for queueing variable length reads or writes
static inline void _ModuleInterface_QueueVarTransfer(std::deque<ModuleTransferQueueItem*>& queue, ModuleTransferQueueItem* new_transfer, ModuleTransferType type,
                                                     uint8_t reg_addr, uint8_t* buf, uint16_t length, ModuleTransferCallback&& callback) {
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
  new_transfer->callback = std::move(callback);
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
                                                       uint8_t reg_addr, uint32_t value, uint16_t length, ModuleTransferCallback&& callback) {
  //configure transfer for the short read (reading into the internal value buffer) or write (copy value into the internal value buffer, write from there)
  new_transfer->type = type;
  new_transfer->reg_addr = reg_addr;
  new_transfer->data_ptr = (uint8_t*)&new_transfer->value_buffer;
  new_transfer->length = length;
  new_transfer->value_buffer = value;
  new_transfer->success = false;
  new_transfer->callback = std::move(callback);
  //add transfer to queue
  try {
    queue.push_back(new_transfer);
  } catch (...) {
    delete new_transfer;
    throw;
  }
}

void ModuleInterface::ReadRegisterAsync(uint8_t reg_addr, uint8_t* buf, uint16_t length, ModuleTransferCallback&& callback) {
  _ModuleInterface_QueueVarTransfer(this->queued_transfers, this->CreateTransferQueueItem(), TF_READ, reg_addr, buf, length, std::move(callback));
  this->StartQueuedAsyncTransfer();
}

void ModuleInterface::ReadRegister8Async(uint8_t reg_addr, ModuleTransferCallback&& callback) {
  _ModuleInterface_QueueShortTransfer(this->queued_transfers, this->CreateTransferQueueItem(), TF_READ, reg_addr, 0, 1, std::move(callback));
  this->StartQueuedAsyncTransfer();
}

void ModuleInterface::ReadRegister16Async(uint8_t reg_addr, ModuleTransferCallback&& callback) {
  _ModuleInterface_QueueShortTransfer(this->queued_transfers, this->CreateTransferQueueItem(), TF_READ, reg_addr, 0, 2, std::move(callback));
  this->StartQueuedAsyncTransfer();
}

void ModuleInterface::ReadRegister32Async(uint8_t reg_addr, ModuleTransferCallback&& callback) {
  _ModuleInterface_QueueShortTransfer(this->queued_transfers, this->CreateTransferQueueItem(), TF_READ, reg_addr, 0, 4, std::move(callback));
  this->StartQueuedAsyncTransfer();
}


void ModuleInterface::WriteRegister8(uint8_t reg_addr, uint8_t value) {
  this->WriteRegister(reg_addr, &value, 1);
}

void ModuleInterface::WriteRegister16(uint8_t reg_addr, uint16_t value) {
  this->WriteRegister(reg_addr, (uint8_t*)&value, 2);
}

void ModuleInterface::WriteRegister32(uint8_t reg_addr, uint32_t value) {
  this->WriteRegister(reg_addr, (uint8_t*)&value, 4);
}


void ModuleInterface::WriteRegisterAsync(uint8_t reg_addr, const uint8_t* buf, uint16_t length, ModuleTransferCallback&& callback) {
  _ModuleInterface_QueueVarTransfer(this->queued_transfers, this->CreateTransferQueueItem(), TF_WRITE, reg_addr, (uint8_t*)buf, length, std::move(callback));
  this->StartQueuedAsyncTransfer();
}

void ModuleInterface::WriteRegister8Async(uint8_t reg_addr, uint8_t value, ModuleTransferCallback&& callback) {
  _ModuleInterface_QueueShortTransfer(this->queued_transfers, this->CreateTransferQueueItem(), TF_WRITE, reg_addr, (uint32_t)value, 1, std::move(callback));
  this->StartQueuedAsyncTransfer();
}

void ModuleInterface::WriteRegister16Async(uint8_t reg_addr, uint16_t value, ModuleTransferCallback&& callback) {
  _ModuleInterface_QueueShortTransfer(this->queued_transfers, this->CreateTransferQueueItem(), TF_WRITE, reg_addr, (uint32_t)value, 2, std::move(callback));
  this->StartQueuedAsyncTransfer();
}

void ModuleInterface::WriteRegister32Async(uint8_t reg_addr, uint32_t value, ModuleTransferCallback&& callback) {
  _ModuleInterface_QueueShortTransfer(this->queued_transfers, this->CreateTransferQueueItem(), TF_WRITE, reg_addr, value, 4, std::move(callback));
  this->StartQueuedAsyncTransfer();
}


//registers the given callback with the given event mask and identifier. identifier 0 is "unidentifiable". if the given (non-zero) identifier already exists, the old callback is replaced.
void ModuleInterface::RegisterCallback(ModuleEventCallback&& cb, uint32_t event_mask, uint64_t identifier) {
  if (!cb || event_mask == 0) {
    throw std::invalid_argument("ModuleInterface RegisterCallback requires a non-empty function and a non-zero event mask");
  }

  //iterate through existing registrations to see if this identifier is already registered
  if (identifier != 0) {
    for (auto& reg : this->registered_callbacks) {
      if (reg.identifier == identifier) {
        //function already registered: replace old callback and event mask with new ones
        reg.func = std::move(cb);
        reg.event_mask = event_mask;
        return;
      }
    }
  }

  //new function: add registration for it
  auto& new_reg = this->registered_callbacks.emplace_back();
  new_reg.func = std::move(cb);
  new_reg.event_mask = event_mask;
  new_reg.identifier = identifier;
}

void ModuleInterface::UnregisterCallback(uint64_t identifier) {
  if (identifier == 0) {
    throw std::invalid_argument("ModuleInterface UnregisterCallback only works for nonzero identifiers");
  }

  //iterate through existing registrations to find the function with the given identifier
  for (auto i = this->registered_callbacks.begin(); i < this->registered_callbacks.end(); i++) {
    auto& reg = *i;
    if (reg.identifier == identifier) {
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

    if (transfer->callback) {
      try {
        transfer->callback(transfer->success, transfer->value_buffer, transfer->length);
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
