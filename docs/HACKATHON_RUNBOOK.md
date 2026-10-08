# Schedulix — 48hr QNX Hackathon Runbook

**Team operating document. Work top to bottom. Do not skip ahead.**

| | |
| --- | --- |
| Event | 48hr QNX Hackathon |
| Project | Schedulix — Automotive RTOS Performance & Latency Analyzer |
| Target | QNX OS 8.0.0 (QSTI), Raspberry Pi 4, `aarch64le` |
| Repo | <https://github.com/Abdul99Aleem/Schedulix.git> |
| Branch | `hw/procurement-and-gap-plan` |
| Board IP | `192.168.10.5` — user `qnxuser` / `qnxuser`, `su` / `root` |
| Docs | [`BRINGUP_GUIDE.md`](BRINGUP_GUIDE.md) · [`PROBLEM_STATEMENT_COMPLIANCE.md`](PROBLEM_STATEMENT_COMPLIANCE.md) |

---

## The pitch — memorise this

> **"We don't just show that a deadline was missed. We show which task caused
> it, for how long, with hardware ground truth."**

Every other team will show a CPU utilisation graph. That is table stakes.
Our defensible, differentiating claim is **delay attribution correlated to a
physical GPIO marker** — and it is the only one we can physically prove on
stage.

---

## 1. Non-negotiables

These five must be true before we demo. Everything else is optional.

- [ ] **N1** Three workloads run on a real board (`BRAKE`, `ADAS`, `DIAG`)
- [ ] **N2** A real load sweep produces real latency numbers (S1, 20%→95%)
- [ ] **N3** At least one deadline miss is decomposed and attributed
- [ ] **N4** **GPIO LED blinks on header pin 11, on stage, while the trace shows the edge**
- [ ] **N5** The GUI shows **real analyzer output**, not `MockProvider` numbers

> **N4 is the money shot.** A physical LED flashing in lockstep with a trace
> record is what a judge remembers an hour later. Nothing else comes close.
>
> **N5 is the risk.** If we demo the GUI with fabricated numbers and a judge
> spots it, we lose more than if we never opened it. Fix it or hide it.

### What we do NOT claim

State these plainly. Do not let anyone be caught out.

| Claim | Status |
|---|---|
| CAN on a real bus | **Blocked** — SPI driver won't start (see §7) |
| Context switches | **Unverified** — settle it in §3 Step 1, then claim or drop |
| GUI graphs | **Fabricated until N5** |

---

## 2. Roles

Assign before you start. One person may hold several.

| Role | Owns |
|---|---|
| **HW** | Board, serial, wiring, GPIO LEDs, CAN, any physical fault |
| **BE** | Backend: build, workloads, trace, analyzer, experiments |
| **UI** | Qt GUI, JSON loader, screenshots, demo deck |
| **PRES** | Demo script, rehearsal, timing, judge Q&A, GitHub/README |

> Rule: **only PRES talks during the demo.** Everyone else operates silently.
> Decide who clicks, who narrates — never both.

---

## 3. PRE-FLIGHT — before the hackathon starts

Do this tonight. All of it is board time and it is the highest value per
minute available.

### Step 1 — Settle the context-switch claim (5 min)

This is the one claim with a provenance problem. Either it works or it
doesn't; both outcomes are fine, a fabrication is not.

```sh
which tracelogger || echo "NOT ON THIS IMAGE"
tracelogger --help 2>&1 | head -20        # learn the real flags

slay tracelogger 2>/dev/null
on -p 63 tracelogger -f /tmp/schedulix.kev -s 8192 -n 5 &
sleep 3

ls -lh /tmp/schedulix.kev                 # did it write anything?
pidin ar | grep tracelogger               # is it even alive?
schedulix trace decode /tmp/schedulix.kev # our libtraceparser decode
```

> ⚠️ **`-f`, not `-T`.** `tracelogger -T` is wrong and will fail. The correct
> flag comes from our own `src/qnx_tracer.c`.
>
> `tracelogger` needs root — use `on -p 63` or `su`.

**Decision:** file records → add to README as verified. File empty → delete
every mention of context switches from all docs.

### Step 2 — Capture the real metrics nobody has read (15 min)

Both metrics are already in the analyzer. Nobody has looked at the output on a
live board.

```sh
schedulix experiment run S4     # CAN burst / event storm
schedulix experiment run S1     # load sweep 20% -> 95%   <-- README-worthy
schedulix report
```

**Save this output.** It replaces fabricated screenshots.

### Step 3 — Run the verification harness (2 min)

```sh
scp -o "MACs=hmac-sha2-256" tools/qnx_bringup_check.sh qnxuser@192.168.10.5:/tmp/
ssh -m hmac-sha2-256 qnxuser@192.168.10.5
su                                    # password: root
sh /tmp/qnx_bringup_check.sh
```

Read-only; prints `[OK]` / `[GAP]` per subsystem. **Screenshot this for the
README.** Judges rarely see a team that ships its own verification tooling.

### Step 4 — Free CAN test (2 min, only while HAT is detached)

```sh
ls /dev/io-spi/spi0/
spi-bcm2711 &
ls -l /dev/io-spi/ 2>&1
```

SPI comes up clean → the old fault was the hand-wiring, CAN is recoverable.
Still `Exit 1` → see §7.

### Step 5 — Confirm the GUI problem (5 min, UI)

Open the Qt app. Confirm it is still on `MockProvider`. This is N5 and the
biggest remaining software gap.

### Step 6 — Commit and push

```bash
python tools/run_all_tests.py      # expect 39/39
git add -A && git commit -m "..." && git push origin hw/procurement-and-gap-plan
```

### ✅ Pre-flight gate

**Do not start the hackathon until Step 2 and 3 output is captured and
committed.** Real numbers on a real board are the foundation of the whole demo.
Everything after this is polish.

---

## 4. DAY 1 — morning: capture the baseline

**Goal: real numbers from a real board, committed to git.**

| # | Who | Task | Done |
|---|---|---|---|
| 1.1 | BE | Confirm board boots; `uname -a`, `pidin info` | ☐ |
| 1.2 | BE | Rebuild and redeploy binary | ☐ |
| 1.3 | BE | `schedulix status`, `uart status`, `gpio test` | ☐ |
| 1.4 | HW | **LED on pin 11 visibly blinks** — film it | ☐ |
| 1.5 | BE | `experiment run S1` → capture sweep curve | ☐ |
| 1.6 | BE | `experiment run S4` → capture report | ☐ |
| 1.7 | BE | Find one real deadline miss, decompose it | ☐ |
| 1.8 | PRES | Screenshot everything, commit | ☐ |

```sh
# 1.2 build (Windows cmd)
call C:\Users\<you>\qnx800\qnxsdp-env.bat
make

# 1.2 deploy
scp -o "MACs=hmac-sha2-256" build/aarch64le-debug/schedulix_can qnxuser@192.168.10.5:/tmp/
ssh -m hmac-sha2-256 qnxuser@192.168.10.5
su                                                  # root
cp /tmp/schedulix_can /tmp/ && chmod 755 /tmp/schedulix_can
```

### ☑️ Gate 1 — do not proceed without

- [ ] Workloads visibly running on the board
- [ ] S1 curve captured as a real artifact
- [ ] **LED blinking on pin 11, witnessed by HW**
- [ ] Everything committed and pushed

> If the LED does not blink, **stop and debug it now.** It is N4 and the
> whole demo leans on it. It worked before — this is a regression, not a
> new feature.

---

## 5. DAY 1 — afternoon: attempt CAN, timeboxed

> **Hard rule: 90 minutes maximum. Then stop, permanently.**
> CAN is one interface out of three. It must not eat the demo.

| # | Who | Task |
|---|---|---|
| 2.1 | HW | Seat the HAT on the 40-pin header — **no jumper wires** |
| 2.2 | HW | Upload driver + installer |
| 2.3 | HW | Run installer, reboot, re-run |
| 2.4 | HW | If it fails: `slog2info \| grep -i spi \| tail -30` |
| 2.5 | BE | While CAN runs: begin the JSON loader |

```sh
scp -o "MACs=hmac-sha2-256" third_party/can-mcp2515/aarch64le/bin/can-mcp2515 qnxuser@192.168.10.5:/tmp/
scp -o "MACs=hmac-sha2-256" tools/qnx_can_install.sh qnxuser@192.168.10.5:/tmp/
ssh -m hmac-sha2-256 root@192.168.10.5
sh /tmp/qnx_can_install.sh
```

Exit code **2** at step 4 is expected, not a fault — the QNX CAN DDK requires a
target **reboot** to apply `spi.conf`. Reboot and re-run; the script is
idempotent.

### ⏱️ 90-minute STOP

If `/dev/can0` is not there by the 90-minute mark, **abandon CAN permanently**
and record the reason. Move all remaining time to the GUI. Do not let a
blocked third-party driver sink the demo.

---

## 6. DAY 2 — the JSON loader, then demo prep

### Morning: close N5

| # | Who | Task | Done |
|---|---|---|---|
| 3.1 | UI | Dump real analyzer JSON (`analysis.json`, `manifest.json`) | ☐ |
| 3.2 | UI | Write the loader | ☐ |
| 3.3 | UI | Point Dashboard/Timeline/Jitter at it | ☐ |
| 3.4 | UI | **Delete or clearly label `MockProvider`** | ☐ |
| 3.5 | UI | Re-screenshot every README figure with real data | ☐ |

> Non-negotiable: no screen in the demo shows invented numbers. If the loader
> isn't done, hide the GUI and demo the CLI only. The CLI is the *mandatory*
> deliverable; the GUI is optional.

### Afternoon: demo prep

| # | Who | Task |
|---|---|---|
| 4.1 | ALL | Rehearse the full demo **3 times**, timed |
| 4.2 | PRES | Record a screen capture of the working demo as backup |
| 4.3 | PRES | Walk the judge Q&A bank (§9) |
| 4.4 | ALL | **Code freeze.** Only demo-critical fixes after this |
| 4.5 | PRES | Final `git push`, README polish, screenshots |

---

## 7. CAN risk register

| Symptom | Meaning | Action |
|---|---|---|
| `spi-bcm2711` exits 1, no output | Driver bails before config parsing | `slog2info \| grep -i spi \| tail -30` — **the one diagnostic never yet run** |
| `/dev/io-spi/spi0` missing | SPI never came up | Reboot; the DDK requires it, not a driver restart |
| Driver up, zero frames | **Wrong crystal** | Read the marking. `EAS12.000` = 12 MHz. Wrong value starts cleanly and receives nothing, silently |
| No `/dev/can0` at all | Driver didn't attach | `-D` for foreground logging |

**Ruled out — do not re-investigate:** the `spi.conf` rewrite (stock config
fails identically), the crystal (read 12 MHz off the board), the driver binary
(installs at the expected 148,840 bytes).

> **Root cause of the original fault, for the record:** board 1 stopped booting
> and was written off. The HAT's SPI signals had been hand-wired with jumper
> leads. The HAT's power LED went solid → blink → dark across three cycles
> while 5 V stayed good, with a cold SoC — consistent with a 3.3 V rail fault.
> **Inferred, not measured** (no multimeter). Board 2 boots the same SD card
> cleanly, which confirms the SD card was innocent. **Seat the HAT on the
> header. Never hand-wire it.**

---

## 8. Demo script (5 minutes)

PRES narrates. One operator clicks. Silence from everyone else.

```
0:00  PROBLEM
      "ASL-D braking and ASIL-B perception share one SoC. A CPU graph tells
       you a deadline blew. It does not tell you why."

0:30  LIVE BOARD
      schedulix status
      Three ECUs: BRAKE (prio 20, 10ms), ADAS (15, 20ms), DIAG (10, 50ms)
      "Mixed-criticality, exactly like a real vehicle."

1:00  LOAD SWEEP                                    [S1 — the curve]
      schedulix experiment run S1
      CPU climbs 20→95%. p99 bends. Deadline misses appear.
      "There's the knee."

2:00  ONE MISS, DECOMPOSED
      Response = ready-wait 4.2ms + preemption 1.8ms + execution 0.3ms
      "Preemption dominates. This is not a compute problem — it's contention."

2:30  ROOT CAUSE
      Culprit PID/TID/priority + the exact overlap window.
      Evidence level: CONFIRMED vs INFERRED.
      "We tell you not just that it missed, but who did it to it."

3:30  THE MONEY SHOT                               [N4 — GPIO marker]
      LED on header pin 11 pulses.
      Trace shows the edge at the same instant.
      "Software says this timestamp. The wire says it happened here.
       Our timeline is physically verified, not self-reported."

4:15  UART EVENT                                    [external trigger]
      Frame arrives → workload dispatches → appears in trace.
      "Our event path is real hardware, not a mock."

4:45  THE ASK
      What we'd do next.
```

**If CAN came up:** insert an 20-second CAN beat after 4:15.

---

## 9. Judge Q&A bank

| Question | Answer |
|---|---|
| **Why QNX for automotive RTOS?** | Drivers are **userspace processes** serving `io_msg` over a device namespace. A driver fault kills one process, not the kernel. Plus priority-based preemptive scheduling and message-passing IPC with bounded latency. |
| **Why not Linux + PREEMPT_RT?** | PREEMPT_RT bounds *scheduling* latency, but drivers are still in-kernel and a fault is fatal. QNX isolates that failure domain. |
| **Why shared memory instead of message passing?** | QNX's native IPC is `msg_send`/`msg_receive`. We deliberately chose a **bounded, lock-free, zero-copy MPSC ring** because message passing can allocate and can block. Deterministic over idiomatic. |
| **How do you trace?** | `tracelogger` writes binary kernel events to a `.kev` file; we decode with `libtraceparser`. On top of that, zero-allocation application instrumentation. |
| **How do you know your timings are right?** | GPIO markers via `mmap(MAP_PHYS)` on `0xFE200000`, cross-checked against `GPLEV` readback **and a physical LED**. Hardware ground truth. |
| **What about jitter?** | p50/p95/p99 per task from the same trace, plus the load-sweep curve showing where it degrades. |
| **What's your biggest gap?** | Answer it directly — see §1. Honesty here scores better than a bluff. |
| **Where's your CAN?** | Blocked by a target-side SPI issue. Driver builds from QNX source, installer is written and tested; the target driver won't start and we've documented what we ruled out. **Then move on — don't defend it.** |

---

## 10. Command reference card

```sh
# --- access (the -m flag is MANDATORY on this board) ---
ssh -m hmac-sha2-256 qnxuser@192.168.10.5     # password: qnxuser
su                                              # password: root
scp -o "MACs=hmac-sha2-256" <file> qnxuser@192.168.10.5:/tmp/

# --- build (Windows cmd) ---
call C:\Users\<you>\qnx800\qnxsdp-env.bat
make
ntoaarch64-strip build/aarch64le-debug/schedulix_can   # 333KB -> 130KB

# --- tests (no board needed) ---
python tools/run_all_tests.py       # 39/39 (38/39 on a fresh clone)

# --- Schedulix CLI ---
schedulix status
schedulix uart status | schedulix uart send <data>
schedulix gpio test                       # expect REAL PHYSICAL
schedulix workload start | stop
schedulix stress start --cpu 80
schedulix trace start | stop | status
schedulix trace decode /tmp/schedulix.kev
schedulix analyze | schedulix report
schedulix experiment list                # S0..S6
schedulix experiment run S1               # load sweep  <- key artifact
schedulix experiment run S4               # CAN burst

# --- QNX utilities worth showing a judge ---
pidin info                    # system report
pidin ar                      # all processes (drivers are processes!)
pidin fds                     # open file descriptors
pidin mem
ps -A -o pid,pri,args         # QNX exposes thread priority directly
on -p 63 <cmd>                # run at priority 63
slog2info | grep -i spi       # drivers log to slog, NOT the terminal
tracelogger -f FILE -s 65536 -b 64        # -f, NOT -T
```

---

## 11. Discipline rules

1. **Time-box everything.** Every block here has a limit. When it expires, move
   on and record the outcome.
2. **One task, one commit.** Message says what and why.
3. **Never claim unverified.** If §3 Step 1 produced nothing, context switches
   are not in the demo.
4. **Never show invented numbers.** No `MockProvider` on screen.
5. **PRES talks alone during the demo.**
6. **Code freeze on Day 2 afternoon.** Feature work stops.
7. **Screen recording always running** during rehearsal.

---

## 12. Sign-off

| Non-negotiable | Owner | Verified by | Done |
|---|---|---|---|
| N1 — workloads run on real board | BE | | ☐ |
| N2 — real load sweep captured | BE | | ☐ |
| N3 — miss decomposed and attributed | BE | | ☐ |
| N4 — **LED blinks on pin 11 on stage** | HW | | ☐ |
| N5 — GUI shows real data | UI | | ☐ |
| 3× rehearsal complete | PRES | | ☐ |
| Backup recording exists | PRES | | ☐ |
| Final push + README polished | ALL | | ☐ |

**Known gaps going in:** CAN on a real bus (blocked, §7) · context switches
(pending §3 Step 1) · Pi 5 GPIO base unverified · CAN requires a reboot to
apply `spi.conf`, not a driver restart.

Full detail: [`BRINGUP_GUIDE.md`](BRINGUP_GUIDE.md) ·
[`INCIDENT_SPI_DRIVER.md`](INCIDENT_SPI_DRIVER.md) ·
[`PROBLEM_STATEMENT_COMPLIANCE.md`](PROBLEM_STATEMENT_COMPLIANCE.md)