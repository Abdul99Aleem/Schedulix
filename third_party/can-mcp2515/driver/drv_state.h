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

#ifndef CAN_MCP2515_DRIVER_DRV_STATE_H_
#define CAN_MCP2515_DRIVER_DRV_STATE_H_

#include <stdint.h>
#include <time.h>
#include <hw/libcan.h>

#include <mcp2515/state.h>
#include <mcp2515/util.h>

/**
 * @file
 *
 * State information required by the driver to function
 */

/**
 * Default value for the drv_state_t.clock member set by a call to
 * drv_state_init(). It can be used to tell if the user has set a value via
 * the command line.
 *
 * The value itself is invalid for the clock frequency.
 */
#define DRV_STATE_CLOCK_UNSET 0

/**
 * Default value for the drv_state_t.gpio member set by a call to
 * drv_state_init(). It can be used to tell if the user has set a value via
 * the command line.
 *
 * The value itself is invalid for the gpio.
 */
#define DRV_STATE_GPIO_UNSET -1

/**
 * The various modes the driver can operate in when determining whether
 * a MID is a SID or an EID.
 */
typedef enum {
    MID_MODE_SID = 0,   ///< The MID is a SID
    MID_MODE_EID,       ///< The MID is an EID.

    /**
     * The MID can be either a SID or an EID.
     * It depends on the state of bit 30 in the MID.
     * If the bit is set, then the MID is an EID.
     * If the bit is unset, then the MID is a SID.
     */
    MID_MODE_EXT        ///< The MID 
} mid_mode_t;

struct drv_state;

/**
 * State for each node exposed into the filesystem by the driver.
 */
struct can_node {
    /**
     * The per-node state required by libcan
     */
    CANDEV cdev;
    /*
     * The overall state for the driver that owns this node
     */
    struct drv_state* dstate;
    /*
     * The MID currently associated with the node
     */
    uint32_t mid;
    /*
     * The mfilter currently associated with the node
     */
    uint32_t mfilter;

    /**
     * How many frames have been dropped by this node.
     * Only relevant for RX nodes.
     */
    uint32_t dropCount;
    /**
     * The timestamp when the 1st frame was dropped in the last period.
     */
    uint32_t dropTs;

    /**
     * The index of the underlying TX or RX buffer associated with this node
     */
    uint8_t bufferIdx;

    /**
     * True (non-zero) if the node is actively being used to transmit a frame.
     * False (zero) otherwise.
     */
    uint8_t inUse;

    /**
     * The transmit priority for the node.
     * Only valid for TX nodes.
     */
    uint8_t txPriority;
};
typedef struct can_node can_node_t;

/**
 * State for the driver
 */
struct drv_state {
    // Configuration. Set during startup and constant after that
    /**
     * @name driverConfig
     *
     * Configuration for the driver. It is meant to be set at startup
     * and from them on kept constant for the lifetime of the process.
     */
    /**@{*/
    /**
     * The absolute path in the filesystem for the SPI node that represents
     * the MCP2515 this driver will control.
     *
     * The default value is NULL representing an unset value.
     */
    const char* spiPath;

    /**
     * The input clock frequency to the MCP2515. Must be in the range
     * [MCP2515_MIN_CLOCK_FREQUENCY, MCP2515_MAX_CLOCK_FREQUENCY]
     *
     * The default value is set to DRV_STATE_CLOCK_UNSET representing
     * an unset value.
     */
    uint32_t clock;

    /**
     * The GPIO used for the MCP2515's interrupt.
     *
     * The default value is set to DRV_STATE_GPIO_UNSET representing an unset
     * value.
     */
    int gpio;

    /**
     * The bitrate (Bits-Per-Second) of the CAN bus the MCP2515 is
     * connected to.
     *
     * The default value is 500,000 (500kbps).
     */
    uint32_t bps;

    /**
     * The (can) unit this driver will expose in the filesystem. For example,
     * if unit is equal to 2 then this driver will be exposed to the filesystem
     * as /dev/can2/
     *
     * The default value is 0.
     */
    uint32_t unit;

    /**
     * Whether the driver should operate in debug mode or not. In debug mode
     * all logging is sent to the console instead of SLOG2.
     *
     * The default value is 0 (false).
     */
    int debug;

    /**
     * The logging verbosity of the driver.
     *
     * The default value is SLOG2_NOTICE
     */
    int verbosity;

    /**
     * The number of TX nodes the driver will expose.
     * Only relevant for IO mode to limit the number of TX nodes exposed.
     *
     * The default value is MCP2515_NUM_TX_BUFFERS (3).
     */
    uint32_t numTxNodes;

    /**
     * The number of RX nodes the driver will expose.
     * Only relevant for IO mode to limit the number of RX nodes exposed.
     *
     * The default value is MCP2515_NUM_RX_BUFFERS (2).
     */
    uint32_t numRxNodes;

    /**
     * The size of the per-node msg queue. This is the maximum number of
     * CAN messages that an RX or TX node can have pending.
     *
     * The default value is 256
     */
    uint32_t msgQueueSize;

    /**
     * The size of the per-node wait queue. This is the maximum number of
     * clients that can be blocked on a node waiting for a frame to be
     * received.
     *
     * The default value is 8
     */
    uint32_t waitQueueSize;

    /**
     * The operating mode of the driver.
     *
     * A driver in IO mode has nodes that can filter on receiption for CAN
     * frames with specific MIDs. A driver in RAW mode receives everything
     * it sees from the CAN bus.
     *
     * The default value is IO mode.
     */
    CANDEV_MODE mode;

    /**
     * How the driver should interpret MIDs. Are they SIDs, EIDs, or a
     * combination of them. See @ref mid_mode_t for details.
     *
     * The default value is MID_MODE_SID.
     */
    mid_mode_t midMode;

    /**
     * Should the HW triple-sample bits during reception. See the MCP2515
     * datasheet for details.
     *
     * The default value is false (0).
     */
    int tripleSample;

    /**
     * An optional string detailing what UID/GIDs of the driver.
     * The driver will change its UID/GIDs to match those specified.
     * Nodes created in the filesystem will have the same UID and the first
     * GID from the list of GIDs specified.
     *
     * See set_ids_from_arg() in the QNX documentation for details on the
     * form of the string.
     *
     * The default value is NULL. The driver will run as whatever user
     * started it, usually root:root.
     */
    const char* userStr;
    /**@}*/

    /**
     * State associated with the underlying MCP2515 library.
     */
    mcp2515_state_t mcp2515;

    /**
     * Opaque data used by the platform support subsystem
     */
    void* platform;

    /**
     * Callback functions used by the libcan library. It needs to persist
     * for the lifetime of the driver so is kept here as a void pointer so
     * that it isn't destroyed until the driver needs to exit.
     */
    void* driverFuncs;

    /**
     * The IRQ associated with the GPIO used by the MCP2515 controlled by
     * the driver.
     */
    int irqId;

    /**
     * The nodes exposed by the driver in the filesystem. Each one represents
     * an underlying HW transmit (TX) or receive (RX) buffer.
     */
    can_node_t* nodes;

    /**
     * The current delta from the 'set' timestamp (the epoch) and the timestamp
     * generated at that moment. Used to adjust the current timestamp to be
     * referenced from the set timestamp/epoch.
     */
    uint32_t tsDelta;
};
typedef struct drv_state drv_state_t;

/**
 * Initialize the driver's state.
 *
 * All values will be set to defaults
 *
 * @param[in]   state   The state context for the driver
 */
void drv_state_init(drv_state_t* state);

/**
 * Deinitializes the driver's state.
 *
 * After this is called none of the members in state are valid.
 *
 * Calling this API on an uninitialized state is ok and results in a no-op.
 *
 * @param[in]   state   The state context for the driver
 */
void drv_state_deinit(drv_state_t* state);

#endif  // CAN_MCP2515_DRIVER_DRV_STATE_H_
