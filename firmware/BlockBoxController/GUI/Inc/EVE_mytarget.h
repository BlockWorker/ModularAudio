/*
 * EVE_mytarget.h
 *
 *  Created on: Feb 23, 2025
 *      Author: Alex
 */

#ifndef INC_EVE_MYTARGET_H_
#define INC_EVE_MYTARGET_H_

#include "cpp_main.h"
#include "EVE.h"

#ifdef __cplusplus
extern "C" {
#endif


#define EVE_MMAP_BASE ((uint32_t)0x90000000UL)

#define EVE_MMAP_RAMG_READ_PTR ((uint8_t*)(EVE_MMAP_BASE))
#define EVE_MMAP_RAMG_WRITE_PTR ((uint8_t*)(EVE_MMAP_BASE | 0x800000))

#define EVE_MMAP_RAM_DL_PTR ((uint8_t*)(EVE_MMAP_BASE | 0x300000))
#define EVE_MMAP_RAM_CMD_PTR ((uint8_t*)(EVE_MMAP_BASE | 0x308000))

#define EVE_MMAP_REGACCESS32(addr) (*(uint32_t*)(EVE_MMAP_BASE | (addr)))
#define EVE_MMAP_REGACCESS16(addr) (*(uint16_t*)(EVE_MMAP_BASE | (addr)))
#define EVE_MMAP_REGACCESS8(addr) (*(uint8_t*)(EVE_MMAP_BASE | (addr)))

#define MMAP_REG_ANA_COMP           EVE_MMAP_REGACCESS8 (REG_ANA_COMP         )
#define MMAP_REG_BIST_EN            EVE_MMAP_REGACCESS8 (REG_BIST_EN          )
#define MMAP_REG_CLOCK              EVE_MMAP_REGACCESS32(REG_CLOCK            )
#define MMAP_REG_CMDB_SPACE         EVE_MMAP_REGACCESS16(REG_CMDB_SPACE       )
#define MMAP_REG_CMDB_WRITE         EVE_MMAP_REGACCESS32(REG_CMDB_WRITE       )
#define MMAP_REG_CMD_DL             EVE_MMAP_REGACCESS16(REG_CMD_DL           )
#define MMAP_REG_CMD_READ           EVE_MMAP_REGACCESS16(REG_CMD_READ         )
#define MMAP_REG_CMD_WRITE          EVE_MMAP_REGACCESS16(REG_CMD_WRITE        )
#define MMAP_REG_CPURESET           EVE_MMAP_REGACCESS8 (REG_CPURESET         )
#define MMAP_REG_CSPREAD            EVE_MMAP_REGACCESS8 (REG_CSPREAD          )
#define MMAP_REG_CTOUCH_EXTENDED    EVE_MMAP_REGACCESS8 (REG_CTOUCH_EXTENDED  )
#define MMAP_REG_CTOUCH_TOUCH0_XY   EVE_MMAP_REGACCESS32(REG_CTOUCH_TOUCH0_XY )
#define MMAP_REG_CTOUCH_TOUCH4_X    EVE_MMAP_REGACCESS16(REG_CTOUCH_TOUCH4_X  )
#define MMAP_REG_CTOUCH_TOUCH4_Y    EVE_MMAP_REGACCESS16(REG_CTOUCH_TOUCH4_Y  )
#define MMAP_REG_CTOUCH_TOUCH1_XY   EVE_MMAP_REGACCESS32(REG_CTOUCH_TOUCH1_XY )
#define MMAP_REG_CTOUCH_TOUCH2_XY   EVE_MMAP_REGACCESS32(REG_CTOUCH_TOUCH2_XY )
#define MMAP_REG_CTOUCH_TOUCH3_XY   EVE_MMAP_REGACCESS32(REG_CTOUCH_TOUCH3_XY )
#define MMAP_REG_TOUCH_CONFIG       EVE_MMAP_REGACCESS16(REG_TOUCH_CONFIG     )
#define MMAP_REG_DATESTAMP          EVE_MMAP_REGACCESS32(REG_DATESTAMP        )
#define MMAP_REG_DITHER             EVE_MMAP_REGACCESS8 (REG_DITHER           )
#define MMAP_REG_DLSWAP             EVE_MMAP_REGACCESS8 (REG_DLSWAP           )
#define MMAP_REG_FRAMES             EVE_MMAP_REGACCESS32(REG_FRAMES           )
#define MMAP_REG_FREQUENCY          EVE_MMAP_REGACCESS32(REG_FREQUENCY        )
#define MMAP_REG_GPIO               EVE_MMAP_REGACCESS8 (REG_GPIO             )
#define MMAP_REG_GPIOX              EVE_MMAP_REGACCESS16(REG_GPIOX            )
#define MMAP_REG_GPIOX_DIR          EVE_MMAP_REGACCESS16(REG_GPIOX_DIR        )
#define MMAP_REG_GPIO_DIR           EVE_MMAP_REGACCESS8 (REG_GPIO_DIR         )
#define MMAP_REG_HCYCLE             EVE_MMAP_REGACCESS16(REG_HCYCLE           )
#define MMAP_REG_HOFFSET            EVE_MMAP_REGACCESS16(REG_HOFFSET          )
#define MMAP_REG_HSIZE              EVE_MMAP_REGACCESS16(REG_HSIZE            )
#define MMAP_REG_HSYNC0             EVE_MMAP_REGACCESS16(REG_HSYNC0           )
#define MMAP_REG_HSYNC1             EVE_MMAP_REGACCESS16(REG_HSYNC1           )
#define MMAP_REG_ID                 EVE_MMAP_REGACCESS8 (REG_ID               )
#define MMAP_REG_INT_EN             EVE_MMAP_REGACCESS8 (REG_INT_EN           )
#define MMAP_REG_INT_FLAGS          EVE_MMAP_REGACCESS8 (REG_INT_FLAGS        )
#define MMAP_REG_INT_MASK           EVE_MMAP_REGACCESS8 (REG_INT_MASK         )
#define MMAP_REG_MACRO_0            EVE_MMAP_REGACCESS32(REG_MACRO_0          )
#define MMAP_REG_MACRO_1            EVE_MMAP_REGACCESS32(REG_MACRO_1          )
#define MMAP_REG_MEDIAFIFO_READ     EVE_MMAP_REGACCESS32(REG_MEDIAFIFO_READ   )
#define MMAP_REG_MEDIAFIFO_WRITE    EVE_MMAP_REGACCESS32(REG_MEDIAFIFO_WRITE  )
#define MMAP_REG_OUTBITS            EVE_MMAP_REGACCESS16(REG_OUTBITS          )
#define MMAP_REG_PCLK               EVE_MMAP_REGACCESS8 (REG_PCLK             )
#define MMAP_REG_PCLK_POL           EVE_MMAP_REGACCESS8 (REG_PCLK_POL         )
#define MMAP_REG_PLAY               EVE_MMAP_REGACCESS8 (REG_PLAY             )
#define MMAP_REG_PLAYBACK_FORMAT    EVE_MMAP_REGACCESS8 (REG_PLAYBACK_FORMAT  )
#define MMAP_REG_PLAYBACK_FREQ      EVE_MMAP_REGACCESS16(REG_PLAYBACK_FREQ    )
#define MMAP_REG_PLAYBACK_LENGTH    EVE_MMAP_REGACCESS32(REG_PLAYBACK_LENGTH  )
#define MMAP_REG_PLAYBACK_LOOP      EVE_MMAP_REGACCESS8 (REG_PLAYBACK_LOOP    )
#define MMAP_REG_PLAYBACK_PLAY      EVE_MMAP_REGACCESS8 (REG_PLAYBACK_PLAY    )
#define MMAP_REG_PLAYBACK_READPTR   EVE_MMAP_REGACCESS32(REG_PLAYBACK_READPTR )
#define MMAP_REG_PLAYBACK_START     EVE_MMAP_REGACCESS32(REG_PLAYBACK_START   )
#define MMAP_REG_PWM_DUTY           EVE_MMAP_REGACCESS8 (REG_PWM_DUTY         )
#define MMAP_REG_PWM_HZ             EVE_MMAP_REGACCESS16(REG_PWM_HZ           )
#define MMAP_REG_RENDERMODE         EVE_MMAP_REGACCESS8 (REG_RENDERMODE       )
#define MMAP_REG_ROTATE             EVE_MMAP_REGACCESS8 (REG_ROTATE           )
#define MMAP_REG_SNAPFORMAT         EVE_MMAP_REGACCESS8 (REG_SNAPFORMAT       )
#define MMAP_REG_SNAPSHOT           EVE_MMAP_REGACCESS8 (REG_SNAPSHOT         )
#define MMAP_REG_SNAPY              EVE_MMAP_REGACCESS16(REG_SNAPY            )
#define MMAP_REG_SOUND              EVE_MMAP_REGACCESS16(REG_SOUND            )
#define MMAP_REG_SPI_WIDTH          EVE_MMAP_REGACCESS8 (REG_SPI_WIDTH        )
#define MMAP_REG_SWIZZLE            EVE_MMAP_REGACCESS8 (REG_SWIZZLE          )
#define MMAP_REG_TAG                EVE_MMAP_REGACCESS8 (REG_TAG              )
#define MMAP_REG_TAG_X              EVE_MMAP_REGACCESS16(REG_TAG_X            )
#define MMAP_REG_TAG_Y              EVE_MMAP_REGACCESS16(REG_TAG_Y            )
#define MMAP_REG_TAP_CRC            EVE_MMAP_REGACCESS32(REG_TAP_CRC          )
#define MMAP_REG_TAP_MASK           EVE_MMAP_REGACCESS32(REG_TAP_MASK         )
#define MMAP_REG_TOUCH_ADC_MODE     EVE_MMAP_REGACCESS8 (REG_TOUCH_ADC_MODE   )
#define MMAP_REG_TOUCH_CHARGE       EVE_MMAP_REGACCESS16(REG_TOUCH_CHARGE     )
#define MMAP_REG_TOUCH_DIRECT_XY    EVE_MMAP_REGACCESS32(REG_TOUCH_DIRECT_XY  )
#define MMAP_REG_TOUCH_DIRECT_Z1Z2  EVE_MMAP_REGACCESS32(REG_TOUCH_DIRECT_Z1Z2)
#define MMAP_REG_TOUCH_MODE         EVE_MMAP_REGACCESS8 (REG_TOUCH_MODE       )
#define MMAP_REG_TOUCH_OVERSAMPLE   EVE_MMAP_REGACCESS8 (REG_TOUCH_OVERSAMPLE )
#define MMAP_REG_TOUCH_RAW_XY       EVE_MMAP_REGACCESS32(REG_TOUCH_RAW_XY     )
#define MMAP_REG_TOUCH_RZ           EVE_MMAP_REGACCESS16(REG_TOUCH_RZ         )
#define MMAP_REG_TOUCH_RZTHRESH     EVE_MMAP_REGACCESS16(REG_TOUCH_RZTHRESH   )
#define MMAP_REG_TOUCH_SCREEN_XY    EVE_MMAP_REGACCESS32(REG_TOUCH_SCREEN_XY  )
#define MMAP_REG_TOUCH_SETTLE       EVE_MMAP_REGACCESS8 (REG_TOUCH_SETTLE     )
#define MMAP_REG_TOUCH_TAG          EVE_MMAP_REGACCESS8 (REG_TOUCH_TAG        )
#define MMAP_REG_TOUCH_TAG1         EVE_MMAP_REGACCESS8 (REG_TOUCH_TAG1       )
#define MMAP_REG_TOUCH_TAG1_XY      EVE_MMAP_REGACCESS32(REG_TOUCH_TAG1_XY    )
#define MMAP_REG_TOUCH_TAG2         EVE_MMAP_REGACCESS8 (REG_TOUCH_TAG2       )
#define MMAP_REG_TOUCH_TAG2_XY      EVE_MMAP_REGACCESS32(REG_TOUCH_TAG2_XY    )
#define MMAP_REG_TOUCH_TAG3         EVE_MMAP_REGACCESS8 (REG_TOUCH_TAG3       )
#define MMAP_REG_TOUCH_TAG3_XY      EVE_MMAP_REGACCESS32(REG_TOUCH_TAG3_XY    )
#define MMAP_REG_TOUCH_TAG4         EVE_MMAP_REGACCESS8 (REG_TOUCH_TAG4       )
#define MMAP_REG_TOUCH_TAG4_XY      EVE_MMAP_REGACCESS32(REG_TOUCH_TAG4_XY    )
#define MMAP_REG_TOUCH_TAG_XY       EVE_MMAP_REGACCESS32(REG_TOUCH_TAG_XY     )
#define MMAP_REG_TOUCH_TRANSFORM_A  EVE_MMAP_REGACCESS32(REG_TOUCH_TRANSFORM_A)
#define MMAP_REG_TOUCH_TRANSFORM_B  EVE_MMAP_REGACCESS32(REG_TOUCH_TRANSFORM_B)
#define MMAP_REG_TOUCH_TRANSFORM_C  EVE_MMAP_REGACCESS32(REG_TOUCH_TRANSFORM_C)
#define MMAP_REG_TOUCH_TRANSFORM_D  EVE_MMAP_REGACCESS32(REG_TOUCH_TRANSFORM_D)
#define MMAP_REG_TOUCH_TRANSFORM_E  EVE_MMAP_REGACCESS32(REG_TOUCH_TRANSFORM_E)
#define MMAP_REG_TOUCH_TRANSFORM_F  EVE_MMAP_REGACCESS32(REG_TOUCH_TRANSFORM_F)
#define MMAP_REG_TRACKER            EVE_MMAP_REGACCESS32(REG_TRACKER          )
#define MMAP_REG_TRACKER_1          EVE_MMAP_REGACCESS32(REG_TRACKER_1        )
#define MMAP_REG_TRACKER_2          EVE_MMAP_REGACCESS32(REG_TRACKER_2        )
#define MMAP_REG_TRACKER_3          EVE_MMAP_REGACCESS32(REG_TRACKER_3        )
#define MMAP_REG_TRACKER_4          EVE_MMAP_REGACCESS32(REG_TRACKER_4        )
#define MMAP_REG_TRIM               EVE_MMAP_REGACCESS8 (REG_TRIM             )
#define MMAP_REG_VCYCLE             EVE_MMAP_REGACCESS16(REG_VCYCLE           )
#define MMAP_REG_VOFFSET            EVE_MMAP_REGACCESS16(REG_VOFFSET          )
#define MMAP_REG_VOL_PB             EVE_MMAP_REGACCESS8 (REG_VOL_PB           )
#define MMAP_REG_VOL_SOUND          EVE_MMAP_REGACCESS8 (REG_VOL_SOUND        )
#define MMAP_REG_VSIZE              EVE_MMAP_REGACCESS16(REG_VSIZE            )
#define MMAP_REG_VSYNC0             EVE_MMAP_REGACCESS16(REG_VSYNC0           )
#define MMAP_REG_VSYNC1             EVE_MMAP_REGACCESS16(REG_VSYNC1           )


#define EVE_PHY_SMALL_TRANSFER_TIMEOUT 5


typedef enum {
  MMAP_UNKNOWN = 0,
  MMAP_MAIN_RAM = 1,
  MMAP_FUNC_RAM = 2
} EVEMMapMode;

typedef enum {
  TRANSFERMODE_SINGLE = 0,
  TRANSFERMODE_QUAD = 1
} EVETransferMode;

typedef enum {
  TRANSFERSPEED_1M = 256,
  TRANSFERSPEED_10M = 26,
  TRANSFERSPEED_MAX = 16
} EVETransferSpeed;


#ifdef __cplusplus
}

class EVEDriver;

class EVETargetPHY {
  friend EVEDriver;
public:
  //send a host command to to the display; ends any ongoing memory mapping
  void SendHostCommand(uint8_t command, uint8_t param);

  //directly read from the display; ends any ongoing memory mapping
  void DirectReadBuffer(uint32_t address, uint8_t* buf, uint32_t size, uint32_t timeout);
  void DirectRead8(uint32_t address, uint8_t* value_ptr);
  void DirectRead16(uint32_t address, uint16_t* value_ptr);
  void DirectRead32(uint32_t address, uint32_t* value_ptr);

  //directly write to the display; ends any ongoing memory mapping
  void DirectWriteBuffer(uint32_t address, const uint8_t* buf, uint32_t size, uint32_t timeout);
  void DirectWrite8(uint32_t address, uint8_t value);
  void DirectWrite16(uint32_t address, uint16_t value);
  void DirectWrite32(uint32_t address, uint32_t value);

  void EnsureMMapMode(EVEMMapMode mode);
  void EndMMap();

  void SetTransferSpeed(EVETransferSpeed speed);

  EVEMMapMode GetMMapMode() noexcept;
  EVETransferMode GetTransferMode() const noexcept;
  EVETransferSpeed GetTransferSpeed() const noexcept;

  EVETargetPHY() noexcept : mmap_mode(MMAP_UNKNOWN), transfer_mode(TRANSFERMODE_SINGLE) {}

private:
  EVEMMapMode mmap_mode;
  EVETransferMode transfer_mode;

  void SetTransferMode(EVETransferMode mode);

  void configure_main_mmap();
  void configure_func_mmap();
};

#endif

#endif /* INC_EVE_MYTARGET_H_ */
