# Schedulix — Hardware Procurement & Code Gap Plan

Branch: `hw/procurement-and-gap-plan`
Written: 2026-10-01
Scope: Problem statement 16, Automotive RTOS Performance & Latency Analyzer

---

## 1. Executive summary

The backend is **functionally complete against the problem statement on software**, and
the QNX target has been running real workloads (`qnx_logs_7.txt`). But the three
peripherals named in the problem statement are in very different states:

| Peripheral | Code state | Hardware needed | Buy priority |
| --- | --- | --- | --- |
| **UART** | **Real and complete.** `uart_adapter.c` opens `/dev/ser1`, configures 115200 8N1, non-blocking, simulated fallback. | USB-to-UART adapter only | **P0 — buy now** |
| **GPIO** | **Structurally present but non-functional on hardware.** Never sets `GPFSEL`, so pins stay in input mode and nothing is driven. Confirmed `MOCK` on the Pi. | Jumper wires + logic analyzer | **P1 — buy wires/analyzer, fix code** |
| **CAN** | **Zero real hardware path.** `qnx_can_adapter.c` is a comment saying it is not compiled. All frames come from a 5-entry static array or TCP. | MCP2515 HAT + transceiver + terminators + second node | **P2 — buy HAT, then write the adapter** |

**Do not buy anything until you read Section 4.** The GPIO fix is a code bug, not a
hardware gap, and the CAN adapter code does not exist yet. Only the UART adapter is a
buy-and-it-works item.

---

## 2. What the code actually does

### 2.1 CAN — simulated only, and this is a gap

`src/simulated_can_adapter.c` returns 5 hardcoded frames:

```c
static const can_frame_t k_sim_frames[] = {
    { .id = 0x100, .dlc = 1, .data = {0x01} },
    { .id = 0x200, .dlc = 1, .data = {0x01} },
    { .id = 0x300, .dlc = 1, .data = {0x01} },
    { .id = 0x999, .dlc = 1, .data = {0x01} }, /* UNKNOWN */
    { .id = 0x100, .dlc = 3, .data = {0xAA, 0xBB, 0xCC} },
};
```

`src/qnx_can_adapter.c` in full:

```c
/* This file is intentionally NOT compiled by default.
 * Rename qnx_can_adapter.c.stub -> qnx_can_adapter.c and adjust Makefile
 * when real hardware integration begins. See .stub for template.
 */
```

The `.stub` file correctly documents that QNX has **no SocketCAN**. There is no
`/dev/can0` convention. The DDK exposes `/dev/can0` with mailbox sub-paths like
`/dev/can0/tx2` and `/dev/can1/rx0`. Your own `can_adapter.h` diagram already flags
`qnx_can_adapter.c` as "tomorrow". It is still tomorrow.

`can_injector.c` is a **TCP** server, not CAN. It is useful and already works, but it
does not satisfy "External CAN events generate workload". Keep it as a fallback.

### 2.2 GPIO — the pin mode bug

`src/gpio_marker.c:77` admits the bug in a comment:

```c
/* BCM2711: GPSET0 offset 7, GPCLR0 offset 10, GPLEV etc. Simplified */
/* Real BSP would configure GPFSEL first — stub sets only */
volatile uint32_t *set = g_gpio_base + 7;
volatile uint32_t *clr = g_gpio_base + 10;
```

**The register offsets are wrong too.** On BCM2711 (`docs/gpio.md` gets this right):

| Register | Byte offset | Word offset |
| --- | --- | --- |
| `GPFSEL0` | `0x00` | 0 |
| `GPSET0` | `0x1C` | **7** ✓ |
| `GPCLR0` | `0x28` | **10** ✓ |

So `GPSET0` and `GPCLR0` offsets are correct. The fatal problem is the missing
`GPFSEL` write. At reset all `GPFSELn` bits are `000` = input. Writing `GPSET0`
while a pin is an input produces **no electrical output**. That is why your target
log shows:

```
[GPIO] no HW, mock mode (validation uses sw timestamps)
```

and `VALIDATION_LOG.md` TEST 6 "PASS" is a **software-timestamp comparison against
software-timestamp**, i.e. it validates nothing physical.

Two more issues in the same function:

- `GPIO4`, `GPIO17`, `GPIO27` are all in bank 0 (pins 0–31), so single-register
  writes are fine. If you extend past pin 31 you need bank 1 registers at `+32`.
- `gpio_marker_get()` returns `-1` unconditionally. If you ever want to read back a
  pin through `GPLEV0` (offset 13) it is not implemented.
- `mmap` of `/dev/mem` at `0xFE200000` requires root **and** a QNX BSP that permits it.
  If it fails you silently fall to mock. On QNX the more reliable path is the BSP's
  own `gpio-bcm2711` utility.

### 2.3 UART — genuinely ready

`src/uart_adapter.c` is the one peripheral implemented properly:

- `open()` with `O_RDWR | O_NOCTTY | O_NONBLOCK`
- Full `termios` setup, `cfsetospeed(B115200)`, 8N1, no flow control
- `VMIN=0`, `VTIME=5` so reads never block the RT tasks
- Graceful fallback to a 256-byte loopback queue if the device is absent
- Trace emission hooks via `uart_adapter_trace_rx/tx`

`main.c` exposes it as `uart init|send|receive|test`. **Buy a USB-to-UART adapter and
this works the same day it arrives.** No code change needed.

One correctness bug to fix regardless: `uart_adapter_trace_tx()` at line 151 records
`TRACE_EXTERNAL_EVENT_RX` for a **transmit**. It should be the TX marker or a distinct
event type, otherwise TX and RX are indistinguishable in the trace.

### 2.4 Working well

These are real and should not be touched:

- `tracelogger` + `libtraceparser` `.kev` decoding, statically linked, real
  context-switch capture confirmed on target (~114 KB `.kev`)
- 64-bit timestamp reconstruction with rollover detection
- S0–S6 experiment matrix, 20→95 % load sweep verified on the Pi
- MPSC shared-memory ring buffer, zero `malloc` in the hot path
- Delay attribution + RCA with `CONFIRMED`/`INFERRED` evidence levels

### 2.5 Qt frontend gap

`schedulix_v1/src/models/MockProvider.cpp` hardcodes every metric. There is **no
JSON parsing** — `grep` for `QJson` / `analysis_` / `QFile` in `src/` returns
nothing. The problem statement asks for an "Optional Python/Qt/Web Performance Trace
Viewer" fed by the CLI output. Right now the GUI is a rendering mock with no data
path. This is the largest remaining software gap and it needs no hardware.

---

## 3. Platform reality for QNX on Pi 4

Facts that shape the purchase, from QNX SDP 8.0 documentation:

- **Pi 4 (BCM2711) is the validated platform.** SDP 8.0 GA lists "Raspberry Pi4
  Model B" among validated platforms. A BSP exists for Pi 5 (BCM2712) in the
  documentation nav, but Pi 4 is what carries the validated status.
- **The 1 GB variant is explicitly unsupported.** Buy 4 GB or 8 GB.
- BSP ships drivers for: startup, I²C, network (GENET), SD/MMC, serial, SPI, PCI,
  USB OTG host, watchdog.
- **There is no GPIO resource-manager driver.** The BSP provides a *utility*,
  `gpio-bcm2711`, plus `mbox-bcm2711`. So `open("/dev/gpio-0")` in your
  `gpio_marker_init()` will fail on QNX. That whole probe loop is dead code on QNX.
- **There is no CAN driver in the BSP.** QNX's own DDK tutorial uses a Waveshare
  2-channel MCP2515 HAT on a Pi 4 with the `can-mcp2515` driver. That is your
  reference implementation.
- USB OTG host controller is supported, so a USB CAN adapter is possible, but the
  driver is vendor-specific (`dev-can-linux` covers PCAN/Kvaser/Advantech/Vector).

---

## 4. What to buy, and what NOT to buy

### 4.1 Buy now (P0) — works on arrival

| Item | Spec | Why |
| --- | --- | --- |
| USB-to-UART adapter | 3.3 V TTL logic level, CP2102 or FTDI, **not** RS232 | The only thing standing between you and working UART |
| Jumper wires | Male-female dupont, assorted | GPIO markers |
| 120 Ω resistor | 2 off, or 2× 60 Ω | CAN bus termination |
| microSD | 16 GB, A2 class | QNX image + traces |

**Critical:** the adapter must be **3.3 V logic level**. An RS232-level adapter will
put ±9 V on the Pi's UART pins and damage the GPIO block. Verify "3.3 V TTL" on the
listing, not just "USB to serial".

### 4.2 Buy now (P1) — needed to prove GPIO

| Item | Spec |
| --- | --- |
| Logic analyzer | 8-channel, 24 MHz+ (Saleae Logic 8 / clone, or a 100 MS/s LA2000-style unit) |
| LED + 330 Ω resistor | 3 off, to confirm pins actually drive |
| Breadboard + jumper set | Half-size is enough |

A 24 MHz analyzer resolves ~40 ns, comfortably inside your 50 µs tolerance window.
Do not buy a 100 MHz scope for this; the logic analyzer is the right instrument and
much cheaper.

### 4.3 Buy after you fix code (P2)

| Item | Spec | Gate |
| --- | --- | --- |
| MCP2515 2-channel CAN HAT | Waveshare 2-CH CAN HAT, **MCP2515 not MCP2515FD** | Only after `qnx_can_adapter.c` exists |
| CAN transceiver | Usually integrated on the HAT. If separate: TJA1050 or MCP2551 | — |
| Second CAN node | See 4.4 | Decide route first |

### 4.4 The second CAN node — pick one

**Route A: second MCP2515 HAT on the Pi itself.** Simplest. QNX's DDK tutorial does
exactly this — two channels on one board, `CAN0-H↔CAN1-H`, and it walks you through
loopback. But it is self-loopback, not an independent ECU simulation.

**Route B: USB-CAN adapter on your Windows host.** More realistic injection, but the
adapter must be one `dev-can-linux` supports: PCAN, Kvaser, Advantech, Vector, or
SJA1000/PLX90xx bridge cards. **If you buy a random cheap Chinese USB-CAN dongle, it
almost certainly will not have a QNX driver.** Check your candidate against that list
*before* ordering.

**Route C: no second node.** Inject from the host over TCP using the
`can_injector` / `tools/can_sender.py` path that already works. Honest fallback: it
does not satisfy "external CAN event" literally, but it is defensible if you document
it, and it costs nothing.

### 4.5 Do not buy

- **Anything CAN-FD.** Problem statement says CAN. `MCP2515FD` needs a different driver.
- **`MCP25625`.** Different chip, different driver.
- **Pi 5.** BSP exists but Pi 4 is the validated SDP 8.0 target.
- **Pi 4 1 GB.** Explicitly unsupported.
- **Isolated/high-voltage CAN gear.** Your traffic is 3.3 V bench-level.
- **A second Pi.** One target plus your Windows host covers everything.

---

## 5. Code work required, in dependency order

### Step 1 — Fix GPIO pin mode (smallest fix, biggest visible win)

`src/gpio_marker.c`. Set `GPFSEL` before touching `GPSET0`/`GPCLR0`:

```c
/* BCM2711 GPFSEL0 covers GPIO 0-9, one 3-bit field per pin.
   Field for pin n: (n / 10) register, bits (n % 10) * 3 .. +2.
   001 = output, 100 = input, 000 = input (reset). */
static inline void pin_set_output(volatile uint32_t *base, int pin) {
    volatile uint32_t *fsel = base + ((pin / 10));       /* GPFSEL0 = word 0 */
    uint32_t shift = (pin % 10) * 3;
    *fsel = (*fsel & ~(0x7u << shift)) | (0x1u << shift); /* 001 = output */
}
```

Then in `gpio_marker_init()`, after a successful `mmap`, configure all three pins as
outputs. Add a memory barrier before the `GPSET0`/`GPCLR0` write so the write is not
reordered past the mode change.

Also implement `gpio_marker_get()` via `GPLEV0` (word offset 13) so you can read a
pin back and prove the wire moved, not just that a register write returned.

**On QNX specifically:** the `/dev/gpio-0` probe loop will never succeed. Either keep
the `/dev/mem` + `mmap` route and require root, or shell out to the BSP utility:

```sh
gpio-bcm2711 set 4 op pn dh    # drive high
gpio-bcm2711 set 4 op pn dl    # drive low
```

The utility route will not give you sub-microsecond timestamps. If your timing
validation needs precision, the `mmap` route is the only option, and you must run as
root.

### Step 2 — Write the real CAN adapter

`src/qnx_can_adapter.c.stub` already has the right skeleton. Fill in the frame struct
and the DDK API. QNX DDK CAN exposes mailboxes, not a Linux-style `read()`:

```
/dev/can0/rx0   /dev/can1/rx0     # receive mailboxes
/dev/can0/tx2   /dev/can0/tx3 ... # transmit mailboxes
```

Configure with `canctl`, start with:

```sh
can-mcp2515 --mid=eid -s /dev/io-spi/spi0/dev0 -c 16000000 -g 23
can-mcp2515 --mid=eid -s /dev/io-spi/spi0/dev1 -c 16000000 -g 25 -u 1
```

**The `-c` value must match your HAT's crystal.** Modules ship with either 8 MHz or
16 MHz. A mismatch gives you a driver that starts cleanly and zero frames. Read the
crystal marking before configuring.

You also need `/etc/system/config/spi/spi.conf` with `busno=0`, `base=0xfe204000`,
`irq=150`, `input_clock=500000000`, and two `[dev]` sections at `clock_rate=10000000`.
This is verbatim from the QNX DDK quickstart.

Then rename `.stub` → `.c` and ensure exactly one adapter is linked. The Makefile uses
`rwildcard src *.c`, so both files would compile and you would get duplicate symbols.
Exclude the stub explicitly.

### Step 3 — Fix the UART trace marker

`src/uart_adapter.c:151`. TX is recorded as `TRACE_EXTERNAL_EVENT_RX`. Add or reuse a
distinct TX marker so the analyzer can tell directions apart.

### Step 4 — Wire the Qt frontend to real data

Replace `MockProvider` with a JSON loader over the `analysis_s*.json` / `manifest_s*.json`
the CLI already writes. The Qt app builds clean against Qt 6.8.3; this is pure C++
`QJsonDocument` work and needs no hardware. This is what turns the GUI from a
screenshot into a tool.

### Step 5 — Make GPIO validation meaningful

`gpio_marker_validate()` currently compares software timestamps to software timestamps
when mocked, which is vacuous. Once real pins drive, the logic analyzer measurement is
the independent check. Either feed measured pulse widths in, or drop the claim and let
the analyzer be the sole validation.

---

## 6. Bring-up order

Follow this so each step is verifiable before the next.

1. **UART first.** Plug in the 3.3 V TTL adapter. Confirm `ls /dev/ser*` on the Pi.
   `./schedulix_can uart init` must print `simulated` → change to physical. Then
   `uart send "CAN?"` and `uart receive`.
2. **GPIO second.** Flash the `GPFSEL` fix. Build, deploy. `./schedulix_can gpio test`
   must print `REAL PHYSICAL`, not `MOCK`. Wire an LED on GPIO4 with a 330 Ω resistor
   to ground and confirm it lights before trusting the analyzer.
3. **Logic analyzer third.** Clip on GPIO4/17/27. Run `--full`. Confirm three distinct
   pulse trains at 10 ms / 20 ms / 50 ms period. Now the latency validation is real.
4. **CAN last.** Mount the HAT, configure `spi.conf`, start `can-mcp2515`, confirm
   `/dev/can0` appears. Loop back CAN0↔CAN1 first and verify with `candump`-equivalent
   before wiring a second node.
5. **Qt last.** Point the frontend at real JSON, then screenshot with genuine data.

---

## 7. Verification gates

A claim is only done when it is measured, not asserted.

| Claim | How to prove it |
| --- | --- |
| UART is physical | `uart init` prints physical; loopback shows RX bytes on the wire |
| GPIO is physical | `gpio test` prints `REAL`; LED lights; analyzer sees edges |
| GPIO timing is accurate | Measured pulse width vs software interval within 50 µs + 5 % |
| CAN is physical | `/dev/can0` exists; external frame appears in `analysis_*.json` |
| Deadlines missed under load | S1 p99 > S0 p99, with `misses > 0` and a named `root_cause` |
| RCA is confirmed | `evidence_level: CONFIRMED` with a real `interferer_tid` |
| GUI shows real data | Screenshot with data traceable to a specific `analysis_s4.json` |

The current `VALIDATION_LOG.md` TEST 6 is the one entry that does not meet this bar.
Fix it as part of Step 1.

---

## 8. Summary BOM

| # | Item | Spec | Qty | Priority |
| --- | --- | --- | --- | --- |
| 1 | Raspberry Pi 4B | 4 GB or 8 GB, **not 1 GB** | 1 | Have |
| 2 | microSD card | 16 GB A2 | 1 | Have |
| 3 | USB-to-UART adapter | **3.3 V TTL**, CP2102/FTDI, not RS232 | 1 | **P0** |
| 4 | Jumper wires | Dupont assorted | 1 set | **P0** |
| 5 | 120 Ω resistor | 1/4 W | 2 | **P0** |
| 6 | LED + 330 Ω | for GPIO proof | 3 each | **P1** |
| 7 | Breadboard | half-size | 1 | **P1** |
| 8 | Logic analyzer | 8-ch, ≥24 MHz | 1 | **P1** |
| 9 | MCP2515 2-CH CAN HAT | Waveshare, MCP2515 not FD | 1 | **P2** |
| 10 | Second CAN node | Route A/B/C per Section 4.4 | 1 | **P2** |
| 11 | USB-CAN adapter | only if Route B, and only PCAN/Kvaser/Advantech/Vector | 0–1 | **P2** |

Items 1 and 2 are already in hand. **Items 3–8 are cheap and unblock real
verification immediately** — roughly the cost of one evening's parts. Items 9–11
should wait until the CAN adapter code exists, otherwise you are debugging hardware
and software simultaneously with no way to attribute failures.