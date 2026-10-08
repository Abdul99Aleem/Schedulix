# Schedulix — GPIO Timing Marker

This document describes the hardware timing verification system implemented using Raspberry Pi 4 GPIO pins.

## 1. Objectives & Overview
Software performance metrics can suffer from execution-path overhead or clock-read delays. To validate timing parameters independently:
1. When an ECU workload runs (e.g., `BRAKE_CTL`), a specific GPIO pin is pulled **HIGH** at start.
2. The pin is pulled **LOW** at completion.
3. An external logic analyzer or oscilloscope captures the physical pulse duration.
4. The system validates software measurement correctness by checking `sw_duration` vs. `gpio_pulse_width` within a 50µs tolerance window.

## 2. Hardware Mapping
Pins are configured using Broadcom (BCM) GPIO numbering on the Raspberry Pi 4 expansion header:
* **Pin BCM 4 (Header Pin 7)**: mapped to `BRAKE_CTL` (Task ID 1).
* **Pin BCM 17 (Header Pin 11)**: mapped to `ADAS_FUSION` (Task ID 2).
* **Pin BCM 27 (Header Pin 13)**: mapped to `DIAG_POLL` (Task ID 3).

## 3. Physical Register Mapping (BCM2711)
* Physical base address: `0xFE200000`.
* **Mapping mechanism — not `/dev/mem`.** QNX has no `/dev/mem` on this image
  (`ls: /dev/mem: No such file or directory`), so `open("/dev/mem")` + `mmap` always
  fails. The correct QNX pattern maps physical memory directly, as used by this project's
  own working driver `third_party/can-mcp2515/driver/rpi4.c`:

  ```c
  #define BCM2711_GPIO_BASE   0xfe200000
  data->regs = mmap(0, __PAGESIZE, PROT_NOCACHE | PROT_READ | PROT_WRITE,
                    MAP_PHYS | MAP_SHARED, NOFD, BCM2711_GPIO_BASE);
  ```

  Key points: **no `/dev/mem` file descriptor**; `MAP_PHYS | MAP_SHARED` with the `NOFD`
  argument; and `PROT_NOCACHE` alongside read/write.
* Fallback: If device drivers are missing or privileges are insufficient, Schedulix
  automatically logs `gpio_mode: mock` and simulates the behavior in software without
  throwing errors.

### Register offsets

Offsets as defined by the BSP-derived constants in
`third_party/can-mcp2515/driver/rpi4.c` (verified working on QNX + Pi 4):

| Register | Byte offset | Notes |
| --- | --- | --- |
| `GPFSEL0–5` | `0x00`–`0x14` | **3 bits per pin, 10 pins per register. `001` = output.** Must be written *before* any `GPSET0`/`GPCLR0` |
| `GPSET0` / `GPSET1` | `0x1C` / `0x20` | Write-only. Set pin high |
| `GPCLR0` / `GPCLR1` | `0x28` / `0x2C` | Write-only. Set pin low |
| `GPLEV0` / `GPLEV1` | `0x34` / `0x38` | Read pin level |

**`GPFSEL` is not optional — it is the fatal step.** At reset every `GPFSELn` field reads
`000` = input. In input mode `GPSET0`/`GPCLR0` drive the internal pull-up/pull-down only
and produce **no electrical output whatsoever**. Any code that writes `GPSET0`/`GPCLR0`
without first programming `GPFSELn` will appear to work and emit nothing. The register
offsets for `GPSET0`/`GPCLR0` in the existing source are correct; `GPFSEL` is simply
missing.

The `GPFSEL` register for a pin is selected as `FSEL0 + (gpio / 10)`, and the 3-bit field
for that pin is at bit position `(gpio % 10) * 3` within it.

### GPIO numbering and banks
* Valid GPIO range on the Pi 4 is **0–53** (`BCM2711_GPIO_MIN` 0, `BCM2711_GPIO_MAX` 53,
  confirmed in `rpi4.c`).
* Each register bank covers **32** pins. **Bank 1 uses the same register addresses but
  offsets the bit index by `+32`** — e.g. GPIO 25 sets bit 25 of `GPSET0`, while GPIO 42
  sets bit `42 - 32` = bit 10 of `GPSET1`.

## 4. Current Implementation Status — **pending rewrite**

`src/gpio_marker.c` does **not** yet implement any of the above. Known defects:

| Issue | Detail |
| --- | --- |
| Wrong mapping mechanism | Still `open("/dev/mem", O_RDWR\|O_SYNC)` + `mmap`, at `src/gpio_marker.c:52-54`. Fails on QNX, which has no `/dev/mem` |
| `GPFSELn` never written | `src/gpio_marker.c:76-80` acknowledges this in a comment ("Real BSP would configure GPFSEL first — stub sets only"). Pins stay inputs, so there is no electrical output |
| `gpio_marker_get()` is a stub | `src/gpio_marker.c:90` returns `-1` unconditionally — no readback, so validation can never use a measured level |
| Bank 1 not implemented | Only GPIO 0–31 handled; the `+32` bank offset is missing, so pins above 31 are unreachable |
| Vacuous validation when mocked | `gpio_marker_validate()` compares software timestamps to software timestamps |

Until this is rewritten, `schedulix gpio test` will print `MOCK`, and no LED will light.
The physical timing verification described in §1 remains **unvalidated**.
