# Current State - Schedulix Backend (`schedulix_can`)

## 1. Repository Structure
* **QNX Backend (`c:\Users\User\ide-8.0.3-workspace\schedulix_can`)**:
  * `src/`: Core implementation files in C (workloads, analyzer, trace collector, stress generator, GPIO markers, CAN simulation/TCP inject, QNX tracelogger integration).
  * `tests/`: Python validation tests (`run_tests.py`, `test_phase6.py`, `test_analyzer_integrity.py`) and C unit tests (`test_decoder.c`).
  * `tools/`: Python helper clients (`can_sender.py`, `host_sim.py`) and shell validation scripts (`pi_validate.sh`).
  * `build/`: Target directories for compilation outputs (`aarch64le-debug`, `x86_64-debug`).
* **Qt Frontend (`c:\Users\User\Documents\schedulix_v1`)**:
  * Qt Creator/CMake configuration.
  * QML-based theme, components, and views (Dashboard, Timeline, Jitter, Experiments, RootCause).
  * C++ controllers and data models (`SystemMetrics`, `TaskMetrics`, `TimelineModel`, `JitterModel`, `RootCauseModel`, `ExperimentModel`).
  * `MockProvider.cpp` providing pre-cooked dataset values.

## 2. Existing Components
* **QNX Workload Config & Tasks (`workload.c`, `workload_config.c`)**: Periodic workloads `BRAKE_CTL` (prio 20, period 10ms), `ADAS_FUSION` (prio 15, period 20ms), and `DIAG_POLL` (prio 10, period 50ms) executing deterministic CPU load cycles.
* **Trace Collector (`trace_collector.c`)**: Lockless shared-memory ring buffer using atomic index operations.
* **Monotonic Timing (`trace_instrumentation.c`)**: High-res timing calculations (release, ready, start, finish, preemption, CPU migration).
* **QNX Kernel Tracer (`qnx_tracer.c`)**: Spawns and configures the native QNX `tracelogger` to generate context switch records in `/tmp/schedulix.kev`.
* **Stress Generator (`stress_generator.c`, `stress_scenarios.c`)**: Generates custom CPU load contention (20% to 95% via load sweep) and priority inversion scenarios (S0–S6).
* **GPIO Timing Markers (`gpio_marker.c`)**: Pulsing logic for physical pins (GPIO 4, 17, 27) with software fallback verification.
* **Analyzer (`analyzer.c`)**: Correlates binary trace records with raw QNX kernel events and attributes root cause triggers for misses.

## 3. Build System
* **QNX Backend**: Built via GNU Make with QNX SDP 8.0 compiler toolchain (`qcc` targeting `aarch64le` for Raspberry Pi 4/5).
* **Qt Frontend**: CMake based with Qt 6.8.3.

## 4. Targets
* **QNX Target**: Raspberry Pi 4 QNX RTOS 8.0 (`aarch64le`) at IP address `192.168.10.2`.
* **Qt Target**: Windows host system running the desktop UI client.

## 5. Known Working Pieces
* Compilation clean for `aarch64le` (Pi) and `x86_64` targets.
* Workload scheduling and execution, trace collection in SHM, and QNX `tracelogger` privilege check/provenance capture.
* Analyzer root cause classification and delay attribution.
* Monotonic latency data sweep generation (loads 20% to 95% run cleanly on RPi 4, as verified by target log `qnx_logs_7.txt`).
* All Python host verification and regression unit tests pass (13/13 Phase 6 tests, 3/3 analyzer tests).

## 6. Known Missing / Gap Items (Phases 2.11 - 2.15)
* **UART Device Integration (Phase 2.12)**: Currently, only UART RX/TX comments exist in the code. A real QNX UART event reader/writer mapping `/dev/ser1` or `/dev/ser2` is not implemented.
* **CLI Structuring (Phase 2.14)**: The current CLI parser in `main.c` relies on basic flags (`--full`, `--sweep`, `--stress`, `--analyze`) rather than the structural command formats like `schedulix workload start`, `schedulix stress start --cpu 80`, etc.
* **Qt Integration**: The Qt app relies solely on `MockProvider` and does not yet parse the QNX backend's JSON reports.

## 7. Proposed Implementation Sequence
1. **Implement UART Driver/Sim Adapter (`uart_adapter.h/c`)**: Add UART reader loop opening serial interface `/dev/ser1` (with simulated loopback fallback if physical serial port not connected), logging UART RX/TX event records.
2. **Refactor CLI Parser**: Structure `main.c` argument loop to match `schedulix <noun> <verb>` command verbs. Provide output reporting in JSON and clean exit codes.
3. **Re-build & Verify**: Build binary for `aarch64le` and execute python unit tests.

## 8. Risks
* **Blocking UART operations**: Blocking on `read()` for serial port could starve real-time tasks. Must run in a non-blocking mode or a dedicated low-priority helper thread.
* **Physical Hardware Dependencies**: UART and GPIO require physical connections on Pi for end-to-end verification. Must keep robust software stubs/mocks enabled automatically when physical drivers/ports are missing.
