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

/**
 * @file
 *
 * Things relating to the internal state of the MCP2515 hardware.
 */

#ifndef LIB_PUBLIC_MCP2515_STATE_H_
#define LIB_PUBLIC_MCP2515_STATE_H_

#include <mcp2515/bit_timing.h>
#include <mcp2515/logging.h>

/**
 * An instance of the @ref can_bit_timing_consts_t struct with values
 * suitable for the MCP2515.
 */
extern const can_bit_timing_consts_t MCP2515_BTConsts;

#define MCP2515_STATE_INIT_COOKIE 0x506943414E484154  // PiCANHAT

/**
 * Internal state for the MCP2515 hardware.
 */
struct mcp2515_state {
  uint64_t cookie;              ///< Magic cookie

  /**
   * Path to the SPI node representing the MCP2515.
   * By default will be NULL.
   */
  const char* spiPath;          ///< Path to the SPI node representing the MCP2515 being communicated with

  /**
   * File descriptor for an open connection to the SPI node representing the MCP2515.
   * By default will be -1.
   */
  int fd;
};
typedef struct mcp2515_state mcp2515_state_t;

/**
 * Initialize an mcp2515_state_t instance to default values.
 *
 * @param[in] state     The state to initialize
 */
void mcp2515_state_init(mcp2515_state_t* state);

/**
 * Deinitialize an mcp2515_state_t.
 *
 * This frees any resources managed by the mcp2515_state_t instance.
 *
 * @param[in] state     The state to deinitialize. May be NULL in which case
 *                      this function is a No-op.
 */
void mcp2515_state_deinit(mcp2515_state_t* state);

#endif  // LIB_PUBLIC_MCP2515_STATE_H_
