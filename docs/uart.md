# Schedulix — UART Driver & Simulation Adapter

This document describes the design, implementation, and timing validation of the UART interface in Schedulix.

## 1. Objectives & Overview
The UART module handles data transmission and reception markers to correlate external serial messages with ECU workload releases and scheduling analysis. 

Key attributes:
* **Bounded execution**: No unbounded string processing in timing paths.
* **Deterministic timing**: Non-blocking read and write interfaces.
* **Dual mode**: Auto-detects physical QNX serial device (e.g., `/dev/ser1`) vs. software-simulated loopback.

## 2. API Contract (`uart_adapter.h`)
```c
int  uart_adapter_init(const char *device_path);
void uart_adapter_shutdown(void);
int  uart_adapter_send(const uint8_t *data, size_t len);
int  uart_adapter_receive(uint8_t *buf, size_t max_len);
const char* uart_adapter_status(void);
bool uart_adapter_is_simulated(void);
int  uart_adapter_trace_rx(uint32_t act_id, uint32_t corr_id, uint8_t byte);
int  uart_adapter_trace_tx(uint32_t act_id, uint32_t corr_id, uint8_t byte);
```

## 3. Implementation Details
* **QNX Physical Mode**: Opens `/dev/ser1` or `/dev/ser2` in non-blocking mode (`O_NONBLOCK`). Disables canonical input processing, signal characters, and echoing. Configures standard 115200 8N1 serial parameters.
* **Simulated Loopback Mode**: If the serial device fails to open, the adapter falls back to a simulated circular loopback queue of `256` bytes.
* **Trace Emission**: Invokes `trace_collector_record_simple` to append `TRACE_EXTERNAL_EVENT_RX` entries with arguments defining UART markers, byte values, and correlation IDs.
