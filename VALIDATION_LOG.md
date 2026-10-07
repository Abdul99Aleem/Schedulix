# Schedulix — Validation Log

This document records the compilation, host-side unit testing, and QNX target-side validation of the Schedulix Backend program.

## 1. Compilation Verification
* **Target Build**: `aarch64le` (Raspberry Pi 4/5)
* **Compiler**: QNX SDP 8.0 `qcc` (GCC 12.2.0)
* **Toolchain location**: `C:\Users\User\qnx800` (SDP 8.0, `host\win64\x86_64\usr\bin\qcc.exe`).
  `C:\QNX` holds **only the IDE** — `QNX Software Center` and `qnxmomenticside`. It is not
  the SDP and contains no `qcc`.
* **Build Profiles**: `debug`
* **Status**: **PASS** (Zero errors, built binary output to `build/aarch64le-debug/schedulix_can`).
* **Full rebuild**: zero errors **and zero warnings**, re-verified 2026-10-07 after the
  GPIO marker rewrite. `ntoaarch64-strip` is available in the same `usr\bin` and was used to
  shrink the binary for board upload.

## 2. Host-Side Verification
* **Python Verification Test (`tests/run_tests.py`)**: Checks CAN parsing and event mappings.
  * **Result**: **PASS** (5/5 tests passed).
* **Stress Generator Validation (`tests/test_phase6.py`)**: Validates stress scenarios S0-S6 definitions, configurations, and baseline parameters.
  * **Result**: **PASS** (13/13 tests passed).
* **Analyzer Integrity Validation (`tests/test_analyzer_integrity.py`)**: Tests binary trace file header verification, bad magic checks, and regression changes.
  * **Result**: **PASS** (3/3 tests passed).
* All three suites were **re-run on 2026-10-07 after the GPIO marker rewrite**, including the
  `trace_event_type_t` enum change (`TRACE_GPIO_MARKER_HIGH = 14`, `TRACE_GPIO_MARKER_LOW = 15`,
  appended so values 0–13 keep their existing meaning). Still 5/5, 13/13, 3/3.

## 3. QNX Pi Target-Side Verification (Log Analysis — NOT session-verified)

> **Provenance note.** The results in this section are derived from **reading an older
> target execution log** (`docs/logs/qnx_logs_7.txt`). They were *not* observed live during
> the 2026-10-06 session and have not been reproduced against the board currently in hand.
> They describe what the log claims, not what has been independently confirmed. See
> §4 for the live results.
>
> **One exception:** TEST 6 (GPIO) has since been **independently reproduced on the board** on
> 2026-10-07 against the rewritten `src/gpio_marker.c`, and is no longer resting on the log.
> See [§4.4](#44-gpio-marker--pass-verified-2026-10-07-real-hardware--led).

Based on verification runs executed on `root@qnxpi` (from QNX target execution logs `docs/logs/qnx_logs_7.txt`):
* **TEST 1: Workloads (`--workloads`)**: ECU workloads initialize nominal execution. `BRAKE_CTL` (prio 20, period 10ms), `ADAS_FUSION` (prio 15, period 20ms), `DIAG_POLL` (prio 10, period 50ms) are spawned. -> **PASS**.
* **TEST 2: Full Core (`--full`)**: Feeds CAN frame events, triggers workload releases, captures monotonic timing parameters, and flushes traces. -> **PASS**.
* **TEST 3: Kernel Tracer (`tracelogger`)**: Captures real context-switch events under root privileges into `/tmp/schedulix.kev` (~114KB). -> **PASS**.
* **TEST 4: Load Sweep (`--sweep`)**: Saturation sweep from 20% to 95% CPU load runs cleanly, generating bin, manifest, and analysis reports for each level. -> **PASS**.
* **TEST 5: Mutex Contention (`--stress 2`)**: Blocks `BRAKE_CTL` lock under priority inversion, proving `MUTEX_BLOCK` root-cause attribution. -> **PASS**.
* **TEST 6: GPIO Validation**: -> **PASS — verified on real hardware 2026-10-07. See §4.4.**
  * **Superseded correction, retained for history.** This entry previously read **MOCK / INCONCLUSIVE**, on the grounds that the log shows `[GPIO] no HW, mock mode (validation uses sw timestamps)` — in mock mode the test compares software timestamps against software timestamps, which is vacuous and demonstrates nothing about electrical output.
  * **That correction was accurate for the code as it stood at the time, and is not a criticism of the original PASS.** The root cause was a real set of defects in `src/gpio_marker.c`: it opened `/dev/mem` (absent on this QNX image) instead of using `mmap(MAP_PHYS|MAP_SHARED, NOFD, PROT_NOCACHE, 0xFE200000)`, it **never wrote `GPFSELn`**, so `GPSET0`/`GPCLR0` could produce no electrical output, and `gpio_marker_get()` was a stub returning `-1` unconditionally. A fix was required before a physical proof was even possible.
  * `src/gpio_marker.c` has since been rewritten (Phase 2). The GPIO result recorded in §4.4 is the first one in which the registers were genuinely mapped, the pins were genuinely switched to output, and the levels were read back from `GPLEV`. **Read §4.4, not this §3 entry, for the current GPIO verdict.** The `qnx_logs_7.txt` run above still describes the pre-rewrite binary and remains accurate for it.
  * For reference, `schedulix gpio test` prints `REAL PHYSICAL` when the registers are genuinely mapped and `MOCK` on the fallback path (`src/main.c:515`). There is no bare `REAL` string — `schedulix status` uses a different phrasing, `REAL (BCM2711 registers mapped)` / `MOCK (software simulation)`, at `src/main.c:249`.

## 4. Live Hardware Verification (session-verified)

Everything in this section was observed directly on the board, not inferred from a log.

* §4.1–§4.3, §4.5 — 2026-10-06 (serial console session).
* §4.4, §4.6, §4.7 — 2026-10-07 (GPIO marker rewrite verification via the Momentics IDE,
  host test re-runs, and the SSH bring-up).

### 4.1 Board identity
* `QNX qnxpi 8.0.0 2025/07/30-19:17:34EDT RaspberryPi4B aarch64le`
* 4× Cortex-A72 @ 1500 MHz; FreeMem 7530MB / 8128MB; 38 processes, 258 threads.
* Correct SoC and the 8 GB model — not the unsupported low-memory variant.

### 4.2 Serial console and privilege — **PASS**
* CH340 USB-TTL adapter, **115200 8N1**, on the Pi's mini-UART (`/dev/ser1`).
* **Root shell obtained with no password**: `root@console:/#`, reporting `uid=0(root)` on
  hostname `qnxpi`.
* This clears the previously recorded blocker "CAN driver install needs root; no `sudo`,
  root fs read-only".
* **This remains the fallback route** whenever SSH is misconfigured (§4.7) — a plain `ssh`
  fails in a way that looks like a board fault. It is also the only route in when Ethernet is
  absent, which was the situation for the whole 2026-10-06 session.

### 4.3 UART adapter — physical mode confirmed, TX proven
* `schedulix uart status` reports `UART Adapter Backend: physical` and
  `physical device /dev/ser1 configured at 115200 baud` — i.e. the adapter opened the real
  device rather than falling back to the simulated loopback.
* `open()`, `tcgetattr()` and `tcsetattr()` all succeed, and QNX permits a second open of
  `/dev/ser1` while the console owns it.
* **TX proven end-to-end**: `schedulix uart send SCHEDULIX_TX_PROOF_42` produced 21 bytes
  observed on the host COM6 **before** the program's own `printf` output — the signature of
  genuinely transmitted bytes rather than buffered echo.
* **Serial device caveat**: the board has exactly one serial device, `/dev/ser1`, driven by
  `devc-serminiuart` at base `0xfe215000` (the BCM2715 mini-UART/AUX, not the PL011).
  `/dev/ser1` **is** the console, so RX testing is confounded: bytes sent from the host are
  consumed by the login shell, and `uart receive` returned the console's own newline. This is
  a test-harness confound, not evidence that RX is broken.

### 4.4 GPIO marker — **PASS** (verified 2026-10-07, real hardware + LED)

`src/gpio_marker.c` was rewritten and the result verified on the board with an LED wired to
header pin 11 (BCM 17). **This supersedes the earlier MOCK / INCONCLUSIVE correction on TEST 6
in §3** — see §4.4.5 for why that correction was correct at the time.

Verbatim output of `schedulix gpio test`, run from the Momentics IDE against the QNX Pi 4:

```
GPIO Marker validation test:
[GPIO] BCM2711 registers mapped at 0xfe200000
GPIO Availability: REAL PHYSICAL
fsel before: 4=0 17=0 27=0  (1=output)
Toggling all three marker pins, 200 ms apart...
  BRAKE(GPIO4/pin7)  ... fsel=1  hw high=1 low=0 confirmed=YES  pulse=200360 us  delta=360 us  valid=YES
  ADAS (GPIO17/pin11) ... fsel=1  hw high=1 low=0 confirmed=YES  pulse=200903 us  delta=903 us  valid=YES
  DIAG (GPIO27/pin13) ... fsel=1  hw high=1 low=0 confirmed=YES  pulse=200924 us  delta=924 us  valid=YES
fsel after : 4=1 17=1 27=1  (1=output)
Validation: delta 360167 ns, valid: YES
Hardware readback: level after high=1, after low=0, confirmed=YES
```

**The operator physically observed the LED blink.** That is first-class evidence alongside the
software output, not a footnote: `GPLEV` readback proves the register state, and human eyes on
the wire prove the pad actually moved.

#### 4.4.1 What this establishes, point by point

| Claim | Evidence |
| --- | --- |
| `mmap(MAP_PHYS\|MAP_SHARED, NOFD, PROT_NOCACHE, 0xFE200000)` **succeeds on the target** | `[GPIO] BCM2711 registers mapped at 0xfe200000`, then `REAL PHYSICAL` rather than the mock-fallback string |
| Availability is **REAL PHYSICAL**, not MOCK | `GPIO Availability: REAL PHYSICAL` — this was the pass criterion |
| All three marker pins moved **fsel 0 → 1** (input → output) | `fsel before: 4=0 17=0 27=0` → `fsel after : 4=1 17=1 27=1`. This is the specific bug that previously made every marker invisible |
| `GPLEV` readback is real, not vacuous | `hw high=1 low=0 confirmed=YES` on **all three pins** — the pin reads back what it was asked to drive |
| Pulse width is within tolerance | ~200.9 ms against a 200 ms nominal; deltas of 360 / 903 / 924 µs = **0.18–0.46 % error**, inside the documented 50 µs + 5 % budget |
| Per-pin validity | `valid=YES` on all three |

#### 4.4.2 Prior board readings that motivated the rewrite

Read before the rewrite, and the reason a physical proof had been impossible:

```
GPIO  4: level=1 fsel=3 alt=4 func=TXD3  pull=UP
GPIO 17: level=0 fsel=0     func=INPUT  pull=DOWN
GPIO 27: level=0 fsel=0     func=INPUT  pull=DOWN
ls: /dev/mem: No such file or directory
```

`/dev/mem` does not exist on this image, so the old `open("/dev/mem")` mapping could never
succeed. All three pins were inputs or an alt function, so no output was possible.

> **Discrepancy flagged, not resolved.** `gpio-bcm2711 get 4` earlier reported GPIO 4 as
> `fsel=3 alt=4 func=TXD3`, but the `mmap` read in the verification run showed
> `fsel before: 4=0` (input). Most likely **different boots / different BSP driver state** —
> a driver that claims GPIO 4 for TXD3 would leave `fsel=3`, whereas a boot without that claim
> leaves it at the reset value 0.
>
> This matters and is an **open question**: **GPIO 4 is the TXD3 pin on this SoC.** Forcing it
> to output could disturb a driver that claims it. Confirm whether anything on the QNX image
> actually uses TXD3 before choosing a different marker pin for `BRAKE_CTL`.

#### 4.4.3 Three bugs the test run exposed (all now fixed)

1. **`gpio test` only ever toggled GPIO 4 (BRAKE).** An LED wired to pin 11 (ADAS) or pin 13
   (DIAG) stayed dark and the test looked broken. **The operator's LED was on pin 11 and was
   therefore never driven** — this was the direct cause of "the LED didn't turn on".
   *Fix:* the test now drives all three pins, 200 ms apart, printing `fsel`, both readback
   levels, pulse width, delta and validity for each.
2. **`fsel` was printed before the toggle only.** `gpio_marker_set()` configures the pin on
   first use, so the pre-toggle print could only ever show the reset state — making a working
   pin look untouched.
   *Fix:* `fsel` is now printed **both before and after**.
3. **The `GPFSEL` read-modify-write ran on every edge.** That inflated the measured pulse from a
   2.0 ms nominal to **~2.98 ms** in the first run, which is why that run reported
   `delta 984426 ns, valid: NO`.
   *Fix:* per-pin configuration is cached in a `g_pin_configured[]` array, so the register
   write is paid **once per pin**.

#### 4.4.4 What the rewrite changed in the code

| Change | Where |
| --- | --- |
| `/dev/mem` + `mmap(NULL, 4096, …)` replaced with `mmap(NULL, __PAGESIZE, PROT_NOCACHE\|PROT_READ\|PROT_WRITE, MAP_PHYS\|MAP_SHARED, NOFD, 0xFE200000)` | `src/gpio_marker.c:143-146` |
| `GPFSELn` written (`001` = output) **before** any `GPSET0`/`GPCLR0`, once per pin | `:243-246` |
| `gpio_marker_get()` implemented via `GPLEV` (byte `0x34` = word 13), bank 1 at `+32` | `:213-225` |
| GPIO range extended to 0–53 | `GPIO_MAX_PIN 53` at `:71` |
| Hardware confirmation added to validation: `hw_level_high`, `hw_level_low`, `hw_confirmed`; `valid` now requires **both** tolerance **and** `hw_confirmed` | `:324-332` |
| Markers given dedicated event types instead of masquerading as `TRACE_CPU_MIGRATION` | `:292`, `:297` |

#### 4.4.5 Status of the earlier MOCK / INCONCLUSIVE correction

Kept visible in §3 rather than deleted, because it was **right about the old code**:

* The `qnx_logs_7.txt` run genuinely printed `[GPIO] no HW, mock mode` — the mapping failed,
  so nothing physical could be claimed from it.
* The three defects above (no `/dev/mem`, no `GPFSEL` write, stub readback) each independently
  made the test vacuous.
* **What superseded it** is the rewrite plus the run above: registers mapped, `fsel` confirmed
  0 → 1 on all three pins, `GPLEV` readback confirmed on all three pins, and a physically
  observed LED.

One consequence worth stating plainly: the analyzer previously **discarded** GPIO marker records
entirely (`TRACE_CPU_MIGRATION` fell through `analyzer.c`'s `default: break`). That is no longer
true — see §4.4.6.

#### 4.4.6 Trace-side change: markers are now first-class records

* `trace_event_type_t` gained `TRACE_GPIO_MARKER_HIGH = 14` and `TRACE_GPIO_MARKER_LOW = 15`,
  **appended at the end** so existing values 0–13 keep their meaning. Binary trace records are
  wire format; inserting mid-enum would silently renumber archived traces and the Qt frontend.
* Name strings added: `"GPIO_MARKER_HIGH"`, `"GPIO_MARKER_LOW"` (`src/trace_schema.c:18-19`).
* **The analyzer no longer discards them.** `analyzer.c:152-157` captures the edges into
  `gpio_marker_high_ns` / `gpio_marker_low_ns`. They are deliberately kept **separate** from
  `external_event_time`: a marker is self-generated by this process, not an incoming trigger, so
  treating it as one would corrupt root-cause attribution.
* **They survive `TRACE_MODE_EVENT_ONLY`.** `trace_collector.c:38-39` adds both types to the
  `need_drop()` keep-list, because dropping the instrument-validation signal would defeat its
  purpose.

### 4.5 Deployment channel — Momentics IDE

The Momentics IDE was proven as a **deployment and log channel**: it compiles, deploys to the
board, runs the binary and streams the target's `stdout` live into the IDE console. Verified by
running `gpio test` and receiving the output quoted in §4.4.

This is now a **proven deployment path alongside the serial console**, and the one that does
**not** depend on the fragile serial upload path (`base64` + foreground `head -c N` over an
ungated 115200 link, which cost two board power-cycles before it worked reliably). For larger
binaries prefer the IDE.

Serial remains the right tool for **when Ethernet is unavailable**, and it stays the
**recovery route when SSH is misconfigured** (§4.7) — the `-m hmac-sha2-256` requirement means a
plain `ssh` can fail in a way that looks like a board fault.

A third channel is now available: **`scp` over SSH** (§4.7), which is both fast and gives root
once `PermitRootLogin yes` is set. That makes the fragile serial upload path unnecessary for
Phase 4.

### 4.6 Host-side unit tests — **PASS**
Re-run during this session, all green:
* `tests/run_tests.py` — **5/5**
* `tests/test_phase6.py` — **13/13**
* `tests/test_analyzer_integrity.py` — **3/3**

### 4.7 SSH access — **PASS** (verified 2026-10-07)

SSH was unusable for the whole 2026-10-06 session because Ethernet was not connected. With
Ethernet restored on 2026-10-07 it was brought up, and three *different* failures had to be
cleared in sequence before a login succeeded. The board and the network did not change at any
point; three symptoms, three independent root causes, all recoverable.

**The working command, verified live:**

```powershell
ssh -m hmac-sha2-256 qnxuser@192.168.10.5     # password: qnxuser
```

Then escalate with `su`, password `root`. Root SSH was enabled afterwards with
`sed -i 's/^#*PermitRootLogin.*/PermitRootLogin yes/' /usr/etc/ssh/sshd_config`, `slay -f sshd`,
`/usr/bin/sshd -f /usr/etc/ssh/sshd_config`. `scp -o "MACs=hmac-sha2-256"` works for the same
reason, which is what unblocks Phase 4.

#### 4.7.1 Three symptoms, three root causes

| Client | Symptom | Root cause |
| --- | --- | --- |
| Momentics IDE terminal | `Error connecting 192.168.10.5 : SSH client error: Algorithm negotiation fail` | The IDE's SSH client offers no MAC algorithm the board accepts, and gives no way to pass `-m`. It fails **before** authentication, so credentials are irrelevant. |
| `ssh root@192.168.10.5` (no flag) | `Corrupted MAC on input.` / `message authentication code incorrect` | The client picks `hmac-sha2-512-etm@openssh.com` by default; the board's **`-etm` MAC implementations are broken**. |
| `ssh root@192.168.10.5` (with `-m hmac-sha2-256`) | Password prompt three times, then silent failure | `PermitRootLogin no` — **root login is disabled by default on the QSTI image**. |

Note the ordering the failures appeared in, because it is diagnostic: the *third* attempt got
as far as a password prompt, which by itself proves the MAC negotiation had already been fixed.
The remaining failure was an authorization policy, not a cryptography failure.

#### 4.7.2 The authoritative `sshd_config`, read from the board

```
PermitRootLogin yes
Protocol 2
HostKey /data/var/ssh/ssh_host_rsa_key
HostKey /data/var/ssh/ssh_host_ed25519_key
Ciphers aes128-ctr,aes192-ctr,aes256-ctr
MACs hmac-sha2-512-etm@openssh.com,hmac-sha2-256-etm@openssh.com,umac-128-etm@openssh.com,hmac-sha2-512,hmac-sha2-256,umac-128@openssh.com
KexAlgorithms curve25519-sha256@libssh.org,ecdh-sha2-nistp256,ecdh-sha2-nistp384,ecdh-sha2-nistp521,diffie-hellman-group-exchange-sha256
AuthorizedKeysFile      .ssh/authorized_keys
UsePAM yes
PasswordAuthentication no
PermitUserEnvironment yes
PidFile none
Subsystem sftp /system/bin/sftp-server
SshdSessionPath /system/bin/sshd-session
```

Two conclusions follow, and both are worth stating because the second one is counter-intuitive:

1. **The `-etm` MAC variants are listed first, and they are broken.** A client takes the
   server's first preference by default, which is `hmac-sha2-512-etm@openssh.com`, producing
   `Corrupted MAC on input`. Forcing the **non-ETM** `hmac-sha2-256` — which is *also* in the
   list — works. So `-m` is required because the board's *preferred* MAC implementation is
   defective, **not** because the board lacks modern algorithms. It has them; some of them are
   broken. The QNX QSTI guidance (`ssh -m hmac-sha2-256` for Windows clients) is correct, but
   the underlying reason is more specific than "QNX does not offer modern algorithms".

2. **`PasswordAuthentication no` + `UsePAM yes` means the `qnxuser` login succeeded through
   keyboard-interactive → PAM, not through password authentication.** This explains two things
   that otherwise look contradictory: `qnxuser` logged in *despite* `PasswordAuthentication no`,
   and `root` failed for a **different** reason (`PermitRootLogin no`) rather than because the
   password was wrong. `PasswordAuthentication no` alone would not have blocked either account.

   *Inferred from the config plus the observed successful login — not confirmed by a
   packet capture or an sshd trace.* See [§4.7.4](#474-read-from-the-board-but-not-yet-exercised).

#### 4.7.3 Credentials

| Account | Password | Notes |
| --- | --- | --- |
| `qnxuser` | `qnxuser` | Default. Works over SSH **with `-m hmac-sha2-256`**. |
| `root` | `root` | Default. `su` from `qnxuser` works with this. Root SSH was disabled until `PermitRootLogin yes` was set. |
| serial console | *(none)* | Passwordless root shell at `root@console:/#`. No SSH involved. |

Source for the defaults and for the root-login-disabled behaviour: the
[QNX QSTI for Raspberry Pi guide, "Interacting with the system"](https://www.qnx.com/developers/docs/qnxeverywhere/com.qnx.doc.target_images/topic/qsti/interacting-with-the-system.html).
The credentials themselves were then confirmed against the board.

#### 4.7.4 Read from the board, but not yet exercised

Recorded as configuration state, not as results:

* The keyboard-interactive → PAM path for `qnxuser` (§4.7.2 conclusion 2) is an inference.
* `Subsystem sftp` and `SshdSessionPath /system/bin/sshd-session` are present in the config and
  were not directly exercised; `scp` was confirmed working, but as a client-side observation.
* `AuthorizedKeysFile .ssh/authorized_keys` exists as a setting; **no key-based login has been
  set up or tested.**
* The PC has two Ethernet adapters (`192.168.10.1` for the board, `192.168.56.1`), and the
  second confuses routing. That is a host-side issue, not board state, but it is a prerequisite
  for any SSH work.

#### 4.7.5 A process failure worth recording

`tools/qnx_bringup_check.sh` and `tools/qnx_ssh_diag.sh` were run **from Git Bash on the Windows
PC** instead of on the board. The output described the Windows machine, not the board — the
giveaway was the line

```
C:/Users/User/AppData/Local/Temp  238G  205G   33G  87% /tmp
```

which is Git Bash's MSYS `/tmp` mapping. Every `[GAP]` in that output ("no IPv4 address", "no
serial device nodes", "`/dev/mem` not readable", "NOT root") was a Windows artefact.
`qnx_ssh_diag.sh` printed nothing for the same reason — its output went to
`C:\Users\User\AppData\Local\Temp\ssh_diag.txt`.

**Rule recorded:** QNX shell scripts in `tools/` must be transferred to the board and run there
(`sh /tmp/<script>.sh`). They are not portable to Windows or Git Bash. This makes them useless
as the *first* diagnostic when SSH itself is broken — the serial console is the fallback route
in. Full account in `docs/SESSION_LOG_2026-10-06.md` §16.4.

#### 4.7.6 Board `/tmp` as inventoried over this SSH session

Observed, as root, after the reboots. `/tmp` **survived** them — it is `/data/var/tmp` on
`/dev/hd0t179`, persistent.

* `schedulix` — 154,384 B. This is the **stale** `deploy/` binary ([§4.4](#44-gpio-marker--pass-verified-2026-10-07-real-hardware--led) is the current build at ~332 KB).
* `schedulix_can` — 150,520 B. **Unexplained**: matches no known build. Flagged for deletion.
* Neither is the binary that passed the GPIO LED verification. That run used the Momentics IDE,
  which deploys over `qconn` and does **not** use `/tmp`. Do not assume `/tmp` holds the
  verified build.
* Junk debris from the failed serial upload experiments: `cal`, `d.bin`, `echo`, `hp.txt`,
  `p.bin`, `spd`, `schedulecho`, `scheecho`, `seecho` (zero-byte and 1-byte files;
  `schedulecho` and `scheecho` are clearly mangled command fragments), plus `spi.conf`,
  `spi2.conf`, `elvis1.ses`, `T3`, `keep_files`, `qnx_bringup_check.sh`.

**Action: clean `/tmp` and redeploy deliberately.** Not yet done.

### 4.8 Still unverified
* **CAN hardware** — the MCP2515 driver is still **not** installed on this board. The transport
  blocker that stopped it is gone (`scp -o "MACs=hmac-sha2-256"` works, §4.7), so the artefacts
  can now be uploaded directly; what remains is the HAT not being mounted and the crystal
  frequency being unconfirmed (§4.8, GPIO 4 item below is separate). Phase 4 is **unblocked but
  not started**.
* **UART receive against an external peer** — unverified, for the console-sharing reason in
  §4.3.
* **GPIO 4 / TXD3 contention** — open question, see §4.4.2. GPIO 4 is the TXD3 pin on this SoC;
  whether forcing it to output disturbs a driver that claims it is **not** established.
* **Marker timing under real workload load** — §4.4 measures a deliberate 200 ms pulse in a
  test harness. Marker jitter while `BRAKE_CTL` / `ADAS_FUSION` / `DIAG_POLL` are running is
  **not** yet characterised.
* **The keyboard-interactive → PAM path** — inferred from `UsePAM yes` + `PasswordAuthentication no`
  plus a successful login (§4.7.2). Not confirmed by a packet capture or an sshd trace.
* **Key-based SSH login** — `AuthorizedKeysFile` is configured but no key has been installed or
  tested.
* **The cause of the broken `-etm` MACs** — worked around, not diagnosed. See §4.7.2 conclusion 1.
