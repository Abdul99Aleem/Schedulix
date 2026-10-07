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
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <mcp2515/connect.h>
#include <mcp2515/logging.h>
#include <mcp2515/util.h>

int mcp2515_connect(mcp2515_state_t* state, const char* spiPath) {
    int rc = EOK;
    int fd = -1;
    char* localpath = NULL;
    char errStr[ERR_STR_SIZE];

    localpath = strdup(spiPath);
    if (localpath == NULL) {
        LOGE("Unable to duplicate spiPath: %s", spiPath);
        rc = ENOMEM;
        goto error;
    }

    fd = open(spiPath, O_RDWR);
    if (fd == -1) {
        rc = errno;
        LOGE("Unable to open '%s'. Error=%s", spiPath, sstrerror(rc, errStr, sizeof(errStr)));
        goto error;
    }

    LOGI("Connected to %s with fd %d", spiPath, fd);
    state->spiPath = localpath;
    state->fd = fd;
    return EOK;

error:
    if (fd != -1) {
        close(fd);
    }
    free(localpath);
    return rc;
}

void mcp2515_disconnect(mcp2515_state_t* state) {
    if (state->fd != -1) {
        close(state->fd);
        state->fd = -1;
    }
    free((void*)state->spiPath);
    state->spiPath = NULL;
}
