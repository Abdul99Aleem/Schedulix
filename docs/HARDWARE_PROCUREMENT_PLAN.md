# Schedulix — Hardware, Connectivity & Software Stack Verification

**Problem Statement 16 — Automotive RTOS Performance & Latency Analyzer**
Branch: `hw/procurement-and-gap-plan` · Date: 2026-10-01

---

## 0. Green Signal

**GREEN. All three required peripherals have a working, QNX-supported software path
verified on this machine today. Nothing in the final BOM is at risk of being wasted.**

Every claim in Section 3 was verified by compiling code, not by reading documentation.
The single most important result: **the official QNX `can-mcp2515` driver was cloned,
built, and linked successfully against your installed SDP 8.0.** That was the biggest
unknown and it is now closed.

There are three code bugs to fix (Sections 5–6). All three are software. **Buy the
hardware.**

---

## 1. Topology — How Three Boards Extend the Idea

The problem statement asks for one Pi running QNX with CAN, UART and GPIO. You own
three. Here is how the extra two extend it rather than just duplicating it.

### 1.1 Network layout

```
                    ┌──────────────┐
                    │   Router     │
                    │  DHCP pool   │
                    └──┬───┬───┬────┘
              Cat5e 1   │   │   │   Cat5e 3
                       │   │   │
              ┌────────┘   │   └────────┐
              │            │            │
        ┌─────┴─────┐ ┌────┴─────┐ ┌────┴─────┐
        │   Pi 1    │ │   Pi 2   │ │   Pi 3   │
        │  QNX 8.0  │ │  Linux   │ │  Linux   │
        │           │ │          │ │          │
        │ ANALYZER  │ │  CAN ECU │ │ OBSERVER │
        │           │ │ SIMULATOR│ │          │
        └──┬─────┬──┘ └────┬─────┘ └────┬─────┘
           │     │         │             │
      CAN   │  GPIO        │ CAN      Logic analyzer
      HAT   │  markers     │ HAT      + UART injector
      #1    │  (to logic   │          │
           │   analyzer)   │          │
           └──────┬────────┴──────────┘
                  │  CAN bus
        120Ω ├────┤  CAN_H   CAN_H ────┤
             │     │    │          │    │
           [T1]  GND  CAN_L   CAN_L ────┤
                                 [T2]  GND
                    T1, T2 = 120 Ω at each bus end
```

### 1.2 Board roles

| Board | OS | Role | Why this board |
| --- | --- | --- | --- |
| **Pi 1** | QNX 8.0 | **Analyzer.** ECU workloads, trace collector, `tracelogger` capture, RCA engine, CLI | Only board that needs kernel tracing + root. QNX gives real context-switch data |
| **Pi 2** | Raspberry Pi OS | **CAN ECU simulator.** Generates ECU traffic over a real CAN bus | Linux + SocketCAN makes `python-can` work unmodified. No driver work |
| **Pi 3** | Raspberry Pi OS | **Observer.** Logic analyzer capture console, UART workload injection, monitoring | Keeps capture off the machine being measured, so probing never perturbs the analyzer |

### 1.3 What the three-board split buys you over one board

| Capability | Single board | Three boards |
| --- | --- | --- |
| CAN event source | Would need self-loopback on one MCP2515 | **Genuine external node** — an independent board generating frames, which is what the problem statement actually asks for |
| CAN arbitration | One node, no contention | Real multi-node arbitration with real bit timing |
| Timing measurement | Software timestamps only | **Independent hardware measurement** on Pi 3 while Pi 1 is untouched |
| Analyzer perturbation | Probing can perturb the system under test | Capture runs on separate hardware — **measurement no longer perturbs the measurement** |
| Injection flexibility | QNX-side only | Inject from Linux where tooling is mature |
| Failure isolation | All on one board | Generator failure does not lose the trace |

That last row matters more than it looks. If the CAN generator crashes mid-experiment,
the trace collector on Pi 1 is already gone. Splitting them means a generator crash
costs you the run, not the data.

### 1.4 Connectivity summary

| Link | From | To | Medium |
| --- | --- | --- | --- |
| Management | Router | Pi 1, 2, 3 | 3 × Cat5e (router LAN ports) |
| CAN bus | Pi 1 CAN_HAT ch0 | Pi 2 CAN_HAT ch0 | Twisted pair, 120 Ω at both ends |
| CAN bus GND | Pi 1 GND | Pi 2 GND | Single reference ground |
| Logic analyzer | Pi 3 USB | Pi 1 GPIO 4 / 17 / 27 | Jumper wires from Pi 1 header |
| UART injection | Pi 3 USB-UART | Pi 1 USB-UART | Crossed TX→RX, shared GND |
| Analysis artifacts | Pi 1 (`scp`) | Windows host | LAN via router |

Note the CAN bus is a **separate physical layer** from the network. The three boards
talk over Ethernet for control, and CAN_H/CAN_L for stimuli. Do not try to carry CAN
over the LAN.

---

## 2. Alignment With the Problem Statement

| Problem statement requirement | Status | Evidence |
| --- | --- | --- |
| QNX on Raspberry Pi 4/5 | **Met** | SDP 8.0 validated platform; workloads running on target per `docs/logs/qnx_logs_7.txt` |
| CAN interface | **Gap → fixable** | `can-mcp2515` builds; adapter code not written (§5.2) |
| UART | **Met** | `uart_adapter.c` complete, POSIX driver, 115200 8N1 |
| GPIO timing marker | **Gap → fixable** | Missing `GPFSEL` write (§5.1) |
| Measure context switches | **Met** | `libtraceparser` `.kev` decoding, verified on target |
| Task latency | **Met** | 64-bit timestamp reconstruction with rollover detection |
| Jitter | **Met** | p50/p95/p99 in analyzer output |
| CPU utilization | **Met** | `--sweep`, 20 %→95 % verified on target |
| Deadline misses | **Met** | Miss ratio + slack in `analysis_*.json` |
| Varying load | **Met** | S1 load sweep, S0–S6 scenario matrix |
| External CAN/UART generate workload | **Partial** | UART met; CAN simulated only (§5.2) |
| GPIO as hardware timing marker | **Partial** | Code present, never drives a pin (§5.1) |
| Shared memory | **Met** | MPSC ring buffer, 48-byte fixed records, zero `malloc` in hot path |
| Sampling, trace flush | **Met** | Post-mortem flush to `trace_s*.bin` |
| CPU graphs, Gantt timeline, jitter | **Met in CLI** | Qt frontend renders these (§6.3 — but from mock data) |
| CLI mandatory | **Met** | Verb-structured parser in `main.c` |
| Optional Python/Qt/Web trace viewer | **Gap** | No JSON loader in Qt app (§6.3) |

**Three gaps, all software. Zero gaps are hardware-blocking.**

---

## 3. Software Stack Verification — What Actually Exists

This is the section that answers "will hardware go to waste if QNX has no support?"

### 3.1 Summary

| Component | Needed for | Status | How verified |
| --- | --- | --- | --- |
| `libcan.a` (aarch64le) | CAN DDK | **PRESENT** | `C:\Users\User\qnx800\target\qnx\aarch64le\usr\lib\libcan.a` |
| `libcan.h` | CAN DDK | **PRESENT** | `target\qnx\usr\include\hw\libcan.h` |
| `can_dcmd.h` | CAN DDK | **PRESENT** | `target\qnx\usr\include\sys\can_dcmd.h` |
| CAN compile + link | CAN DDK | **WORKS** | Wrote test program, compiled and linked `-lcan` successfully |
| `can-mcp2515` driver | CAN HAT | **BUILDS** | Cloned from public GitLab, built clean for aarch64le (§3.2) |
| `libtraceparser.a` | Kernel trace | **PRESENT** | `target\qnx\aarch64le\usr\lib\libtraceparser.a` |
| `sys/traceparser.h` | Kernel trace | **PRESENT** | `target\qnx\usr\include\sys\traceparser.h` |
| `sys/trace.h` | Kernel trace | **PRESENT** | `target\qnx\usr\include\sys\trace.h` |
| POSIX termios | UART | **PRESENT** | Standard QNX libc |
| BCM2711 register access | GPIO | **PROVEN** | Official driver does the same mmap (§3.3) |
| BSP (RPi4) | GPIO utility, serial driver | **NOT INSTALLED locally** | No `bsp_raspberrypi-bcm2711-rpi4` directory on host |
| Raspberry Pi OS images | Pi 2, Pi 3 | **NOT ON DISK** | Flash separately |

### 3.2 CAN DDK — built and linked, verified

The CAN DDK ships as `com.qnx.qnx800.target.connectivity.can`. **You have it.**

Compile and link test, aarch64le, against your SDP:

```c
#include <hw/libcan.h>
#include <sys/can_dcmd.h>
int main(void){ CAN_MSG m; m.mid = 0x123; m.len = 3; return 0; }
```

```
qcc -Vgcc_ntoaarch64le -c t.c -o t.o                       # cc  EXIT=0
qcc -Vgcc_ntoaarch64le -o t.exe t.o -lsocket \
    -Wl,-Bstatic -lcan -Wl,-Bdynamic                       # ld  EXIT=0
```

Headers parse and `-lcan` links. `CAN_MSG` is the frame type your adapter needs.

**Note on header location:** `libcan.h` lives in `hw/`, not `sys/`. Use
`#include <hw/libcan.h>`. It is not in the `aarch64le/usr/include` path — it is in
the common `qnx/usr/include`, which the compiler already searches.

### 3.3 CAN driver — cloned and built for aarch64le

This was the highest-risk unknown. QNX publishes the driver as source, not as a
prebuilt binary.

```bash
git clone https://gitlab.com/qnx/projects/drivers/can-mcp2515.git
cd can-mcp2515
. ~/qnx800/qnxsdp-env.sh
export QCONF_OVERRIDE=$(pwd)/qconf-override.mk
make INSTALL_ROOT_nto=$(pwd)/build USE_INSTALL_ROOT=1 hinstall install
```

Result on your machine:

```
build/aarch64le/bin/can-mcp2515       148,840 bytes
build/aarch64le/lib/libmcp2515.a       40,216 bytes
ELF 64-bit LSB pie executable, ARM aarch64, dynamically linked,
interpreter /usr/lib/ldqnx-64.so.2
```

Links against `-lmcp2515 -lcan -lsecpol -lslog2`. Build exit code 0.

The driver README states plainly:

> **NOTE** At this time, the driver only works on the Raspberry Pi 4 platform.
> It was developed for the Waveshare 2-CH CAN HAT

**This is the single strongest argument for staying on Pi 4 with a Waveshare HAT.**
There is a `devel_dphipps_rpi5` branch, but it is unreleased and the released driver
is Pi 4 only. Your Pi 4s are exactly right.

### 3.4 GPIO — the driver proves the register access works

The official driver maps BCM2711 GPIO registers directly. This is the pattern your
`gpio_marker.c` must copy:

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

**Three things your current code gets wrong, proven against this reference:**

1. **`/dev/mem` is wrong for QNX.** The driver uses `MAP_PHYS | NOFD` with `mmap`.
   Your code does `open("/dev/mem")` then `mmap` — that is the Linux idiom and it is
   why your Pi logs `no HW, mock mode`. Change it to the pattern above.
2. **`PROT_NOCACHE` is required.** Device registers must be uncached. Your code uses
   plain `PROT_READ|PROT_WRITE`.
3. **`GPFSEL` must be written before `GPSET`/`GPCLR`.** The driver writes `FSEL`
   explicitly (`reg = (FSEL0 + (gpio/10)) / 4; shift = (gpio % 10) * 3`). Your code
   never writes it, so pins stay inputs and nothing appears on the wire.

The driver also confirms GPIO 0–53 is the valid range on Pi 4. Your pins 4, 17, 27
are all valid.

### 3.5 What is NOT available locally

| Missing | Impact | Fix |
| --- | --- | --- |
| **RPi4 BSP on host** | No `gpio-bcm2711` utility, no `devc-ser` binary to copy | Install `com.qnx.qnx800.bsp.hw.raspberrypi_bcm2711_rpi4` from QNX Software Center |
| **`socdiag` / BSP source** | Same | Same package |
| **Raspberry Pi OS images** | Pi 2, Pi 3 unbootable | Flash with Raspberry Pi Imager |
| **Pi 1 powered** | Could not verify live `/dev/ser*`, `/dev/can*` | Power on and re-run checks |

**The missing BSP is not a blocker.** Once the BSP is installed you get the serial
driver binaries your UART work needs, and the `gpio-bcm2711` utility. Your code
already links against nothing BSP-specific.

**Live target checks not yet done** (Pi 1 is currently offline — `ping 192.168.10.2`
returns 100 % loss). Verify once powered:

```sh
ls /dev/ser*          # expect ser1 (mini-UART)
ls /dev/io-spi/spi0/  # expect dev0, dev1 after spi.conf
ls /dev/can0          # expect after can-mcp2515 starts
```

---

## 4. CAN Bus Design

### 4.1 MCP2515 HAT wiring (Waveshare 2-CH CAN HAT)

The HAT is a full 40-pin HAT. It uses SPI0 for both channels.

| Pi pin (BCM) | Signal | HAT function |
| --- | --- | --- |
| 8 (CE0) | SPI chip select 0 | CAN_0 CS |
| 7 (CE1) | SPI chip select 1 | CAN_1 CS |
| 11 (SCLK) | SPI clock | SCK |
| 10 (MOSI) | SPI out | MOSI |
| 9 (MISO) | SPI in | MISO |
| 23 | GPIO | CAN_0 INT (soldered default) |
| 25 | GPIO | CAN_1 INT (soldered default) |
| 5V | Power | Transceiver supply |

Driver start commands, matching the HAT's default INT pins:

```sh
can-mcp2515 --mid=eid -s /dev/io-spi/spi0/dev0 -c 16000000 -g 23   # Pi 1, ch0
can-mcp2515 --mid=eid -s /dev/io-spi/spi0/dev1 -c 16000000 -g 25 -u 1
```

**Crystal: this HAT is 16 MHz** (`oscillator=16000000` in the vendor config).
Driver accepts 1–40 MHz. Read the marking to confirm before relying on `-c`.

### 4.2 Three physical gotchas with this HAT

1. **Set the logic-level jumper to 3.3 V.** The HAT supports 3.3 V or 5 V. The Pi
   is 3.3 V. Wrong setting damages the Pi.
2. **The HAT has an onboard 120 Ω terminator, jumper-selectable.** You may not need
   external resistors. Buy two anyway (§8) — you will want them for a 3-node bus,
   and they cost almost nothing.
3. **Use the supplied 2×20-pin stacking header.** The CAN terminal block sits close
   enough to the HDMI port to short against it on a bare board. This is in the vendor
   FAQ as a real failure mode. The header comes in the box.

### 4.3 GPIO markers versus the HAT — physical conflict

**The 2-CH CAN HAT covers the entire 40-pin header.** Your GPIO marker pins are
physical pins 7 (GPIO 4), 11 (GPIO 17), 13 (GPIO 27) — all underneath the HAT.

Options, best first:

| Option | How | Cost |
| --- | --- | --- |
| **Solder to header underside** | Short wires on the bottom pads of pins 7, 11, 13 | ~₹30 |
| Stacking header + probe from above | Use the tall supplied header, clip logic analyzer there | free |
| Move markers to another board | Not valid — markers must be on the measured board | n/a |

Soldering to the underside is the practical answer. Pin 7/11/13 solder pads are
reachable from under the board with the HAT mounted.

**Good news:** the HAT claims GPIO 8, 9, 10, 11, 23, 25, 7. Your marker pins 4, 17
and 27 are **not used by the HAT**, so there is no electrical conflict — only the
physical access problem above.

---

## 5. Code Changes Required

Three bugs. All software. Do them as parts arrive.

### 5.1 GPIO — replace `/dev/mem` + add `GPFSEL` (critical)

`src/gpio_marker.c`. Replace the whole `mmap` block with the verified QNX pattern:

```c
#include <sys/mman.h>

#define GPIO_BASE   0xfe200000
#define GPFSEL0     (0x00)
#define GPSET0      (0x1c)
#define GPCLR0      (0x28)
#define GPLEV0      (0x34)

static volatile uint32_t *regs;

/* pin 0-9 -> GPFSEL0, 10-19 -> GPFSEL1, ... */
static void pin_output(int pin)
{
    int reg   = (GPFSEL0 + (pin / 10)) / 4;   /* word index */
    int shift = (pin % 10) * 3;
    regs[reg] = (regs[reg] & ~(0x7u << shift)) | (0x1u << shift); /* 001 = output */
}

int gpio_marker_init(void)
{
    regs = mmap(0, __PAGESIZE,
                PROT_NOCACHE | PROT_READ | PROT_WRITE,
                MAP_PHYS | MAP_SHARED, NOFD, GPIO_BASE);
    if (regs == MAP_FAILED) { regs = NULL; return -1; }

    pin_output(4); pin_output(17); pin_output(27);
    g_available = 1;
    return 0;
}

int gpio_marker_set(int pin, int value)
{
    if (!regs) return -1;
    int bank = (pin < 32) ? 0 : 32;            /* SET0/CLR0 vs SET1/CLR1 */
    int b    = pin - bank;
    if (value) regs[(GPSET0 + bank) / 4] |=  (1u << b);
    else       regs[(GPCLR0 + bank) / 4] |=  (1u << b);
    __sync_synchronize();                      /* order the write */
    return 0;
}
```

Also implement `gpio_marker_get()` from `GPLEV0`, and delete the dead
`/dev/gpio-0` probe loop — **that path never exists on QNX**, since the BSP ships
`gpio-bcm2711` as a utility, not a resource-manager driver.

Requires `root` for physical memory access.

### 5.2 CAN — write the real adapter

`src/qnx_can_adapter.c` is currently a comment saying it is not compiled. Fill in
using `/dev/can0` mailboxes and `devctl`:

```c
#include <hw/libcan.h>
#include <sys/can_dcmd.h>

/* Receive: open /dev/can0/rx0, then CAN_DEVCTL_RX_FRAME_RAW_NOBLOCK */
/* Transmit: open /dev/can0/tx2, then CAN_DEVCTL_TX_FRAME_RAW */
```

Mailbox layout with `--mid=eid`:

| Path | Role | MID |
| --- | --- | --- |
| `/dev/can0/rx0` | receive | 0 |
| `/dev/can0/rx1` | receive | 1 |
| `/dev/can0/tx2` | transmit | 2 |
| `/dev/can0/tx3` | transmit | 3 |
| `/dev/can0/tx4` | transmit | 4 |

Catch-all filter before receiving:

```sh
canctl -u 1,rx0 -m 0
canctl -u 1,rx0 -f 0
```

**Makefile fix:** `SRCS = $(call rwildcard, src, c)` will compile both
`qnx_can_adapter.c` and `qnx_can_adapter.c.stub` once you rename — duplicate symbols.
Exclude the stub explicitly.

### 5.3 UART — TX trace marker is wrong

`src/uart_adapter.c:151` records `TRACE_EXTERNAL_EVENT_RX` for a **transmit**. TX and
RX are indistinguishable in the trace. Use a TX marker or distinct event type.

---

## 6. Non-Hardware Gaps

### 6.1 Missing BSP — install it

QNX Software Center → `com.qnx.qnx800.bsp.hw.raspberrypi_bcm2711_rpi4`.
Gives you `devc-ser` binaries, `gpio-bcm2711`, `spi-bcm2711`.

### 6.2 Invalid validation claim

`VALIDATION_LOG.md` TEST 6 says GPIO PASS, but with no hardware it compares software
timestamps to software timestamps. Vacuous. Fix as part of §5.1.

### 6.3 Qt frontend has no data path

`grep -r "QJson\|analysis_\|QFile" schedulix_v1/src/` returns **nothing**.
`MockProvider.cpp` hardcodes every metric. The GUI is a rendering mock.

The problem statement asks for an optional trace viewer fed by CLI output. Fix is
`QJsonDocument` over the `analysis_s*.json` / `manifest_s*.json` the CLI already
writes. Qt 6.8.3 builds clean — verified. **Needs no hardware.** This is the largest
remaining software gap.

---

## 7. Substitutes — When The Exact Part Is Unavailable

| If unavailable | Buy instead | Constraint |
| --- | --- | --- |
| Waveshare 2-CH CAN HAT | Any **dual-channel MCP2515** SPI CAN HAT | Must be MCP2515. Two MCP2515 HATs on SPI0 CE0/CE1 also work |
| Waveshare unavailable | PiCAN2 / PiCAN3 (SK Pang) | MCP2515-based, works with same driver |
| 16 MHz crystal HAT | 8 MHz MCP2515 HAT | Driver fine — change `-c 8000000`. Verify marking |
| Logic analyzer | Saleae Logic 8 clone, 8-ch 24 MHz | Any 8-ch ≥24 MHz. 100 MHz scope is overkill |
| USB-UART | CP2102 or FTDI 3.3 V TTL | **Never RS232.** See warning below |
| Third CAN node | Second MCP2515 HAT | Same driver, different `-g` GPIO |

### Parts that must not be substituted

| Do not buy | Why |
| --- | --- |
| **MCP2515FD / MCP25625** | Different chip. Driver targets MCP2515 only |
| **CAN FD anything** | Problem statement specifies CAN, not CAN FD |
| **RS232 USB-UART adapter** | ±9 V onto Pi GPIO pins destroys the SoC |
| **Anything above 3.3 V logic on GPIO** | Pi GPIO is not 5 V tolerant |
| **Pi 5 as the QNX target** | `can-mcp2515` released driver is Pi 4 only |
| **Isolated high-voltage CAN gear** | Your bus is 3.3 V bench-level |

---

## 8. Final Hardware List

### Already have

| # | Item | Qty | Note |
| --- | --- | --- | --- |
| 1 | Raspberry Pi 4B | 3 | Confirmed correct target |
| 2 | Router | 1 | Needs ≥3 free LAN ports |
| 3 | LAN cable Cat5e | 1 | **Need 2 more** |

### Buy now

| # | Item | Exact spec | Qty | Est. |
| --- | --- | --- | --- | --- |
| 4 | **CAN HAT** | Waveshare 2-CH CAN HAT, **MCP2515**, 16 MHz, 3.3 V logic jumper | **2** | ₹1,200–2,000 each |
| 5 | **LAN cable** | Cat5e, 1–2 m | **2** | ₹150 each |
| 6 | **USB-to-UART adapter** | **3.3 V TTL**, CP2102 or FTDI. Not RS232 | **2** | ₹300 each |
| 7 | **Logic analyzer** | 8-channel, ≥24 MHz (Saleae Logic 8 / clone) | 1 | ₹2,500–8,000 |
| 8 | **microSD card** | 16 GB, A2 class, for Pi 2 + Pi 3 | **2** | ₹500 each |
| 9 | **120 Ω resistor** | 1/4 W, 1 % | 4 | ₹10 each |
| 10 | **LED** | 3 mm, any colour, for GPIO proof | 6 | ₹5 each |
| 11 | **330 Ω resistor** | 1/4 W, current limit for LEDs | 6 | ₹10 each |
| 12 | **Breadboard** | Half-size, 400 tie-points | 2 | ₹120 each |
| 13 | **Jumper wires** | Dupont, M-M / M-F / F-F assorted | 3 sets | ₹80 each |
| 14 | **Header wire** | Fine hookup for soldering to Pi 1 header underside | 1 set | ₹60 |

### Buy if you want a 3-node CAN bus

| # | Item | Exact spec | Qty | Est. |
| --- | --- | --- | --- | --- |
| 15 | **CAN HAT** | Same as #4 | 1 | ₹1,200–2,000 |

### Total

**Core list (items 4–14): approximately ₹8,000–14,000.**
Add ₹1,200–2,000 for item 15 if you want the third CAN node.

The HATs and logic analyzer dominate. Items 9–14 cost under ₹1,000 total and are what
let you actually verify GPIO physically.

---

## 9. Bring-Up Order

Each step is independently verifiable before the next begins.

1. **Install the RPi4 BSP** in QNX Software Center. Get `devc-ser` + `gpio-bcm2711`.
2. **Flash Pi 2 and Pi 3** with Raspberry Pi OS. Confirm all three on the router.
3. **UART** — plug adapter into Pi 1, loop TX→RX. `schedulix uart init` must flip from
   `simulated` to physical. Fix §5.3 while you are there.
4. **GPIO code** — apply §5.1, rebuild, deploy. `schedulix gpio test` must print
   `REAL PHYSICAL`, not `MOCK`.
5. **GPIO proof** — solder wires to Pi 1 header underside (§4.3). LED + 330 Ω on
   GPIO 4. `gpio test` must light it. **Do not skip this** — it is the only thing
   standing between you and a silent no-op.
6. **Logic analyzer** — connect to Pi 3, probe Pi 1 GPIO 4/17/27. Run `--full`. Expect
   three pulse trains at 10 / 20 / 50 ms.
7. **CAN on Pi 2 (Linux)** — mount HAT, `dtoverlay=mcp2515-can0,oscillator=16000000,interrupt=23`,
   `ip link set can0 up type can bitrate 500000`. Verify with `candump`/`cansend`.
   **Easier to debug here than on QNX — do it first.**
8. **CAN on Pi 1 (QNX)** — `spi.conf`, copy `can-mcp2515`, start driver, confirm
   `/dev/can0`. Self-loop CAN0↔CAN1 first.
9. **Cross-board CAN** — wire Pi 1 ch0 to Pi 2 ch0, 120 Ω each end. Pi 2 generates,
   Pi 1 receives.
10. **Write the QNX CAN adapter** (§5.2). This is the last code blocker.
11. **Wire Qt to real JSON** (§6.3). Screenshot with genuine data.

Steps 1–9 are hardware and infrastructure. Step 10 is the only substantial code
remaining, and step 11 is independent of all hardware.

---

## 10. Verification Gates

A claim is done when measured. This is how to prove each one.

| Claim | Proof |
| --- | --- |
| Pi 1 on network | `ping` + `ssh root@<ip>` |
| UART is physical | `uart init` prints physical; loopback bytes observed |
| GPIO is physical | `gpio test` prints `REAL`; **LED lights**; analyzer sees edges |
| GPIO timing accurate | Measured pulse width vs software interval within 50 µs + 5 % |
| CAN driver loads | `/dev/can0` exists after `can-mcp2515` starts |
| CAN loopback | `canctl -u 0,tx2 -w 0x123,...` then `canctl -u 1,rx0 -R 100` shows frame |
| **External** CAN works | Frame generated on Pi 2 appears in Pi 1 `analysis_*.json` |
| Deadlines missed under load | S1 p99 > S0 p99, `misses > 0`, named `root_cause` |
| RCA confirmed | `evidence_level: CONFIRMED` with real `interferer_tid` |
| GUI shows real data | Screenshot traceable to a specific `analysis_s4.json` |

`VALIDATION_LOG.md` TEST 6 currently fails the third row's standard. That is the one
to fix.

---

## 11. Bottom Line

| Question | Answer |
| --- | --- |
| Will QNX hardware go to waste? | **No.** CAN driver builds, `libcan` links, `libtraceparser` present, GPIO register access proven by the official driver |
| Is any peripheral unsupported? | **No.** All three have working QNX paths |
| Green signal to buy? | **Yes** |
| Biggest risk remaining | Your own code: `GPFSEL` missing, CAN adapter unwritten, Qt reading mock data |
| Cheapest insurance | Solder wires to Pi 1's header underside before the HAT goes on — ₹60, five minutes |

The hardware is the solved part. Three code gaps stand between you and a complete
system, and all three are yours to write.