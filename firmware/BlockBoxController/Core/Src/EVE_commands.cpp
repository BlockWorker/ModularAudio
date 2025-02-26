/*
 * EVE_commands.cpp
 *
 *  Created on: Feb 25, 2025
 *      Author: Alex
 */

#include "EVE.h"


EVE_Driver eve_drv;


/* ##################################################################
    helper functions
##################################################################### */

/**
 * @brief Check if the coprocessor completed executing the current command list.
 * @return - E_OK - if EVE is not busy (no DMA transfer active and REG_CMDB_SPACE has the value 0xffc, meaning the CMD-FIFO is empty
 * @return - EVE_IS_BUSY - if a DMA transfer is active or REG_CMDB_SPACE has a value smaller than 0xffc
 * @return - EVE_FIFO_HALF_EMPTY - if no DMA transfer is active and REG_CMDB_SPACE shows more than 2048 bytes available
 * @return - E_NOT_OK - if there was a coprocessor fault and the recovery sequence was executed
 * @note - if there is a coprocessor fault the external flash is not reinitialized by EVE_busy()
 */
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

/**
 * @brief Helper function, wait for the coprocessor to complete the FIFO queue.
 */
HAL_StatusTypeDef EVE_Driver::WaitUntilNotBusy(uint32_t timeout) {
  uint8_t busy;

  uint32_t start_tick = HAL_GetTick();

  do {
    ReturnOnError(this->IsBusy(&busy));

    if (timeout != HAL_MAX_DELAY) {
      if ((HAL_GetTick() - start_tick) > timeout) {
        return HAL_TIMEOUT;
      }
    }
  } while (busy != E_OK);

  return HAL_OK;
}

/**
 * @brief Helper function to check if EVE_busy() tried to recover from a coprocessor fault.
 * The internal fault indicator is cleared so it could be set by EVE_busy() again.
 * @return - EVE_FAULT_RECOVERED - if EVE_busy() detected a coprocessor fault
 * @return - E_OK - if EVE_busy() did not detect a coprocessor fault
 */
uint8_t EVE_Driver::GetAndResetFaultState() {
  uint8_t ret = E_OK;

  if (fault_recovered == EVE_FAULT_RECOVERED) {
    ret = EVE_FAULT_RECOVERED;
    fault_recovered = E_OK;
  }

  return ret;
}

/**
 * @brief Clears the display-list command buffer, exiting display-list mode.
 */
void EVE_Driver::ClearDLCmdBuffer() {
  this->dl_cmd_buffer.clear();
}

/**
 * @brief Sends the buffered display-list commands to the display, then exits display-list mode.
 */
HAL_StatusTypeDef EVE_Driver::SendBufferedDLCmds(uint32_t timeout) {
  //send commands as block transfer and clear afterwards
  ReturnOnError(this->SendCmdBlockTransfer((const uint8_t*)this->dl_cmd_buffer.data(), this->dl_cmd_buffer.size() * sizeof(uint32_t), timeout));
  this->ClearDLCmdBuffer();

  return HAL_OK;
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

HAL_StatusTypeDef EVE_Driver::SendCmdBlockTransfer(const uint8_t* p_data, uint32_t length, uint32_t timeout) {
  if (p_data == NULL || length % 4 != 0) {
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
    ReturnOnError(this->WaitUntilNotBusy(timeout));
  }

  return HAL_OK;
}


/* ##################################################################
    commands and functions to be used outside of display-lists
##################################################################### */

/**
 * @brief Returns the source address and size of the bitmap loaded by the previous CMD_LOADIMAGE.
 * @note - Meant to be called outside display-list building.
 * @note - Includes executing the command and waiting for completion.
 */
HAL_StatusTypeDef EVE_Driver::CmdGetProps(uint32_t *p_pointer, uint32_t *p_width, uint32_t *p_height) {
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

/**
 * @brief Returns the next address after a CMD_INFLATE and other commands.
 * @note - Meant to be called outside display-list building.
 * @note - Includes executing the command and waiting for completion.
 */
HAL_StatusTypeDef EVE_Driver::CmdGetPtr(uint32_t* p_pointer) {
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

/**
 * @brief Decompress data into RAM_G.
 * @note - The data must be correct and complete, padded to a multiple of 4 bytes.
 * @note - Meant to be called outside display-list building.
 * @note - Includes executing the command and waiting for completion.
 */
HAL_StatusTypeDef EVE_Driver::CmdInflate(uint32_t ptr, const uint8_t *p_data, uint32_t len) {
  if (p_data == NULL || len % 4 != 0) {
    return HAL_ERROR;
  }

  uint32_t cmd[2] = { CMD_INFLATE, ptr };

  //write command and data as block transfer
  ReturnOnError(this->phy.DirectWriteBuffer(REG_CMDB_WRITE, (const uint8_t*)cmd, 8, EVE_PHY_SMALL_TRANSFER_TIMEOUT));
  return this->SendCmdBlockTransfer(p_data, len, 30);
}

/**
 * @brief Trigger interrupt INT_CMDFLAG.
 * @note - Meant to be called outside display-list building.
 * @note - Includes executing the command and waiting for completion.
 */
HAL_StatusTypeDef EVE_Driver::CmdInterrupt(uint32_t msec) {
  uint32_t cmd[2] = { CMD_INTERRUPT, msec };

  //write command and wait for completion
  ReturnOnError(this->phy.DirectWriteBuffer(REG_CMDB_WRITE, (const uint8_t*)cmd, 8, EVE_PHY_SMALL_TRANSFER_TIMEOUT));
  return this->WaitUntilNotBusy(10); //TODO: do we need to wait at least `msec`? probably not?
}

/**
 * @brief Loads and decodes a JPEG/PNG image into RAM_G.
 * @note - Decoding PNG images takes significantly more time than decoding JPEG images.
 * @note - In doubt use the EVE Asset Builder to check if PNG/JPEG files are compatible.
 * @note - If the image is in PNG format, the top 42kiB of RAM_G will be overwritten.
 * @note - The data must be padded to a multiple of 4 bytes.
 * @note - Meant to be called outside display-list building.
 * @note - Includes executing the command and waiting for completion.
 */
HAL_StatusTypeDef EVE_Driver::CmdLoadImage(uint32_t ptr, uint32_t options, const uint8_t *p_data, uint32_t len) {
  if (p_data == NULL || len % 4 != 0) {
    return HAL_ERROR;
  }

  uint32_t cmd[3] = { CMD_LOADIMAGE, ptr, options };

  //write command and data as block transfer (unless mediafifo is used)
  ReturnOnError(this->phy.DirectWriteBuffer(REG_CMDB_WRITE, (const uint8_t*)cmd, 12, EVE_PHY_SMALL_TRANSFER_TIMEOUT));
  if ((options & EVE_OPT_MEDIAFIFO) == 0) {
    if (p_data == NULL) {
      return HAL_ERROR;
    }
    return this->SendCmdBlockTransfer(p_data, len, 30);
  } else {
    return HAL_OK;
  }
}

/**
 * @brief Set up a streaming media FIFO in RAM_G.
 * @note - Meant to be called outside display-list building.
 * @note - Includes executing the command and waiting for completion.
 */
HAL_StatusTypeDef EVE_Driver::CmdMediaFifo(uint32_t ptr, uint32_t size) {
  uint32_t cmd[3] = { CMD_MEDIAFIFO, ptr, size };

  //write command and wait for completion
  ReturnOnError(this->phy.DirectWriteBuffer(REG_CMDB_WRITE, (const uint8_t*)cmd, 12, EVE_PHY_SMALL_TRANSFER_TIMEOUT));
  return this->WaitUntilNotBusy(10);
}

/**
 * @brief Copy a block of RAM_G.
 * @note - Meant to be called outside display-list building.
 * @note - Includes executing the command and waiting for completion.
 */
HAL_StatusTypeDef EVE_Driver::CmdMemcpy(uint32_t dest, uint32_t src, uint32_t num) {
  uint32_t cmd[4] = { CMD_MEMCPY, dest, src, num };

  //write command and wait for completion
  ReturnOnError(this->phy.DirectWriteBuffer(REG_CMDB_WRITE, (const uint8_t*)cmd, 16, EVE_PHY_SMALL_TRANSFER_TIMEOUT));
  return this->WaitUntilNotBusy(10);
}

/**
 * @brief Compute a CRC-32 for RAM_G.
 * @note - Meant to be called outside display-list building.
 * @note - Includes executing the command and waiting for completion.
 */
HAL_StatusTypeDef EVE_Driver::CmdMemcrc(uint32_t ptr, uint32_t num, uint32_t* p_crc) {
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

/**
 * @brief Fill RAM_G with a byte value.
 * @note - Meant to be called outside display-list building.
 * @note - Includes executing the command and waiting for completion.
 */
HAL_StatusTypeDef EVE_Driver::CmdMemset(uint32_t ptr, uint8_t value, uint32_t num) {
  uint32_t cmd[4] = { CMD_MEMSET, ptr, (uint32_t)value, num };

  //write command and wait for completion
  ReturnOnError(this->phy.DirectWriteBuffer(REG_CMDB_WRITE, (const uint8_t*)cmd, 16, EVE_PHY_SMALL_TRANSFER_TIMEOUT));
  return this->WaitUntilNotBusy(10);
}

/**
 * @brief Write zero to RAM_G.
 * @note - Meant to be called outside display-list building.
 * @note - Includes executing the command and waiting for completion.
 */
HAL_StatusTypeDef EVE_Driver::CmdMemzero(uint32_t ptr, uint32_t num) {
  uint32_t cmd[3] = { CMD_MEMZERO, ptr, num };

  //write command and wait for completion
  ReturnOnError(this->phy.DirectWriteBuffer(REG_CMDB_WRITE, (const uint8_t*)cmd, 12, EVE_PHY_SMALL_TRANSFER_TIMEOUT));
  return this->WaitUntilNotBusy(10);
}

/**
 * @brief Play back motion-JPEG encoded AVI video.
 * @note - The data must be padded to a multiple of 4 bytes.
 * @note - Meant to be called outside display-list building.
 * @note - Includes executing the command.
 * @note - Does not wait for completion in order to allow the video to be paused or terminated by REG_PLAY_CONTROL
 */
HAL_StatusTypeDef EVE_Driver::CmdPlayVideo(uint32_t options, const uint8_t *p_data, uint32_t len) {
  if (p_data == NULL || len % 4 != 0) {
    return HAL_ERROR;
  }

  uint32_t cmd[2] = { CMD_PLAYVIDEO, options };

  //write command and data as block transfer (unless mediafifo is used)
  ReturnOnError(this->phy.DirectWriteBuffer(REG_CMDB_WRITE, (const uint8_t*)cmd, 8, EVE_PHY_SMALL_TRANSFER_TIMEOUT));
  if ((options & EVE_OPT_MEDIAFIFO) == 0) {
    if (p_data == NULL) {
      return HAL_ERROR;
    }
    return this->SendCmdBlockTransfer(p_data, len, 30);
  } else {
    return HAL_OK;
  }
}

/**
 * @brief Rotate the screen and set up transform matrix accordingly.
 * @note - Meant to be called outside display-list building.
 * @note - Includes executing the command and waiting for completion.
 */
HAL_StatusTypeDef EVE_Driver::CmdSetRotate(uint32_t rotation) {
  uint32_t cmd[2] = { CMD_SETROTATE, rotation };

  //write command and wait for completion
  ReturnOnError(this->phy.DirectWriteBuffer(REG_CMDB_WRITE, (const uint8_t*)cmd, 8, EVE_PHY_SMALL_TRANSFER_TIMEOUT));
  return this->WaitUntilNotBusy(10);
}

/**
 * @brief Take a snapshot of the current screen.
 * @note - Meant to be called outside display-list building.
 * @note - Includes executing the command and waiting for completion.
 */
HAL_StatusTypeDef EVE_Driver::CmdSnapshot(uint32_t ptr) {
  uint32_t cmd[2] = { CMD_SNAPSHOT, ptr };

  //write command and wait for completion
  ReturnOnError(this->phy.DirectWriteBuffer(REG_CMDB_WRITE, (const uint8_t*)cmd, 8, EVE_PHY_SMALL_TRANSFER_TIMEOUT));
  return this->WaitUntilNotBusy(10);
}

/**
 * @brief Take a snapshot of part of the current screen with format option.
 * @note - Meant to be called outside display-list building.
 * @note - Includes executing the command and waiting for completion.
 */
HAL_StatusTypeDef EVE_Driver::CmdSnapshot2(uint32_t fmt, uint32_t ptr, int16_t xc0, int16_t yc0, uint16_t wid, uint16_t hgt) {
  uint32_t cmd[5] = {
      CMD_SNAPSHOT2, fmt, ptr,
      ((uint32_t)xc0) | (((uint32_t)yc0) << 16U),
      ((uint32_t)wid) | (((uint32_t)hgt) << 16U)
  };

  //write command and wait for completion
  ReturnOnError(this->phy.DirectWriteBuffer(REG_CMDB_WRITE, (const uint8_t*)cmd, 20, EVE_PHY_SMALL_TRANSFER_TIMEOUT));
  return this->WaitUntilNotBusy(10);
}

/**
 * @brief Track touches for a graphics object.
 * @note - Meant to be called outside display-list building.
 * @note - Includes executing the command and waiting for completion.
 */
HAL_StatusTypeDef EVE_Driver::CmdTrack(int16_t xc0, int16_t yc0, uint16_t wid, uint16_t hgt, uint16_t tag) {
  uint32_t cmd[4] = {
      CMD_TRACK,
      ((uint32_t)xc0) | (((uint32_t)yc0) << 16U),
      ((uint32_t)wid) | (((uint32_t)hgt) << 16U),
      (uint32_t)tag
  };

  //write command and wait for completion
  ReturnOnError(this->phy.DirectWriteBuffer(REG_CMDB_WRITE, (const uint8_t*)cmd, 16, EVE_PHY_SMALL_TRANSFER_TIMEOUT));
  return this->WaitUntilNotBusy(10);
}

/**
 * @brief Load the next frame of a video.
 * @note - Meant to be called outside display-list building.
 * @note - Includes executing the command and waiting for completion.
 */
HAL_StatusTypeDef EVE_Driver::CmdVideoFrame(uint32_t dest, uint32_t result_ptr) {
  uint32_t cmd[3] = { CMD_VIDEOFRAME, dest, result_ptr };

  //write command and wait for completion
  ReturnOnError(this->phy.DirectWriteBuffer(REG_CMDB_WRITE, (const uint8_t*)cmd, 12, EVE_PHY_SMALL_TRANSFER_TIMEOUT));
  return this->WaitUntilNotBusy(10);
}


/* ##################################################################
    patching and initialization
##################################################################### */

/**
 * @brief Writes all parameters defined for the display selected in EVE_config.h.
 * to the corresponding registers.
 * It is used by Init() and can be used to refresh the register values if needed.
 */
HAL_StatusTypeDef EVE_Driver::WriteDisplayParameters() {
  //perform writes in mmap mode
  ReturnOnError(this->phy.EnsureMMapMode(MMAP_FUNC_RAM));

  //Initialize Display
  MMAP_REG_HSIZE = EVE_HSIZE;      //active display width
  MMAP_REG_HCYCLE = EVE_HCYCLE;    //total number of clocks per line, incl front/back porch
  MMAP_REG_HOFFSET = EVE_HOFFSET;  //start of active line
  MMAP_REG_HSYNC0 = EVE_HSYNC0;    //start of horizontal sync pulse
  MMAP_REG_HSYNC1 = EVE_HSYNC1;    //end of horizontal sync pulse
  MMAP_REG_VSIZE = EVE_VSIZE;      //active display height
  MMAP_REG_VCYCLE = EVE_VCYCLE;    //total number of lines per screen, including pre/post
  MMAP_REG_VOFFSET = EVE_VOFFSET;  //start of active screen
  MMAP_REG_VSYNC0 = EVE_VSYNC0;    //start of vertical sync pulse
  MMAP_REG_VSYNC1 = EVE_VSYNC1;    //end of vertical sync pulse
  MMAP_REG_SWIZZLE = EVE_SWIZZLE;  //FT8xx output to LCD - pin order
  MMAP_REG_PCLK_POL = EVE_PCLKPOL; //LCD data is clocked in on this PCLK edge
  MMAP_REG_CSPREAD = EVE_CSPREAD;  //helps with noise, when set to 1 fewer signals are changed simultaneously, reset-default: 1

  //Configure Touch
  MMAP_REG_TOUCH_MODE = EVE_TMODE_CONTINUOUS; //enable touch
#if defined (EVE_TOUCH_RZTHRESH)
  MMAP_REG_TOUCH_RZTHRESH = EVE_TOUCH_RZTHRESH; //configure the sensitivity of resistive touch
#else
  MMAP_REG_TOUCH_RZTHRESH = 1200U; //set a reasonable default value if none is given
#endif

#if defined (EVE_ROTATE)
  MMAP_REG_ROTATE = EVE_ROTATE & 7U; //bit0 = invert, bit2 = portrait, bit3 = mirrored
  //reset default value is 0x0 - not inverted, landscape, not mirrored
#endif

  return this->phy.EndMMap();
}

/**
 * @brief Initializes EVE according to the selected configuration from EVE_config.h.
 * @return E_OK in case of success
 * @note - Has to be executed with the SPI setup to 11 MHz or less as required by FT8xx / BT8xx!
 * @note - Additional settings can be made through extra macros.
 * @note - EVE_TOUCH_RZTHRESH - configure the sensitivity of resistive touch, defaults to 1200.
 * @note - EVE_ROTATE - set the screen rotation: bit0 = invert, bit1 = portrait, bit2 = mirrored.
 * @note - needs a set of calibration values for the selected rotation since this rotates before calibration!
 * @note - EVE_BACKLIGHT_FREQ - configure the backlight frequency, default is not writing it which results in 250Hz.
 * @note - EVE_BACKLIGHT_PWM - configure the backlight pwm, defaults to 0x20 / 25%.
 */
HAL_StatusTypeDef EVE_Driver::Init(uint8_t* p_result) {
  if (p_result == NULL) {
    return HAL_ERROR;
  }

  *p_result = E_NOT_OK;

  //power cycle
  HAL_GPIO_WritePin(LCD_PD_N_GPIO_Port, LCD_PD_N_Pin, GPIO_PIN_RESET);
  HAL_Delay(6); //minimum time for power-down is 5ms
  HAL_GPIO_WritePin(LCD_PD_N_GPIO_Port, LCD_PD_N_Pin, GPIO_PIN_SET);
  HAL_Delay(21); //minimum time to allow from rising PD_N to first access is 20ms

#if defined (EVE_HAS_CRYSTAL)
  ReturnOnError(this->phy.SendHostCommand(EVE_CLKEXT, 0U)); //setup EVE for external clock
#else
  ReturnOnError(this->phy.SendHostCommand(EVE_CLKINT, 0U)); //setup EVE for internal clock
#endif

  ReturnOnError(this->phy.SendHostCommand(EVE_ACTIVE, 0U)); //start EVE
  HAL_Delay(40); //give EVE a moment of silence to power up

  ReturnOnError(this->WaitRegID(p_result));
  if (*p_result == E_OK) {
    ReturnOnError(this->WaitReset(p_result));
    if (*p_result == E_OK) {
      //use mmap mode for writing
      ReturnOnError(this->phy.EnsureMMapMode(MMAP_FUNC_RAM));

      MMAP_REG_PWM_DUTY = 0U; //turn off backlight

      ReturnOnError(this->WriteDisplayParameters());

      ReturnOnError(this->phy.EnsureMMapMode(MMAP_FUNC_RAM)); //make sure we still have mmap enabled

      //disable Audio for now
      MMAP_REG_VOL_PB = 0U;      //turn recorded audio volume down, reset-default is 0xff
      MMAP_REG_VOL_SOUND = 0U;   //turn synthesizer volume down, reset-default is 0xff
      MMAP_REG_SOUND = EVE_MUTE; //set synthesizer to mute

      //write a basic display-list to get things started
      *(uint32_t*)EVE_MMAP_RAM_DL_PTR = DL_CLEAR_COLOR_RGB;
      *((uint32_t*)EVE_MMAP_RAM_DL_PTR + 1U) = (DL_CLEAR | CLR_COL | CLR_STN | CLR_TAG);
      *((uint32_t*)EVE_MMAP_RAM_DL_PTR + 2U) = DL_DISPLAY; //end of display list
      MMAP_REG_DLSWAP = EVE_DLSWAP_FRAME;

      //nothing is being displayed yet - enable the pixel clock now
      MMAP_REG_GPIO = 0x80U; //enable the DISP signal to the LCD panel, it is set to output in REG_GPIO_DIR by default
      MMAP_REG_PCLK = EVE_PCLK; //start clocking data to the LCD panel

#if defined (EVE_BACKLIGHT_FREQ)
      MMAP_REG_PWM_HZ = EVE_BACKLIGHT_FREQ; //set backlight frequency to configured value
#endif

#if defined (EVE_BACKLIGHT_PWM)
      MMAP_REG_PWM_DUTY = EVE_BACKLIGHT_PWM; //set backlight pwm to user requested level
#else
      MMAP_REG_PWM_DUTY = 0x20U; //turn on backlight pwm to 25%
#endif

      ReturnOnError(this->phy.EndMMap());

      HAL_Delay(1);
      return this->WaitUntilNotBusy(20); //just to be safe, wait for EVE to not be busy
    }
  }

  return HAL_OK;
}

HAL_StatusTypeDef EVE_Driver::SetTransferMode(EVE_TransferMode mode) {
  if (this->GetTransferMode() == mode) {
    //already in desired mode
    return HAL_OK;
  }

  if (mode != TRANSFERMODE_SINGLE && mode != TRANSFERMODE_QUAD) {
    //invalid transfer mode requested
    return HAL_ERROR;
  }

  //write to spi config register
  uint8_t spi_config = (mode == TRANSFERMODE_QUAD) ? 0x02 : 0x00;
  ReturnOnError(this->phy.DirectWrite8(REG_SPI_WIDTH, spi_config));

  //configure interface to new width
  return this->phy.SetTransferMode(mode);
}

EVE_TransferMode EVE_Driver::GetTransferMode() {
  return this->phy.GetTransferMode();
}


/**
 * @brief Waits for either reading REG_ID with a value of 0x7c, indicating that
 *  an EVE chip is present and ready to communicate, or untill a timeout of 400ms has passed.
 * @return Returns E_OK in case of success, EVE_FAIL_REGID_TIMEOUT if the
 * value of 0x7c could not be read.
 */
HAL_StatusTypeDef EVE_Driver::WaitRegID(uint8_t* p_result) {
  *p_result = EVE_FAIL_REGID_TIMEOUT;
  uint8_t regid = 0U;

  for (uint16_t timeout = 0U; timeout < 400U; timeout++) {
    HAL_Delay(1);

    ReturnOnError(this->phy.DirectRead8(REG_ID, &regid));
    if (regid == 0x7CU) {
      //EVE is up and running
      *p_result = E_OK;
      break;
    }
  }

  return HAL_OK;
}

/**
 * @brief Waits for either REG_CPURESET to indicate that the audio, touch and
 * coprocessor units finished their respective reset cycles,
 * or untill a timeout of 50ms has passed.
 * @return Returns E_OK in case of success, EVE_FAIL_RESET_TIMEOUT if either the
 * audio, touch or coprocessor unit indicate a fault by not returning from reset.
 */
HAL_StatusTypeDef EVE_Driver::WaitReset(uint8_t* p_result) {
  *p_result = EVE_FAIL_RESET_TIMEOUT;
  uint8_t reset = 0U;

  for (uint16_t timeout = 0U; timeout < 50U; timeout++) {
    HAL_Delay(1);

    ReturnOnError(this->phy.DirectRead8(REG_CPURESET, &reset));
    if ((reset & 0x7U) == 0U) {
      //EVE reports all units running
      *p_result = E_OK;
      break;
    }
  }

  return HAL_OK;
}


/* ##################################################################
    functions for display lists
##################################################################### */

/**
 * @brief Generic function for display-list and coprocessor commands with no arguments.
 * @note Appends to display-list command buffer
 */
void EVE_Driver::CmdDL(uint32_t command) {
  this->dl_cmd_buffer.push_back(command);
}


/**
 * @brief Appends commands from RAM_G to the display list.
 * @note Appends to display-list command buffer
 */
void EVE_Driver::CmdAppend(uint32_t ptr, uint32_t num) {
  this->dl_cmd_buffer.push_back(CMD_APPEND);
  this->dl_cmd_buffer.push_back(ptr);
  this->dl_cmd_buffer.push_back(num);
}

/**
 * @brief Set the background color.
 * @note Appends to display-list command buffer
 */
void EVE_Driver::CmdBGColor(uint32_t color) {
  this->dl_cmd_buffer.push_back(CMD_BGCOLOR);
  this->dl_cmd_buffer.push_back(color & 0xFFFFFFU);
}

/**
 * @brief Draw a button with a label.
 * @note Appends to display-list command buffer
 */
void EVE_Driver::CmdButton(int16_t xc0, int16_t yc0, uint16_t wid, uint16_t hgt, uint16_t font, uint16_t options, const char *p_text) {
  if (p_text == NULL) {
    return;
  }

  this->dl_cmd_buffer.push_back(CMD_BUTTON);
  this->dl_cmd_buffer.push_back(((uint32_t)xc0) | (((uint32_t)yc0) << 16U));
  this->dl_cmd_buffer.push_back(((uint32_t)wid) | (((uint32_t)hgt) << 16U));
  this->dl_cmd_buffer.push_back(((uint32_t)font) | (((uint32_t)options) << 16U));
  this->WriteString(p_text);
}

/**
 * @brief Execute the touch screen calibration routine.
 * @note Appends to display-list command buffer
 * @note Pauses command execution until calibration is done
 */
void EVE_Driver::CmdCalibrate() {
  this->dl_cmd_buffer.push_back(CMD_CALIBRATE);
  this->dl_cmd_buffer.push_back(0U);
}

/**
 * @brief Draw an analog clock.
 * @note Appends to display-list command buffer
 */
void EVE_Driver::CmdClock(int16_t xc0, int16_t yc0, uint16_t rad, uint16_t options, uint16_t hours, uint16_t mins, uint16_t secs, uint16_t msecs) {
  this->dl_cmd_buffer.push_back(CMD_CLOCK);
  this->dl_cmd_buffer.push_back(((uint32_t)xc0) | (((uint32_t)yc0) << 16U));
  this->dl_cmd_buffer.push_back(((uint32_t)rad) | (((uint32_t)options) << 16U));
  this->dl_cmd_buffer.push_back(((uint32_t)hours) | (((uint32_t)mins) << 16U));
  this->dl_cmd_buffer.push_back(((uint32_t)secs) | (((uint32_t)msecs) << 16U));
}

/**
 * @brief Draw a rotary dial control.
 * @note Appends to display-list command buffer
 */
void EVE_Driver::CmdDial(int16_t xc0, int16_t yc0, uint16_t rad, uint16_t options, uint16_t val) {
  this->dl_cmd_buffer.push_back(CMD_DIAL);
  this->dl_cmd_buffer.push_back(((uint32_t)xc0) | (((uint32_t)yc0) << 16U));
  this->dl_cmd_buffer.push_back(((uint32_t)rad) | (((uint32_t)options) << 16U));
  this->dl_cmd_buffer.push_back((uint32_t)val);
}

/**
 * @brief Set the foreground color.
 * @note Appends to display-list command buffer
 */
void EVE_Driver::CmdFGColor(uint32_t color) {
  this->dl_cmd_buffer.push_back(CMD_FGCOLOR);
  this->dl_cmd_buffer.push_back(color & 0xFFFFFFU);
}

/**
 * @brief Draw a gauge.
 * @note Appends to display-list command buffer
 */
void EVE_Driver::CmdGauge(int16_t xc0, int16_t yc0, uint16_t rad, uint16_t options, uint16_t major, uint16_t minor, uint16_t val, uint16_t range) {
  this->dl_cmd_buffer.push_back(CMD_GAUGE);
  this->dl_cmd_buffer.push_back(((uint32_t)xc0) | (((uint32_t)yc0) << 16U));
  this->dl_cmd_buffer.push_back(((uint32_t)rad) | (((uint32_t)options) << 16U));
  this->dl_cmd_buffer.push_back(((uint32_t)major) | (((uint32_t)minor) << 16U));
  this->dl_cmd_buffer.push_back(((uint32_t)val) | (((uint32_t)range) << 16U));
}

/*void EVE_Driver::CmdGetMatrix(int32_t *p_a, int32_t *p_b, int32_t *p_c, int32_t *p_d, int32_t *p_e, int32_t *p_f) {

}*/

/**
 * @brief Set up the highlight color used in 3D effects for CMD_BUTTON and CMD_KEYS.
 * @note Appends to display-list command buffer
 */
void EVE_Driver::CmdGradColor(uint32_t color) {
  this->dl_cmd_buffer.push_back(CMD_GRADCOLOR);
  this->dl_cmd_buffer.push_back(color & 0xFFFFFFU);
}

/**
 * @brief Draw a smooth color gradient.
 * @note Appends to display-list command buffer
 */
void EVE_Driver::CmdGradient(int16_t xc0, int16_t yc0, uint32_t rgb0, int16_t xc1, int16_t yc1, uint32_t rgb1) {
  this->dl_cmd_buffer.push_back(CMD_GRADIENT);
  this->dl_cmd_buffer.push_back(((uint32_t)xc0) | (((uint32_t)yc0) << 16U));
  this->dl_cmd_buffer.push_back(rgb0 & 0xFFFFFFU);
  this->dl_cmd_buffer.push_back(((uint32_t)xc1) | (((uint32_t)yc1) << 16U));
  this->dl_cmd_buffer.push_back(rgb1 & 0xFFFFFFU);
}

/**
 * @brief Draw a row of key buttons with labels.
 * @note Appends to display-list command buffer
 * @note - The tag value of each button is set to the ASCII value of its label.
 * @note - Does not work with UTF-8.
 */
void EVE_Driver::CmdKeys(int16_t xc0, int16_t yc0, uint16_t wid, uint16_t hgt, uint16_t font, uint16_t options, const char *p_text) {
  if (p_text == NULL) {
    return;
  }

  this->dl_cmd_buffer.push_back(CMD_KEYS);
  this->dl_cmd_buffer.push_back(((uint32_t)xc0) | (((uint32_t)yc0) << 16U));
  this->dl_cmd_buffer.push_back(((uint32_t)wid) | (((uint32_t)hgt) << 16U));
  this->dl_cmd_buffer.push_back(((uint32_t)font) | (((uint32_t)options) << 16U));
  this->WriteString(p_text);
}

/**
 * @brief Draw a number.
 * @note Appends to display-list command buffer
 */
void EVE_Driver::CmdNumber(int16_t xc0, int16_t yc0, uint16_t font, uint16_t options, int32_t number) {
  this->dl_cmd_buffer.push_back(CMD_NUMBER);
  this->dl_cmd_buffer.push_back(((uint32_t)xc0) | (((uint32_t)yc0) << 16U));
  this->dl_cmd_buffer.push_back(((uint32_t)font) | (((uint32_t)options) << 16U));
  this->dl_cmd_buffer.push_back(number);
}

/**
 * @brief Draw a progress bar.
 * @note Appends to display-list command buffer
 */
void EVE_Driver::CmdProgress(int16_t xc0, int16_t yc0, uint16_t wid, uint16_t hgt, uint16_t options, uint16_t val, uint16_t range) {
  this->dl_cmd_buffer.push_back(CMD_PROGRESS);
  this->dl_cmd_buffer.push_back(((uint32_t)xc0) | (((uint32_t)yc0) << 16U));
  this->dl_cmd_buffer.push_back(((uint32_t)wid) | (((uint32_t)hgt) << 16U));
  this->dl_cmd_buffer.push_back(((uint32_t)options) | (((uint32_t)val) << 16U));
  this->dl_cmd_buffer.push_back((uint32_t)range);
}

/**
 * @brief Load a ROM font into bitmap handle.
 * @note Appends to display-list command buffer
 * @note - generates display list commands, so it needs to be put in a display list
 */
void EVE_Driver::CmdRomFont(uint32_t font, uint32_t romslot) {
  this->dl_cmd_buffer.push_back(CMD_ROMFONT);
  this->dl_cmd_buffer.push_back(font);
  this->dl_cmd_buffer.push_back(romslot);
}

/**
 * @brief Apply a rotation to the current matrix.
 * @note Appends to display-list command buffer
 */
void EVE_Driver::CmdRotate(uint32_t angle) {
  this->dl_cmd_buffer.push_back(CMD_ROTATE);
  this->dl_cmd_buffer.push_back(angle & 0xFFFFU);
}

/**
 * @brief Apply a scale to the current matrix.
 * @note Appends to display-list command buffer
 */
void EVE_Driver::CmdScale(int32_t scx, int32_t scy) {
  this->dl_cmd_buffer.push_back(CMD_SCALE);
  this->dl_cmd_buffer.push_back(scx);
  this->dl_cmd_buffer.push_back(scy);
}

/**
 * @brief Draw a scroll bar.
 * @note Appends to display-list command buffer
 */
void EVE_Driver::CmdScrollBar(int16_t xc0, int16_t yc0, uint16_t wid, uint16_t hgt, uint16_t options, uint16_t val, uint16_t size, uint16_t range) {
  this->dl_cmd_buffer.push_back(CMD_SCROLLBAR);
  this->dl_cmd_buffer.push_back(((uint32_t)xc0) | (((uint32_t)yc0) << 16U));
  this->dl_cmd_buffer.push_back(((uint32_t)wid) | (((uint32_t)hgt) << 16U));
  this->dl_cmd_buffer.push_back(((uint32_t)options) | (((uint32_t)val) << 16U));
  this->dl_cmd_buffer.push_back(((uint32_t)size) | (((uint32_t)range) << 16U));
}

/**
 * @brief Set the base for number output.
 * @note Appends to display-list command buffer
 */
void EVE_Driver::CmdSetBase(uint32_t base) {
  this->dl_cmd_buffer.push_back(CMD_SETBASE);
  this->dl_cmd_buffer.push_back(base);
}

/**
 * @brief Generate the corresponding display list commands for given bitmap information.
 * @note Appends to display-list command buffer
 */
void EVE_Driver::CmdSetBitmap(uint32_t addr, uint16_t fmt, uint16_t width, uint16_t height) {
  this->dl_cmd_buffer.push_back(CMD_SETBITMAP);
  this->dl_cmd_buffer.push_back(addr);
  this->dl_cmd_buffer.push_back(((uint32_t)fmt) | (((uint32_t)width) << 16U));
  this->dl_cmd_buffer.push_back((uint32_t)height);
}

/**
 * @brief Register one custom font into the coprocessor engine.
 * @note Appends to display-list command buffer
 * @note - does not set up the bitmap parameters of the font
 */
void EVE_Driver::CmdSetFont(uint32_t font, uint32_t ptr) {
  this->dl_cmd_buffer.push_back(CMD_SETFONT);
  this->dl_cmd_buffer.push_back(font);
  this->dl_cmd_buffer.push_back(ptr);
}

/**
 * @brief Set up a custom font for use by the coprocessor engine.
 * @note Appends to display-list command buffer
 * @note - generates display list commands, so it needs to be put in a display list
 */
void EVE_Driver::CmdSetFont2(uint32_t font, uint32_t ptr, uint32_t firstchar) {
  this->dl_cmd_buffer.push_back(CMD_SETFONT2);
  this->dl_cmd_buffer.push_back(font);
  this->dl_cmd_buffer.push_back(ptr);
  this->dl_cmd_buffer.push_back(firstchar);
}

/**
 * @brief Set the scratch bitmap for widget use.
 * @note Appends to display-list command buffer
 */
void EVE_Driver::CmdSetScratch(uint32_t handle) {
  this->dl_cmd_buffer.push_back(CMD_SETSCRATCH);
  this->dl_cmd_buffer.push_back(handle);
}

/**
 * @brief Start a continuous sketch update.
 * @note Appends to display-list command buffer
 */
void EVE_Driver::CmdSketch(int16_t xc0, int16_t yc0, uint16_t wid, uint16_t hgt, uint32_t ptr, uint16_t format) {
  this->dl_cmd_buffer.push_back(CMD_SKETCH);
  this->dl_cmd_buffer.push_back(((uint32_t)xc0) | (((uint32_t)yc0) << 16U));
  this->dl_cmd_buffer.push_back(((uint32_t)wid) | (((uint32_t)hgt) << 16U));
  this->dl_cmd_buffer.push_back(ptr);
  this->dl_cmd_buffer.push_back((uint32_t)format);
}

/**
 * @brief Draw a slider.
 * @note Appends to display-list command buffer
 */
void EVE_Driver::CmdSlider(int16_t xc0, int16_t yc0, uint16_t wid, uint16_t hgt, uint16_t options, uint16_t val, uint16_t range) {
  this->dl_cmd_buffer.push_back(CMD_SLIDER);
  this->dl_cmd_buffer.push_back(((uint32_t)xc0) | (((uint32_t)yc0) << 16U));
  this->dl_cmd_buffer.push_back(((uint32_t)wid) | (((uint32_t)hgt) << 16U));
  this->dl_cmd_buffer.push_back(((uint32_t)options) | (((uint32_t)val) << 16U));
  this->dl_cmd_buffer.push_back((uint32_t)range);
}

/**
 * @brief Start an animated spinner.
 * @note Appends to display-list command buffer
 */
void EVE_Driver::CmdSpinner(int16_t xc0, int16_t yc0, uint16_t style, uint16_t scale) {
  this->dl_cmd_buffer.push_back(CMD_SLIDER);
  this->dl_cmd_buffer.push_back(((uint32_t)xc0) | (((uint32_t)yc0) << 16U));
  this->dl_cmd_buffer.push_back(((uint32_t)style) | (((uint32_t)scale) << 16U));
}

/**
 * @brief Draw a text string.
 * @note Appends to display-list command buffer
 */
void EVE_Driver::CmdText(int16_t xc0, int16_t yc0, uint16_t font, uint16_t options, const char *p_text) {
  if (p_text == NULL) {
    return;
  }

  this->dl_cmd_buffer.push_back(CMD_TEXT);
  this->dl_cmd_buffer.push_back(((uint32_t)xc0) | (((uint32_t)yc0) << 16U));
  this->dl_cmd_buffer.push_back(((uint32_t)font) | (((uint32_t)options) << 16U));
  this->WriteString(p_text);
}

/**
 * @brief Draw a toggle switch with labels.
 * @note Appends to display-list command buffer
 */
void EVE_Driver::CmdToggle(int16_t xc0, int16_t yc0, uint16_t wid, uint16_t font, uint16_t options, uint16_t state, const char *p_text) {
  if (p_text == NULL) {
    return;
  }

  this->dl_cmd_buffer.push_back(CMD_TOGGLE);
  this->dl_cmd_buffer.push_back(((uint32_t)xc0) | (((uint32_t)yc0) << 16U));
  this->dl_cmd_buffer.push_back(((uint32_t)wid) | (((uint32_t)font) << 16U));
  this->dl_cmd_buffer.push_back(((uint32_t)options) | (((uint32_t)state) << 16U));
  this->WriteString(p_text);
}

/**
 * @brief Apply a translation to the current matrix.
 * @note Appends to display-list command buffer
 */
void EVE_Driver::CmdTranslate(int32_t tr_x, int32_t tr_y) {
  this->dl_cmd_buffer.push_back(CMD_TRANSLATE);
  this->dl_cmd_buffer.push_back(tr_x);
  this->dl_cmd_buffer.push_back(tr_y);
}


/**
 * @brief Set the current color red, green and blue.
 * @note Appends to display-list command buffer
 */
void EVE_Driver::ColorRGB(uint32_t color) {
  this->CmdDL(DL_COLOR_RGB | (color & 0xFFFFFFU));
}

/**
 * @brief Set the current color alpha.
 * @note Appends to display-list command buffer
 */
void EVE_Driver::ColorA(uint8_t alpha) {
  this->CmdDL(DL_COLOR_A | ((uint32_t)alpha));
}


//write a string to the command buffer, in context of a command
void EVE_Driver::WriteString(const char *p_text) {
  const uint8_t* p_bytes = (const uint8_t*)p_text;

  if (p_bytes == NULL) {
    return;
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

/*HAL_StatusTypeDef EVE_Driver::CalibrateManual(uint16_t width, uint16_t height) {

}*/


/* ##################################################################
    meta-commands (commonly used sequences of display-list entries)
##################################################################### */

HAL_StatusTypeDef EVE_Driver::CmdBeginDisplay(bool color, bool stencil, bool tag, uint32_t clear_color) {
  ReturnOnError(this->CmdMemzero(EVE_RAM_DL, EVE_RAM_DL_SIZE));
  this->CmdDL(CMD_DLSTART);
  this->CmdDL(DL_CLEAR_COLOR_RGB | (clear_color & 0xFFFFFFU));
  this->CmdDL(CLEAR(color, stencil, tag));
  return HAL_OK;
}

HAL_StatusTypeDef EVE_Driver::CmdBeginDisplayLimited(bool color, bool stencil, bool tag, uint32_t clear_color, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  ReturnOnError(this->CmdMemzero(EVE_RAM_DL, EVE_RAM_DL_SIZE));
  this->CmdDL(CMD_DLSTART);
  this->CmdDL(SCISSOR_XY(x, y));
  this->CmdDL(SCISSOR_SIZE(w, h));
  this->CmdDL(DL_CLEAR_COLOR_RGB | (clear_color & 0xFFFFFFU));
  this->CmdDL(CLEAR(color, stencil, tag));
  return HAL_OK;
}

void EVE_Driver::CmdPoint(int16_t x0, int16_t y0, uint16_t size) {
  this->CmdDL(DL_BEGIN | EVE_POINTS);
  this->CmdDL(POINT_SIZE(16 * size));
  this->CmdDL(VERTEX2F(16 * x0, 16 * y0));
  this->CmdDL(DL_END);
}

void EVE_Driver::CmdLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t width) {
  this->CmdDL(DL_BEGIN | EVE_LINES);
  this->CmdDL(LINE_WIDTH(16 * width));
  this->CmdDL(VERTEX2F(16 * x0, 16 * y0));
  this->CmdDL(VERTEX2F(16 * x1, 16 * y1));
  this->CmdDL(DL_END);
}

void EVE_Driver::CmdRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t corner) {
  this->CmdDL(DL_BEGIN | EVE_RECTS);
  this->CmdDL(LINE_WIDTH(16 * corner));
  this->CmdDL(VERTEX2F(16 * x0, 16 * y0));
  this->CmdDL(VERTEX2F(16 * x1, 16 * y1));
  this->CmdDL(DL_END);
}

void EVE_Driver::CmdEndDisplay() {
  this->CmdDL(DL_DISPLAY);
  this->CmdDL(CMD_SWAP);
}

void EVE_Driver::CmdTag(uint8_t tag) {
  this->CmdDL(TAG(tag));
}

void EVE_Driver::CmdTagMask(bool enable_tag) {
  this->CmdDL(TAG_MASK(enable_tag));
}

void EVE_Driver::CmdScissor(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  this->CmdDL(SCISSOR_XY(x, y));
  this->CmdDL(SCISSOR_SIZE(w, h));
}
