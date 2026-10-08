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

#ifndef LIB_PUBLIC_MCP2515_COMMAND_H_
#define LIB_PUBLIC_MCP2515_COMMAND_H_

/**
 * @file
 *
 * APIs associated with commanding the MCP2515 to do something.
 *
 * Contrast this with the APIs in io.h, which expose the underlying SPI
 * protocol of the MCP2515.
 */

#include <assert.h>

#include <sys/can_dcmd.h>

#include "bit_timing.h"
#include "mcp2515.h"
#include "state.h"

/**
 * Set the operating mode of the MCP2515.
 *
 * Changing the operating mode is done after all pending transmissions are
 * completed. Thus this API will block if there are pending transmissions.
 *
 * @note Changing the operating mode can reset several internal counters.
 *       Please see the MCP2515's datasheet for details.
 *
 * @param[in]   state   The state context for the MCP2515
 * @param[in]   opmod   The desired operating mode
 *
 * @return EOK(0) if successful, one of the other errno codes on failure.
 */
int mcp2515_cmd_set_op_mode(mcp2515_state_t* state, mcp2515_canstat_opmod_t opmod);

/**
 * Program the bit timings of the MCP2515.
 *
 * The bit timings can only be set when the MCP2515's opmod is CONFIG. Trying
 * to do so in a different mode results in undefined behaviour.
 *
 * @param[in]   state   The state context for the MCP2515
 * @param[in]   bt      The calculated bit-timings for the desired bitrate.
 *                      See bit_timing.h.
 *
 * @return EOK(0) if successful, one of the other errno codes on failure.
 */
int mcp2515_cmd_set_bit_timings(mcp2515_state_t* state, const can_bit_timings_t* bt);

/**
 * Program the MID filter for one of the MCP2515's RX buffers.
 *
 * A MID filter can only be set when the MCP2515's opmod is CONFIG. Trying
 * to do so in a different mode results in undefined behaviour.
 *
 * @note While each MCP2515 RX buffer supports multiple MID filters, only one
 *       can be set for each RX buffer.
 *
 * @param[in]   state   The state context for the MCP2515
 * @param[in]   rxIdx   The index of the RX buffer to program
 * @param[in]   mid     The MID to program.
 *                      A SID occupies bits [18:28] of the MID.
 *                      An EID occupies bits [0:28] of the MID.
 * @param[in]   isEid   Is the MID an EID (non-zero) or a SID (zero)
 *
 * @return EOK(0) if successful, one of the other errno codes on failure.
 */
int mcp2515_cmd_set_rx_mid_filter(mcp2515_state_t* state, int rxIdx, uint32_t mid, int isEid);

/**
 * Program the MID mask for one of the MCP2515's RX buffers.
 *
 * A MID mask can only be set when the MCP2515's opmod is CONFIG. Trying
 * to do so in a different mode results in undefined behaviour.
 *
 * @param[in]   state   The state context for the MCP2515
 * @param[in]   rxIdx   The index of the RX buffer to program
 * @param[in]   mask    The mask to program.
 *                      A SID mask occupies bits [18:28] of the mask.
 *                      An EID mask occupies bits [0:28] of the mask.
 * @param[in]   isEid   Is the mask for an EID (non-zero) or a SID (zero)
 *
 * @return EOK(0) if successful, one of the other errno codes on failure.
 */
int mcp2515_cmd_set_rx_mid_mask(mcp2515_state_t* state, int rxIdx, uint32_t mask, int isEid);

/**
 * Program the reception mode of one of the MCP2515's RX buffers.
 *
 * @param[in]   state   The state context for the MCP2515
 * @param[in]   rxIdx   The index of the RX buffer to program
 * @param[in]   mode    The reception mode
 *
 * @return EOK(0) if successful, one of the other errno codes on failure.
 */
int mcp2515_cmd_set_rx_mode(mcp2515_state_t* state, int rxIdx, mcp2515_rxbnctrl_rxm_t mode);

/**
 * Write a CAN_MSG to a MCP2515 TX buffer and queues it for transmission.
 *
 * The Request-To-Send bit for the specified TX buffer is automatically set
 * by this API.
 *
 * This API does NOT block until the CAN frame is transmitted. It returns
 * once the frame has been accepted by the MCP2515 and the RTS bit is set.
 * To know when the frame is sent you will need to process the generated
 * interrupt and check the empty status of the TX buffer.
 *
 * @param[in]   state   The state context for the MCP2515
 * @param[in]   txBuf   The index of the TX buffer to use for transmission.
 *                      The CAN frame is loaded into this buffer.
 * @param[in]   msg     The CAN frame to load into the buffer and send.
 *
 * @return EOK(0) if successful, one of the other errno codes on failure.
 */
int mcp2515_cmd_tx_can_message(mcp2515_state_t* state, int txBuf, CAN_MSG* msg);

/**
 * Read a CAN_MSG from a MCP2515 RX buffer and clear the associated interrupt
 *
 * This API does NOT block until a CAN frame is available in the buffer.
 * It reads whatever is available in the buffer at the time it is called.
 * To know when a frame is available you will need to process the generated
 * interrupt and check the full status of the RX buffer.
 *
 * The interrupt associated with the indicated RX buffer is automatically
 * cleared by this API.
 *
 * @param[in]   state   The state context for the MCP2515
 * @param[in]   rxBuf   The index of the RX buffer to read data from.
 *                      The CAN frame is loaded from this buffer.
 * @param[out]  msg     The CAN frame is read from the MCP2515 into this buffer
 *
 * @return EOK(0) if successful, one of the other errno codes on failure.
 */
int mcp2515_cmd_rx_can_message(mcp2515_state_t* state, int rxBuf, CAN_MSG* msg);

#endif  // LIB_PUBLIC_MCP2515_COMMAND_H_
