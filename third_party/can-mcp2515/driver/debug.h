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

#ifndef CAN_MCP2515_DRIVER_DEBUG_H_
#define CAN_MCP2515_DRIVER_DEBUG_H_

/**
 * @file
 *
 * APIs used to generate debug information in the logging sinks.
 */

#include "drv_state.h"

/**
 * Generate logging events with the values of the MCP2515's various status
 * registers. The values of the registers are read at the time of the call.
 *
 * @param[in]   state   The state context for the driver
 */
void dump_mcp2515_status_registers(drv_state_t* state);

/**
 * Generate logging events with the values of an RX buffer's filtering
 * (MID/MASK) registers. The values of the registers are read at the
 * time of the call.
 *
 * @param[in]   state   The state context for the driver
 * @param[in]   rxId    The RX buffer
 */
void dump_mcp2515_rx_filter_registers(drv_state_t* state, int rxId);

/**
 * Generate logging events with the values of ALL the RX buffers'
 * filtering (MID/MASK) registers. The values of the registers
 * are read at the time of the call.
 *
 * @param[in]   state   The state context for the driver
 */
void dump_mcp2515_all_rx_filter_registers(drv_state_t* state);

/**
 * Generate logging events suitable for debugging problems with the
 * MCP2515.
 *
 * Currently it consists of all the status registers, and the RX filter
 * registers for all the RX buffers.
 *
 * @param[in]   state   The state context for the driver
 */
void dump_debug_info(drv_state_t* state);

#endif  // CAN_MCP2515_DRIVER_COMMAND_LINE_H_
