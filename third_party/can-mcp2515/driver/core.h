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

#ifndef CAN_MCP2515_DRIVER_CORE_H_
#define CAN_MCP2515_DRIVER_CORE_H_

#include "drv_state.h"

// Main entry point of the core code
// This will parse the command line, configure, and start the driver.
// (by calling parse_command_line(), connect(), configureLibcan(), and go())
/**
 * Main entry point of the driver.
 *
 * It is meant to be called from main() and does all the parsing, configuration,
 * and execution required for the driver. It will only return when it is time
 * for the driver to exit.
 *
 * @param[in]   state   The INITIALIZED state context for the driver. It is
 *                      up to the caller to call drv_state_init() before the
 *                      call to execute(), and to call drv_state_deinit()
 *                      after the call to execute() returns.
 * @param[in]   argc    The argc value passed to main by the OS.
 * @param[in]   argv    The argv value passed to main by the OS.
 */
int execute(drv_state_t* dstate, int argc, char* argv[]);

#endif  // CAN_MCP2515_DRIVER_CORE_H_
