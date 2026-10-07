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
#include <stdio.h>
#include <string.h>
#include <sys/slog2.h>

#include "core.h"
#include "drv_state.h"

static drv_state_t DState;

int main(int argc, char* argv[]) {
    drv_state_init(&DState);
    if (execute(&DState, argc, argv) != EOK) {
        return 1;
    }
    drv_state_deinit(&DState);

    return 0;
}
