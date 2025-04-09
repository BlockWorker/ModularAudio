/*
 * retarget.c
 *
 *  Created on: Sep 9, 2022
 *      Author: Luc, Alex
 */




// All credit to Carmine Noviello for this code
// https://github.com/cnoviello/mastering-stm32/blob/master/nucleo-f030R8/system/src/retarget/retarget.c

// Modified for asynchronous sending - Alex

#include <_ansi.h>
#include <_syslist.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/times.h>
#include <limits.h>
#include <signal.h>
#include <../Inc/retarget.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#if !defined(OS_USE_SEMIHOSTING)

#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

UART_HandleTypeDef *gHuart;

static uint8_t sendBuf[4096] = { 0 };
static uint16_t buf_writePos = 0;
static uint16_t buf_lastReadPos = 0;

void RetargetInit(UART_HandleTypeDef *huart) {
  gHuart = huart;

  /* Disable I/O buffering for STDOUT stream, so that
   * chars are sent out as soon as they are printed. */
  setvbuf(stdout, NULL, _IONBF, 0);
}

int _isatty(int fd) {
  if (fd >= STDIN_FILENO && fd <= STDERR_FILENO)
    return 1;

  errno = EBADF;
  return 0;
}

int _write(int fd, char* ptr, int len) {
  HAL_StatusTypeDef hstatus;

  if (fd == STDOUT_FILENO || fd == STDERR_FILENO) {
    __disable_irq();

    memcpy(sendBuf + buf_writePos, ptr, len);
    buf_writePos += len;

    if (buf_lastReadPos == 0) {
      hstatus = HAL_UART_Transmit_IT(gHuart, sendBuf, buf_writePos);
      buf_lastReadPos = buf_writePos;
      __enable_irq();
      if (hstatus == HAL_OK)
	return len;
      else
	return EIO;
    } else {
      __enable_irq();
      return len;
    }
  }
  errno = EBADF;
  return -1;
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef* huart) {
  if (huart == gHuart) {
    if (buf_writePos <= buf_lastReadPos) {
      buf_writePos = buf_lastReadPos = 0;
    } else {
      HAL_UART_Transmit_IT(gHuart, sendBuf + buf_lastReadPos, buf_writePos - buf_lastReadPos);
      buf_lastReadPos = buf_writePos;
    }
  }
}

int _close(int fd) {
  if (fd >= STDIN_FILENO && fd <= STDERR_FILENO)
    return 0;

  errno = EBADF;
  return -1;
}

int _lseek(int fd, int ptr, int dir) {
  (void) fd;
  (void) ptr;
  (void) dir;

  errno = EBADF;
  return -1;
}

int _read(int fd, char* ptr, int len) {
  HAL_StatusTypeDef hstatus;

  if (fd == STDIN_FILENO) {
    hstatus = HAL_UART_Receive(gHuart, (uint8_t *) ptr, 1, HAL_MAX_DELAY);
    if (hstatus == HAL_OK)
      return 1;
    else
      return EIO;
  }
  errno = EBADF;
  return -1;
}

int _fstat(int fd, struct stat* st) {
  if (fd >= STDIN_FILENO && fd <= STDERR_FILENO) {
    st->st_mode = S_IFCHR;
    return 0;
  }

  errno = EBADF;
  return 0;
}

int _getpid(void)
{
  return 1;
}

int _kill(int pid, int sig)
{
  (void)pid;
  (void)sig;
  errno = EINVAL;
  return -1;
}

#endif //#if !defined(OS_USE_SEMIHOSTING)
