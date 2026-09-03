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

## 3. QNX Pi Target-Side Verification (Log Analysis)
Based on verification runs executed on `root@qnxpi` (from QNX target execution logs `docs/logs/qnx_logs_7.txt`):
* **TEST 1: Workloads (`--workloads`)**: ECU workloads initialize nominal execution. `BRAKE_CTL` (prio 20, period 10ms), `ADAS_FUSION` (prio 15, period 20ms), `DIAG_POLL` (prio 10, period 50ms) are spawned. -> **PASS**.
* **TEST 2: Full Core (`--full`)**: Feeds CAN frame events, triggers workload releases, captures monotonic timing parameters, and flushes traces. -> **PASS**.
* **TEST 3: Kernel Tracer (`tracelogger`)**: Captures real context-switch events under root privileges into `/tmp/schedulix.kev` (~114KB). -> **PASS**.
* **TEST 4: Load Sweep (`--sweep`)**: Saturation sweep from 20% to 95% CPU load runs cleanly, generating bin, manifest, and analysis reports for each level. -> **PASS**.
* **TEST 5: Mutex Contention (`--stress 2`)**: Blocks `BRAKE_CTL` lock under priority inversion, proving `MUTEX_BLOCK` root-cause attribution. -> **PASS**.
* **TEST 6: GPIO Validation**: Mapped hardware or mock validation toggles pins, verifying timing deltas are within the 50µs tolerance window. -> **PASS**.
