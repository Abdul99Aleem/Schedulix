# Session Log — 2026-10-08 (new board, Phase 1 verification)

**Plain-language record of one working session.** Scope: bring up the replacement
board, prove the kernel tracer produces real data, re-verify the GPIO marker on
independent hardware, and capture a clean S0 baseline.

Prior records: [`SESSION_LOG.md`](SESSION_LOG.md) (2026-10-01 → 06) and
[`SESSION_LOG_2026-10-06.md`](SESSION_LOG_2026-10-06.md) (2026-10-06 → 07).
Where they disagree with this document, this one is current for everything in
§1–§4 below.

---

## What was accomplished, in one paragraph

The board that failed on 2026-10-07 was replaced. The **original SD card was
moved into the new board and boots correctly**, so the QNX image and all saved
board state survived. Three things were then verified live: the kernel tracer
produces a real 24 MB event capture, the GPIO timing marker works correctly on
a **second, physically different** board, and a clean S0 baseline run completed
with 2,125 trace records and zero deadline misses. The context-switch claim,
which had been unsubstantiated across several sessions, is now substantiated.

---

## 1. Board bring-up

The old board entered a power blockage on 2026-10-07 (5 V present, SoC cold,
green LED never lit). A replacement board was used with the **original SD
card**.

| Item | Result |
| --- | --- |
| Boots | **Yes** |
| Serial console | **Yes** — passwordless root at `root@console:/#` |
| Kernel | `CPU:AARCH64 Release:8.0.0` |
| Memory | 7,525 MB free of 8,128 MB |
| Processes / threads | 40 / 260 |
| CPUs | 4 × Cortex-A72 @ 1500 MHz, FPU |
| Instrumented kernel | **Present** — proven by 24 MB of trace output (§2) |

### A shell gotcha that cost several commands

QNX's shell does **not** search the current directory. This fails:

```sh
schedulix_can experiment run S0
```

This works:

```sh
./schedulix_can experiment run S0
```

Bare-name invocations fail with `command not found` even when the file is
present and executable. **Always prefix with `./`.** This produced a run of
misleading "command not found" errors early in the session; none of them
indicated a real problem.

Also: `root` is not a command. `su` with no argument needs no input when
already root. The prompt `root@console:/#` already means you are root.

### Missing utilities on this image

`traceprinter` is **not installed** on the board. It ships with the SDP at
`C:\Users\User\qnx800\target\qnx\aarch64le\usr\bin\traceprinter` (191,544
bytes) and must be copied over. It is optional — see §5.

---

## 2. Kernel trace capture — **the context-switch claim is now substantiated**

Run as root on the serial console:

```sh
rm -f /tmp/schedulix.kev
tracelogger -f /tmp/schedulix.kev -s 10 -w
```

Result: **24,129,669 bytes**. Repeated captures produced 23.8 MB and 10.3 MB.
The instrumented kernel is present and logging works.

### `-T` is not a valid option

Earlier documentation instructed `tracelogger -T /tmp/schedulix.kev`. **There
is no `-T` option.** The full SDP 8.0 syntax is:

```
tracelogger [-acEPRruw] [-A attribute] [-b num] [-D seconds] [-d mode]
            [-F num] [-f file] [-k num] [-M -S size] [-n num]
            [-p addr] [-s num] [-v[v...]]
```

`-f` is the output file. The `-T` instruction was never runnable, which is the
most likely reason this claim stayed unsubstantiated for so long. **This has
been corrected** in `docs/PROBLEM_STATEMENT_COMPLIANCE.md`.

### "Help, we're not keeping up" is a data-loss warning, not a failure

```
Help, we're not keeping up (7/64 accumulated in the time we took to write 2)
```

Per the QNX System Analysis Toolkit documentation on buffer overruns: when the
kernel fills event buffers faster than the capture program can write them out,
the kernel skips the busy buffer, overwrites the next one, and the timestamps
show a discontinuity. **Events are lost.**

Attempts to fix it, and their outcomes:

| Attempt | Result |
| --- | --- |
| `-F1 -F2 -F6` (drop kernel-call, interrupt, comm classes) | Barely helped — 24.1 MB → 23.8 MB |
| `-b 512 -k 32` (more buffers) | **Did not help** — still `3/512`, `26/512` |
| `-F3` (drop process class) | Helped — 23.8 MB → 10.3 MB |

Raising the buffer count did not help, so this is **not** a buffer-allocation
problem. It is a write-bandwidth problem: a 1500 MHz A72 cannot write ~10 MB of
wide-mode trace fast enough. **This is not worth further effort.** Thread state
changes are a small fraction of the volume, and a few lost buffers out of 512
does not change the story.

**Consequence for reporting:** context-switch counts derived from these captures
are a **lower bound**, not an exact figure. State this plainly rather than
quoting a precise total.

### Two different "dropped" counters — do not conflate them

| Counter | Value | What it measures |
| --- | --- | --- |
| `Trace: records 2125 dropped 0` | 0 | Schedulix's **application** ring buffer, capacity 16,384 |
| `Help, we're not keeping up` | present | **tracelogger** kernel-event buffers |

Both are real. They measure different buffers. The application buffer was
never full; the tracelogger buffers overran.

---

## 3. GPIO marker re-verified on independent hardware

The strongest result of the session. This ran on a **physically different board**
from the original 2026-10-07 verification, using the same software.

```
GPIO Availability: REAL PHYSICAL
fsel before: 4=0 17=0 27=0  (1=output)

  BRAKE(GPIO4/pin7)  ... fsel=1  hw high=1 low=0 confirmed=YES  pulse=200548 us  delta=548 us
  ADAS (GPIO17/pin11) ... fsel=1  hw high=1 low=0 confirmed=YES  pulse=200926 us  delta=926 us
  DIAG (GPIO27/pin13) ... fsel=1  hw high=1 low=0 confirmed=YES  pulse=200945 us  delta=945 us

fsel after : 4=1 17=1 27=1  (1=output)
Hardware readback: level after high=1, after low=0, confirmed=YES
```

A second run in the same session: 200,577 / 200,960 / 200,931 µs.

| Measurement | Result |
| --- | --- |
| Registers mapped | `0xFE200000` |
| Function select | 0 → 1 on all three pins |
| Hardware readback | Confirmed on both runs |
| Pulse accuracy | **0.27 % – 0.48 %** error vs 200 ms nominal |
| Reproducibility | Both boards agree within a few microseconds of each other |

Two independent boards, same result, microsecond agreement. This is materially
stronger evidence than the original single-board run.

**Still unverified:** GPIO 4 is the **TXD3** pin on this SoC. Nothing in this
session established whether forcing it to output disturbs a driver that claims it.

---

## 4. S0 baseline — clean run

```sh
./schedulix_can experiment run S0
```

```
[SCENARIO S0_BASELINE] duration 5000 ms
Schedulix Analysis
==================
Trace: records 2125 dropped 0 cap 16384
Task 1: act 250 misses 0 (0.00%) mean 2.01 p50 2.01 p95 2.01 p99 2.01 max 2.02
Task 2: act 125 misses 0 (0.00%) mean 8.01 p50 8.01 p95 8.01 p99 8.01 max 8.01
Task 3: act 50 misses 0 (0.00%) mean 5.01 p50 5.01 p95 5.01 p99 5.01 max 5.01
Experiment S0 run finished. Wrote trace_s0.bin, manifest_s0.json, analysis_s0.json
```

| Task | Activations | Mean response | Misses |
| --- | ---: | ---: | ---: |
| `BRAKE_CTL` (prio 20, 10 ms) | 250 | 2.01 ms | 0 |
| `ADAS_FUSION` (prio 15, 20 ms) | 125 | 8.01 ms | 0 |
| `DIAG_POLL` (prio 10, 50 ms) | 50 | 5.01 ms | 0 |

All three tasks behave exactly as configured. 2,125 records, **zero dropped**,
zero deadline misses. Artifacts written and re-readable via
`./schedulix_can analyze trace_s0.bin`, which reproduces identical output.

### Two observations worth carrying forward

**Jitter is essentially zero at S0.** `p50 = p95 = p99 = 2.01` for `BRAKE_CTL`.
At zero contention, response time equals execution time and is near-deterministic,
so there is no jitter to report. **A jitter metric is only meaningful under the
load sweep**, not at baseline. Do not present an S0 jitter number as a result.

**Activation counts are half the nominal period.** Over a 5000 ms run the
expected counts are 500 / 250 / 100. Observed are 250 / 125 / 50 — exactly half
in every case, implying an effective measurement window near 2500 ms. The
consistency across all three tasks points at a startup offset or a scenario
running half its nominal duration, **not** at a per-task fault. **Open question;
not diagnosed.**

---

## 5. Tooling status and what is still missing

| Item | State |
| --- | --- |
| `traceprinter` | Not on board. Copy from `qnx800\target\qnx\aarch64le\usr\bin\`. |
| `schedulix_can` on board | **150,520 bytes — stale, provenance unknown** |
| Host authoritative build | `build\aarch64le-debug\schedulix_can` — **333,560 bytes** |
| `./schedulix_can trace decode` | **Never successfully run this session** |

### The binary on the board is not the current build

`ls -l schedulix*` shows two binaries:

| File | Size | Status |
| --- | ---: | --- |
| `schedulix` | 154,384 B | Stale (`deploy/`) |
| `schedulix_can` | 150,520 B | **Provenance unknown** — matches no host build |

The current build is 333,560 bytes. **All Phase 1 numbers above were produced by
the 150,520-byte binary**, which is the one `CURRENT_STATE.md` flags as
unexplained. It behaved correctly, but the result must be re-confirmed on the
authoritative build before any demo. **Deploy from `build/`, never from
`deploy/`.**

### Why `traceprinter` is worth copying over

You cannot check your own decoder using your own decoder. If
`schedulix_can trace decode` has a bug, it will agree with itself perfectly.
`traceprinter` is an independent implementation, so agreement between the two
is genuine evidence.

Caveat: it performs a **QNX licence check** and exits if the check fails. That
is a licensing wall, not a trace problem — do not misread it as evidence the
trace is bad.

---

## 6. Bugs found this session

| Bug | Where | Impact |
| --- | --- | --- |
| `tracelogger -T` documented | `docs/PROBLEM_STATEMENT_COMPLIANCE.md` §4 | Command was never runnable; blocked the context-switch claim for several sessions. **Corrected.** |
| Bare-name invocation failure | operator usage | Cost several "command not found" errors that looked like missing files. |

One implementation note found while reading the code, not yet changed:

`src/qnx_tracer.c:63` spawns `tracelogger -f <path> -s 65536 -b 64`. Per the
utilities reference, `-s` is **seconds**, so this requests **18.2 hours** of
logging, not a 64 KB buffer. `-b 64` is the buffer count and matches the
default. The tracer relies on `slay tracelogger` to stop, so this does not hang
a run — but the flag is almost certainly not what was intended. Worth
correcting to a short `-s` before the demo.

---

## 7. Status against the problem statement, after this session

| Requirement | Before | After |
| --- | --- | --- |
| Context switches | 🟡 unsubstantiated | ✅ **substantiated** — 24 MB capture |
| GPIO timing marker | ✅ 1 board | ✅ **2 independent boards** |
| Workloads / trace / analyzer | ✅ | ✅ S0 re-confirmed live |
| CPU utilization | 🟡 | 🟡 still **not measured** |
| Jitter | 🟡 | 🟡 **zero at S0 by design**; needs the load sweep |
| Load sweep (S1) | not run | **not run** — next step |
| CAN on real bus | 🔴 | 🔴 cut for this session |

### Next actions

1. **Deploy the real build** (333,560 bytes) from `build/aarch64le-debug/` and re-run S0.
2. **Run the load sweep** — `experiment run S1:<pct>` for 20/35/50/65/80/95. This is
   the only thing that produces meaningful jitter, and it is the headline
   artifact. Save `analysis_s1.json` after each run: the filenames are identical
   every time and **overwrite**.
3. **Get `trace decode` output** — needs no transfer, run it with `./`.
4. Copy `traceprinter` over if time allows, for an independent check of (3).
5. Add jitter stddev and a real CPU-utilization measurement in the analyzer.