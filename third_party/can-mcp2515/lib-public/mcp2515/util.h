/*
 * Copyright (c) 2025, BlackBerry Limited. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef LIB_PUBLIC_MCP2515_UTIL_H_
#define LIB_PUBLIC_MCP2515_UTIL_H_

#include <gulliver.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/**
 * @file
 *
 * Handy utilities and constants.
 */

#define CAN_MAXLEN 8            ///< Maximum payload of a CAN frame
#define CANFD_MAXLEN 64         ///< Maximum payload of a CAN-FD frame

#define CAN_SID_MASK ((1 << 11) - 1)    ///< Mask for an 11bit SID
#define CAN_SID_MAX ((1 << 11) - 1)     ///< Maximum valid SID value
#define CAN_EID_MASK ((1 << 29) - 1)    ///< Mask for a 29bit EID
#define CAN_EID_MAX ((1 << 29) - 1)     ///< Maximum valid EID value

/**
 * Macro to return the offset, in bytes, of a member in a struct.
 *
 * @param[in] VAR       The name of the member in the struct
 * @param[in] TYPE      The structure's type
 *
 * @note Compilation will fail if either of the parameters are invalid.
 *
 * @return The byte offset of the member within a struct.
 */
#define OFFSET_OF(VAR, TYPE) ((uintptr_t) & (((TYPE*)0)->VAR))

/**
 * Macro to return a pointer to thing that contains some element.
 * Think of it as a way to go from the pointer of myStruct.myMember to a
 * pointer to just myStruct.
 *
 * @note Compilation will fail if either TYPE or ELEMENT are invalid.
 *
 * @param[in] VAR       The pointer to the element
 * @param[in] TYPE      The type of the container
 * @param[in] ELEMENT   The name of the element, in container, that VAR points to.
 *
 * @return A pointer to TYPE that contains the ELEMENT VAR.
 */
#define CONTAINER_OF(VAR, TYPE, ELEMENT) ((TYPE*)((unsigned char*)(VAR)-OFFSET_OF(ELEMENT, TYPE)))

#if defined(__LITTLEENDIAN__)
#define ENDIAN_HTOLE16(x) (x)
#define ENDIAN_HTOLE32(x) (x)
#define ENDIAN_HTOLE64(x) (x)
#elif defined(__BIGENDIAN__)
#define ENDIAN_HTOLE16(x) ENDIAN_SWAP16((x))
#define ENDIAN_HTOLE32(x) ENDIAN_SWAP32((x))
#define ENDIAN_HTOLE64(x) ENDIAN_SWAP64((x))
#else
#error I do not know what my endianess is :(
#endif

/**
 * Clamp a value to be within a specified range.
 *
 * @param[in] x         The value to clamp
 * @param[in] lower     The lower bound of the valid range.
 * @param[in] upper     The lower bound of the valid range.
 *
 * @return The value x if it is in the range lower <= x <= upper. Otherwise
 *         it returns lower if x < lower or upper if x > upper.
 */
#define CLAMP(x, lower, upper) ((x) <= (upper) ? ((x) >= (lower) ? (x) : (lower)) : (upper))

/**
 * Calculates the absolute difference, ignoring signing, of two values.
 *
 * @param[in] a The first value
 * @param[in] b The second value
 *
 * @return The absolute difference between a and b
 */
#define ABS_DIFF(a, b) ((a) <= (b) ? (b) - (a) : (a) - (b))

/**
 * Converts a value in seconds to the equivalent number of nanoseconds.
 *
 * @param[in] s The number of seconds
 *
 * @return The equivalent number of nanoseconds
 */
#define SEC_TO_NS(s) ((s) * (1000ull * 1000ull * 1000ull))

/**
 * Get a 32bit SW timestamp, in us.
 *
 * @return A 32bit SW timestamp in us. It starts at 0 on system reset and
 *         rolls over every 71 minutes or so.
 */
static inline uint32_t get_sw_timestamp() {
  struct timespec ts = {0, 0};
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint32_t)(timespec2nsec(&ts) / 1000);  // NOLINT
}

/**
 * Maximum size of an error string, including NULL-terminator, returned by
 * sstrerror().
 */
#define ERR_STR_SIZE 128

/**
 * Replacement for strerror and strerror_r
 *
 * strerror() isn't thread safe and strerror_r() is clunky to use in
 * printf() style functions.
 *
 * Try to deal with both of these with a helper.
 *
 * @param[in] err       The errno to convert to human readable string
 * @param[in] buf       The buffer to place the human readable string
 * @param[in] size      The size, in bytes, of the buffer @ref buf.
 *
 * @return A pointer to @ref buf with either the human readable errno string,
 *         or the string "UNKNOWN(num)" if there is a problem.
 */
static inline char* sstrerror(int err, char* buf, int size) {
  int rc = strerror_r(err, buf, size);
  if (rc) {
    snprintf(buf, size, "UNKNOWN(%d)", err);
  }
  return buf;
}

#endif  // LIB_PUBLIC_MCP2515_UTIL_H_
