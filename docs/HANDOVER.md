# Project Handover — what is done, what is next

**As of 2026-10-08.** Single reference for state and priorities. Supersedes
[`SESSION_LOG_2026-10-06.md`](SESSION_LOG_2026-10-06.md) where they disagree.

Branch `hw/procurement-and-gap-plan`. Host tests **26/26 pass**, 1 suite skipped
(`test_spi_conf_edit.py` needs `awk`; run from Git Bash for 39/39). Build clean,
zero warnings, `build/aarch64le-debug/schedulix_can` at 340,784 bytes.

---

## 1. Verified — safe to present

Every item below was observed on hardware, not inferred.

| Item | Evidence |
| --- | --- |
| **Platform** | QNX 8.0.0, 4× Cortex-A72 @ 1500 MHz, 8128 MB. Re-confirmed on the replacement board: 40 processes, 260 threads, 7,525 MB free. |
| **GPIO timing markers** | **Two independent boards.** `REAL PHYSICAL`, fsel 0→1 on all three pins, `GPLEV` readback confirmed. Pulse error **0.27 %–0.48 %** across six captures, agreeing between boards within microseconds. LED on header pin 11 observed blinking. |
| **UART TX** | `/dev/ser1` physical backend, 115200 8N1. 21 bytes observed on host COM6 *before* the program's own `printf`. |
| **Workload execution** | `BRAKE_CTL` / `ADAS_FUSION` / `DIAG_POLL` at configured priorities and periods. Activation counts exact: 500 / 250 / 100 in a 10 s run at 10 / 20 / 50 ms. |
| **Latency, percentiles, deadline misses** | Measured on hardware across S0, S4 and the S1 sweep. Zero records dropped at capacity 65,536. |
| **Kernel trace capture** | 24 MB from an instrumented kernel. Reproducible. |
| **Kernel trace parsing** | `libtraceparser` path decodes correctly on hardware — real PIDs, TIDs, `STATE_*` names, `Cycles/sec: 808465461`. Five real defects found and fixed. |
| **CLI** | Noun/verb, verified on board. |
| **Host test suite** | 26/26. |
| **Qt GUI** | Builds and launches with the Qt 6.8.3 MinGW toolchain. |

---

## 2. Implemented, not verified — present with care

| Item | State |
| --- | --- |
| **Context switch count** | Capture works, parser works, **the count is not a validated measurement.** Decoding still yields only `PID 1` system threads; workload TIDs are absent. `tracelogger` buffer overruns persist. Present as *instrumented and decoding correctly, count not yet validated*. |
| **Jitter** | Percentiles exist; **no stddev is computed anywhere.** Zero at S0 by design — response equals execution with no contention. |
| **CPU utilization** | **Not measured.** `stress_generator_utilization()` returns the load you *requested*, not a measurement. Do not report it as utilization. |
| **External-event workload generation** | Dispatch proven via the simulated adapter (`DISPATCH: brake event`). Physical bus missing. |
| **UART RX as trigger** | Unproven — `/dev/ser1` is the serial console, so RX is confounded. |

---

## 3. Not implemented

| Item | State |
| --- | --- |
| **CAN on real bus** | Blocked at installer step 4 of 8. Driver builds (148,840 B), installer steps 1–3 confirmed. `spi-bcm2711` exits 1 with no output. **Cut for this cycle** — do not demo. |
| **`qnx_can_adapter.c`** | 199-byte stub, not compiled. Wrong paths: TX on `/dev/can0/tx2`, RX on `/dev/can0/rx0`. |
| **Qt data path** | GUI runs on `MockProvider`. Screenshots in the README show fabricated numbers. **Highest-value remaining software work** — needs no hardware. |
| **Pi 5 support** | GPIO base hardcoded for BCM2711. Claim Pi 4 only. |

---

## 4. Defects found and fixed this cycle

Twelve commits, `556463a` → `7223e03`. Every one surfaced by hardware output
rather than code inspection. This is the substantive work of the session.

| Commit | Defect |
| --- | --- |
| `871fa1d` | `tracelogger -T` documented but nonexistent — the command was never runnable, which is why the context-switch claim sat unverified for sessions. Corrected with full SDP 8.0 syntax. |
| `556463a` | Parser registered callbacks against an *internal* encoding (`_TRACE_PR_TH_C >> 10`) where an *external* class is required — never fired. `traceparser_get_info` result cast to `uint64_t*` when it returns `unsigned*` — 8 bytes read across two 4-byte fields, every timestamp decoded identically. Rollover compared against the trace base instead of the previous event. `get_events(NULL, 0)` returned 0 for a full parse. Correlator used MUTEX=11, CONDVAR=12, SEM=15, DESTROY=23; real values are 13, 14, 17, 25 — nanosleep was misread as mutex blocking, a wrong root cause on any periodic workload. |
| `d690637`, `11cc7c4` | Context-switch counting walked through the `_NTO_TRACE_START` state dump, inflating the total ~10×. Dump now bounded by timestamp, guarded on `STATE_CREATE`. Also corrected an over-claim: absence of timestamp regressions in a *prefix* cannot support a statement about a whole trace. |
| `91675da` | `qnx_tracer.c` spawned `tracelogger -s 65536`, and `-s` is **seconds** — an 18.2-hour capture that only `slay` ever ended. No class filters, so kernel-call and interrupt traffic swamped the file. |
| `e95a499` | Application ring was 16,384 records; S4's storm dropped 4,926 (23 % of the trace), so S4 latency described only surviving events. Now 65,536 via a single define. Tracer stop switched to SIGINT with a settle loop so the `.kev` flushes. |
| `01ce387` | `experiment run S1:20` parsed with `atoi(id_str+1)`, silently discarding `:20`. Every sweep point ran at the hardcoded 80 %. |
| `b31e7bc` | Analysis JSON emitted ~2,500 activation records ≈ 2 MB. Also repeated one run-level context-switch counter on every row, implying per-task attribution that does not exist. |
| `bf1df1f` | `detail_note` had a trailing comma before `}`, so **every** `analysis_s1_*.json` was rejected by `json.load`. READY was stamped at enqueue rather than dispatch. |
| `7223e03` | `tools/sweep_table.py`, with an explicit warning when the curve is flat. |

**Resolved, not a defect:** activation counts appeared to be half the nominal
period. The sweep runs 10 s and `experiment run` runs 5 s. Both were correct.

---

## 5. The key finding — why the load sweep is flat

**This is correct QNX behaviour, not a bug.** Established from the QNX System
Architecture guide:

> Under `SCHED_FIFO`, same-priority threads are **not** preempted by each other.
> A thread runs until it voluntarily blocks. Round-robin (`SCHED_RR`) is the
> policy that timeslices equal priorities, at 4× the clock period.

The sweep runs the stressor at `SCHED_FIFO` priority **20** — the same as
`BRAKE_CTL`. Equal-priority FIFO threads serialise by blocking, never by
contending for the ready queue. So no ready-queue delay is generated, and
`ready_wait = 0.00 ms` with `preempt = 0.00 ms` across 20–95 % is the correct
measurement, not a blind spot.

**The fix:** make the stressor strictly higher priority than the workloads
(21 or 22). Then it genuinely preempts, `BRAKE_CTL` sits in the ready queue, and
`ready_wait` becomes measurable. Priorities above 63 need the
`PROCMGR_AID_PRIORITY` ability, which root has.

I characterised this flat curve as a bug four times before finding the real
cause. Each fix was real; each revealed the next layer.

### Structural limit on self-instrumentation

QNX's documented approach to scheduling latency:

> "To properly gauge the latency in triggering a response in user code, you
> should log the QNX Neutrino READY and RUNNING thread states."

Ready-queue delay is `RUNNING − READY` **in the kernel trace**, per thread. A
thread cannot observe its own dispatch delay from inside itself. The current
design enqueues a periodic task's own activation from within that task, so
release and dispatch are the same instant by construction — no amount of
relocating the READY stamp changes this.

The standard remedy is a **separate release thread** at lower priority per
periodic task: it stamps the ideal release time and enqueues the activation, so
the workload can genuinely measure dispatch delay. This is established practice
in real-time design, not a project-specific quirk.

Similarly, CPU utilization per QNX docs *"must contain at minimum, the Neutrino
RUNNING thread state"* — it is derived from kernel events, not application
timing.

---

## 6. Next phases

### Phase A — stressor priority above the workloads (15 min) · **do first**

Raise the pinned stressor from priority 20 to 22 in
`stress_generator_start_load_pinned` (`src/stress_generator.c:147`). Rerun
`--sweep`. Expect `p99` to climb and `ready_wait` to become non-zero.

One line. Highest value per minute remaining.

### Phase B — jitter stddev (20 min, no hardware)

Add `stddev_ms` and `max_minus_p50_ms` to `per_task_stats_t`
(`src/analyzer.h`), computed in `analyzer_per_task_stats` where the sorted array
already exists. Closes a requirement row your problem statement names
explicitly. Variance is visible in stddev even when percentiles do not move —
which is exactly this situation.

### Phase C — separate release thread (1–2 h)

Restructure the periodic path so a low-priority releaser stamps ideal release
time and enqueues, making `ready_wait` a genuine measurement. The architecturally
correct fix.

### Phase D — CPU utilization from kernel trace (1–2 h)

Derive from `RUNNING` thread states per the documented method, using the
`libtraceparser` path already fixed. Depends on that path yielding workload TIDs.

### Phase E — Qt JSON loader (2–3 h, no hardware)

Replace `MockProvider` with a real parser. Makes the README screenshots honest —
and the screenshots are what a judge sees first. Needs Phase D's CPU fields in
`analyzer_write_json`.

### Phase F — demo and documentation (1 h) · **non-negotiable**

Compliance matrix verdicts, screenshot set, demo script, timed rehearsal.

### Priority under time pressure

| Available | Do |
| --- | --- |
| 4 h+ | A → B → E → F |
| 2 h | A → B → F |
| 1 h | F only |

**Phase F decides how you are judged.** Verified results presented clearly beat
an unfinished feature.

### Cut without hesitation

CAN — already blocked; do not mention it. Pi 5 — claim Pi 4 only. Marker
jitter under load — nice, not required.

---

## 7. How to present this

**Lead with the six verified items.** GPIO on two boards with a photographed
LED, UART TX proven, workload timing exact, latency and deadline-miss
measurement on hardware, kernel trace capture and decoding after five real
defects, working CLI.

**State the flat curve as a finding, with its explanation.** "Under FIFO
equal-priority contention the platform shows no latency knee — which is the
documented semantics, and we can demonstrate it becomes a knee under true
preemption." That is stronger than a smooth curve you cannot defend, and it
demonstrates why safety-critical designs separate critical from non-critical
priorities.

**Do not quote** a context-switch count, a CPU utilization figure, or the Qt
dashboard's numbers. All three are unverified, and the Qt screenshots in
`README.md` are fabricated.

### Screenshots to capture

1. **LED blinking** — highest value, needs a phone camera mid-run
2. `gpio test` full output including `fsel before` / `fsel after`
3. `experiment run S0` complete report, all three task lines
4. `ls -l /tmp/schedulix.kev` — the 24 MB capture
5. `pidin info` — AArch64, 8.0.0, 4× A72
6. Sweep table once Phase A lands

Do not crop out `Help, we're not keeping up`. "Buffers overran on a 1500 MHz
A72, so counts are a lower bound" is a strong answer; hiding it invites the
question you did not want.

---

## 8. Operating notes

- **QNX shell does not search the current directory.** Always `./schedulix_can`.
  Bare-name invocation fails with `command not found` even when the file exists.
- **Deploy from `build/`, never `deploy/`.** Every binary in `deploy/` predates
  the UART and CLI work.
- **Upload:** `.\tools\serial_console.ps1 -UploadFile <path> -RemotePath /tmp/<name>`.
  Needs COM6 to itself; close any other terminal first.
- **`traceprinter`** is not on the board. Copy from
  `C:\Users\User\qnx800\target\qnx\aarch64le\usr\bin\traceprinter`. Requires a
  QNX licence key — a licence failure is a licensing wall, not a trace problem.
- **The IDE builds its own binary** with different flags (154,936 bytes observed
  vs 340,784 unstripped / 134,256 stripped). Verify a deploy with `grep -c` on a
  string unique to the intended build.
- `analysis_s1.json` **overwrites** every run. `--sweep` writes per-load
  filenames and is preferable.

### Uncommitted work in the tree

At the time of writing, `git status` shows **six modified files not from this
session**, including a ~392-line rewrite of `qnx_kernel_trace_parser.c` and a new
`tests/test_parser.c`. Review before committing — that work may resolve the
workload-TID problem, or may be in progress.

---

## 9. Where to read more

| Question | Document |
| --- | --- |
| Reproduce end to end | [`BRINGUP_GUIDE.md`](BRINGUP_GUIDE.md) |
| Requirement scorecard | [`PROBLEM_STATEMENT_COMPLIANCE.md`](PROBLEM_STATEMENT_COMPLIANCE.md) |
| SPI/CAN incident | [`INCIDENT_SPI_DRIVER.md`](INCIDENT_SPI_DRIVER.md) |
| This session in full detail | [`SESSION_LOG_2026-10-08.md`](SESSION_LOG_2026-10-08.md) |
| Hardware bring-up | [`BRINGUP_GUIDE.md`](BRINGUP_GUIDE.md) §12, [`SERIAL_CONSOLE_RUNBOOK.md`](SERIAL_CONSOLE_RUNBOOK.md) |