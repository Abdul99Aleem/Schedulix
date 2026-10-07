# Schedulix — Serial Console Runbook

**Board:** QNX 8.0.0 `qnxpi` on Raspberry Pi 4B, aarch64le, 4× Cortex-A72 @ 1500 MHz
**Transport:** USB-TTL (CH340, VID `1A86` / PID `7523`) on **COM6**, 115200 8N1, no flow control
**Login:** `root`, no password — prompt is `root@console:/#`

> **Ethernet is not available.** Everything that needs the board goes over serial. This
> document is the operational path for all board work. It is self-contained: you do not
> need to read the session narrative to use it.

---

## Quick start — three commands to a root shell

```powershell
# 1. Open the interactive console (works from the repo root)
powershell -ExecutionPolicy Bypass -File tools\serial_console.ps1

# 2. The board is silent until spoken to — press Enter, expect:
#    root@console:/#

# 3. Confirm you are root
id
```

Expected `id` output: `uid=0(root) gid=0(root)`.

If any of the three fails, go straight to [§5 Troubleshooting](#5-troubleshooting).

**Typical follow-on:**

```powershell
# one-shot command, prints the reply and exits
powershell -ExecutionPolicy Bypass -File tools\serial_console.ps1 -Command "id; hostname"

# ship a binary to the board
powershell -ExecutionPolicy Bypass -File tools\serial_console.ps1 -UploadFile build\aarch64le-debug\schedulix_can -RemotePath /tmp/sx
```

---

## Contents

| # | Section |
| --- | --- |
| [1](#1-hardware-wiring) | Hardware wiring |
| [2](#2-putty) | PuTTY |
| [3](#3-serial_consoleps1--all-three-modes) | `serial_console.ps1` — all three modes |
| [4](#4-how-the-upload-works-and-why) | How the upload works, and why |
| [5](#5-troubleshooting) | Troubleshooting |
| [6](#6-split-role-operating-model) | Split-role operating model |
| [7](#7-phase-1-result--worked-example) | Phase 1 result, worked example |
| [8](#8-gotchas-and-conventions) | Gotchas and conventions |
| [A](#a-reference-data) | Reference data (board, host, tool defaults) |

---

## 1. Hardware wiring

### 1.1 Net list

Raspberry Pi 40-pin header (pin 1 = 3V3, at the top of the board).

| From | To | Divider? | Notes |
| --- | --- | --- | --- |
| Pi pin 6 (GND) | adapter GND | no | **mandatory** shared reference |
| Pi pin 8 (GPIO14, Pi **transmit**) | adapter **RXD** | **no** | CH340 RXD input is 5 V tolerant |
| adapter **TXD** | 330 Ω → TAP | **yes** | series leg |
| TAP | Pi pin 10 (GPIO15, Pi **receive**) | — | attenuated output |
| TAP | 330 Ω + 330 Ω (in series) → adapter GND | — | shunt leg |
| Pi pin 1 (3V3) | *nothing* | — | **deliberately unconnected** |
| Pi pin 2 (5V) | *nothing* | — | **deliberately unconnected** |

### 1.2 Signal flow

```
  Raspberry Pi 4 (40-pin header)                 CH340 USB-TTL adapter
  ───────────────────────────────                 ──────────────────────
  pin 6   GND      ──────────────────────────────────►  GND
  pin 8   GPIO14   ─────────── Pi TX ────────────────►  RXD
  pin 10  GPIO15   ◀────────── Pi RX ───┐
                                          │
                     ┌────────────────────┘
                     │
                     └────── TAP  ◀── TXD ── [330 Ω] ── adapter TXD
                            │
                        [330 Ω]
                            │
                        [330 Ω]
                            │
                          adapter GND
```

**TX and RX must cross over.** The Pi's transmit pin (8) goes to the adapter's
**receive** pin, and the adapter's transmit pin goes to the Pi's **receive** pin (10).
Straight-through wiring (pin 8 → RXD is right; but adapter TXD → pin 8 would be wrong)
produces exactly zero output. Non-crossover is the single most common wiring error and
is the first thing to check in [§5](#5-troubleshooting).

**GND is not optional.** It is the shared logic reference between two separately powered
devices. Without it the two signal lines float relative to each other and the levels are
meaningless — you may see nothing, or noise that looks vaguely like characters.

**VCC is deliberately left unconnected.** Pins 1 (3V3) and 2 (5V) have **no wire at all**.
Powering the dongle from the Pi adds nothing — it draws from USB — and it re-introduces
the possibility of 5 V appearing on a GPIO, which is the hazard the divider exists to
prevent.

### 1.3 The divider, and why it is fitted

**The hazard.** QNX's PL011 console runs 3.3 V logic. BCM2711 GPIOs are **not 5 V
tolerant**. A CH340 dongle is commonly claimed to drive TX at 5 V from USB VBUS even with
VCC unconnected — but **no multimeter was available**, so the actual level of *this*
dongle was never measured. Both outcomes are plausible and they need opposite fixes:

| If adapter TX is… | Direct connection | With the 2:1 divider as built |
| --- | --- | --- |
| **5 V** | destroys pin 10 | 5 V × 660/990 = **3.33 V** — correct |
| **3.3 V** | fine | 3.3 V × 660/990 = **2.20 V** — below threshold |

The PL011 input threshold is roughly 0.7 × VDD ≈ **2.31 V**. A 2.20 V signal lands under
it and characters decode unreliably or not at all.

**Why the divider is the right choice under uncertainty:**

- It **cannot damage the pin** — it only ever attenuates.
- It is **self-diagnosing** — its output *is* the measurement:

| What you see | Conclusion | Action |
| --- | --- | --- |
| clean readable text | adapter TX is 5 V, divider correct as built | **keep the divider** |
| garbage or nothing | adapter TX is 3.3 V, output too low | **remove the divider**, wire TXD straight to pin 10 |

A divider that is wrong produces a *diagnostic symptom*, not damage. That property is
what makes it safe to fit blind.

**Divider values, as built** (three of the six 330 Ω resistors):

| Quantity | Value |
| --- | --- |
| Series resistance, TXD → TAP | 330 Ω |
| TAP → GND | 330 + 330 = 660 Ω |
| Total DC load on TXD | 990 Ω |
| Ratio | 660/990 = 0.667 |
| Output for 5 V in | 3.33 V |
| Output for 3.3 V in | 2.20 V (the diagnostic case) |

**Rise time is a non-issue.** The Thevenin resistance is 330 ∥ 660 = 220 Ω; with the
CH340's low output impedance and the Pi's input capacitance of a few picofarads, the RC
corner sits far below the **8.7 µs bit period at 115200 baud**. The divider does not
degrade the bit rate.

### 1.4 Pin 8 → adapter RXD needs no divider

Pi pin 8 drives 3.3 V into the CH340's RXD input. That input is 5 V tolerant, so a direct
connection is correct. A divider in this direction would only push the signal under the
adapter's *own* input threshold for no benefit.

### 1.5 GPIO marker wires — solder before the HAT

Header pins **7, 11 and 13** (BCM 4, 17, 27 — `BRAKE_CTL`, `ADAS_FUSION`, `DIAG_POLL`)
need wires soldered to the header **underside**. These must be done **before the 40-pin
RS485 CAN HAT is mounted**, because the HAT covers them and they become unreachable
afterwards. If the HAT is already fitted, the marker pins have to come off to solder.

### 1.6 Why `/dev/ser1` is the console

Board serial configuration, verified:

| Fact | Value |
| --- | --- |
| Serial devices present | exactly one: `/dev/ser1` |
| Driver | `devc-serminiuart -e -b115200 -c500000000 -e -F -u1 0xfe215000,125` |
| Base `0xfe215000` | BCM2715 **mini-UART / AUX** |
| Is `/dev/ser1` the console? | **yes** |
| `-e` flag | the driver **echoes every byte it receives** — this dominates every design decision in [§4](#4-how-the-upload-works-and-why) |
| `-F` flag | no flow control |

---

## 2. PuTTY

PuTTY is optional. [`tools/serial_console.ps1`](#3-serial_consoleps1--all-three-modes) is
preferred for scripted work: no registry dependency, no launcher-dialog ambiguity, plus
one-shot capture and upload, which PuTTY does not have. Use PuTTY when you want to type by
hand.

PuTTY lives at `C:\Program Files\PuTTY\putty.exe`.

### 2.1 The recipe that works

Write these values under `HKCU\Software\SimonTatham\PuTTY\Default Settings`:

```
Speed             = 115200    (DWORD)
SerialFlowControl = 0
DataBits          = 8
StopBits          = 1
Parity            = 0
PortNumber        = "COM6"    (String)
```

then launch:

```
putty.exe -serial COM6
```

The registry write is required because `-serial` supplies the **port** but not the **line
settings** — PuTTY falls back to its stored defaults for speed otherwise. `Default Settings`
is where those defaults come from.

Verified live: after launching this way, a second process attempting to open COM6 gets
**access denied** — which confirms the first process really is holding the port.

### 2.2 What a correct session looks like

- A **black window is the correct idle state.** The board only speaks when spoken to.
  Press Enter and the prompt appears.
- Maximised, PuTTY shows a session bar: `Serial COM6 115200 8N1  Flow control: None`.
- The window title of a **live session is `PuTTY`**. A window titled `PuTTY Configuration`
  is the launcher dialog with no session open — that is the §5.4 failure mode.

### 2.3 Failed alternatives — do not retry these

| Attempt | Result | Conclusion |
| --- | --- | --- |
| `putty.exe -baud 115200` | "PuTTY Command Line Error" dialog | **`-baud` is not a valid switch** |
| `putty.exe -speed 115200` | process exits immediately | **`-speed` is not a valid switch either** |
| Saved session under `HKCU\...\PuTTY\Sessions\SchedulixConsole`, loaded with `-load` | fell back to the Configuration dialog; session did **not** open | registry-session route unusable here; the entry was removed again |
| Clicking **Open** in the launcher dialog | nothing useful appeared | PuTTY was sitting on the launcher dialog with no session (PID 7780, `MainWindowTitle` = `PuTTY Configuration`) |

### 2.4 Port diagnostics

```powershell
Get-PnpDevice | Where-Object { $_.FriendlyName -match 'CH34|USB-Serial' } |
  Select-Object Status, FriendlyName, InstanceId | Format-List
```

Expected: `USB-SERIAL CH340 (COM6)`, `InstanceId` `USB\VID_1A86&PID_7523\...`,
`Status` **OK**. This is the authoritative check.

| Probe | Result | Interpretation |
| --- | --- | --- |
| `[System.IO.Ports.SerialPort]::GetPortNames()` | COM3, COM4, COM5, COM6 | COM6 present |
| `Get-PnpDevice` | CH340 on COM6, Status OK | driver installed, COM6 is the right port |
| `Get-CimInstance Win32_SerialPort` | **COM6 absent** | WMI enumeration quirk — **not a fault**. `Get-PnpDevice` supersedes it. |

**Access denied** from a second opener means the port is being held correctly by the first
process. That is a confirmation, not an error.

---

## 3. `serial_console.ps1` — all three modes

Windows / PowerShell 5.1+. This is the serial-console counterpart to `tools/qnx_ssh.py`:
used when Ethernet is unavailable, or when root is required.

The port is always opened as **115200 8N1, no flow control** (`SerialPort $Port, $Baud,
'None', 8, 'One'`), with a 500 ms read timeout.

### 3.1 Parameters

| Parameter | Type | Default | Meaning |
| --- | --- | --- | --- |
| `-Port` | string | `COM6` | COM port of the CH340 |
| `-Baud` | int | `115200` | Line speed. Must match the board |
| `-Command` | string | *(unset)* | **One-shot mode.** Send this command, capture the reply, exit |
| `-CaptureSeconds` | int | `8` | **One-shot mode.** Hard cap on the capture window; early return on prompt detection can only make it shorter |
| `-UploadFile` | string | *(unset)* | **Upload mode.** Local file to ship to the board |
| `-RemotePath` | string | `/tmp/<leaf of -UploadFile>` | **Upload mode.** Destination path. Ignored in the other two modes |
| `-ChunkSize` | int | `64` | **Upload mode.** base64 characters per written line |
| `-ChunkDelayMs` | int | `12` | **Upload mode.** Inter-chunk delay. Raise this if transfers are unreliable |

Mode selection order in the script: `-UploadFile` is tested first, then `-Command`, then
interactive. If you supply both `-UploadFile` and `-Command`, upload wins and `-Command`
is silently ignored. If you supply `-RemotePath` without `-UploadFile` it is ignored.

`-UploadFile` is passed through `Resolve-Path`, so it is resolved against the **current
PowerShell working directory** — run from the repo root or use an absolute path.

`-CaptureSeconds` applies only to one-shot mode. The upload's decode phase uses its own
fixed 90-second deadline and does not read this parameter.

### 3.2 Interactive mode (default)

```powershell
powershell -ExecutionPolicy Bypass -File tools\serial_console.ps1
```

Clears the host screen, prints a banner (port, format, `exit: Ctrl+C`), then mirrors
serial output and forwards keystrokes. **The board stays silent until you type — press
Enter.**

Key forwarding is explicit rather than raw, because the board's line discipline is not
raw:

| Key | Bytes written |
| --- | --- |
| Enter | `\r\n` |
| Backspace | `0x7F` |
| Escape | `0x1B` |
| everything else | `$key.KeyChar` |

The keystroke pump is wrapped in `try/catch`: `[Console]::KeyAvailable` throws when stdin
is redirected, and the catch keeps the output mirror running, so the script still works
as a dumb logger when piped or run from a scheduler.

Exit with `Ctrl+C`.

> **Interactive mode holds COM6 open for its entire lifetime.** Any one-shot
> `serial_console.ps1` or `-UploadFile` run attempted while it is running will fail with
> **access denied**. That is expected, not a fault — close the interactive session first.

### 3.3 One-shot mode

```powershell
powershell -ExecutionPolicy Bypass -File tools\serial_console.ps1 -Command "id; hostname"
powershell -ExecutionPolicy Bypass -File tools\serial_console.ps1 -Command "pidin info" -CaptureSeconds 20
```

Sends the command followed by CRLF, captures the reply to stdout, closes the port, exits 0.

**Prompt detection.** One-shot mode returns as soon as a shell prompt appears at the end
of the stream:

```powershell
if ($buffer.ToString() -match '(?m)[\r\n][^\r\n]*[#$]\s*$') { break }
```

`-CaptureSeconds` remains only as the cap for commands that produce no prompt. Measured
effect of this refinement: **8 s → 0.57 s** for a typical command.

> **One-shot mode is not suitable for anything that needs a TTY.** `vi`, `top`, a bare
> `sh`, `tracelogger`, anything reading raw keystrokes or streaming indefinitely — those
> read the tty directly and one-shot mode closes the port the moment the prompt-detection
> regex matches. Use **interactive mode** for those.

### 3.4 Upload mode

```powershell
powershell -ExecutionPolicy Bypass -File tools\serial_console.ps1 -UploadFile build\aarch64le-debug\schedulix_can -RemotePath /tmp/sx
```

Upload mode automatically `chmod +x`es the destination. Without `-RemotePath` it lands at
`/tmp/<original filename>`. Full mechanism and rationale in [§4](#4-how-the-upload-works-and-why).

### 3.5 Failure to open

```
ERROR: could not open COM6 - <message>
Another terminal (PuTTY?) may still be holding the port.
```

Exit code 1. Cause is almost always another process holding COM6 — PuTTY, or an
interactive `serial_console.ps1` still running.

---

## 4. How the upload works, and why

### 4.1 The constraints that forced the design

1. **There is no serial file-transfer path in the PuTTY suite.** PSCP and plink are
   SSH-only. PuTTY's raw serial mode is a terminal, not a transport.
2. **Ethernet is not connected**, so `tools/qnx_ssh.py` is unavailable.
3. **The artefacts are large** — 148–331 KB. They cannot be typed by hand.
4. **A QNX SDP 8.0 toolchain *is* present** at `C:\Users\User\qnx800`
   (`host\win64\x86_64\usr\bin\qcc.exe`, gcc 12.2.0, plus `make` and `ntoaarch64-strip`).
   `C:\QNX` holds **only the IDE** — QNX Software Center and qnxmomenticside — and is a
   common source of the wrong conclusion that no toolchain exists. Binaries can therefore be
   rebuilt and stripped locally; see §4.4.1.
5. **The Momentics IDE is a working alternative deployment channel** (§4.4.2) and has proved
   more reliable than serial for larger binaries, because it does not depend on the console
   line discipline described in §4.4.
6. **Hardware flow control is not wired** (RTS/CTS absent). A single large `Write()`
   overruns the Pi's UART receive FIFO and bytes are dropped **silently** — no error, no
   short-write indication. Chunking with an inter-chunk delay is mandatory, not cosmetic.

### 4.2 Pipeline

```
  local binary
    └─ gzip                     (board has gunzip; roughly thirds the wire time)
        └─ base64               (board has base64; output has no shell metacharacters)
            └─ written as a stream of fixed-length lines into a
               FOREGROUND sink:   head -c N > /tmp/sx.b64
            └─ on target:  tr -d '\r\n' < /tmp/sx.b64 | base64 -d | gunzip > /tmp/sx
            └─ verify:     wc -c compared against the local byte count
            └─ chmod +x /tmp/sx
```

### 4.3 The sink: a foreground `head -c N > file`

```sh
rm -f /tmp/sx.b64; head -c <N> > /tmp/sx.b64
```

The prompt returns on its own, with no `Ctrl+D` and no `SIGINT` — both of which are needed
and neither of which works on this console (see §4.4).

> **`head` is quirky — read this before changing `N`.** Measured on this console:
> `head -c 200` did **not** block waiting for 200 bytes. It returned after the first line
> and wrote only **1 byte**; sending data afterwards found the shell already back at a
> prompt. So `head` consumes input **line by line** and stops once its byte budget is
> spent, rather than blocking for the full count.
>
> Consequence: **N must be comfortably larger than the real stream**, or the tail of the
> payload is truncated. A 332,624-byte payload failed this way (`staging 1 B`, three
> attempts) while smaller payloads succeeded. The tool therefore uses roughly a **3×
> margin** plus a fixed page, and over-sends **4000** padding newlines to guarantee `head`
> runs out of input cleanly.

N is computed as:

| Term | Value |
| --- | --- |
| `lines` | `ceil(base64Length / -ChunkSize)` |
| `dataBytesMax` | `base64Length + lines × 2` — the larger CRLF case |
| `headBytes` | `ceil(dataBytesMax × 3) + 8192` — deliberately over-asked |

The surplus is whitespace, discarded by `tr -d '\r\n'` before decoding; anything left over
lands as blank lines at the shell prompt.

The line discipline does **not** fold CRLF into LF — each line costs 2 bytes. This is not
assumed: staging sizes come back at exactly `base64Length + (lines × 2) + 400`.

### 4.4 Four things that do not work — and their symptoms

These are the traps. Each cost real time; three of them are recoverable only by
power-cycling the board.

| # | Approach | Symptom | Why |
| --- | --- | --- | --- |
| 1 | `cat > file` + `Ctrl+D` | Shell **never returns**; console wedges | This console delivers neither `Ctrl+D` as EOF to the reader nor `Ctrl+C` as SIGINT. **The board must be power-cycled.** |
| 2 | **Backgrounded** sink: `head -c N > file &` | Board prints `[1] + Stopped (tty input)  head -c ...` | A background job reading the tty takes **SIGTTIN** and is stopped. **The sink must run in the foreground.** |
| 3 | Per-chunk shell appends: `echo '<chunk>' >> file` | Transfer silently truncates — **154,384 bytes became 6,889** | One dropped byte leaves an unterminated quote, `ksh` enters continuation mode, and **every remaining chunk is silently swallowed**. |
| 4 | `dd bs=64 count=N of=file` | Prompt **never returns**; console wedges again | Tested as a "more deterministic" replacement for `head`. It did not behave as expected and required a further power cycle. **Do not reintroduce it.** |

### 4.4.1 Practical consequence: strip before uploading

Because large payloads are where the sink's behaviour becomes unreliable, **strip the
binary first**. The QNX SDP 8.0 toolchain on the host includes `ntoaarch64-strip`:

```powershell
$env:QNX_HOST   = 'C:\Users\User\qnx800\host\win64\x86_64'
$env:QNX_TARGET = 'C:\Users\User\qnx800\target\qnx'
& "$env:QNX_HOST\usr\bin\ntoaarch64-strip.exe" build\aarch64le-debug\schedulix_can
```

The unstripped debug build is ~332 KB. A stripped binary is far smaller, which shortens
the transfer dramatically and keeps it inside the payload range already proven to work
(826 B, 30 KB, 154 KB and 331 KB all verified byte-for-byte at various points in the
session).

Also available on the host, and worth using for the same reason:

| Tool | Path (under `C:\Users\User\qnx800\host\win64\x86_64\usr\bin`) |
| --- | --- |
| `qcc` (gcc 12.2.0) | `qcc.exe` |
| `ntoaarch64-strip` | `ntoaarch64-strip.exe` |
| `make` | `make.exe` |

Build and strip in one go:

```powershell
$env:QNX_HOST   = 'C:\Users\User\qnx800\host\win64\x86_64'
$env:QNX_TARGET = 'C:\Users\User\qnx800\target\qnx'
$env:PATH       = "$env:QNX_HOST\usr\bin;$env:QNX_TARGET\usr\bin;$env:PATH"
& "$env:QNX_HOST\usr\bin\make.exe" rebuild
& "$env:QNX_HOST\usr\bin\ntoaarch64-strip.exe" build\aarch64le-debug\schedulix_can
powershell -ExecutionPolicy Bypass -File tools\serial_console.ps1 `
  -UploadFile build\aarch64le-debug\schedulix_can -RemotePath /tmp/sx
```

Trap 3 is the reason the current design writes raw base64 into a `head` sink rather than
through `echo`. It also explains why chunking and drain-on-every-iteration (§4.5) are not
optional tuning.

### 4.5 The echo drain

The console driver runs with `-e`, so **every byte the host sends comes back**. Left to
accumulate, that echo overruns the board's receive buffer, the console goes silent, and
recovery again requires a **power cycle**. The client therefore drains incoming bytes on
**every single chunk iteration**, not once at the end.

### 4.6 Verification and reporting

- Progress printed every 60 chunks: percentage, chunk counts, effective KB/s.
- **Three attempts**, 3-second pause between.
- Staging size checked before decode and reported as `staging N B (+D padding)` or
  `staging N B, short by M` followed by `stream looks truncated`.
- Decode, unpack, `chmod +x`, then `FINAL_` byte count compared against the source.
- Success prints `OK  N bytes on target, matches source  (XX.Xs)` and exits 0.
- Failure prints `TRANSFER INCOMPLETE after 3 attempts.` and exits 1.

**The final byte count is the authoritative integrity check** and is the only one that
matters across a flow-control-less link. Size verification is not advisory — a transfer
that reports `OK` is byte-verified.

### 4.7 Measured performance

Sustained rate ≈ **2.5 KB/s**.

| Payload | Result | Time |
| --- | --- | --- |
| 826 B (`tools/spi.conf.mcp2515`) | ok | — |
| 30,720 B | ok | 17.2 s |
| 154,384 B | ok | 44.3 s |
| 331,152 B (`build/aarch64le-debug/schedulix_can`) | ok | 81.6 s |

Budget roughly **1 minute per 160 KB** when planning a session.

> **Above roughly 330 KB, prefer the IDE.** A 332 KB transfer failed with `staging 1 B`
> (see §4.4.4 — `head -c N` truncates when its budget runs out early). Strip the binary, or
> use the Momentics IDE channel in §4.4.2.

### 4.4.2 Alternative channel: the Momentics IDE

Since 2026-10-07 the **Momentics IDE is a proven deployment and log channel**, and for
anything larger than ~330 KB it is the recommended route.

What it does: compiles the project, deploys the binary to the board over the network, runs
it, and streams the process's `stdout` live into the IDE console pane. Verified by running
`schedulix gpio test`, whose output appeared in the IDE console and drove a real LED on
header pin 11.

**It does not use SSH.** Deployment goes through `qconn`, so none of the SSH algorithm
problems in §4.8.2 apply to it. That is precisely why it is more robust than the serial
path for large binaries.

Practical split:

| Task | Channel |
| --- | --- |
| Build, deploy, run, watch logs | **Momentics IDE** — set program arguments, press Run |
| Interactive root shell, `passwd`, `slay`, privileged commands | **Serial console** (§§2–3) or `ssh` (§4.8.2) |

The IDE runs the binary as a normal user. That was sufficient for the GPIO test — the
`mmap(MAP_PHYS)` call succeeded and reported `REAL PHYSICAL` — but it should not be assumed
for every privileged operation.

### 4.4.3 Host networking gotcha

The workstation had **two active Ethernet adapters** (`192.168.56.1` on `Ethernet 2`, and
`192.168.10.1` on `Ethernet` alongside the board at `192.168.10.5`). This made SSH behave
erratically: one attempt failed with `No route to host` while a later one reached the
password prompt on the same address. Traffic was being arbitrated between two NICs.

If both a failed and a successful attempt to the same address are observed, check for a
second active adapter:

```powershell
Get-NetIPAddress -AddressFamily IPv4 | Where-Object { $_.IPAddress -notlike '127.*' }
Disable-NetAdapter -Name "Ethernet 2" -Confirm:$false
```

### 4.4.4 Why `head -c N` is fragile above ~330 KB

Measured on this console: `head -c 200` does **not** block waiting for 200 bytes. It
returns after the first line, having written **1 byte**; sending data afterwards found the
shell already back at a prompt. `head` therefore consumes input **line by line** and stops
once its byte budget is spent, rather than blocking for the full count.

Consequence: if the budget is close to the payload length, the tail is truncated. A
332,624-byte transfer failed three times with `staging 1 B`, while 331,152 bytes had
succeeded. The tool now uses roughly a 3× margin plus 8 KB, but **strip the binary or use
the IDE rather than relying on this**.

### 4.4.5 Recorded console-wedging incidents

Three power cycles were needed during this work. All three were caused by an approach that
parks the shell in a foreground reader:

| Incident | Cause |
| --- | --- |
| 1 | `cat > file` + `Ctrl+D` — `Ctrl+D` is not delivered as EOF on this console |
| 2 | Echo never drained while streaming — ~81 KB of echo overran the receive buffer |
| 3 | `dd bs=N count=M` — tested as a "more deterministic" reader; never returned the prompt |

Recovery is always the same: power-cycle the board, then reconnect. The USB-TTL should be
plugged in **before** powering the board, otherwise the console can end up bound to nothing.

### 4.4.6 SSH to the board

Per the official [QNX QSTI for Raspberry Pi guide, "Interacting with the system"](https://www.qnx.com/developers/docs/qnxeverywhere/com.qnx.doc.target_images/topic/qsti/interacting-with-the-system.html):

- Default credentials are **`qnxuser` / `qnxuser`** and **`root` / `root`**.
- **Root login over SSH is disabled by default.** To enable it, on the board:
  ```sh
  su
  sed -i 's/^#*PermitRootLogin.*/PermitRootLogin yes/' /usr/etc/ssh/sshd_config
  slay -f sshd
  /usr/bin/sshd -f /usr/etc/ssh/sshd_config
  ```
- **Windows clients must request `hmac-sha2-256`**, because QNX sshd does not offer the
  default MAC algorithms. One-off:
  ```powershell
  ssh -m hmac-sha2-256 root@192.168.10.5
  scp -o "MACs=hmac-sha2-256" build/aarch64le-debug/schedulix_can qnxpi:~/bin
  ```
  Persistent, via `C:\Users\User\.ssh\config`. **One directive per line** —
  `Host qnxpi 192.168.10.5` on a single line is parsed as two host patterns,
  so SSH silently falls back to the local Windows username instead:
  ```
  Host qnxpi
     HostName 192.168.10.5
     User root
     MACs hmac-sha2-256
  ```
  After that, `ssh qnxpi` and `scp <file> qnxpi:/tmp/` need no flags.

That same page documents the serial wiring independently: **TX is pin 8, RX is pin 10**,
ground pin 6 or 14, and the power pins must never be connected — corroborating the corrected
crossover in §2.

---

## 5. Troubleshooting

### 5.1 Nothing at all on the serial

```
Press Enter in PuTTY / interactive console → nothing comes back.
```

Work down this list **in order**. The first item is the most common error by a wide
margin.

| # | Check | How | If it is wrong |
| --- | --- | --- | --- |
| 1 | **TX/RX crossover** | Trace both signal wires by hand. Pin 8 (Pi TX) must reach adapter **RXD**. Adapter **TXD** must reach pin 10 (Pi RX). | Re-wire. Pin 8 → RXD **and** TXD → pin 10. Straight-through or reversed gives total silence. |
| 2 | **GND** | Pin 6 to adapter GND, continuity-checked | Re-connect. Without a common reference the levels are meaningless. |
| 3 | **Port holder** | `Get-PnpDevice \| Where-Object FriendlyName -match 'CH34'`; then try opening COM6 | Close PuTTY, or `Ctrl+C` out of an interactive `serial_console.ps1`, and retry |
| 4 | Line settings | 115200 8N1, no flow control, COM6 | Correct PuTTY registry values (§2.1) or `-Baud 115200` |
| 5 | Board powered | Power LED, and try the power-cycle procedure (§5.7) | Power-cycle |
| 6 | Divider fitted | See §5.2 | — |

### 5.2 Garbled characters

| Cause | Action |
| --- | --- |
| Baud mismatch — board is 115200, client is something else | Set 115200 on both sides |
| **3.3 V adapter TX with the divider still fitted** — output is 2.20 V, under the PL011 input threshold of ≈2.31 V | **Remove the divider**, wire adapter TXD straight to pin 10 |

This is the divider's designed diagnostic: clean text ⇒ TX is 5 V, keep the divider;
garbage ⇒ TX is 3.3 V, remove it. A wrong divider produces a symptom, never damage.

### 5.3 Console goes completely silent, passive reads return 0 bytes

**Power-cycle the board.** Do not attempt further commands; there is no way out.

Two causes, both avoidable:

| Cause | How it happens | How to avoid |
| --- | --- | --- |
| **Undrained echo overrun** | The `-e` echo accumulated because the client stopped reading, overran the board's receive buffer | Never leave the client idle mid-transfer; the tool drains on every chunk by design. Do not run a second reader alongside it. |
| **Wedged foreground reader** | A `cat > file` that never saw `Ctrl+D` (§4.4 trap 1) is still holding the tty | Never use `cat > file` on this console. Use `-UploadFile`. |

### 5.4 PuTTY never opens a window

| Step | Action |
| --- | --- |
| 1 | Launch from a shell: `"C:\Program Files\PuTTY\putty.exe" -serial COM6` and read any error text |
| 2 | Check `Alt+Tab` and the taskbar — the window may be behind something |
| 3 | Check the window title: `PuTTY Configuration` = launcher dialog, no session. `PuTTY` = live session |
| 4 | `Win+Shift+Arrow` moves a window that is off-screen |
| 5 | Confirm the port is not held by another process (an `Access denied` from a second opener is a *good* sign) |

### 5.5 Upload reports `TRANSFER INCOMPLETE` or a size mismatch

| Step | Action |
| --- | --- |
| 1 | **Raise `-ChunkDelayMs`** to slow the stream — e.g. `-ChunkDelayMs 30` |
| 2 | Retry. Three attempts are automatic; a manual retry is also fine |
| 3 | If it still fails, reduce `-ChunkSize` (e.g. `-ChunkSize 32`) |
| 4 | Confirm nothing else is reading COM6 concurrently |
| 5 | Check free space in `/tmp` (≈53M blocks on `/dev/hd0t179`); a full `/tmp` truncates the staging file |
| 6 | If the board went silent during the attempt → power-cycle (§5.7), then re-upload |

### 5.6 Board silent after `boot`, or driver wedged

Power-cycle. There is no soft reset available over serial on this image.

### 5.7 Power-cycle recovery procedure

```text
1. Unplug the Pi's USB power for ~5 seconds.
2. Replug it.
3. Wait for the console — boot messages, then the prompt.
4. From the host, confirm the board is back:
```

```powershell
powershell -ExecutionPolicy Bypass -File tools\serial_console.ps1 -Command "id"
```

Expect `uid=0(root) gid=0(root)`. If nothing comes back after the boot window, repeat
step 1 — a 5-second discharge is short for a board with bulk capacitance.

---

## 6. Split-role operating model

This division exists so that nothing requires root over the network — and Ethernet is not
available anyway.

| Role | Tool | Privilege | Availability |
| --- | --- | --- | --- |
| **Bulk file transfer** | `tools/serial_console.ps1 -UploadFile` | **root** (via the console shell) | **Available now** — the only transport in use |
| **Privileged commands** | `tools/serial_console.ps1` interactive, or `-Command` | **root** | **Available now** |
| **SSH as `qnxuser`** | `tools/qnx_ssh.py` | `qnxuser` | **Only if Ethernet is ever restored** |

**About `tools/qnx_ssh.py`.** It is the Ethernet-path equivalent of the serial client and
is a genuine SSH client, but it uses **paramiko** rather than Windows OpenSSH because QNX's
sshd advertises legacy key-exchange and MAC algorithms that OpenSSH 9.x refuses by
default with no clean way to re-enable. paramiko 5.0.0 is installed on this host for that
reason. Credentials come from `SCHEDULIX_PI_PASSWORD` or `--password`; if empty it tries a
short list of common QNX defaults against `qnxuser` then `root`.

Do not plan around SSH. Plan around serial.

---

## 7. Phase 1 result — worked example

The UART adapter was proven on real hardware through this transport. This is the reference
procedure; repeat it after any upload to confirm the binary is the one you think it is.

### 7.1 Upload

```powershell
powershell -ExecutionPolicy Bypass -File tools\serial_console.ps1 `
  -UploadFile build\aarch64le-debug\schedulix_can -RemotePath /tmp/sx
```

Result: `OK  331152 bytes on target, matches source  (81.6s)`

### 7.2 Status — the pass criterion

```console
$ /tmp/sx uart status
UART Adapter Backend: physical
UART Status: physical device /dev/ser1 configured at 115200 baud
```

**`physical` is the pass criterion.** The string to reject comes from the simulated
fallback:

```
simulated loopback mode (dev /dev/ser1 offline)
```

**The distinction matters more than it looks.** The simulated path pushes into an
in-process 256-byte ring buffer and reads it straight back, so `uart send` followed by
`uart receive` succeeds *perfectly* while proving nothing at all. A green round-trip
against the loopback queue is indistinguishable from a green round-trip over copper unless
you read the status string.

### 7.3 Transmit

```console
$ /tmp/sx uart send SCHEDULIX_TX_PROOF_42
SCHEDULIX_TX_PROOF_42Sent 21 bytes over UART: 'SCHEDULIX_TX_PROOF_42'
```

Note the ordering. The transmitted string appears **before** the program's own `printf`,
and they are on the same physical wire:

```
  adapter TXD → divider → Pi pin 10  ──►  PL011 RX → uart_adapter_send() writes first
  adapter RXD ← divider ← Pi pin 8   ◄──  PL011 TX → printf() emitted locally afterwards
```

So `SCHEDULIX_TX_PROOF_42` appearing *ahead of* the program's message is the **signature
of real TX on the wire**. The `printf` also travels the console, but it is emitted locally
after the send returns — which is why the order is inverted relative to normal logging.
If the string instead appeared *after* the message, it was coming from the program, not
the wire.

### 7.4 Device contention — did not materialise

The anticipated blocker was that `/dev/ser1` **is** the console device, so opening it a
second time would either fail or interleave shell echo into received data.

**QNX permits a second open of `/dev/ser1` while the console driver owns it.** The open
succeeded, the termios configuration applied, and `uart status` reported `physical`. The
anticipated conflict did not materialise.

### 7.5 Receive — the known confound, and the remedy

`/dev/ser1` **is** the console. Anything the host sends is consumed by the login shell, not
by the application. `uart receive` therefore returned the **console's own newline** — the
shell's response to the terminating Enter, read straight back out of the UART.

This is a measurement artefact of the wiring, not a defect in the adapter.

**Remedy.** Jumper the **USB-TTL adapter's own TX and RX pins** together. Transmitted bytes
then loop back into the Pi's RX, and the host's *direct* transmission to the console no
longer reaches the Pi's receive pin as a competing stream. Because `ksh` is busy executing
a compound command, it is not reading stdin at that moment, so `uart receive` gets an
uncontested read of the looped-back bytes.

> **Do NOT jumper Pi pin 8 → Pi pin 10.** That places the Pi's TX **output** in direct
> contention with the divider's output driving the same pin. Two outputs on one net
> produce a corrupted, undefined level. Loop back on the **adapter** side only.

---

## 8. Gotchas and conventions

| # | Gotcha | What to do |
| --- | --- | --- |
| 1 | **A blank screen is normal.** The board only speaks when spoken to | Press Enter. Expect `root@console:/#` |
| 2 | The prompt is `root@console:/#`, but the **working directory is not fixed** — it may be `/etc/settings` or anywhere | `cd /tmp` explicitly before assuming paths |
| 3 | The prompt can change format as the shell settles, e.g. trailing `root@console:#` | Cosmetic. Also `ksh: ???@??: cannot execute` at boot is a harmless hostname-resolution failure |
| 4 | **`???` in program output** is a UTF-8 character the 3.3 V console cannot render | Not corruption. Strip or ignore it |
| 5 | **The binary to deploy is `build/aarch64le-debug/schedulix_can`.** Every binary in `deploy/` is **stale** | A byte scan for `UART Adapter Backend`, `uart status`, `uart send`, `uart receive` and `gpio test` finds **none** of them in any `deploy/` binary, while the `build/` binary has **all** of them. Verified: `deploy/schedulix_can` (179,264 B), `deploy/schedulix_can-aarch64le-debug` (154,384 B), `deploy/schedulix_can-x86_64` (175,360 B) → zero hits; `build/aarch64le-debug/schedulix_can` (331,152 B) → all hits |
| 6 | **Echoes in captured output are the driver's `-e` echo of your own command**, not the board talking | Filter them mentally when reading logs; one-shot capture always contains them |
| 7 | **Always verify the board is still responsive after an upload** before starting the next operation | `powershell -ExecutionPolicy Bypass -File tools\serial_console.ps1 -Command "id"` |
| 8 | Interactive mode holds COM6; a concurrent one-shot run gets **access denied** | Expected. Close the interactive session first |
| 9 | `pkill`, `gcc` and `cc` are **not available** on the board | Use the shell builtin `kill`; there is no compiler on the board, so binaries must come from the host |
| 10 | The root filesystem is read-only and `/` is 100% full | Write to `/tmp` (→ `/data/var/tmp` on `/dev/hd0t179`, ≈53M blocks free) or `/data` |
| 11 | No `chmod` problem — upload mode already sets the executable bit | Just run it |
| 12 | Interactive mode redirects the host screen (`Clear-Host`) | Your scrollback is lost; capture to a log if the output matters |

---

## A. Reference data

### A.1 Board

| Item | Value |
| --- | --- |
| Version | `QNX qnxpi 8.0.0 2025/07/30-19:17:34EDT RaspberryPi4B aarch64le` |
| CPUs | 4 × Cortex-A72 @ 1500 MHz |
| Memory | 7530 MB free / 8128 MB |
| Serial device | `/dev/ser1` (the console) |
| Driver | `devc-serminiuart -e -b115200 -c500000000 -e -F -u1 0xfe215000,125` |
| Login | `root`, no password |
| Prompt | `root@console:/#` |
| `/tmp` | → `/data/var/tmp` on `/dev/hd0t179`, ≈53M blocks free |

**Available on the board:** `base64`, `cksum`, `tr`, `gzip`, `gunzip`, `zcat`, `split`,
`head` (with working `-c`), shell builtin `kill`.

**Not available:** `pkill`, `gcc`, `cc`.

### A.2 Host

| Item | Value |
| --- | --- |
| Adapter | CH340, VID `1A86` / PID `7523`, on **COM6** |
| InstanceId | `USB\VID_1A86&PID_7523\5&5114092&0&3` |
| Shell | Windows, PowerShell 5.1 |
| PuTTY | `C:\Program Files\PuTTY\putty.exe` |
| QNX SDP toolchain | **`C:\Users\User\qnx800`** — gcc 12.2.0 `qcc`, `make`, `ntoaarch64-strip`. `C:\QNX` is only the IDE |
| paramiko | 5.0.0 (SSH only, only if Ethernet returns) |
| Multimeter | **none** — hence the self-diagnosing divider |

### A.3 `serial_console.ps1` defaults

```
-Port           COM6
-Baud           115200
-CaptureSeconds 8
-ChunkSize      64
-ChunkDelayMs   12
RemotePath      /tmp/<leaf of -UploadFile>
Line settings   8 data bits, 1 stop bit, no parity, no flow control
Modes           -UploadFile, then -Command, then interactive
```

### A.4 File index

| Path | Role |
| --- | --- |
| `tools/serial_console.ps1` | Serial console client: interactive / one-shot / upload |
| `tools/qnx_ssh.py` | paramiko SSH client — the Ethernet-path equivalent |
| `tools/spi.conf.mcp2515` | 826 B MCP2515 SPI config — the upload smoke test |
| `tools/qnx_can_install.sh` | 8-step on-target CAN installer |
| `build/aarch64le-debug/schedulix_can` | **the** binary to deploy (331,152 B) |
| `docs/uart.md` | UART adapter design |
| `docs/gpio.md` | GPIO marker design |
| `third_party/can-mcp2515/aarch64le/bin/can-mcp2515` | Prebuilt CAN driver (148,840 B) |