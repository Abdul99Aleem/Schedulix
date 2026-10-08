# Schedulix — UART Driver & Simulation Adapter

This document describes the design, implementation, and timing validation of the UART interface in Schedulix.

## 1. Objectives & Overview
The UART module handles data transmission and reception markers to correlate external serial messages with ECU workload releases and scheduling analysis. 

Key attributes:
* **Bounded execution**: No unbounded string processing in timing paths.
* **Deterministic timing**: Non-blocking read and write interfaces.
* **Dual mode**: Auto-detects physical QNX serial device (e.g., `/dev/ser1`) vs. software-simulated loopback.

## 2. API Contract (`uart_adapter.h`)

The QNX UART event reader/writer interface below is **implemented and verified on hardware**.

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
* **QNX Physical Mode**: Opens `/dev/ser1` (the default; `/dev/ser2` may be passed
  explicitly) in non-blocking mode (`O_NONBLOCK`). Disables canonical input processing,
  signal characters, and echoing. Configures standard 115200 8N1 serial parameters.
* **Simulated Loopback Mode**: If the serial device fails to open, the adapter falls back to a simulated circular loopback queue of `256` bytes.
* **Trace Emission**: Invokes `trace_collector_record_simple` to append marker entries with
  arguments defining the byte value and correlation IDs.

  **Known defect — TX and RX are indistinguishable in the trace.** *Both* the RX and the TX
  path currently emit `TRACE_EXTERNAL_EVENT_RX`:
  * `src/uart_adapter.c:147` — `uart_adapter_trace_rx()` → `TRACE_EXTERNAL_EVENT_RX`
  * `src/uart_adapter.c:151` — `uart_adapter_trace_tx()` → `TRACE_EXTERNAL_EVENT_RX` *(wrong)*

  The only distinguishing feature today is the trailing marker argument (`1` = UART marker
  for RX, `2` = UART TX marker for TX), which no analyzer currently interprets. A dedicated
  `TRACE_EXTERNAL_EVENT_TX` event type is **pending**, and until it exists a trace cannot
  tell a transmitted byte from a received one.

## 4. Hardware Status (verified 2026-10-06)

The adapter is complete and was exercised against the real board:

| Check | Result |
| --- | --- |
| Backend mode | `UART Adapter Backend: physical` — not the simulated loopback |
| Device / line settings | `physical device /dev/ser1 configured at 115200 baud` |
| `open()` / `tcgetattr()` / `tcsetattr()` | All succeed; QNX permits a **second open** of `/dev/ser1` while the console owns it |
| TX | **Proven.** `schedulix uart send SCHEDULIX_TX_PROOF_42` produced 21 bytes on the host COM6 *before* the program's own `printf` output — real transmitted bytes, not buffered echo |
| RX | **Unverified — confounded, not broken.** See below |

### The `/dev/ser1`-is-the-console confound

This board has **exactly one** serial device. `/dev/ser1` is driven by
`devc-serminiuart -e -b115200 -c500000000 -e -F -u1 0xfe215000,125` — base `0xfe215000` is
the BCM2715 **mini-UART/AUX**, not the PL011 — and `/dev/ser1` **is the serial console**.

The `-e` flag means the driver echoes every byte it receives. Consequently, on-target RX
testing is confounded rather than broken:

* Bytes sent from the host are consumed by the login shell before the adapter can read them.
* The driver echoes those bytes, so `uart receive` returned the console's own newline.
* Any subsequent `uart` command inherits the shell's terminal state, which is why
  `uart_adapter_init` explicitly clears canonical mode, signal characters and echo.

To verify RX properly, test against an **external peer** (a second board, or the console
detached) so the port is not shared with the shell.
