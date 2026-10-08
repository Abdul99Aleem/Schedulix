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
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <string.h>

#include <mcp2515/bit_timing.h>
#include <mcp2515/command.h>
#include <mcp2515/io.h>
#include <mcp2515/mcp2515.h>
#include <mcp2515/util.h>

int mcp2515_cmd_set_bit_timings(mcp2515_state_t* state, const can_bit_timings_t* bt) {
    mcp2515_io_write_packet_t pkt;

    assert(state && "state is NULL");
    assert(bt && "bt is NULL");

    memset(&pkt.data[0], 0, 3);

    // Register order is CNF3, CNF2, CNF1
    //CNF3
    assert(bt->phase2 >= 2 && bt->phase2 <= 8);
    MCP2515_SET_BITFIELD(pkt.data[0], bt->phase2-1, CNF3_PHSEG2);
    //CNF2
    assert(bt->prop >= 1 && bt->prop <= 8);
    assert(bt->phase1 >= 1 && bt->phase2 <= 8);
    MCP2515_SET_BITFIELD(pkt.data[1], 1, CNF2_BTLMODE);
    MCP2515_SET_BITFIELD(pkt.data[1], bt->phase1-1, CNF2_PHSEG1);
    MCP2515_SET_BITFIELD(pkt.data[1], bt->prop-1, CNF2_PRSEG);
    //CNF1
    assert(bt->sjw >= 1 && bt->sjw <= 4);
    assert(bt->brp >= 2 && bt->brp <= 128);
    MCP2515_SET_BITFIELD(pkt.data[2], bt->sjw-1, CNF1_SJW);
    MCP2515_SET_BITFIELD(pkt.data[2], bt->brp/2-1, CNF1_BRP);

    return mcp2515_io_write(state, MCP2515_CNF3_REG_ADDR, 3, &pkt);
}

int mcp2515_cmd_set_rx_mid_filter(mcp2515_state_t* state, int rxIdx, uint32_t mid, int isEid) {
    mcp2515_io_write_packet_t pkt;
    char errStr[ERR_STR_SIZE];
    int rc;

    assert(state && "state is NULL");
    assert(rxIdx >= 0 && rxIdx < MCP2515_NUM_RX_BUFFERS && "rxIdx is invalid");

    // RX0 uses filters 0 & 1
    // RX1 uses filters 2, 3, 4, and 5
    // Since there is no way to disable a filter, we have to set all filters associated
    // with a mailbox to ensure that we don't accidentally end up accepting things
    // we don't want.
    int i;
    int end;
    if (rxIdx == 0) {
        i = 0;
        end = 1;
    } else {
        i = 2;
        end = 5;
    }

    for (; i <= end; ++i) {
        mcp2515_encode_mid(mid, isEid, &pkt.data[0]);
        if (isEid) {
            // EID flag
            MCP2515_SET_BITFIELD(pkt.data[1], 1, RXFnSIDL_EXIDE);
        }

        rc = mcp2515_io_write(state, MCP2515_RXFnSIDH_REG_ADDR(i), 4, &pkt);
        if (rc) {
            LOGE("Unable to set rx mid filter %d. Error=%s", i, sstrerror(rc, errStr, sizeof(errStr)));
            return rc;
        }
    }

    // An Rx mailbox can be set to receive either SDFs, EDFs, or both. I need
    // it to reflect that style of MID so set it here.
    rc = mcp2515_cmd_set_rx_mode(state, rxIdx, isEid ? MCP2515_RXBnCTRL_RXM_EXTENDED_ONLY : MCP2515_RXBnCTRL_RXM_STANDARD_ONLY);
    if (rc) {
        LOGE("Unable to set rx mode. Error=%s", sstrerror(rc, errStr, sizeof(errStr)));
        return rc;
    }

    return EOK;
}

int mcp2515_cmd_set_rx_mid_mask(mcp2515_state_t* state, int rxIdx, uint32_t mask, int isEid) {
    mcp2515_io_write_packet_t pkt;

    assert(state && "state is NULL");
    assert(rxIdx >= 0 && rxIdx < MCP2515_NUM_RX_BUFFERS && "rxIdx is invalid");

    mcp2515_encode_mid(mask, isEid, &pkt.data[0]);
    return mcp2515_io_write(state, MCP2515_RXMnSIDH_REG_ADDR(rxIdx), 4, &pkt);
}

int mcp2515_cmd_set_rx_mode(mcp2515_state_t* state, int rxIdx, mcp2515_rxbnctrl_rxm_t mode) {
    assert(state && "state is NULL");
    assert(rxIdx >= 0 && rxIdx < MCP2515_NUM_RX_BUFFERS && "rxIdx is invalid");

    return MCP2515_IO_SET_BITFIELD(state, MCP2515_RXBnCTRL_REG_ADDR(rxIdx), RXBnCTRL_RXM, mode);
}

int mcp2515_cmd_set_op_mode(mcp2515_state_t* state, mcp2515_canstat_opmod_t opmod) {
    int rc;
    char errStr[ERR_STR_SIZE];
    int poll;
    struct timespec ts = {.tv_sec = 0, .tv_nsec = 300*1000};

    assert(state && "state is NULL");

    // Set the mode
    rc = MCP2515_IO_SET_BITFIELD(state, MCP2515_CANCTRL_REG_ADDR, CANCTRL_REQOP, opmod);
    if (rc) {
        LOGE("Unable to set operating mode to %d. Error=%s", opmod, sstrerror(rc, errStr, sizeof(errStr)));
        return rc;
    }

    // Operating mode switch happens after all pending transmissions are complete.
    // How long a txBuffer can take to flush itself out depends on the size
    // of the CANFrame and the bitrate.
    //
    // So poll for the operating mode switch to complete.
    //
    // It doesn't _really_ matter/ if I wait longer than I need to. Other than
    // CAN communication will be delayed (and thus CANFrames might get missed).
    // So ideally we want to wait as little as possible.
    //
    // That said, the range for how long it takes is rather large. Try to deal
    // with this by setting up an initial poll tailored for the two most common
    // bitrates.
    // At 1Mbs a frame takes ~47-125us.
    // At 500Kbps a frame takes ~94-250us
    // Given that there are three tx buffers it can take up to 250*3=750us to wait,
    // assuming there is no contention on the bus. Bump that up a bit to give a bit
    // a slack and I'm going to poll every 300us or so. I'm doing it 10 times for
    // a max delay of 3ms.
    for (poll = 0; poll < 10; ++poll) {
        if (poll != 0) nanospin(&ts);
        int status = mcp2515_io_read_byte(state, MCP2515_CANSTAT_REG_ADDR);
        if (status >= 0 && MCP2515_GET_BITFIELD(status, CANSTAT_OPMOD) == opmod) {
            return EOK;
        }
    }
    LOGW("mcp2515@%d: Change to opmod=%d has exceeded initial poll....", state->fd, opmod);

    // I still haven't transitioned. This could be because the bitrate is really slow
    // or the bus is really contended or whatever. I'm going to keep polling but this
    // time I'll do a poll every ms (or so).
    for (poll = 0; poll < 10; ++poll) {
        if (poll != 0) usleep(1000);
        int status = mcp2515_io_read_byte(state, MCP2515_CANSTAT_REG_ADDR);
        if (status >= 0 && MCP2515_GET_BITFIELD(status, CANSTAT_OPMOD) == opmod) {
            return EOK;
        }
    }

    LOGE("mcp2515@%d: Unable to change operating mode to %d.", opmod);
    return EIO;
}

int mcp2515_cmd_tx_can_message(mcp2515_state_t* state, int txBuf, CAN_MSG* msg) {
    int rc;
    mcp2515_io_load_tx_buffer_packet_t pkt;
    char errStr[ERR_STR_SIZE];

    assert(state != NULL && "state is NULL");
    assert(txBuf >= 0 && txBuf < MCP2515_NUM_TX_BUFFERS && "txBuf out of range");
    assert(msg != NULL && "msg is NULL");
    assert(msg->len <= 8 && "msg is too large");

    // Bytes in the data/payload of pkt are (in order)
    // TXBnSIDH, TXBnSIDL, TXBnEID8, TXBnEID0, TXBnDLC,
    // TXBnDATA0, TXBnDATA1, TXBnDATA2, TXBnDATA3,
    // TXBnDATA4, TXBnDATA5, TXBnDATA6, TXBnDATA7

    memset(&pkt.data[0], 0, 5+8);

    // Encode the mid.
    mcp2515_encode_mid(msg->mid, msg->ext.is_extended_mid, &pkt.data[0]);

    // Set DLC
    MCP2515_SET_BITFIELD(pkt.data[4], msg->len, TXBnDLC_DLC);

    // Set correct bit if frame is RTR.
    if (msg->ext.is_remote_frame) {
        MCP2515_SET_BITFIELD(pkt.data[4], 1, TXBnDLC_RTR);
    }

    // Data!
    memcpy(&pkt.data[5], msg->dat, msg->len);

    rc = mcp2515_io_load_tx_buffer(state, txBuf, 0, &pkt);
    if (rc) {
        LOGE("mcp2515@%d: tx_can_message(txBuf=%d) FAILED to load message! Error=%s",
             state->fd, txBuf, sstrerror(rc, errStr, sizeof(errStr)));
        return rc;
    }

    // Set the request-to-send flag
    rc = MCP2515_IO_SET_BIT(state, MCP2515_TXBnCTRL_REG_ADDR(txBuf), TXBnCTRL_TXREQ);
    if (rc) {
        LOGE("mcp2515@%d: tx_can_message(txBuf=%d) FAILED to RTS! Error=%s",
             state->fd, txBuf, sstrerror(rc, errStr, sizeof(errStr)));
        return rc;
    }

    LOGD1("mcp2515@%d: tx_can_message(txBuf=%d, msg=(mid=0x%08x, len=%d eid=%d rtr=%d data=%02x%02x%02x%02x%02x%02x%02x%02x))",
            state->fd, txBuf, msg->mid, msg->len, msg->ext.is_extended_mid, msg->ext.is_remote_frame,
            msg->dat[0], msg->dat[1], msg->dat[2], msg->dat[3],
            msg->dat[4], msg->dat[5], msg->dat[6], msg->dat[7]);

    return EOK;
}

int mcp2515_cmd_rx_can_message(mcp2515_state_t* state, int rxBuf, CAN_MSG* msg) {
    mcp2515_io_read_rx_buffer_packet_t pkt;
    int rc;
    char errStr[ERR_STR_SIZE];
    uint32_t mid;
    int isEid;

    assert(state != NULL && "state is NULL");
    assert(rxBuf >= 0 && rxBuf < MCP2515_NUM_RX_BUFFERS && "rxBuf out of range");
    assert(msg != NULL && "msg is NULL");

    // Read the message from the HW.
    // Reading it this way (instead of the generic SPI read command)
    // will automatically clear the RX buffer's interrupt.
    rc = mcp2515_io_read_rx_buffer(state, rxBuf, 0, &pkt);
    if (rc) {
        LOGE("mcp2515@%d: rx_can_message(rxBuf=%d) FAILED! Error=%s",
             state->fd, rxBuf, sstrerror(rc, errStr, sizeof(errStr)));
        return rc;
    }

    // Bytes in the data/payload of pkt are (in order)
    // RXBnSIDH, RXBnSIDL, RXBnEID8, RXBnEID0, RXBnDLC,
    // RXBnDATA0, RXBnDATA1, RXBnDATA2, RXBnDATA3,
    // RXBnDATA4, RXBnDATA5, RXBnDATA6, RXBnDATA7
    //
    // Decode them into an actual CAN_MSG
    memset(msg, 0, sizeof(*msg));

    // Figure out the mid
    mcp2515_decode_mid(&pkt.data[0], &mid, &isEid);
    msg->mid = mid;
    msg->ext.is_extended_mid = isEid;

    // Is it an RTR frame?
    if (isEid) {
        // RTR bit is in the DLC byte
        msg->ext.is_remote_frame = MCP2515_GET_BITFIELD(pkt.data[4], RXBnDLC_RTR);
    } else {
        // RTR bit is in the SIDL byte
        msg->ext.is_remote_frame = MCP2515_GET_BITFIELD(pkt.data[1], RXBnSIDL_SRR);
    }

    // DLC
    msg->len = MCP2515_GET_BITFIELD(pkt.data[4], RXBnDLC_DLC);

    // Data!
    memcpy(&msg->dat[0], &pkt.data[5], msg->len);

    LOGD1("mcp2515@%d: rx_can_message(rxBuf=%d) => MSG(mid=0x%08x, len=%d eid=%d rtr=%d data=%02x%02x%02x%02x%02x%02x%02x%02x",
            state->fd, rxBuf, msg->mid, msg->len, msg->ext.is_extended_mid, msg->ext.is_remote_frame,
            msg->dat[0], msg->dat[1], msg->dat[2], msg->dat[3],
            msg->dat[4], msg->dat[5], msg->dat[6], msg->dat[7]);

    return EOK;
}
