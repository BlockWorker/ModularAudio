/*
******************************************************************************
**
**  File        : LinkerScript.ld
**
**  Author      : STM32CubeIDE
**
**  Abstract    : Linker script for STM32H7 series
**                1024Kbytes FLASH and 560Kbytes RAM
**
**                Set heap size, stack size and stack location according
**                to application requirements.
**
**                Set memory bank area and size if external memory is used.
**
**  Target      : STMicroelectronics STM32
**
**  Distribution: The file is distributed as is, without any warranty
**                of any kind.
**
*****************************************************************************
** @attention
**
** Copyright (c) 2023 STMicroelectronics.
** All rights reserved.
**
** This software is licensed under terms that can be found in the LICENSE file
** in the root directory of this software component.
** If no LICENSE file comes with this software, it is provided AS-IS.
**
****************************************************************************
*/

/* Entry Point */
ENTRY(Reset_Handler)

/* Highest address of the user mode heap and stack */
_eheap = ORIGIN(RAM_D1) + LENGTH(RAM_D1);       /* end of RAM_D1 */
_estack = ORIGIN(DTCMRAM) + LENGTH(DTCMRAM);    /* end of DTCMRAM */
/* Generate a link error if heap and stack don't fit into RAM */
_Min_Heap_Size = 0x2000;      /* required amount of heap  */
_Min_Stack_Size = 0x2000; /* required amount of stack */
/* Reserve space at start of memory space, to avoid overwriting ITCMRAM code in case of a null-pointer error */
_Null_Pointer_Buffer_Size = 0x1000;

/* Specify the memory areas */
MEMORY
{
  ITCMRAM (xrw)    : ORIGIN = 0x00000000,   LENGTH = 64K
  DTCMRAM (xrw)    : ORIGIN = 0x20000000,   LENGTH = 128K
  FLASH    (rx)    : ORIGIN = 0x08000000,   LENGTH = 1024K
  RAM_D1  (xrw)    : ORIGIN = 0x24000000,   LENGTH = 320K
  RAM_D2  (xrw)    : ORIGIN = 0x30000000,   LENGTH = 32K
  RAM_D3  (xrw)    : ORIGIN = 0x38000000,   LENGTH = 16K
}

/* Define output sections */
SECTIONS
{
  /* The startup code goes first into FLASH */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector)) /* Startup code */
    . = ALIGN(4);
  } >FLASH
  
  /* Safety buffer at start of ITCMRAM so that any potential null pointer errors don't overwrite RAM code */
  ._itcm_null_pointer_buffer (NOLOAD) :
  {
    . = ALIGN(4);
    . = . + _Null_Pointer_Buffer_Size;
    . = ALIGN(4);
  } > ITCMRAM
  
  /* ITCMRAM section for RAM code (text) */
  ._itcm_ram_text :
  {
    . = ALIGN(4);
    _BeginITCMText = .;
    *(.RamFunc)                     /* .RamFunc sections */
    *(.RamFunc*)                    /* .RamFunc* sections */
    *libCMSIS-DSP*:*(.text .text*)  /* CMSIS DSP library (for performance) */
    /**(.text.memcpy)                 /* Standard C memcpy function */
    /**(.text.memset)                 /* Standard C memset function */
    . = ALIGN(4);
    _EndITCMText = .;
  } > ITCMRAM AT> FLASH
  _InitITCMText = LOADADDR (._itcm_ram_text);

  /* The program code and other data goes into FLASH */
  .text :
  {
    . = ALIGN(4);
    *(.text)           /* .text sections (code) */
    *(.text*)          /* .text* sections (code) */
    *(.glue_7)         /* glue arm to thumb code */
    *(.glue_7t)        /* glue thumb to arm code */
    *(.eh_frame)

    KEEP (*(.init))
    KEEP (*(.fini))

    . = ALIGN(4);
    _etext = .;        /* define a global symbols at end of code */
  } >FLASH

  /* Constant data goes into FLASH */
  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)         /* .rodata sections (constants, strings, etc.) */
    *(.rodata*)        /* .rodata* sections (constants, strings, etc.) */
    . = ALIGN(4);
  } >FLASH

  .ARM.extab   : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >FLASH
  .ARM : {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } >FLASH

  .preinit_array     :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array*))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  } >FLASH

  .init_array :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT(.init_array.*)))
    KEEP (*(.init_array*))
    PROVIDE_HIDDEN (__init_array_end = .);
  } >FLASH

  .fini_array :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT(.fini_array.*)))
    KEEP (*(.fini_array*))
    PROVIDE_HIDDEN (__fini_array_end = .);
  } >FLASH

  /* used by the startup to initialize data */
  _sidata = LOADADDR(.data);

  /* Initialized data sections goes into RAM, load LMA copy after code */
  .data :
  {
    . = ALIGN(4);
    _sdata = .;        /* create a global symbol at data start */
    *(.data)           /* .data sections */
    *(.data*)          /* .data* sections */

    . = ALIGN(4);
    _edata = .;        /* define a global symbol at data end */
  } >RAM_D1 AT> FLASH

  /* Uninitialized data section */
  . = ALIGN(4);
  .bss :
  {
    /* This is used by the startup in order to initialize the .bss section */
    _sbss = .;         /* define a global symbol at bss start */
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)

    . = ALIGN(4);
    _ebss = .;         /* define a global symbol at bss end */
    __bss_end__ = _ebss;
  } >RAM_D1

  /* User_heap section, used to check that there is enough RAM_D1 left */
  ._user_heap (NOLOAD) :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    . = . + _Min_Heap_Size;
    . = ALIGN(8);
  } > RAM_D1
  
  /* Extra blocks for D2, D3, and DTCM RAM availability */
  ._d2_bss_block (NOLOAD) :
  {
    . = ALIGN(4);
    _BeginD2Bss = .;
    KEEP(*(.d2_bss))
    . = ALIGN(4);
    _EndD2Bss = .;
  } > RAM_D2
  
  ._d2_data_block :
  {
    . = ALIGN(4);
    _BeginD2Data = .;
    KEEP(*(.d2_data))
    . = ALIGN(4);
    _EndD2Data = .;
  } > RAM_D2 AT> FLASH
  _InitD2Data = LOADADDR (._d2_data_block);
  
  ._d3_bss_block (NOLOAD) :
  {
    . = ALIGN(4);
    _BeginD3Bss = .;
    KEEP(*(.d3_bss))
    . = ALIGN(4);
    _EndD3Bss = .;
  } > RAM_D3
  
  ._d3_data_block :
  {
    . = ALIGN(4);
    _BeginD3Data = .;
    KEEP(*(.d3_data))
    . = ALIGN(4);
    _EndD3Data = .;
  } > RAM_D3 AT> FLASH
  _InitD3Data = LOADADDR (._d3_data_block);
  
  ._dtcm_bss_block (NOLOAD) :
  {
    . = ALIGN(4);
    _BeginDTCMBss = .;
    KEEP(*(.dtcm_bss))
    . = ALIGN(4);
    _EndDTCMBss = .;
  } > DTCMRAM
  
  ._dtcm_data_block :
  {
    . = ALIGN(4);
    _BeginDTCMData = .;
    KEEP(*(.dtcm_data))
    . = ALIGN(4);
    _EndDTCMData = .;
  } > DTCMRAM AT> FLASH
  _InitDTCMData = LOADADDR (._dtcm_data_block);
  
  /* User_stack section, used to check that there is enough DTCMRAM left */
  ._user_stack (NOLOAD) :
  {
    . = ALIGN(8);
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } > DTCMRAM
  
  /* ITCM data block; intended for read-only data (but could theoretically house read-write data too) */
  ._itcm_data_block :
  {
    . = ALIGN(4);
    _BeginITCMData = .;
    KEEP(*(.itcm_data))
    . = ALIGN(4);
    _EndITCMData = .;
  } > ITCMRAM AT> FLASH
  _InitITCMData = LOADADDR (._itcm_data_block);
  

  /* Remove information from the standard libraries */
  /DISCARD/ :
  {
    libc.a ( * )
    libm.a ( * )
    libgcc.a ( * )
  }

  .ARM.attributes 0 : { *(.ARM.attributes) }
}
