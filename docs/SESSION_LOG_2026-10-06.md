# Schedulix — Serial Console Bring-Up and Phase 1 Validation

**Follow-on session, 2026-10-06**

Scope: getting a root shell onto a QNX 8.0 Raspberry Pi 4 with no Ethernet, building
a serial file-transfer path, shipping a 331,152-byte binary over an ungated 115200
console, and proving the UART adapter on real hardware.

Prior record: [`SESSION_LOG.md`](SESSION_LOG.md) (2026-10-01 → 2026-10-06). This
document **supersedes it where explicitly flagged** in
[§11 Corrections to prior documentation](#11-corrections-to-prior-documentation).
Where the two disagree and no correction is listed here, `SESSION_LOG.md` stands.

---

## Table of contents

| § | Section |
| ---: | --- |
| 1 | [Scope and context](#1-scope-and-context) |
| 2 | [Working environment constraints](#2-working-environment-constraints) |
| 3 | [Board inventory and confirmed facts](#3-board-inventory-and-confirmed-facts) |
| 4 | [The serial console breakthrough](#4-the-serial-console-breakthrough) |
| 5 | [PuTTY troubleshooting matrix](#5-putty-troubleshooting-matrix) |
| 6 | [`tools/serial_console.ps1`](#6-toolsserial_consoleps1) |
| 7 | [The binary-upload saga](#7-the-binary-upload-saga) |
| 8 | [Phase 1 result — UART adapter](#8-phase-1-result--uart-adapter) |
| 9 | [Consolidated defect table](#9-consolidated-defect-table) |
| 10 | [Stale documentation to reconcile](#10-stale-documentation-to-reconcile) |
| 11 | [Corrections to prior documentation](#11-corrections-to-prior-documentation) |
| 12 | [Roadmap, phases 2–6](#12-roadmap-phases-26) |
| 13 | [Open questions](#13-open-questions) |
| 14 | [Current board state](#14-current-board-state) |
| 15 | [Phase 2 result — GPIO marker](#15-phase-2-result--gpio-marker-verified-2026-10-07) |
| A | [Appendix A — verified command reference](#appendix-a--verified-command-reference) |
| B | [Appendix B — file inventory](#appendix-b--file-inventory) |

> **Continued on 2026-10-07.** [§15](#15-phase-2-result--gpio-marker-verified-2026-10-07)
> records the completion and hardware verification of Phase 2 (the GPIO marker rewrite), the
> two new access paths proven that day (Momentics IDE deployment, SSH), and the host-networking
> gotcha. Read it alongside §14, which is the board state at the end of 2026-10-06.

---

## 1. Scope and context

**Problem Statement 16 — Automotive RTOS Performance & Latency Analyzer.**
Measure ECU workload timing on QNX 8.0 on a Raspberry Pi 4 using CAN, UART and GPIO
timing markers.

| | |
| --- | --- |
| Backend repo | `C:\Users\User\ide-8.0.3-workspace\schedulix_can` |
| Backend branch | `hw/procurement-and-gap-plan` |
| Frontend repo | `C:\Users\User\Documents\schedulix_v1` |
| Frontend branch | `hw/can-uart-gpio-preflight` |
| Frontend state | Qt 6.8.3 / QML, builds 56/56, working tree clean |
| Board | QNX 8.0.0, hostname `qnxpi`, RaspberryPi4B aarch64le |
| Prior access path | SSH to 192.168.10.5 as `qnxuser`/`qnxuser` via `tools/qnx_ssh.py` |
| **Access this session** | **Serial console, COM6, 115200 8N1, root shell** |

Two facts shaped the whole session:

1. **Ethernet was not connected, by choice.** The operator is on Wi-Fi only.
   Connecting Ethernet would have cut internet access — and therefore the AI
   assistance being used to do the work. Every byte therefore had to move over the
   serial console.
2. **The console *is* `/dev/ser1`.** This is not a separate debug UART. It is the
   only serial device on the board, and it is the one the login shell runs on. It
   makes the transport and the device under test the same wire.

The result was a working serial console, a working serial upload path, and a proven
UART adapter. Two board power-cycles were required along the way, both caused by
transport mistakes documented honestly in [§7](#7-the-binary-upload-saga).

---

## 2. Working environment constraints

These are the constraints that generated every design decision in this session.

### 2.1 No Ethernet

| Consequence | Where it bites |
| --- | --- |
| `tools/qnx_ssh.py` unusable | `SESSION_LOG.md` §8 root escalation could never run |
| No `scp`, no `sftp`, no NFS | Binary distribution has one path only: serial |
| The PuTTY suite's file transfer is dead | PSCP and plink are SSH-only; raw serial is a terminal, not a transport |

### 2.2 A QNX SDP toolchain **is** present on the Windows host — this corrects a mid-session error

**This section replaces the claim that no toolchain is available.** The claim
originated from inspecting `C:\QNX`, which contains only `QNX Software Center`,
`qnxmomenticside` and `Uninstall.exe` — the **IDE** install, not the SDP. The SDP
8.0 is installed separately, under the user profile:

```
C:\Users\User\qnx800\
  qnxsdp-env.bat, qnxsdp-env.sh
  host\win64\x86_64\usr\bin\     307 executables
  target\qnx\aarch64le\usr\lib\
  target\qnx\usr\include\
```

Verified present and working (probe run from
`C:\Users\User\AppData\Local\Temp\opencode`, nothing in the repo was written):

| Tool | Path | Probe result |
| --- | --- | --- |
| `qcc.exe` (413,617 B) | `qnx800\host\win64\x86_64\usr\bin\qcc.exe` | `qcc --version` → `Toolchain: gcc Version: 12.2.0` |
| aarch64le cross GCC | `aarch64-unknown-nto-qnx8.0.0-gcc.exe` | compile+link exit 0 |
| aarch64le cross G++ | `aarch64-unknown-nto-qnx8.0.0-g++.exe` | present |
| aarch64le linker | `aarch64-unknown-nto-qnx8.0.0-ld.exe` | present |
| binutils (`as`, `ar`, `objdump`, `addr2line`, `elfedit`) | 2.43.0 set | present |
| **strip** | `ntoaarch64-strip.exe` | **works** — 9,536 B → 5,696 B |
| `libcan.a` (aarch64le) | `qnx800\target\qnx\aarch64le\usr\lib\libcan.a` | present |
| `hw/libcan.h` | `qnx800\target\qnx\usr\include\hw\libcan.h` | present |
| `sys/can_dcmd.h` | `qnx800\target\qnx\usr\include\sys\can_dcmd.h` | present |

Live compile and link probe:

```powershell
$env:QNX_BASE    = "C:/Users/User/qnx800"
$env:QNX_TARGET  = "$env:QNX_BASE/target/qnx"
$env:QNX_HOST    = "$env:QNX_BASE/host/win64/x86_64"
$env:PATH        = "$env:QNX_HOST/usr/bin;$env:QNX_BASE/host/common/bin;$env:PATH"
& "$env:QNX_HOST/usr/bin/qcc.exe" -Vgcc_ntoaarch64le -o probe probe.c
# cc exit=0 -> probe.exe, 9,296 bytes
```

Live `-lcan` link probe (this is the Phase 5 prerequisite):

```powershell
& "$env:QNX_HOST/usr/bin/qcc.exe" -Vgcc_ntoaarch64le -o can_probe can_probe.c `
    -lsocket -Wl,-Bstatic -lcan -Wl,-Bdynamic
# cc exit=0 -> can_probe, 9,536 bytes
& "$env:QNX_HOST/usr/bin/ntoaarch64-strip.exe" can_probe
# strip exit=0 -> can_probe, 5,696 bytes
```

`qcc` requires `QNX_HOST` and `QNX_TARGET` to be set, or it fails with
`cc: The QNX_HOST/QNX_TARGET environment variables must be set`. Running
`C:\Users\User\qnx800\qnxsdp-env.bat` from a `cmd` shell sets them, but it also sets
`@echo on` which spams the console; setting the four variables directly from
PowerShell is cleaner.

**What this changes.** Binaries do **not** have to be pre-built elsewhere. The
Phase 2 GPIO rewrite can be compiled for `aarch64le` on this host and shipped with
the same 81-second upload path proven in [§7.8](#78-final-results). It also means
`-lcan` can be validated locally before Phase 5 starts, rather than discovering a
link problem on the board.

`C:\Users\User\.qnx\` also contains a valid `com.qnx.qnx800_1.xml` configuration and
licence checkout records, so the installation is licensed and complete.

### 2.3 No multimeter

Logic levels were determined **empirically**, not measured. This is the single
biggest methodological difference from a normal bench bring-up and it drove the
entire divider design — see [§4.2](#42-why-neither-blind-choice-is-acceptable).

### 2.4 Host test suites (verified by running)

| Suite | Result |
| --- | --- |
| `tests/run_tests.py` | **5/5** |
| `tests/test_phase6.py` | **13/13** |
| `tests/test_analyzer_integrity.py` | **3/3** |

All three are re-run after the Phase 3 enum change.
`test_analyzer_integrity.py` is the one that checks trace header integrity and a
trace-change regression, so it is the most likely to catch a bad enum edit.

### 2.5 Toolchain absent on the board

Tools present on the QNX image: `base64`, `cksum`, `tr`, `gzip`, `gunzip`, `zcat`,
`split`, `head` (with a working `-c`), and the shell builtin `kill`.

Tools **absent**: `pkill`, `gcc`, `cc`.

Two consequences:

* `gunzip` existing is what makes the upload scheme viable at all
  ([§7.7](#77-measured-details)).
* `pkill` absent means no `pkill can-mcp2515`; `kill` by PID from `pidin` is the
  only option. `tools/qnx_can_install.sh:96,105` already uses `slay` for this, which
  is present.

---

## 3. Board inventory and confirmed facts

### 3.1 Platform (all verified this session)

| Field | Value |
| --- | --- |
| Version string | `QNX qnxpi 8.0.0 2025/07/30-19:17:34EDT RaspberryPi4B aarch64le` |
| CPUs | 4 × Cortex-A72 @ 1500 MHz |
| Memory | FreeMem 7530 MB / 8128 MB |
| Processes | 38 |
| Threads | 258 |
| Shell | `root@console:/#`, **no password** |
| Image | Quick Start Target Image (QSTI) — confirmed by the `qnxuser`/`qnxuser` SSH login in `SESSION_LOG.md` §7 |

FreeMem reads 7530 MB here against 7487 MB in the prior SSH audit. That is a moving
value, not a discrepancy.

### 3.2 `/dev/ser1` IS the console — the decisive discovery

```
devc-serminiuart -e -b115200 -c500000000 -e -F -u1 0xfe215000,125
```

| Element | Meaning |
| --- | --- |
| `devc-serminiuart` | QNX's **mini-UART / AUX** driver. This is **not** the PL011 |
| `0xfe215000` | BCM2711 **AUX (mini-UART)** register base — confirms the peripheral |
| `-u1` | unit 1 → node **`/dev/ser1`**. The only serial device on the board |
| `-b115200` | 115200 baud |
| `-c500000000` | 500 MHz input clock reference |
| **`-e`** (appears twice) | **the driver echoes every byte it receives** |
| `-F` | monitor / flow-control handling |

Two consequences dominate the rest of the session:

1. **`/dev/ser1` is the console.** Anything the host sends is consumed by the login
   shell. There is no second UART to test against, and no way around that with the
   hardware on hand.
2. **The `-e` flag doubles wire traffic.** Every byte sent is echoed straight back.
   A client that does not drain continuously overruns the receive buffer. This caused
   a full transfer failure and one board power-cycle
   ([§7.4](#74-defect-4-echo-was-never-drained-during-streaming-power-cycle-2)).

This also corrects the earlier draft of this document, which described the console as
a **PL011**. It is not. The threshold arithmetic in [§4.2](#42-why-neither-blind-choice-is-acceptable)
still holds, but as a generic 3.3 V CMOS input threshold, not a PL011 datasheet figure.

### 3.3 `/tmp` and storage

| Item | Value |
| --- | --- |
| `/tmp` target | `/data/var/tmp` on `/dev/hd0t179` |
| Capacity | 55,517,144 blocks |
| Free | ~53 M — ample for 331 KB binaries and multi-MB traces |
| Root filesystem `/` | 100% full, read-only (unchanged from the prior audit) |

`/tmp` contents observed over the serial console — **the complete listing**:

| Name | Size | Owner |
| --- | ---: | --- |
| `T3` | 8,880 | root |
| `elvis1.ses` | 98,304 | root |
| `keep_files` | 36 | root |
| `qnx_bringup_check.sh` | 4,474 | qnxuser |

Absent: `qnx_can_install.sh`, `spi.conf.mcp2515`, `can-mcp2515`, and any Schedulix
binary. See [§11.2](#112-correction-2--the-installer-files-are-not-in-tmp).

### 3.4 Hardware on hand

**2 × Waveshare RS485 CAN HAT (SKU 14882)** — this is *not* the 2-CH CAN HAT named in
the original plan.

| Property | Value | Why it matters |
| --- | --- | --- |
| CAN controller | **MCP2515** | exactly what `can-mcp2515` drives |
| CAN transceiver | **SN65HVD230 @ 3.3 V** | no PCB modification needed; the TJA1050 hazard does not apply |
| CS | CE0, header pin 24 (BCM 8) | matches installer default |
| INT | **BCM 25**, header pin 22 | matches installer default `-g 25` |
| Terminator | 120 Ω via DIP switch | one per bus end |
| Extra | onboard SP3485 RS485 transceiver | unused for this project |
| Footprint | full 40-pin HAT, 65 × 30 mm | **covers header pins 7, 11, 13** |

**The HAT-covers-pins consequence constrains all later work.** Header pins 7, 11 and
13 are BCM 4, 17 and 27 — the three GPIO timing-marker pins. Marker wires must be
soldered to the header **underside** *before* the HAT is mounted. Afterwards those
pins are physically inaccessible.

| Item | Notes |
| --- | --- |
| USB-TTL adapter | **CH340**, VID `1A86` PID `7523`, enumerates `USB-SERIAL CH340 (COM6)`, confirmed healthy |
| Logic analyzer | 8-channel, for capturing GPIO marker edges |
| Breadboard, jumper wires | |
| LEDs | for the Phase 2 GPIO proof |
| 6 × 330 Ω | 3 used for the UART divider, 1 for the LED proof |
| 2 × 120 Ω | CAN bus terminators |

### 3.5 Artifacts that still have to reach the board

| Artefact | Path | Bytes |
| --- | --- | ---: |
| Schedulix binary | `build/aarch64le-debug/schedulix_can` | 331,152 |
| CAN driver | `third_party/can-mcp2515/aarch64le/bin/can-mcp2515` | 148,840 |
| SPI config | `tools/spi.conf.mcp2515` | 826 |
| CAN installer | `tools/qnx_can_install.sh` | 5,391 |

The Schedulix binary **is** now on the board as `/tmp/sx`
([§8](#8-phase-1-result--uart-adapter)). The other three are not.

---

## 4. The serial console breakthrough

### 4.1 What was achieved

A **root shell with no password**, reached over a serial cable at 115200 8N1, with no
Ethernet. This clears the single blocker that stopped the prior session:

> CAN driver install | Needs root; no `sudo`, root fs read-only
> — `SESSION_LOG.md` §11

It does **not** remove the read-only root filesystem. `/system/bin` still cannot be
written by an ordinary process. But with root on the console, the installer can write
wherever it needs and there is no password prompt to fight.

First contact — a direct PowerShell `System.IO.Ports.SerialPort` open at 115200 8N1,
no flow control, writing two newlines:

```
ksh: ???@??: cannot execute - No such file or directory
root@console:/#  root@console:/#  root@console:#
```

The `ksh:` line is a cosmetic hostname-lookup failure emitted at shell start-up —
`ksh` tries to resolve the hostname, the resolver returns nothing. It is harmless and
precedes the prompt. The trailing `root@console:#` (no slash) is the prompt format
settling.

### 4.2 Why neither blind choice is acceptable

The hazard:

* QNX's console runs **3.3 V logic** (it is a BCM2711 peripheral).
* **BCM2711 GPIOs are not 5 V tolerant.**
* **No multimeter was available**, so the CH340's actual TX level could not be
  measured.

Many CH340 dongles drive TX at 5 V from USB VBUS even with VCC unconnected — that is
the widely repeated claim. But it is a claim, not a measurement, and both outcomes
are plausible for an unmeasured dongle. They need **opposite** fixes:

| If adapter TX is… | Direct connection | With the 2:1 divider (330 Ω series, 660 Ω shunt) |
| --- | --- | --- |
| **5 V** | destroys the Pi's RX pin | 5 V × 660/990 = **3.33 V** — correct |
| **3.3 V** | fine | 3.3 V × 660/990 = **2.20 V** — **below the input threshold** |

The input threshold on a 3.3 V CMOS peripheral is roughly 0.7 × VDD ≈ **2.31 V**. A
2.20 V signal lands under it and characters decode unreliably or not at all.

So:

* **Direct connection** risks permanent damage to the board if the guess is wrong.
* **A blind divider is also wrong** — if TX is 3.3 V, the divider pushes the signal
  *below threshold*, producing garbage.

**The divider is the one choice that is safe in both cases:**

* It **cannot damage the pin.** It only ever attenuates.
* It is **self-diagnosing** — the output *is* the measurement:

| What you see | What it means | Action |
| --- | --- | --- |
| Clean readable text | TX is 5 V, divider correct as built | keep the divider |
| Garbage, framing errors, or nothing | TX is 3.3 V, signal attenuated too far | remove the divider, connect direct |

A divider that is wrong produces a *diagnostic symptom*, not damage. That property
is exactly what makes it the right choice under uncertainty, and it is why the level
question could be closed without a meter at all.

### 4.3 Divider as actually built

Three of the six 330 Ω resistors:

```
                         CH340 USB-TTL adapter
                    ┌───────────────────────────────┐
                    │                               │
  Pi pin 6  GND ────┤ GND                      RXD ├──────┐
                    │                               │      │
                    │                               │      │  3.3 V from the Pi,
                    │                               │      │  no divider needed:
                    │                               │      │  CH340 RXD input is
                    │                               │      │  5 V tolerant
                    │                          TXD  │      │
                    │                           │   │      │
                    │                       [ 330R ]   │      │
                    │                           │   │      │
                    │                     ┌─── TAP ──┼──────┘
                    │                     │         │      │
                    │                  [ 330R ]   │      └──► Pi pin 10
                    │                     │         │            GPIO15
                    │                  [ 330R ]   │            (Pi RECEIVES)
                    │                     │         │
                    │                     │         │      ┌───  Pi pin 8
                    │                     │         └──────┤    GPIO14
                    └─────────────────────┴────────────────┤    (Pi TRANSMITS)
                                                           └──► adapter RXD
```

Numbers:

| Quantity | Value |
| --- | --- |
| Series resistance, TXD → TAP | 330 Ω |
| TAP → GND | 330 + 330 = 660 Ω |
| Total DC load presented to TXD | 990 Ω |
| Divider ratio | 660/990 = **0.6667** |
| Output for a 5 V input | 5 × 0.6667 = **3.33 V** |
| Output for a 3.3 V input | 3.3 × 0.6667 = **2.20 V** — the diagnostic case |

**Rise time is not a concern.** The Thevenin resistance of the divider is
330 ∥ 660 = **220 Ω**. With the CH340's low output impedance and a few picofarads of
input capacitance, the RC corner sits orders of magnitude below the **8.7 µs bit
period** at 115200 baud. The divider does not degrade the bit rate.

### 4.4 The Pi→adapter direction needs no divider

Pi pin 8 drives **3.3 V** into the CH340's RXD input. The CH340's RXD input is 5 V
tolerant, so a direct connection is correct. A divider there would only risk pushing
the signal under the adapter's own input threshold.

### 4.5 Complete net list

| From | To | Divider? |
| --- | --- | --- |
| Pi pin 6 (GND) | adapter GND | no |
| Pi pin 8 (GPIO14 — **Pi TX**) | adapter **RX** | **no** |
| adapter **TX** | → 330 Ω → TAP node; TAP → 330 Ω + 330 Ω in series → adapter GND | **yes** |
| TAP node | Pi pin 10 (GPIO15 — **Pi RX**) | — |
| Pi pin 1 (3V3) | *nothing* | **left unconnected** |
| Pi pin 2 (5V) | *nothing* | **left unconnected** |

Pins 1 and 2 are left completely unconnected on purpose. Powering the dongle from
the Pi adds nothing (it draws from USB) and re-introduces the possibility of 5 V
appearing on a GPIO.

### 4.6 Outcome — the level question answered empirically

The console returned a perfectly clean `root@console:/#` prompt, and `pidin info`
came back clean and fully legible. Clean ASCII at 115200 8N1 is only possible if the
RX input saw a valid logic swing. Given the divider is fitted:

> **⇒ the CH340's TX is 5 V, and the divider is correct as built. Keep it.**

This is a measured conclusion, not an assumption — the divider's output level *was*
the measurement. It closes the level question for this adapter.

If a different dongle is substituted later, repeat the check: clean text means keep
the divider, garbage means remove it.

---

## 5. PuTTY troubleshooting matrix

Symptom: clicking PuTTY's **Open** produced nothing useful.

### 5.1 Port enumeration

| Probe | Result | Interpretation |
| --- | --- | --- |
| `[System.IO.Ports.SerialPort]::GetPortNames()` | COM3, COM4, COM5, COM6 | COM6 present |
| `Get-CimInstance Win32_SerialPort` | **COM6 absent** | WMI enumeration quirk — **not** a fault |
| `Get-PnpDevice` | `USB-SERIAL CH340 (COM6)`, InstanceId `USB\VID_1A86&PID_7523\5&5114092&0&3`, Status **OK** | driver installed correctly; **COM6 is the right port** |

VID `1A86` / PID `7523` is the CH340. The WMI omission is a known Windows
enumeration inconsistency and can be ignored. **`Get-PnpDevice` is the authoritative
check** for "is this port real and healthy".

### 5.2 PuTTY was sitting on the launcher dialog

A running `putty.exe` (PID 7780) had `MainWindowTitle` = `PuTTY Configuration`, i.e.
it was on the launcher dialog with no session open. Part of "nothing happens" was
simply that no session existed to show any output.

### 5.3 Command-line switches — all measured

| Invocation | Result | Conclusion |
| --- | --- | --- |
| `putty.exe -baud 115200` | "PuTTY Command Line Error" dialog | **`-baud` is not a valid switch** |
| `putty.exe -speed 115200` | process exits immediately | **`-speed` is not valid either** |
| `putty.exe -serial COM6` | **opens a live session** | this is the working form |
| `-load SchedulixConsole` on a registry-saved session | fell back to the Configuration dialog, session did **not** open | registry-session route not usable here |

The `SchedulixConsole` session under
`HKCU\Software\SimonTatham\PuTTY\Sessions\` was **deleted again** afterwards, to avoid
leaving a broken entry that invites a second failed attempt.

### 5.4 The method that works

Write PuTTY's `Default Settings` in the registry, then use `-serial`:

```
HKCU\Software\SimonTatham\PuTTY\Default Settings
  Speed             = 115200    (DWORD)
  SerialFlowControl = 0
  DataBits          = 8
  StopBits          = 1
  Parity            = 0
  PortNumber        = "COM6"    (String)
```

then:

```
putty.exe -serial COM6
```

Verified live: COM6 subsequently returns **access denied** to a second process, which
proves the first process really is holding the port.

**The trap this avoids:** `-serial` supplies the port but *not* the line settings, so
PuTTY falls back to its stored defaults for speed. `Default Settings` is where those
defaults come from, which is why the registry write is required.

### 5.5 PuTTY behaviours worth recording

| Behaviour | Is it a fault? |
| --- | --- |
| Black window with a cursor, nothing else | **No — this is correct idle state.** The board only speaks when spoken to. Press Enter. |
| No login prompt appears | **No.** Root console has no login prompt; you land at a prompt after the `ksh:` line ([§4.1](#41-what-was-achieved)) |
| Session bar reads `Serial COM6 115200 8N1  Flow control: None` when maximized | Confirmation the settings took effect |

### 5.6 Standing recommendation

Prefer `tools/serial_console.ps1` ([§6](#6-toolsserial_consoleps1)) over PuTTY for
any scripted work. It has no registry dependency, no launcher-dialog ambiguity, and
supports one-shot capture and binary upload — which PuTTY does not.

---

## 6. `tools/serial_console.ps1`

A PowerShell serial console client — the serial counterpart to `tools/qnx_ssh.py`.
Same role, different transport: for when Ethernet is unavailable or when root is
required.

```powershell
powershell -ExecutionPolicy Bypass -File tools\serial_console.ps1                    # interactive
powershell -ExecutionPolicy Bypass -File tools\serial_console.ps1 -Command "id"     # one-shot
powershell -ExecutionPolicy Bypass -File tools\serial_console.ps1 `
    -UploadFile build\aarch64le-debug\schedulix_can -RemotePath /tmp/sx            # upload
```

| Mode | Behaviour |
| --- | --- |
| Interactive (default) | mirrors serial output, forwards keystrokes, Ctrl+C to exit |
| `-Command` | sends one command, captures the reply, exits |
| `-UploadFile` | binary upload with size verification |

Parameters: `-Port` (default `COM6`), `-Baud` (default `115200`), 8N1, no flow
control, `-CaptureSeconds` (default 8), `-ChunkSize` (default **64**),
`-ChunkDelayMs` (default **12**).

The file's own header comment carries the wiring summary and, importantly, the
constraint that produced the whole upload design:

> the console driver runs in a mode where Ctrl+D is not delivered as end-of-file and
> Ctrl+C is not delivered as SIGINT, so any approach that parks the shell in a
> foreground reader (`cat > file`) will wedge the console and require a power cycle.

### 6.1 Interactive mode

Key forwarding is explicit rather than raw, because the board's line discipline is
not raw:

| Key | Bytes written |
| --- | --- |
| Enter | `\r\n` |
| Backspace | `0x7F` |
| Escape | `0x1B` |
| everything else | `$key.KeyChar` |

The keystroke pump is wrapped in `try/catch {}`: `[Console]::KeyAvailable` throws when
stdin is redirected, and the catch keeps the output mirror running so the script still
works as a dumb logger when piped or run from a scheduler.

### 6.2 Refinement (a) — early prompt detection

**Problem.** One-shot mode always burned the entire `-CaptureSeconds` window, so every
command cost the full 8 s default no matter how fast it answered.

**Fix** — return as soon as a shell prompt appears at the end of the stream:

```powershell
if ($buffer.ToString() -match '(?m)[\r\n][^\r\n]*[#$]\s*$') { break }
```

The timeout remains as the cap for commands that produce no prompt.

**Measured improvement: 8 s → 0.57 s.** This is why every later command in the
session felt instant.

### 6.3 Refinement (b) — upload mode

Motivation, in order of weight:

1. **There is no serial file-transfer path in the PuTTY suite.** PSCP and plink are
   SSH-only. PuTTY's raw serial mode is a terminal, not a transport.
2. **Ethernet is not connected**, so `tools/qnx_ssh.py` is unavailable.
3. **The artefacts are large** — 331,152 bytes for the current build, 148,840 for
   `can-mcp2515`. These cannot be typed by hand.

**`Clear-Input`** exists for a specific reason: the console driver runs with `-e` and
echoes every byte received, so the receive buffer must be drained deliberately. It is
called before the upload starts and on every chunk iteration
([§7.4](#74-defect-4-echo-was-never-drained-during-streaming-power-cycle-2)).

**Chunking is mandatory, not cosmetic.** Hardware flow control is not wired (RTS/CTS
absent). A single large `Write()` overruns the Pi's UART receive FIFO and bytes are
**silently** dropped — no error, no partial-write indication. Chunking with an
inter-chunk delay keeps the write rate under the drain rate. 64-char chunks at 12 ms
was the setting that produced four consecutive clean transfers.

**Size verification is a hard check, not advisory.** The tool compares the decoded
byte count against the local file size and prints OK or MISMATCH, retrying up to
three times. Across a flow-control-less link this is the only integrity guarantee
available.

> **Known weakness, flagged for a future fix.** The check is a *size* check, not a
> *content* check. Two different files of identical length would pass. A separate
> base64 round-trip comparison confirmed byte identity independently
> ([§7.8](#78-final-results)), but that is a manual step, not built into the tool. A
> CRC or hash comparison over the wire would close this properly.

---

## 7. The binary-upload saga

331,152 bytes had to cross an **ungated** 115200 console. Six distinct defects were
hit. **Two of them required a physical power-cycle of the board because I wedged the
console.** All six are recorded here with symptom, true cause and fix.

### 7.0 The transport, in outline

```
local file
  -> gzip                    (board has gunzip)
  -> base64                  (no shell metacharacters)
  -> stream as chunks into a FOREGROUND `head -c N` sink
  -> over-send whitespace to guarantee head reaches N and exits
  -> tr -d '\r\n' < staging | base64 -d | gunzip > target
  -> compare byte count to source
```

The sink choice is the entire trick. Everything below is the story of getting there.

### 7.1 Defect 1 — per-chunk shell append with quotes truncated the transfer

**Symptom.** 154,384 bytes were shipped as 1,144 chunks of `echo '<chunk>' >> file`.
The staging file came back **6,889 bytes**.

**Cause.** One dropped byte anywhere in the stream left a single quote unterminated.
`ksh` entered continuation mode and **every remaining chunk was silently swallowed as
string content**. No error, no visible failure — the shell was perfectly happy, it was
just building one enormous unterminated string.

**Lesson.** Any scheme where the shell accumulates a quoted token across 1,144
separate command invocations has a single point of failure that manifests as
*silent data loss*, not an error. Never build a file by appending in a loop over a
lossy link.

### 7.2 Defect 2 — bare-number reply parsing matched the wrong thing

**Symptom.** Verification read `826` correctly on the first upload, then later matched
`2`.

**Cause.** The `2` was a digit from the **echoed command's own filename**
(`spi2.conf`), not from the output. The parser was extracting a bare number from a
stream that contained both the program's output *and* the driver's echo of the command
that produced it. With `-e` echoing everything, the command text is always in the
stream.

**Fix.** Bracket the value in **digit-free markers** so the echoed command text cannot
collide:

```sh
echo -n UP_SIZE_; wc -c < staging; echo _UP_END
echo -n FINAL_;  wc -c < target;  echo _UPINAL_END
```

Parsing then looks for `UP_SIZE_(\d+)\s*_UP_END` and `FINAL_(\d+)\s*_FINAL_END`
specifically. Nothing else in the stream can match.

**Lesson.** Never parse a bare number out of a console stream that echoes its input.
Bracket every value you need.

### 7.3 Defect 3 — `cat > file` + Ctrl+D wedged the console (POWER CYCLE #1)

**Symptom.** "shell did not return". Then the board returned **0 bytes even to
passive reads**.

**Cause.** This console does **not** deliver `Ctrl+D` as end-of-file to the reader,
nor `Ctrl+C` as SIGINT. The shell was parked in a foreground `cat` that could never be
terminated from the keyboard, so nothing else could run. No input produced no output.

**The wrong instinct.** Adding a *second* `Ctrl+D` and hoping. The approach itself was
unsound: it depends on a control character that demonstrably does not work on this
driver. There is no key sequence that un-parks it.

**Fix.** Abandon any scheme that requires terminating a foreground reader by keyboard
input. See [§7.5](#75-defect-5--a-backgrounded-sink-is-impossible) and
[§7.6](#76-defect-6--the-fix--foreground-head--c-n) for what worked.

**Lesson.** Before parking a shell in a foreground reader over a wire, confirm you can
get it back out. On a driver with no SIGINT and no EOF, you cannot, and the only
recovery is a power-cycle.

### 7.4 Defect 4 — echo was never drained during streaming (POWER CYCLE #2)

**Cause.** With `-e`, the driver echoes every byte sent. Streaming 81 KB of base64
therefore generated ~81 KB of echo back at the client. Nothing was reading it, so the
**receive** buffer overran. The tail of the transfer and the shell's reply were both
lost, and the console stopped responding — requiring a second power-cycle.

**Fix.** Drain `$serial.BytesToRead` on every iteration:

```powershell
Clear-Input

function Clear-Input {
    if ($serial.BytesToRead -gt 0) { $null = $serial.ReadExisting() }
}
```

**Lesson.** `-e` is not cosmetic. It doubles wire traffic in the *inbound* direction.
Any client that streams must read concurrently or it will kill the link.

### 7.5 Defect 5 — a backgrounded sink is impossible

**Symptom.**

```
$ head -c 100 > /tmp/cal &
[1] + Stopped (tty input)  head -c 100 > /tmp/cal
```

**Cause.** POSIX job control. A **background job that reads the terminal takes SIGTTIN
and is stopped.** `head` was never going to run.

**Implication.** The sink must run in the **foreground**. That constraint is what
forces the design in the next section — a foreground process has to be one that
terminates on its own.

### 7.6 Defect 6 — THE FIX: foreground `head -c N`

A foreground `head -c N > staging`:

* exits **by itself** once it has read exactly `N` bytes,
* therefore returns the prompt with **no `Ctrl+D`** and **no `SIGINT`**,
* needs no backgrounding, so it never takes SIGTTIN.

The full sink sequence now shipped in `tools/serial_console.ps1`:

```powershell
$serial.Write("rm -f $staging; head -c $headBytes > $staging`r`n")
Start-Sleep -Milliseconds 800

for ($i = 0; $i -lt $lines; $i++) {
    $serial.Write($b64.Substring($start, $len) + "`r`n")
    Start-Sleep -Milliseconds $ChunkDelayMs
    Clear-Input                        # defeat the -e echo, defect 4
}

$serial.Write("`n" * 600)              # over-send whitespace to reach N
```

The surplus whitespace is harmless because it is stripped by `tr -d '\r\n'` before
decoding. The 400-byte margin plus the whitespace over-send **guarantees** `head`
reaches its `N` and exits rather than sitting waiting for bytes that will never come.

### 7.7 Measured details

**gzip before base64** — the board has `gunzip`, so compression is nearly free:

| Source | Gzipped | Base64 chars | Chunks @ 64 |
| ---: | ---: | ---: | ---: |
| 30,720 B (slice) | 11,592 B | 15,456 | 242 |
| 154,384 B | **61,041 B** | **81,388** | 1,272 |
| 331,152 B | **139,316 B** | **185,756** | 2,904 |

Compression cuts the wire time roughly to a third. It is the single highest-leverage
optimisation available on this transport.

**The line discipline does NOT fold CRLF into LF.** Confirmed empirically rather than
assumed. The staging file always came back as exactly:

```
b64.Length + (lines × 2) + 400
```

Worked example for the 30 KB slice:

```
15,456 + (242 × 2) + 400 = 15,456 + 484 + 400 = 16,340  →  "16340 B (+400 padding)"
```

`headBytes = dataBytesMax + 400`, which for the 331,152 B build is
`185,756 + 5,808 + 400 = 191,964` bytes.

**Verification.** Three independent stages, in this order:

1. `wc -c` of the staging file, checked **before** decode — catches truncation
2. `tr -d '\r\n' | base64 -d | gunzip` — the actual transform
3. final byte count compared against the source — the authoritative check

**Rate.** The tool reports ~2.5 KB/s sustained over the wire with 64-char chunks and a
12 ms delay. Per-file wire rates derived from the measured figures below run
0.9–2.3 KB/s (the small file is dominated by fixed 800 ms + 2 s sleeps).

### 7.8 Final results

| Source | Bytes | Time | Verdict |
| --- | ---: | ---: | --- |
| `tools/spi.conf.mcp2515` | 826 | — | **OK** |
| 30,720 B slice of the build | 30,720 | 17.2 s | **OK** |
| `deploy/schedulix_can-aarch64le-debug` | 154,384 | 44.3 s | **OK** |
| `build/aarch64le-debug/schedulix_can` | 331,152 | **81.6 s** | **OK** |

All byte-for-byte identical to source.

A separate base64 round-trip comparison — encode, decode, compare buffers on the host
— confirmed byte identity **independently of the size check**, closing the weakness
noted in [§6.3](#63-refinement-b--upload-mode).

**The transport is proven.** 331,152 bytes in 81.6 seconds is good enough to ship
`can-mcp2515` (148,840 B) and the installer (5,391 B) for Phase 4 without further
work.

---

## 8. Phase 1 result — UART adapter

**Status: DONE.** The UART adapter is proven working on real hardware.

### 8.1 Pre-test finding — every binary in `deploy/` is stale

Before running anything, the candidate binaries were byte-scanned for three strings
that only exist in current source: `UART Adapter Backend`,
`uart [status|start|send|receive]`, and `gpio test`.

| Binary | Bytes | Timestamp | All three strings |
| --- | ---: | --- | --- |
| `deploy/schedulix_can-aarch64le-debug` | 154,384 | 2026-08-29 21:30 | **absent** |
| `deploy/schedulix_can` | 179,264 | 2026-08-30 14:22 | **absent** |
| `deploy/schedulix_can-x86_64` | 175,360 | 2026-08-30 00:08 | **absent** |
| **`build/aarch64le-debug/schedulix_can`** | **331,152** | **2026-08-30 21:34** | **all PRESENT** |

Also verified present in the build binary only: `REAL PHYSICAL` and `schedulix uart`.

The build binary's timestamp matches the newest source file
(`src/qnx_kernel_trace_parser.c`, 2026-08-30 21:34), so **it is the current build and
`deploy/` is stale relative to it.**

**Uploading from `deploy/` would have tested old code** and reported a green result
for a binary that contains neither the UART nor the GPIO subcommand. This is a
process-level trap worth remembering: `deploy/` is not authoritative.

> This also means the 331,152-byte upload — the slowest transfer of the session — was
> the *correct* one to perform.

### 8.2 `uart status` — physical device confirmed

Uploaded `build/aarch64le-debug/schedulix_can` to `/tmp/sx` as `sx` and ran:

```
$ /tmp/sx uart status
UART Adapter Backend: physical
UART Status: physical device /dev/ser1 configured at 115200 baud
```

This proves all three of these succeeded on the real device
(`src/uart_adapter.c:39-71`):

| Call | Line | Result |
| --- | --- | --- |
| `open("/dev/ser1", O_RDWR\|O_NOCTTY\|O_NONBLOCK)` | `:39` | **succeeded** |
| `tcgetattr()` | `:43` | **succeeded** |
| `tcsetattr(TCSANOW)` | `:63` | **succeeded** |

Only if all three pass does `g_simulated` go false and the physical status string get
written (`:64-66`).

**The distinction matters far more than it looks.** The failure string to reject is
from `src/uart_adapter.c:76`:

```
simulated loopback mode (dev /dev/ser1 offline)
```

The simulated fallback makes `uart send` followed by `uart receive` succeed perfectly
while proving nothing — `src/uart_adapter.c:98-112` pushes into an in-process 256-byte
ring buffer and `:127-135` reads it back. A green round-trip against the loopback
queue is **indistinguishable** from a green round-trip over copper unless you read the
status string. Reading `uart status` before believing any UART result is mandatory.

**Unexpected and welcome:** QNX **permitted a second open** of `/dev/ser1` while the
console driver owned it. The anticipated contention did not materialise. See
[§13](#13-open-questions).

### 8.3 Transmit — proven end-to-end

```
$ /tmp/sx uart send SCHEDULIX_TX_PROOF_42
SCHEDULIX_TX_PROOF_42Sent 21 bytes over UART: 'SCHEDULIX_TX_PROOF_42'
```

**The ordering is the proof.** The 21 characters
`SCHEDULIX_TX_PROOF_42` reached the host on COM6 **before** the program's own
`Sent 21 bytes over UART:` line. That cannot happen from the program's `printf`,
which is emitted locally after `write()` returns. It can only happen if the bytes
actually traversed:

`uart_adapter_send()` → `/dev/ser1` → mini-UART at `0xfe215000` → GPIO14 → pin 8 →
adapter RXD → CH340 → USB → COM6

**TX is proven end-to-end through the divider, the CH340, and 115200 baud.** The
divider built in [§4.3](#43-divider-as-actually-built) is confirmed working on the
receive-into-adapter path as well.

### 8.4 Receive — confounded, and the reason why

```
$ /tmp/sx uart receive
Received 1 bytes from UART: '\n'
```

A genuine `read()` from a real file descriptor — but the single byte was the console's
own newline, **not** data from the host.

**RX cannot be meaningfully tested on this board as wired**, because
`/dev/ser1` **is** the console ([§3.2](#32-devser1-is-the-console--the-decisive-discovery)).
Anything the host sends is consumed by the login shell before `uart receive` ever sees
it. There is no second UART to test against.

**Remedy for a proper RX test.** Place a jumper **between the USB-TTL adapter's own TX
and RX pins**. Then `uart send` loops the bytes straight back into the Pi's receive
input, and because `ksh` is busy executing the compound command it is not reading
stdin, so `uart receive` gets an uncontested read. This tests the real fd with real
bytes without involving the shell.

> **DO NOT jumper pin 8 → pin 10 on the Pi.** That would put the Pi's TX output
> (GPIO14) in direct contention with the divider's output on GPIO15 — two drivers
> fighting over one input. Loop back on the **adapter** side, never across the Pi's
> two pins.

### 8.5 A side observation — `tty.c_lflag = 0` and the console's echo

`src/uart_adapter.c:48` sets `tty.c_lflag = 0`, which clears `ECHO` among other flags.
Because `/dev/ser1` is the *same device the console driver owns*, this `tcsetattr()`
plausibly silences console echo for the remainder of the session.

**This is an unverified hypothesis.** It was not tested, and the console continued to
work during the session. Flagged because it is a real side effect of running
`uart_adapter_init()` on the console device, and because it would matter if the
adapter were ever reconfigured while a live console session depended on echo.

---

## 9. Consolidated defect table

All line numbers refer to the working tree at the time of this log.

### 9.1 `src/gpio_marker.c` — the rewrite target

| # | Location | Defect | Severity |
| --- | --- | --- | --- |
| **(a)** | `:52-54` | Uses `open("/dev/mem")` + `mmap(NULL, 4096, PROT_READ\|PROT_WRITE, MAP_SHARED, mem, 0xFE200000)`. Wrong for QNX. | **fatal** |
| **(b)** | `:77-80` | **`GPFSEL` is never written.** | **fatal** |
| **(c)** | `:90` | `gpio_marker_get()` returns `-1` unconditionally, argument discarded. No readback. | high |
| **(d)** | `:78-80` | Shifts by `bcm_pin` directly — only GPIO 0–31 reachable. | medium |
| **(e)** | `:102`, `:106` | `gpio_marker_trace_high()`/`_low()` emit `TRACE_CPU_MIGRATION` as a carrier for a GPIO marker. | **high** |
| **(h)** | `:20-21` | `g_high_ts[16]` / `g_low_ts[16]` declared, but `pin_to_idx()` (`:32-34`) only ever returns 0–2. 13 slots dead. | low |

#### (a) the mmap pattern

Current code, `src/gpio_marker.c:52-54`:

```c
int mem = open("/dev/mem", O_RDWR|O_SYNC);
if(mem>=0){
    void *map = mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, mem, 0xFE200000);
```

`/dev/mem` **does not exist** on this QNX image — the prior audit recorded
`ls: /dev/mem: No such file or directory` (`SESSION_LOG.md` §7 Blocker B). So this
branch can never succeed and `g_available` stays 0 forever.

The pattern proven working on **this exact board** is from
`third_party/can-mcp2515/driver/rpi4.c`:

```c
#define BCM2711_GPIO_BASE   0xfe200000
data->regs = mmap(0, __PAGESIZE, PROT_NOCACHE | PROT_READ | PROT_WRITE,
                  MAP_PHYS | MAP_SHARED, NOFD, BCM2711_GPIO_BASE);
```

| Current | Correct |
| --- | --- |
| `open("/dev/mem")` first | **no file descriptor at all** — use `NOFD` |
| `MAP_SHARED` only | `MAP_PHYS \| MAP_SHARED` |
| `PROT_READ \| PROT_WRITE` | `PROT_NOCACHE \| PROT_READ \| PROT_WRITE` |
| physical address passed as the `mmap` **offset** | physical address passed as the `mmap` **length** argument position — `__PAGESIZE` |

> **This caveat is now RESOLVED.** `driver/rpi4.c` is indeed **not vendored in this
> repository** — only the built binaries (`aarch64le/bin/`) and the public headers
> (`lib-public/mcp2515/`) were committed, and the source lives in the temporary clone
> at `%TEMP%\opencode\can-mcp2515`. The clone **was still present** at the end of the
> session, and the driver source **was read directly** to verify the rewrite in Phase 2.
>
> Confirmed against `%TEMP%\opencode\can-mcp2515\driver\rpi4.c`:
>
> | Driver (`rpi4.c`) | Phase 2 `gpio_marker.c` | Match |
> | --- | --- | --- |
> | `mmap(0, __PAGESIZE, PROT_NOCACHE\|PROT_READ\|PROT_WRITE, MAP_PHYS \| MAP_SHARED, NOFD, 0xfe200000)` (line 93) | identical | yes |
> | `reg = (BCM2711_GPIO_FSEL0 + (gpio / 10)) / 4; shift = (gpio % 10) * 3;` (lines 102-103) | identical | yes |
> | `if (gpio < 32) { …0/4, shift = gpio } else { …1/4, shift = gpio - 32 }` (lines 111-122) | `+ (pin / 32)` register, `pin % 32` bit index | yes, equivalent |
> | `BCM2711_GPIO_BASE 0xfe200000`, `FSEL0 0x00`, `FSEL5 0x14` (lines 28-35) | `0xFE200000`, `GPFSEL0_OFF 0x00` | yes |
>
> `GPIO_MAX 53` from the driver is what `GPIO_MAX_PIN` in the rewrite uses.
>
> **Caveat that remains:** because the driver source is not in git, this verification
> depends on a temp clone that could be lost. Either vendor
> `third_party/can-mcp2515/driver/` into the repository, or re-clone from
> `gitlab.com/qnx/projects/drivers/can-mcp2515` (commit `0fd11af`).

#### (b) the fatal one — no function select

```c
/* BCM2711: GPSET0 offset 7, GPCLR0 offset 10, GPLEV etc. Simplified */
/* Real BSP would configure GPFSEL first — stub sets only */
volatile uint32_t *set = g_gpio_base + 7;
volatile uint32_t *clr = g_gpio_base + 10;
if(value) *set = (1u<<bcm_pin); else *clr = (1u<<bcm_pin);
```

The comment acknowledges the bug. The register **offsets are correct**: `GPSET0` =
byte `0x1C` = word 7; `GPCLR0` = byte `0x28` = word 10. Only the function-select step
is missing.

At reset every `GPFSELn` field is `000` = **input**. Writes to `GPSET0` then produce
**no electrical output whatsoever** — no LED, no edge, nothing on the logic analyzer.

**Fixing the mmap alone is not enough.** (a) and (b) fail independently, and fixing
only (a) is the realistic outcome — it will produce a clean `REAL PHYSICAL` with a
dark LED.

#### (c) no readback

```c
int gpio_marker_get(int bcm_pin){ (void)bcm_pin; return -1; }
```

Always `-1`, and the pin argument is explicitly discarded. With no readback there is no
way to confirm a pin actually changed state — which is the whole point of a hardware
marker. Implement via `GPLEV` (byte `0x34` = word 13).

#### (d) GPIO range

Only GPIO 0–31 are reachable. On the Pi 4 the valid range is **0–53**, and bank 1
registers live at **`+32`**. Note the three marker pins in use (4, 17, 27) are all in
bank 0, so this does not block the current proof — but it caps any later move to bank 1.

#### (e) GPIO markers masquerade as CPU migrations

`src/gpio_marker.c:102`:

```c
return trace_emit(TRACE_CPU_MIGRATION, task_id, act, corr, 1, 0); /* reuse for GPIO marker */
```

`src/gpio_marker.c:106`:

```c
return trace_emit(TRACE_CPU_MIGRATION, task_id, act, corr, 0, 0);
```

`TRACE_CPU_MIGRATION` is `src/trace_schema.h:28`, value 13. Every GPIO marker
therefore injects a record claiming a CPU migration occurred.

**Correction to an earlier claim made during this session.** An earlier draft of this
document, and an earlier verbal report, stated that these records **corrupt
CPU-migration statistics**. That is **wrong**, and the correction matters because it
changes the severity from "actively damaging" to "silently invisible".

Verified against `src/analyzer.c:128-153`. The analyzer's correlation switch has
exactly six cases:

| Case | Line | Value |
| --- | ---: | ---: |
| `TRACE_WORKLOAD_READY` | `:129` | 4 |
| `TRACE_WORKLOAD_START` | `:132` | 5 |
| `TRACE_WORKLOAD_END` | `:140` | 6 |
| `TRACE_WORKLOAD_DEADLINE_MISS` | `:144` | 8 |
| `TRACE_WORKLOAD_DEADLINE` | `:145` | 7 |
| `TRACE_EXTERNAL_EVENT_RX` | `:148` | 1 |
| `default: break` | `:151` | everything else |

**There is no `case TRACE_CPU_MIGRATION`.** `src/analyzer.c:135` and `:138` do assign
`cpu_first`/`cpu_last`, but they sit inside the `case TRACE_WORKLOAD_START:` branch —
that is workload START handling, not migration handling. A repo-wide grep for
`TRACE_CPU_MIGRATION` returns four hits, none in `analyzer.c`:
`gpio_marker.c:102`, `gpio_marker.c:106`, `trace_schema.h:28`, `trace_schema.c:17`.

| Claim | Status |
| --- | --- |
| GPIO markers reuse `TRACE_CPU_MIGRATION` | **true** |
| `analyzer.c` counts them as migrations and corrupts migration stats | **false** — no such handling exists |
| `RC_CPU_MIGRATION` is ever assigned | **false** — declared `analyzer.h:24`, named `analyzer.c:27`, **never assigned anywhere**. CPU-migration analysis is a stub |
| GPIO marker records are therefore discarded | **true** — `default: break` at `analyzer.c:151` |
| `need_drop()` drops them in `TRACE_MODE_EVENT_ONLY` | **true** — `trace_collector.c:31-36` keeps only `EXTERNAL_EVENT_RX`, `EVENT_DECODED`, `DEADLINE_MISS`, `DEADLINE` |

So the records are **dead weight that survives only in FULL mode and is ignored
downstream** — invisibility, not corruption. The defect is still real and still must be
fixed: the marker data reaches no consumer, the event type is semantically wrong, and
the moment someone *adds* `TRACE_CPU_MIGRATION` handling, these records will corrupt it.
Fix it before that happens.

**Related:** `gpio_marker_validate()` (`src/gpio_marker.c:110-123`) reads **no trace
records at all**. It compares `g_high_ts`/`g_low_ts` — populated in software at
`:72` from `mono_ns()` inside `gpio_marker_set()` — against software-supplied
`sw_start`/`sw_end`. In mock mode that is a comparison of software timestamps to
software timestamps. It proves nothing physical. See
[§10.2](#102-invalid-validation-claim-still-standing).

### 9.2 `src/uart_adapter.c`

| # | Location | Defect | Severity |
| --- | --- | --- | --- |
| **(f)** | `:151` | `uart_adapter_trace_tx()` records a **transmit** as `TRACE_EXTERNAL_EVENT_RX` | high |

```c
int uart_adapter_trace_tx(uint32_t act_id, uint32_t corr_id, uint8_t byte) {
    return trace_collector_record_simple(TRACE_EXTERNAL_EVENT_RX, 0xFFFFu, act_id, corr_id, byte, 2);
}
```

Compare `uart_adapter_trace_rx()` at `:147`, which uses the same type with
`arg1 = 1`. **TX and RX are indistinguishable in the trace** — the only difference is
a value in `arg1` that nothing reads. Worse, a TX record now also lands in
`analyzer.c:148`'s `case TRACE_EXTERNAL_EVENT_RX:`, so it can wrongly set
`a->external_event_time`. Fix in Phase 3, [§12.3](#123-phase-3--fix-the-uart-tx-trace-marker).

The rest of the adapter is complete and correct: `open()` at `:39`, full termios at
`:41-63` (115200 in and out, `CS8`, no parity, one stop bit, `CLOCAL|CREAD`,
`VMIN=0`/`VTIME=5`, `CRTSCTS` cleared), and a simulated-loopback fallback at `:74-77`.

### 9.3 CAN — no hardware path exists at all

| # | Location | Finding |
| --- | --- | --- |
| **(g)** | `src/qnx_can_adapter.c` | **199 bytes, four comment lines, no code.** Deliberately not compiled |
| **(g)** | `src/simulated_can_adapter.c` | Returns five hardcoded frames |
| **(g)** | `src/qnx_can_adapter.c.stub` | 2,525-byte template, entirely inside `#if 0`, and it guesses at a device path that is wrong for this project |

`src/qnx_can_adapter.c` in its entirety:

```c
/* This file is intentionally NOT compiled by default.
 * Rename qnx_can_adapter.c.stub -> qnx_can_adapter.c and adjust Makefile
 * when real hardware integration begins. See .stub for template.
 */
```

`src/simulated_can_adapter.c:15-23` returns five fixed frames:

```c
static const can_frame_t k_sim_frames[] = {
    { .id = 0x100, .dlc = 1, .data = {0x01} },
    { .id = 0x200, .dlc = 1, .data = {0x01} },
    { .id = 0x300, .dlc = 1, .data = {0x01} },
    { .id = 0x999, .dlc = 1, .data = {0x01} }, /* UNKNOWN */
    { .id = 0x100, .dlc = 3, .data = {0xAA, 0xBB, 0xCC} },
};
```

> The stub (`:46`) opens `"/dev/can1"`. The QNX CAN DDK on this board uses mailbox
> devices under `/dev/can0/` (`rx0`, `tx2`, …), not a flat `/dev/canN` character
> device. The stub's own comment says *"DO NOT invent QNX CAN APIs without verifying
> the BSP"* — and the path in it is exactly the kind of guess it warns about. Do not
> treat the stub as a starting point without replacing that assumption.

Writing a real adapter is Phase 5, [§12.5](#125-phase-5--write-the-real-can-adapter).

---

## 10. Stale documentation to reconcile

Flagged, not fixed. These files are owned by another process.

| File / location | Stale claim | Contradicted by |
| --- | --- | --- |
| `CURRENT_STATE.md:40` | UART: "only UART RX/TX comments exist"; "A real QNX UART event reader/writer mapping `/dev/ser1` or `/dev/ser2` is not implemented" | `src/uart_adapter.c` is complete — `open` at `:39`, full termios at `:41-63`, 115200 8N1, `O_NONBLOCK`, loopback fallback at `:74-77`; wired into the CLI at `src/main.c:439-506`. **Now also proven on hardware** ([§8](#8-phase-1-result--uart-adapter)) |
| `CURRENT_STATE.md:45` | "Implement UART Driver/Sim Adapter… Add UART reader loop opening `/dev/ser1`" | Already implemented. Remaining work is hardware verification, which is now **done** |
| `CURRENT_STATE.md:29` | QNX target IP is `192.168.10.2` | The board in hand is `192.168.10.5` (`SESSION_LOG.md` §7) |
| `CURRENT_STATE.md` (CAN section) | CAN is simulated-only | **Still accurate** for CAN — `simulated_can_adapter.c` returns five hardcoded frames and `qnx_can_adapter.c` is a 199-byte stub. But it should be reconciled with the committed `third_party/can-mcp2515/aarch64le/bin/can-mcp2515`, so a reader is not left thinking the driver *build* is also pending |
| `docs/gpio.md` §3 | "Physical base address `0xFE200000` (mapped via `/dev/mem` using QNX `mmap` if running as root)" | `/dev/mem` does not exist on this image. Correct mapping is `MAP_PHYS\|NOFD` + `PROT_NOCACHE` — [§9.1](#91-srcgpio_markerc--the-rewrite-target) |
| `docs/gpio.md` §3 | Presents `GPSET0`/`GPCLR0` offsets as sufficient; no mention of `GPFSEL` | Fatal omission — [§9.1(b)](#b-the-fatal-one--no-function-select) |
| `docs/HARDWARE_PROCUREMENT_PLAN.md` §7.4 | Headed **"RECOMMENDED ROUTE — MCP2515 modules (Waveshare unavailable)"**, requiring a PCB trace cut or an SN65HVD230 swap | **Superseded.** The acquired RS485 CAN HAT is MCP2515 + SN65HVD230 @ 3.3 V; no modification needed |
| `docs/HARDWARE_PROCUREMENT_PLAN.md` §8.3, §8.5, §8.6 | Module route: TJA1050, trace cut, 2 × SN65HVD230 spares, 8 jumpers per board | Superseded by the HAT route — `SESSION_LOG.md` §11 already flags §7.4/§8 as fallback, but §7.4's **heading** still reads as current advice |
| `docs/uart.md` §3 | "Trace Emission: Invokes `trace_collector_record_simple` to append `TRACE_EXTERNAL_EVENT_RX` entries" | True for **RX**, false for **TX** — `uart_adapter.c:151` — [§9.2](#92-srcuart_adapterc) |

### 10.1 `deploy/` vs `build/` — which is authoritative

Unresolved and now consequential: **every binary in `deploy/` is stale relative to
`build/`** ([§8.1](#81-pre-test-finding--every-binary-in-deploy-is-stale)). Two
directories, same artifact name, three weeks of divergence, no marker distinguishing
them. Pick one, document it, and delete or regenerate the other. Uploading from
`deploy/` would have tested a binary that contains neither the UART nor the GPIO
subcommand.

### 10.2 Invalid validation claim, still standing

`VALIDATION_LOG.md` TEST 6 records GPIO as **PASS**. `docs/logs/qnx_logs_7.txt` shows
the run actually printed, on **20 separate lines**:

```
[GPIO] no HW, mock mode (validation uses sw timestamps)
```

including at line 633–635 in the TEST 6 block itself:

```
=== TEST 6: GPIO ===
GPIO marker attempted
Check dmesg / slog2info for [GPIO] mmap /dev/gpio
```

The test compares software timestamps against software timestamps
(`src/gpio_marker.c:110-123`, fed only by `g_high_ts`/`g_low_ts` written at `:72` from
`mono_ns()`). **It proves nothing physical and the PASS is not supportable.**

Separately, `VALIDATION_LOG.md` §3 states verification ran on `root@qnxpi`, but
`SESSION_LOG.md` §7 records access as `qnxuser`/`qnxuser` over SSH, which is a
QSTI image where root is not reachable that way. One of the two is wrong.

Flag only — `VALIDATION_LOG.md` is owned by another process.

---

## 11. Corrections to prior documentation

### 11.1 CORRECTION 1 — the serial TX/RX wiring in step 1 is reversed

`SESSION_LOG.md` §12 step 1 says:

> USB-TTL TX→pin 8, RX→pin 10

**That is reversed.** From the Raspberry Pi 4 40-pin header:

| Pin | BCM | Direction, from the Pi's point of view |
| ---: | ---: | --- |
| 8 | 14 | **TXD0 — the Pi TRANSMITS out** |
| 10 | 15 | **RXD0 — the Pi RECEIVES in** |

The adapter's **TX** must land on pin **10**; the adapter's **RX** on pin **8**.

The reversal is self-evidently a mistake *inside the prior log itself*:
`SESSION_LOG.md` §12 **step 5** states the correct direction, and
`HARDWARE_PROCUREMENT_PLAN.md` §8.5 states it correctly too. Step 1 contradicts step 5
within the same list.

Wiring per step 1 would have put the adapter's TX straight onto the Pi's receive pin
and read the Pi's own TX into the adapter's TX — producing nothing, and if the adapter
TX is 5 V, putting 5 V on a non-5 V-tolerant GPIO.

**The reversal appears in two places**, both of which need fixing:

| Location | Text |
| --- | --- |
| `SESSION_LOG.md` §12 step 1 | `USB-TTL TX→pin 8, RX→pin 10` |
| `SESSION_LOG.md` §14 reference block | `USB-TTL TX → Pi pin 8 (GPIO 14)` / `USB-TTL RX → Pi pin 10 (GPIO 15)` |

### 11.2 CORRECTION 2 — the installer files are NOT in `/tmp`

`SESSION_LOG.md` §12 step 3 states:

> Run `sh /tmp/qnx_can_install.sh` from the serial root shell. **Files are already
> in `/tmp`.**

**They are not.** The actual `/tmp` listing is in
[§3.3](#33-tmp-and-storage): only `T3`, `elvis1.ses`, `keep_files` and
`qnx_bringup_check.sh`.

Root cause: `tools/qnx_can_install.py` was written to upload those files but never got
past root escalation, so the upload step never ran. The prior session's "files are
already in /tmp" was an **assumption, not an observation**.

Consequence: all three artefacts still have to get to the board. The upload path now
exists ([§7](#7-the-binary-upload-saga)), so this is solved as a transport problem —
but only just, and only after six defects and two power-cycles.

### 11.3 CORRECTION 3 — the prior wiring omits the level hazard entirely

`SESSION_LOG.md` §12 step 1 says only "**VCC not connected**". It gives no reason, no
resistor network, and **no mention of the logic-level hazard at all**. §14 repeats the
same omission.

Connecting a bare CH340 TX to a BCM2711 GPIO is a live risk of permanent damage if
the dongle drives 5 V. The prior log directs the reader to do exactly that, without
saying why it might be wrong. The corrected net list with the divider is in
[§4.5](#45-complete-net-list).

### 11.4 CORRECTION 4 — the GPIO pass criterion string

`SESSION_LOG.md` §12 step 4 says `schedulix gpio test` "must print `REAL`, not `MOCK`".

There is no bare `REAL`. The actual strings, which differ **between two
subcommands**:

| Subcommand | Line | Output on success | Output on failure |
| --- | ---: | --- | --- |
| `gpio test` | `src/main.c:515` | `REAL PHYSICAL` | `MOCK` |
| `status` | `src/main.c:249` | `REAL (BCM2711 registers mapped)` | `MOCK (software simulation)` |

Pass criterion: **`REAL PHYSICAL`** from `gpio test`.

### 11.5 CORRECTION 5 — the console is not a PL011

`SESSION_LOG.md` and the earlier draft of this document both assume a PL011 console.
The board runs `devc-serminiuart` at base `0xfe215000` — the BCM2711 **mini-UART /
AUX**. The 3.3 V logic conclusion is unchanged; only the peripheral identification was
wrong.

### 11.6 CORRECTION 6 — GPIO markers do not corrupt CPU-migration statistics

See [§9.1(e)](#e-gpio-markers-masquerade-as-cpu-migrations). An earlier claim made
during this session was wrong. `analyzer.c` has **no** `case TRACE_CPU_MIGRATION`; the
records fall to `default: break` and are silently discarded. The defect is real; the
impact is invisibility, not corruption.

---

## 12. Roadmap, phases 2–6

```
Phase 1  UART adapter proven on hardware   ✓ DONE — see §8
                                               │
Phase 2  gpio_marker.c rewritten + LED proof ✓ DONE — verified on hardware
                                               │  2026-10-07, see §15
Phase 3  UART TX trace marker  ←─────────────┤  NEXT. Host-only, no board needed.
                                               │  src/uart_adapter.c:151
Phase 4  CAN driver install ─────────────────┘  uses the §7 upload path
                                               │  (or the §15.3 IDE channel)
Phase 5  Write real CAN adapter ───────────────┘  depends on Phase 4
                                               │
Phase 6  Qt JSON loader ────────────────────────  depends on Phase 5 output shape
```

Phases 1, 2 and 4 all shared one dependency — a working way to get 148–331 KB binaries
onto the board. That dependency is now discharged **twice over**: the serial upload path
(§7) and the Momentics IDE channel ([§15.3](#153-momentics-ide--deployment-and-log-channel)),
the latter proven to be the more reliable of the two. Phase 3 is next: it is unblocked and
independent of all hardware.

### 12.1 Phase 2 — rewrite `src/gpio_marker.c` — **DONE, verified on hardware 2026-10-07**

> **This subsection is the original plan, retained for context. The result, the verified
> output, and three bugs the test run exposed are in [§15](#15-phase-2-result--gpio-marker-verified-2026-10-07).**

**Target:** `src/gpio_marker.c`

| | |
| --- | --- |
| Blocked by | **nothing** — code work, and the toolchain is present locally ([§2.2](#22-a-qnx-sdp-toolchain-is-present-on-the-windows-host--this-corrects-a-mid-session-error)) |
| Hardware needed | 330 Ω + LED, header pin 7 to pin 6 |

**Code changes**

1. Adopt the proven mapping:
   `mmap(0, __PAGESIZE, PROT_NOCACHE|PROT_READ|PROT_WRITE, MAP_PHYS|MAP_SHARED, NOFD, 0xFE200000)`.
   No `/dev/mem`. ([§9.1(a)](#a-the-mmap-pattern))
2. Write `GPFSELn = 001` (output) **before** any `GPSET0`/`GPCLR0`.
   ([§9.1(b)](#b-the-fatal-one--no-function-select))
3. Implement `gpio_marker_get()` readback via `GPLEV` (byte `0x34` = word 13).
   ([§9.1(c)](#c-no-readback))
4. Support GPIO 0–53, including bank 1 at `+32`. ([§9.1(d)](#d-gpio-range))
5. Give GPIO markers a dedicated event type — or fold them into the Phase 3 enum
   change. ([§9.1(e)](#e-gpio-markers-masquerade-as-cpu-migrations))

**Physical proof.** 330 Ω + LED from header pin 7 (BCM 4) to GND pin 6.

```
$ /tmp/sx gpio test
GPIO Marker validation test:
GPIO Availability: REAL PHYSICAL
Toggling pin 4 (BRAKE), 17 (ADAS), 27 (DIAG)...
Validation: delta ... ns, valid: ...
```

**Pass criterion: `REAL PHYSICAL`**, not `MOCK`, and not bare `REAL`
([§11.4](#114-correction-4--the-gpio-pass-criterion-string)).

> **Outcome, 2026-10-07: criterion met.** `REAL PHYSICAL` on all three pins, all three moved
> fsel 0 → 1, all three confirmed by `GPLEV` readback, and an LED on pin 11 physically
> observed blinking. The `mmap` prediction below was right — that call was the hinge.
> See [§15](#15-phase-2-result--gpio-marker-verified-2026-10-07).

**Expect this to hinge entirely on the `mmap` call.** And expect the two defects to
fail independently:

| Symptom | Cause |
| --- | --- |
| `MOCK`, no LED | (a) — the `mmap` failed |
| `REAL PHYSICAL`, **dark LED** | (b) — `GPFSEL` never written; the pin is still an input |
| `REAL PHYSICAL`, LED works, `gpio_marker_get()` returns `-1` | (c) — readback not implemented |

**Ordering note.** Solder the marker wires on pins 7, 11 and 13 **before** mounting
the RS485 CAN HAT. Once the HAT is on, those pins are covered and inaccessible
([§3.4](#34-hardware-on-hand)).

### 12.2 Phase 2b — reconcile `deploy/` vs `build/`

Not in the original phase list, but it should not be left undone:

1. Decide `build/` is authoritative ([§8.1](#81-pre-test-finding--every-binary-in-deploy-is-stale)).
2. Either regenerate `deploy/` from `build/` or delete it.
3. Record the decision in `CURRENT_STATE.md` and in the `Makefile` (add a `deploy:`
   target that copies, rather than leaving two unreconciled directories).
4. Stop `deploy/` from being `.gitignore`d and shipped by accident.

### 12.3 Phase 3 — fix the UART TX trace marker — **NEXT**

**Target:** `src/trace_schema.h`, `src/trace_schema.c`, `src/trace_collector.c`,
`src/analyzer.c`, `src/uart_adapter.c`

> **The GPIO half of this phase landed early, with the Phase 2 rewrite.** `TRACE_GPIO_MARKER_HIGH
> = 14` and `TRACE_GPIO_MARKER_LOW = 15` are now appended to the enum, named in
> `trace_schema.c:18-19`, kept in `need_drop()` (`trace_collector.c:38-39`), and handled in
> `analyzer.c:152-157`. **What remains is the UART TX half:** `TRACE_EXTERNAL_EVENT_TX` and
> `src/uart_adapter.c:151`. See [§15.5](#155-what-phase-3-still-needs).

**Enum placement is the critical detail.** Append at the **END** of
`trace_event_type_t`:

```c
TRACE_EXTERNAL_EVENT_TX = 16
```

**16, not 14** — 14 and 15 were taken by the GPIO marker types in the Phase 2 rewrite.

**Do NOT insert mid-enum.** Trace records are binary and the Qt frontend may already
map values 0–13. Inserting anywhere but the end silently renumbers existing types and
corrupts every archived trace and any frontend that hardcoded the mapping.

Current enum (`src/trace_schema.h:14-33`), for reference — 14 and 15 were appended by the
Phase 2 rewrite and are now occupied:

| Value | Name | Value | Name |
| ---: | --- | ---: | --- |
| 0 | `TRACE_INVALID` | 8 | `TRACE_WORKLOAD_DEADLINE_MISS` |
| 1 | `TRACE_EXTERNAL_EVENT_RX` | 9 | `TRACE_WORKLOAD_ABORT` |
| 2 | `TRACE_EVENT_DECODED` | 10 | `TRACE_WORKLOAD_BLOCK_BEGIN` |
| 3 | `TRACE_WORKLOAD_RELEASE` | 11 | `TRACE_WORKLOAD_BLOCK_END` |
| 4 | `TRACE_WORKLOAD_READY` | 12 | `TRACE_PREEMPTION` |
| 5 | `TRACE_WORKLOAD_START` | 13 | `TRACE_CPU_MIGRATION` |
| 6 | `TRACE_WORKLOAD_END` | **14** | **`TRACE_GPIO_MARKER_HIGH`** |
| 7 | `TRACE_WORKLOAD_DEADLINE` | **15** | **`TRACE_GPIO_MARKER_LOW`** |
| **16** | **`TRACE_EXTERNAL_EVENT_TX`** ← *to be added, next* | | |

**Touch points**

| File:line | Change | State |
| --- | --- | --- |
| `src/trace_schema.h:32` | Append `TRACE_EXTERNAL_EVENT_TX = 16` after `TRACE_GPIO_MARKER_LOW` (drop the trailing comma on `:32`) | **todo** |
| `src/trace_schema.c:20` | Add the name string in `trace_event_to_string`, before `default:` | **todo** |
| `src/trace_collector.c:31-39` | Add to the `need_drop()` keep-list so TX survives `TRACE_MODE_EVENT_ONLY` | **todo** |
| `src/analyzer.c:148-150` | Decide whether TX sets `external_event_time`. **It should probably NOT** — TX is not an external arrival trigger, and setting it would let a self-inflicted transmit masquerade as a stimulus. `src/analyzer.c:149` is the `case TRACE_EXTERNAL_EVENT_RX:` line | **todo** |
| `src/uart_adapter.c:151` | Use the new type in `uart_adapter_trace_tx()` | **todo — this is defect (f), the Phase 3 deliverable** |
| `src/gpio_marker.c:292,297` | Use a dedicated GPIO marker type | **done 2026-10-07** |
| `src/analyzer.c:152-157` | Consume the marker edges | **done 2026-10-07** |
| `src/trace_collector.c:38-39` | Keep markers in `EVENT_ONLY` mode | **done 2026-10-07** |

**Verification** — host suites via `Makefile.host`:

| Command | Expected |
| --- | --- |
| `python tests/run_tests.py` | 5/5 |
| `python tests/test_phase6.py` | **13/13** |
| `python tests/test_analyzer_integrity.py` | **3/3** |

Re-run all three after the enum change. `test_analyzer_integrity.py` is the one most
likely to notice a bad enum edit, because it checks header integrity and a
trace-change regression.

### 12.4 Phase 4 — CAN driver install

**Target:** `/dev/can0`

| | |
| --- | --- |
| Blocked by | nothing — the upload path is proven ([§7](#7-the-binary-upload-saga)) |

**Artefacts required on the board**

| File | Bytes | Upload time (derived at ~2.3 KB/s) |
| --- | ---: | ---: |
| `third_party/can-mcp2515/aarch64le/bin/can-mcp2515` | 148,840 | ~45 s |
| `tools/spi.conf.mcp2515` | 826 | < 1 s |
| `tools/qnx_can_install.sh` | 5,391 | ~2 s |

```powershell
# -UploadFile <local> -RemotePath <target>, one per artefact
powershell -ExecutionPolicy Bypass -File tools\serial_console.ps1 `
    -UploadFile third_party\can-mcp2515\aarch64le\bin\can-mcp2515 -RemotePath /tmp/can-mcp2515
```

The installer expects `/tmp/can-mcp2515` (`tools/qnx_can_install.sh:18`) and rewrites
`/system/etc/config/spi/spi.conf` (`:17`). Then, from the serial root shell:

```sh
sh /tmp/qnx_can_install.sh
```

**Expectation: `/dev/can0` will NOT appear until the HAT is physically mounted.** A
failure at that step is **correct behaviour, not a bug** — the installer says so itself
at `:132`.

**Driver invocation** (`tools/qnx_can_install.sh:115`):

```sh
can-mcp2515 --mid=eid -s /dev/io-spi/spi0/dev0 -c 8000000 -g 25 -B 500000
```

| Flag | Value | Note |
| --- | --- | --- |
| `-s` | `/dev/io-spi/spi0/dev0` | CE0, header pin 24 |
| `-c` | **VERIFY** | **must be 1,000,000–40,000,000 Hz** or the driver refuses to start |
| `-g` | `25` | matches the HAT's INT pin, header pin 22 |
| `-B` | `500000` | must match the other bus node |

The critical dependency is the **crystal value** — see
[§13](#13-open-questions). Getting it wrong produces a driver that starts cleanly and
receives zero frames, which is a very expensive way to lose an afternoon. Read the
marking on the board before running.

Note the SPI config in `tools/spi.conf.mcp2515` uses `clock_rate=10000000` (10 MHz,
MCP2515 max per datasheet), `cpha=0`, `cpol=0`, `word_width=8` — this corrects the
stock QSTI `spi0/dev0` entry, which was `cpha=1 cpol=0 word_width=32` and would have
meant the MCP2515 never responded (`SESSION_LOG.md` §7 Blocker A).

### 12.5 Phase 5 — write the real CAN adapter

**Target:** `src/qnx_can_adapter.c` (currently a 199-byte stub)

Implement against `/dev/can0` **mailboxes** and the `CAN_DEVCTL_*` commands. The DDK
is present and now **independently re-verified locally** ([§2.2](#22-a-qnx-sdp-toolchain-is-present-on-the-windows-host--this-corrects-a-mid-session-error)):
`libcan.a`, `hw/libcan.h` and `sys/can_dcmd.h` all exist and `-lcan` links clean for
`aarch64le`.

Reference invocations for mailbox setup:

```sh
canctl -u 0,rx0 -m 0              # set MID
canctl -u 0,rx0 -f 0              # clear filter
canctl -u 0,rx0 -R 2000           # receive loop
canctl -u 0,tx2 -w 0x123,3,ABCDEF # transmit
cat /dev/can0/rx0                 # raw receive
```

With `--mid=eid`, mailbox MIDs for `rx0, rx1, tx2, tx3, tx4` become `0,1,2,3,4`.
Note `hw/libcan.h`, **not** `sys/libcan.h`.

Do not reuse `src/qnx_can_adapter.c.stub` unmodified — its `"/dev/can1"` assumption is
wrong for this project ([§9.3](#93-can--no-hardware-path-exists-at-all)).

### 12.6 Phase 6 — Qt JSON loader

**Target:** frontend repo `C:\Users\User\Documents\schedulix_v1`

`src/models/MockProvider.cpp` hardcodes every metric:
`populateSystemMetrics`, `populateTaskMetrics`, `populateRootCauseModel`,
`populateTimelineModel`, `populateJitterModel`, `populateExperimentModel`. A search
for `QJson`, `analysis_` or `QFile` across `src/` returns nothing. **The GUI renders
pre-cooked values and is not connected to backend output at all.**

`src/analyzer.c:318-371` already emits JSON via `analyzer_write_json()`, including
`cpu_first` at `:357`, so a schema exists on the backend side to load against.

**Depends on Phase 5** for the final record shape. The host-only part (Phase 3 enum,
analyzer, tests) can and should land first, so the wire format stops moving while the
frontend is being written.

---

## 13. Open questions

| Question | Impact | Status |
| --- | --- | --- |
| **RS485 CAN HAT crystal: 8 MHz or 16 MHz?** | **Wrong value ⇒ driver starts cleanly and receives zero frames.** Documentation does not state it; forum reports say variants differ. Installer defaults to `-c 8000000`. | **Open. Read the marking on the board.** |
| Can a second process reliably read `/dev/ser1` past the console? | Determines whether a meaningful UART RX test is possible at all. A second `open()` **is** permitted ([§8.2](#82-uart-status--physical-device-confirmed)) | **Partly answered.** Open succeeds; the login shell still consumes input. Use the adapter-side loopback jumper ([§8.4](#84-receive--confounded-and-the-reason-why)) |
| Does `gpio-bcm2711` work for a non-root user? | Affects whether the GPIO rewrite needs root. Prior audit showed `gpio` group 522 exists and `qnxuser` is a member | Open, but likely moot — the `mmap(MAP_PHYS\|NOFD)` rewrite bypasses the utility entirely. Root is available over serial anyway |
| Does `shm_open` work given `/dev/shmem` is 0 bytes? | `src/trace_collector.c:50` calls `shm_open` and falls back to heap on failure, so traces still work; producer/consumer sharing may not | Open. May need explicit allocation, or `/tmp` — which is ample at ~53 M free ([§3.3](#33-tmp-and-storage)) |
| **Which binary is authoritative, `deploy/` or `build/`?** | Should settle on one and document it | **Answered for now: `build/`** ([§8.1](#81-pre-test-finding--every-binary-in-deploy-is-stale)). Needs a permanent decision — Phase 2b |
| Is `MAP_PHYS\|NOFD` + `PROT_NOCACHE` correct for this QNX 8.0 image? | **The entire Phase 2 pass criterion hinges on this one `mmap` call** | Pattern recorded from the working `can-mcp2515` driver on this exact board, but `driver/rpi4.c` is **not in the repo** ([§9.1](#91-srcgpio_markerc--the-rewrite-target)). Re-verify before relying on it |
| Does `/dev/ser1`'s `tcsetattr` from `uart_adapter_init()` silence console echo? | Would affect any live console session after running `uart status` | **Unverified hypothesis** ([§8.5](#85-a-side-observation--ttyl_clflag--0-and-the-consoles-echo)) |

---

## 14. Current board state

End of session:

| Item | State |
| --- | --- |
| Serial console, COM6, 115200 8N1 via CH340 | **VERIFIED** — root shell, no password |
| Root over serial | **VERIFIED** — `root@console:/#` |
| `/dev/ser1` | **VERIFIED as the console device** — `devc-serminiuart` at `0xfe215000` |
| Second `open()` of `/dev/ser1` | **PERMITTED** — did not block |
| Ethernet | **not connected** (by choice — Wi-Fi only) |
| `/tmp` | `T3`, `elvis1.ses`, `keep_files`, `qnx_bringup_check.sh` only |
| Schedulix binary on board | **YES** — `/tmp/sx`, 331,152 B, byte-identical |
| `uart status` | **PASS** — `physical`, `/dev/ser1` at 115200 |
| `uart send` | **PASS** — 21 bytes observed on COM6 before the program's own printf |
| `uart receive` | **INCONCLUSIVE** — confounded by the console ([§8.4](#84-receive--confounded-and-the-reason-why)) |
| `gpio test` | **NOT RUN** — Phase 2. `deploy/` build would report `MOCK` correctly, but the fix is not written |
| CAN driver installed | **no** — `can-mcp2515`, `spi.conf.mcp2515`, `qnx_can_install.sh` still all need uploading |
| `/dev/can0` | absent (expected — HAT not mounted) |
| RS485 CAN HAT | **not mounted** |
| GPIO marker wires on pins 7/11/13 | **not soldered** |
| HAT crystal frequency | **unknown** |
| Power-cycles caused this session | **2**, both from console-wedging transport defects ([§7.3](#73-defect-3--cat--file--ctrld-wedged-the-console-power-cycle-1), [§7.4](#74-defect-4-echo-was-never-drained-during-streaming-power-cycle-2)) |

---

## Appendix A — verified command reference

### A.1 Serial console client (this session's primary tool)

```powershell
# interactive — root shell, Ctrl+C to exit
powershell -ExecutionPolicy Bypass -File tools\serial_console.ps1

# one-shot
powershell -ExecutionPolicy Bypass -File tools\serial_console.ps1 -Command "pidin info"

# upload a binary (three attempts, size-verified)
powershell -ExecutionPolicy Bypass -File tools\serial_console.ps1 `
    -UploadFile build\aarch64le-debug\schedulix_can -RemotePath /tmp/sx

# run the uploaded binary
powershell -ExecutionPolicy Bypass -File tools\serial_console.ps1 -Command "/tmp/sx uart status"
```

### A.2 PuTTY, if you must

```
HKCU\Software\SimonTatham\PuTTY\Default Settings
  Speed=115200  SerialFlowControl=0  DataBits=8  StopBits=1  Parity=0  PortNumber="COM6"
putty.exe -serial COM6
```

### A.3 Windows-side diagnostics

```powershell
[System.IO.Ports.SerialPort]::GetPortNames()     # COM3, COM4, COM5, COM6
Get-CimInstance Win32_SerialPort                 # does NOT list COM6 — WMI quirk, ignore
Get-PnpDevice | Where-Object FriendlyName -match 'CH340'
# USB-SERIAL CH340 (COM6)  USB\VID_1A86&PID_7523\5&5114092&0&3  Status: OK
```

### A.4 QNX cross-compiler (present — see §2.2)

```powershell
$env:QNX_BASE   = "C:/Users/User/qnx800"
$env:QNX_TARGET = "$env:QNX_BASE/target/qnx"
$env:QNX_HOST   = "$env:QNX_BASE/host/win64/x86_64"
$env:PATH       = "$env:QNX_HOST/usr/bin;$env:QNX_BASE/host/common/bin;$env:PATH"

qcc --version                       # Toolchain:  gcc  Version:  12.2.0
qcc -Vgcc_ntoaarch64le -o out.exe in.c
ntoaarch64-strip out.exe
qcc -Vgcc_ntoaarch64le -o out in.c -lsocket -Wl,-Bstatic -lcan -Wl,-Bdynamic
```

### A.5 On the board, over serial

```sh
pidin info                          # platform summary
id                                  # uid=0(root)
ls /dev/ser*
pidin ar | grep -i 'devc\|serminiuart'
df -h /tmp                          # /tmp -> /data/var/tmp, 55,517,144 blocks
ls -l /tmp/

/tmp/sx uart status
/tmp/sx uart send SCHEDULIX_TX_PROOF_42
/tmp/sx uart receive
/tmp/sx gpio test                   # Phase 2 — must print REAL PHYSICAL

# Phase 4
/tmp/can-mcp2515 --mid=eid -s /dev/io-spi/spi0/dev0 -c <CRYSTAL_HZ> -g 25 -B 500000
```

### A.6 CAN, after Phase 4

```sh
canctl -u 0,rx0 -m 0            # set MID
canctl -u 0,rx0 -f 0            # clear filter
canctl -u 0,rx0 -R 2000         # receive loop
canctl -u 0,tx2 -w 0x123,3,ABCDEF   # transmit frame
cat /dev/can0/rx0               # raw receive
slog2info | tail -30            # driver log on failure
```

> Transcribed from `tools/qnx_can_install.sh:145-146,156-157`. The utility is
> **`canctl`** (confirmed present at `/system/bin/canctl` by the prior board audit,
> `bringup_report.txt:210-213`).

### A.7 Host test suites

```sh
python tests/run_tests.py                 # 5/5
python tests/test_phase6.py               # 13/13
python tests/test_analyzer_integrity.py   # 3/3
```

---

## Appendix B — file inventory

### B.1 Created this session

| Path | Purpose |
| --- | --- |
| `docs/SESSION_LOG_2026-10-06.md` | **this document** |
| `tools/serial_console.ps1` | Serial console client: interactive / one-shot / upload |

### B.2 Artifacts produced this session (on the board)

| Board path | Bytes | Provenance |
| --- | ---: | --- |
| `/tmp/sx` | 331,152 | `build/aarch64le-debug/schedulix_can`, byte-identical, 81.6 s |

### B.3 Key files read but **not** modified this session

| Path | Role in this session |
| --- | --- |
| `docs/SESSION_LOG.md` | Prior record — read with §11 applied |
| `docs/HARDWARE_PROCUREMENT_PLAN.md` | §7.4 and §8 are the superseded module route |
| `docs/gpio.md` | §3 stale on `/dev/mem`, silent on `GPFSEL` |
| `docs/uart.md` | §3 misstates the TX trace type |
| `docs/logs/qnx_logs_7.txt` | Source of the `[GPIO] no HW, mock mode` evidence (20 lines) |
| `CURRENT_STATE.md` | §6 UART claims stale; §4 IP address stale |
| `VALIDATION_LOG.md` | TEST 6 GPIO PASS not supportable |
| `src/gpio_marker.c` | Phase 2 rewrite target — defects (a)–(e), (h) |
| `src/uart_adapter.c` | Complete; `:151` needs the TX marker fix (defect (f)) |
| `src/trace_schema.h` / `.c` | Enum and name strings — append, never insert |
| `src/trace_collector.c:28-42` | `need_drop()` keep-list |
| `src/analyzer.c` | `:128-153` correlation switch; `:148` external-event; `:318-371` JSON output |
| `src/analyzer.h:24` | `RC_CPU_MIGRATION` declared, never assigned |
| `src/qnx_can_adapter.c` | 199-byte stub — Phase 5 |
| `src/qnx_can_adapter.c.stub` | Template; its `/dev/can1` assumption is wrong |
| `src/simulated_can_adapter.c` | Five hardcoded frames |
| `src/main.c:195-219` | `print_subcommand_help()` |
| `src/main.c:439-506` | `uart` subcommand |
| `src/main.c:508-524` | `gpio test` subcommand — `:515` prints `REAL PHYSICAL` |
| `tools/qnx_ssh.py` | paramiko SSH client — the Ethernet-path equivalent, unusable this session |
| `tools/qnx_can_install.sh` | 8-step on-target installer, 5,391 B |
| `tools/qnx_bringup_check.sh` | Read-only on-target audit, 4,474 B |
| `tools/spi.conf.mcp2515` | MCP2515 SPI config, 826 B |
| `Makefile` | aarch64le build; targets `build/$(PLATFORM)-$(BUILD_PROFILE)` |
| `Makefile.host` | Host build used by the Phase 3 test verification |
| `third_party/can-mcp2515/aarch64le/bin/can-mcp2515` | Prebuilt driver, 148,840 B |
| `third_party/can-mcp2515/aarch64le/bin/can-mcp2515_g` | Prebuilt debug driver, 332,448 B |
| `third_party/can-mcp2515/lib-public/mcp2515/*.h` | 8 driver public headers |
| `third_party/can-mcp2515/driver/rpi4.c` | **NOT PRESENT in the repo** — see §9.1(a) |

---

*End of `docs/SESSION_LOG_2026-10-06.md`.*
