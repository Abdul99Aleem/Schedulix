# Schedulix: Automotive RTOS Performance & Latency Analyzer

[![QNX](https://img.shields.io/badge/RTOS-QNX_Neutrino_8.0-blue.svg)](https://blackberry.qnx.com)
[![Target](https://img.shields.io/badge/Target-Raspberry_Pi_4_%2F_5_(aarch64)-red.svg)](https://www.raspberrypi.com)
[![GUI](https://img.shields.io/badge/Frontend-Qt_6_%2F_QML-green.svg)](https://www.qt.io)
[![License](https://img.shields.io/badge/License-Apache_2.0-lightgrey.svg)](LICENSE)

**Schedulix** is an explainable real-time performance and latency analysis engine designed for safety-critical automotive workloads running on **QNX Neutrino RTOS (SDP 8.0)**. 

In modern Software-Defined Vehicles (SDVs), mixed-criticality workloads (e.g., ASIL-D Braking, ASIL-B ADAS perception, and diagnostic telemetry) share multicore SoCs. Traditional CPU utilization graphs show *that* a system is loaded, but fail to explain *why* a deadline was missed, which higher-priority task caused preemption, or how microsecond jitter cascaded.

Schedulix bridges this gap by combining **deterministic, zero-allocation application instrumentation** with **native QNX kernel trace decoding (`libtraceparser`)** to deliver quantitative delay attribution, context-switch correlation, and evidence-backed root-cause analysis.

---

## Visual Showcase

| Performance Dashboard | Scheduling Gantt Timeline |
| :---: | :---: |
| ![Dashboard](screenshots/01_dashboard_1536x864.png) | ![Timeline](screenshots/02_timeline_1536x864.png) |
| *Real-time task latencies, response percentiles (p50/p95/p99), and deadline slack* | *Multi-core CPU execution lanes, thread state transitions, and context switches* |

| Root-Cause Analysis (RCA) | Automotive Experiment Suite |
| :---: | :---: |
| ![Root Cause](screenshots/03_rootcause_1536x864.png) | ![Experiments](screenshots/04_experiments_1536x864.png) |
| *Quantitative delay decomposition identifying culprit PID/TID and preemption windows* | *Automated benchmark scenarios (S0–S6) under varying synthetic and hardware stress* |

---

## Core Engineering Highlights

- **Native QNX `libtraceparser` C API Integration**: Directly decodes raw binary `.kev` trace streams captured by `tracelogger`. Reconstructs 64-bit cycle timestamps using sequential rollover detection—zero brittle regex text parsing.
- **Zero-Allocation MPSC Ring Buffer**: Fixed-size 48-byte records in bounded shared memory with lock-free sequence publication. Guaranteed zero `malloc()` calls in the critical execution path to preserve deterministic RTOS timing.
- **Formal Delay Attribution Model**: Quantitatively decomposes activation latency:
  $$\text{Response Time} = \text{Ready Queue Wait} + \text{Preemption Duration} + \text{Blocking Time} + \text{Execution Time}$$
- **Evidence-Backed Root Cause Engine**: Classifies deadline misses with explicit confidence levels (`CONFIRMED` vs `INFERRED`) and identifies the interfering process ID, thread ID, priority, and exact overlap window.
- **Hardware & Peripherals**:
  - **Physical UART**: Real POSIX serial driver (`/dev/ser1` at 115200 baud) for telemetry & external event triggering.
  - **Hardware GPIO Markers**: Direct BCM2711 peripheral register memory mapping (`0xFE200000`) for sub-microsecond physical oscilloscope timing verification.
  - **CAN Layer**: Non-blocking simulated adapter queue + TCP socket injection server.
- **Automated S0–S6 Benchmark Matrix**: Built-in test harness executing baseline runs, 20%–95% CPU load sweeps, mutex priority inversions, CAN event storms, and core affinity contention.

---

## System Architecture

```
 ┌────────────────────────────────────────────────────────────────────────┐
 │                      AUTOMOTIVE WORKLOAD LAYER                         │
 │   [BRAKE_CTL (Prio 20)]    [ADAS_FUSION (Prio 15)]    [DIAG (Prio 10)] │
 └─────────────────┬──────────────────────┬──────────────────────┬────────┘
                   │                      │                      │
           (Semantic Events)      (Semantic Events)      (Semantic Events)
                   ▼                      ▼                      ▼
 ┌────────────────────────────────────────────────────────────────────────┐
 │                 MPSC BOUNDED SHARED-MEMORY RING BUFFER                 │
 │     • Lock-free sequence publication     • 48-byte fixed records       │
 │     • Zero runtime heap allocation       • Post-mortem file flush      │
 └───────────────────────────────────┬────────────────────────────────────┘
                                     │
                                     ▼
                            [ trace_s4.bin ]
                                     │
 ┌───────────────────────────────────┴────────────────────────────────────┐
 │                       SCHEDULIX ANALYZER ENGINE                        │
 │                                                                        │
 │   [ trace_s4.bin ] ────────► Correlation & Latency Decomposition       │
 │                                      ▲                                 │
 │   [ /tmp/schedulix.kev ] ────────────┘                                 │
 │          ▲                                                             │
 │   (libtraceparser C API) ──► Reconstructs 64-bit raw kernel events:    │
 │                              THRUNNING, THREADY, THMUTEX, CSwitches    │
 └───────────────────────────────────┬────────────────────────────────────┘
                                     │
                                     ▼
                          [ analysis_s4.json ]
                                     │
         ┌───────────────────────────┴───────────────────────────┐
         ▼                                                       ▼
 ┌───────────────┐                                       ┌───────────────┐
 │  CLI REPORTS  │                                       │  Qt 6 / QML   │
 │ (Text/Tables) │                                       │  DASHBOARD    │
 └───────────────┘                                       └───────────────┘
```

---

## Real-Time Workload Model

Schedulix instruments a representative automotive mixed-criticality workload set:

| Task Name | Task ID | Priority | Period | Deadline | Execution Demand | Criticality | Target Role |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :--- |
| **`BRAKE_CTL`** | `1` | `20` (SCHED_FIFO) | 10 ms | 10 ms | 2.0 ms | **ASIL-D** | Emergency Braking & Stability Control |
| **`ADAS_FUSION`**| `2` | `15` (SCHED_FIFO) | 20 ms | 20 ms | 8.0 ms | **ASIL-B** | Radar/Camera Sensor Fusion |
| **`DIAG_POLL`** | `3` | `10` (SCHED_FIFO) | 50 ms | 50 ms | 5.0 ms | **QM** | OBD-II / UDS Diagnostics Telemetry |
| **`STRESS_WORKER`**| `4-8`| `5 - 22` | Config | Config | Config | **Stress** | Background Load & Contention Injector |

---

## Automated Experiment Suite (S0–S6)

Schedulix includes an automated benchmark runner to evaluate scheduling determinism under controlled stress:

| Scenario | Code | Description | Injected Condition | Evaluated Behavior |
| :--- | :---: | :--- | :--- | :--- |
| **Nominal Baseline** | `S0` | Clean execution | No background stress | Zero jitter, nominal slack (+7.98 ms) |
| **Load Sweep** | `S1` | Scaled CPU saturation | 20% ➔ 95% background load | Monotonic latency increase, knee detection |
| **Mutex Contention** | `S2` | Priority inversion | Shared resource lock | Priority inheritance & blocking delays |
| **CAN Burst** | `S3` | External I/O load | 10x CAN frame bursts | Event Manager dispatch latency & queueing |
| **Preemption Storm** | `S4` | High-prio preemption | Prio 22 task preemption | Confirmed preemption RCA & lateness metrics |
| **Mixed Criticality** | `S5` | Combined interference | CPU load + CAN + Mutex | Cumulative latency degradation |
| **Core Affinity** | `S6` | Multicore contention | Pinning tasks to Core 0 | Cache thrashing & scheduler migration |

---

## Directory Structure

```
Schedulix/
├── src/                         # QNX Native C Performance Engine
│   ├── main.c                   # CLI router & command entrypoint
│   ├── workload.c / .h          # Real-time periodic workload threads
│   ├── workload_config.c / .h   # Automotive ECU workload parameters
│   ├── trace_collector.c / .h   # Lock-free MPSC ring buffer collector
│   ├── trace_schema.h           # 48-byte binary trace record schema
│   ├── trace_instrumentation.h  # Zero-overhead timestamp macros
│   ├── qnx_kernel_trace_parser.c/.h # Native libtraceparser .kev parser
│   ├── scheduler_correlator.c/.h# Per-CPU scheduler transition correlator
│   ├── analyzer.c / .h          # Delay attribution & RCA engine
│   ├── stress_generator.c / .h  # Synthetic workload & CPU load generator
│   ├── stress_scenarios.c / .h  # Automated S0-S6 experiment harness
│   ├── uart_adapter.c / .h      # Physical POSIX UART driver (/dev/ser1)
│   ├── gpio_marker.c / .h       # Hardware GPIO memory mapper (0xFE200000)
│   ├── can_decoder.c / .h       # CAN frame ID & payload decoder
│   └── event_manager.c / .h     # Asynchronous event dispatcher
├── gui/                         # Qt 6 / QML Desktop Trace Viewer
│   ├── Main.qml                 # Main window container & navigation
│   ├── main.cpp                 # Qt application runner & backend bridge
│   ├── qml/                     # QML views (Dashboard, Timeline, RCA, Experiments)
│   └── src/                     # C++ trace file parser & QML data models
├── screenshots/                 # High-resolution application screenshots
├── docs/                        # Specifications, architecture, & QNX guides
├── tests/                       # Unit tests & Python verification scripts
├── Makefile                     # QNX SDP 8.0 qcc Makefile (aarch64le & x86_64)
└── Makefile.host                # Host fallback build (Linux/GCC/Clang)
```

---

## 📖 Documentation

**Start here → [`docs/BRINGUP_GUIDE.md`](docs/BRINGUP_GUIDE.md)** — the complete
reproducible procedure, from a clean clone to CAN frames on the wire: clone,
host tests, cross-compile, deploy, verify UART and GPIO, build and install the
MCP2515 CAN driver, and verify CAN.

It also carries a [mistakes-to-avoid section](docs/BRINGUP_GUIDE.md#12-mistakes-to-avoid)
worth reading *before* touching hardware.

### Run the host test suite

No board required:

```bash
python tools/run_all_tests.py     # expect 39/39 host tests PASS
```

On a fresh clone you get **38/39**, with one test skipped: `test_qnx_build`
checks the compiled QNX binary, so it needs `make` to have run first (section 4
of the guide). Run `make`, then re-test, to reach 39/39.

### Document index

| Document | Contents |
| --- | --- |
| [`docs/HANDOVER.md`](docs/HANDOVER.md) | **Start here** — what is verified, what is next, and why the load sweep is flat |
| [`docs/BRINGUP_GUIDE.md`](docs/BRINGUP_GUIDE.md) | **Master guide** — clone → build → deploy → UART → GPIO → CAN → verify |
| [`docs/PROBLEM_STATEMENT_COMPLIANCE.md`](docs/PROBLEM_STATEMENT_COMPLIANCE.md) | Requirement-by-requirement scorecard, verified vs. unverified |
| [`docs/INCIDENT_SPI_DRIVER.md`](docs/INCIDENT_SPI_DRIVER.md) | The SPI bring-up incident, what was ruled out, and the resume procedure |
| [`docs/RUNBOOK.md`](docs/RUNBOOK.md) | Day-to-day operating commands |
| [`docs/SERIAL_CONSOLE_RUNBOOK.md`](docs/SERIAL_CONSOLE_RUNBOOK.md) | Serial console setup and troubleshooting |
| [`docs/architecture.md`](docs/architecture.md) | Component design |
| [`docs/timing-model.md`](docs/timing-model.md) | Delay attribution mathematics |
| [`docs/gpio.md`](docs/gpio.md) · [`docs/uart.md`](docs/uart.md) | Per-interface detail |
| [`docs/HARDWARE_PROCUREMENT_PLAN.md`](docs/HARDWARE_PROCUREMENT_PLAN.md) | Bill of materials, wiring, CAN bus topology |
| [`CURRENT_STATE.md`](CURRENT_STATE.md) | Live project state |
| [`VALIDATION_LOG.md`](VALIDATION_LOG.md) | Chronological verification record |

### Target requirements

| | |
| --- | --- |
| OS | QNX Neutrino 8.0.0, Quick Start Target Image (QSTI) |
| Hardware | Raspberry Pi 4 (BCM2711) |
| Toolchain | QNX SDP 8.0 — the **SDP**, not just the IDE |
| Serial console | 115200 8N1, passwordless root at `root@console:/#` |
| SSH | `ssh -m hmac-sha2-256 qnxuser@<target-ip>` — the MAC override is **mandatory** |
| CAN | MCP2515 on SPI0/CE0 (Waveshare RS485 CAN HAT, SKU 14882) — **seat on the 40-pin header** |

> ⚠️ **Before wiring CAN:** read
> [BRINGUP_GUIDE §12.1](docs/BRINGUP_GUIDE.md#121-seating-the-can-hat-on-the-40-pin-header).
> The HAT must seat directly on the header. Hand-wiring the SPI signals with
> jumper leads removed the pinout guarantee and caused a power fault on the
> target that ended bring-up.

---

## Build & Deployment Guide

> The summary below covers building and deploying. For the full end-to-end
> procedure — including CAN driver installation and every known gotcha — use
> [`docs/BRINGUP_GUIDE.md`](docs/BRINGUP_GUIDE.md).

### Prerequisites
- **QNX Software Development Platform (SDP) 8.0**
- **QNX Neutrino RTOS Target** (Raspberry Pi 4 / 5 or x86_64 QNX VM)
- **Qt 6.5+** (for the optional Qt/QML UI viewer)

### 1. Cross-Compiling for QNX Target (aarch64le)

Source the QNX environment and compile using `qcc`:

```bash
# Set up QNX SDP environment
source ~/qnx800/qnxsdp-env.sh     # Linux / macOS
# or: C:\Users\User\qnx800\qnxsdp-env.bat  # Windows

# Build release/debug aarch64 binary
make PLATFORM=aarch64le BUILD_PROFILE=debug
```

The resulting statically-linked binary is created at: `build/aarch64le-debug/schedulix_can`.

### 2. Deploying to Raspberry Pi 4

Transfer the executable to the QNX target:

```bash
scp build/aarch64le-debug/schedulix_can root@<target-ip>:/tmp/
```

---

## Running on Target (CLI Commands)

SSH into your QNX board as `root` (`uid=0` is required for `tracelogger` kernel access):

```bash
ssh root@<target-ip>
cd /tmp
```

### 1. Check System Status & Peripherals
```bash
./schedulix_can status
```
*Outputs workload states, physical UART detection (`/dev/ser1`), GPIO availability, and privileged kernel tracer status.*

### 2. Run an Automated Experiment (e.g., Scenario 4: Preemption Storm)
```bash
./schedulix_can experiment run S4
```
*Generates `trace_s4.bin`, `manifest_s4.json`, and `analysis_s4.json` after running the automated preemption benchmark.*

### 3. Decode Raw QNX Kernel Events (`.kev`)
```bash
./schedulix_can trace decode /tmp/schedulix.kev | head -n 30
```
*Decodes binary kernel events (`THRUNNING`, `THREADY`, context switches) with nanosecond timestamps.*

### 4. Post-Mortem Trace Analysis
```bash
./schedulix_can analyze trace_s4.bin
```
*Executes the RCA engine and prints percentile stats (p50/p95/p99) and deadline slack.*

---

## Sample JSON Output (`analysis.json`)

```json
{
  "trace": {
    "records": 16384,
    "dropped": 0
  },
  "per_task": [
    {
      "task_id": 1,
      "activations": 1923,
      "misses": 0,
      "miss_ratio": 0.0000,
      "mean_ms": 2.012,
      "p50": 2.012,
      "p95": 2.018,
      "p99": 2.024,
      "max": 2.038
    }
  ],
  "activations": [
    {
      "task_id": 1,
      "activation_id": 1352,
      "correlation_id": 1352,
      "response_ns": 2012444,
      "slack_ns": 7987556,
      "cpu_first": 1,
      "root_cause": "HIGH_PRIORITY_PREEMPTION",
      "evidence_level": "CONFIRMED",
      "interferer_pid": 4294967295,
      "interferer_tid": 260,
      "interferer_priority": 22,
      "preemption_duration_ns": 10900000,
      "lateness_ns": 0,
      "context_switches": 42
    }
  ]
}
```

---

## Hackathon Team & Details

- **Project**: Schedulix — Automotive RTOS Performance & Latency Analyzer
- **Institution**: Vasavi College of Engineering
- **Team Members**:
  - **Abdul Aleem** (1602-23-735-001)
  - **Kritika Giridhar** (1602-23-735-018)
  - **Rishi N.** (1602-23-735-033)
- **Problem Statement**: Track 16 — Automotive RTOS Scheduling Analysis, Instrumentation & Observability on QNX / Raspberry Pi
