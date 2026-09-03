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
* Physical base address: `0xFE200000` (mapped via `/dev/mem` using QNX `mmap` if running as root).
* `GPSET0` register offset: `0x1C` (words offset 7).
* `GPCLR0` register offset: `0x28` (words offset 10).
* Fallback: If device drivers are missing or privileges are insufficient, Schedulix automatically logs `gpio_mode: mock` and simulates the behavior in software without throwing errors.
