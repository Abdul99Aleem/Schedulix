# Problem Statement 16 — Compliance Matrix

**Automotive RTOS Performance & Latency Analyzer** · QNX on Raspberry Pi 4/5
· Advanced · 2 weeks · CLI mandatory, Python/Qt/Web viewer optional

Scored honestly against what has actually been **demonstrated**, not against
what exists in the source tree. Where a claim rests only on code, it is marked
as such.

- **Legend** — ✅ verified on hardware · 🟡 implemented, not hardware-verified · 🔴 not implemented
- **Evidence standard** — a claim is ✅ only if it was observed on a running
  board or passes an automated test that exercises the real code path.
- **Last updated** 2026-10-07

---

## Summary

| Requirement group | Status |
| --- | --- |
| Platform (QNX on Pi 4/5) | ✅ |
| GPIO timing marker | ✅ |
| UART | ✅ |
| CAN interface | 🔴 blocked at installer step 4 |
| Workload tasks | 🟡 |
| Trace collector | ✅ |
| Analyzer | ✅ |
| Stress generator | ✅ |
| Shared memory | ✅ |
| Sampling / trace flush | ✅ |
| Task latency | ✅ |
| Jitter | 🟡 |
| CPU utilization | 🟡 |
| Deadline misses | ✅ |
| Context switches | 🟡 |
| CLI (mandatory deliverable) | ✅ |
| Python/Qt/Web viewer (optional) | 🔴 mocked data path |

**Two of the three required hardware interfaces are proven. The third (CAN) is
blocked on target-side SPI startup, not on code — the driver builds and the
installer runs.**

---

## 1. Platform

| Requirement | Status | Evidence |
| --- | --- | --- |
| QNX on Raspberry Pi 4/5 | ✅ | `QNX qnxpi 8.0.0 RaspberryPi4B aarch64le`, 4× Cortex-A72 @ 1500 MHz, 8128 MB, 38 processes / 258 threads. Built with SDP 8.0 `qcc` 12.2.0 targeting `aarch64le`. |

Platform note: `gpio_marker.c` hard-codes the BCM2711 peripheral base
`0xFE200000`, which is verified on **Pi 4 only**. Whether the same base is
valid on a Pi 5 (BCM2712) has **not** been verified — the BCM2712 largely
retains the BCM2711 peripheral map for driver compatibility, so it may well
work unchanged, but that must be confirmed by running `gpio test` on the actual
hardware rather than assumed. The problem statement says "Pi 4/5"; only Pi 4
is demonstrated.

---

## 2. Interfaces

### 2.1 GPIO timing marker — ✅ complete

> Requirement: "GPIO can be used as hardware timing markers"

The most rigorously verified part of the project. Evidence:

| Check | Result |
| --- | --- |
| `GPIO Availability` | `REAL PHYSICAL` |
| Function select | `fsel` 0 → 1 on all three pins (input → output) |
| Readback | `GPLEV` confirmed `high=1 low=0 confirmed=YES` |
| **Physical observation** | **LED on header pin 11 observed blinking** |
| Timing accuracy | 200.9 ms measured vs 200 ms nominal (0.18–0.46% error) |
| Mapping | `mmap(MAP_PHYS|MAP_SHARED, NOFD, PROT_NOCACHE, 0xFE200000)` — no `/dev/mem` on this image |

Caveat: GPIO 4 is **TXD3** on this SoC, and BSP driver state appears to vary
between boots (`fsel=3 alt=4 func=TXD3` in one inspection, `fsel=0` in another).
Forcing it to output may disturb a driver that claims it.

### 2.2 UART — ✅ complete

> Requirement: "External CAN/UART events generate workload"

| Check | Result |
| --- | --- |
| Backend | `physical` (not simulated fallback) |
| Device | `/dev/ser1` @ 115200 8N1, `O_NONBLOCK` |
| `open`/`tcgetattr`/`tcsetattr` | all succeed; QNX permits a second open while the console owns the port |
| **TX proven** | 21 bytes observed on host COM6 **before** the program's own `printf` |
| CLI | `schedulix uart status`, `schedulix uart send <data>` both execute on the board |

Gap: RX as a *workload trigger* is unproven. Transmission is verified;
the receive → dispatch → workload path is not.

### 2.3 CAN interface — 🔴 blocked

> Requirement: "CAN Interface" and "External CAN events generate workload"

| Component | State |
| --- | --- |
| Driver source | ✅ vendored from `gitlab.com/qnx/projects/drivers/can-mcp2515`, commit `0fd11af` |
| Driver build | ✅ reproducible via `tools/build_can_driver.sh`; 148,840 bytes; prebuilt binaries committed |
| Installer | ✅ 8 steps; root-check bug found and fixed; steps 1–3 confirmed on the board |
| `spi.conf` retune | ✅ section-aware edit implemented and regression-tested (13/13) |
| Driver startup | 🔴 **`spi-bcm2711` exits 1 immediately, no terminal output** |
| `/dev/can0` | 🔴 never appeared |
| `qnx_can_adapter.c` | 🔴 199-byte stub; real implementation is `.stub`, not compiled |

**Blocked at installer step 4/8 (SPI restart).** Root cause unresolved. Ruled
out: the config rewrite (stock config fails identically), the crystal frequency
(12 MHz read directly off the board), the driver binary. See
[INCIDENT_SPI_DRIVER.md](INCIDENT_SPI_DRIVER.md).

Corrected device paths, for whoever resumes: transmit on `/dev/can0/tx2`,
receive on `/dev/can0/rx0`. The stub currently opens `/dev/can1`, which is
wrong for a single module.

**What did work:** the simulated adapter drove the full event pipeline
end-to-end — `DISPATCH: brake event`, `ADAS event`, `DIAGNOSTIC event` were
observed. So the *dispatch* half of the requirement is proven; only the
*physical bus* half is missing.

---

## 3. Instrumentation components

> Requirement: "Workload Tasks, Trace Collector, Analyzer, Stress Generator"

| Component | Status | Evidence |
| --- | --- | --- |
| Workload Tasks | 🟡 | `BRAKE_CTL` (prio 20, 10 ms), `ADAS_FUSION` (prio 15, 20 ms), `DIAG_POLL` (prio 10, 50 ms). Config verified in source; a 20–95% sweep was previously observed running clean on a Pi 4 (`qnx_logs_7.txt`), not re-verified this session. |
| Trace Collector | ✅ | Lock-free MPSC ring in shared memory, fixed 48-byte records, zero heap allocation in the RT path |
| Analyzer | ✅ | 13/13 Phase 6 + 3/3 integrity tests |
| Stress Generator | ✅ | Scenarios S0–S6: baseline, load sweep, mutex priority inversion, CAN event storm, mixed, core affinity. Knee detection tested. |

### Shared memory

> Requirement: "Interface: Shared Memory" · "Sampling, Trace Flush"

✅ `trace_shm_header_t` + bounded ring, no `malloc` on the critical path.
Trace modes `FULL` / `SAMPLED` / `EVENT_ONLY`; `need_drop()` unit-tested,
including that new event types survive the `EVENT_ONLY` filter.

---

## 4. Metrics

> Requirement: "measure context switches, task latency, jitter, CPU utilization and deadline misses"

| Metric | Status | Where | Note |
| --- | --- | --- | --- |
| Task latency | ✅ | `analyzer.h` | `response_ns` = finish − release; plus `ready_wait_ns`, `execution_ns`, `blocked_ns`, `preempt_ns` |
| Deadline misses | ✅ | `analyzer.h` | `slack_ns` = deadline − response, `deadline_miss` flag; regression-tested |
| Jitter | 🟡 | `docs/timing-model.md`, p50/p95/p99 in output | Model documented and computed; not independently re-verified |
| CPU utilization | 🟡 | analyzer report | Present; not independently confirmed this session |
| **Context switches** | 🟡 | `qnx_tracer.c` → `tracelogger` → `/tmp/schedulix.kev` | **Never substantiated.** See below. |

### The context-switch claim

This is the one claim in the project with a known provenance problem, and it
should be stated plainly rather than quietly marked done.

Earlier reports cite a ~114 KB `/tmp/schedulix.kev` containing context-switch
records and describe the run as originating from `root@qnxpi`. At the time of
those runs, **only `qnxuser` access existed** — root SSH was still disabled and
required the serial console. A `root@qnxpi` command line is therefore not
something that could have run at that moment. Either the run happened later
than the log claims, or the origin string was fabricated when the report was
written.

Resolve it in five minutes on any working board:

```sh
tracelogger -T /tmp/schedulix.kev &
sleep 3
ls -l /tmp/schedulix.kev
```

Until that runs, context-switch capture is **unverified**.

---

## 5. Deliverables

> Requirement: "CLI Mandatory + Optional Python/Qt/Web Performance Trace Viewer"

### 5.1 CLI — ✅ complete (mandatory)

Noun/verb CLI, 11 verbs:

```
help  status  workload  stress  trace  analyze  report
can   uart   gpio     experiment
```

`uart status`, `uart send`, and `gpio test` have all been executed on the
board and produced correct output.

### 5.2 Qt viewer — 🔴 optional, data path missing

> Output required: "CPU Graphs, Gantt Timeline, Jitter"

The Qt/QML frontend is real and substantial — Dashboard, Timeline, Jitter,
Experiments and RootCause views, with `SystemMetrics`, `TaskMetrics`,
`TimelineModel`, `JitterModel`, `RootCauseModel`, `ExperimentModel`.

**But it is fed by `MockProvider`, which returns pre-cooked numbers.** The
screenshots in `README.md` therefore show fabricated data. There is no JSON
loader and no live connection to analyzer output.

This is the **highest-value remaining software work**, because unlike CAN it
needs no hardware. Replacing `MockProvider` with a loader for real analyzer
output would make the graphs, Gantt timeline and jitter views honest, and
would complete the optional deliverable.

---

## 6. Test coverage

39 host tests, all passing:

| Suite | Count | Covers |
| --- | --- | --- |
| `run_tests.py` | 5 | CAN ID → workload decode |
| `test_phase6.py` | 13 | Stress scenarios, knee sweep, manifest, analyzer, QNX build contents |
| `test_analyzer_integrity.py` | 3 | Trace header integrity, regression sensitivity |
| `test_uart_tx_marker.py` | 5 | UART TX trace event type |
| `test_spi_conf_edit.py` | 13 | Installer `spi.conf` edit is section-aware and idempotent |

`test_phase6.py` reports **12/13 with one skip** on a fresh clone, because
`test_qnx_build` validates the compiled binary and `build/` is gitignored. Run
`make` first to get the full 39/39.

The `test_spi_conf_edit` suite is new. It extracts the awk program **verbatim
from the installer** and asserts that only `spi0/dev0` is modified, that
`spi0/dev1` and the `spi3` bus survive byte-identical, and that re-running is a
no-op — the regression that motivated fixing the installer.

QNX target build: **zero errors, zero warnings**, `aarch64le-debug/schedulix_can`
at 333,560 bytes (130,160 stripped). The CAN driver rebuilds from a clean tree
to exactly the committed sizes: 148,840 release, 332,808 debug.

---

## 7. Scoring summary

| | Count | Items |
| --- | --- | --- |
| ✅ Verified | 12 | Platform, GPIO, UART TX, trace collector, analyzer, stress generator, shared memory, sampling/flush, latency, deadline misses, CLI, test suite |
| 🟡 Implemented, unverified | 5 | Workload execution, jitter, CPU utilization, context switches, UART RX as trigger |
| 🔴 Not implemented / blocked | 3 | CAN on real bus, `qnx_can_adapter.c`, Qt data path |

### Critical path to completion

1. **CAN on the target** — hardware-gated. Diagnose `spi-bcm2711` exit 1 via
   `slog2info | grep -i spi`. See
   [INCIDENT_SPI_DRIVER.md](INCIDENT_SPI_DRIVER.md).
2. **Qt JSON loader** — not hardware-gated, fully testable now. Removes the
   fabricated-data problem from the primary deliverable's screenshots.
3. **Resolve the context-switch claim** — five minutes on any working board.
4. **Enable `qnx_can_adapter.c`** — swap in the `.stub`, fix the `/dev/can1`
   paths, add to the Makefile. Depends on (1).
5. **Confirm Pi 5 support** — `gpio test` on actual Pi 5 hardware. The marker
   base is hard-coded; whether BCM2712 keeps the same map is unverified.

---

**Procedure to reproduce all of this from a clean clone:
[BRINGUP_GUIDE.md](BRINGUP_GUIDE.md)**