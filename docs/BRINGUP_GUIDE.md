# Schedulix Bring-Up Guide

**Complete, reproducible procedure: from a clean clone to CAN frames on the wire.**

This is the single document to follow. It assumes you are starting from
nothing: a fresh clone, a Raspberry Pi 4 with the QNX Quick Start Target Image
(QSTI), and no prior knowledge of this session's history.

- Target: **QNX OS 8.0.0** (`qnxpi`), **aarch64le**, Raspberry Pi 4
- Toolchain: **QNX SDP 8.0** (`qcc` 12.2.0, gcc 12.2.0)
- CAN controller: **MCP2515** on SPI0/CE0
- Repo: <https://github.com/Abdul99Aleem/Schedulix>

> **Read section 12 before you touch hardware.** It lists the mistakes that
> cost the most time on this project, including one that damaged a board.
> Every one of them was made by following incomplete instructions.

---

## Table of contents

1. [What you need](#1-what-you-need)
2. [Clone the repository](#2-clone-the-repository)
3. [Host-side verification (no board required)](#3-host-side-verification)
4. [Build for the target](#4-build-for-the-target)
5. [Get onto the board](#5-get-onto-the-board)
6. [Verify UART](#6-verify-uart)
7. [Verify GPIO markers](#7-verify-gpio-markers)
8. [Build the MCP2515 CAN driver](#8-build-the-mcp2515-can-driver)
9. [Install CAN on the QSTI image](#9-install-can-on-the-qsti-image)
10. [Verify CAN](#10-verify-can)
11. [Troubleshooting](#11-troubleshooting)
12. [Mistakes to avoid](#12-mistakes-to-avoid)
13. [What is verified vs. unverified](#13-what-is-verified-vs-unverified)

---

## 1. What you need

### Hardware

| Item | Notes |
| --- | --- |
| Raspberry Pi 4 (or 5) | 4 GB is plenty. Confirmed board: `RaspberryPi4B aarch64le`, 4× Cortex-A72 @ 1500 MHz, 8128 MB. Both Pi 4 and Pi 5 are `aarch64le`, so the same binary targets both. |
| microSD card with QNX QSTI 8.0 | Written per the [QSTI guide](https://www.qnx.com/developers/docs/qnxeverywhere/com.qnx.doc.target_images/topic/qsti/intro.html). |
| USB-TTL serial adapter (CH340 or similar) | **Strongly recommended.** It is your lifeline when SSH breaks. |
| MCP2515 CAN module | The Waveshare RS485 CAN HAT (SKU 14882) was used here. A bare MCP2515 on a breakout works too. |
| 120 Ω resistor | One at **each end** of a two-node bus. |
| Second CAN node | Optional but needed to see frames. See section 10. |

### Software on your PC

| Item | Notes |
| --- | --- |
| Git | Any recent version. |
| Python 3.9+ | 3.11.8 used here. Only needed for the host tests and helper tools. |
| QNX SDP 8.0 | The **SDP**, not just the IDE. `C:\QNX` on this machine holds only the IDE and is **not** sufficient. |
| SSH client | OpenSSH. Git for Windows ships one. |

### About the serial console wiring

This trips up almost everyone, including us.

```
Pi pin  6  (GND)                 -> adapter GND
Pi pin  8  (GPIO14, Pi TX out)   -> adapter RX      <-- crossed
Pi pin 10  (GPIO15, Pi RX in)    -> adapter TX      <-- crossed

adapter VCC -> Pi pin 2 (5V)     <-- optional; USB-TTL often needs 5V
```

**TX and RX must cross over.** Straight-through wiring gives you a silent
console on a perfectly healthy board, which is indistinguishable from a dead
board. Check this before concluding anything is broken.

---

## 2. Clone the repository

```bash
git clone https://github.com/Abdul99Aleem/Schedulix.git
cd Schedulix/schedulix_can          # if you cloned the parent repo
```

Or work on a branch:

```bash
git clone https://github.com/Abdul99Aleem/Schedulix.git
cd Schedulix/schedulix_can
git checkout hw/procurement-and-gap-plan
```

### SSH client configuration (important)

The board's `sshd_config` lists broken `-etm` MAC algorithms first. Standard
clients negotiate the server's first preference and fail with
`Corrupted MAC on input`. You must force a working algorithm:

```bash
ssh -m hmac-sha2-256 qnxuser@192.168.10.5
```

To stop typing that, create `~/.ssh/config` (`C:\Users\<you>\.ssh\config` on
Windows). **Each directive needs its own line:**

```
Host qnxpi
    HostName 192.168.10.5
    User qnxuser
    MACs hmac-sha2-256
```

Then `ssh qnxpi` is enough.

> **This is a common mistake.** Writing
> `Host qnxpi 192.168.10.5` followed by `User qnxuser` does **not** work —
> `Host qnxpi 192.168.10.5` is parsed as *two host patterns*, so SSH matches
> nothing useful and silently falls back to your local Windows username,
> `User`. Until you get the config right, always use the explicit form
> `ssh -m hmac-sha2-256 qnxuser@192.168.10.5`.

For file transfer, the same override applies:

```bash
scp -o "MACs=hmac-sha2-256" build/aarch64le-debug/schedulix_can qnxuser@192.168.10.5:/tmp/
```

### Getting a root shell

The QSTI image has **no `sudo`**. Escalate with `su`:

```bash
ssh -m hmac-sha2-256 qnxuser@192.168.10.5
# password: qnxuser
$ su
# password: root
root@qnxpi:/data/home/qnxuser#
```

Root over SSH is also possible once `PermitRootLogin yes` is set in the
board's `sshd_config`.

**The serial console needs no password at all** — it drops you straight at
`root@console:/#`. See [docs/SERIAL_CONSOLE_RUNBOOK.md](SERIAL_CONSOLE_RUNBOOK.md).

---

## 3. Host-side verification

Everything in section 3 runs on your PC and needs **no board**. Do it first —
it is fast and it isolates tooling problems from hardware problems.

```bash
python tools/run_all_tests.py            # everything, one command
```

Expected:

```
============================================================
PASS run_tests.py                 CAN frame decode             5 case(s)
PASS test_phase6.py               stress scenarios + analyzer  13/13
PASS test_analyzer_integrity.py   trace integrity              3/3
PASS test_uart_tx_marker.py       UART TX trace marker         5/5
PASS test_spi_conf_edit.py        installer spi.conf edit      13/13
============================================================

39/39 host tests PASS
```

Or run them individually:

```bash
python tests/run_tests.py                  # CAN frame decode       5/5
python tests/test_phase6.py                # stress + analyzer     13/13
python tests/test_analyzer_integrity.py    # trace integrity        3/3
python tests/test_uart_tx_marker.py        # UART TX trace marker   5/5
python tests/test_spi_conf_edit.py         # spi.conf edit         13/13
```

`test_spi_conf_edit.py` needs `awk` on your `PATH`. On Windows that means
running it from **Git Bash**, not PowerShell:

```bash
# Git Bash
python tools/run_all_tests.py
```

If `awk` is missing the suite reports **SKIP**, not FAIL, and the aggregate
drops to 26/26. That is a missing host tool, not a code failure — but it does
mean the installer's config edit is **unvalidated**, so run it once somewhere
with `awk` (Git Bash, Linux, macOS) before trusting the installer on a board.

### Optional: build and run the whole backend on your PC

`Makefile.host` produces a native binary so you can exercise the CLI without a
board at all. It needs a host toolchain:

- **Linux / macOS**: `gcc` and `make` are usually already there.
- **Windows**: you need a compiler and `make`. Git Bash alone is **not**
  enough — it has neither. Options: MSYS2 (`pacman -S mingw-w64-x86_64-gcc make`),
  WSL, or a full Visual Studio / MinGW install.

```bash
make -f Makefile.host          # -> build-host/schedulix_can
./build-host/schedulix_can help
```

This path is a convenience for CLI exploration. It is **not** a substitute for
section 4 — the QNX build is the one that runs on the board, and the two
differ (static `libtraceparser`, `mmap(MAP_PHYS)`, POSIX serial).

---

## 4. Build for the target

You need the **SDP's** environment, which puts `qcc` and `make` on the path.

**Windows (cmd):**

```bat
call C:\Users\<you>\qnx800\qnxsdp-env.bat
make
```

**Linux / macOS (bash):**

```bash
. ~/qnx800/qnxsdp-env.sh
make
```

`qnxsdp-env` sets `QNX_HOST`, `QNX_TARGET` and prepends the SDP `usr/bin` to
`PATH`. Without it you get `qcc: command not found`.

### Build variants

```bash
make                                    # aarch64le debug  (default, Raspberry Pi 4/5)
make BUILD_PROFILE=release              # -O2, smaller binary
make rebuild                            # clean + rebuild
make clean
```

`PLATFORM` is configurable, but **only the variants installed in your SDP will
work.** Check with `qcc -V` before assuming one is available. On the SDP 8.0
install used for this project:

```
12.2.0,gcc_ntoaarch64le          <-- use this for Raspberry Pi 4/5
12.2.0,gcc_ntoaarch64le_cxx
12.2.0,gcc_ntoaarch64le_gpp
12.2.0,gcc_ntox86_64             <-- QNX VM target
12.2.0,gcc_ntox86_64_cxx
12.2.0,gcc_ntox86_64_gpp
```

`armv7le` (Raspberry Pi 2/3) is **not** in that list — `make PLATFORM=armv7le`
fails at the first compile with an unsupported-variant error until the optional
variant is installed. Raspberry Pi 4 and 5 both use `aarch64le`.

### Expected output

```
$ make
qcc -Vgcc_ntoaarch64le -c ... -o build/aarch64le-debug/src/analyzer.o ...
...
qcc -Vgcc_ntoaarch64le -o build/aarch64le-debug/schedulix_can  ... -lsocket -lm -Wl,-Bstatic -ltraceparser -Wl,-Bdynamic
```

**Zero errors and zero warnings is the pass criterion.** The binary lands at:

```
build/aarch64le-debug/schedulix_can          (333,560 bytes, debug)
```

To shrink it before uploading:

```bash
ntoaarch64-strip build/aarch64le-debug/schedulix_can
```

`ntoaarch64-strip` ships with the SDP at `$QNX_HOST/usr/bin`, so it is on the
`PATH` once `qnxsdp-env` has been sourced. Measured on this project:
**333,560 → 130,160 bytes** (61% smaller).

That matters because **serial upload over the CH340 is unreliable above roughly
330 KB** and caused repeated board power-cycles. Stripping first is what makes
a serial fallback viable — see section 5c.

> Do **not** confuse `Makefile` (QNX target) with `Makefile.host` (native PC
> build). Section 4 uses `Makefile`; section 3's optional step uses
> `Makefile.host`.

---

## 5. Get onto the board

Three access paths, all verified. Pick one.

### 5a. SCP (fastest for a standalone binary)

```bash
scp -o "MACs=hmac-sha2-256" build/aarch64le-debug/schedulix_can qnxuser@192.168.10.5:/tmp/
ssh -m hmac-sha2-256 qnxuser@192.168.10.5
$ su
# password: root
root@qnxpi:/# cp /tmp/schedulix_can /system/bin/   # if writable, else just /tmp
root@qnxpi:/# chmod 755 /tmp/schedulix_can
root@qnxpi:/# /tmp/schedulix_can help
```

> Serial upload over the CH340 was tried and is **not reliable** above roughly
> 330 KB and caused repeated board power-cycles. Use SCP.

### 5b. Momentics IDE (the Run button)

The IDE compiles, deploys over `qconn`, runs, and streams target `stdout` to
its console. It is unaffected by the SSH MAC problem, so it is the most
reliable route when SSH misbehaves. It gives no root shell — use SCP when you
need root.

### 5c. Serial console

The escape hatch when nothing else works. See
[docs/SERIAL_CONSOLE_RUNBOOK.md](SERIAL_CONSOLE_RUNBOOK.md), or:

```powershell
powershell -File tools/serial_console.ps1
```

Verify the board answers:

```
# on the board
uname -a
# expect: QNX qnxpi 8.0.0 ... RaspberryPi4B aarch64le

pidin info
```

---

## 6. Verify UART

UART is the easier of the two hardware interfaces and a good first proof that
the deployment path works.

```bash
# on the board, as root
/tmp/schedulix_can uart status
```

Expected, on a QSTI image:

```
UART Adapter Backend: physical
physical device /dev/ser1 configured at 115200 baud
```

`physical` is the pass criterion. If you see a simulated/loopback backend,
`/dev/ser1` could not be opened.

### Prove transmission

With the USB-TTL on your PC attached to pins 8/10 (section 1), and a serial
client running on COM6 at 115200 8N1:

```bash
# on the board
/tmp/schedulix_can uart send SCHEDULIX_TX_PROOF_42
```

You should see `SCHEDULIX_TX_PROOF_42` on the PC's serial console. The proof
is that the 21 bytes arrive **before** the program's own `printf` output —
that ordering cannot happen unless the bytes really went out the wire.

---

## 7. Verify GPIO markers

```bash
# on the board, as root
/tmp/schedulix_can gpio test
```

Expected:

```
GPIO Availability: REAL PHYSICAL
```

Then three markers on pins **4 / 17 / 27** (header **7 / 11 / 13**) toggle
200 ms apart, each reporting `confirmed=YES`:

```
pin 4  high=1 low=0 confirmed=YES valid=YES
pin 17 high=1 low=0 confirmed=YES valid=YES
pin 27 high=1 low=0 confirmed=YES valid=YES
```

**Strongest confirmation: put an LED on header pin 11 and watch it blink.** That
proves the register writes reach the physical pin, not just the driver.

### How it works, and why it is written this way

There is **no `/dev/mem`** in this QNX image. An earlier implementation tried
to open it and failed; that approach is dead. The working approach maps the
BCM2711 peripheral block directly:

```c
mmap(NULL, 0x1000, PROT_READ|PROT_WRITE,
     MAP_PHYS|MAP_SHARED, NOFD, 0xFE200000);
```

Register accesses:

| Register | Offset | Purpose |
| --- | --- | --- |
| `GPFSEL0-9` | `0x00` | function select — write `001` to make a pin an output |
| `GPSET0/1` | `0x1C` / `0x20` | set pins high |
| `GPCLR0/1` | `0x28` / `0x2C` | set pins low |
| `GPLEV0/1` | `0x34` / `0x38` | read actual pin levels |

Two details that matter:

1. **`fsel` must be set to output *before* driving.** Setting the level on a
   pin that is still an input does nothing visible.
2. **Bank 1 is at `+32`.** Pin N ≥ 32 uses register offset `N - 32`.

Measured pulse width on hardware was **200.9 ms against a 200 ms nominal**
(0.18–0.46% error across pins).

> **Open item.** GPIO 4 is the **TXD3** pin on this SoC. One inspection run
> reported `fsel=3 alt=4 func=TXD3` while the verification run read `fsel=0`,
> so BSP driver state appears to vary between boots. Forcing it to output
> could disturb a driver that claims it. Confirm nothing on your image uses
> TXD3 before relying on pin 4. Pins 11 and 13 are unaffected.

> **Pi 5.** The `0xFE200000` base above is verified on **Pi 4 / BCM2711 only**.
> The BCM2712 in the Pi 5 largely retains the BCM2711 peripheral map for driver
> compatibility, so this may well work unchanged — but that has **not** been
> verified. Run `gpio test` and confirm `confirmed=YES` and a visible LED before
> trusting marker timings on a Pi 5.

---

## 8. Build the MCP2515 CAN driver

The stock QNX QSTI image **does not include** a CAN driver. The MCP2515 driver
must be built from QNX's source.

### 8a. Use the prebuilt binary (fastest)

Release and debug builds are **already committed** to the repo at:

```
third_party/can-mcp2515/aarch64le/bin/can-mcp2515       (release, 148,840 bytes)
third_party/can-mcp2515/aarch64le/bin/can-mcp2515_g     (debug)
```

Skip to section 9 if that suits you.

### 8b. Build from source

Source is vendored under `third_party/can-mcp2515`, from
<https://gitlab.com/qnx/projects/drivers/can-mcp2515>, commit `0fd11af`.

```bash
# Git Bash / Linux / macOS
sh tools/build_can_driver.sh
```

The script locates the SDP itself, in this order: an already-sourced
environment (`QNX_HOST`/`QNX_TARGET`), then `$QNX_SDP`, then a guess at the
default install path. It prints which one it used. To be explicit:

```bash
QNX_SDP=/opt/qnx800 sh tools/build_can_driver.sh
```

Expected output:

```
== SDP
   root    : /c/Users/User/qnx800  (guessed)
   host    : /c/Users/User/qnx800/host/win64/x86_64
   target  : /c/Users/User/qnx800/target/qnx

== seeding make infrastructure from the SDP
   seeded 11 directories

== building (release + debug, aarch64le)
...
== result
Driver built.
```

**`seeded 11 directories` is the line to check.** If it says `seeded 0`, the
seeding silently did nothing — see the PATH trap below. The script now treats
that as a hard error rather than letting the build fail confusingly later.

Output lands in `third_party/can-mcp2515/build/aarch64le/bin/`. The committed
copies live in `third_party/can-mcp2515/aarch64le/bin/`.

> **Reproducibility.** A fresh build reproduces the committed binaries at
> **exactly** the same size — 148,840 bytes release, 332,808 debug — but the
> SHA-256 differs, because the driver embeds the repository commit id at compile
> time (you can see `REPO_STATUS=<sha>` in the compile line). Same size, not
> same hash, is expected and is not a problem.

Or build by hand:

```bash
# Linux / macOS
. ~/qnx800/qnxsdp-env.sh
cd third_party/can-mcp2515
make INSTALL_ROOT_nto=${PWD}/build USE_INSTALL_ROOT=1 hinstall install

# Windows
call C:\Users\<you>\qnx800\qnxsdp-env.bat
cd third_party\can-mcp2515
make INSTALL_ROOT_nto=%CD%\build USE_INSTALL_ROOT=1 install
```

Note the **manual recipe omits `hinstall` on Windows**; the recursive make
handles it differently there. The `tools/build_can_driver.sh` wrapper works on
both and is the path to prefer.

> **Why the script copies files around first.** The driver uses QNX recursive
> make, whose Makefiles contain bare `include recurse.mk` and
> `include qconfig.mk`. GNU make resolves an unqualified include against the
> **current working directory**, not the directory of the including file. Since
> the build recurses through `lib/nto/aarch64/a.le` and `driver/nto/aarch64/le.g`,
> those files must exist in **every** directory containing a Makefile. They ship
> with the SDP, not with the repo:
>
> ```
> $QNX_TARGET/usr/include/recurse.mk
> $QNX_TARGET/usr/include/mk/qconfig.mk
> $QNX_TARGET/usr/include/mk/qconf-{win64,linux,nto}.mk
> ```
>
> `MKFILES_ROOT` must point at `$QNX_TARGET/usr/include/mk`. Without this the
> build dies at `Makefile:16: recurse.mk: No such file or directory`.
>
> These generated files are covered by `third_party/can-mcp2515/.gitignore`, so
> they never reach the repository.

> **Windows PATH trap — worth understanding, because it fails silently.**
> `$QNX_HOST/usr/bin` ships its **own** `find.exe`, `cp.exe`, `mkdir.exe` and
> `dirname.exe`. If you prepend it to `PATH` *before* doing the seeding, those
> shadow the MSYS/GNU tools, and the SDP copies do not understand MSYS paths
> like `/c/Users/...` — they report `No such file or directory` for
> directories that plainly exist. The failure is invisible, because `find`
> runs inside a `for` word-list where `set -e` cannot see it, so the loop just
> seeds nothing.
>
> The script therefore **seeds first, with the original tools, and only then
> switches `PATH` over** for `make`. Keep that ordering if you edit the script.

---

## 9. Install CAN on the QSTI image

> **Read section 12.1 before wiring anything.** The HAT must seat on the
> 40-pin header. Flying jumper wires on this HAT caused a hardware failure
> that ended bring-up.

### 9.1 Wiring

The MCP2515 HAT goes straight onto the header. For reference, the signals it
uses are:

| Pi header pin | BCM | Signal | Module pin |
| --- | --- | --- | --- |
| 1 | — | 3.3 V | VCC |
| 6 | — | GND | GND |
| 19 | 10 | MOSI | SI |
| 21 | 9 | MISO | SO |
| 23 | 11 | SCK | SCLK |
| **24** | **8 (CE0)** | **CS** | CS |
| **22** | **25** | **INT** | INT |

Two of these are easy to get wrong and both break CAN silently:

- **CS must be on pin 24 (CE0).** The driver opens `/dev/io-spi/spi0/dev0`,
  which is chip-select 0.
- **INT must be on pin 22 (BCM GPIO 25).** The driver is started with
  `-g 25`; a mismatch means interrupts never arrive.

### 9.2 Crystal frequency — read it, do not assume

A wrong crystal value is the single most common CAN failure, and it fails
**silently**: the driver starts cleanly and you receive zero frames, with no
error message.

Read the marking on the silver can-shaped crystal next to the MCP2515. On our
boards it reads **`EAS12.000` = 12 MHz**, which is what the installer uses.

For the Waveshare RS485 CAN HAT (SKU 14882):

| Board | Crystal |
| --- | --- |
| Current | 12 MHz |
| Pre-Aug-2019 | 8 MHz |

The SKU alone does not disambiguate these, and the Waveshare product page does
not state it. Generic bare MCP2515 modules are usually 8 MHz.

### 9.3 Upload and run the installer

```bash
scp -o "MACs=hmac-sha2-256" third_party/can-mcp2515/aarch64le/bin/can-mcp2515 qnxuser@192.168.10.5:/tmp/
scp -o "MACs=hmac-sha2-256" tools/qnx_can_install.sh              qnxuser@192.168.10.5:/tmp/
```

Then, as **root** on the board:

```sh
ssh -m hmac-sha2-256 root@192.168.10.5
# or: su  from a qnxuser session
root@qnxpi:/# sh /tmp/qnx_can_install.sh
```

### 9.4 What the installer does

| Step | Action |
| --- | --- |
| 1/8 | Verify `/tmp/can-mcp2515`, `/dev/io-spi/spi0/`, `dev0` all present |
| 2/8 | Install driver to `/system/bin/can-mcp2515`, mode 755 |
| 3/8 | Retune **only** `spi0/dev0` in `spi.conf`; back up the stock file once |
| 4/8 | Tell you to reboot; attempt an in-place driver restart |
| 5/8 | Stop any running `can-mcp2515`, clear stale `/dev/can*` |
| 6/8 | Start `can-mcp2515 --mid=eid -s .../dev0 -c 12000000 -g 25 -B 500000` |
| 7/8 | Verify `/dev/can0` exists; dump `slog2info` if not |
| 8/8 | `canctl` catch-all receive filter |

**If step 4 stops with exit code 2, that is expected, not a fault.** The QNX
CAN DDK says to **reboot the target** after editing `spi.conf`. Reboot and
re-run the script; it is safe to re-run.

```sh
# on the board
shutdown && reboot
# then
sh /tmp/qnx_can_install.sh
```

### 9.5 The spi.conf change, explained

The stock QSTI `spi0/dev0` is configured for a different peripheral:

| Key | Stock | Required for MCP2515 | Why |
| --- | --- | --- | --- |
| `cpha` | `1` | **`0`** | MCP2515 supports SPI **mode 0** and **mode 3** only. Stock is mode 1, which the part cannot do. |
| `cpol` | `0` | `0` | already correct |
| `word_width` | `32` | **`8`** | MCP2515 is an 8-bit SPI device. 32-bit words misalign every frame. |
| `clock_rate` | `5000000` | *unchanged* | 5 MHz is well within the MCP2515's 10 MHz maximum. Changing it buys nothing and risks signal-integrity problems on flying wires. |

The installer edits **only the three wrong keys, inside only the `spi0/dev0`
block.** Everything else — `spi0/dev1`, the `spi3` bus, comments — is passed
through byte-identical. This is enforced by `tests/test_spi_conf_edit.py`,
which asserts the `spi3` bus (`base=0xfe204600 irq=151`) survives.

> **Regression this prevents.** An earlier installer replaced the whole file
> with a DDK-derived one, silently dropping `spi0/dev1` and the entire `spi3`
> bus. It is not what caused the SPI failure (the stock file failed
> identically), but it was a real defect and is now fixed and regression-tested.

**Doing it by hand**, if you prefer — in the block where `devno=0`:

```
cpha=0
cpol=0
word_width=8
```

To restore:

```sh
cp /system/etc/config/spi/spi.conf.stock.<timestamp> /system/etc/config/spi/spi.conf
shutdown && reboot
```

---

## 10. Verify CAN

### 10.1 Devices appear

```sh
ls -d /dev/can0/*/
```

Expect five mailbox nodes:

```
/dev/can0/rx0
/dev/can0/rx1
/dev/can0/tx2
/dev/can0/tx3
/dev/can0/tx4
```

`rx0`/`rx1` are receive mailboxes; `tx2`/`tx3`/`tx4` are transmit. With
`--mid=eid` the MIDs are `0, 1, 2, 3, 4` respectively. (With `--mid=sid` they
would be `0x0, 0x40000, 0x80000, 0xC0000, 0x100000`.)

### 10.2 Loopback test, one board

The fastest proof, requiring no second node. **Do this before** wiring a bus:

```sh
# terminal 1 - receive
canctl -u 0,rx0 -R 2000

# terminal 2 - transmit
canctl -u 0,tx2 -w 0x123,3,ABCDEF
```

A payload-only send is also possible:

```sh
echo TESTING > /dev/can0/tx2
```

### 10.3 Two-node test, two boards

```
Pi 1 module                        Pi 2 module
CAN_H ────────┬────────────────────── CAN_H
              │
            [T1] 120 Ω            [T2] 120 Ω
              │
CAN_L ────────┴────────────────────── CAN_L
  GND ───────────────────────────────  GND   (shared reference)
```

Fit terminators **at the two ends only**. On a 2-node bus, both ends
terminate.

```sh
# Pi 1
canctl -u 0,rx0 -R 2000

# Pi 2 (Linux)
cansend can0 123#ABCDEF
```

### 10.4 Feed CAN into Schedulix

```sh
/tmp/schedulix_can can status
/tmp/schedulix_can trace start --mode=FULL
/tmp/schedulix_can can send
/tmp/schedulix_can trace stop
/tmp/schedulix_can analyze
/tmp/schedulix_can report
```

The event pipeline dispatches CAN IDs to workloads: `0x100` → `BRAKE`,
`0x200` → `ADAS`, `0x300` → `DIAGNOSTIC`. Those dispatches have been observed
from the **simulator**; a real-bus frame driving a workload is the one
CAN-path claim still unverified (see section 13).

> **Phase 5 note.** `src/qnx_can_adapter.c` is an **empty stub** — the real
> adapter is `src/qnx_can_adapter.c.stub` and is not compiled. It must be
> enabled in the Makefile when CAN hardware work resumes. Note that the stub
> opens `/dev/can1`, which is **wrong** for a single module: correct paths are
> `/dev/can0/tx2` (transmit) and `/dev/can0/rx0` (receive).

---

## 11. Troubleshooting

### `spi-bcm2711` exits 1 with no output at all

The driver logs to **slog**, never to the terminal. This is the first thing to
check, always:

```sh
slog2info | grep -i spi | tail -30
```

Then:

```sh
pidin ar | grep -iE 'spi|mbox'     # anything holding the peripheral?
ls -l /dev/mailbox*                  # mailbox driver present?
spi-bcm2711 &                        # try with NO config argument
sleep 2
ls -l /dev/io-spi/ 2>&1
```

Exit 1 with zero output while `verbose=5` is set means the driver bailed
**before config parsing** — which means the config is not the problem. That is
a useful deduction; it saves re-editing `spi.conf` for no reason.

### `/dev/io-spi/spi0` missing

```sh
cat /system/etc/config/spi/spi.conf     # did the edit land?
ls -l /dev/io-spi/                       # did any bus appear?
```

Remember: **reboot**, do not just restart the driver. See 9.4.

### SSH: `Corrupted MAC on input`

Use `-m hmac-sha2-256`. See section 2.

### SSH: `PTY allocation request failed on channel 0`

**Authentication succeeded** — this happens afterwards, when the board cannot
allocate a pseudo-terminal. It is a resource-exhaustion symptom, not a
credential problem. Use the serial console and check:

```sh
pidin info
free
```

A power cycle clears it.

### SSH: `No route to host` / ping `General failure`

No ARP reply, so there is no electrical link. Check in this order:

1. Is the board powered — red LED on?
2. Is the Ethernet cable seated at **both** ends?
3. Are the link LEDs lit on both ends?

On your PC, `Get-NetAdapter` should show the Ethernet adapter as `Up` with a
non-zero link speed. If it reads `Disconnected 0 bps`, the problem is physical
layer, not SSH.

### Driver installed, zero frames received

Almost always the crystal. Re-read section 9.2, change `CLOCK_HZ` at the top
of `qnx_can_install.sh`, and re-run. Then check CS is on pin 24 and INT is on
pin 22.

### Get live driver logging

```sh
slay can-mcp2515
/system/bin/can-mcp2515 --mid=eid -s /dev/io-spi/spi0/dev0 -c 12000000 -g 25 -B 500000 -D
```

`-D` runs it in the foreground with full logging. Use it before guessing.

---

## 12. Mistakes to avoid

Every item here actually happened on this project.

### 12.1 Seating the CAN HAT on the 40-pin header

**This is the important one.** The MCP2515 HAT is designed to plug directly
onto the header. The silkscreen guarantees the pin mapping; hand-wiring the
SPI signals with jumper leads removes that guarantee entirely.

Connecting individual signal wires creates a direct path for a short — VCC or
GND onto MISO/MOSI/SCK/CS/INT. On this project that produced a progressive
failure: the HAT's power LED was solid on first power-up, blinked and went out
on the next cycle, then stopped lighting altogether. The board subsequently
failed to boot its OS while 5 V was still present. **Both the CAN work and the
board were lost.**

**Seat the HAT. Do not hand-wire it.**

If you have no 40-pin header available, buy one — they cost almost nothing —
and solder the HAT to that.

### 12.2 Hand-wiring the USB-TTL with TX/RX straight-through

Get a silent console on a healthy board, then spend an hour convinced the
board is dead. Cross TX and RX. See section 1.

### 12.3 Running `tools/*.sh` on your PC instead of the board

Every `.sh` in `tools/` is a **QNX** script. They must be copied to the board
and run there:

```sh
# WRONG - reports your Windows machine's state and invents false gaps
sh tools/qnx_can_install.sh

# RIGHT
scp -o "MACs=hmac-sha2-256" tools/qnx_can_install.sh qnxuser@192.168.10.5:/tmp/
# then, on the board:
sh /tmp/qnx_can_install.sh
```

MSYS maps `/tmp` to `C:\Users\<you>\AppData\Local\Temp`, so a Windows run looks
like it half-works and produces misleading output.

### 12.4 Writing `~/.ssh/config` directives on one line

`Host qnxpi 192.168.10.5` + `User qnxuser` does not work. One directive per
line. See section 2.

### 12.5 Believing a documented command was verified

Several commands in this project's history were written into runbooks and
**never actually executed**. One was an installer's root check:

```sh
[ "$(id)" = "root" ] || die "must run as root"
```

`$(id)` expands to `uid=0(root) gid=0(root) groups=0(root)`, which never equals
`root`. **The check could not pass for anyone, including root.** The correct
form is:

```sh
[ "$(id -u)" = "0" ] || die "must run as root"
```

Run the command. If it errors, you found something.

### 12.6 Editing a config file wholesale

Replacing `spi.conf` instead of editing the one wrong entry dropped two
sections. Back up first, change the minimum, and **assert the edit landed**:

```sh
sed -n '/devno=0/,/^$/p' /system/etc/config/spi/spi.conf
```

### 12.7 Answering QNX questions from memory

QNX differs from Linux in ways that silently break builds and runtime
behaviour. There is no `/dev/mem` on this image. There is no `sudo`. `/tmp` is
not where you think. `/system` is read-only and 100% full. `spi.conf` needs a
reboot, not a driver restart.

Consult the documentation:
<https://www.qnx.com/developers/docs/qnxeverywhere/index.html>

### 12.8 Power-cycling a board that will not boot

If a board shows red LED but no green activity LED and the CPU stays at room
temperature, the SoC is not executing. Repeated power-on will not fix it and
may stress components that are already struggling. Stop and diagnose.

### 12.9 Trusting a script step that failed silently

`set -e` does **not** catch a failure inside a `for` word-list:

```sh
for d in $(find "$X" -name Makefile -exec dirname {} \;); do ...; done
```

If `find` fails, the substitution is empty, the loop body never runs, and the
script reports success having done nothing. This actually happened in
`tools/build_can_driver.sh`: it reported `seeded 0 directories` and carried on.

**Count what you did and assert on it.** The script now fails hard if the
count is zero. Apply the same habit to your own scripts and to any result you
report:

```sh
n=$(ls -1 /dev/io-spi/spi0/ 2>/dev/null | wc -l)
[ "$n" -ge 1 ] || { echo "FAIL: no SPI devices"; exit 1; }
```

### 12.10 Shadowing your shell tools with the SDP

`$QNX_HOST/usr/bin` contains its own `find`, `cp`, `mkdir`, `dirname` and
`rm`. Prepend it to `PATH` and those replace the MSYS/GNU versions, which then
cannot handle `/c/...` paths. Anything you run after the prepend may report
`No such file or directory` for files that exist.

Seed and prepare with the normal tools; switch `PATH` over only for the build
itself.

---

## 13. What is verified vs. unverified

Honesty about evidence matters more than a green checklist. Full matrix in
[docs/PROBLEM_STATEMENT_COMPLIANCE.md](PROBLEM_STATEMENT_COMPLIANCE.md).

**Verified on real hardware:**

| Item | Evidence |
| --- | --- |
| QNX 8.0.0 on Pi 4 | `uname -a`, `pidin info` |
| Serial console, root shell | `root@console:/#`, 115200 8N1, passwordless |
| SSH + SCP | `ssh -m hmac-sha2-256 qnxuser@192.168.10.5` |
| UART | `uart status` → `physical`, `/dev/ser1` @115200; 21 bytes on the wire before its own `printf` |
| GPIO markers | `REAL PHYSICAL`; fsel 0→1 on all 3 pins; `confirmed=YES`; **LED observed blinking on header pin 11**; 200.9 ms vs 200 ms nominal |
| CAN driver build | 148,840 bytes from commit `0fd11af`, committed to the repo |
| CAN driver install | Steps 1–3 confirmed on the board; binary installed, `spi.conf` retuned |
| Host test suite | 39/39 passing |

**Code complete, not verified on hardware:**

- Workload execution on the board (BRAKE/ADAS/DIAG scheduling)
- Jitter and CPU-utilisation figures in the analyzer output
- Context switches via `tracelogger` → `/tmp/schedulix.kev`

**Not done:**

- `/dev/can0` appearing on the target — blocked at installer step 4
- Real-bus CAN frames driving workloads
- Qt frontend data path — the GUI still runs on `MockProvider`

### Verify the unverified items first, on a working board

```sh
# context switches - 5 minutes, settles a long-standing doubt
tracelogger -T /tmp/schedulix.kev &
sleep 3
ls -l /tmp/schedulix.kev

# workloads actually running
/tmp/schedulix_can workload start
/tmp/schedulix_can status
```

---

## Related documents

| Document | Contents |
| --- | --- |
| [PROBLEM_STATEMENT_COMPLIANCE.md](PROBLEM_STATEMENT_COMPLIANCE.md) | Requirement-by-requirement scorecard |
| [SERIAL_CONSOLE_RUNBOOK.md](SERIAL_CONSOLE_RUNBOOK.md) | Serial console in depth |
| [RUNBOOK.md](RUNBOOK.md) | Day-to-day operating commands |
| [INCIDENT_SPI_DRIVER.md](INCIDENT_SPI_DRIVER.md) | The SPI bring-up incident and how it was debugged |
| [architecture.md](architecture.md) | Component design |
| [timing-model.md](timing-model.md) | Delay attribution mathematics |
| [gpio.md](gpio.md) / [uart.md](uart.md) | Per-interface detail |
| [../CURRENT_STATE.md](../CURRENT_STATE.md) | Live project state |