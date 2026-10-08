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

#include <mcp2515/mcp2515.h>
#include <mcp2515/state.h>

#include "drv_state.h"
#include "rpi4.h"

#define DEFAULT_BITRATE 500000
#define DEFAULT_MSG_QUEUE_SIZE 256
#define DEFAULT_WAIT_QUEUE_SIZE 8

void drv_state_init(drv_state_t* state) {
    memset(state, 0, sizeof(*state));

    mcp2515_state_init(&state->mcp2515);

    state->gpio = DRV_STATE_GPIO_UNSET;
    state->bps = DEFAULT_BITRATE;
    state->numTxNodes = MCP2515_NUM_TX_BUFFERS;
    state->numRxNodes = MCP2515_NUM_RX_BUFFERS;
    state->mode = CANDEV_MODE_IO;
    state->msgQueueSize = DEFAULT_MSG_QUEUE_SIZE;
    state->waitQueueSize = DEFAULT_WAIT_QUEUE_SIZE;
    state->irqId = -1;
}

void drv_state_deinit(drv_state_t* state) {
    if (state != NULL) {
        mcp2515_state_deinit(&state->mcp2515);
        platform_rpi4_deinitialize(state);
    }
}
