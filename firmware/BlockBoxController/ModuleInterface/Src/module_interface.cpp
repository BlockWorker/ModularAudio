/*
 * module_interface_common.cpp
 *
 *  Created on: May 9, 2025
 *      Author: Alex
 */

#include "module_interface.h"


uint8_t ModuleInterface::ReadRegister8(uint16_t reg_addr) {
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
                                                     uint16_t reg_addr, uint8_t* buf, uint16_t length, ModuleTransferCallback&& callback) {
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
                                                       uint16_t reg_addr, uint32_t value, uint16_t length, ModuleTransferCallback&& callback) {
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

void ModuleInterface::ReadRegisterAsync(uint16_t reg_addr, uint8_t* buf, uint16_t length, ModuleTransferCallback&& callback) {
  _ModuleInterface_QueueVarTransfer(this->queued_transfers, this->CreateTransferQueueItem(), TF_READ, reg_addr, buf, length, std::move(callback));
  this->StartQueuedAsyncTransfer();
}

void ModuleInterface::ReadRegister8Async(uint16_t reg_addr, ModuleTransferCallback&& callback) {
  _ModuleInterface_QueueShortTransfer(this->queued_transfers, this->CreateTransferQueueItem(), TF_READ, reg_addr, 0, 1, std::move(callback));
  this->StartQueuedAsyncTransfer();
}

void ModuleInterface::ReadRegister16Async(uint16_t reg_addr, ModuleTransferCallback&& callback) {
  _ModuleInterface_QueueShortTransfer(this->queued_transfers, this->CreateTransferQueueItem(), TF_READ, reg_addr, 0, 2, std::move(callback));
  this->StartQueuedAsyncTransfer();
}

void ModuleInterface::ReadRegister32Async(uint16_t reg_addr, ModuleTransferCallback&& callback) {
  _ModuleInterface_QueueShortTransfer(this->queued_transfers, this->CreateTransferQueueItem(), TF_READ, reg_addr, 0, 4, std::move(callback));
  this->StartQueuedAsyncTransfer();
}


void ModuleInterface::WriteRegister8(uint16_t reg_addr, uint8_t value) {
  this->WriteRegister(reg_addr, &value, 1);
}

void ModuleInterface::WriteRegister16(uint16_t reg_addr, uint16_t value) {
  this->WriteRegister(reg_addr, (uint8_t*)&value, 2);
}

void ModuleInterface::WriteRegister32(uint16_t reg_addr, uint32_t value) {
  this->WriteRegister(reg_addr, (uint8_t*)&value, 4);
}


void ModuleInterface::WriteRegisterAsync(uint16_t reg_addr, const uint8_t* buf, uint16_t length, ModuleTransferCallback&& callback) {
  _ModuleInterface_QueueVarTransfer(this->queued_transfers, this->CreateTransferQueueItem(), TF_WRITE, reg_addr, (uint8_t*)buf, length, std::move(callback));
  this->StartQueuedAsyncTransfer();
}

void ModuleInterface::WriteRegister8Async(uint16_t reg_addr, uint8_t value, ModuleTransferCallback&& callback) {
  _ModuleInterface_QueueShortTransfer(this->queued_transfers, this->CreateTransferQueueItem(), TF_WRITE, reg_addr, (uint32_t)value, 1, std::move(callback));
  this->StartQueuedAsyncTransfer();
}

void ModuleInterface::WriteRegister16Async(uint16_t reg_addr, uint16_t value, ModuleTransferCallback&& callback) {
  _ModuleInterface_QueueShortTransfer(this->queued_transfers, this->CreateTransferQueueItem(), TF_WRITE, reg_addr, (uint32_t)value, 2, std::move(callback));
  this->StartQueuedAsyncTransfer();
}

void ModuleInterface::WriteRegister32Async(uint16_t reg_addr, uint32_t value, ModuleTransferCallback&& callback) {
  _ModuleInterface_QueueShortTransfer(this->queued_transfers, this->CreateTransferQueueItem(), TF_WRITE, reg_addr, value, 4, std::move(callback));
  this->StartQueuedAsyncTransfer();
}


void ModuleInterface::Init() {
  //reset state

  //callback clearing disabled - I think we want them to stick around, to allow re-init without losing callbacks, as well as callback registration before init

  for (auto transfer : this->queued_transfers) {
    delete transfer;
  }
  this->queued_transfers.clear();

  for (auto transfer : this->completed_transfers) {
    delete transfer;
  }
  this->completed_transfers.clear();

  this->async_transfer_active = false;
}

void ModuleInterface::LoopTasks() {
  //vector to store copy of completed transfers
  static std::vector<ModuleTransferQueueItem*> completions;

  //make copy of completed async transfers to iterate through, under disabled interrupts to avoid modifications in the meantime
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  completions = this->completed_transfers;
  //clear completed transfers
  this->completed_transfers.clear();
  __set_PRIMASK(primask);

  //iterate through copied list, executing callbacks
  for (auto transfer : completions) {
    if (transfer->callback) {
      try {
        transfer->callback(transfer->success, transfer->value_buffer, transfer->length);
      } catch (const std::exception& exc) {
        DEBUG_PRINTF("* Exception in ModuleInterface transfer callback: %s\n", exc.what());
      } catch (...) {
        DEBUG_PRINTF("* Unknown exception in ModuleInterface transfer callback!\n");
      }
    }

    delete transfer;
  }

  //try to start the next async transfer if possible
  this->StartQueuedAsyncTransfer();
}


ModuleInterface::~ModuleInterface() {
  for (auto transfer : this->queued_transfers) {
    delete transfer;
  }

  for (auto transfer : this->completed_transfers) {
    delete transfer;
  }
}


ModuleTransferQueueItem* ModuleInterface::CreateTransferQueueItem() {
  return new ModuleTransferQueueItem;
}
