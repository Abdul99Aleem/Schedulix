# Schedulix — Bring-Up Session Log

**Complete record of the hardware bring-up work, 2026-10-01 to 2026-10-06**

This document captures everything done so far: what was verified, what was built,
what was discovered, what failed and why, and what remains. It is intended to be
the single reference for resuming the work.

Related: [`HARDWARE_PROCUREMENT_PLAN.md`](HARDWARE_PROCUREMENT_PLAN.md) — parts list,
topology, software stack verification.

---

## 1. Scope

**Problem Statement 16 — Automotive RTOS Performance & Latency Analyzer**
QNX on Raspberry Pi 4, with CAN, UART and GPIO timing markers.

Two repositories are involved:

| Repo | Path | Branch | Purpose |
| --- | --- | --- | --- |
| Backend | `C:\Users\User\ide-8.0.3-workspace\schedulix_can` | `hw/procurement-and-gap-plan` | QNX C engine, CAN driver, tooling |
| Frontend | `C:\Users\User\Documents\schedulix_v1` | `hw/can-uart-gpio-preflight` | Qt 6 / QML trace viewer |

Neither repository had any git history at session start. Both were initialised.

---

## 2. Session timeline

| # | Phase | Outcome |
| --- | --- | --- |
| 1 | Initial repository survey | Read the full backend. Found CAN simulated, GPIO broken, UART complete |
| 2 | Frontend preflight harness | Built `hw/preflight.py`; Qt builds clean 56/56 |
| 3 | **Software stack verification** | **`libcan` and `libtraceparser` found; `can-mcp2515` cloned and built for aarch64le** |
| 4 | Hardware procurement planning | Three-board topology, BOM, substitutes |
| 5 | CAN board selection | Waveshare unavailable → routed to MCP2515 modules |
| 6 | Inventory audit | Found power supply gap; precise jumper counts; pin conflict check |
| 7 | Board bring-up | Connected to QNX Pi 4 at 192.168.10.5; full audit captured |
| 8 | CAN installer | Written for the real board; SPI mode bug identified |

---

## 3. Phase 1 — Codebase survey

### What was found working

| Component | File | Status |
| --- | --- | --- |
| ECU workloads | `src/workload.c` | BRAKE_CTL (prio 20, 10 ms), ADAS_FUSION (prio 15, 20 ms), DIAG_POLL (prio 10, 50 ms) |
| MPSC ring buffer | `src/trace_collector.c` | Lock-free, 48-byte records, zero `malloc` in hot path |
| QNX kernel trace | `src/qnx_tracer.c` | Spawns `tracelogger`, writes `/tmp/schedulix.kev` |
| `libtraceparser` decode | `src/qnx_kernel_trace_parser.c` | Real `.kev` parsing, 64-bit timestamp rollover |
| Delay attribution | `src/analyzer.c` | `CONFIRMED` / `INFERRED` evidence levels |
| Stress matrix | `src/stress_scenarios.c` | S0–S6 scenarios |
| UART | `src/uart_adapter.c` | **Complete** — termios, 115200 8N1, non-blocking, loopback fallback |

### Three defects identified

**Defect 1 — CAN has no hardware path.**

`src/qnx_can_adapter.c` in its entirety:

```c
/* This file is intentionally NOT compiled by default.
 * Rename qnx_can_adapter.c.stub -> qnx_can_adapter.c and adjust Makefile
 * when real hardware integration begins. See .stub for template.
 */
```

`src/simulated_can_adapter.c` returns five hardcoded frames:

```c
static const can_frame_t k_sim_frames[] = {
    { .id = 0x100, .dlc = 1, .data = {0x01} },
    { .id = 0x200, .dlc = 1, .data = {0x01} },
    { .id = 0x300, .dlc = 1, .data = {0x01} },
    { .id = 0x999, .dlc = 1, .data = {0x01} }, /* UNKNOWN */
    { .id = 0x100, .dlc = 3, .data = {0xAA, 0xBB, 0xCC} },
};
```

`src/can_injector.c` is a **TCP** server, not CAN. It works but does not satisfy
"external CAN events generate workload".

**Defect 2 — GPIO never configures pin mode.**

`src/gpio_marker.c:77`, with the bug acknowledged in a comment:

```c
/* BCM2711: GPSET0 offset 7, GPCLR0 offset 10, GPLEV etc. Simplified */
/* Real BSP would configure GPFSEL first — stub sets only */
volatile uint32_t *set = g_gpio_base + 7;
volatile uint32_t *clr = g_gpio_base + 10;
```

Register offsets are **correct** (`GPSET0` = byte 0x1C = word 7; `GPCLR0` = byte 0x28 =
word 10). The fatal problem is that `GPFSEL` is never written. At reset all `GPFSELn`
fields are `000` = input, so `GPSET0` writes produce **no electrical output**.

Additional issues in the same file:
- `gpio_marker_get()` returns `-1` unconditionally — no readback
- Only `GPIO0–31` handled; bank 1 registers at `+32` not implemented
- `gpio_marker_validate()` compares software timestamps to software timestamps when
  mocked — vacuous

**Defect 3 — UART TX trace marker is wrong.**

`src/uart_adapter.c:151` records `TRACE_EXTERNAL_EVENT_RX` for a **transmit**:

```c
int uart_adapter_trace_tx(uint32_t act_id, uint32_t corr_id, uint8_t byte) {
    return trace_collector_record_simple(TRACE_EXTERNAL_EVENT_RX, 0xFFFFu, act_id, corr_id, byte, 2);
}
```

TX and RX are indistinguishable in the trace.

### Two additional gaps

**Invalid validation claim.** `VALIDATION_LOG.md` TEST 6 records GPIO as PASS, but
`docs/logs/qnx_logs_7.txt` shows:

```
[GPIO] no HW, mock mode (validation uses sw timestamps)
```

The test compares software timestamps against software timestamps. It proves nothing
physical.

**Qt frontend has no data path.** In `schedulix_v1`, a search for `QJson`,
`analysis_` or `QFile` across `src/` returns nothing. `src/models/MockProvider.cpp`
hardcodes every metric:

```cpp
void MockProvider::populateSystemMetrics(SystemMetrics *metrics)
void MockProvider::populateTaskMetrics(TaskMetrics *metrics)
void MockProvider::populateRootCauseModel(RootCauseModel *model)
void MockProvider::populateTimelineModel(TimelineModel *model)
void MockProvider::populateJitterModel(JitterModel *model)
void MockProvider::populateExperimentModel(ExperimentModel *model)
```

The GUI renders pre-cooked values. It is not connected to backend output.

---

## 4. Phase 2 — Frontend preflight harness

Created in `schedulix_v1/hw/`:

| File | Lines | Purpose |
| --- | --- | --- |
| `preflight.py` | 322 | Library/tool/port inventory, self-tests, target probe |
| `requirements-host.txt` | 24 | Pinned host dependencies |
| `README.md` | 63 | Usage |

### First run — before installing anything

```
13 pass  6 warn  3 fail
```

Installed during this phase: `pyserial` 3.5, `python-can` 4.6.1. Both passed
functional self-tests:

- **Virtual CAN round-trip** — `0x123` payload `01 02 03` transmitted and received
- **UART framing** — `loop://` echo returned `b'CAN?'`

The 3 failures were `RPi.GPIO`, `gpiozero`, `spidev` — all Linux-only, correctly
skipped on the Windows host via `sys_platform` markers.

### Qt build verification

Configured with CMake + Ninja against Qt 6.8.3 MinGW:

```
[56/56] Linking CXX executable appschedulix_v1.exe
BUILD OK
```

### Noise suppression added

`python-can` probes every vendor DLL on import and floods stderr. A `quiet_can_probe()`
context manager silences the `can` logger during `detect_available_configs()` only,
so real errors still surface.

---

## 5. Phase 3 — Software stack verification

This was the critical phase. The question was: **if QNX lacks a driver, will the
hardware be wasted?**

### 5.1 Findings — everything needed is present

| Component | Status | Location / proof |
| --- | --- | --- |
| `libcan.a` (aarch64le) | **PRESENT** | `qnx800/target/qnx/aarch64le/usr/lib/libcan.a` |
| `libcan.h` | **PRESENT** | `qnx800/target/qnx/usr/include/hw/libcan.h` |
| `can_dcmd.h` | **PRESENT** | `qnx800/target/qnx/usr/include/sys/can_dcmd.h` |
| CAN compile + link | **WORKS** | Test program built and linked `-lcan` — see below |
| `libtraceparser.a` | **PRESENT** | `qnx800/target/qnx/aarch64le/usr/lib/libtraceparser.a` |
| `sys/traceparser.h` | **PRESENT** | `qnx800/target/qnx/usr/include/sys/traceparser.h` |
| `sys/trace.h` | **PRESENT** | `qnx800/target/qnx/usr/include/sys/trace.h` |
| `can-mcp2515` driver | **BUILDS** | Cloned from public GitLab, built for aarch64le |
| BSP (`rpi4`) | **NOT INSTALLED locally** | Absent from host; available in QNX Software Center |

### 5.2 libcan compile and link test

Written to prove the CAN DDK is usable, not merely present:

```c
#include <hw/libcan.h>
#include <sys/can_dcmd.h>
#include <stdio.h>
int main(void){
    CAN_MSG m;
    printf("sizeof CAN_MSG=%zu\n", sizeof(CAN_MSG));
    m.mid = 0x123;
    m.len = 3;
    printf("libcan + can_dcmd headers OK\n");
    return 0;
}
```

Result:

```
qcc -Vgcc_ntoaarch64le -c t.c -o t.o          cc EXIT=0
qcc -Vgcc_ntoaarch64le -o t.exe t.o \
    -lsocket -Wl,-Bstatic -lcan -Wl,-Bdynamic  ld EXIT=0
```

**Note:** `libcan.h` lives under `hw/`, not `sys/`. Use `#include <hw/libcan.h>`.

### 5.3 can-mcp2515 driver — cloned and built

Highest-risk unknown. QNX publishes source, not a prebuilt binary.

```bash
git clone https://gitlab.com/qnx/projects/drivers/can-mcp2515.git
cd can-mcp2515
. ~/qnx800/qnxsdp-env.sh
export QCONF_OVERRIDE=$(pwd)/qconf-override.mk
make INSTALL_ROOT_nto=$(pwd)/build USE_INSTALL_ROOT=1 hinstall install
```

Build output:

```
build/aarch64le/bin/can-mcp2515       148,840 bytes
build/aarch64le/bin/can-mcp2515_g     332,448 bytes
build/aarch64le/lib/libmcp2515.a       40,216 bytes

ELF 64-bit LSB pie executable, ARM aarch64, dynamically linked,
interpreter /usr/lib/ldqnx-64.so.2
```

Links against `-lmcp2515 -lcan -lsecpol -lslog2`. Exit code 0.

Source commit: `0fd11af` (`main` branch).

Driver README states:

> **NOTE** At this time, the driver only works on the Raspberry Pi 4 platform.
> It was developed for the Waveshare 2-CH CAN HAT

This settled the board question — Pi 4 is correct and the driver is chip-specific
(MCP2515), not board-specific.

### 5.4 The driver solved the GPIO question

`can-mcp2515/driver/rpi4.c` maps BCM2711 GPIO registers directly. This is the
pattern `gpio_marker.c` must adopt:

```c
/* from can-mcp2515/driver/rpi4.c — VERIFIED WORKING on QNX + Pi 4 */
#define BCM2711_GPIO_BASE   0xfe200000
#define BCM2711_GPIO_FSEL0  (0x00)
#define BCM2711_GPIO_SET0   (0x1c)
#define BCM2711_GPIO_CLR0   (0x28)
#define BCM2711_GPIO_LEV0   (0x34)

data->regs = mmap(0, __PAGESIZE,
                  PROT_NOCACHE | PROT_READ | PROT_WRITE,
                  MAP_PHYS | MAP_SHARED, NOFD,
                  BCM2711_GPIO_BASE);
```

Three corrections this revealed against `gpio_marker.c`:

| Current code | Correct (per official driver) |
| --- | --- |
| `open("/dev/mem")` then `mmap` | `mmap` with `MAP_PHYS \| NOFD` |
| `PROT_READ \| PROT_WRITE` | `PROT_NOCACHE \| PROT_READ \| PROT_WRITE` |
| `GPFSEL` never written | Must be written before `GPSET0`/`GPCLR0` |

Driver also confirms valid GPIO range on Pi 4 is **0–53**.

### 5.5 Driver command-line interface

Extracted from `driver/command_line.c` and `driver/can-mcp2515.use`:

**Required:**
| Flag | Meaning |
| --- | --- |
| `-s, --spi <path>` | SPI device node, e.g. `/dev/io-spi/spi0/dev0` |
| `-c, --clock <Hz>` | Crystal frequency, **must be 1,000,000–40,000,000** |
| `-g, --gpio <gpio>` | MCP2515 INT pin |

**Optional:** `-3/--triple-sample`, `-B/--bps` (default 500000), `-D/--debug`,
`-M/--msg-queue-size` (default 256), `--mid=sid\|eid\|ext`, `-m/--mode=io\|raw`,
`-u/--unit`, `-v/--verbose`, `-W/--wait-queue-size`.

**MID semantics:** `--mid=sid` (default) assumes 11-bit IDs; `eid` assumes 29-bit;
`ext` puts SIDs/EIDs in one space with bit 30 as the discriminator. With `--mid=eid`,
mailbox MIDs for `rx0, rx1, tx2, tx3, tx4` become `0,1,2,3,4`.

### 5.6 GPIO utility interface

From BSP documentation:

```sh
gpio-bcm2711 get|set|funcs [gpio number] [options]
```

Options for `set`: `ip` input, `op` output, `a0`–`a5` alternate function, `pu`/`pd`/`pn`
pull, `dh`/`dl` drive high/low.

```sh
gpio-bcm2711 set 20 op pn dh    # pin 20 output, no pull, drive high
gpio-bcm2711 set 20 op pn dl    # drive low
```

---

## 6. Phase 4–6 — Hardware planning

Full detail in [`HARDWARE_PROCUREMENT_PLAN.md`](HARDWARE_PROCUREMENT_PLAN.md).
Key outcomes:

### Three-board topology

| Board | OS | Role |
| --- | --- | --- |
| Pi 1 | QNX 8.0 | Analyzer — workloads, trace collection, RCA, CAN receive, GPIO markers, UART |
| Pi 2 | Raspberry Pi OS | CAN ECU simulator |
| Pi 3 | Raspberry Pi OS | Logic analyzer capture console, UART injection, monitoring |

Benefits over one board: genuine external CAN node, real multi-node arbitration,
independent hardware measurement, no perturbation of the board under test, and
failure isolation (a generator crash costs the run, not the data).

### CAN board selection — why not the obvious option

The **Waveshare 2-CH CAN HAT** was the target board. It became unavailable.

| | Waveshare 2-CH | Generic MCP2515 module |
| --- | --- | --- |
| Controller | 2× MCP2515 | 1× MCP2515 |
| Transceiver | SN65HVD230 | **TJA1050** |
| Safe on Pi 3.3 V GPIO | Yes | **No** |
| Channels | 2 | 1 |
| Crystal | 16 MHz | usually 8 MHz |
| Price | ₹1,860–1,949 | ₹200–500 |

**The TJA1050 hazard.** TJA1050 is a 5 V-only transceiver with no separate VIO pin;
its RXD output swings to VCC. Powering such a module from the Pi's 5 V pin puts 5 V on
MISO, a GPIO input. The Pi is not 5 V tolerant. Fixes: cut the PCB trace feeding the
transceiver, or replace with SN65HVD230.

Transceiver safety comparison:

| Transceiver | Why safe |
| --- | --- |
| SN65HVD230 | Runs entirely at 3.3 V |
| MCP2551 | Has a separate VIO pin |
| TJA1050 | No VIO pin. Logic follows VCC. **Unsafe** |

### Inventory gap found during audit

The user's original BOM listed **one** power supply. Three boards need three.

| # | Item | Spec | Qty | Cost |
| --- | --- | --- | --- | --- |
| 1 | USB-C power supply | **5.1 V / 3 A** | **3** | ₹1,050 |
| 2 | CAN HAT / module | MCP2515 + SN65HVD230 | 2 | ₹800 |
| 3 | SN65HVD230 | DIP-8/SOIC-8 spare | 2 | ₹80 |
| 4 | LAN cable Cat5e | 1–2 m | **3** | ₹450 |
| 5 | Jumper wire set | **120-piece assorted** | 1 | ₹200 |
| 6 | Logic analyzer | 8-ch, ≥24 MHz | 1 | ₹3,000 |
| 7 | Breadboard | half-size | 2 | ₹240 |
| 8 | USB-to-UART | **3.3 V TTL** | 2 | ₹600 |
| 9 | microSD | 16 GB A2 | 2 | ₹1,000 |
| 10 | LED + 330 Ω + 120 Ω | | 6 / 6 / 4 | ₹130 |
| 11 | Pi 4 case with fan | minimum Pi 1 | 1 | ₹700 |
| | **TOTAL** | | | **≈ ₹8,050** |

Two corrections found during audit:
- **Buy 3 LAN cables, not 2** — the spare's twisted pair becomes the CAN_H/CAN_L link
- **Buy a 120-piece jumper set, not small ones** — 25 × F-F required

### Precise jumper inventory

| Purpose | F-F | F-M | Total |
| --- | ---: | ---: | ---: |
| Pi 1 — MCP2515 module | 8 | 0 | 8 |
| Pi 1 — GPIO markers + GND | 1 | 3 | 4 |
| Pi 1 — UART loopback | 3 | 0 | 3 |
| Pi 2 — MCP2515 module | 8 | 0 | 8 |
| **Subtotal** | **20** | **3** | **23** |
| Spares | 5 | 3 | 8 |
| **TOTAL** | **25** | **6** | **31** |

F-F dominates because both Pi header pins and module pins are male.

### Verified pin allocation — no conflicts

| Pin | BCM | Used by |
| ---: | ---: | --- |
| 1 | — | CAN module VCC |
| 2 | — | CAN module 5 V feed |
| 6 | — | CAN GND + UART loopback GND *(shared, intended)* |
| 7 | 4 | BRAKE GPIO marker |
| 8 | 14 | UART TXD |
| 9 | — | Logic analyzer GND |
| 10 | 15 | UART RXD |
| 11 | 17 | ADAS GPIO marker |
| 13 | 27 | DIAG GPIO marker |
| 19 | 10 | CAN MOSI |
| 21 | 9 | CAN MISO |
| 22 | 25 | CAN INT |
| 23 | 11 | CAN SCK |
| 24 | 8 | CAN CS |

Correction made during this phase: UART is **pin 8 TX / pin 10 RX**, not pin 11
(pin 11 is GPIO 17, the ADAS marker).

---

## 7. Phase 7 — Board bring-up

### Network situation

The board in hand is **not** the `192.168.10.2` board from earlier logs. Current
target is **192.168.10.5**.

Laptop is on WiFi `192.168.29.129` with Ethernet disconnected. The user chose to avoid
running both simultaneously, so the workflow became: **plug Ethernet → run one
script → unplug → report**.

### Obstacle 1 — Windows OpenSSH refused QNX sshd

First attempt used `tools/bringup.ps1` + `ssh`. Failure:

```
Corrupted MAC on input.
ssh_dispatch_run_fatal: Connection to 192.168.10.5 port 22:
  message authentication code incorrect
```

Diagnosis: **the board was alive** — port 22 accepted a connection and began an SSH
handshake. Windows OpenSSH 9.x had dropped SHA-1 MACs, `diffie-hellman-group1` and
CBC ciphers by default, and QNX sshd offers some of them.

The ICMP failure was a red herring — QNX images commonly drop or ignore ICMP.

### Obstacle 2 — OpenSSH option sets did not fix it

Updated `bringup.ps1` to try 5 algorithm sets × 2 users. Still failed.

### Resolution — paramiko

`paramiko` 5.0.0 installed via pip. Its preferred algorithm lists include the legacy
set QNX needs:

```
_preferred_macs    : hmac-md5, hmac-md5-96, hmac-sha1, hmac-sha1-96,
                     hmac-sha2-256, hmac-sha2-256-etm@openssh.com,
                     hmac-sha2-512, hmac-sha2-512-etm@openssh.com
_preferred_ciphers : 3des-cbc, aes128-cbc, aes128-ctr, aes192-cbc,
                     aes256-cbc, aes256-ctr, aes128-gcm@openssh.com, ...
_preferred_kex     : curve25519-sha256@libssh.org, diffie-hellman-group-exchange-sha256,
                     diffie-hellman-group14-sha256, diffie-hellman-group16-sha512,
                     ecdh-sha2-nistp256/384/521
```

Limitation noted: `paramiko` does **not** support `ssh-dss` (DSA).

### Probe result — connected

```
  fail     user=qnxuser  pwd=none  algo=paramiko default
             SSHException: No authentication methods available
  SUCCESS  user=qnxuser  pwd=qnxuser  algo=paramiko default
```

The **default** algorithm set worked. The earlier failures were a Windows OpenSSH
client issue, not a board problem.

Board identity:

```
CPU:AARCH64 Release:8.0.0  FreeMem:7487MB/8128MB
Processor1..4: Cortex-A72 1500MHz FPU
QNX qnxpi 8.0.0 2025/07/30-19:17:34EDT RaspberryPi4B aarch64le
uid=1000(qnxuser) groups=1000(qnxuser),36(screen),521(sensor),
     522(gpio),523(mbox),524(spi)
```

Interpretation:

| Finding | Meaning |
| --- | --- |
| Login `qnxuser`/`qnxuser` | **Quick Start Target Image (QSTI)**, not the BSP reference image |
| QNX 8.0.0, build 2025/07/30 | Matches validated SDP 8.0 platform |
| 4 × Cortex-A72 @ 1500 MHz | Correct SoC |
| 8128 MB total | **8 GB model** — not the unsupported 1 GB variant |
| Groups 522/523/524 | gpio, mbox and spi groups **exist in this image** |

### Audit results — full capture

Recorded in `bringup_report.txt` (7,617 bytes).

**Working:**

| Item | Evidence |
| --- | --- |
| UART device | `/dev/ser1` exists |
| SPI driver | `spi-bcm2711` running, PID 487449 |
| SPI nodes | `/dev/io-spi/spi0/dev0`, `dev1` both present, group `spi` |
| GPIO utility | `/system/bin/gpio-bcm2711` present (28,168 bytes) |
| mbox utility | `/system/bin/mbox-bcm2711` present |
| CAN utility | `/system/bin/canctl` present |
| Kernel tracer | `/system/bin/tracelogger` present and running |
| Storage | `/system` 2.9 G with 2.4 G free; `/data` 26 G |

**Blocker A — the stock spi.conf would have broken CAN.**

The QSTI image ships `spi0/dev0` configured as:

```
clock_rate=5000000
cpha=1
cpol=0
word_width=32
```

The MCP2515 requires (per the QNX CAN DDK reference configuration):

```
clock_rate=10000000
cpha=0
cpol=0
word_width=8
```

Wrong `cpha` and `word_width` means the MCP2515 would never respond. This was
invisible until the audit compared the board's live config against the driver's
documented requirement.

The stock config also defines `spi0/dev1` (`cpha=0 cpol=1 word_width=32`) and a whole
`spi3` bus. Rewritten config preserves `spi3` and defines only `dev0`.

**Blocker B — no `/dev/mem`.**

```
ls: /dev/mem: No such file or directory
```

`gpio_marker.c` calls `open("/dev/mem")`. That will always fail on QNX. Confirms the
`gpio-bcm2711` utility is the correct path.

**Blocker C — root filesystem read-only, no sudo.**

```
ifs          11M  11M  0  100%  /
sudo: command not found
```

`/` is 100% full and read-only. Installing into `/system/bin` requires root, and there
is no `sudo`. Also: `/dev/shmem` is 0 bytes, so shared-memory ring buffers cannot go
there — they must use `/tmp` or an allocated region.

**Blocker D — no default gateway.**

```
[WARN] no default gateway
192.168.10.5 on /24, link is 100baseTX full-duplex
```

Fine for direct laptop-to-board Ethernet. Required later for the three-board topology.

---

## 8. Phase 8 — CAN driver installer

### Driver deployed

```
third_party/can-mcp2515/aarch64le/bin/can-mcp2515       148,840 bytes
third_party/can-mcp2515/aarch64le/bin/can-mcp2515_g     332,448 bytes
third_party/can-mcp2515/lib-public/mcp2515/*.h                       8 headers
```

Originally built in `%TEMP%\opencode\can-mcp2515`; copied into the repository because
Temp is volatile.

### Tools created

| File | Purpose |
| --- | --- |
| `tools/qnx_ssh.py` | Paramiko client; probes algorithm sets, uploads, runs |
| `tools/qnx_audit.py` | Runs the audit in blocks, tees to `bringup_report.txt` |
| `tools/qnx_can_install.py` | Uploads driver, finds root, runs installer, dumps `slog2info` |
| `tools/qnx_bringup_check.sh` | On-target audit shell script (7 blocks) |
| `tools/qnx_can_install.sh` | On-target installer |
| `tools/spi.conf.mcp2515` | MCP2515 SPI configuration template |
| `tools/bringup.ps1` / `.cmd` | Windows OpenSSH launcher (superseded by paramiko) |

### Installer behaviour

`qnx_can_install.sh` in 8 steps: verify prerequisites → install binary → back up and
rewrite `spi.conf` → restart `spi-bcm2711` → stop any existing CAN driver → start
`can-mcp2515` → verify `/dev/can0` → set catch-all filter with `canctl`.

Configurable at the top of the script:

```sh
CLOCK_HZ=8000000     # MCP2515 crystal
BPS=500000           # CAN bitrate
INT_GPIO=25          # MCP2515 INT
SPI_DEV=/dev/io-spi/spi0/dev0
```

On failure it dumps `slog2info | tail -30` and lists the four likely causes.

### Root escalation problem

The installer requires root. The Python wrapper tries an empty password first (common
for QSTI), then `root`, `toor`, `qnxuser`. If none work it prints the serial console
recipe and the reflash option.

---

## 9. Phase 9 — Hardware acquired

The user obtained:

| Item | Status |
| --- | --- |
| 2× **Waveshare RS485 CAN HAT** | Acquired |
| Logic analyzer | Acquired |
| Breadboard | Acquired |
| Jumper wires | Acquired |
| LEDs | Acquired |
| USB-TTL UART adapter | Acquired |
| 6× 330 Ω resistors | Acquired |
| 2× 120 Ω resistors | Acquired |

### RS485 CAN HAT suitability — confirmed

This is **not** the 2-CH CAN HAT, but it is fully usable.

| Property | RS485 CAN HAT (SKU 14882) |
| --- | --- |
| CAN controller | **MCP2515** |
| CAN transceiver | **SN65HVD230** |
| Operating voltage | **3.3 V** |
| CAN channels | 1 |
| CS | CE0, pin 24 (GPIO 8) |
| INT | **GPIO 25**, pin 22 |
| Transceiver | 3.3 V — **safe, no modification needed** |
| Extra | SP3485 RS485 transceiver (unused for this project) |
| Onboard | 120 Ω terminator via DIP switch, TVS protection |
| Dimensions | 65 × 30 mm |

The critical requirements are met: MCP2515 chip for `can-mcp2515`, and a 3.3 V
transceiver needing no PCB modification. Two HATs give exactly the required two-node
bus. `-g 25` matches the installer default.

**Consequence:** the route changed back from breadboard modules to HATs. This removes
the trace-cut or SN65HVD230 swap entirely.

**New constraint:** the RS485 CAN HAT is a full 40-pin HAT, so it **covers physical
pins 7, 11 and 13** — the GPIO marker pins. Marker wires must be soldered to the
header underside *before* mounting.

**Unknown:** crystal frequency. Documentation does not state it; a forum note
suggests variants differ. Installer defaults to `-c 8000000`.

---

## 10. Commit history

### Backend — `schedulix_can`, branch `hw/procurement-and-gap-plan`

| Commit | Message |
| --- | --- |
| `f0ac4d4` | Initial commit (pre-existing) |
| `da4b0eb` | Add hardware procurement and code gap plan for PS16 peripherals |
| `32f4735` | Rewrite hardware plan: 3-Pi topology, verified QNX software stack, substitutes, final BOM |
| `20ec609` | Add CAN board selection guide, correct BOM to 1 HAT + CANable |
| `f1c3405` | Switch selected CAN route to MCP2515 modules (Waveshare unavailable) |
| `ad84973` | Complete verified hardware inventory: power supply gap, precise jumper counts, pin conflict check |
| `4cdc8ac` | Add CAN driver binary (aarch64le), spi.conf, bring-up check and install scripts |
| `4b5567a` | Add Windows bring-up launcher for single-Ethernet-session workflow |
| `71a726b` | Bring-up launcher: auto-negotiate legacy SSH algorithms QNX sshd requires |
| `677c1d2` | Add paramiko-based QNX SSH client (legacy algorithm support, stage diagnostics) |
| `d0c762f` | Add block-by-block QNX audit runner with tee-to-report |
| `15b4ee9` | CAN installer for real QSTI board: fix SPI mode/word_width, root escalation, slog2info capture |

### Frontend — `schedulix_v1`, branch `hw/can-uart-gpio-preflight`

| Commit | Message |
| --- | --- |
| `d8de1a0` | Baseline: Qt6/QML Schedulix analyzer + hardware preflight harness |
| `8f33934` | Add CAN/UART/GPIO hardware preflight harness and requirements |

---

## 11. Current state

### Working

- Both repositories under git with clean histories
- Qt 6.8.3 frontend builds 56/56
- `can-mcp2515` compiled for aarch64le and committed
- SSH to the board via paramiko (`qnxuser`/`qnxuser`)
- Board confirmed: QNX 8.0.0, Pi 4B, 8 GB, 4 cores
- `/dev/ser1` present — UART code will find its device
- SPI driver running with `dev0`/`dev1` nodes
- `gpio-bcm2711`, `mbox-bcm2711`, `canctl`, `tracelogger` all present

### Not yet done

| Item | Blocker |
| --- | --- |
| CAN driver install | Needs root; no `sudo`, root fs read-only |
| `spi.conf` SPI-mode fix | Needs root |
| GPIO physical proof | Needs the `GPFSEL` code fix and an LED wired |
| UART loopback test | Adapter not yet wired |
| CAN hardware test | HAT not yet mounted |
| `gpio_marker.c` rewrite | Code work |
| `qnx_can_adapter.c` | Code work — does not exist |
| UART TX marker fix | Code work |
| Qt JSON loader | Code work |

### Note on the selected CAN hardware

The plan document was routed to breadboard MCP2515 modules when Waveshare was
unavailable. The **RS485 CAN HAT has since been acquired**, which changes three
things in that document:

| Aspect | Module route (documented) | RS485 CAN HAT route (actual) |
| --- | --- | --- |
| Transceiver modification | Cut trace or fit SN65HVD230 | **Not needed** — already SN65HVD230 at 3.3 V |
| Channels per board | 1 | 1 |
| GPIO marker access | Direct, header free | **Blocked** — HAT covers pins 7/11/13 |
| Header soldering | Not required | **Required before mounting** |
| Total cost | ≈ ₹6,500 | Already spent |

Sections 7.4 and 8 of `HARDWARE_PROCUREMENT_PLAN.md` describe the module route and
should be read as the fallback. The HAT satisfies every driver requirement:
MCP2515 controller, 3.3 V SN65HVD230 transceiver, CS on CE0 (pin 24), INT on GPIO 25
(pin 22) — which matches the installer defaults exactly.

---

## 12. Next steps, in order

1. **Wire the serial console.** USB-TTL TX→pin 8, RX→pin 10, GND→pin 6, **VCC not
   connected**, 115200 8N1. Serial gives a root shell with no password. This unblocks
   everything else.
2. **Solder GPIO marker wires to the header underside** on the board that will run
   QNX, *before* mounting the RS485 CAN HAT. Pins 7, 11, 13.
3. **Run `sh /tmp/qnx_can_install.sh` from the serial root shell.** Files are already
   in `/tmp`. This rewrites `spi.conf`, installs the driver binary, and reports
   whether `/dev/can0` appears. Expect failure at `/dev/can0` until the HAT is mounted.
4. **LED proof.** 330 Ω + LED from GPIO 4 (pin 7) to ground. `schedulix gpio test`
   must print `REAL`, not `MOCK`.
5. **UART loopback.** Adapter TX→pin 10, RX→pin 8, GND→pin 6.
6. **Read the crystal marking** on the RS485 CAN HAT and set `CLOCK_HZ` accordingly.
7. **Mount the HAT**, wire CAN_H/CAN_L between two boards with 120 Ω at each end.
8. **Rewrite `gpio_marker.c`** using `gpio-bcm2711` instead of `/dev/mem` mmap.
9. **Write `qnx_can_adapter.c`** using `/dev/can0` mailboxes and `CAN_DEVCTL_*`.
10. **Fix the UART TX marker** at `src/uart_adapter.c:151`.
11. **Write the Qt JSON loader** replacing `MockProvider`.

---

## 13. Open questions

| Question | Impact |
| --- | --- |
| RS485 CAN HAT crystal: 8 MHz or 16 MHz? | Wrong value = driver starts cleanly, zero frames |
| Does `root` accept an empty password over SSH? | If yes, no serial console needed |
| Does `gpio-bcm2711` work as `qnxuser`? | gpio group membership suggests yes |
| Is `/tmp` writable and large enough for trace files? | Audit showed `/system` has 2.4 G free |
| Can shared memory work given `/dev/shmem` is 0 bytes? | May need `shm_open` with explicit allocation |

---

## 14. Reference: verified command reference

### Working on the board

```bash
# Connection
python tools\qnx_ssh.py probe
python tools\qnx_audit.py
python tools\qnx_can_install.py

# Via paramiko once connected
pidin info                      # platform summary
pidin ar                        # running processes
ifconfig                       # network
ls /dev/ser*                   # UART nodes
ls /dev/io-spi/spi0/            # SPI nodes
pidin ar | grep spi            # SPI driver status
gpio-bcm2711 get 20            # read GPIO 20
gpio-bcm2711 set 20 op pn dh   # drive GPIO 20 high
slog2info | tail -40           # system log
df -h                          # filesystem usage

# CAN (after install)
canctl -u 0,rx0 -m 0            # set MID
canctl -u 0,rx0 -f 0            # clear filter
canctl -u 0,rx0 -R 2000         # receive loop
canctl -u 0,tx2 -w 0x123,3,ABCDEF   # transmit frame
cat /dev/can0/rx0              # raw receive
```

### Serial console

```
USB-TTL TX  → Pi pin 8   (GPIO 14)
USB-TTL RX  → Pi pin 10  (GPIO 15)
USB-TTL GND → Pi pin 6 or 14
USB-TTL VCC → DO NOT CONNECT
115200 8N1, no login prompt, root shell
```

### MCP2515 driver

```sh
can-mcp2515 --mid=eid -s /dev/io-spi/spi0/dev0 -c 8000000 -g 25 -B 500000
can-mcp2515 ... -D          # foreground with live logging
```

---

## 15. Files created this session

### Backend `schedulix_can`

Line counts from the files as committed.

| Path | Lines | Purpose |
| --- | ---: | --- |
| `docs/HARDWARE_PROCUREMENT_PLAN.md` | 757 | Complete hardware plan, topology, verification, BOM |
| `docs/SESSION_LOG.md` | this file | Session record |
| `tools/qnx_bringup_check.sh` | 110 | On-target audit script |
| `tools/qnx_can_install.sh` | 144 | CAN driver installer |
| `tools/spi.conf.mcp2515` | 30 | MCP2515 SPI configuration |
| `tools/qnx_ssh.py` | 335 | Paramiko SSH client with diagnostics |
| `tools/qnx_audit.py` | 143 | Block-by-block audit runner |
| `tools/qnx_can_install.py` | 177 | Installer with root escalation |
| `tools/bringup.ps1` | 132 | Windows launcher (OpenSSH, superseded by paramiko) |
| `tools/bringup.cmd` | 11 | Double-click wrapper |
| `third_party/can-mcp2515/aarch64le/bin/*` | — | Prebuilt driver binaries, 148,840 and 332,448 bytes |
| `third_party/can-mcp2515/lib-public/mcp2515/*.h` | 8 | Driver public headers |
| `.gitignore` | 4 | Build artefacts |
| `bringup_report.txt` | 7,617 bytes | Audit output (generated, untracked) |

### Frontend `schedulix_v1`

| Path | Lines | Purpose |
| --- | ---: | --- |
| `hw/preflight.py` | 245 | Library/tool/port inventory and self-tests |
| `hw/README.md` | 45 | Usage documentation |
| `hw/requirements-host.txt` | 18 | Pinned dependencies |
| `.gitignore` | 5 | Build artefacts |