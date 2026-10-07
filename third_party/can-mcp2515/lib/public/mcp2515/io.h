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

#ifndef LIB_PUBLIC_MCP2515_GS_USB_IO_H_
#define LIB_PUBLIC_MCP2515_GS_USB_IO_H_

/**
 * @file
 *
 * APIs exposing the underlying SPI protocol the MCP2515 uses.
 */

#include <stdint.h>

#include <hw/io-spi.h>

#include "mcp2515.h"
#include "state.h"

/* ********************************************************************
 * Convenience macros for mcp2515_io_bit_modify
 *
 * Used to easily set a bitfield via the mcp2515_io_bit_modify call.
 *
 * The 'bf' value should be one of the bit field names
 * defined in mcp2515.h WITHOUT the 'MCP2515_' prefix
 * nor the '_MASK' or _OFFSET suffixes.
 * *******************************************************************/
/**
 * Sets a bitfield within one of the MCP2515 registers using the MCP2515's
 * Bit Modify instruction.
 *
 * @param[in]   state   The state context for the MCP2515
 * @param[in]   reg     The address of the register to modify.
 * @param[in]   bf      The name of the bitfield. This will be used to reference
 *                      the MCP2515_<bf>_MASK & MCP2515_<bf>_OFFSET values from
 *                      mcp2515.h
 * @param[in]   value   The unshifted value to write into the bitfield.
 *
 * @return The result from calling mcp2515_io_bit_modify(). See that function
 *         for details on the return value.
 */
#define MCP2515_IO_SET_BITFIELD(state, reg, bf, value) \
    (mcp2515_io_bit_modify(state, reg, MCP2515_##bf##_MASK, (value) << MCP2515_##bf##_OFFSET))

/**
 * Sets a single bit within one of the MCP2515 registers using the MCP2515's
 * Bit Modify instruction.
 *
 * @param[in]   state   The state context for the MCP2515
 * @param[in]   reg     The address of the register to modify.
 * @param[in]   bf      The name of the bitfield. This will be used to reference
 *                      the MCP2515_<bf>_MASK value from mcp2515.h
 *
 * @return The result from calling mcp2515_io_bit_modify(). See that function
 *         for details on the return value.
 */
#define MCP2515_IO_SET_BIT(state, reg, bf) \
    (mcp2515_io_bit_modify(state, reg, MCP2515_##bf##_MASK, MCP2515_##bf##_MASK))

/**
 * Clears a single bit within one of the MCP2515 registers using the MCP2515's
 * Bit Modify instruction.
 *
 * @param[in]   state   The state context for the MCP2515
 * @param[in]   reg     The address of the register to modify.
 * @param[in]   bf      The name of the bitfield. This will be used to reference
 *                      the MCP2515_<bf>_MASK value from mcp2515.h
 *
 * @return The result from calling mcp2515_io_bit_modify(). See that function
 *         for details on the return value.
 */
#define MCP2515_IO_CLEAR_BIT(state, reg, bf) \
    (mcp2515_io_bit_modify(state, reg, MCP2515_##bf##_MASK, 0))

/**
 * Structure defining the data sent for the MCP2515's Read instruction.
 *
 * The members of this structure are filled in by the mcp2515_io_read() API.
 */
struct mcp2515_io_read_packet {
    /** The common SPI header required by the QNX SPI devctl interface. */
    spi_xchng_t spiHdr;
    /** The command/instruction byte for the MCP2515 SPI packet. */
    uint8_t cmd;
    /** The address of the register to read */
    uint8_t addr;
    /** The buffer to place the bytes read from the MCP2515 */
    uint8_t data[MCP2515_MAX_COMM_PAYLOAD];
} __attribute__((packed));
typedef struct mcp2515_io_read_packet mcp2515_io_read_packet_t;

/**
 * Structure defining the data sent for the MCP2515's Read RX Buffer instruction
 *
 * The members of this structure are filled in by the
 * mcp2515_io_read_rx_buffer() API.
 */
struct mcp2515_io_read_rx_buffer_packet {
    /** The common SPI header required by the QNX SPI devctl interface */
    spi_xchng_t spiHdr;
    /** The command/instruction byte for the MCP2515 SPI packet. */
    uint8_t cmd;
    /** The buffer to place the bytes read from the MCP2515 */
    uint8_t data[MCP2515_MAX_READ_RX_BUFFER_PAYLOAD];
} __attribute__((packed));
typedef struct mcp2515_io_read_rx_buffer_packet mcp2515_io_read_rx_buffer_packet_t;

/**
 * Structure defining the data sent for the MCP2515's Byte Write instruction.
 *
 * The members of this structure are filled in by the mcp2515_io_write() API.
 */
struct mcp2515_io_write_packet {
    /** The common SPI header required by the QNX SPI devctl interface */
    spi_xchng_t spiHdr;
    /** The command/instruction byte for the MCP2515 SPI packet. */
    uint8_t cmd;
    /** The address of the register to read */
    uint8_t addr;
    /** The buffer containing bytes to write to the MCP2515 */
    uint8_t data[MCP2515_MAX_COMM_PAYLOAD];
} __attribute__((packed));
typedef struct mcp2515_io_write_packet mcp2515_io_write_packet_t;

/**
 * Structure defining the data sent for the MCP2515's Load TX Buffer instruction.
 *
 * The members of this structure are filled in by the
 * mcp2515_io_load_tx_buffer() API.
 */
struct mcp2515_io_load_tx_buffer_packet {
    /** The common SPI header required by the QNX SPI devctl interface */
    spi_xchng_t spiHdr;
    /** The command/instruction byte for the MCP2515 SPI packet. */
    uint8_t cmd;
    /** The buffer containing the bytes of the CAN frame to load */
    uint8_t data[MCP2515_MAX_LOAD_TX_BUFFER_PAYLOAD];
} __attribute__((packed));
typedef struct mcp2515_io_load_tx_buffer_packet mcp2515_io_load_tx_buffer_packet_t;

/**
 * Structure storing the results of the MCP2515's Read Status instruction.
 *
 * The members of this structure are filled in by the
 * mcp2515_io_read_status() API.
 */
struct mcp2515_io_read_status_result {
    /** Indicates whether an RX buffer is full (non-zero) or empty (zero). */
    int rxFull[MCP2515_NUM_RX_BUFFERS];
    /** Indicates whether a TX buffer is empty (non-zero) or full (zero). */
    int txEmpty[MCP2515_NUM_TX_BUFFERS];
    /** Indicates whether transmission from a TX buffer is pending (non-zero)
     *  or not (zero). */
    int txPending[MCP2515_NUM_TX_BUFFERS];
};
typedef struct mcp2515_io_read_status_result mcp2515_io_read_status_result_t;

/**
 * Structure storing the results of the MCP2515's Rx Status instruction.
 *
 * The members of this structure are filled in by the
 * mcp2515_io_rx_status() API.
 */
struct mcp2515_io_rx_status_result {
    int hasFrame;       ///< If FALSE (0) none of the other members are valid
    int bufferIdx;      ///< Index of RxBuffer that has a frame.
    int eid;            ///< If TRUE (non-zero) frame has an extended ID. Otherwise standard ID
    int rtr;            ///< If TRUE (non-zero) frame is an remote-tranmission-request
    int filterIdx;      ///< The index of the filter that matched the frame
};
typedef struct mcp2515_io_rx_status_result mcp2515_io_rx_status_result_t;

/**
 * Sends a Reset instruction to the MCP2515
 *
 * @note This API only sends the instruction. It does not wait for the RESET
 *       to complete.
 *
 * @param[in]   state   The state context for the MCP2515
 *
 * @return EOK(0) if the instruction was successfully sent, one of the other
 *         errno codes if there is a failure during communication.
 */
int mcp2515_io_reset(mcp2515_state_t* state);

/**
 * Sends a Read instruction to the MCP2515
 *
 * @note This API sends the instruction and reads back whatever data is
 *       available on the SPI link. It is unable to determine if the MCP2515
 *       actually responded to the instruction or not.
 *
 * @param[in]   state   The state context for the MCP2515
 * @param[in]   addr    The address of the register to start reading from
 * @param[in]   numBytes The number of bytes to read. Bytes are read from
 *                       sequential registers starting at the one specified
 *                       by @ref addr.
 * @param[out]  pkt     Caller allocated struct to use for the SPI transaction.
 *                      The bytes read from the MCP2515 will be in the pkt's
 *                      data member on return.
 *
 * @return EOK(0) if the instruction was successfully sent, one of the other
 *         errno codes if there is a failure during communication.
 */
int mcp2515_io_read(mcp2515_state_t* state, int addr, int numBytes, mcp2515_io_read_packet_t* pkt);

/**
 * Sends a Read RX Buffer instruction to the MCP2515
 *
 * @note This API sends the instruction and reads back whatever data is
 *       available on the SPI link. It is unable to determine if the MCP2515
 *       actually responded to the instruction or not.
 *
 * @param[in]   state   The state context for the MCP2515
 * @param[in]   bufferIdx The index of the RX buffer to read the CAN frame from.
 * @param[in]   dataOnly If true (non-zero) only the 8 data bytes of the CAN
 *                       frame are read from the RX buffer. If false (zero)
 *                       then the data AND header information of the CAN frame
 *                       is read from the RX buffer.
 * @param[out]  pkt     Caller allocated struct to use for the SPI transaction.
 *                      The bytes read from the MCP2515 will be in the pkt's
 *                      data member on return.
 *
 * @return EOK(0) if the instruction was successfully sent, one of the other
 *         errno codes if there is a failure during communication.
 */
int mcp2515_io_read_rx_buffer(mcp2515_state_t* state, int bufferIdx,
                              int dataOnly, mcp2515_io_read_rx_buffer_packet_t* pkt);

/**
 * Sends a Write instruction to the MCP2515
 *
 * @param[in]   state   The state context for the MCP2515
 * @param[in]   addr    The address of the register to start writing to
 * @param[in]   numBytes The number of bytes to write. Bytes are written from
 *                       sequential registers starting at the one specified
 *                       by @ref addr.
 * @param[out]  pkt     Caller allocated struct to use for the SPI transaction.
 *                      The bytes written to the MCP2515 MUST be in the pkt's
 *                      data member when this API is called.
 *
 * @return EOK(0) if the instruction was successfully sent, one of the other
 *         errno codes if there is a failure during communication.
 */
int mcp2515_io_write(mcp2515_state_t* state, int addr, int numBytes, mcp2515_io_write_packet_t* pkt);

/**
 * Sends a Load TX Buffer instruction to the MCP2515
 *
 * @param[in]   state   The state context for the MCP2515
 * @param[in]   bufferIdx The index of the TX buffer to load the CAN frame into.
 * @param[in]   dataOnly If true (non-zero) only the 8 data bytes of the CAN
 *                       frame are read from the RX buffer. If false (zero)
 *                       then the data AND header information of the CAN frame
 *                       is read from the RX buffer.
 * @param[out[  pkt     Caller allocated struct to use for the SPI transaction.
 *                      The bytes read from the MCP2515 will be in the pkt's
 *                      data member on return.
 *
 * @return EOK(0) if the instruction was successfully sent, one of the other
 *         errno codes if there is a failure during communication.
 */
int mcp2515_io_load_tx_buffer(mcp2515_state_t* state, int bufferIdx, int dataOnly, mcp2515_io_load_tx_buffer_packet_t* pkt);

/**
 * Sends an RTS (Request-To-Send) instruction to the MCP2515
 *
 * @param[in]   state   The state context for the MCP2515
 * @param[in]   tx0     If true (non-zero) request transmission from TX buffer 0
 * @param[in]   tx1     If true (non-zero) request transmission from TX buffer 1
 * @param[in]   tx2     If true (non-zero) request transmission from TX buffer 2
 *
 * @return EOK(0) if the instruction was successfully sent, one of the other
 *         errno codes if there is a failure during communication.
 */
int mcp2515_io_rts(mcp2515_state_t* state, int tx0, int tx1, int tx2);

/**
 * Sends a Read Status instruction to the MCP2515
 *
 * @note This API sends the instruction and reads back whatever data is
 *       available on the SPI link. It is unable to determine if the MCP2515
 *       actually responded to the instruction or not.
 *
 * @param[in]   state   The state context for the MCP2515
 * @param[out]  status  The buffer to place the status read from the MCP2515.
 *
 * @return EOK(0) if the instruction was successfully sent, one of the other
 *         errno codes if there is a failure during communication.
 */
int mcp2515_io_read_status(mcp2515_state_t* state, mcp2515_io_read_status_result_t* status);

/**
 * Sends an RX Status instruction to the MCP2515
 *
 * @note This API sends the instruction and reads back whatever data is
 *       available on the SPI link. It is unable to determine if the MCP2515
 *       actually responded to the instruction or not.
 *
 * @param[in]   state   The state context for the MCP2515
 * @param[out]  status  The buffer to place the status read from the MCP2515.
 *
 * @return EOK(0) if the instruction was successfully sent, one of the other
 *         errno codes if there is a failure during communication.
 */
int mcp2515_io_rx_status(mcp2515_state_t* state, mcp2515_io_rx_status_result_t* status);

/**
 * Sends a Bit Modify instruction to the MCP2515
 *
 * @param[in]   state   The state context for the MCP2515
 * @param[in]   addr    The address of the register to modify
 * @param[in]   mask    The mask to identify what bits are modified. A bit that
 *                      is set in the mask is modified in the register.
 * @param[in]   data    The new bit values for the register. Only bits that
 *                      are set in the mask are changed in the register. Thus
 *                      only the bits in data that are also set in the mask
 *                      are used.
 *
 * @return EOK(0) if the instruction was successfully sent, one of the other
 *         errno codes if there is a failure during communication.
 */
int mcp2515_io_bit_modify(mcp2515_state_t* state, uint8_t addr, uint8_t mask, uint8_t data);

/**
 * A convenience function to write a single byte to the MCP2515.
 *
 * Usually used when updating a single register.
 *
 * @param[in]   state   The state context for the MCP2515
 * @param[in]   addr    The address of the register to write to
 * @param[in]   byte    The data to write to the register.
 *
 * @return EOK(0) if the instruction was successfully sent, one of the other
 *         errno codes if there is a failure during communication.
 */
int mcp2515_io_write_byte(mcp2515_state_t* state, int addr, uint8_t byte);

/**
 * A convenience function to read a single byte from the MCP2515.
 *
 * Usually used when trying to read a single register.
 *
 * @note This API sends the instruction and reads back whatever data is
 *       available on the SPI link. It is unable to determine if the MCP2515
 *       actually responded to the instruction or not.
 *
 * @param[in]   state   The state context for the MCP2515
 * @param[in]   addr    The address of the register to read from
 *
 * @return The (unsigned) byte of data read from the register.
 *         If there is a failure during communication, the negative of an
 *         errno is returned instead. Thus a value >=0 indicates success
 *         while a negative value indicates failure.
 */
int mcp2515_io_read_byte(mcp2515_state_t* state, int addr);

#endif  // LIB_PUBLIC_MCP2515_GS_USB_IO_H_
