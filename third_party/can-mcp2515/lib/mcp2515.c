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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <mcp2515/bit_timing.h>
#include <mcp2515/mcp2515.h>

// The constants are a bit misleading, specifically BRP
// For the MCP2515 BRP(actual) = 2*(1 + BRP(reg))
// where BRP(reg) ranges from 0-63.
// So when doing the calculation, I have to assume
// the range is 2-128.
const can_bit_timing_consts_t MCP2515_BTConsts = {
    .brpMin = 2,
    .brpMax = 128,
    .brpInc = 2,
    .sjwMax = 4,
    .tseg1Min = 2,
    .tseg1Max = 16,
    .tseg2Min = 2,
    .tseg2Max = 8
};

// Encodes a MID into a form compatible with MCP2515's message ID registers.
// These registers are (in order)
// (TXBnSIDH, TXBnSIDL, TXBnEID8, TXBnEID0) or
// (RXFnSIDH, RXFnSIDL, RXFnEID8, RXFnEID0) or
// (RXMnSIDH, RXMnSIDL, RXMnEID8, RXMnEID0) or
// If a SID is being encoded the EID specific bits are set to 0
//
// regs must point to a buffer at least 4 bytes in length.
// regs[0] == *SIDH, regs[1] == *SIDL, regs[2] == *EID8, regs[3] == *EID0
void mcp2515_encode_mid(uint32_t mid, int isEid, uint8_t* regs) {
    assert(regs != NULL);

    memset(regs, 0, 4);

    // There are four registers in play, in order:
    // *SIDH, *SIDL, *EID8, *EID0
    // A SID mid is in bits 28:18
    // An EID mid is in bits 28:0

    // The MSBits of the EID map to the SID bits
    // Bits [28:21] of the EID
    // or
    // Bits [10:3] of the SID, which maps to bits [28:21] of the mid
    MCP2515_SET_BITFIELD(regs[0], (mid >> 21) & 0xFF, RXBnSIDH_SID3_10);

    // Bits [2:0] of the SID, which maps to bits [20:18] of the mid
    // OR
    // Bits [20:18] of the EID
    MCP2515_SET_BITFIELD(regs[1], (mid >> 18) & 0x7, RXBnSIDL_SID0_2);

    if (isEid) {
        // Bits [17:16] of the EID
        MCP2515_SET_BITFIELD(regs[1], (mid >> 16) & 0x3, RXBnSIDL_EID16_17);

        // Bits [15:8] of the EID
        MCP2515_SET_BITFIELD(regs[2], (mid >> 8) & 0xFF, RXBnEID8_15);

        // Bits [7:0] of the EID
        MCP2515_SET_BITFIELD(regs[3], mid & 0xFF, RXBnEID0_7);

        // Set the flag in *SIDL reg to indicate EID
        // This should be ignored if encoding for RXMnSIDL
        MCP2515_SET_BITFIELD(regs[1], 1, RXBnSIDL_IDE);

    } else {
        // Don't care about the remaining EID bits. HOWEVER the MCP2515 will automatically
        // apply these against the data in an SDF when doing filtering. We don't want that
        // to happen so these bits are set to 0 to be ignored when this function is used
        // to set a mask.
    }
}

// Decodes a MID from a set of MCP2515 message ID registers.
// These registers are (in order)
// (TXBnSIDH, TXBnSIDL, TXBnEID8, TXBnEID0) or
// (RXFnSIDH, RXFnSIDL, RXFnEID8, RXFnEID0) or
// (RXMnSIDH, RXMnSIDL, RXMnEID8, RXMnEID0) or
//
// regs must point to a buffer at least 4 bytes in length.
// regs[0] == *SIDH, regs[1] == *SIDL, regs[2] == *EID8, regs[3] == *EID0
//
// On return isEid is non-zero if the mid is extended (29 bits), zero
// if it is standard (11 bits)
void mcp2515_decode_mid(uint8_t* regs, uint32_t* mid, int *isEid) {
    uint32_t m;

    assert(regs != NULL);
    assert(mid != NULL);
    assert(isEid != NULL);
    // Decode the MID
    // A SID mid is in bits 28:18
    // An EID mid is in bits 28:0
    //
    // The MSBits of the EID map to the SID bits
    // Bits [28:21] of the EID
    // or
    // Bits [10:3] of the SID, which maps to bits [28:21] of the mid
    m = MCP2515_GET_BITFIELD(regs[0], RXBnSIDH_SID3_10) << 21;

    // Bits [2:0] of the SID, which maps to bits [20:18] of the mid
    // OR
    // Bits [20:18] of the EID
    m |= MCP2515_GET_BITFIELD(regs[1] , RXBnSIDL_SID0_2) << 18;

    if (MCP2515_GET_BITFIELD(regs[1], RXBnSIDL_IDE)) {
        *isEid = 1;

        // Bits [17:16] of the EID
        m |= MCP2515_GET_BITFIELD(regs[1], RXBnSIDL_EID16_17) << 16;

        // Bits [15:8] of the EID
        m |= MCP2515_GET_BITFIELD(regs[2], RXBnEID8_15) << 8;

        // Bits [7:0] of the EID
        m |= MCP2515_GET_BITFIELD(regs[3], RXBnEID0_7);
    } else {
        *isEid = 0;
    }

    *mid = m;
}
