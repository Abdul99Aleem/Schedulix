# Schedulix — Validation Log

This document records the compilation, host-side unit testing, and QNX target-side validation of the Schedulix Backend program.

## 1. Compilation Verification
* **Target Build**: `aarch64le` (Raspberry Pi 4/5)
* **Compiler**: QNX SDP 8.0 `qcc` (GCC 12.2.0)
* **Build Profiles**: `debug`
* **Status**: **PASS** (Zero errors, built binary output to `build/aarch64le-debug/schedulix_can`).

## 2. Host-Side Verification
* **Python Verification Test (`tests/run_tests.py`)**: Checks CAN parsing and event mappings.
  * **Result**: **PASS** (5/5 tests passed).
* **Stress Generator Validation (`tests/test_phase6.py`)**: Validates stress scenarios S0-S6 definitions, configurations, and baseline parameters.
  * **Result**: **PASS** (13/13 tests passed).
* **Analyzer Integrity Validation (`tests/test_analyzer_integrity.py`)**: Tests binary trace file header verification, bad magic checks, and regression changes.
  * **Result**: **PASS** (3/3 tests passed).

## 3. QNX Pi Target-Side Verification (Log Analysis — NOT session-verified)

> **Provenance note.** The results in this section are derived from **reading an older
> target execution log** (`docs/logs/qnx_logs_7.txt`). They were *not* observed live during
> the 2026-10-06 session and have not been reproduced against the board currently in hand.
> They describe what the log claims, not what has been independently confirmed. See
> §4 for the live results.

Based on verification runs executed on `root@qnxpi` (from QNX target execution logs `docs/logs/qnx_logs_7.txt`):
* **TEST 1: Workloads (`--workloads`)**: ECU workloads initialize nominal execution. `BRAKE_CTL` (prio 20, period 10ms), `ADAS_FUSION` (prio 15, period 20ms), `DIAG_POLL` (prio 10, period 50ms) are spawned. -> **PASS**.
* **TEST 2: Full Core (`--full`)**: Feeds CAN frame events, triggers workload releases, captures monotonic timing parameters, and flushes traces. -> **PASS**.
* **TEST 3: Kernel Tracer (`tracelogger`)**: Captures real context-switch events under root privileges into `/tmp/schedulix.kev` (~114KB). -> **PASS**.
* **TEST 4: Load Sweep (`--sweep`)**: Saturation sweep from 20% to 95% CPU load runs cleanly, generating bin, manifest, and analysis reports for each level. -> **PASS**.
* **TEST 5: Mutex Contention (`--stress 2`)**: Blocks `BRAKE_CTL` lock under priority inversion, proving `MUTEX_BLOCK` root-cause attribution. -> **PASS**.
* **TEST 6: GPIO Validation**: **NOT a physical proof.** The same log shows `[GPIO] no HW, mock mode (validation uses sw timestamps)`. In mock mode the test compares software timestamps against software timestamps, which is vacuous — it demonstrates nothing about electrical output. Recording it as PASS is misleading. -> **MOCK / INCONCLUSIVE**.
  * For reference, `schedulix gpio test` prints `REAL PHYSICAL` when the registers are genuinely mapped and `MOCK` on the fallback path (`src/main.c:515`). There is no bare `REAL` string — `schedulix status` uses a different phrasing, `REAL (BCM2711 registers mapped)` / `MOCK (software simulation)`, at `src/main.c:249`.

## 4. Live Hardware Verification — 2026-10-06 (session-verified)

Everything in this section was observed directly on the board, not inferred from a log.

### 4.1 Board identity
* `QNX qnxpi 8.0.0 2025/07/30-19:17:34EDT RaspberryPi4B aarch64le`
* 4× Cortex-A72 @ 1500 MHz; FreeMem 7530MB / 8128MB; 38 processes, 258 threads.
* Correct SoC and the 8 GB model — not the unsupported low-memory variant.

### 4.2 Serial console and privilege — **PASS**
* CH340 USB-TTL adapter, **115200 8N1**, on the Pi's mini-UART (`/dev/ser1`).
* **Root shell obtained with no password**: `root@console:/#`, reporting `uid=0(root)` on
  hostname `qnxpi`.
* This clears the previously recorded blocker "CAN driver install needs root; no `sudo`,
  root fs read-only".

### 4.3 UART adapter — physical mode confirmed, TX proven
* `schedulix uart status` reports `UART Adapter Backend: physical` and
  `physical device /dev/ser1 configured at 115200 baud` — i.e. the adapter opened the real
  device rather than falling back to the simulated loopback.
* `open()`, `tcgetattr()` and `tcsetattr()` all succeed, and QNX permits a second open of
  `/dev/ser1` while the console owns it.
* **TX proven end-to-end**: `schedulix uart send SCHEDULIX_TX_PROOF_42` produced 21 bytes
  observed on the host COM6 **before** the program's own `printf` output — the signature of
  genuinely transmitted bytes rather than buffered echo.
* **Serial device caveat**: the board has exactly one serial device, `/dev/ser1`, driven by
  `devc-serminiuart` at base `0xfe215000` (the BCM2715 mini-UART/AUX, not the PL011).
  `/dev/ser1` **is** the console, so RX testing is confounded: bytes sent from the host are
  consumed by the login shell, and `uart receive` returned the console's own newline. This is
  a test-harness confound, not evidence that RX is broken.

### 4.4 Host-side unit tests — **PASS**
Re-run during this session, all green:
* `tests/run_tests.py` — **5/5**
* `tests/test_phase6.py` — **13/13**
* `tests/test_analyzer_integrity.py` — **3/3**

### 4.5 Still unverified
* **GPIO physical output (LED)** — `src/gpio_marker.c` still uses `/dev/mem` (absent on QNX),
  never writes `GPFSELn`, has a stub `gpio_marker_get()`, and covers only GPIO 0–31.
  `schedulix gpio test` has **not** been shown to print `REAL PHYSICAL`, and no LED has been
  observed lighting.
* **CAN hardware** — the MCP2515 driver has not been installed on this board; `spi.conf` and
  the installer were never uploaded to `/tmp` because the uploader was root-blocked.
* **UART receive against an external peer** — unverified, for the console-sharing reason in
  §4.3.
