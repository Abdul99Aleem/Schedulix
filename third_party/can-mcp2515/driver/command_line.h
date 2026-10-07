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

#ifndef CAN_MCP2515_DRIVER_COMMAND_LINE_H_
#define CAN_MCP2515_DRIVER_COMMAND_LINE_H_

/**
 * @file
 *
 * API to parse the command line.
 */

#include "drv_state.h"

/**
 * Print the help for this application to stdout.
 */
void help(void);

/**
 * Parse the command line for the application.
 *
 * The passed in state should be in its default, freshly initialized state.
 *
 * @param[in]   argc    The argc element passed to the application.
 * @param[in]   argv    The argv element passed to the application.
 * @param[out]  state   The state for the driver. Its configuration members
 *                      will be set as directed by the command line being
 *                      parsed.
 *
 * @return EOK(0) if successful, one of the other errno codes on failure.
 */
int parse_command_line(int argc, char* argv[], drv_state_t* state);

#endif  // CAN_MCP2515_DRIVER_COMMAND_LINE_H_
