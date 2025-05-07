/*
 * cpp_main.h
 *
 *  Created on: Feb 23, 2025
 *      Author: Alex
 */

#ifndef INC_CPP_MAIN_H_
#define INC_CPP_MAIN_H_


#define MAIN_LOOP_PERIOD_MS 10


#ifdef __cplusplus

#include <exception>
#include <stdexcept>
#include <string>
#include <stdio.h>

//inline string formatting macro, limited to 256 characters: "action" is performed with the format result in "__msg"
#define InlineFormat(action, fmt, ...) do { char __msg[256]; snprintf(__msg, 256, fmt, __VA_ARGS__); (action); } while (0)

//quick error-throw macro with custom error message
#define ThrowOnHALErrorMsg(x, msg) do { HAL_StatusTypeDef __res = (x); \
  if (__res != HAL_OK) InlineFormat(throw DriverError((DriverErrorType)__res, __msg), "%s error type %d @ %s l. %d", msg, __res, __FILE__, __LINE__); } while (0)
//quick error-throw macro
#define ThrowOnHALError(x) ThrowOnHALErrorMsg(x, "HAL")

//quick exception-ignore macro
#define IgnoreExceptions(x) do { try { (x); } catch (...) {} } while (0)

extern "C"
{
#endif


#include "main.h"


int cpp_main();


#ifdef __cplusplus
}

typedef enum {
  DRV_FAILED = HAL_ERROR,
  DRV_BUSY = HAL_BUSY,
  DRV_TIMEOUT = HAL_TIMEOUT
} DriverErrorType;

class DriverError : public std::runtime_error {
public:
  const DriverErrorType type;

  DriverError(DriverErrorType type, const std::string& desc) : std::runtime_error(desc), type(type) {}
  DriverError(DriverErrorType type, const char* desc) : std::runtime_error(desc), type(type) {}
};

#endif



#endif /* INC_CPP_MAIN_H_ */
