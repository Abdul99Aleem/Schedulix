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
#include <stdlib.h>
#include <unistd.h>

#include <mcp2515/bit_timing.h>
#include <mcp2515/state.h>

void mcp2515_state_init(mcp2515_state_t* state) {
    memset(state, 0, sizeof(*state));

    state->fd = -1;

    state->cookie = MCP2515_STATE_INIT_COOKIE;
}

void mcp2515_state_deinit(mcp2515_state_t* state) {
    if (state->cookie == MCP2515_STATE_INIT_COOKIE) {
        if (state->fd != -1) {
            close(state->fd);
            state->fd = -1;
        }

        free((char*)state->spiPath);
        state->spiPath = NULL;

        state->cookie = 0;
    }
}
