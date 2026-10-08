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

#include "debug.h"
#include "drv_state.h"
#include "mcp2515/command.h"
#include "mcp2515/io.h"
#include "mcp2515/mcp2515.h"

int do_status_regs(mcp2515_state_t* state, char *ptr, size_t size) {
    mcp2515_io_read_packet_t pkt;
    int rc;
    char* origPtr = ptr;

    ptr += snprintf(ptr, size-(ptr-origPtr), "Dumping MCP2515 Status Registers\n");
    ptr += snprintf(ptr, size-(ptr-origPtr), "================================\n");

    // Read blocks of status registers.
    rc = mcp2515_io_read(state, MCP2515_BFPCTRL_REG_ADDR, 4, &pkt);
    if (!rc) {
        ptr += snprintf(ptr, size-(ptr-origPtr), "BFPCTRL=0x%02x TXRTSCTRL=0x%02x CANSTAT=0x%02x CANCTRL=0x%02x\n",
               pkt.data[0], pkt.data[1], pkt.data[2], pkt.data[3]);
    } else {
        ptr += snprintf(ptr, size-(ptr-origPtr), "BFPCTRL=?? TXRTSCTRL=?? CANSTAT=?? CANCTRL=??\n");
    }

    rc = mcp2515_io_read(state, MCP2515_TEC_REG_ADDR, 2, &pkt);
    if (!rc) {
        ptr += snprintf(ptr, size-(ptr-origPtr), "TEC=%d REC=%d\n", pkt.data[0], pkt.data[1]);
    } else {
        ptr += snprintf(ptr, size-(ptr-origPtr), "TEC=? REC=?\n");
    }

    rc = mcp2515_io_read(state, MCP2515_CNF3_REG_ADDR, 6, &pkt);
    if (!rc) {
        ptr += snprintf(ptr, size-(ptr-origPtr), "CNF1=0x%02x CNF2=0x%02x CNF3=0x%02x CANINTE=0x%02x CANINTF=0x%02x EFLG=0x%02x\n",
                pkt.data[0], pkt.data[1], pkt.data[2], pkt.data[3], pkt.data[4], pkt.data[5]);
    } else {
        ptr += snprintf(ptr, size-(ptr-origPtr), "CNF1=?? CNF2=?? CNF3=?? CANINTE=?? CANINTF=?? EFLG=??\n");
    }

    // I have to read the tx/rx ctrl registers one at a time....
    for (int i = 0; i < MCP2515_NUM_TX_BUFFERS; i++) {
        rc = mcp2515_io_read(state, MCP2515_TXBnCTRL_REG_ADDR(i), 1, &pkt);
        if (!rc) {
            ptr += snprintf(ptr, size-(ptr-origPtr), "TXB%dCTRL=0x%02x ", i, pkt.data[0]);
        } else {
            ptr += snprintf(ptr, size-(ptr-origPtr), "TXB%dCTRL=?? ", i);
        }
    }
    *(ptr-1) = '\n';

    for (int i = 0; i < MCP2515_NUM_RX_BUFFERS; i++) {
        rc = mcp2515_io_read(state, MCP2515_RXBnCTRL_REG_ADDR(i), 1, &pkt);
        if (!rc) {
            ptr += snprintf(ptr, size-(ptr-origPtr), "RXB%dCTRL=0x%02x ", i, pkt.data[0]);
        } else {
            ptr += snprintf(ptr, size-(ptr-origPtr), "RXB%dCTRL=?? ", i);
        }
    }
    *(ptr-1) = '\n';

    return ptr-origPtr;
}

static int do_rx_filter_regs(mcp2515_state_t* state, int filter, char *ptr, size_t size) {
    mcp2515_io_read_packet_t pkt;
    int rc;
    char* origPtr = ptr;

    rc = mcp2515_io_read(state, MCP2515_RXFnSIDH_REG_ADDR(filter), 4, &pkt);
    if (!rc) {
        ptr += snprintf(ptr, size-(ptr-origPtr), "RXF%dSIDH=0x%02x RXF%dSIDL=0x%02x RXF%dEID8=0x%02x RXF%dEID0=0x%02x\n",
               filter, pkt.data[0], filter, pkt.data[1], filter, pkt.data[2], filter, pkt.data[3]);
    } else {
        ptr += snprintf(ptr, size-(ptr-origPtr), "RXF%dSIDH=?? RXF%dSIDL=?? RXF%dEID8=?? RXF%dEID0=??x\n",
                filter, filter, filter, filter);
    }

    return ptr - origPtr;
}

static int do_rx_mask_regs(mcp2515_state_t* state, int mask, char *ptr, size_t size) {
    mcp2515_io_read_packet_t pkt;
    int rc;
    char* origPtr = ptr;

    rc = mcp2515_io_read(state, MCP2515_RXMnSIDH_REG_ADDR(mask), 4, &pkt);
    if (!rc) {
        ptr += snprintf(ptr, size-(ptr-origPtr), "RXM%dSIDH=0x%02x RXM%dSIDL=0x%02x RXM%dEID8=0x%02x RXM%dEID0=0x%02x\n",
               mask, pkt.data[0], mask, pkt.data[1], mask, pkt.data[2], mask, pkt.data[3]);
    } else {
        ptr += snprintf(ptr, size-(ptr-origPtr), "RXM%dSIDH=?? RXM%dSIDL=?? RXM%dEID8=?? RXM%dEID0=??x\n",
                mask, mask, mask, mask);
    }

    return ptr - origPtr;
}

static int do_rxb_filter_regs(mcp2515_state_t* state, int rxId, char *ptr, size_t size) {
    char* origPtr = ptr;
    int i;

    ptr += snprintf(ptr, size-(ptr-origPtr), "Dumping MCP2515 RX%d Filter/Mask Registers\n", rxId);
    ptr += snprintf(ptr, size-(ptr-origPtr), "==========================================\n");
    ptr += do_rx_mask_regs(state, rxId, ptr, size-(ptr-origPtr));
    if (rxId == 0) {
        // Two filters: [0,1]
        for (i = 0; i <= 1; ++i) {
            ptr += do_rx_filter_regs(state, i, ptr, size-(ptr-origPtr));
        }
    } else {
        // Four filters: [2,5]
        for (i = 2; i <= 5; ++i) {
            ptr += do_rx_filter_regs(state, i, ptr, size-(ptr-origPtr));
        }
    }

    return ptr - origPtr;
}

void dump_mcp2515_status_registers(drv_state_t* state) {
    char buffer[1024];

    do_status_regs(&state->mcp2515, buffer, sizeof(buffer));
    LOGC("\n%s", buffer);
}

void dump_mcp2515_rx_filter_registers(drv_state_t* state, int rxId) {
    char buffer[1024];

    do_rxb_filter_regs(&state->mcp2515, rxId, buffer, sizeof(buffer));
    LOGC("\n%s", buffer);
}

void dump_mcp2515_all_rx_filter_registers(drv_state_t* state) {
    char buffer[1024];
    char *ptr = buffer;
    int i;

    // Need to be in configuration mode to read registers?
    LOGN("Switching mcp2515 to config mode to read filter registers.");
    mcp2515_cmd_set_op_mode(&state->mcp2515, MCP2515_CANSTAT_OPMOD_CONFIG);

    for (i = 0; i < MCP2515_NUM_RX_BUFFERS; ++i) {
        ptr += do_rxb_filter_regs(&state->mcp2515, i, ptr, sizeof(buffer)-(ptr-buffer));
    }
    mcp2515_cmd_set_op_mode(&state->mcp2515, MCP2515_CANSTAT_OPMOD_NORMAL);

    LOGC("\n%s", buffer);
}

