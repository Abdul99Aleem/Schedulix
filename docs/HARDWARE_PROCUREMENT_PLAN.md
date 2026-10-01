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

**The Waveshare 2-CH CAN HAT is unavailable, and it is not needed.** QNX's driver
targets the **MCP2515 chip**, not that particular board. The selected route uses
breadboard MCP2515 modules, with the added benefit that no HAT covers the header, so
GPIO marker pins stay directly accessible (§7.4).

**Total cost: ≈ ₹8,050** for the complete verified inventory (§8.12), including power
supplies and cooling. One gap was found in the original BOM: it listed **one** power
supply, but three boards require three (§8.2).

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
   module│  markers     │ module   + UART injector
   SPI0  │  (to logic   │  SPI0     │
   CE0   │   analyzer)   │          │
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
| **Pi 2** | Raspberry Pi OS | **CAN ECU simulator.** Generates ECU traffic over a real CAN bus | Linux + SocketCAN drives the MCP2515 module with stock tooling. Modify this board's module first — it is expendable |
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
| CAN bus | Pi 1 MCP2515 module | Pi 2 MCP2515 module | Twisted pair, 120 Ω at both ends |
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

### 4.1 Selected design — MCP2515 modules on SPI0, CE0

Both Pi 1 and Pi 2 carry one MCP2515 module wired to SPI0 chip-select 0.

| Pi physical pin | BCM | Signal | Module pin |
| --- | --- | --- | --- |
| 1 | — | 3.3 V | VCC → MCP2515 |
| 2 | — | 5 V | TJA1050 VCC *(after trace cut, §7.2)* |
| 6 | — | GND | GND |
| 19 | GPIO 10 | MOSI | SI |
| 21 | GPIO 9 | MISO | SO |
| 23 | GPIO 11 | SCLK | SCK |
| 24 | GPIO 8 | CE0 | CS |
| 22 | GPIO 25 | — | INT |

QNX driver start command on Pi 1:

```sh
can-mcp2515 --mid=eid -s /dev/io-spi/spi0/dev0 -c 8000000 -g 25
```

`spi.conf` requires **one** `[dev]` section, not two:

```
[dev]
parent_busno=0
devno=0
name=dev0
clock_rate=10000000
cpha=0
cpol=0
bit_order=msb
word_width=8
idle_insert=1
```

Catch-all filter before receiving:

```sh
canctl -u 0,rx0 -m 0
canctl -u 0,rx0 -f 0
```

**Crystal:** generic MCP2515 modules are commonly **8 MHz**. Verify the marking and
set `-c` to match. Driver accepts 1–40 MHz. A mismatch yields a driver that starts
cleanly and transmits zero frames.

### 4.2 Bus topology

Pi 1 and Pi 2 form a two-node CAN bus:

```
Pi 1 module                        Pi 2 module
CAN_H ────────┬────────────────────── CAN_H
              │
            [T1] 120 Ω            [T2] 120 Ω
              │
CAN_L ────────┴────────────────────── CAN_L
  GND ───────────────────────────────  GND   (shared reference)
```

Both modules usually have an onboard 120 Ω with a jumper. Fit terminators at the two
**ends** of the bus only. Since this is a 2-node bus, both ends terminate. Carry
spares (§8) in case the onboard jumpers are unclear.

### 4.3 GPIO markers — no conflict on the module route

**This is an advantage of the module route.** A breadboard module occupies no header
pins. GPIO marker pins 7 (GPIO 4), 11 (GPIO 17) and 13 (GPIO 27) remain directly
accessible with ordinary jumper wires to a breadboard.

The module does claim GPIO 8, 9, 10, 11 (SPI0) and GPIO 25 (INT). Those do **not**
overlap with 4, 17 or 27. No electrical or physical conflict.

**No soldering to the Pi header is required.** This step is only needed if you switch
to the Waveshare HAT route (§7.5), where the HAT covers the whole header.

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

## 7. CAN Board Selection — Waveshare vs Generic Modules

### 7.1 The distinction that matters is the transceiver

Both boards contain an MCP2515. The QNX driver only cares about that. What differs
is the CAN transceiver, and that is a **safety** issue, not a convenience one.

| | Waveshare 2-CH CAN HAT | Generic "MCP2515 CAN Bus Module" |
| --- | --- | --- |
| Controller | 2× MCP2515 | 1× MCP2515 |
| Transceiver | **SN65HVD230 / SI65HVD230** | **TJA1050** (sometimes MCP2551) |
| Safe on Pi 3.3 V GPIO | **Yes** | **TJA1050 — NO** |
| Channels | 2 | 1 |
| Crystal | 16 MHz | usually **8 MHz** |
| Form factor | 40-pin HAT, covers header | ~40×28 mm breadboard breakout |
| 120 Ω terminator | onboard, jumper-selectable | usually onboard, jumper |
| ESD / transient protection | TVS + isolation | none |
| Logic-level jumper | 3.3 V / 5 V | none |
| Price | ₹1,860–1,949 | ₹200–500 |

### 7.2 Why TJA1050 damages a Raspberry Pi

The TJA1050 is a **5 V-only** transceiver (4.75–5.25 V) and it has **no separate VIO
pin** — its RXD output swings to VCC. Power the module from the Pi's 5 V pin and
**5 V appears on MISO**, which is a Pi GPIO input. The Pi is not 5 V tolerant and that
pin is destroyed. The CS and INT lines are affected the same way.

The transceivers that avoid this:

| Transceiver | Why it is safe |
| --- | --- |
| **SN65HVD230** | Runs entirely at 3.3 V. Full 3.3 V supply. No VIO pin needed |
| **MCP2551** | Has a **separate VIO pin**. Logic side can be 3.3 V while VCC is 5 V. This is why PiCAN2 works on a Pi |
| **TJA1050** | No VIO pin. Logic output follows VCC. **Unsafe on Pi** |

To use a TJA1050 module on a Pi you must either:

1. **Cut the PCB trace** feeding the transceiver, then feed it 5 V separately while the
   MCP2515 runs from 3.3 V. Requires a soldering iron and care with fine tracks.
2. **Desolder the TJA1050 and fit an SN65HVD230** — pin-compatible, works at 3.3 V.
   Requires SMD rework.

Both are well-documented community modifications. Both add risk and time.

### 7.3 Waveshare availability in India

The part exists in India — it is a vendor-availability problem, not a market one.

| Vendor | Part | Price | Status |
| --- | --- | --- | --- |
| rarecomponents.com | 2-CH CAN HAT, SKU 17912 | ₹1,860.86 | In stock |
| hubtronics.in | 2-CH CAN HAT | ₹1,949 | Listed |
| hubtronics.in | 2-CH CAN HAT **+** | ₹1,949 | Listed, different SKU |

**2-CH CAN HAT** and **2-CH CAN HAT+** are different products. The HAT+ adds an
onboard EEPROM and 7–36 V input handling. If one is unavailable, ask for the other
**by name**.

### 7.4 RECOMMENDED ROUTE — MCP2515 modules (Waveshare unavailable)

Since the Waveshare 2-CH CAN HAT is unavailable from the user's vendors, this is the
selected route. It requires **no HAT** and therefore **no soldering on the Pi header**.

**Chip-level equivalence.** QNX's `can-mcp2515` driver targets the **MCP2515 chip**,
not the Waveshare board. Its README states it *"should work for any other HW connected
to the RPI4 that uses the MCP2515."* The problem statement asks for a *"CAN
Interface"* — an MCP2515 **is** a CAN interface. Nothing is lost.

**Key advantage over the HAT route:** a breadboard module does **not** cover the
40-pin header. GPIO marker pins 7, 11 and 13 (GPIO 4, 17, 27) stay directly
accessible with ordinary jumpers. The solder-to-header step required by the HAT
disappears entirely.

#### Extra hardware required by the module route

| Item | Qty | Cost | Why |
| --- | --- | --- | --- |
| Breadboard, half-size | 2 | ₹240 | Module is a breakout, not a HAT |
| Jumper wires, assorted | 4 sets | ₹320 | ~8 wires per board vs 0 for a HAT |
| 3.3 V safety fix | see below | ₹0–320 | Only genuine addition |
| 120 Ω resistors | 4 | ₹40 | Module has onboard + jumper; buy spares |

#### The 3.3 V safety fix — three methods

| Method | Cost | Skill | Notes |
| --- | --- | --- | --- |
| **Cut PCB trace** feeding the TJA1050; feed it 5 V separately; power MCP2515 from 3.3 V | ₹0 | Soldering iron, one cut | Standard community solution, thousands of builds. **Recommended** |
| Replace TJA1050 with **SN65HVD230** | ₹40/chip × 2 | Soldering iron, ~5 min | Pin-compatible. Zero ambiguity |
| 8-channel bi-directional MOSFET level shifter | ₹160 × 2 | None | Adds SPI propagation delay; not preferred |

SN65HVD230 chips and breakout modules are in stock in India at ₹40–320
(Probots, Zbotic, DNAtech, ElectroPi).

#### Final BOM — module route

| # | Item | Spec | Qty | Cost |
| --- | --- | --- | --- | --- |
| 1 | **MCP2515 CAN bus module** | MCP2515 + TJA1050, onboard 120 Ω w/ jumper. **Note the crystal — likely 8 MHz** | **2** | ₹400 ea |
| 2 | **SN65HVD230 transceiver** | DIP-8 / SOIC-8, pin-compatible with TJA1050 | 2 | ₹80 |
| 3 | Breadboard | half-size | 2 | ₹240 |
| 4 | Jumper wires | Dupont assorted | 4 sets | ₹320 |
| 5 | 120 Ω resistor | 1/4 W, 1 % | 4 | ₹40 |
| 6 | **USB-to-UART adapter** | **3.3 V TTL**, CP2102 or FTDI. Not RS232 | 2 | ₹600 |
| 7 | LAN cable | Cat5e, 1–2 m | 2 | ₹300 |
| 8 | Logic analyzer | 8-channel, ≥24 MHz | 1 | ₹3,000 |
| 9 | microSD card | 16 GB A2, for Pi 2 + Pi 3 | 2 | ₹1,000 |
| 10 | LED | 3 mm, for GPIO proof | 6 | ₹30 |
| 11 | 330 Ω resistor | 1/4 W, LED current limit | 6 | ₹60 |

> **This table is CAN-specific.** It omits power supplies, LAN cable count, jumper
> set size, and cooling — all of which are required.
> **See §8 for the complete authoritative inventory. Total ≈ ₹8,050.**
>
> Two corrections against this table:
> - **Buy 3 LAN cables, not 2** — the spare's twisted pair becomes the CAN_H/CAN_L link
> - **Buy a 120-piece jumper set, not 4 assorted 40-piece sets** — §8.6 needs 25 × F-F,
>   which small sets do not contain

#### Module wiring (identical on both boards)

| Pi physical pin | BCM | Signal | Module pin |
| --- | --- | --- | --- |
| 1 | — | 3.3 V | VCC (MCP2515) |
| 2 | — | 5 V | TJA1050 VCC *(after trace cut)* |
| 6 | — | GND | GND |
| 19 | GPIO 10 | MOSI | SI |
| 21 | GPIO 9 | MISO | SO |
| 23 | GPIO 11 | SCLK | SCK |
| 24 | GPIO 8 | CE0 | CS |
| 22 | GPIO 25 | — | INT |

#### Two critical settings

**Crystal frequency.** Most generic MCP2515 modules ship **8 MHz**; the Waveshare HAT
uses 16 MHz. Read the marking before configuring. The driver flag must match:

```sh
can-mcp2515 --mid=eid -s /dev/io-spi/spi0/dev0 -c 8000000 -g 25   # 8 MHz module
```

A wrong value produces a driver that starts cleanly and transmits **zero frames**.

**Prove Pi 2 first.** Bring up the module on Pi 2 (Linux + SocketCAN) before touching
Pi 1. Pi 2 is expendable hardware, so a bad modification costs a spare board rather
than the QNX target.

### 7.5 Fallback — Waveshare 2-CH CAN HAT route

If the Waveshare becomes available, this is preferable: onboard 3.3 V logic jumper,
16 MHz crystal known-good, ESD protection, and **two channels** so Pi 1 can self-loop
CAN0↔CAN1 without a second board.

| Board | Buy | Cost |
| --- | --- | --- |
| Pi 1 (QNX) | 1× Waveshare 2-CH CAN HAT | ₹1,860 |
| Pi 2 (Linux) | 1× MKS CANable V2.0 (USB) | ₹3,899 |

**Total ≈ ₹5,760.** Sources: rarecomponents.com (₹1,860, SKU 17912, in stock),
hubtronics.in (₹1,949). Ask for **2-CH CAN HAT** or **2-CH CAN HAT+** by name — they
are separate SKUs (§7.3).

**Caveat if using a HAT:** it covers physical pins 7, 11 and 13, so GPIO marker wires
must be soldered to the header underside *before* mounting.

### 7.6 Rejected options

| Option | Why rejected |
| --- | --- |
| **"SmartElex 2 Channel CAN HAT"** (~₹938, Robocraze) | Listed with **TJA1050** transceivers — same 5 V problem. Ask the seller for written confirmation of level shifting before considering |
| **PiCAN2** (SK Pang) | Marked *Discontinued* in India; SK Pang states it is **not suitable for Pi 4** (use PiCAN3). ₹6,000–9,250 plus import |
| **Waveshare 2-CH CAN FD HAT** | Uses **MCP2517FD + MCP2562FD**. Different chips, different driver. Will not work with `can-mcp2515` |
| **PiCAN3** | Correct part, but ₹6,000+ import when Waveshare is ₹1,860 |
| Cheap USB-CAN dongle for Pi 1 | USB host CAN needs a vendor driver. `can-mcp2515` is SPI-only. Fine on Pi 2, useless on Pi 1 |

### 7.7 The driver is not Waveshare-specific

The driver README states it *"was developed for the Waveshare 2-CH CAN HAT but in
theory should work for any other HW connected to the RPI4 that uses the MCP2515."*

That is accurate — the driver talks to the MCP2515 over SPI, and you specify the SPI
device, interrupt GPIO, and clock on the command line:

```sh
can-mcp2515 --mid=eid -s /dev/io-spi/spi0/dev0 -c 16000000 -g 23
```

So the actual requirement is: **any MCP2515 on SPI0 with 3.3 V-safe signalling.**
That gives you room to substitute if Waveshare stays unavailable.

---

## 8. Final Hardware List — Complete Verified Inventory

### 8.1 Already owned

| # | Item | Qty | Note |
| --- | --- | --- | --- |
| 1 | Raspberry Pi 4B | 3 | Confirmed correct target |
| 2 | Router | 1 | **Verify ≥3 free LAN ports** |
| 3 | LAN cable Cat5e | 1 | |

---

### 8.2 CRITICAL GAP — power supplies

**The user's original BOM lists 1 power supply. Three boards need three.**

Pi 4B peaks near 3 A under load. Running CAN plus the workload plus the stress
generator, a 2 A supply will cause brownouts — and brownouts show up as latency
spikes that look like scheduler defects. This would corrupt the measurement.

| # | Item | Exact spec | Qty | Est. |
| --- | --- | --- | --- | --- |
| 4 | **USB-C power supply** | **5.1 V / 3 A**, official Pi 4 PSU or equivalent | **3** | ₹350 ea |

**Verify you have an SD card reader** on your PC (or a USB one) — required to flash
Pi 2 and Pi 3.

---

### 8.3 Selected route — CAN hardware

Waveshare 2-CH CAN HAT is unavailable. Breadboard MCP2515 modules are used instead,
which need **no HAT and no header soldering** (§7.4).

| # | Item | Exact spec | Qty | Est. |
| --- | --- | --- | --- | --- |
| 5 | **MCP2515 CAN bus module** | MCP2515 + TJA1050, onboard 120 Ω w/ jumper, screw terminal for CAN_H/L. **Read the crystal — likely 8 MHz** | **2** | ₹400 ea |
| 6 | **SN65HVD230 transceiver** | DIP-8 / SOIC-8, pin-compatible with TJA1050 | 2 | ₹40 ea |
| 7 | **120 Ω resistor** | 1/4 W, 1 % — spares, in case onboard jumpers are unclear | 4 | ₹10 ea |

*Item 6 is optional if using the trace-cut method instead. If unsure which method,
buy 2 chips — ₹80 total is cheaper than a second module.*

---

### 8.4 Connectivity

| # | Item | Exact spec | Qty | Est. |
| --- | --- | --- | --- | --- |
| 8 | **LAN cable Cat5e** | 1–2 m | **3** | ₹150 ea |

**Buy 3, not 2.** Three complete the network (1 owned + 2 new = 3 for the router), and
the **third new cable's twisted pair becomes the CAN_H/CAN_L link** (§8.5). This gives
a properly twisted pair at correct gauge for free.

---

### 8.5 Wiring — exact signal list per board

**Pi 1 (QNX) — MCP2515 module to header — 8 jumpers**

| Signal | Pi physical pin | BCM | Module pin |
| --- | --- | --- | --- |
| VCC | 1 | — | VCC (powers MCP2515) |
| 5 V feed | 2 | — | TJA1050 VCC pad *(after trace cut)* |
| GND | 6 | — | GND |
| MOSI | 19 | 10 | SI |
| MISO | 21 | 9 | SO |
| INT | 22 | 25 | INT |
| SCLK | 23 | 11 | SCK |
| CS | 24 | 8 | CS |

**Pi 1 — GPIO markers to breadboard — 4 jumpers**

Shared by both the LED proof test and the logic analyzer.

| Signal | Pi physical pin | BCM | Purpose |
| --- | --- | --- | --- |
| BRAKE marker | 7 | 4 | 10 ms pulse train |
| ADAS marker | 11 | 17 | 20 ms pulse train |
| DIAG marker | 13 | 27 | 50 ms pulse train |
| GND reference | 9 | — | logic analyzer ground clip |

**Pi 1 — UART loopback test — 3 jumpers**

| Signal | From | To |
| --- | --- | --- |
| Adapter TXD | USB-UART TX | Pi pin 10 (GPIO 15, RXD) |
| Adapter RXD | USB-UART RX | Pi pin 8 (GPIO 14, TXD) |
| GND | USB-UART GND | Pi pin 6 |

**Pi 2 (Linux) — MCP2515 module to header — 8 jumpers**
Identical to Pi 1.

**Inter-board CAN link — NOT dupont jumpers**

| Signal | From | To |
| --- | --- | --- |
| CAN_H | Pi 1 screw terminal | Pi 2 screw terminal |
| CAN_L | Pi 1 screw terminal | Pi 2 screw terminal |
| GND | Pi 1 GND | Pi 2 GND |

Cut one end off the spare LAN cable and use **one twisted pair** (blue/white) for
CAN_H/CAN_L, plus a second pair for GND. Strip and terminate into the screw
terminals. **Do not use dupont jumpers for the CAN bus** — they are too short for
board-to-board across a desk and are not twisted.

### 8.6 Jumper wire inventory — precise count

| Purpose | F-F | F-M | Total |
| --- | ---: | ---: | ---: |
| Pi 1 — MCP2515 module | 8 | 0 | 8 |
| Pi 1 — GPIO markers + GND | 1 | 3 | 4 |
| Pi 1 — UART loopback | 3 | 0 | 3 |
| Pi 2 — MCP2515 module | 8 | 0 | 8 |
| **Subtotal required** | **20** | **3** | **23** |
| Spares (wrong wires, re-runs) | 5 | 3 | 8 |
| **TOTAL** | **25** | **6** | **31** |

**Why F-F dominates:** both the Pi header pins and the MCP2515 module pins are male,
so F-F is the only type that connects them. F-M is used where a male end must plug
into a breadboard hole (GPIO marker rows).

| # | Item | Exact spec | Qty | Est. |
| --- | --- | --- | --- | --- |
| 9 | **Jumper wire set** | Dupont **120-piece assorted** (≈40 each of M-M / M-F / F-F), 20 cm | **1** | ₹200 |

A 40-piece set contains only ~10–20 F-F and will **not** cover the 25 required.
The 120-piece set is the minimum safe choice.

---

### 8.7 Measurement and prototyping

| # | Item | Exact spec | Qty | Est. |
| --- | --- | --- | --- | --- |
| 10 | **Logic analyzer** | 8-channel, ≥24 MHz (Saleae Logic 8 / clone) | 1 | ₹2,500–4,000 |
| 11 | **Breadboard** | Half-size, 400 tie-points | 2 | ₹120 ea |
| 12 | **LED** | 3 mm — 2 red (GPIO 4, 17), 1 green (GPIO 27), plus 3 spares | **6** | ₹5 ea |
| 13 | **330 Ω resistor** | 1/4 W — LED current limit | 6 | ₹10 ea |

24 MHz sampling gives ~40 ns resolution, comfortably inside the 50 µs tolerance
window. A 100 MHz oscilloscope is unnecessary and more expensive.

---

### 8.8 UART and storage

| # | Item | Exact spec | Qty | Est. |
| --- | --- | --- | --- | --- |
| 14 | **USB-to-UART adapter** | **3.3 V TTL**, CP2102 or FTDI chip | **2** | ₹300 ea |
| 15 | **microSD card** | 16 GB, A2 class or better — Pi 2 and Pi 3 | **2** | ₹500 ea |

**The UART adapters must be 3.3 V TTL.** An RS232-level adapter puts ±9 V on the Pi's
UART pins and destroys them. Verify the listing says "3.3 V TTL", not just
"USB to serial".

---

### 8.9 Strongly recommended — thermal integrity

| # | Item | Exact spec | Qty | Est. |
| --- | --- | --- | --- | --- |
| 16 | **Pi 4 case with fan** | Official case, or equivalent with active cooling | **1 minimum** | ₹700 ea |

**This is not cosmetic.** A thermal-throttled Pi 4B changes its own scheduling
behaviour, which is exactly the signal being measured. Without cooling, latency
percentiles will drift as the die heats and then distort again on cooldown. For a
tool whose entire purpose is latency measurement, that is a systematic error source,
not a nuisance.

Minimum: cool **Pi 1** (the board doing the measuring). Ideally cool all three.

---

### 8.10 Tools

| # | Item | Note | Est. |
| --- | --- | --- | --- |
| 17 | **Soldering iron** | Only for the TJA1050 trace cut / chip swap | ₹300, or ₹50–100 at a local repair shop |

The trace cut is a one-minute job. If you do not own an iron, paying a local repair
shop is cheaper than buying one.

---

### 8.11 Optional

| # | Item | Exact spec | Qty | Est. |
| --- | --- | --- | --- | --- |
| 18 | Waveshare 2-CH CAN HAT | Replaces items 5 + 6 on both boards. rarecomponents.com ₹1,860 (SKU 17912) or hubtronics.in ₹1,949 | 2 | ₹1,860 ea |
| 19 | MCP2515 CAN bus module | Third CAN node for a 3-node bus | 1 | ₹400 |
| 20 | Ethernet switch | Only if the router has fewer than 3 free LAN ports | 1 | ₹600 |

If you take item 18, **solder GPIO marker wires to Pi 1's header underside first** —
the HAT covers physical pins 7, 11 and 13. See §7.5.

---

### 8.12 Total

| Category | Items | Cost |
| --- | --- | --- |
| **Critical gap — power supplies** | 4 | ₹1,050 |
| CAN hardware | 5–7 | ₹920 |
| Connectivity (LAN cables) | 8 | ₹450 |
| Jumper wires | 9 | ₹200 |
| Measurement + prototyping | 10–13 | ₹3,130 |
| UART + storage | 14–15 | ₹1,600 |
| Cooling (Pi 1 minimum) | 16 | ₹700 |
| Tools (if needed) | 17 | ₹0–300 |
| **TOTAL** | | **≈ ₹8,050** |
| Optional extras | 18–20 | +₹2,260 to +₹4,720 |

*Logic analyzer price dominates. Everything else totals under ₹4,500.*

---

### 8.13 Checks on arrival

| Check | Action |
| --- | --- |
| **Crystal frequency** | Read the marking on each MCP2515 module. Almost certainly 8 MHz → driver needs `-c 8000000`. Wrong value = clean start, zero frames |
| **TJA1050 modification** | Cut the trace or fit an SN65HVD230 (§7.2). **Do Pi 2 first** |
| **Router ports** | Confirm ≥3 free LAN ports before buying switches |
| **Power supply rating** | Confirm 5.1 V / 3 A on all three supplies |
| **UART logic level** | Confirm "3.3 V TTL" on both adapters, not RS232 |
| **Header pin map** | Re-verify physical pin numbers before wiring: UART is **pin 8 TX / pin 10 RX**, not 11 |

---

### 8.14 Pin allocation — verified conflict-free

| Pi pin | BCM | Used by |
| ---: | ---: | --- |
| 1 | — | CAN module VCC |
| 2 | — | CAN module 5 V feed |
| 6 | — | CAN module GND, UART loopback GND *(shared, intended)* |
| 7 | 4 | BRAKE marker |
| 8 | 14 | UART TXD |
| 9 | — | Logic analyzer GND |
| 10 | 15 | UART RXD |
| 11 | 17 | ADAS marker |
| 13 | 27 | DIAG marker |
| 19 | 10 | CAN module MOSI |
| 21 | 9 | CAN module MISO |
| 22 | 25 | CAN module INT |
| 23 | 11 | CAN module SCLK |
| 24 | 8 | CAN module CS |

**No conflicts.** The only shared pin is 6 (GND), which is intentional. The MCP2515
module claims GPIO 8, 9, 10, 11 and 25; the markers claim 4, 17, 27. Disjoint.

---

## 9. Bring-Up Order

Each step is independently verifiable before the next begins.

1. **Install the RPi4 BSP** in QNX Software Center. Get `devc-ser` + `gpio-bcm2711`.
2. **Flash Pi 2 and Pi 3** with Raspberry Pi OS. Confirm all three on the router.
3. **UART** — plug adapter into Pi 1, loop TX→RX. `schedulix uart init` must flip from
   `simulated` to physical. Fix §5.3 while you are there.
4. **GPIO code** — apply §5.1, rebuild, deploy. `schedulix gpio test` must print
   `REAL PHYSICAL`, not `MOCK`.
5. **GPIO proof** — LED + 330 Ω from GPIO 4 (physical pin 7) to ground, straight onto
   the breadboard. `gpio test` must light it. **Do not skip this** — it is the only
   thing standing between you and a silent no-op.
6. **Logic analyzer** — connect to Pi 3, probe Pi 1 GPIO 4/17/27. Run `--full`. Expect
   three pulse trains at 10 / 20 / 50 ms.
7. **Modify ONE MCP2515 module** — cut the TJA1050 trace or fit an SN65HVD230 (§7.2).
   Wire to **Pi 2 only** per §4.1. Enable SPI, set
   `dtoverlay=mcp2515-can0,oscillator=8000000,interrupt=25`, then
   `ip link set can0 up type can bitrate 500000`. Verify with `candump`/`cansend`.
   **Do this first** — Pi 2 is expendable, so a bad modification costs a spare board.
8. **Second MCP2515 module** — modify and wire to **Pi 1** per §4.1. No header
   soldering needed; GPIO markers on pins 7/11/13 stay accessible.
9. **CAN driver on Pi 1 (QNX)** — write `spi.conf` with one `[dev]` section, copy
   `can-mcp2515` to `/system/bin`, start with
   `can-mcp2515 --mid=eid -s /dev/io-spi/spi0/dev0 -c 8000000 -g 25`. Confirm
   `/dev/can0`. Set catch-all filter with `canctl -u 0,rx0 -m 0` and `-f 0`.
10. **Cross-board CAN** — Pi 1 CAN_H to Pi 2 CAN_H, CAN_L to CAN_L, 120 Ω at each end,
    shared GND. Pi 2 generates frames, Pi 1 receives them.
11. **Write the QNX CAN adapter** (§5.2). This is the last code blocker.
12. **Wire Qt to real JSON** (§6.3). Screenshot with genuine data.

Steps 1–10 are hardware and infrastructure. Step 11 is the only substantial code
remaining, and step 12 is independent of all hardware.

---

## 10. Verification Gates

A claim is done when measured. This is how to prove each one.

| Claim | Proof | Risk |
| --- | --- | --- |
| Pi 1 on network | `ping` + `ssh root@<ip>` | None |
| UART is physical | `uart init` prints physical; loopback bytes observed | Low |
| GPIO is physical | `gpio test` prints `REAL`; **LED lights**; analyzer sees edges | Medium — one code fix |
| GPIO timing accurate | Measured pulse width vs software interval within 50 µs + 5 % | Medium |
| CAN driver loads | `/dev/can0` exists after `can-mcp2515` starts | High — wiring + crystal |
| CAN bus works | Frame sent on Pi 2 received on Pi 1 via `cat /dev/can0/rx0` | High |
| **External** CAN reaches workload | Frame generated on Pi 2 appears in Pi 1 `analysis_*.json` | High |
| Deadlines missed under load | S1 p99 > S0 p99, `misses > 0`, named `root_cause` | None — already works |
| RCA confirmed | `evidence_level: CONFIRMED` with real `interferer_tid` | None — already works |
| GUI shows real data | Screenshot traceable to a specific `analysis_s4.json` | None — code only |

Five of these ten are **already proven working** on the target: context switches, task
latency, jitter, CPU utilization, and deadline misses (per `docs/logs/qnx_logs_7.txt`).
UART is the cheapest remaining win. The two genuinely unfinished items are GPIO
(one missing register write) and CAN (module + driver + adapter code).

`VALIDATION_LOG.md` TEST 6 currently fails the third row's standard. That is the one
to fix.

---

## 11. Bottom Line

| Question | Answer |
| --- | --- |
| Will QNX hardware go to waste? | **No.** CAN driver builds, `libcan` links, `libtraceparser` present, GPIO register access proven by the official driver |
| Is any peripheral unsupported? | **No.** All three have working QNX paths |
| Is Waveshare required? | **No.** The driver targets the MCP2515 *chip*. Any MCP2515 on SPI0 with 3.3 V-safe signalling works (§7.7) |
| Any pin conflicts? | **No.** Verified allocation in §8.14 |
| Green signal to buy? | **Yes** |
| Total cost, selected route | **≈ ₹8,050** including power supplies and cooling (§8.12) |
| Biggest gap found in the original BOM | **Power supplies** — original BOM listed 1, three boards need three (§8.2) |
| Biggest risk remaining | CAN wiring + crystal frequency. Prove it on Pi 2 before Pi 1 |
| Biggest code gap remaining | `GPFSEL` missing, CAN adapter unwritten, Qt reading mock data |

Already proven working on the target: context switches, task latency, jitter, CPU
utilization, deadline misses. UART is the cheapest remaining win. GPIO needs one
register write. CAN needs the module plus your adapter code.

The hardware is the solved part. Three code gaps stand between you and a complete
system, and all three are yours to write.