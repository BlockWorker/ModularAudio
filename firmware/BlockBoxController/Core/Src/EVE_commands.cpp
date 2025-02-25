/*
 * EVE_commands.cpp
 *
 *  Created on: Feb 25, 2025
 *      Author: Alex
 */

#include "EVE.h"


/* ##################################################################
    helper functions
##################################################################### */

HAL_StatusTypeDef EVE_Driver::IsBusy(uint8_t* p_result) {
  if (p_result == NULL) {
    return HAL_ERROR;
  }

  uint16_t space;
  *p_result = EVE_IS_BUSY;

  ReturnOnError(phy.DirectRead16(REG_CMDB_SPACE, &space));

  // (REG_CMDB_SPACE & 0x03) != 0 -> we have a coprocessor fault
  if ((space & 3) != 0) {
    //we have a coprocessor fault, make EVE play with us again
    *p_result = EVE_FAULT_RECOVERED;
    this->fault_recovered = EVE_FAULT_RECOVERED; /* save fault recovery state */
    return this->CoprocessorFaultRecover();
  } else {
    if (space == 0xFFC) {
      *p_result = E_OK;
    } else if (space > 0x800) {
      *p_result = EVE_FIFO_HALF_EMPTY;
    } else {
      *p_result = EVE_IS_BUSY;
    }
  }

  return HAL_OK;
}

HAL_StatusTypeDef EVE_Driver::WaitUntilNotBusy(uint32_t timeout) {
  uint8_t busy;

  if (timeout == HAL_MAX_DELAY) {
    do {
      ReturnOnError(this->IsBusy(&busy));
    } while (busy != E_OK);
  } else {
    uint32_t start_tick = HAL_GetTick();

    do {
      ReturnOnError(this->IsBusy(&busy));

      if ((HAL_GetTick() - start_tick) >= timeout) {
        return HAL_TIMEOUT;
      }
    } while (busy != E_OK);
  }

  return HAL_OK;
}

uint8_t EVE_Driver::GetAndResetFaultState() {
  uint8_t ret = E_OK;

  if (fault_recovered == EVE_FAULT_RECOVERED) {
    ret = EVE_FAULT_RECOVERED;
    fault_recovered = E_OK;
  }

  return ret;
}

void EVE_Driver::ClearDLCmdBuffer() {
  this->dl_cmd_buffer.clear();
  this->in_display_list = false;
}

HAL_StatusTypeDef EVE_Driver::SendBufferedDLCmds() {
  if (!this->in_display_list) {
    //not in display-list mode
    return HAL_ERROR;
  }

  //send commands as block transfer and clear afterwards
  ReturnOnError(this->SendCmdBlockTransfer((const uint8_t*)this->dl_cmd_buffer.data(), this->dl_cmd_buffer.size()));
  this->ClearDLCmdBuffer();

  return HAL_OK;
}

bool EVE_Driver::IsInDisplayList() {
  return this->in_display_list;
}


HAL_StatusTypeDef EVE_Driver::CoprocessorFaultRecover() {
  //use mmap mode for the recovery
  ReturnOnError(this->phy.EnsureMMapMode(MMAP_FUNC_RAM));

  MMAP_REG_CPURESET = 1U; //hold coprocessor engine in the reset condition
  MMAP_REG_CMD_READ = 0U; //reset command FIFO positions
  MMAP_REG_CMD_WRITE = 0U;
  MMAP_REG_CMD_DL = 0U; //reset REG_CMD_DL to 0 as required by the BT81x programming guide, should not hurt FT8xx

  MMAP_REG_PCLK = EVE_PCLK; //restore REG_PCLK in case it was set to zero by an error

  MMAP_REG_CPURESET = 0U; //set REG_CPURESET to 0 to restart the coprocessor engine

  ReturnOnError(this->phy.EndMMap());

  HAL_Delay(10); //just to be safe
  return HAL_OK;
}

HAL_StatusTypeDef EVE_Driver::SendCmdBlockTransfer(const uint8_t* p_data, uint32_t length) {
  if (p_data == NULL) {
    return HAL_ERROR;
  }
  if (length == 0) {
    return HAL_OK;
  }

  uint32_t bytes_left = length;
  uint32_t offset = 0;

  while (bytes_left > 0) {
    uint32_t block_len = MIN(bytes_left, 3840);

    ReturnOnError(this->phy.DirectWriteBuffer(REG_CMDB_WRITE, p_data + offset, block_len, 10));
    offset += block_len;
    bytes_left -= block_len;
    ReturnOnError(this->WaitUntilNotBusy(10));
  }

  return HAL_OK;
}


/* ##################################################################
    commands and functions to be used outside of display-lists
##################################################################### */

HAL_StatusTypeDef EVE_Driver::CmdGetProps(uint32_t *p_pointer, uint32_t *p_width, uint32_t *p_height) {
  if (this->in_display_list) {
    //should be used outside of display list building
    return HAL_ERROR;
  }

  const uint32_t cmd[4] = { CMD_GETPROPS, 0, 0, 0 };

  //write command and wait for completion
  ReturnOnError(this->phy.DirectWriteBuffer(REG_CMDB_WRITE, (const uint8_t*)cmd, 16, EVE_PHY_SMALL_TRANSFER_TIMEOUT));
  ReturnOnError(this->WaitUntilNotBusy(10));

  //get results using mmap mode
  ReturnOnError(this->phy.EnsureMMapMode(MMAP_FUNC_RAM));

  uint16_t cmdoffset = MMAP_REG_CMD_WRITE;

  if (p_pointer != NULL) {
    *p_pointer = *(EVE_MMAP_RAM_CMD_PTR + ((cmdoffset - 12U) & 0xfffU));
  }
  if (p_width != NULL) {
    *p_width = *(EVE_MMAP_RAM_CMD_PTR + ((cmdoffset - 8U) & 0xfffU));
  }
  if (p_height != NULL) {
    *p_height = *(EVE_MMAP_RAM_CMD_PTR + ((cmdoffset - 4U) & 0xfffU));
  }

  return this->phy.EndMMap();
}

HAL_StatusTypeDef EVE_Driver::CmdGetPtr(uint32_t* p_pointer) {
  if (this->in_display_list) {
    //should be used outside of display list building
    return HAL_ERROR;
  }

  if (p_pointer == NULL) {
    return HAL_ERROR;
  }

  const uint32_t cmd[2] = { CMD_GETPTR, 0 };

  //write command and wait for completion
  ReturnOnError(this->phy.DirectWriteBuffer(REG_CMDB_WRITE, (const uint8_t*)cmd, 8, EVE_PHY_SMALL_TRANSFER_TIMEOUT));
  ReturnOnError(this->WaitUntilNotBusy(10));

  //get result
  uint16_t cmdoffset;
  ReturnOnError(this->phy.DirectRead16(REG_CMD_WRITE, &cmdoffset));
  return this->phy.DirectRead32(EVE_RAM_CMD + ((cmdoffset - 4U) & 0xfffU), p_pointer);
}

HAL_StatusTypeDef EVE_Driver::CmdInflate(uint32_t ptr, const uint8_t *p_data, uint32_t len) {
  if (this->in_display_list) {
    //should be used outside of display list building
    return HAL_ERROR;
  }

  if (p_data == NULL) {
    return HAL_ERROR;
  }

  uint32_t cmd[2] = { CMD_INFLATE, ptr };

  //write command and data as block transfer
  ReturnOnError(this->phy.DirectWriteBuffer(REG_CMDB_WRITE, (const uint8_t*)cmd, 8, EVE_PHY_SMALL_TRANSFER_TIMEOUT));
  return this->SendCmdBlockTransfer(p_data, len);
}

HAL_StatusTypeDef EVE_Driver::CmdInterrupt(uint32_t msec) {
  if (this->in_display_list) {
    //should be used outside of display list building
    return HAL_ERROR;
  }

  uint32_t cmd[2] = { CMD_INTERRUPT, msec };

  //write command and wait for completion
  ReturnOnError(this->phy.DirectWriteBuffer(REG_CMDB_WRITE, (const uint8_t*)cmd, 8, EVE_PHY_SMALL_TRANSFER_TIMEOUT));
  return this->WaitUntilNotBusy(10); //TODO: do we need to wait at least `msec`? probably not?
}

HAL_StatusTypeDef EVE_Driver::CmdLoadImage(uint32_t ptr, uint32_t options, const uint8_t *p_data, uint32_t len) {
  if (this->in_display_list) {
    //should be used outside of display list building
    return HAL_ERROR;
  }

  uint32_t cmd[3] = { CMD_LOADIMAGE, ptr, options };

  //write command and data as block transfer (unless mediafifo is used)
  ReturnOnError(this->phy.DirectWriteBuffer(REG_CMDB_WRITE, (const uint8_t*)cmd, 12, EVE_PHY_SMALL_TRANSFER_TIMEOUT));
  if ((options & EVE_OPT_MEDIAFIFO) == 0) {
    if (p_data == NULL) {
      return HAL_ERROR;
    }
    return this->SendCmdBlockTransfer(p_data, len);
  } else {
    return HAL_OK;
  }
}

HAL_StatusTypeDef EVE_Driver::CmdMediaFifo(uint32_t ptr, uint32_t size) {
  if (this->in_display_list) {
    //should be used outside of display list building
    return HAL_ERROR;
  }

  uint32_t cmd[3] = { CMD_MEDIAFIFO, ptr, size };

  //write command and wait for completion
  ReturnOnError(this->phy.DirectWriteBuffer(REG_CMDB_WRITE, (const uint8_t*)cmd, 12, EVE_PHY_SMALL_TRANSFER_TIMEOUT));
  return this->WaitUntilNotBusy(10);
}

HAL_StatusTypeDef EVE_Driver::CmdMemcpy(uint32_t dest, uint32_t src, uint32_t num) {
  if (this->in_display_list) {
    //should be used outside of display list building
    return HAL_ERROR;
  }

  uint32_t cmd[4] = { CMD_MEMCPY, dest, src, num };

  //write command and wait for completion
  ReturnOnError(this->phy.DirectWriteBuffer(REG_CMDB_WRITE, (const uint8_t*)cmd, 16, EVE_PHY_SMALL_TRANSFER_TIMEOUT));
  return this->WaitUntilNotBusy(10);
}

HAL_StatusTypeDef EVE_Driver::CmdMemcrc(uint32_t ptr, uint32_t num, uint32_t* p_crc) {
  if (this->in_display_list) {
    //should be used outside of display list building
    return HAL_ERROR;
  }

  if (p_crc == NULL) {
    return HAL_ERROR;
  }

  const uint32_t cmd[4] = { CMD_MEMCRC, ptr, num, 0 };

  //write command and wait for completion
  ReturnOnError(this->phy.DirectWriteBuffer(REG_CMDB_WRITE, (const uint8_t*)cmd, 16, EVE_PHY_SMALL_TRANSFER_TIMEOUT));
  ReturnOnError(this->WaitUntilNotBusy(10));

  //get result
  uint16_t cmdoffset;
  ReturnOnError(this->phy.DirectRead16(REG_CMD_WRITE, &cmdoffset));
  return this->phy.DirectRead32(EVE_RAM_CMD + ((cmdoffset - 4U) & 0xfffU), p_crc);
}

HAL_StatusTypeDef EVE_Driver::CmdMemset(uint32_t ptr, uint8_t value, uint32_t num) {
  if (this->in_display_list) {
    //should be used outside of display list building
    return HAL_ERROR;
  }

  uint32_t cmd[4] = { CMD_MEMSET, ptr, (uint32_t)value, num };

  //write command and wait for completion
  ReturnOnError(this->phy.DirectWriteBuffer(REG_CMDB_WRITE, (const uint8_t*)cmd, 16, EVE_PHY_SMALL_TRANSFER_TIMEOUT));
  return this->WaitUntilNotBusy(10);
}

HAL_StatusTypeDef EVE_Driver::CmdMemzero(uint32_t ptr, uint32_t num) {
  if (this->in_display_list) {
    //should be used outside of display list building
    return HAL_ERROR;
  }

  uint32_t cmd[3] = { CMD_MEMZERO, ptr, num };

  //write command and wait for completion
  ReturnOnError(this->phy.DirectWriteBuffer(REG_CMDB_WRITE, (const uint8_t*)cmd, 12, EVE_PHY_SMALL_TRANSFER_TIMEOUT));
  return this->WaitUntilNotBusy(10);
}

HAL_StatusTypeDef EVE_Driver::CmdPlayVideo(uint32_t options, const uint8_t *p_data, uint32_t len) {
  if (this->in_display_list) {
    //should be used outside of display list building
    return HAL_ERROR;
  }

  uint32_t cmd[2] = { CMD_PLAYVIDEO, options };

  //write command and data as block transfer (unless mediafifo is used)
  ReturnOnError(this->phy.DirectWriteBuffer(REG_CMDB_WRITE, (const uint8_t*)cmd, 8, EVE_PHY_SMALL_TRANSFER_TIMEOUT));
  if ((options & EVE_OPT_MEDIAFIFO) == 0) {
    if (p_data == NULL) {
      return HAL_ERROR;
    }
    return this->SendCmdBlockTransfer(p_data, len);
  } else {
    return HAL_OK;
  }
}

HAL_StatusTypeDef EVE_Driver::CmdSetRotate(uint32_t rotation) {
  if (this->in_display_list) {
    //should be used outside of display list building
    return HAL_ERROR;
  }

  uint32_t cmd[2] = { CMD_SETROTATE, rotation };

  //write command and wait for completion
  ReturnOnError(this->phy.DirectWriteBuffer(REG_CMDB_WRITE, (const uint8_t*)cmd, 8, EVE_PHY_SMALL_TRANSFER_TIMEOUT));
  return this->WaitUntilNotBusy(10);
}

HAL_StatusTypeDef EVE_Driver::CmdSnapshot(uint32_t ptr) {
  if (this->in_display_list) {
    //should be used outside of display list building
    return HAL_ERROR;
  }

  uint32_t cmd[2] = { CMD_SNAPSHOT, ptr };

  //write command and wait for completion
  ReturnOnError(this->phy.DirectWriteBuffer(REG_CMDB_WRITE, (const uint8_t*)cmd, 8, EVE_PHY_SMALL_TRANSFER_TIMEOUT));
  return this->WaitUntilNotBusy(10);
}

HAL_StatusTypeDef EVE_Driver::CmdSnapshot2(uint32_t fmt, uint32_t ptr, int16_t xc0, int16_t yc0, uint16_t wid, uint16_t hgt) {
  if (this->in_display_list) {
    //should be used outside of display list building
    return HAL_ERROR;
  }

  uint32_t cmd[5] = {
      CMD_SNAPSHOT2, fmt, ptr,
      (uint32_t)xc0 | (((uint32_t)yc0) << 16U),
      (uint32_t)wid | (((uint32_t)hgt) << 16U)
  };

  //write command and wait for completion
  ReturnOnError(this->phy.DirectWriteBuffer(REG_CMDB_WRITE, (const uint8_t*)cmd, 20, EVE_PHY_SMALL_TRANSFER_TIMEOUT));
  return this->WaitUntilNotBusy(10);
}

HAL_StatusTypeDef EVE_Driver::CmdTrack(int16_t xc0, int16_t yc0, uint16_t wid, uint16_t hgt, uint16_t tag) {
  if (this->in_display_list) {
    //should be used outside of display list building
    return HAL_ERROR;
  }

  uint32_t cmd[4] = {
      CMD_TRACK,
      (uint32_t)xc0 | (((uint32_t)yc0) << 16U),
      (uint32_t)wid | (((uint32_t)hgt) << 16U),
      (uint32_t)tag
  };

  //write command and wait for completion
  ReturnOnError(this->phy.DirectWriteBuffer(REG_CMDB_WRITE, (const uint8_t*)cmd, 16, EVE_PHY_SMALL_TRANSFER_TIMEOUT));
  return this->WaitUntilNotBusy(10);
}

HAL_StatusTypeDef EVE_Driver::CmdVideoFrame(uint32_t dest, uint32_t result_ptr) {
  if (this->in_display_list) {
    //should be used outside of display list building
    return HAL_ERROR;
  }

  uint32_t cmd[3] = { CMD_VIDEOFRAME, dest, result_ptr };

  //write command and wait for completion
  ReturnOnError(this->phy.DirectWriteBuffer(REG_CMDB_WRITE, (const uint8_t*)cmd, 12, EVE_PHY_SMALL_TRANSFER_TIMEOUT));
  return this->WaitUntilNotBusy(10);
}


/* ##################################################################
    patching and initialization
##################################################################### */

HAL_StatusTypeDef EVE_Driver::WriteDisplayParameters() {

}

HAL_StatusTypeDef EVE_Driver::Init(uint8_t* p_result) {

}


HAL_StatusTypeDef EVE_Driver::WaitRegID() {

}

HAL_StatusTypeDef EVE_Driver::WaitReset() {

}

HAL_StatusTypeDef EVE_Driver::EnablePixelClock() {

}


/* ##################################################################
    functions for display lists
##################################################################### */

void EVE_Driver::CmdDL(uint32_t command) {

}


void EVE_Driver::CmdAppend(uint32_t ptr, uint32_t num) {

}

void EVE_Driver::CmdBGColor(uint32_t color) {

}

void EVE_Driver::CmdButton(int16_t xc0, int16_t yc0, uint16_t wid, uint16_t hgt, uint16_t font, uint16_t options, const char *p_text) {

}

void EVE_Driver::CmdCalibrate() {

}

void EVE_Driver::CmdClock(int16_t xc0, int16_t yc0, uint16_t rad, uint16_t options, uint16_t hours, uint16_t mins, uint16_t secs, uint16_t msecs) {

}

void EVE_Driver::CmdDial(int16_t xc0, int16_t yc0, uint16_t rad, uint16_t options, uint16_t val) {

}

void EVE_Driver::CmdFGColor(uint32_t color) {

}

void EVE_Driver::CmdGauge(int16_t xc0, int16_t yc0, uint16_t rad, uint16_t options, uint16_t major, uint16_t minor, uint16_t val, uint16_t range) {

}

void EVE_Driver::CmdGetMatrix(int32_t *p_a, int32_t *p_b, int32_t *p_c, int32_t *p_d, int32_t *p_e, int32_t *p_f) {

}

void EVE_Driver::CmdGradColor(uint32_t color) {

}

void EVE_Driver::CmdGradient(int16_t xc0, int16_t yc0, uint32_t rgb0, int16_t xc1, int16_t yc1, uint32_t rgb1) {

}

void EVE_Driver::CmdKeys(int16_t xc0, int16_t yc0, uint16_t wid, uint16_t hgt, uint16_t font, uint16_t options, const char *p_text) {

}

void EVE_Driver::CmdNumber(int16_t xc0, int16_t yc0, uint16_t font, uint16_t options, int32_t number) {

}

void EVE_Driver::CmdProgress(int16_t xc0, int16_t yc0, uint16_t wid, uint16_t hgt, uint16_t options, uint16_t val, uint16_t range) {

}

void EVE_Driver::CmdRomFont(uint32_t font, uint32_t romslot) {

}

void EVE_Driver::CmdRotate(uint32_t angle) {

}

void EVE_Driver::CmdScale(int32_t scx, int32_t scy) {

}

void EVE_Driver::CmdScrollBar(int16_t xc0, int16_t yc0, uint16_t wid, uint16_t hgt, uint16_t options, uint16_t val, uint16_t size, uint16_t range) {

}

void EVE_Driver::CmdSetBase(uint32_t base) {

}

void EVE_Driver::CmdSetBitmap(uint32_t addr, uint16_t fmt, uint16_t width, uint16_t height) {

}

void EVE_Driver::CmdSetFont(uint32_t font, uint32_t ptr) {

}

void EVE_Driver::CmdSetFont2(uint32_t font, uint32_t ptr, uint32_t firstchar) {

}

void EVE_Driver::CmdSetScratch(uint32_t handle) {

}

void EVE_Driver::CmdSketch(int16_t xc0, int16_t yc0, uint16_t wid, uint16_t hgt, uint32_t ptr, uint16_t format) {

}

void EVE_Driver::CmdSlider(int16_t xc0, int16_t yc0, uint16_t wid, uint16_t hgt, uint16_t options, uint16_t val, uint16_t range) {

}

void EVE_Driver::CmdSpinner(int16_t xc0, int16_t yc0, uint16_t style, uint16_t scale) {

}

void EVE_Driver::CmdText(int16_t xc0, int16_t yc0, uint16_t font, uint16_t options, const char *p_text) {

}

void EVE_Driver::CmdToggle(int16_t xc0, int16_t yc0, uint16_t wid, uint16_t font, uint16_t options, uint16_t state, const char *p_text) {

}

void EVE_Driver::CmdTranslate(int32_t tr_x, int32_t tr_y) {

}


void EVE_Driver::ColorRGB(uint32_t color) {

}

void EVE_Driver::ColorA(uint8_t alpha) {

}


void EVE_Driver::WriteString(const char *p_text) {
  const uint8_t* p_bytes = (const uint8_t*)p_text;

  if (!this->in_display_list || p_bytes == NULL) {
    return HAL_ERROR;
  }

  for (uint8_t textindex = 0U; textindex < 249U; textindex += 4U) {
    uint32_t calc = 0U;

    for (uint8_t index = 0U; index < 4U; index++) {
      uint8_t data = p_bytes[textindex + index];

      if (data == 0U) {
        this->dl_cmd_buffer.push_back(calc);
        return;
      }

      calc += ((uint32_t)data) << (index * 8U);
    }

    this->dl_cmd_buffer.push_back(calc);
  }

  this->dl_cmd_buffer.push_back(0); //ensure zero-termination when the line is too long
}


/* ##################################################################
    special purpose functions
##################################################################### */

HAL_StatusTypeDef EVE_Driver::CalibrateManual(uint16_t width, uint16_t height) {

}
