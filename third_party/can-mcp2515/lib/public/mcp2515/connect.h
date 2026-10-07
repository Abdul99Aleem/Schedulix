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

#ifndef LIB_PUBLIC_MCP2515_CONNECT_H_
#define LIB_PUBLIC_MCP2515_CONNECT_H_

/**
 * @file
 *
 * APIs relating to the connection between the library and the underlying
 * physical SPI link to the MCP2515. 
 */

#include "state.h"

/**
 * Connect to a MCP2515's SPI link
 *
 * @param[in]   state   The state context for the MCP2515 that will be connected
 * @param[in]   spiPath The path to the SPI node representing the MCP2515
 *
 * @return EOK(0) if successful, one of the other errno codes on failure.
 */
int mcp2515_connect(mcp2515_state_t *state, const char* spiPath);

/**
 * Disconnect from a MCP2515's SPI link
 *
 * If there is no active connection this API is a no-op
 *
 * @param[in]   state   The state context for the MCP2515
 */
void mcp2515_disconnect(mcp2515_state_t *state);

#endif  // LIB_PUBLIC_MCP2515_CONNECT_H_
