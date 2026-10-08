# Current State - Schedulix Backend (`schedulix_can`)

> **Procedure to reproduce this project end to end — from a clean clone, through
> the QNX cross-build, to CAN frames on the wire:
> [`docs/BRINGUP_GUIDE.md`](docs/BRINGUP_GUIDE.md).**
>
> Requirement-by-requirement scorecard:
> [`docs/PROBLEM_STATEMENT_COMPLIANCE.md`](docs/PROBLEM_STATEMENT_COMPLIANCE.md).
> The Phase 4 SPI incident and the ruled-out hypotheses:
> [`docs/INCIDENT_SPI_DRIVER.md`](docs/INCIDENT_SPI_DRIVER.md).
>
> Host test suite, no board needed: `python tools/run_all_tests.py` → **39/39**
> (**26/26 + 1 skip from PowerShell** — `test_spi_conf_edit.py` needs `awk`;
> run it from Git Bash for the full 39).
>
> **START HERE → [`docs/HANDOVER.md`](docs/HANDOVER.md).** What is verified, what
> is not, the twelve defects fixed this cycle, why the load sweep is flat (it is
> correct `SCHED_FIFO` behaviour, not a bug), and the prioritised next phases.
>
> **Latest session: [`docs/SESSION_LOG_2026-10-08.md`](docs/SESSION_LOG_2026-10-08.md).**
> The board was replaced and the **original SD card moved into it**. Verified
> live on 2026-10-08: instrumented kernel trace capture (**24 MB** — the
> context-switch claim is now substantiated), GPIO markers on a **second
> independent board** (0.27 %–0.48 % pulse error, microsecond agreement with
> board 1), and a clean **S0 baseline** (2,125 records, 0 dropped, 0 misses).
> Open: load sweep not yet run, so jitter has no meaningful number; CPU
> utilization is not measured.
>
> **QNX shell gotcha:** the current directory is not searched. Use
> `./schedulix_can`, never bare `schedulix_can`.

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
* **GPIO Timing Markers (`gpio_marker.c`)**: **Rewritten and verified on real hardware 2026-10-07.** Maps the BCM2711 register block with `mmap(MAP_PHYS|MAP_SHARED, NOFD, PROT_NOCACHE, 0xFE200000)` (there is no `/dev/mem` in this QNX image), writes `GPFSELn = 001` to switch each pin to output **before** driving `GPSET0`/`GPCLR0`, and reads levels back from `GPLEV`. Covers GPIO 0–53 including bank 1 at `+32`. Markers on pins 4 / 17 / 27 (header 7 / 11 / 13) were toggled 200 ms apart; all three moved fsel 0 → 1, all three read back `high=1 low=0 confirmed=YES`, and an **LED on header pin 11 was physically observed blinking**. See [`docs/SERIAL_CONSOLE_RUNBOOK.md`](docs/SERIAL_CONSOLE_RUNBOOK.md) and `VALIDATION_LOG.md` §4.4.
  * Markers now carry dedicated event types `TRACE_GPIO_MARKER_HIGH = 14` / `TRACE_GPIO_MARKER_LOW = 15`, **appended** so existing values 0–13 keep their meaning. They are **no longer discarded by the analyzer** — `analyzer.c` captures the edges into `gpio_marker_high_ns` / `gpio_marker_low_ns`, kept deliberately separate from `external_event_time` because a marker is self-generated, not an incoming trigger. They also survive `TRACE_MODE_EVENT_ONLY`.
  * **Open question:** GPIO 4 is the **TXD3** pin on this SoC. An earlier `gpio-bcm2711 get 4` reported it as `fsel=3 alt=4 func=TXD3`, while the verification run read `fsel=0` — different boots or BSP driver state. Forcing it to output could disturb a driver that claims it; confirm nothing on the image uses TXD3.
* **Analyzer (`analyzer.c`)**: Correlates binary trace records with raw QNX kernel events and attributes root cause triggers for misses.

## 3. Build System
* **QNX Backend**: Built via GNU Make with QNX SDP 8.0 compiler toolchain (`qcc` targeting `aarch64le` for Raspberry Pi 4/5). The SDP is installed at `C:\Users\User\qnx800` (`qcc` = gcc 12.2.0); `C:\QNX` contains only the IDE (`QNX Software Center`, `qnxmomenticside`), not the SDP. `ntoaarch64-strip` is available for shrinking binaries before board upload.
* **Qt Frontend**: CMake based with Qt 6.8.3.

## 4. Targets
* **QNX Target**: Raspberry Pi 4 QNX RTOS 8.0 (`aarch64le`) at IP address `192.168.10.5`. **The board was replaced on 2026-10-08** after the power blockage in [`docs/INCIDENT_SPI_DRIVER.md`](docs/INCIDENT_SPI_DRIVER.md); the **original SD card was moved into the new board and boots correctly**. Re-confirmed identity on the replacement: `CPU:AARCH64 Release:8.0.0`, 4× Cortex-A72 @1500 MHz, 8128 MB, 40 processes / 260 threads, 7,525 MB free, **instrumented kernel present** (proven by 24 MB of `tracelogger` output). A **passwordless root shell** is available over the serial console (`root@console:/#`, 115200 8N1).
  * **Not installed on this image:** `traceprinter`. It ships with the SDP at `C:\Users\User\qnx800\target\qnx\aarch64le\usr\bin\traceprinter` (191,544 B) and must be copied over. It requires a valid QNX licence key and exits without it — that is a licensing wall, not a trace problem.
  * **Three proven access paths.**
    1. **SSH** — working, verified 2026-10-07. The `-m` flag is **mandatory**:
       ```sh
       ssh -m hmac-sha2-256 qnxuser@192.168.10.5     # password: qnxuser
       ```
       Credentials: `qnxuser`/`qnxuser`, `root`/`root`. Root SSH was disabled (`PermitRootLogin no`) by default and has since been enabled. Stop typing the flag — create `C:\Users\<you>\.ssh\config`. **One directive per line:**
       ```
       Host qnxpi
          HostName 192.168.10.5
          User qnxuser
          MACs hmac-sha2-256
       ```
       Then `ssh qnxpi` is enough. `-m hmac-sha2-256` is required because the board's sshd lists
       broken `-etm` MAC algorithms first and the client picks the server's first preference by
       default (`Corrupted MAC on input`). It is **not** because the board lacks modern
       algorithms. For file transfer, `scp -o "MACs=hmac-sha2-256"` works for the same reason.
       **Trap:** writing `Host qnxpi 192.168.10.5` on one line is parsed as *two host patterns*,
       so SSH matches nothing useful and silently falls back to the local Windows username.
       Until the config is right, always use the explicit
       `ssh -m hmac-sha2-256 qnxuser@192.168.10.5`.
    2. **Serial console** via CH340 on COM6 — passwordless root at `root@console:/#`. No network
       involved. This is the **fallback whenever SSH is misconfigured**, and the only way in when
       Ethernet is absent.
    3. **Momentics IDE** — proven 2026-10-07; compiles, deploys over `qconn`, runs, and streams
       target `stdout` to the IDE console. Provides no root shell, and is unaffected by the SSH
       MAC issue.
* **Qt Target**: Windows host system running the desktop UI client.

## 5. Known Working Pieces
* Compilation clean for `aarch64le` (Pi) and `x86_64` targets.
* Workload scheduling and execution, trace collection in SHM, and QNX `tracelogger` privilege check/provenance capture.
* Analyzer root cause classification and delay attribution.
* Monotonic latency data sweep generation (loads 20% to 95% run cleanly on RPi 4, as verified by target log `qnx_logs_7.txt`).
* All Python host verification and regression unit tests pass (13/13 Phase 6 tests, 3/3 analyzer tests).
* **UART adapter, verified on hardware 2026-10-06**: `src/uart_adapter.c` opens `/dev/ser1` in physical mode and transmits for real — 21 bytes were observed on the host COM6 from `schedulix uart send`, ahead of the program's own `printf`.
* **Noun/verb CLI, verified on hardware 2026-10-06**: `schedulix uart status` and `schedulix uart send <data>` execute correctly on the board.
* **GPIO marker, verified on real hardware 2026-10-07**: `schedulix gpio test` prints `GPIO Availability: REAL PHYSICAL`, drives all three marker pins (4 / 17 / 27), and reports `confirmed=YES` and `valid=YES` for each against a 200 ms nominal. An LED on header pin 11 was **physically observed blinking**. See `VALIDATION_LOG.md` §4.4.
* **Momentics IDE as a deployment and log channel, verified 2026-10-07**: the IDE compiles, deploys to the board, runs the binary, and streams target `stdout` live to the IDE console. This is the deployment path that does not depend on the fragile serial upload.
* **SSH access, verified 2026-10-07**: `ssh -m hmac-sha2-256 qnxuser@192.168.10.5` logs in with password `qnxuser`; `su` with password `root` gives a root shell; `PermitRootLogin yes` has been set so `ssh root@…` works too. `scp -o "MACs=hmac-sha2-256"` works for uploads. See `VALIDATION_LOG.md` §4.7 and `docs/RUNBOOK.md`.
* **Tools caveat:** the shell scripts in `tools/` (`qnx_bringup_check.sh`, `qnx_ssh_diag.sh`, `qnx_can_install.sh`) are **QNX scripts**. They must be copied to the board and run there (`sh /tmp/<script>.sh`). Run from Git Bash or PowerShell on Windows they report the *Windows* machine's state and produce false gaps — MSYS maps `/tmp` to `C:\Users\User\AppData\Local\Temp`. Because that makes them useless as a first diagnostic when SSH is broken, the serial console is the route in.
* Host toolchain: QNX SDP 8.0 at `C:\Users\User\qnx800` (`qcc` gcc 12.2.0, `ntoaarch64-strip`). `C:\QNX` holds **only the IDE**. Full `aarch64le` rebuild is clean — zero errors, zero warnings.

## 6. Known Missing / Gap Items (Phases 2.11 - 2.15)
* **UART Device Integration (Phase 2.12)**: **Done.** `src/uart_adapter.c` is a complete QNX event reader/writer, not a stub: termios setup, 115200 8N1, `O_NONBLOCK`, and a simulated loopback fallback when the physical port cannot be opened. `uart_adapter_init()` defaults to `/dev/ser1`. Verified on live hardware on 2026-10-06 — `schedulix uart status` reports `UART Adapter Backend: physical` and `physical device /dev/ser1 configured at 115200 baud`; `open()`, `tcgetattr()` and `tcsetattr()` all succeed, and QNX permits a second open of `/dev/ser1` while the console owns it. TX is proven end-to-end: `schedulix uart send SCHEDULIX_TX_PROOF_42` produced 21 bytes on the host COM6 *before* the program's own `printf` output.
  * One outstanding defect: `src/uart_adapter.c:151` records a **transmit** as `TRACE_EXTERNAL_EVENT_RX`, so TX and RX are indistinguishable in the trace. A dedicated `TRACE_EXTERNAL_EVENT_TX` type is pending.
  * RX is **confounded, not broken**: on this board `/dev/ser1` **is** the serial console (driver `devc-serminiuart`, base `0xfe215000`, the BCM2715 mini-UART/AUX), so bytes sent from the host are consumed by the login shell. `uart receive` returned the console's own newline. RX remains unverified against an external peer.
* **CLI Structuring (Phase 2.14)**: **Done.** `src/main.c` implements a noun/verb parser (`schedulix <noun> <verb>`), not flat flags. Nouns are `status`, `workload`, `stress`, `trace`, `analyze`, `report`, `can`, `uart`, `gpio`, `experiment`, `help`. Verified live on the board — `schedulix uart status` and `schedulix uart send <data>` both work, and the binary advertises `uart status|start|send|receive` plus `gpio` and `can` groups. `--port <path>` and `--baud <rate>` override the UART defaults.
* **Deployable binary is stale**: the authoritative artefact is `build/aarch64le-debug/schedulix_can` (331,152 bytes, 2026-08-30 21:34, matching the newest source file). **Every binary in `deploy/` is stale** — a byte scan for `UART Adapter Backend`, `uart [status|start|send|receive]` and `gpio test` found **none** of them in `deploy/schedulix_can-aarch64le-debug` (154,384 B, 2026-08-29), `deploy/schedulix_can` (179,264 B, 2026-08-30) or `deploy/schedulix_can-x86_64` (175,360 B, 2026-08-30). All three strings are present in `build/aarch64le-debug/schedulix_can`. Deploy from `build/`, not `deploy/`.
* **Board `/tmp` holds only stale binaries and debris — clean it up.** `/tmp` is `/data/var/tmp` on `/dev/hd0t179` and **persists across reboots**, so it accumulates. Current contents, as inventoried over SSH as root after the reboots:
  * `schedulix` — 154,384 B. The **stale** `deploy/` binary.
  * `schedulix_can` — 150,520 B. **Unexplained** — matches no known build (the current build is ~332 KB). Flagged, provenance unknown.
  * **Neither is the binary that passed the GPIO LED verification.** That ran from the Momentics IDE, which deploys over `qconn` and does not use `/tmp`. Do not assume the verified build is in `/tmp`.
  * Junk debris from the failed serial upload experiments: zero-byte and 1-byte files `cal`, `d.bin`, `echo`, `hp.txt`, `p.bin`, `spd`, `schedulecho`, `scheecho`, `seecho` — the last three being mangled command fragments.
  * Also present: `spi.conf`, `spi2.conf`, `elvis1.ses`, `T3`, `keep_files`, `qnx_bringup_check.sh`.

  Delete the stale binaries and the debris, then deploy deliberately from `build/`.
* **Qt Integration**: The Qt app relies solely on `MockProvider` and does not yet parse the QNX backend's JSON reports.

## 7. Proposed Implementation Sequence
1. ~~**Rewrite `gpio_marker.c`**: replace the `/dev/mem` mmap with `MAP_PHYS | MAP_SHARED` + `NOFD` + `PROT_NOCACHE` on `0xfe200000`, write `GPFSELn` before any `GPSET0`/`GPCLR0`, implement real `gpio_marker_get()`, and cover the full GPIO 0–53 range (bank 1 at `+32`).~~ **Done, 2026-10-07, verified on hardware.**
2. **Fix the UART TX trace marker** at `src/uart_adapter.c:151`: add a dedicated `TRACE_EXTERNAL_EVENT_TX` type so TX and RX are distinguishable. **This is now the next item.** Append it at the **end** of `trace_event_type_t` for the same reason `TRACE_GPIO_MARKER_HIGH = 14` / `_LOW = 15` were appended — trace records are binary, and inserting mid-enum would renumber 0–13.
3. **Install the CAN driver (Phase 4)** — **BLOCKED at installer step 4/8 (2026-10-07).** Steps 1–3 are done and confirmed on the board: `can-mcp2515` (148,840 B) installed to `/system/bin/`, stock `spi.conf` backed up, `spi0/dev0` retuned for the MCP2515. Step 4 fails because **`spi-bcm2711` exits 1 immediately with no terminal output**. This is **not** a config problem — restoring the stock file reproduces the failure exactly, and the crystal is confirmed at 12 MHz (`EAS12.000`). Target-side investigation is on hold following a power blockage on the board; see [`docs/INCIDENT_SPI_DRIVER.md`](docs/INCIDENT_SPI_DRIVER.md). Three installer defects found and fixed in the process: a root check that could never pass, a wholesale `spi.conf` replacement that silently dropped `spi0/dev1` and the whole `spi3` bus (now a section-aware awk edit, regression-tested 13/13), and a backup that was re-stamped on every run. The QNX CAN DDK specifies a **target reboot** to apply `spi.conf`, not a driver restart; the installer now follows that.
4. **Write `qnx_can_adapter.c`**: real `/dev/can0` mailboxes and `CAN_DEVCTL_*` calls to replace the simulated adapter and the TCP injector. Depends on step 3. Note the existing `.stub` opens `/dev/can1`, which is wrong for a single module — transmit on `/dev/can0/tx2`, receive on `/dev/can0/rx0`.
5. **Verify UART RX against an external peer**: `/dev/ser1` doubles as the console, so RX must be proven on a second board or with the console detached.
6. **Add JSON output reporting and clean exit codes** to the noun/verb CLI.
7. **Write the Qt JSON loader** to replace `MockProvider`.

## 8. Risks
* **Kernel trace overruns are expected and not fixable by configuration.** `tracelogger` prints `Help, we're not keeping up` because the kernel fills event buffers faster than they can be written out. Raising buffer counts (`-b 512 -k 32`) does **not** help — this is write bandwidth on a 1500 MHz A72, not a misconfiguration. Dropping the process class (`-F3`) does reduce volume (23.8 MB → 10.3 MB). **Consequence: context-switch counts from these captures are a lower bound, not exact. Say so when reporting.** Do not conflate this with the application ring buffer's `dropped 0`, which measures something else entirely.
* **`tracelogger -T` does not exist.** The output file is `-f`. The SDP 8.0 syntax is `tracelogger [-acEPRruw] [-A attribute] [-b num] [-D seconds] [-d mode] [-F num] [-f file] [-k num] [-M -S size] [-n num] [-p addr] [-s num] [-v[v...]]`. Documentation that said `-T` was never runnable, which is why the context-switch claim went unverified for several sessions. Corrected in `docs/PROBLEM_STATEMENT_COMPLIANCE.md`.
* **`src/qnx_tracer.c:63` likely has a flag mix-up.** It spawns `tracelogger -f <path> -s 65536 -b 64`, but `-s` is **seconds**, so this asks for 18.2 hours of logging; `-b 64` is the buffer count and matches the default. Runs still terminate because `qnx_tracer_stop()` calls `slay tracelogger`. Not yet corrected.
* **Blocking UART operations**: *Mitigated.* `uart_adapter_init()` opens the port with `O_NONBLOCK` and sets `VMIN = 0` / `VTIME = 5` (a 0.5 s read timeout), so `read()` cannot starve real-time tasks. If throughput ever requires it, move I/O to a dedicated low-priority helper thread.
* **Physical Hardware Dependencies**: GPIO markers now have a **verified physical path** (LED observed blinking, `GPLEV` readback confirmed). Remaining caveats: marker wires on header pins 7/11/13 must be soldered **before** the RS485 CAN HAT is mounted, since the HAT covers them; and GPIO 4 is the **TXD3** pin, so confirm nothing on the image claims it before relying on it as a marker. Keep the software mocks enabled automatically when the physical mapping is missing — and treat any `MOCK` result as unproven rather than as a pass.
* **Marker timing under load**: the 2026-10-07 verification measured a deliberate 200 ms pulse in a test harness. Marker jitter while `BRAKE_CTL` / `ADAS_FUSION` / `DIAG_POLL` are actually running is **not yet characterised**.
* **SSH is brittle in a way that looks like a board fault**: a plain `ssh` fails with `Corrupted MAC on input` because the client picks the board's *first-listed* MAC, and the `-etm` implementations are broken. Always pass `-m hmac-sha2-256` (or set `MACs` in `~/.ssh/config`). If SSH is misconfigured the serial console is the fallback. Two other traps: root login was disabled by default, and the host has two Ethernet adapters (`192.168.10.1` for the board, `192.168.56.1`) — the second confuses routing.
* **Stale deploy artefacts**: shipping a binary from `deploy/` rather than `build/` will silently produce a program with no UART or CLI subcommands.
