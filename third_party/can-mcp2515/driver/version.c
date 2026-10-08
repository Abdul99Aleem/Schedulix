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
#include <stdio.h>
#include <string.h>

#include "version.h"

#ifndef CAN_MCP2515_VERSION_MAJOR
#error CAN_MCP2515_VERSION_MAJOR is not defined!
#endif
#ifndef CAN_MCP2515_VERSION_MINOR
#error CAN_MCP2515_VERSION_MINOR is not defined!
#endif
#ifndef CAN_MCP2515_VERSION_PATCH
#error CAN_MCP2515_VERSION_PATCH is not defined!
#endif
#ifndef CAN_MCP2515_REPO_STATUS
#error CAN_MCP2515_REPO_STATUS is not defined!
#endif

void print_version(void) {
    char buffer[128];
    char* ptr = buffer;

    ptr += snprintf(ptr, sizeof(buffer)-(ptr-buffer), "Version: %d.%d.%d\n",
            CAN_MCP2515_VERSION_MAJOR, CAN_MCP2515_VERSION_MINOR,
            CAN_MCP2515_VERSION_PATCH);
    if (strlen(CAN_MCP2515_REPO_STATUS) != 0) {
        ptr += snprintf(ptr, sizeof(buffer)-(ptr-buffer), "Repo Status:%s\n",
                        CAN_MCP2515_REPO_STATUS);
    }
    fputs(buffer, stdout);
}
