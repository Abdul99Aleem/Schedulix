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
#include <stdlib.h>
#include <time.h>
#include <hw/libcan.h>
#include <secpol/ids.h>

#include <mcp2515/bit_timing.h>
#include <mcp2515/command.h>
#include <mcp2515/connect.h>
#include <mcp2515/io.h>
#include <mcp2515/logging.h>
#include <mcp2515/state.h>
#include <mcp2515/util.h>

#include "command_line.h"
#include "core.h"
#include "debug.h"
#include "drv_state.h"
#include "rpi4.h"

#define MAX_RX_ERROR_COUNT 10

#define ONE_SEC_IN_US (1 * 1000 * 1000)

#define PULSE_IRQ_FIRED  _PULSE_CODE_MINAVAIL

#define MID_TYPE_MASK (1 << 30)
#define MID_TYPE_SID (0 << 30)
#define MID_TYPE_EID (1 << 30)

#define MID_SID_MASK (0x7FF << 18)
#define MID_EID_MASK (0x1FFFFFFF)

#define SID_TO_MID(s) ((s) << 18)
#define MID_TO_SID(s) ((s) >> 18)

static inline int mid_is_sid(uint32_t mid) {
    return (mid & MID_TYPE_MASK) == MID_TYPE_SID;
}
static inline int mid_is_eid(uint32_t mid) {
    return (mid & MID_TYPE_MASK) == MID_TYPE_EID;
}
static inline uint32_t mid_set_type(uint32_t mid, uint32_t type) {
    return (mid & ~MID_TYPE_MASK) | type;
}

static inline uint32_t get_node_id(can_node_t* node) {
    //return (((uintptr_t)node - (uintptr_t)node->dstate->nodes) / sizeof(*node));
    return node->cdev.dev_unit;
}
static inline int is_rx_node(can_node_t* node) { return get_node_id(node) < node->dstate->numRxNodes; }
static inline int is_tx_node(can_node_t* node) { return !is_rx_node(node); }

#define IS_TX_MAILBOX(dstate, node) ((dstate))

static const char* dcmd_to_str(int cmd) {
    switch (cmd) {
        case CAN_DEVCTL_WRITE_CANMSG_EXT:
            return "extended write devctl";
        case CAN_DEVCTL_READ_CANMSG_EXT:
            return "extended read devctl";
        case CAN_DEVCTL_TX_FRAME_RAW:
            return "raw write devctl";
        case CAN_DEVCTL_RX_FRAME_RAW_BLOCK:
            return "raw blocking read devctl";
        case CAN_DEVCTL_RX_FRAME_RAW_NOBLOCK:
            return "raw nonblocking read devctl";
        case CAN_DEVCTL_SET_PRIO:
            return "set priority devctl";
        case CAN_DEVCTL_GET_PRIO:
            return "get priority devctl";
        case CAN_DEVCTL_SET_MID:
            return "set mid devctl";
        case CAN_DEVCTL_SET_MFILTER:
            return "set mfilter devctl";
        case CAN_DEVCTL_SET_TIMESTAMP:
            return "set timestamp devctl";
        case CAN_DEVCTL_GET_MID:
            return "get mid devctl";
        case CAN_DEVCTL_GET_MFILTER:
            return "get mfilter devctl";
        case CAN_DEVCTL_GET_TIMESTAMP:
            return "get timestamp devctl";
        case CAN_DEVCTL_DEBUG_INFO2:
            return "debug info 2 devctl";
        case CAN_DEVCTL_ERROR:
            return "error devctl";
        case CAN_DEVCTL_GET_STATS:
            return "get stats devctl";
        case CAN_DEVCTL_DEBUG_INFO:
            return "debug info devctl";
        default:
            return "other devctl";
    }
}


// libcan wants us to transmit the next frame from the specified device/node
static void libcan_transmit_cb(CANDEV* cdev) {
    can_node_t* node;
    canmsg_t* msg;
    int rc;

    if (cdev == NULL) {
        LOGW("libcan_transmit_cb: NULL cdev, ignoring");
        return;
    }
    node = CONTAINER_OF(cdev, can_node_t, cdev);

    // If the node is in use there is nothing I can do at this time.
    // When the interrupt fires saying it is done I will send the frame then.
    if (node->inUse) {
        LOGD2("libcan_transmit_cb(node=%d, txBuf=%u): Tx Buffer is in use. Pending transmission.",
              get_node_id(node), node->bufferIdx);
        return;
    }
    msg = canmsg_dequeue_element(cdev->msg_queue);
    if (msg == NULL) {
        LOGD2("libcan_transmit_cb(node=%d, txBuf=%u): No message on queue. Ignoring",
              get_node_id(node), node->bufferIdx);
        return;
    }

    // Assign an ID to the msg if required
    if (msg->cmsg.mid == CAN_MSG_MID_UNKNOWN) {
        msg->cmsg.mid = node->mid;
        if (node->dstate->midMode == MID_MODE_EID ||
           (node->dstate->midMode == MID_MODE_EXT && mid_is_eid(msg->cmsg.mid)))
        {
            msg->cmsg.ext.is_extended_mid = 1;
        } else {
            msg->cmsg.ext.is_extended_mid = 0;
        }
    }
    if (msg->cmsg.ext.is_extended_mid) {
        msg->cmsg.mid &= MID_EID_MASK;
    } else {
        msg->cmsg.mid &= MID_SID_MASK;
    }

    // Transmit and return the element to the free queue
    if ((rc = mcp2515_cmd_tx_can_message(&node->dstate->mcp2515, node->bufferIdx, &msg->cmsg)) != EOK) {
        LOGE("libcan_transmit_cb(node=%d, txBuf=%u): Failed to Tx message(mid=0x%08x len=%d)",
             get_node_id(node), node->bufferIdx, msg->cmsg.mid, msg->cmsg.len);
    }
    node->inUse = 1;
    canmsg_queue_element(cdev->free_queue, msg);
}

static int set_mid(can_node_t* node, uint32_t mid) {
    int rc;
    char errStr[ERR_STR_SIZE];

    assert(node != NULL);
    // Sanitize the mid and set the type mask appropriately.
    switch (node->dstate->midMode) {
        case MID_MODE_SID:
            mid = (mid & MID_SID_MASK) | MID_TYPE_SID;
            break;
        case MID_MODE_EID:
            mid = (mid & MID_EID_MASK) | MID_TYPE_EID;
            break;
        case MID_MODE_EXT:
            if (mid_is_eid(mid)) {
                mid = (mid & MID_EID_MASK) | MID_TYPE_EID;
            } else {
                mid = (mid & MID_SID_MASK) | MID_TYPE_SID;
            }
            break;
    }

    // If this is an rxNode, then the MID gets set as the filter in HW.
    // However that can only be done when the HW is in configuration mode.
    if (is_rx_node(node)) {
        LOGN("Switching mcp2515 to config mode to program MID.");
        rc = mcp2515_cmd_set_op_mode(&node->dstate->mcp2515, MCP2515_CANSTAT_OPMOD_CONFIG);
        if (rc) {
            LOGE("Error changing to config mode. Error=%s", sstrerror(rc, errStr, sizeof(errStr)));
            return rc;
        }

        rc = mcp2515_cmd_set_rx_mid_filter(&node->dstate->mcp2515, node->bufferIdx, mid, mid & MID_TYPE_EID);
        if (rc) {
            LOGE("Error setting mid filter. Error=%s", sstrerror(rc, errStr, sizeof(errStr)));
            return rc;
        }

        rc = mcp2515_cmd_set_op_mode(&node->dstate->mcp2515, MCP2515_CANSTAT_OPMOD_NORMAL);
        if (rc) {
            LOGE("Error changing to normal mode. Error=%s", sstrerror(rc, errStr, sizeof(errStr)));
            return rc;
        }
    }

    node->mid = mid;
    return EOK;
}

static int set_mfilter(can_node_t* node, uint32_t mfilter) {
    int rc;
    char errStr[ERR_STR_SIZE];

    assert(node != NULL);
    assert(is_rx_node(node));

    // Setting filter mask can only be done when the HW is in configuration mode.
    LOGN("Switching mcp2515 to config mode to program filter mask.");
    rc = mcp2515_cmd_set_op_mode(&node->dstate->mcp2515, MCP2515_CANSTAT_OPMOD_CONFIG);
    if (rc) {
        LOGE("Error changing to config mode. Error=%s", sstrerror(rc, errStr, sizeof(errStr)));
        return rc;
    }

    rc = mcp2515_cmd_set_rx_mid_mask(&node->dstate->mcp2515, node->bufferIdx, mfilter, node->mid & MID_TYPE_EID);
    if (rc) {
        LOGE("Error setting mid mask. Error=%s", sstrerror(rc, errStr, sizeof(errStr)));
        return rc;
    }

    rc = mcp2515_cmd_set_op_mode(&node->dstate->mcp2515, MCP2515_CANSTAT_OPMOD_NORMAL);
    if (rc) {
        LOGE("Error changing to normal mode. Error=%s", sstrerror(rc, errStr, sizeof(errStr)));
        return rc;
    }

    node->mfilter = mfilter;
    return EOK;
}

// libcan wants us to process a received devctl. This is our chance
// to decide if we support it or not. The frame tx/rx devctls will
// be fulfilled by the library. All we need to do is make sure they
// are valid.
//
// Return == 0/EOK -> libcan's devctl returns EOK with nbytes=0
// Return > 0 -> libcan's devctl returns EOK with nbytes=Returned value
// Return < 0 -> If errno == ENOTSUP -> libcan handles devctl if it can
//               If errno != ENOTSUP -> libcan's devctl returns errno
static int libcan_devctl_cb(CANDEV* cdev, io_devctl_t* msg) {
    can_node_t* node;
    CAN_DCMD_DATA* data = _IO_INPUT_PAYLOAD(msg);
    uint32_t now;
    int rc = EOK;
    char errStr[ERR_STR_SIZE];

    if (cdev == NULL || msg == NULL) {
        LOGE("libcan_devctl_cb: NULL cdev/msg, dropping request.");
        rc = -EINVAL;
        goto exit;
    }

    node = CONTAINER_OF(cdev, can_node_t, cdev);
    switch (msg->i.dcmd) {
        case CAN_DEVCTL_SET_TIMESTAMP:
            // MCP2515 doesn't support timestamps, so we try to do it in SW
            now = get_sw_timestamp();
            node->dstate->tsDelta = data->timestamp - now;
            LOGN("libcan_devctl_cb: Set timestamp(unit=%d, node=%d, ts=%u) gives delta of %u", cdev->can_unit,
                 get_node_id(node), data->timestamp, node->dstate->tsDelta);
            break;

        case CAN_DEVCTL_GET_TIMESTAMP:
            // MCP2515 doesn't support timestamps, so we try to do it in SW
            now = get_sw_timestamp();
            data->timestamp = now + node->dstate->tsDelta;
            LOGI("libcan_devctl_cb: Get timestamp(unit=%d, node=%d) = %u", cdev->can_unit, get_node_id(node),
                 data->timestamp);
            rc = (int)sizeof(data->timestamp);
            break;

        case CAN_DEVCTL_SET_MID:
            if (cdev->mode == CANDEV_MODE_RAW_FRAME) {
                LOGE("libcan_devctl_cb: Set MID(unit=%d, node=%d, mid=0x%08x) failed. In RAW mode", cdev->can_unit,
                     get_node_id(node), data->mid);
                rc = -EINVAL;
                break;
            }
            rc = set_mid(node, data->mid);
            if (rc == EOK) {
                LOGN("libcan_devctl_cb: Set MID(unit=%d, node=%d, mid=0x%08x)", cdev->can_unit, get_node_id(node), data->mid);
            } else {
                LOGE("libcan_devctl_cb: Set MID(unit=%d, node=%d, mid=0x%08x) FAILED.",
                     cdev->can_unit, get_node_id(node), data->mid);
                rc = -rc;
            }
            break;

        case CAN_DEVCTL_GET_MID:
            data->mid = node->mid;
            if (node->dstate->midMode != MID_MODE_EXT) {
                data->mid &= ~MID_TYPE_MASK;
            }
            LOGI("libcan_devctl_cb: Get MID(unit=%d, node=%d) = 0x%08x", cdev->can_unit, get_node_id(node), data->mid);
            rc = (int)sizeof(data->mid);
            break;

        case CAN_DEVCTL_SET_MFILTER:
            if (is_tx_node(node)) {
                LOGE("libcan_devctl_cb: Set MFILTER(unit=%d, node=%d, mfilter=0x%08x) failed. Is TX node",
                     cdev->can_unit, get_node_id(node), data->mfilter);
                rc = -EINVAL;
                break;
            } else if (cdev->mode == CANDEV_MODE_RAW_FRAME) {
                LOGE("libcan_devctl_cb: Set MFILTER(unit=%d, node=%d, mfilter=0x%08x) failed. In RAW mode", cdev->can_unit,
                     get_node_id(node), data->mfilter);
                rc = -EINVAL;
                break;
            }

            rc = set_mfilter(node, data->mfilter);
            if (rc == EOK) {
                LOGN("libcan_devctl_cb: Set MFILTER(unit=%d, node=%d, mfilter=0x%08x)", cdev->can_unit, get_node_id(node),
                     data->mfilter);
            } else {
                LOGE("libcan_devctl_cb: Set MFILTER(unit=%d, node=%d, mfilter=0x%08x) FAILED",
                     cdev->can_unit, get_node_id(node), data->mfilter);
                rc = -rc;
            }
            break;

        case CAN_DEVCTL_GET_MFILTER:
            if (is_tx_node(node)) {
                LOGE("libcan_devctl_cb: Get MFILTER(unit=%d, node=%d) failed. Is TX node", cdev->can_unit,
                     get_node_id(node));
                rc = -EINVAL;
                break;
            } else if (cdev->mode == CANDEV_MODE_RAW_FRAME) {
                LOGE("libcan_devctl_cb: Get MFILTER(unit=%d, node=%d) failed. In RAW mode", cdev->can_unit,
                     get_node_id(node));
                rc = -EINVAL;
                break;
            }

            data->mfilter = node->mfilter;
            LOGI("libcan_devctl_cb: Get MFILTER(unit=%d, node=%d) = 0x%08x", cdev->can_unit, get_node_id(node),
                 data->mfilter);
            rc = (int)sizeof(data->mfilter);
            break;

        case CAN_DEVCTL_SET_PRIO:
            if (is_rx_node(node)) {
                LOGE("libcan_devctl_cb: Set PRIO(unit=%d, node=%d) failed. Is RX node", cdev->can_unit,
                     get_node_id(node));
                rc = -EINVAL;
                break;
            } else if (data->prio > MCP2515_CANMCF_TPL_MAXVAL) {
                LOGE("libcan_devctl_cb: Set PRIO(unit=%d, node=%d) failed. Maximum priority is %d but wanted %u.",
                     cdev->can_unit, get_node_id(node), MCP2515_CANMCF_TPL_MAXVAL, data->prio);
                rc = -EINVAL;
                break;
            }
            rc = MCP2515_IO_SET_BITFIELD(&node->dstate->mcp2515, MCP2515_TXBnCTRL_REG_ADDR(node->bufferIdx),
                                         TXBnCTRL_TXP, data->prio);
            if (rc == EOK) {
                node->txPriority = data->prio;
                LOGN("libcan_devctl_cb: Set PRIO(unit=%d, node=%d, prio=%u)", cdev->can_unit, get_node_id(node),
                     node->txPriority);
            } else {
                LOGE("libcan_devctl_cb: Set PRIO(unit=%d, node=%d, prio=%u) FAILED. Error=%s",
                     cdev->can_unit, get_node_id(node), data->prio, sstrerror(rc, errStr, sizeof(errStr)));
                rc = -rc;
            }
            break;

        case CAN_DEVCTL_GET_PRIO:
            if (is_rx_node(node)) {
                LOGE("libcan_devctl_cb: Get PRIO(unit=%d, node=%d) failed. Is RX node", cdev->can_unit,
                     get_node_id(node));
                rc = -EINVAL;
                break;
            }
            data->prio = node->txPriority;
            LOGI("libcan_devctl_cb: Get PRIO(unit=%d, node=%d) = %u", cdev->can_unit, get_node_id(node),
                 data->prio);
            rc = (int)sizeof(node->txPriority);
            break;

        // Check here as the one in libcan doesn't generate an error
        // or return a reasonable errno (it returns EPERM for some reason)
        case CAN_DEVCTL_READ_CANMSG_EXT:
        case CAN_DEVCTL_RX_FRAME_RAW_BLOCK:
        case CAN_DEVCTL_RX_FRAME_RAW_NOBLOCK:
            if (is_tx_node(node)) {
                LOGE("libcan_devctl_cb: Read devctl(unit=%d, node=%d) failed. Is TX node", cdev->can_unit,
                     get_node_id(node));
                rc = -EINVAL;
                break;
            }
            // Return ENOTSUP so that libcan handles it
            rc = -ENOTSUP;
            break;

        // Check here as the one in libcan doesn't generate an error
        // or return a reasonable errno (it returns EPERM for some reason)
        case CAN_DEVCTL_WRITE_CANMSG_EXT:
        case CAN_DEVCTL_TX_FRAME_RAW:
            if (is_rx_node(node)) {
                LOGE("libcan_devctl_cb: Write devctl(unit=%d, node=%d) failed. Is RX node", cdev->can_unit,
                     get_node_id(node));
                rc = -EINVAL;
                break;
            } else if (data->canmsg.len > CAN_MSG_DATA_MAX_CAN) {
                LOGE("libcan_devctl_cb: Write devctl(unit=%d, node=%d) failed. Message too large: (%d)",
                      cdev->can_unit, get_node_id(node), data->canmsg.len);
                rc = -EINVAL;
                break;
            }
            // Return ENOTSUP so that libcan handles it
            rc = -ENOTSUP;
            break;

        case CAN_DEVCTL_DEBUG_INFO:
        case CAN_DEVCTL_DEBUG_INFO2:
            dump_mcp2515_status_registers(node->dstate);
            dump_mcp2515_all_rx_filter_registers(node->dstate);
            break;

        default:
            // remaining devctls are not supported
            LOGW("libcan_devctl_cb: Unsupported devctl(unit=%d, node=%d, dmcd=%u(%s))", cdev->can_unit, get_node_id(node),
                 msg->i.dcmd, dcmd_to_str(msg->i.dcmd));
            rc = -ENOTSUP;
            break;
    }

exit:
    if (rc < 0) {
        errno = -rc;
        rc = -1;
    }
    return rc;
}

static void handleErrorFlag(drv_state_t* dstate) {
    uint8_t clearBits = 0;
    int errFlg;
    char buffer[256];
    char* ptr;
    int rc;

    errFlg = mcp2515_io_read_byte(&dstate->mcp2515, MCP2515_EFLG_REG_ADDR);
    if (errFlg < 0) {
        LOGE("Unable to read EFLG. Trying to ignore... Error=%s",
             sstrerror(-errFlg, buffer, sizeof(buffer)));
        errFlg = 0;
        // Clear everything that can be cleared just to try and keep things going
        clearBits = MCP2515_EFLG_RX1OVR_MASK | MCP2515_EFLG_RX0OVR_MASK;
    }

    ptr = buffer;
    ptr += snprintf(ptr, sizeof(buffer)-(ptr-buffer), "Current MCP2515 Error flags:\n");
    if (errFlg & MCP2515_EFLG_RX1OVR_MASK) {
        ptr += snprintf(ptr, sizeof(buffer)-(ptr-buffer), "RXB1 Overflow\n");
        clearBits |= MCP2515_EFLG_RX1OVR_MASK;
    }
    if (errFlg & MCP2515_EFLG_RX0OVR_MASK) {
        ptr += snprintf(ptr, sizeof(buffer)-(ptr-buffer), "RXB0 Overflow\n");
        clearBits |= MCP2515_EFLG_RX0OVR_MASK;
    }
    if (errFlg & MCP2515_EFLG_TXBO_MASK) {
        ptr += snprintf(ptr, sizeof(buffer)-(ptr-buffer), "TEC >= 255 => Bus Off\n");
    }
    if (errFlg & MCP2515_EFLG_TXEP_MASK) {
        ptr += snprintf(ptr, sizeof(buffer)-(ptr-buffer), "128 <= TEC < 255 - TX Error passive\n");
    }
    if (errFlg & MCP2515_EFLG_RXEP_MASK) {
        ptr += snprintf(ptr, sizeof(buffer)-(ptr-buffer), "128 <= REC < 255 - RX Error passive\n");
    }
    if (errFlg & MCP2515_EFLG_TXWAR_MASK) {
        ptr += snprintf(ptr, sizeof(buffer)-(ptr-buffer), "96 <= TEC < 128 - TX Error warning\n");
    }
    if (errFlg & MCP2515_EFLG_RXWAR_MASK) {
        ptr += snprintf(ptr, sizeof(buffer)-(ptr-buffer), "96 <= REC < 128 - RX Error warning\n");
    }
    if (errFlg & MCP2515_EFLG_EWARN_MASK) {
        ptr += snprintf(ptr, sizeof(buffer)-(ptr-buffer), "TXWAR and/or RXWAR are set\n");
    }
    // Remove the trailing newline
    if (*(ptr-1) == '\n') *(ptr-1) = '\0';
    LOGE("%s", buffer);

    // Clear any flags that need to be cleared
    if (clearBits) {
        rc = mcp2515_io_bit_modify(&dstate->mcp2515, MCP2515_EFLG_REG_ADDR, clearBits, 0);
        if (rc) {
            LOGE("Unable to clear bits 0x%02x in EFLG. Interrupt will likely fire again. Error=%s",
                 clearBits, sstrerror(rc, buffer, sizeof(buffer)));
        }
    }
}

static can_node_t* receiveFrame(drv_state_t* dstate, unsigned rxIdx) {
    int rc;
    canmsg_t* cmsg;
    can_node_t* node;
    char errStr[ERR_STR_SIZE];
    uint32_t now;

    now = get_sw_timestamp();

    assert(rxIdx < dstate->numRxNodes);
    node = &dstate->nodes[rxIdx];

    LOGD2("Receiving frame from RX mailbox %d", rxIdx);

    // Deque a message buffer to actually receive the message into.
    cmsg = canmsg_dequeue_element(node->cdev.free_queue);
    if (!cmsg) {
        // Overflow, no free msgs available. Dropping the oldest one.
        cmsg = canmsg_dequeue_element(node->cdev.msg_queue);
        assert(cmsg != NULL);

        if ((now - node->dropTs) > ONE_SEC_IN_US) {
            if (node->dropCount > 1) {
                LOGW("OVERFLOW in RX mailbox %d caused %u additional drop(s) in the last %uus",
                        rxIdx, node->dropCount - 1, now - node->dropTs);
            }
            node->dropCount = 0;
        }
        node->dropCount++;
        if (node->dropCount == 1) {
            LOGW("OVERFLOW in RX mailbox %d. Dropping oldest message with MID 0x%08x",
                    rxIdx, cmsg->cmsg.mid);
            node->dropTs = now;
        }
    }

    // Receive the message
    rc = mcp2515_cmd_rx_can_message(&dstate->mcp2515, rxIdx, &cmsg->cmsg);
    if (!rc) {
        // Set the timestamp in the message
        cmsg->cmsg.ext.timestamp = now + dstate->tsDelta;

        // Queue the message into the node
        canmsg_queue_element(node->cdev.msg_queue, cmsg);
    } else {
        LOGE("Error receiving frame from RX mailbox %d. Error=%s",
             sstrerror(rc, errStr, sizeof(errStr)));
        canmsg_queue_element(node->cdev.free_queue, cmsg);
        // Still need to return the node so that interrupt handling continues.
    }

    return node;
}

static can_node_t* transmitFrameDone(drv_state_t* dstate, unsigned txIdx) {
    can_node_t* node;
    int rc;
    char errStr[ERR_STR_SIZE];

    assert(txIdx < dstate->numTxNodes);
    node = &dstate->nodes[dstate->numRxNodes + txIdx];

    if (!node->inUse) {
        LOGW("TX mailbox %d is done transmission but was not being used!", txIdx);
    }
    node->inUse = 0;
    LOGD2("Transmit from tx mailbox %d is done", txIdx);

    // Clear the interrupt
    rc = mcp2515_io_bit_modify(&dstate->mcp2515, MCP2515_CANINTF_REG_ADDR, MCP2515_CANINTF_TXnIF_MASK(txIdx) , 0);
    if (rc) {
        LOGE("Unable to clear interrupt for TX mailbox %d. Interrupt will likely fire again. Error=%s",
             txIdx, sstrerror(rc, errStr, sizeof(errStr)));
    }

    return node;
}

static CANDEV* libcan_event_handler_cb(void* const hdl, const int code) {
    drv_state_t* dstate = (drv_state_t*)hdl;
    char errStr[ERR_STR_SIZE];
    int intStatus;
    int rc;
    int clearBits = 0;
    can_node_t* node = NULL;

    if (code != PULSE_IRQ_FIRED) {
        LOGW("event_handler: Ignoring unknown pulse %d\n", code);
        // Don't jump to exit as I shouldn't have been called and
        // thus nothing to clear/unmask.
        return NULL;
    }

    // Since multiple GPIOs are grouped together under a given IRQ
    // the IRQ could fire in response to some other GPIO
    if (!platform_rpi4_gpio_irq_active(dstate)) {
        LOGD2("event_handler: IRQ fired but my GPIO is not active. Ignoring.");
        InterruptUnmask(0, dstate->irqId);
        return NULL;
    }

    // Get the initial status
    intStatus = mcp2515_io_read_byte(&dstate->mcp2515, MCP2515_CANINTF_REG_ADDR);
    if (intStatus < 0) {
        LOGE("Unable to read interrupt status. Tx/Rx is likely broken. Error=%s",
             sstrerror(-intStatus, errStr, sizeof(errStr)));
        goto exit;
    }
    LOGD2("Interrupt status=0x%02x", intStatus);

    // Figure out which interrupt to service. Favour receiption over
    // anything else as I want to avoid overflows.
    if (intStatus & MCP2515_CANINTF_RX0IF_MASK) {
        node = receiveFrame(dstate, 0);
    } else if (intStatus & MCP2515_CANINTF_RX1IF_MASK) {
        node = receiveFrame(dstate, 1);
    } else if (intStatus & MCP2515_CANINTF_TX0IF_MASK) {
        node = transmitFrameDone(dstate, 0);
    } else if (intStatus & MCP2515_CANINTF_TX1IF_MASK) {
        node = transmitFrameDone(dstate, 1);
    } else if (intStatus & MCP2515_CANINTF_TX2IF_MASK) {
        node = transmitFrameDone(dstate, 2);
    }
    if (node != NULL) return &node->cdev;

    // The remaining interrupts don't actually return anything to libcan.
    // This means I need to handle them all at once as libcan will NOT call
    // back into this routine once I return NULL.
    if (intStatus & MCP2515_CANINTF_ERRIF_MASK) {
        handleErrorFlag(dstate);
        clearBits |= MCP2515_CANINTF_ERRIF_MASK;
    }
    if (intStatus & MCP2515_CANINTF_WAKIF_MASK) {
        // Wakeup, shouldn't happen as I don't use this. Just note and clear it.
        LOGN("MCP215 woken up due to bus activity.\n");
        clearBits |= MCP2515_CANINTF_WAKIF_MASK;
    }
    if (intStatus & MCP2515_CANINTF_MERRF_MASK) {
        // Message error during rx/tx. Just note it.
        LOGN("MCP215 detected error on bus during tx/rx.\n");
        clearBits |= MCP2515_CANINTF_MERRF_MASK;
    }
    if (clearBits) {
        rc = mcp2515_io_bit_modify(&dstate->mcp2515, MCP2515_CANINTF_REG_ADDR, clearBits, 0);
        if (rc) {
            LOGE("Unable to clear bits 0x%02x in CANINTF. Interrupt will likely fire again. Error=%s",
                 clearBits, sstrerror(rc, errStr, sizeof(errStr)));
        }
    }

exit:
    // No more MCP2515 interrupts to process. Safe to clear and unmask the interrupt.
    platform_rpi4_gpio_irq_clear(dstate);
    InterruptUnmask(0, dstate->irqId);
    return NULL;
}

static int connect(drv_state_t* dstate) {
    int rc;
    uint8_t sinks = MCP2515_LOG_SINK_SLOG2;
    int level = SLOG2_NOTICE;
    char errStr[ERR_STR_SIZE];

    // Make sure config is valid
    if (dstate->spiPath == NULL) {
        printf("Missing spi path\n");
        return EINVAL;
    } else if (dstate->clock == DRV_STATE_CLOCK_UNSET) {
        printf("Missing clock frequency\n");
        return EINVAL;
    } else if (dstate->gpio == DRV_STATE_GPIO_UNSET) {
        printf("Missing GPIO\n");
        return EINVAL;
    }

    // Set up logging, defaults to slog2 unless we are in debug mode
    if (dstate->debug) sinks = MCP2515_LOG_SINK_CONSOLE;
    level += dstate->verbosity;
    level = CLAMP(level, SLOG2_SHUTDOWN, SLOG2_DEBUG2);
    rc = mcp2515_log_init("can_mcp2515_drv", sinks, level);
    if (rc != EOK) {
        printf("Error setting up logging: %s\n", sstrerror(rc, errStr, sizeof(errStr)));
        return rc;
    }

    rc = mcp2515_connect(&dstate->mcp2515, dstate->spiPath);
    if (rc != EOK) {
        printf("can_mcp2515_drv is unable to connect. Error=%s\n", sstrerror(rc, errStr, sizeof(errStr)));
        return rc;
    }

    // Initialize platform support
    rc = platform_rpi4_initialize(dstate);
    if (rc != EOK) {
        printf("can_mcp2515_drv is unable to initialize platform support. Error=%s\n", sstrerror(rc, errStr, sizeof(errStr)));
        return rc;
    }

    return EOK;
}

static void disconnect(drv_state_t* dstate) {

    platform_rpi4_deinitialize(dstate);
    mcp2515_disconnect(&dstate->mcp2515);
}

static int configure_libcan(drv_state_t* dstate) {
    CANDEV_INIT cdevInit;
    uint32_t mid;

    if ((dstate->numRxNodes + dstate->numTxNodes) == 0) {
        printf("There must be at least one tx or rx node\n");
        help();
        return EINVAL;
    }

    // Initialize the resmgr.
    can_drvr_funcs_t* driverFuncs = calloc(1, sizeof(can_drvr_funcs_t));
    if (!driverFuncs) {
        printf("Failed to allocate driver functions struct\n");
        return ENOMEM;
    }
    driverFuncs->transmit = &libcan_transmit_cb;
    driverFuncs->devctl = &libcan_devctl_cb;
    driverFuncs->event_handler = &libcan_event_handler_cb;
    can_resmgr_init(driverFuncs, dstate->verbosity);
    dstate->driverFuncs = driverFuncs;

    // Confiugre the various nodes for the driver
    dstate->nodes = calloc(dstate->numRxNodes + dstate->numTxNodes, sizeof(can_node_t));
    memset(&cdevInit, 0, sizeof(cdevInit));
    cdevInit.can_unit = (int)dstate->unit;
    cdevInit.mode = dstate->mode;
    cdevInit.msgq_size = dstate->msgQueueSize;
    cdevInit.waitq_size = dstate->waitQueueSize;
    cdevInit.mode = dstate->mode;
    for (uint32_t i = 0; i < dstate->numRxNodes + dstate->numTxNodes; ++i) {
        cdevInit.dev_unit = (int)i;
        if (i < dstate->numRxNodes) {
            cdevInit.devtype = CANDEV_TYPE_RX;
            dstate->nodes[i].bufferIdx = (uint8_t)i;
        } else {
            cdevInit.devtype = CANDEV_TYPE_TX;
            dstate->nodes[i].bufferIdx = (uint8_t)(i - dstate->numRxNodes);
            dstate->nodes[i].txPriority = MCP2515_CANMCF_TPL_MAXVAL - (i - dstate->numRxNodes);
        }
        if (can_resmgr_init_device(&dstate->nodes[i].cdev, &cdevInit)) {
            printf("Failed to initialize node %u\n", i);
            return EIO;
        }

        dstate->nodes[i].dstate = dstate;
        dstate->nodes[i].cdev.cflags |= CANDEV_CFLAG_CAN;

        // Default MID is equal to its index.
        mid = i;
        if (dstate->midMode == MID_MODE_EID) {
            mid |= MID_TYPE_EID;
        } else {
            mid = SID_TO_MID(mid);
            mid |= MID_TYPE_SID;
        }
        dstate->nodes[i].mid = mid;
        // Default filter is 0 (allow everything) in RAW mode and 0xFFFFFFFF (exact) for IO mode
        dstate->nodes[i].mfilter = (dstate->mode == CANDEV_MODE_RAW_FRAME) ? 0 : UINT32_MAX;
    }

    return EOK;
}

static int configure_mcp2515(drv_state_t* dstate) {
    int rc;
    char errStr[ERR_STR_SIZE];
    can_node_t* node;

    // Reset mcp2515 into a known good state
    rc = mcp2515_io_reset(&dstate->mcp2515);
    if (rc != EOK) {
        printf("Failed to reset MCP2515. Error=%s", sstrerror(rc, errStr, sizeof(errStr)));
        return rc;
    }
    // I don't see anything explicit in the datasheet for how long a reset takes.
    // However it does note that after power-on or wake-from-sleep the MCP2515
    // holds itself in reset for 128 clock cycles, and that no SPI communication
    // should occur during this. So treat that as the reset time as well and wait.
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = (long)(((SEC_TO_NS(1) * 128) + dstate->clock - 1) / dstate->clock);
    nanosleep(&ts, NULL);

    can_bit_timings_t bt;
    rc = can_calculate_bit_timings(dstate->bps, dstate->clock, &MCP2515_BTConsts, &bt);
    if (rc != EOK) {
        printf("Unable to calculate bit timings\n");
        return rc;
    }
    LOGI("Calculated bit timings: prop=%u, phase1=%u, phase2=%u, sjw=%u, brp=%u",
         bt.prop, bt.phase1, bt.phase2, bt.sjw, bt.brp);
    rc = mcp2515_cmd_set_bit_timings(&dstate->mcp2515, &bt);
    if (rc != EOK) {
        printf("Unable to program bit timings\n");
        return rc;
    }

    // Enable desired interrupts
    uint8_t byte = 0;
    if (dstate->numTxNodes >= 3) byte |= MCP2515_CANINTE_TX2IE_MASK;
    if (dstate->numTxNodes >= 2) byte |= MCP2515_CANINTE_TX1IE_MASK;
    if (dstate->numTxNodes >= 1) byte |= MCP2515_CANINTE_TX0IE_MASK;
    if (dstate->numRxNodes >= 2) byte |= MCP2515_CANINTE_RX1IE_MASK;
    if (dstate->numRxNodes >= 1) byte |= MCP2515_CANINTE_RX0IE_MASK;
    rc = mcp2515_io_write_byte(&dstate->mcp2515, MCP2515_CANINTE_REG_ADDR, byte);
    if (rc != EOK) {
        printf("Unable to enable interrupts\n");
        return rc;
    }

    // Set priorities on Tx mailboxes
    for (uint32_t i = 0; i < dstate->numTxNodes; ++i) {
        node = &dstate->nodes[dstate->numRxNodes+i];
        rc = MCP2515_IO_SET_BITFIELD(&dstate->mcp2515, MCP2515_TXBnCTRL_REG_ADDR(i), TXBnCTRL_TXP, node->txPriority);
        if (rc != EOK) {
            printf("Unable to set Tx%d mailbox priority\n", i);
            return rc;
        }
    }

    // Disable Rx rollover
    rc = MCP2515_IO_CLEAR_BIT(&dstate->mcp2515, MCP2515_RXB0CTRL_REG_ADDR, RXB0CTRL_BUKT);
    if (rc != EOK) {
        printf("Unable to disable rx rollover\n");
        return rc;
    }

    // Set default reception properties
    for (uint32_t i = 0; i < dstate->numRxNodes; ++i) {
        node = &dstate->nodes[i];
        rc = mcp2515_cmd_set_rx_mid_filter(&dstate->mcp2515, i, node->mid, node->mid & MID_TYPE_EID ? 1 : 0);
        if (rc != EOK) {
            printf("Unable to set rx%u mid filter\n", i);
            return rc;
        }
        rc = mcp2515_cmd_set_rx_mid_mask(&dstate->mcp2515, i, node->mfilter, node->mid & MID_TYPE_EID ? 1 : 0);
        if (rc != EOK) {
            printf("Unable to set rx%u mid mask\n", i);
            return rc;
        }

        if (dstate->mode == CANDEV_MODE_RAW_FRAME) {
            // Force the reception mode to accept anything and everything
            rc = mcp2515_cmd_set_rx_mode(&dstate->mcp2515, i, MCP2515_RXBnCTRL_RXM_NO_FILTER);
            if (rc != EOK) {
                printf("Unable to set rx%u mode\n", i);
                return rc;
            }
        }
    }

    // Enable the mcp2515
    rc = mcp2515_cmd_set_op_mode(&dstate->mcp2515, MCP2515_CANSTAT_OPMOD_NORMAL);
    if (rc != EOK) {
        printf("Unable to enable mcp2515\n");
        return rc;
    }

    return EOK;
}

static int go(drv_state_t* dstate) {
    int rc;
    uint32_t ui32;
    char errStr[ERR_STR_SIZE];

    // Create all the device nodes.
    for (ui32 = 0; ui32 < dstate->numRxNodes + dstate->numTxNodes; ++ui32) {
        can_resmgr_create_device(&dstate->nodes[ui32].cdev);
    }

    // Attach my event handler to the IRQ
    rc = can_resmgr_attach_intr(dstate, platform_rpi4_gpio_irq_get(dstate), PULSE_IRQ_FIRED);
    if (rc == -1) {
        printf("Unable to attach IRQ.\n");
        return EIO;
    }
    dstate->irqId = rc;

    // Drop privileges
    if (secpol_transition_type(NULL, NULL, 0)) {
        LOGW("Unable to transition secpol. Continuing... Error=%s", sstrerror(errno, errStr, sizeof(errStr)));
    }

    // Switch to the desired uid/gid
    if (dstate->userStr) {
        if (set_ids_from_arg(dstate->userStr) == -1) {
            rc = errno;
            printf("Unable to drop root. Error=%s\n", sstrerror(rc, errStr, sizeof(errStr)));
            return rc;
        }
        uid_t const uid = geteuid();
        gid_t const gid = getegid();
        LOGN("Running as user='%s' (uid=%u gid=%u)", dstate->userStr, uid, gid);

        // I can't change the attributes of the resmgr's root dir as that isn't
        // exposed by libcan. So it will still be owned by root:root with perms 0555.
        // That should be okay.
        // I can, however, update all the tx/rx nodes to have the correct ownership.
        for (ui32 = 0; ui32 < dstate->numRxNodes + dstate->numTxNodes; ++ui32) {
            dstate->nodes[ui32].cdev.attr.uid = uid;
            dstate->nodes[ui32].cdev.attr.gid = gid;
        }
    }

    // By default, libcan will set rx nodes to have perms 0440 and tx nodes to 0220.
    // This does not work as devctls will be unable to modify rx nodes or read from
    // tx nodes. canctl, for instance, appears to open as O_RDWR which will fail
    // if canctl isn't running as root.
    // To work around this, manually force all the nodes to be 0660
    for (ui32 = 0; ui32 < dstate->numRxNodes + dstate->numTxNodes; ++ui32) {
        dstate->nodes[ui32].cdev.attr.mode |= S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;
    }

    LOGC("Starting: spi=%s, clock=%uHz, gpio=%d, bps=%u, unit=%u, numTx=%u, numRx=%u",
         dstate->spiPath, dstate->clock, dstate->gpio, dstate->bps, dstate->unit,
         dstate->numTxNodes, dstate->numRxNodes);


    // And start the driver/library. This won't return until it is time to stop.
    can_resmgr_start();

    return EOK;
}

void destroy_libcan(drv_state_t* dstate) {
    if (dstate->irqId != -1) {
        // Detach my event handler from the IRQ
        can_resmgr_detach_intr(dstate->irqId, PULSE_IRQ_FIRED);
        dstate->irqId = -1;
    }

    // Destroy all nodes
    for (uint32_t ui32 = 0; ui32 < dstate->numRxNodes + dstate->numTxNodes; ++ui32) {
        can_resmgr_destroy_device(&dstate->nodes[ui32].cdev);
    }
    free(dstate->nodes);
    dstate->nodes = NULL;

    free(dstate->driverFuncs);
    dstate->driverFuncs = NULL;

    can_resmgr_fini();
}


int execute(drv_state_t* dstate, int argc, char* argv[]) {
    int rc;

    if ((rc = parse_command_line(argc, argv, dstate)) != EOK) {
        return rc;
    }

    if ((rc = connect(dstate)) != EOK) {
        return rc;
    }

    if ((rc = configure_libcan(dstate)) != EOK) {
        return rc;
    }

    if ((rc = configure_mcp2515(dstate)) != EOK) {
        return rc;
    }

    if ((rc = go(dstate)) != EOK) {
        return rc;
    }

    LOGC("Shutting down");

    // Cleanup external things
    destroy_libcan(dstate);
    disconnect(dstate);

    return EOK;
}
