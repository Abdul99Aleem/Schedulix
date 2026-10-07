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
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "command_line.h"
#include "drv_state.h"
#include "mcp2515/mcp2515.h"
#include "mcp2515/util.h"
#include "version.h"

static int parse_int(const char* name, const char* s, int* i) {
    int rc = EOK;
    char* ch;
    char errStr[ERR_STR_SIZE];

    errno = 0;
    long l = strtol(s, &ch, 0);  // NOLINT
    if (*s != '\0') {
        if (*ch == '\0') {
            if (errno == 0 && l >= INT_MIN && l <= INT_MAX) {
                *i = (int)l;
            } else if (errno == 0) {
                printf("%s has value too large/small: %s\n", name, s);
                rc = ERANGE;
            } else {
                rc = errno;
                printf("%s has value that can't be converted: %s. Error=%s\n", name, s,
                       sstrerror(rc, errStr, sizeof(errStr)));
            }
        } else {
            printf("%s has value that is not an integer @ idx %d: %s\n", name, (int)(ch - s), s);
            rc = EINVAL;
        }
    } else {
        printf("%s has empty value: %s\n", name, s);
        rc = EINVAL;
    }

    return rc;
}

static int parse_uint32(const char* name, const char* s, uint32_t* i) {
    int rc = EOK;
    char* ch;
    char errStr[ERR_STR_SIZE];

    errno = 0;
    unsigned long l = strtoul(s, &ch, 0);  // NOLINT
    if (*s != '\0') {
        if (*ch == '\0') {
            if (errno == 0 && l <= UINT_MAX) {
                *i = (uint32_t)l;
            } else if (errno == 0) {
                printf("%s has value too large/small: %s\n", name, s);
                rc = ERANGE;
            } else {
                rc = errno;
                printf("%s has value that can't be converted: %s. Error=%s\n", name, s,
                       sstrerror(rc, errStr, sizeof(errStr)));
            }
        } else {
            printf("%s has value that is not an integer @ idx %d: %s\n", name, (int)(ch - s), s);
            rc = EINVAL;
        }
    } else {
        printf("%s has empty valuer: %s\n", name, s);
        rc = EINVAL;
    }

    return rc;
}

void help(void) {
    printf("can-mcp2515 - Driver for an MCP2515 CAN Controller\n");
    print_version();
    printf("\nSee 'use can-mcp2515' for usage information.\n");
}

int parse_command_line(int argc, char* argv[], drv_state_t* state) {
    int rc;
    const char* optString = ":s:c:g:3:B:DhM:m:U:u:vW:V";
    struct option longOpts[] = {
        {.name = "spi", .has_arg = required_argument, .val = 's'},
        {.name = "clock", .has_arg = required_argument, .val = 'c'},
        {.name = "gpio", .has_arg = required_argument, .val = 'g'},
        {.name = "triple-sample", .has_arg = required_argument, .val = '3'},
        {.name = "bps", .has_arg = required_argument, .val = 'B'},
        {.name = "debug", .val = 'D'},
        {.name = "help", .val = 'h'},
        {.name = "mid", .has_arg = required_argument, .val = '@'},
        {.name = "msg-queue-size", .has_arg = required_argument, .val = 'M'},
        {.name = "mode", .has_arg = required_argument, .val = 'm'},
        {.name = "user", .has_arg = required_argument, .val = 'U'},
        {.name = "unit", .has_arg = required_argument, .val = 'u'},
        {.name = "verbose", .val = 'v'},
        {.name = "version", .val = 'V'},
        {.name = "wait-queue-size", .has_arg = required_argument, .val = 'W'},
        {0}};
    while ((rc = getopt_long(argc, argv, optString, longOpts, NULL)) != -1) {  // NOLINT
        switch (rc) {
            case 's':
                state->spiPath = optarg;
                break;
            case 'c':
                if ((rc = parse_uint32("clock", optarg, &state->clock)) != EOK) {
                    return rc;
                }
                if (state->clock < MCP2515_MIN_CLOCK_FREQUENCY || state->clock > MCP2515_MAX_CLOCK_FREQUENCY) {
                    printf("Clock of %uHz is out of range. Must be %u-%uHz.\n",
                           state->clock, MCP2515_MIN_CLOCK_FREQUENCY, MCP2515_MAX_CLOCK_FREQUENCY);
                    help();
                    return EINVAL;
                }

                break;
            case 'g':
                if ((rc = parse_int("gpio", optarg, &state->gpio)) != EOK) {
                    return rc;
                }
                break;
            case '3':
                state->tripleSample = 1;
                break;
            case 'B':
                if ((rc = parse_uint32("bps", optarg, &state->bps)) != EOK) {
                    return rc;
                }
                break;
            case 'D':
                state->debug = 1;
                break;
            case 'h':
                help();
                exit(0);  // NOLINT
            case '@':
                if (strcmp("sid", optarg) == 0) {
                    state->midMode = MID_MODE_SID;
                } else if (strcmp("eid", optarg) == 0) {
                    state->midMode = MID_MODE_EID;
                } else if (strcmp("ext", optarg) == 0) {
                    state->midMode = MID_MODE_EXT;
                } else {
                    printf("Unsupported MID mode: %s\n", optarg);
                    help();
                    return EINVAL;
                }
                break;
            case 'M':
                if ((rc = parse_uint32("msg-queue-size", optarg, &state->msgQueueSize)) != EOK) {
                    return rc;
                }
                break;
            case 'm':
                if (strcmp("io", optarg) == 0) {
                    state->mode = CANDEV_MODE_IO;
                    state->numTxNodes = MCP2515_NUM_TX_BUFFERS;
                    state->numRxNodes = MCP2515_NUM_RX_BUFFERS;
                } else if (strcmp("raw", optarg) == 0) {
                    state->mode = CANDEV_MODE_RAW_FRAME;
                    state->numTxNodes = 1;
                    state->numRxNodes = 1;
                } else {
                    printf("Unsupported mode: %s\n", optarg);
                    help();
                    return EINVAL;
                }
                break;
            case 'U':
                state->userStr = optarg;
                break;
            case 'u':
                if ((rc = parse_uint32("bps", optarg, &state->unit)) != EOK) {
                    return rc;
                }
                break;
            case 'V':
                print_version();
                exit(0);
            case 'v':
                state->verbosity++;
                break;
            case 'W':
                if ((rc = parse_uint32("wait-queue-size", optarg, &state->waitQueueSize)) != EOK) {
                    return rc;
                }
                break;
            case ':':
                printf("Missing argument for option %s\n", argv[optind - 1]);
                help();
                return EINVAL;
            case '?':
                printf("Unknown option: %s\n", argv[optind - 1]);
                help();
                return EINVAL;
        }
    }

    // Ensure the minimum number of parameters are set
    if (state->spiPath == NULL) {
        printf("Missing -s/--spi\n");
        help();
        return EINVAL;
    } else if (state->clock == DRV_STATE_CLOCK_UNSET) {
        printf("Missing -c/--clock\n");
        help();
        return EINVAL;
    } else if (state->gpio == DRV_STATE_GPIO_UNSET) {
        printf("Missing -i/--irq\n");
        help();
        return EINVAL;
    }

    return EOK;
}
