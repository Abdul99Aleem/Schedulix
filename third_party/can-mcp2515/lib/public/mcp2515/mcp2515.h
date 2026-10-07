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

#ifndef LIB_PUBLIC_MCP2515_MCP2515_H_
#define LIB_PUBLIC_MCP2515_MCP2515_H_

/**
 * @file
 *
 * Hardware level information about the MCP2515.
 *
 * See the MCP2515 datasheet for more details about the various registers
 * and the bitfields within them.
 */

#include <stdint.h>

/**
 * Helper that can be used to easily get a bitfield from the value read
 * from a MCP2515 register.
 *
 * @param[in]   value   The value read from the register
 * @param[in]   bf      The NAME of the bitfield to extract. This is used
 *                      to construct references to MCP2515_<bf>_MASK and
 *                      MCP2515_<bf>_OFFSET constants.
 *
 * @return The desired bitfield. Note that the return value is shifted so that
 *         it starts at bit 0. Regardless of where it actually is in the register.
 */          
#define MCP2515_GET_BITFIELD(value, bf) \
    (((value) & (MCP2515_##bf##_MASK)) >> (MCP2515_##bf##_OFFSET))

/**
 * Helper that can be used to easily set a bitfield in a value that will be
 * used to write to a MCP2515 register. The specified value is modified in
 * place.
 *
 * @param[in]   value   The current value for the register. This will be
 *                      modified in place to have the desired value in the
 *                      bitfield.
 * @param[in]   newFieldValue The new value for the bitfield. Note that it
 *                            should start at bit 0. This API is responsible
 *                            for shifting it to the correct location.
 * @param[in]   bf      The NAME of the bitfield to extract. This is used
 *                      to construct references to MCP2515_<bf>_MASK and
 *                      MCP2515_<bf>_OFFSET constants.
 */          
#define MCP2515_SET_BITFIELD(value, newFieldValue, bf) \
    (value) = (((value) & ~(MCP2515_##bf##_MASK)) | (((newFieldValue) << (MCP2515_##bf##_OFFSET)) & (MCP2515_##bf##_MASK)))

/** @name RegAddresses
 * Addresses for the various registers exposed by the MCP2515.
 * See the datasheet for more details.
 *
 * All #defines have the form MCP2515_<regName>_REG_ADDR where <regName> is
 * the name given in the MCP2515 datasheet.
 *
 * @note When there are multiple registers for something an 'n' version of
 *       #define is provided to get the address for the nth register. No
 *       bounds checking is done on the input.
 *
 *       For example. The address for the three TXB*CTRL registers (TXB0CTRL,
 *       TXB1CTRL, and TXB2CTRL) can be specified via TXBnCTRL(0), TXBnCTRL(1),
 *       or TXBnCTRL(2) respectively.
 */
/**@{*/
#define MCP2515_BFPCTRL_REG_ADDR        0x0C
#define MCP2515_CANSTAT_REG_ADDR        0x0E
#define MCP2515_CANCTRL_REG_ADDR        0x0F
#define MCP2515_TEC_REG_ADDR            0x1C
#define MCP2515_REC_REG_ADDR            0x1D
#define MCP2515_CNFn_REG_ADDR(n)        (MCP2515_CNF1_REG_ADDR - (n))
#define MCP2515_CNF3_REG_ADDR           0x28
#define MCP2515_CNF2_REG_ADDR           0x29
#define MCP2515_CNF1_REG_ADDR           0x2A
#define MCP2515_CANINTE_REG_ADDR        0x2B
#define MCP2515_CANINTF_REG_ADDR        0x2C
#define MCP2515_EFLG_REG_ADDR           0x2D

// TX Registers
#define MCP2515_TXBnCTRL_REG_ADDR(n)    (MCP2515_TXB0CTRL_REG_ADDR + (0x10 * (n)))
#define MCP2515_TXB0CTRL_REG_ADDR       0x30
#define MCP2515_TXB1CTRL_REG_ADDR       0x40
#define MCP2515_TXB2CTRL_REG_ADDR       0x50
#define MCP2515_TXRTSCTRL_REG_ADDR      0x0D

#define MCP2515_TXBnSIDH_REG_ADDR(n)    (MCP2515_TXB0SIDH_REG_ADDR + (0x10 * (n)))
#define MCP2515_TXB0SIDH_REG_ADDR       0x31
#define MCP2515_TXB1SIDH_REG_ADDR       0x41
#define MCP2515_TXB2SIDH_REG_ADDR       0x51

#define MCP2515_TXBnSIDL_REG_ADDR(n)    (MCP2515_TXB0SIDL_REG_ADDR + (0x10 * (n)))
#define MCP2515_TXB0SIDL_REG_ADDR       0x32
#define MCP2515_TXB1SIDL_REG_ADDR       0x42
#define MCP2515_TXB2SIDL_REG_ADDR       0x52

#define MCP2515_TXBnEID8_REG_ADDR(n)    (MCP2515_TXB0EID8_REG_ADDR + (0x10 * (n)))
#define MCP2515_TXB0EID8_REG_ADDR       0x33
#define MCP2515_TXB1EID8_REG_ADDR       0x43
#define MCP2515_TXB2EID8_REG_ADDR       0x53

#define MCP2515_TXBnEID0_REG_ADDR(n)    (MCP2515_TXB0EID0_REG_ADDR + (0x10 * (n)))
#define MCP2515_TXB0EID0_REG_ADDR       0x34
#define MCP2515_TXB1EID0_REG_ADDR       0x44
#define MCP2515_TXB2EID0_REG_ADDR       0x54

#define MCP2515_TXBnDLC_REG_ADDR(n)     (MCP2515_TXB0DLC_REG_ADDR + (0x10 * (n)))
#define MCP2515_TXB0DLC_REG_ADDR        0x35
#define MCP2515_TXB1DLC_REG_ADDR        0x45
#define MCP2515_TXB2DLC_REG_ADDR        0x55

#define MCP2515_TXBnDATA_REG_ADDR(n)    (MCP2515_TXB0D0_REG_ADDR + (0x10 * (n)))
#define MCP2515_TXBnDm_REG_ADDR(n,m)    (MCP2515_TXB0D0_REG_ADDR + (0x10 * (n)) + (m))

#define MCP2515_TXB0D0_REG_ADDR         0x36
#define MCP2515_TXB0D1_REG_ADDR         0x37
#define MCP2515_TXB0D2_REG_ADDR         0x38
#define MCP2515_TXB0D3_REG_ADDR         0x39
#define MCP2515_TXB0D4_REG_ADDR         0x3A
#define MCP2515_TXB0D5_REG_ADDR         0x3B
#define MCP2515_TXB0D6_REG_ADDR         0x3C
#define MCP2515_TXB0D7_REG_ADDR         0x3D

#define MCP2515_TXB1D0_REG_ADDR         0x46
#define MCP2515_TXB1D1_REG_ADDR         0x47
#define MCP2515_TXB1D2_REG_ADDR         0x48
#define MCP2515_TXB1D3_REG_ADDR         0x49
#define MCP2515_TXB1D4_REG_ADDR         0x4A
#define MCP2515_TXB1D5_REG_ADDR         0x4B
#define MCP2515_TXB1D6_REG_ADDR         0x4C
#define MCP2515_TXB1D7_REG_ADDR         0x4D

#define MCP2515_TXB2D0_REG_ADDR         0x56
#define MCP2515_TXB2D1_REG_ADDR         0x57
#define MCP2515_TXB2D2_REG_ADDR         0x58
#define MCP2515_TXB2D3_REG_ADDR         0x59
#define MCP2515_TXB2D4_REG_ADDR         0x5A
#define MCP2515_TXB2D5_REG_ADDR         0x5B
#define MCP2515_TXB2D6_REG_ADDR         0x5C
#define MCP2515_TXB2D7_REG_ADDR         0x5D

// RX Registers
#define MCP2515_RXBnCTRL_REG_ADDR(n)    (MCP2515_RXB0CTRL_REG_ADDR + (0x10 * (n)))
#define MCP2515_RXB0CTRL_REG_ADDR       0x60
#define MCP2515_RXB1CTRL_REG_ADDR       0x70

#define MCP2515_RXFnSIDH_REG_ADDR(n)    (((n) >= 3) ? (MCP2515_RXF3SIDH_REG_ADDR + (((n)-3) * 4)) : (MCP2515_RXF0SIDH_REG_ADDR + ((n) * 4)))
#define MCP2515_RXFnSIDL_REG_ADDR(n)    (((n) >= 3) ? (MCP2515_RXF3SIDL_REG_ADDR + (((n)-3) * 4)) : (MCP2515_RXF0SIDL_REG_ADDR + ((n) * 4)))
#define MCP2515_RXFnEID8_REG_ADDR(n)    (((n) >= 3) ? (MCP2515_RXF3EID8_REG_ADDR + (((n)-3) * 4)) : (MCP2515_RXF0EID8_REG_ADDR + ((n) * 4)))
#define MCP2515_RXFnEID0_REG_ADDR(n)    (((n) >= 3) ? (MCP2515_RXF3EID0_REG_ADDR + (((n)-3) * 4)) : (MCP2515_RXF0EID0_REG_ADDR + ((n) * 4)))
#define MCP2515_RXF0SIDH_REG_ADDR       0x00
#define MCP2515_RXF0SIDL_REG_ADDR       0x01
#define MCP2515_RXF0EID8_REG_ADDR       0x02
#define MCP2515_RXF0EID0_REG_ADDR       0x03
#define MCP2515_RXF1SIDH_REG_ADDR       0x04
#define MCP2515_RXF1SIDL_REG_ADDR       0x05
#define MCP2515_RXF1EID8_REG_ADDR       0x06
#define MCP2515_RXF1EID0_REG_ADDR       0x07
#define MCP2515_RXF2SIDH_REG_ADDR       0x08
#define MCP2515_RXF2SIDL_REG_ADDR       0x09
#define MCP2515_RXF2EID8_REG_ADDR       0x0A
#define MCP2515_RXF2EID0_REG_ADDR       0x0B
#define MCP2515_RXF3SIDH_REG_ADDR       0x10
#define MCP2515_RXF3SIDL_REG_ADDR       0x11
#define MCP2515_RXF3EID8_REG_ADDR       0x12
#define MCP2515_RXF3EID0_REG_ADDR       0x13
#define MCP2515_RXF4SIDH_REG_ADDR       0x14
#define MCP2515_RXF4SIDL_REG_ADDR       0x15
#define MCP2515_RXF4EID8_REG_ADDR       0x16
#define MCP2515_RXF4EID0_REG_ADDR       0x17
#define MCP2515_RXF5SIDH_REG_ADDR       0x18
#define MCP2515_RXF5SIDL_REG_ADDR       0x19
#define MCP2515_RXF5EID8_REG_ADDR       0x1A
#define MCP2515_RXF5EID0_REG_ADDR       0x1B

#define MCP2515_RXMnSIDH_REG_ADDR(n)    (MCP2515_RXM0SIDH_REG_ADDR + ((n) * 4))
#define MCP2515_RXMnSIDL_REG_ADDR(n)    (MCP2515_RXM0SIDL_REG_ADDR + ((n) * 4))
#define MCP2515_RXMnEID8_REG_ADDR(n)    (MCP2515_RXM0EID8_REG_ADDR + ((n) * 4))
#define MCP2515_RXMnEID0_REG_ADDR(n)    (MCP2515_RXM0EID0_REG_ADDR + ((n) * 4))
#define MCP2515_RXM0SIDH_REG_ADDR       0x20
#define MCP2515_RXM0SIDL_REG_ADDR       0x21
#define MCP2515_RXM0EID8_REG_ADDR       0x22
#define MCP2515_RXM0EID0_REG_ADDR       0x23
#define MCP2515_RXM1SIDH_REG_ADDR       0x24
#define MCP2515_RXM1SIDL_REG_ADDR       0x25
#define MCP2515_RXM1EID8_REG_ADDR       0x26
#define MCP2515_RXM1EID0_REG_ADDR       0x27
/**@}*/

/** @name RegFields
 * Masks and offsets for the various bitfields contained with the MCP2515's
 * registers. See the datasheet for more details.
 *
 * For each field <fieldName> in register <regName> two #defines are provided:
 * - MCP2515_<regName>_<fieldName>_MASK masks out the bitfield in the register
 * - MCP2515_<regName>_<fieldName>_OFFSET the starting bit of the bitfield in
 *                                        the register
 *
 * @note When there are multiple registers all with the same bitfields, the
 *       'n' version of the <regName> is used.
 *
 *       For example. The <regName> for the fields of the three TXB*CTRL
 *       registers (TXB0CTRL, TXB1CTRL, and TXB2CTRL) is TXBnCTRL
 */
/**@{*/
// CANCTRL fields
#define MCP2515_CANCTRL_REQOP_MASK      0b11100000 // R/W-100
#define MCP2515_CANCTRL_ABAT_MASK       0b00010000 // R/W-0
#define MCP2515_CANCTRL_OSM_MASK        0b00001000 // R/W-0
#define MCP2515_CANCTRL_CLKEN_MASK      0b00000100 // R/W-1
#define MCP2515_CANCTRL_CLKPRE_MASK     0b00000011 // R/W-11
#define MCP2515_CANCTRL_REQOP_OFFSET    5
#define MCP2515_CANCTRL_ABAT_OFFSET     4
#define MCP2515_CANCTRL_OSM_OFFSET      3
#define MCP2515_CANCTRL_CLKEN_OFFSET    2
#define MCP2515_CANCTRL_CLKPRE_OFFSET   0
typedef enum {
    MCP2515_REQOP_NORMAL      = 0,
    MCP2515_REQOP_SLEEP       = 1,
    MCP2515_REQOP_LOOPBACK    = 2,
    MCP2515_REQOP_LISTEN      = 3,
    MCP2515_REQOP_CONFIG      = 4
} mcp2515_canctrl_reqop_t;
typedef enum {
    FULL    = 0,
    HALF    = 1,
    QUARTER = 2,
    EIGHTH  = 3
} mcp2515_canctrl_clkpre_t;

// CANSTAT fields
#define MCP2515_CANSTAT_OPMOD_MASK      0b11100000 // R-100
#define MCP2515_CANSTAT_ICOD_MASK       0b00001110 // R-000
#define MCP2515_CANSTAT_OPMOD_OFFSET    5
#define MCP2515_CANSTAT_ICOD_OFFSET     1
typedef enum {
    MCP2515_CANSTAT_OPMOD_NORMAL      = 0,
    MCP2515_CANSTAT_OPMOD_SLEEP       = 1,
    MCP2515_CANSTAT_OPMOD_LOOPBACK    = 2,
    MCP2515_CANSTAT_OPMOD_LISTEN      = 3,
    MCP2515_CANSTAT_OPMOD_CONFIG      = 4
} mcp2515_canstat_opmod_t;
typedef enum {
    MCP2515_CANSTAT_ICOD_NONE    = 0,
    MCP2515_CANSTAT_ICOD_ERROR   = 1,
    MCP2515_CANSTAT_ICOD_WAKE    = 2,
    MCP2515_CANSTAT_ICOD_TXB0    = 3,
    MCP2515_CANSTAT_ICOD_TXB1    = 4,
    MCP2515_CANSTAT_ICOD_TXB2    = 5,
    MCP2515_CANSTAT_ICOD_RXB0    = 6,
    MCP2515_CANSTAT_ICOD_RXB1    = 7
} mcp2515_canstat_icod_t;

// TXBnCTRL fields
#define MCP2515_TXBnCTRL_ABTF_MASK       0b01000000  // R-0
#define MCP2515_TXBnCTRL_MLOA_MASK       0b00100000  // R-0
#define MCP2515_TXBnCTRL_TXERR_MASK      0b00010000  // R-0
#define MCP2515_TXBnCTRL_TXREQ_MASK      0b00001000  // R/W-0
#define MCP2515_TXBnCTRL_TXP_MASK        0b00000011  // R/W-00
#define MCP2515_TXBnCTRL_ABTF_OFFSET     6
#define MCP2515_TXBnCTRL_MLOA_OFFSET     5
#define MCP2515_TXBnCTRL_TXERR_OFFSET    4
#define MCP2515_TXBnCTRL_TXREQ_OFFSET    3
#define MCP2515_TXBnCTRL_TXP_OFFSET      0
typedef enum {
    MCP2515_TXBnCTRL_TXP_LOWEST  = 0,
    MCP2515_TXBnCTRL_TXP_LOW     = 1,
    MCP2515_TXBnCTRL_TXP_HIGH    = 2,
    MCP2515_TXBnCTRL_TXP_HIGHEST = 3 
} mcp2515_txbnctrl_txp_t;

// TXRTSCTRL fields
#define MCP2515_TXRTSCTRL_B2RTS_MASK    0b00100000  // R-x
#define MCP2515_TXRTSCTRL_B1RTS_MASK    0b00010000  // R-x
#define MCP2515_TXRTSCTRL_B0RTS_MASK    0b00001000  // R-x
#define MCP2515_TXRTSCTRL_B2RTSM_MASK   0b00000100  // R/W-0
#define MCP2515_TXRTSCTRL_B1RTSM_MASK   0b00000010  // R/W-0
#define MCP2515_TXRTSCTRL_B0RTSM_MASK   0b00000001  // R/W-0
#define MCP2515_TXRTSCTRL_B2RTS_OFFSET  5
#define MCP2515_TXRTSCTRL_B1RTS_OFFSET  4
#define MCP2515_TXRTSCTRL_B0RTS_OFFSET  3
#define MCP2515_TXRTSCTRL_B2RTSM_OFFSET 2
#define MCP2515_TXRTSCTRL_B1RTSM_OFFSET 1
#define MCP2515_TXRTSCTRL_B0RTSM_OFFSET 0
typedef enum { 
    MCP2515_TXRTSCTRL_RTSM_DIGITAL_INPUT   = 0,
    MCP2515_TXRTSCTRL_RTSM_REQUEST_TO_SEND = 1
} mcp2515_txrtsctrl_rtsm_t;

// TXBnSIDH fields
#define MCP2515_TXBnSIDH_SID_MASK    0b11111111 // R/W-xxxxxxxx
#define MCP2515_TXBnSIDH_SID_OFFSET  0

// TXBnSIDL fields
#define MCP2515_TXBnSIDL_SID_MASK        0b11100000 // R/W-xxx
#define MCP2515_TXBnSIDL_EXIDE_MASK      0b00001000 // R/W-x
#define MCP2515_TXBnSIDL_EID_MASK        0b00000011 // R/W-xx
#define MCP2515_TXBnSIDL_SID_OFFSET      5
#define MCP2515_TXBnSIDL_EXIDE_OFFSET    3
#define MCP2515_TXBnSIDL_EID_OFFSET      0

// TXBnEID8 fields
#define MCP2515_TXBnEID8_EID_MASK    0b11111111 // R/W-xxxxxxxx
#define MCP2515_TXBnEID8_EID_OFFSET  0

// TXBnEID0 fields
#define MCP2515_TXBnEID0_EID_MASK    0b11111111 // R/W-xxxxxxxx
#define MCP2515_TXBnEID0_EID_OFFSET  0

// TXBnDLC fields
#define MCP2515_TXBnDLC_RTR_MASK     0b01000000 // R/W-x
#define MCP2515_TXBnDLC_DLC_MASK     0b00001111 // R/W-xxxx
#define MCP2515_TXBnDLC_RTR_OFFSET   6
#define MCP2515_TXBnDLC_DLC_OFFSET   0

// TXBnDm fields (Data Bytes)
#define MCP2515_TXBnDm_DATA_MASK     0b11111111 // R/W-xxxxxxxx
#define MCP2515_TXBnDm_DATA_OFFSET   0

// RXBnCTRL fields
#define MCP2515_RXBnCTRL_RXM_MASK       0b01100000  // R/W-00
#define MCP2515_RXBnCTRL_RXRTR_MASK     0b00001000  // R-0
#define MCP2515_RXBnCTRL_RXM_OFFSET     5
#define MCP2515_RXBnCTRL_RXRTR_OFFSET   3

typedef enum {
    MCP2515_RXBnCTRL_RXM_STANDARD_OR_EXTENDED    = 0,
    MCP2515_RXBnCTRL_RXM_STANDARD_ONLY           = 1,
    MCP2515_RXBnCTRL_RXM_EXTENDED_ONLY           = 2,
    MCP2515_RXBnCTRL_RXM_NO_FILTER               = 3
} mcp2515_rxbnctrl_rxm_t;

// RXB0CTRL fields
#define MCP2515_RXB0CTRL_RXM_MASK       0b01100000  // R/W-0
#define MCP2515_RXB0CTRL_RXRTR_MASK     0b00001000  // R-0
#define MCP2515_RXB0CTRL_BUKT_MASK      0b00000100  // R/W-0
#define MCP2515_RXB0CTRL_BUKT1_MASK     0b00000010  // R-0
#define MCP2515_RXB0CTRL_FILHIT0_MASK   0b00000001  // R-0
#define MCP2515_RXB0CTRL_RXM_OFFSET     5
#define MCP2515_RXB0CTRL_RXRTR_OFFSET   3
#define MCP2515_RXB0CTRL_BUKT_OFFSET    2
#define MCP2515_RXB0CTRL_BUKT1_OFFSET   1
#define MCP2515_RXB0CTRL_FILHIT0_OFFSET 0

#define MCP2515_RXB1CTRL_RXM_MASK       0b01100000  // R/W-0
#define MCP2515_RXB1CTRL_RXRTR_MASK     0b00001000  // R-0
#define MCP2515_RXB1CTRL_FILHIT_MASK    0b00000111  // R-0
#define MCP2515_RXB1CTRL_RXM_OFFSET     5
#define MCP2515_RXB1CTRL_RXRTR_OFFSET   3
#define MCP2515_RXB1CTRL_FILHIT_OFFSET  0


// CNF1 fields
#define MCP2515_CNF1_SJW_MASK           0b11000000  // R/W-00
#define MCP2515_CNF1_BRP_MASK           0b00111111  // R/W-000000
#define MCP2515_CNF1_SJW_OFFSET         6
#define MCP2515_CNF1_BRP_OFFSET         0

// CNF2 fields
#define MCP2515_CNF2_BTLMODE_MASK       0b10000000  // R/W-0
#define MCP2515_CNF2_SAM_MASK           0b01000000  // R/W-0
#define MCP2515_CNF2_PHSEG1_MASK        0b00111000  // R/W-000
#define MCP2515_CNF2_PRSEG_MASK         0b00000111  // R/W-000
#define MCP2515_CNF2_BTLMODE_OFFSET     7
#define MCP2515_CNF2_SAM_OFFSET         6
#define MCP2515_CNF2_PHSEG1_OFFSET      3
#define MCP2515_CNF2_PRSEG_OFFSET       0

// CNF3 fields
#define MCP2515_CNF3_SOF_MASK           0b10000000  // R/W-0
#define MCP2515_CNF3_WAKFIL_MASK        0b01000000  // R/W-0
#define MCP2515_CNF3_PHSEG2_MASK        0b00000111  // R/W-0
#define MCP2515_CNF3_SOF_OFFSET         7
#define MCP2515_CNF3_WAKFIL_OFFSET      6
#define MCP2515_CNF3_PHSEG2_OFFSET      0

// CANINTE fields
#define MCP2515_CANINTE_MERRE_MASK      0b10000000 // R/W-0
#define MCP2515_CANINTE_WAKIE_MASK      0b01000000 // R/W-0
#define MCP2515_CANINTE_ERRIE_MASK      0b00100000 // R/W-0
#define MCP2515_CANINTE_TX2IE_MASK      0b00010000 // R/W-0
#define MCP2515_CANINTE_TX1IE_MASK      0b00001000 // R/W-0
#define MCP2515_CANINTE_TX0IE_MASK      0b00000100 // R/W-0
#define MCP2515_CANINTE_RX1IE_MASK      0b00000010 // R/W-0
#define MCP2515_CANINTE_RX0IE_MASK      0b00000001 // R/W-0
#define MCP2515_CANINTE_MERRE_OFFSET    7
#define MCP2515_CANINTE_WAKIE_OFFSET    6
#define MCP2515_CANINTE_ERRIE_OFFSET    5
#define MCP2515_CANINTE_TX2IE_OFFSET    4
#define MCP2515_CANINTE_TX1IE_OFFSET    3
#define MCP2515_CANINTE_TX0IE_OFFSET    2
#define MCP2515_CANINTE_RX1IE_OFFSET    1
#define MCP2515_CANINTE_RX0IE_OFFSET    0

// CANINTF fields
#define MCP2515_CANINTF_TXnIF_MASK(n)   (MCP2515_CANINTF_TX0IF_MASK << (n))
#define MCP2515_CANINTF_TXnIF_OFFSET(n)   (MCP2515_CANINTF_TX0IF_OFFSET + (n))
#define MCP2515_CANINTF_RXnIF_MASK(n)   (MCP2515_CANINTF_RX0IF_MASK << (n))
#define MCP2515_CANINTF_RXnIF_OFFSET(n)   (MCP2515_CANINTF_RX0IF_OFFSET + (n))
#define MCP2515_CANINTF_MERRF_MASK      0b10000000 // R/W-0
#define MCP2515_CANINTF_WAKIF_MASK      0b01000000 // R/W-0
#define MCP2515_CANINTF_ERRIF_MASK      0b00100000 // R/W-0
#define MCP2515_CANINTF_TX2IF_MASK      0b00010000 // R/W-0
#define MCP2515_CANINTF_TX1IF_MASK      0b00001000 // R/W-0
#define MCP2515_CANINTF_TX0IF_MASK      0b00000100 // R/W-0
#define MCP2515_CANINTF_RX1IF_MASK      0b00000010 // R/W-0
#define MCP2515_CANINTF_RX0IF_MASK      0b00000001 // R/W-0
#define MCP2515_CANINTF_MERRF_OFFSET    7
#define MCP2515_CANINTF_WAKIF_OFFSET    6
#define MCP2515_CANINTF_ERRIF_OFFSET    5
#define MCP2515_CANINTF_TX2IF_OFFSET    4
#define MCP2515_CANINTF_TX1IF_OFFSET    3
#define MCP2515_CANINTF_TX0IF_OFFSET    2
#define MCP2515_CANINTF_RX1IF_OFFSET    1
#define MCP2515_CANINTF_RX0IF_OFFSET    0

// RXFnSIDH fields
#define MCP2515_RXFnSIDH_SID3_10_MASK   0b11111111 // R/W-xxxxxxxx
#define MCP2515_RXFnSIDH_SID3_10_OFFSET 0

// RXFnSIDL fields
#define MCP2515_RXFnSIDL_SID0_2_MASK            0b11100000 // R/W-xxx
#define MCP2515_RXFnSIDL_EXIDE_MASK             0b00001000 // R/W-x
#define MCP2515_RXFnSIDL_EID16_17_MASK          0b00000011 // R/W-xx
#define MCP2515_RXFnSIDL_SID0_2_OFFSET          5
#define MCP2515_RXFnSIDL_EXIDE_OFFSET           3
#define MCP2515_RXFnSIDL_EID16_17_OFFSET        0

// RXFnEID8 fields
#define MCP2515_RXFnEID8_15_MASK        0b11111111 // R/W-xxxxxxxx
#define MCP2515_RXFnEID8_15_OFFSET      0

// RXFnEID0 fields
#define MCP2515_RXFnEID0_7_MASK        0b11111111 // R/W-xxxxxxxx
#define MCP2515_RXFnEID0_7_OFFSET      0

// EFLG fields
#define MCP2515_EFLG_RX1OVR_MASK        0b10000000 // R/W-0
#define MCP2515_EFLG_RX0OVR_MASK        0b01000000 // R/W-0
#define MCP2515_EFLG_TXBO_MASK          0b00100000 // R-0
#define MCP2515_EFLG_TXEP_MASK          0b00010000 // R-0
#define MCP2515_EFLG_RXEP_MASK          0b00001000 // R-0
#define MCP2515_EFLG_TXWAR_MASK         0b00000100 // R-0
#define MCP2515_EFLG_RXWAR_MASK         0b00000010 // R-0
#define MCP2515_EFLG_EWARN_MASK         0b00000001 // R-0
#define MCP2515_EFLG_RX1OVR_OFFSET      7
#define MCP2515_EFLG_RX0OVR_OFFSET      6
#define MCP2515_EFLG_TXBO_OFFSET        5
#define MCP2515_EFLG_TXEP_OFFSET        4
#define MCP2515_EFLG_RXEP_OFFSET        3
#define MCP2515_EFLG_TXWAR_OFFSET       2
#define MCP2515_EFLG_RXWAR_OFFSET       1
#define MCP2515_EFLG_EWARN_OFFSET       0

// RXBnSIDH fields
#define MCP2515_RXBnSIDH_SID3_10_MASK   0b11111111 // R-xxxxxxxx
#define MCP2515_RXBnSIDH_SID3_10_OFFSET 0

// RXBnSIDL fields
#define MCP2515_RXBnSIDL_SID0_2_MASK            0b11100000 // R-xxx
#define MCP2515_RXBnSIDL_SRR_MASK               0b00010000 // R-x
#define MCP2515_RXBnSIDL_IDE_MASK               0b00001000 // R-x
#define MCP2515_RXBnSIDL_EID16_17_MASK          0b00000011 // R-xx
#define MCP2515_RXBnSIDL_SID0_2_OFFSET          5
#define MCP2515_RXBnSIDL_SRR_OFFSET             4
#define MCP2515_RXBnSIDL_IDE_OFFSET             3
#define MCP2515_RXBnSIDL_EID16_17_OFFSET        0

// RXBnEID8 fields
#define MCP2515_RXBnEID8_15_MASK        0b11111111 // R-xxxxxxxx
#define MCP2515_RXBnEID8_15_OFFSET      0

// RXBnEID0 fields
#define MCP2515_RXBnEID0_7_MASK         0b11111111 // R-xxxxxxxx
#define MCP2515_RXBnEID0_7_OFFSET       0

// RXBnDLC fields
#define MCP2515_RXBnDLC_RTR_MASK        0b01000000 // R-x
#define MCP2515_RXBnDLC_DLC_MASK        0b00001111 // R-xxxx
#define MCP2515_RXBnDLC_RTR_OFFSET      6
#define MCP2515_RXBnDLC_DLC_OFFSET      0
/**@}*/

/***********************************
 * SPI Commands
 **********************************/
/**
 * The commands/instructions that can be issued to the MCP2515 via SPI.
 */
/**@{*/
#define MCP2515_SPI_CMD_RESET                   0xC0
#define MCP2515_SPI_CMD_READ                    0x03
#define MCP2515_SPI_CMD_READ_RX_BUFFERn_ALL(n)  (0x90 | ((n) << 2))
#define MCP2515_SPI_CMD_READ_RX_BUFFERn_DATA(n) (0x92 | ((n) << 2))
#define MCP2515_SPI_CMD_WRITE                   0x02
#define MCP2515_SPI_CMD_LOAD_TX_BUFFERn_ALL(n)  (0x40 | ((n) << 1))
#define MCP2515_SPI_CMD_LOAD_TX_BUFFERn_DATA(n) (0x41 | ((n) << 1))
#define MCP2515_SPI_CMD_RTS                     0x80
#define MCP2515_SPI_CMD_READ_STATUS             0xA0
#define MCP2515_SPI_CMD_RX_STATUS               0xB0
#define MCP2515_SPI_CMD_BIT_MODIFY              0x05
/**@}*/

/**
 * The absolute maximum number of bytes that can be read or written
 * to/from the MCP2515 in one SPI request. Essentially reading/writing
 * the entire register map (128 registers)
 */
#define MCP2515_MAX_COMM_PAYLOAD 128

/**
 * The max number of bytes that are read by a READ_RX_BUFFERn_ALL or
 * READ_RX_BUFFERn_DATA instruction.
 */
#define MCP2515_MAX_READ_RX_BUFFER_PAYLOAD 13
/**
 * The max number of bytes that are read by a READ_RX_BUFFERn_ALL instruction
 */
#define MCP2515_MAX_READ_RX_BUFFER_ALL_PAYLOAD 13
/**
 * The max number of bytes that are read by a READ_RX_BUFFERn_DATA instruction
 */
#define MCP2515_MAX_READ_RX_BUFFER_DATA_PAYLOAD 8

/**
 * The max number of bytes that are written by a LOAD_TX_BUFFERn_ALL or
 * LOAD_TX_BUFFERn_DATA instruction.
 */
#define MCP2515_MAX_LOAD_TX_BUFFER_PAYLOAD 13
/**
 * The max number of bytes that are written by a LOAD_TX_BUFFERn_ALL instruction
 */
#define MCP2515_MAX_LOAD_TX_BUFFER_ALL_PAYLOAD 13
/**
 * The max number of bytes that are written by a
 * LOAD_TX_BUFFERn_DATA instruction
 */
#define MCP2515_MAX_LOAD_TX_BUFFER_DATA_PAYLOAD 8

/** The number of HW transmit buffers provided by the MCP2515 */
#define MCP2515_NUM_TX_BUFFERS 3

/** The number of HW receive buffers provided by the MCP2515 */
#define MCP2515_NUM_RX_BUFFERS 2

/** The maximum transmit priority that can be set on a HW TX buffer.
 *
 * Valid priorities range from 0 to MCP2515_CANMCF_TPL_MAXVAL
 */
#define MCP2515_CANMCF_TPL_MAXVAL 3

/** Minimum bit clock frequency required by the MCP2515 */
#define MCP2515_MIN_CLOCK_FREQUENCY 1000000
/** Maximum bit clock frequency supported by the MCP2515
 *
 * @note The actual frequency depends on the input voltage.
 *       This value is the larger of the two.
 */
#define MCP2515_MAX_CLOCK_FREQUENCY 40000000

// Encodes a MID into a form compatible with MCP2515's message ID registers.
// These registers are (in order)
// (TXBnSIDH, TXBnSIDL, TXBnEID8, TXBnEID0) or
// (RXFnSIDH, RXFnSIDL, RXFnEID8, RXFnEID0) or
// (RXMnSIDH, RXMnSIDL, RXMnEID8, RXMnEID0) or
// If a SID is being encoded the EID specific bits are set to 0
//
// regs must point to a buffer at least 4 bytes in length.
// regs[0] == *SIDH, regs[1] == *SIDL, regs[2] == *EID8, regs[3] == *EID0
/**
 * Encodes a MID into a form compatible with the MCP2515's registers.
 *
 * The registers that are compatible with this API are:
 * (TXBnSIDH, TXBnSIDL, TXBnEID8, TXBnEID0) or
 * (RXFnSIDH, RXFnSIDL, RXFnEID8, RXFnEID0) or
 * (RXMnSIDH, RXMnSIDL, RXMnEID8, RXMnEID0)
 *
 * @note The output bytes are generated in the same order as the registers
 *       listed above.
 *
 * If a SID is being encoded the EID specific bits of the register are set to 0
 *
 * @param[in]   mid     The MID to encode
 * @param[in]   isEid   True (non-zero) if the MID is an EID.
 *                      False (zero) if the MID is a SID.
 * @param[out]  regs    A 4 byte buffer that will hold the encoded version of
 *                      the MID. The order of bytes is the same order as the
 *                      registers listed above.
 *                      Ie: regs[0] = *SIDH
 *                          regs[1] = *SIDL
 *                          regs[2] = *EID8
 *                          regs[3] = *EID0
 */
void mcp2515_encode_mid(uint32_t mid, int isEid, uint8_t* regs);

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
/**
 * Decodes a MID from the form used by the MCP2515's registers.
 *
 * The registers that are compatible with this API are:
 * (TXBnSIDH, TXBnSIDL, TXBnEID8, TXBnEID0) or
 * (RXFnSIDH, RXFnSIDL, RXFnEID8, RXFnEID0) or
 * (RXMnSIDH, RXMnSIDL, RXMnEID8, RXMnEID0)
 *
 * @note The input bytes are assumed to be in the same order as the registers
 *       listed above.
 *
 * If a SID is being encoded the EID specific bits of the register are set to 0
 *
 * @param[in]   regs    A 4 byte buffer that contains the encoded version of
 *                      the MID. The order of bytes is the same order as the
 *                      registers listed above.
 *                      Ie: regs[0] = *SIDH
 *                          regs[1] = *SIDL
 *                          regs[2] = *EID8
 *                          regs[3] = *EID0
 * @param[out]  mid     The decoded MID.
 *                      A SID occupies bits [18:28] of the MID.
 *                      An EID occupies bits [0:28] of the MID.
 * @param[out]  isEid   True (non-zero) if the MID is an EID.
 *                      False (zero) if the MID is a SID.
 */
void mcp2515_decode_mid(uint8_t* regs, uint32_t* mid, int *isEid);

#endif  // LIB_PUBLIC_MCP2515_MCP2515_H_
