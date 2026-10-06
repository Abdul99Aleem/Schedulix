# Current State - Schedulix Backend (`schedulix_can`)

## 1. Repository Structure
* **QNX Backend (`c:\Users\User\ide-8.0.3-workspace\schedulix_can`)**:
  * `src/`: Core implementation files in C (workloads, analyzer, trace collector, stress generator, GPIO markers, CAN simulation/TCP inject, QNX tracelogger integration).
  * `tests/`: Python validation tests (`run_tests.py`, `test_phase6.py`, `test_analyzer_integrity.py`) and C unit tests (`test_decoder.c`).
  * `tools/`: Python helper clients (`can_sender.py`, `host_sim.py`, `host_sim_core.py`), shell validation scripts (`pi_validate.sh`, `qnx_bringup_check.sh`, `qnx_can_install.sh`), QNX bring-up clients over paramiko (`qnx_ssh.py`, `qnx_audit.py`, `qnx_can_install.py`), the serial-console launcher (`serial_console.ps1`), `spi.conf.mcp2515`, and the OpenSSH launcher `bringup.ps1`/`.cmd` (superseded by paramiko).
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
* **GPIO Timing Markers (`gpio_marker.c`)**: Pulsing logic for physical pins (GPIO 4, 17, 27) with software fallback verification. **Not yet functional on hardware — pending rewrite.** It still opens `/dev/mem` (absent on QNX), never writes `GPFSELn` so `GPSET0`/`GPCLR0` produce no electrical output, has a stub `gpio_marker_get()` that always returns `-1`, and covers only GPIO 0–31. See [`docs/gpio.md`](docs/gpio.md).
* **Analyzer (`analyzer.c`)**: Correlates binary trace records with raw QNX kernel events and attributes root cause triggers for misses.

## 3. Build System
* **QNX Backend**: Built via GNU Make with QNX SDP 8.0 compiler toolchain (`qcc` targeting `aarch64le` for Raspberry Pi 4/5).
* **Qt Frontend**: CMake based with Qt 6.8.3.

## 4. Targets
* **QNX Target**: Raspberry Pi 4 QNX RTOS 8.0 (`aarch64le`) at IP address `192.168.10.5`. The board actually in hand is **not** the `192.168.10.2` board referenced by earlier reports. Confirmed identity: `QNX qnxpi 8.0.0 2025/07/30-19:17:34EDT RaspberryPi4B aarch64le`, 4× Cortex-A72 @1500 MHz, 8128 MB, 38 processes / 258 threads. A **passwordless root shell** is available over the serial console (`root@console:/#`, 115200 8N1).
* **Qt Target**: Windows host system running the desktop UI client.

## 5. Known Working Pieces
* Compilation clean for `aarch64le` (Pi) and `x86_64` targets.
* Workload scheduling and execution, trace collection in SHM, and QNX `tracelogger` privilege check/provenance capture.
* Analyzer root cause classification and delay attribution.
* Monotonic latency data sweep generation (loads 20% to 95% run cleanly on RPi 4, as verified by target log `qnx_logs_7.txt`).
* All Python host verification and regression unit tests pass (13/13 Phase 6 tests, 3/3 analyzer tests).
* **UART adapter, verified on hardware 2026-10-06**: `src/uart_adapter.c` opens `/dev/ser1` in physical mode and transmits for real — 21 bytes were observed on the host COM6 from `schedulix uart send`, ahead of the program's own `printf`.
* **Noun/verb CLI, verified on hardware 2026-10-06**: `schedulix uart status` and `schedulix uart send <data>` execute correctly on the board.

## 6. Known Missing / Gap Items (Phases 2.11 - 2.15)
* **UART Device Integration (Phase 2.12)**: **Done.** `src/uart_adapter.c` is a complete QNX event reader/writer, not a stub: termios setup, 115200 8N1, `O_NONBLOCK`, and a simulated loopback fallback when the physical port cannot be opened. `uart_adapter_init()` defaults to `/dev/ser1`. Verified on live hardware on 2026-10-06 — `schedulix uart status` reports `UART Adapter Backend: physical` and `physical device /dev/ser1 configured at 115200 baud`; `open()`, `tcgetattr()` and `tcsetattr()` all succeed, and QNX permits a second open of `/dev/ser1` while the console owns it. TX is proven end-to-end: `schedulix uart send SCHEDULIX_TX_PROOF_42` produced 21 bytes on the host COM6 *before* the program's own `printf` output.
  * One outstanding defect: `src/uart_adapter.c:151` records a **transmit** as `TRACE_EXTERNAL_EVENT_RX`, so TX and RX are indistinguishable in the trace. A dedicated `TRACE_EXTERNAL_EVENT_TX` type is pending.
  * RX is **confounded, not broken**: on this board `/dev/ser1` **is** the serial console (driver `devc-serminiuart`, base `0xfe215000`, the BCM2715 mini-UART/AUX), so bytes sent from the host are consumed by the login shell. `uart receive` returned the console's own newline. RX remains unverified against an external peer.
* **CLI Structuring (Phase 2.14)**: **Done.** `src/main.c` implements a noun/verb parser (`schedulix <noun> <verb>`), not flat flags. Nouns are `status`, `workload`, `stress`, `trace`, `analyze`, `report`, `can`, `uart`, `gpio`, `experiment`, `help`. Verified live on the board — `schedulix uart status` and `schedulix uart send <data>` both work, and the binary advertises `uart status|start|send|receive` plus `gpio` and `can` groups. `--port <path>` and `--baud <rate>` override the UART defaults.
* **Deployable binary is stale**: the authoritative artefact is `build/aarch64le-debug/schedulix_can` (331,152 bytes, 2026-08-30 21:34, matching the newest source file). **Every binary in `deploy/` is stale** — a byte scan for `UART Adapter Backend`, `uart [status|start|send|receive]` and `gpio test` found **none** of them in `deploy/schedulix_can-aarch64le-debug` (154,384 B, 2026-08-29), `deploy/schedulix_can` (179,264 B, 2026-08-30) or `deploy/schedulix_can-x86_64` (175,360 B, 2026-08-30). All three strings are present in `build/aarch64le-debug/schedulix_can`. Deploy from `build/`, not `deploy/`.
* **Qt Integration**: The Qt app relies solely on `MockProvider` and does not yet parse the QNX backend's JSON reports.

## 7. Proposed Implementation Sequence
1. **Rewrite `gpio_marker.c`**: replace the `/dev/mem` mmap with `MAP_PHYS | MAP_SHARED` + `NOFD` + `PROT_NOCACHE` on `0xfe200000`, write `GPFSELn` before any `GPSET0`/`GPCLR0`, implement real `gpio_marker_get()`, and cover the full GPIO 0–53 range (bank 1 at `+32`).
2. **Fix the UART TX trace marker** at `src/uart_adapter.c:151`: add a dedicated `TRACE_EXTERNAL_EVENT_TX` type so TX and RX are distinguishable.
3. **Write `qnx_can_adapter.c`**: real `/dev/can0` mailboxes and `CAN_DEVCTL_*` calls to replace the simulated adapter and the TCP injector.
4. **Verify UART RX against an external peer**: `/dev/ser1` doubles as the console, so RX must be proven on a second board or with the console detached.
5. **Add JSON output reporting and clean exit codes** to the noun/verb CLI.
6. **Write the Qt JSON loader** to replace `MockProvider`.

## 8. Risks
* **Blocking UART operations**: *Mitigated.* `uart_adapter_init()` opens the port with `O_NONBLOCK` and sets `VMIN = 0` / `VTIME = 5` (a 0.5 s read timeout), so `read()` cannot starve real-time tasks. If throughput ever requires it, move I/O to a dedicated low-priority helper thread.
* **Physical Hardware Dependencies**: GPIO requires physical connections on the Pi for end-to-end verification, and `gpio_marker.c` is still pending rewrite. Keep the software stubs/mocks enabled automatically when physical drivers/ports are missing — and treat any `MOCK` result as unproven rather than as a pass.
* **Stale deploy artefacts**: shipping a binary from `deploy/` rather than `build/` will silently produce a program with no UART or CLI subcommands.
