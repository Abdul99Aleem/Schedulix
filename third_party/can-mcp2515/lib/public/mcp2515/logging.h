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

#ifndef LIB_PUBLIC_MCP2515_LOGGING_H_
#define LIB_PUBLIC_MCP2515_LOGGING_H_

/**
 * @file
 *
 * A simple logging interface used by the library, and clients of it.
 *
 * It can log to stdout, slog2, or both.
 */

#include <stdint.h>
#include <sys/slog2.h>

#define MCP2515_LOG_SINK_NONE 0x0       ///< The NULL sink
#define MCP2515_LOG_SINK_SLOG2 0x1      ///< Sink logging event to SLOG2
#define MCP2515_LOG_SINK_CONSOLE 0x2    ///< Sink logging event to stdout/console

/**
 * Configuration for the logging system
 */
struct mcp2515_log_cfg {
  uint8_t sinks;        ///< What sinks are active
  uint8_t level;        ///< Current logging level. Corresponds to the various SLGO2_* error levels.

  /**
   * The slog2 buffer allocated for logging by this system
   */
  slog2_buffer_t slog2Buffer;
};
typedef struct mcp2515_log_cfg mcp2515_log_cfg_t;

/**
 * Global logging configuration.
 *
 * Usually initialized by a call to mcp2515_log_init().
 *
 * The only member that can be modified in this directly is the
 * MCP2515_LogCfg.level member in order to dynamically change logging verbosity.
 */
extern mcp2515_log_cfg_t MCP2515_LogCfg;

/**
 * Record a log event
 *
 * This API is not meant to be called directly. Use one of the LOG?()
 * macros instead.
 *
 * @param[in]   LEVEL   The logging level of the event. One of the SLOG2_
 *                      log levels.
 * @param[in]   FMT     A printf style format string. It does NOT need to
 *                      be a string literal.
 * @param[in]   ...     Variable number of arguments suitable for the specified
 *                      format string.
 */
void mcp2515_do_log(uint8_t level, const char* fmt, ...);

/**
 * Log an event if it meets the current vebosity level.
 *
 * The specified level is compared to the current logging verbosity. If the
 * specified level is covered by the current verbosity, the event is logged.
 * Otherwise it does nothing.
 *
 * @param[in]   LEVEL   The logging level of the event. One of the SLOG2_
 *                      log levels.
 * @param[in]   FMT     A printf style format string. It does NOT need to
 *                      be a string literal.
 * @param[in]   ...     Variable number of arguments suitable for the specified
 *                      format string.
 */
#define MCP2515_DO_LOG(LEVEL, FMT, ...) \
  if (MCP2515_LogCfg.sinks && MCP2515_LogCfg.level >= (LEVEL)) mcp2515_do_log(LEVEL, FMT, ##__VA_ARGS__)

/**
 * Log an event at the SHUTDOWN level
 *
 * @note Events at the SHUTDOWN level are _always_ logged.
 *
 * @param[in]   FMT     A printf style format string. It does NOT need to
 *                      be a string literal.
 * @param[in]   ...     Variable number of arguments suitable for the specified
 *                      format string.
 */
#define LOGS(FMT, ...) mcp2515_do_log(SLOG2_SHUTDOWN, FMT, ##__VA_ARGS__)

/**
 * Log an event at the CRITICAL level
 *
 * @note Events at the CRITICAL level are only logged if the current
 *       logging verbosity is at the CRITICAL level or higher.
 *
 * @param[in]   FMT     A printf style format string. It does NOT need to
 *                      be a string literal.
 * @param[in]   ...     Variable number of arguments suitable for the specified
 *                      format string.
 */
#define LOGC(FMT, ...) MCP2515_DO_LOG(SLOG2_CRITICAL, FMT, ##__VA_ARGS__)

/**
 * Log an event at the ERROR level
 *
 * @note Events at the ERROR level are only logged if the current
 *       logging verbosity is at the ERROR level or higher.
 *
 * @param[in]   FMT     A printf style format string. It does NOT need to
 *                      be a string literal.
 * @param[in]   ...     Variable number of arguments suitable for the specified
 *                      format string.
 */
#define LOGE(FMT, ...) MCP2515_DO_LOG(SLOG2_ERROR, FMT, ##__VA_ARGS__)

/**
 * Log an event at the WARNING level
 *
 * @note Events at the WARNING level are only logged if the current
 *       logging verbosity is at the WARNING level or higher.
 *
 * @param[in]   FMT     A printf style format string. It does NOT need to
 *                      be a string literal.
 * @param[in]   ...     Variable number of arguments suitable for the specified
 *                      format string.
 */
#define LOGW(FMT, ...) MCP2515_DO_LOG(SLOG2_WARNING, FMT, ##__VA_ARGS__)

/**
 * Log an event at the NOTICE level
 *
 * @note Events at the NOTICE level are only logged if the current
 *       logging verbosity is at the NOTICE level or higher.
 *
 * @param[in]   FMT     A printf style format string. It does NOT need to
 *                      be a string literal.
 * @param[in]   ...     Variable number of arguments suitable for the specified
 *                      format string.
 */
#define LOGN(FMT, ...) MCP2515_DO_LOG(SLOG2_NOTICE, FMT, ##__VA_ARGS__)

/**
 * Log an event at the INFOrmational level
 *
 * @note Events at the INFO level are only logged if the current
 *       logging verbosity is at the INFO level or higher.
 *
 * @param[in]   FMT     A printf style format string. It does NOT need to
 *                      be a string literal.
 * @param[in]   ...     Variable number of arguments suitable for the specified
 *                      format string.
 */
#define LOGI(FMT, ...) MCP2515_DO_LOG(SLOG2_INFO, FMT, ##__VA_ARGS__)

/**
 * Log an event at the DEBUG1 level
 *
 * @note Events at the DEBUG1 level are only logged if the current
 *       logging verbosity is at the DEBUG1 level or higher.
 *
 * @param[in]   FMT     A printf style format string. It does NOT need to
 *                      be a string literal.
 * @param[in]   ...     Variable number of arguments suitable for the specified
 *                      format string.
 */
#define LOGD1(FMT, ...) MCP2515_DO_LOG(SLOG2_DEBUG1, FMT, ##__VA_ARGS__)

/**
 * Log an event at the DEBUG2 level
 *
 * @note Events at the DEBUG2 level are only logged if the current
 *       logging verbosity is at the DEBUG2 level or higher.
 *
 * @param[in]   FMT     A printf style format string. It does NOT need to
 *                      be a string literal.
 * @param[in]   ...     Variable number of arguments suitable for the specified
 *                      format string.
 */
#define LOGD2(FMT, ...) MCP2515_DO_LOG(SLOG2_DEBUG2, FMT, ##__VA_ARGS__)

/**
 * Initialize the logging system
 *
 * This MUST be called before any of the other logging APIs are used.
 *
 * @param[in]   name    The name to use when registering a slog2 buffer.
 *                      Usually this is the name of the application.
 *                      May be NULL if the SLOG2 sink isn't required.
 * @param[in]   sinks   The sinks that will be used created by OR'ing together
 *                      the desired MCP2515_LOG_SINK_* constants.
 * @param[in]   level   The starting verbosity level. Should be one of the
 *                      SLOG2_* verbosity levels.
 *
 * @return EOK(0) if successful, one of the other errno codes on failure.
 */
int mcp2515_log_init(const char* name, uint8_t sinks, uint8_t level);

/**
 * Deinitializes the logging system.
 */
void mcp2515_log_deinit(void);

#endif  // LIB_PUBLIC_MCP2515_LOGGING_H_
