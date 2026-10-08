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
#include <stdarg.h>
#include <stdint.h>
#include <sys/slog2.h>
#include <time.h>
#include <unistd.h>

#include <mcp2515/logging.h>

#define CONSOLE_BUFFER_SIZE 1024

static const char* LevelStrings[] = {
    "SHUTDOWN", "CRITICAL", "ERROR   ", "WARNING ",
    "NOTICE  ", "INFO    ", "DEBUG1  ", "DEBUG2  "
};

mcp2515_log_cfg_t MCP2515_LogCfg;

void mcp2515_do_log(uint8_t level, const char* fmt, ...) {
    va_list ap;

    if (MCP2515_LogCfg.sinks & MCP2515_LOG_SINK_SLOG2) {
        va_start(ap, fmt);
        vslog2f(MCP2515_LogCfg.slog2Buffer, 0, level, fmt, ap);
        va_end(ap);
    }

    if (MCP2515_LogCfg.sinks & MCP2515_LOG_SINK_CONSOLE) {
        char buffer[CONSOLE_BUFFER_SIZE];
        char* ptr;
        struct timespec ts;
        time_t t;
        uint32_t millis;
        struct tm tp;

        // Figure out seconds/milliseconds
        timespec_get(&ts, TIME_UTC);
        millis = ts.tv_nsec / (1000 * 1000);  // NOLINT
        t = ts.tv_sec + millis / 1000;        // NOLINT
        millis %= 1000;                       // NOLINT
        localtime_r(&t, &tp);

        va_start(ap, fmt);
        ptr = buffer;
        ptr += snprintf(ptr, sizeof(buffer) - (ptr - buffer), "%s ", LevelStrings[level]);
        ptr += strftime(ptr, sizeof(buffer) - (ptr - buffer), "%h %d %H:%M:%S", &tp);
        ptr += snprintf(ptr, sizeof(buffer) - (ptr - buffer), ".%03d %u.%u - ", millis, getpid(), gettid());
        ptr += vsnprintf(ptr, sizeof(buffer) - (ptr - buffer), fmt, ap);
        va_end(ap);

        puts(buffer);
    }
}

int mcp2515_log_init(const char* name, uint8_t sinks, uint8_t level) {
    // Make sure inputs are valid
    if ((sinks & MCP2515_LOG_SINK_SLOG2) && !name) return EINVAL;
    if ((sinks & ~(MCP2515_LOG_SINK_SLOG2 | MCP2515_LOG_SINK_CONSOLE)) != 0) return EINVAL;

    memset(&MCP2515_LogCfg, 0, sizeof(MCP2515_LogCfg));

    if (sinks & MCP2515_LOG_SINK_SLOG2) {
        slog2_buffer_set_config_t bCfg = {.num_buffers = 1,
            .buffer_set_name = name,
            .verbosity_level = level,
            .buffer_config = {{"main", 1}},
            .max_retries = UINT32_MAX};

        if (slog2_register(&bCfg, &MCP2515_LogCfg.slog2Buffer, SLOG2_TRY_REUSE_BUFFER_SET)) return errno;
    }

    MCP2515_LogCfg.level = level;
    MCP2515_LogCfg.sinks = sinks;

    return EOK;
}

void mcp2515_log_deinit(void) {
    if (MCP2515_LogCfg.slog2Buffer) {
        // I can't call slog2_reset() as it will destroy all resources
        // allocated by the process and since I'm a library I don't know
        // if the process still needs its buffers. So instead I will
        // leak it and hope the process will either exit, or call slog2_reset()
        MCP2515_LogCfg.slog2Buffer = NULL;
    }
    memset(&MCP2515_LogCfg, 0, sizeof(MCP2515_LogCfg));
}
