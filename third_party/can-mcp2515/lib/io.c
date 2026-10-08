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

#include <mcp2515/io.h>
#include <mcp2515/logging.h>
#include <mcp2515/mcp2515.h>
#include <mcp2515/state.h>
#include <mcp2515/util.h>

// Additional structs that are used internally and not exposed
struct mcp2515_io_reset_packet {
    spi_xchng_t spiHdr;
    uint8_t cmd;
} __attribute__((packed));
typedef struct mcp2515_io_reset_packet mcp2515_io_reset_packet_t;

struct mcp2515_io_rts_packet {
    spi_xchng_t spiHdr;
    uint8_t cmd;
} __attribute__((packed));
typedef struct mcp2515_io_rts_packet mcp2515_io_rts_packet_t;

struct mcp2515_io_read_status_packet {
    spi_xchng_t spiHdr;
    uint8_t cmd;
    uint8_t status;
} __attribute__((packed));
typedef struct mcp2515_io_read_status_packet mcp2515_io_read_status_packet_t;

struct mcp2515_io_rx_status_packet {
    spi_xchng_t spiHdr;
    uint8_t cmd;
    uint8_t status;
} __attribute__((packed));
typedef struct mcp2515_io_rx_status_packet mcp2515_io_rx_status_packet_t;

struct mcp2515_io_bit_modify_packet {
    spi_xchng_t spiHdr;
    uint8_t cmd;
    uint8_t addr;
    uint8_t mask;
    uint8_t data;
} __attribute__((packed));
typedef struct mcp2515_io_bit_modify_packet mcp2515_io_bit_modify_packet_t;

int mcp2515_io_reset(mcp2515_state_t* state) {
    int rc;
    mcp2515_io_reset_packet_t pkt;
    char errStr[ERR_STR_SIZE];

    assert(state && "state is NULL");

    pkt.spiHdr.nbytes = 1;
    pkt.cmd = MCP2515_SPI_CMD_RESET;

    LOGN("mcp2515@%d: Reset()", state->fd);
    rc = devctl(state->fd, DCMD_SPI_DATA_XCHNG, &pkt.spiHdr, sizeof(pkt), NULL);
    if (rc) {
        LOGE("mcp2515@%d: Reset() FAILED! Error=%s", state->fd, sstrerror(rc, errStr, sizeof(errStr)));
        return rc;
    }

    return EOK;
}

int mcp2515_io_read(mcp2515_state_t* state, int addr, int numBytes, mcp2515_io_read_packet_t* pkt) {
    int rc;
    char errStr[ERR_STR_SIZE];

    assert(state && "state is NULL");
    assert(pkt && "pkt is NULL");
    assert(numBytes >= 0 && numBytes < MCP2515_MAX_COMM_PAYLOAD && "numBytes out of range");

    pkt->spiHdr.nbytes = 2 + numBytes;
    pkt->cmd = MCP2515_SPI_CMD_READ;
    pkt->addr = addr;

    LOGD2("mcp2515@%d: Read(addr=0x%02x, numBytes=%d)", state->fd, addr, numBytes);
    rc = devctl(state->fd, DCMD_SPI_DATA_XCHNG, &pkt->spiHdr, sizeof(pkt->spiHdr) + pkt->spiHdr.nbytes, NULL);
    if (rc) {
        LOGE("mcp2515@%d: Read(addr=0x%02x, numBytes=%d) FAILED! Error=%s",
             state->fd, addr, numBytes, sstrerror(rc, errStr, sizeof(errStr)));
        return rc;
    }

    return EOK;
}

int mcp2515_io_read_rx_buffer(mcp2515_state_t* state, int bufferIdx, int dataOnly, mcp2515_io_read_rx_buffer_packet_t* pkt) {
    int rc;
    char errStr[ERR_STR_SIZE];

    assert(state && "state is NULL");
    assert(pkt && "pkt is NULL");
    assert(bufferIdx >= 0 && bufferIdx < MCP2515_NUM_RX_BUFFERS && "bufferIdx out of range");

    pkt->spiHdr.nbytes = 2;
    if (dataOnly) {
        pkt->spiHdr.nbytes += MCP2515_MAX_READ_RX_BUFFER_DATA_PAYLOAD;
        pkt->cmd = MCP2515_SPI_CMD_READ_RX_BUFFERn_DATA(bufferIdx);
    } else {
        pkt->spiHdr.nbytes += MCP2515_MAX_READ_RX_BUFFER_ALL_PAYLOAD;
        pkt->cmd = MCP2515_SPI_CMD_READ_RX_BUFFERn_ALL(bufferIdx);
    }

    LOGD2("mcp2515@%d: ReadRxBuffer(bufferIdx=%d, dataOnly=%d)", state->fd, bufferIdx, dataOnly);
    rc = devctl(state->fd, DCMD_SPI_DATA_XCHNG, &pkt->spiHdr, sizeof(pkt->spiHdr) + pkt->spiHdr.nbytes, NULL);
    if (rc) {
        LOGE("mcp2515@%d: ReadRxBuffer(bufferIdx=%d, dataOnly=%d) FAILED! Error=%s",
             state->fd, bufferIdx, dataOnly, sstrerror(rc, errStr, sizeof(errStr)));
        return rc;
    }

    return EOK;
}

int mcp2515_io_write(mcp2515_state_t* state, int addr, int numBytes, mcp2515_io_write_packet_t* pkt) {
    int rc;
    char errStr[ERR_STR_SIZE];

    assert(state && "state is NULL");
    assert(pkt && "pkt is NULL");
    assert(numBytes >= 0 && numBytes < MCP2515_MAX_COMM_PAYLOAD && "numBytes out of range");

    pkt->spiHdr.nbytes = 2 + numBytes;
    pkt->cmd = MCP2515_SPI_CMD_WRITE;
    pkt->addr = addr;

    LOGD2("mcp2515@%d: Write(addr=0x%02x, numBytes=%d)", state->fd, addr, numBytes);
    rc = devctl(state->fd, DCMD_SPI_DATA_XCHNG, &pkt->spiHdr, sizeof(pkt->spiHdr) + pkt->spiHdr.nbytes, NULL);
    if (rc) {
        LOGE("mcp2515@%d: Write(addr=0x%02x, numBytes=%d) FAILED! Error=%s",
             state->fd, addr, numBytes, sstrerror(rc, errStr, sizeof(errStr)));
        return rc;
    }

    return EOK;
}

int mcp2515_io_load_tx_buffer(mcp2515_state_t* state, int bufferIdx, int dataOnly, mcp2515_io_load_tx_buffer_packet_t* pkt) {
    int rc;
    char errStr[ERR_STR_SIZE];

    assert(state && "state is NULL");
    assert(pkt && "pkt is NULL");
    assert(bufferIdx >= 0 && bufferIdx < MCP2515_NUM_TX_BUFFERS && "bufferIdx out of range");

    pkt->spiHdr.nbytes = 2;
    if (dataOnly) {
        pkt->spiHdr.nbytes += MCP2515_MAX_LOAD_TX_BUFFER_DATA_PAYLOAD;
        pkt->cmd = MCP2515_SPI_CMD_LOAD_TX_BUFFERn_DATA(bufferIdx);
    } else {
        pkt->spiHdr.nbytes += MCP2515_MAX_LOAD_TX_BUFFER_ALL_PAYLOAD;
        pkt->cmd = MCP2515_SPI_CMD_LOAD_TX_BUFFERn_ALL(bufferIdx);
    }

    LOGD2("mcp2515@%d: LoadTxBuffer(bufferIdx=%d, dataOnly=%d)", state->fd, bufferIdx, dataOnly);
    rc = devctl(state->fd, DCMD_SPI_DATA_XCHNG, &pkt->spiHdr, sizeof(pkt->spiHdr) + pkt->spiHdr.nbytes, NULL);
    if (rc) {
        LOGE("mcp2515@%d: LoadTxBuffer(bufferIdx=%d, dataOnly=%d) FAILED! Error=%s",
             state->fd, bufferIdx, dataOnly, sstrerror(rc, errStr, sizeof(errStr)));
        return rc;
    }

    return EOK;
}

int mcp2515_io_rts(mcp2515_state_t* state, int tx0, int tx1, int tx2) {
    int rc;
    mcp2515_io_rts_packet_t pkt;
    char errStr[ERR_STR_SIZE];

    assert(state && "state is NULL");

    pkt.spiHdr.nbytes = 1;
    pkt.cmd = MCP2515_SPI_CMD_RTS;
    if (tx0) pkt.cmd |= (1 << 0);
    if (tx1) pkt.cmd |= (1 << 1);
    if (tx2) pkt.cmd |= (1 << 2);

    LOGD2("mcp2515@%d: RTS(tx0=%d, tx1=%d, tx2=%d)", state->fd, tx0, tx1, tx2);
    rc = devctl(state->fd, DCMD_SPI_DATA_XCHNG, &pkt.spiHdr, sizeof(pkt), NULL);
    if (rc) {
        LOGE("mcp2515@%d: RTS(tx0=%d, tx1=%d, tx2=%d) FAILED! Error=%s",
             state->fd, tx0, tx1, tx2, sstrerror(rc, errStr, sizeof(errStr)));
        return rc;
    }

    return EOK;
}

int mcp2515_io_read_status(mcp2515_state_t* state, mcp2515_io_read_status_result_t* status) {
    int rc;
    mcp2515_io_read_status_packet_t pkt;
    char errStr[ERR_STR_SIZE];

    assert(state && "state is NULL");
    assert(status && "status is NULL");

    pkt.spiHdr.nbytes = 2;
    pkt.cmd = MCP2515_SPI_CMD_READ_STATUS;

    rc = devctl(state->fd, DCMD_SPI_DATA_XCHNG, &pkt.spiHdr, sizeof(pkt), NULL);
    if (rc) {
        LOGE("mcp2515@%d: ReadStatus() FAILED! Error=%s",
             state->fd, sstrerror(rc, errStr, sizeof(errStr)));
        return rc;
    }

    LOGD2("mcp2515@%d: ReadStatus() = 0x%02x", state->fd, pkt.status);

    // Decode the status
    memset(status, 0, sizeof(*status));
    status->rxFull[0] = (pkt.status & 0x1) ? 1 : 0;
    status->rxFull[1] = (pkt.status & 0x2) ? 1 : 0;
    status->txPending[0] = (pkt.status & 0x4) ? 1 : 0;
    status->txEmpty[0] = (pkt.status & 0x8) ? 1 : 0;
    status->txPending[1] = (pkt.status & 0x10) ? 1 : 0;
    status->txEmpty[1] = (pkt.status & 0x20) ? 1 : 0;
    status->txPending[2] = (pkt.status & 0x40) ? 1 : 0;
    status->txEmpty[2] = (pkt.status & 0x80) ? 1 : 0;

    return EOK;
}

int mcp2515_io_rx_status(mcp2515_state_t* state, mcp2515_io_rx_status_result_t* status) {
    int rc;
    mcp2515_io_rx_status_packet_t pkt;
    char errStr[ERR_STR_SIZE];

    assert(state && "state is NULL");
    assert(status && "status is NULL");

    pkt.spiHdr.nbytes = 2;
    pkt.cmd = MCP2515_SPI_CMD_RX_STATUS;

    rc = devctl(state->fd, DCMD_SPI_DATA_XCHNG, &pkt.spiHdr, sizeof(pkt), NULL);
    if (rc) {
        LOGE("mcp2515@%d: RxStatus() FAILED! Error=%s",
             state->fd, sstrerror(rc, errStr, sizeof(errStr)));
        return rc;
    }

    LOGD2("mcp2515@%d: RxStatus() = 0x%02x", state->fd, pkt.status);

    // Decode the status
    memset(status, 0, sizeof(*status));
    if (pkt.status & ((1 << 7) | (1 << 6))) {
        status->hasFrame = 1;
        status->bufferIdx = (pkt.status & (1 << 6)) ? 0 : 1;
        status->eid = (pkt.status & (1 << 4)) ? 1 : 0;
        status->rtr = (pkt.status & (1 << 3)) ? 1 : 0;
        status->filterIdx = pkt.status & 0x7;
    }

    return EOK;
}

int mcp2515_io_bit_modify(mcp2515_state_t* state, uint8_t addr, uint8_t mask, uint8_t data) {
    int rc;
    mcp2515_io_bit_modify_packet_t pkt;
    char errStr[ERR_STR_SIZE];

    assert(state && "state is NULL");

    pkt.spiHdr.nbytes = 4;
    pkt.cmd = MCP2515_SPI_CMD_BIT_MODIFY;
    pkt.addr = addr;
    pkt.mask = mask;
    pkt.data = data;

    LOGD2("mcp2515@%d: BitModify(addr=0x%02x, mask=0x%02x, data=0x%02x)", state->fd, addr, mask, data);
    rc = devctl(state->fd, DCMD_SPI_DATA_XCHNG, &pkt.spiHdr, sizeof(pkt), NULL);
    if (rc) {
        LOGE("mcp2515@%d: BitModify(addr=0x%02x, mask=0x%02x, data=0x%02x) FAILED! Error=%s",
             state->fd, addr, mask, data, sstrerror(rc, errStr, sizeof(errStr)));
        return rc;
    }

    return EOK;
}

int mcp2515_io_write_byte(mcp2515_state_t* state, int addr, uint8_t byte) {
    mcp2515_io_write_packet_t pkt;
    pkt.data[0] = byte;
    return mcp2515_io_write(state, addr, 1, &pkt);
}

int mcp2515_io_read_byte(mcp2515_state_t* state, int addr) {
    mcp2515_io_read_packet_t pkt;
    int rc = mcp2515_io_read(state, addr, 1, &pkt);
    if (!rc) {
        return pkt.data[0];
    } else {
        return -rc;
    }
}

