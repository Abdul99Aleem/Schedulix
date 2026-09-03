# QNX 8.0 Platform Instrumentation Guide

This document describes the native QNX APIs, system configurations, and permissions model required to monitor and analyze scheduling behaviors in Schedulix.

## 1. Native API References

### CPU Number Queries
To record task migrations, threads query the current active core number using:
```c
#include <sys/neutrino.h>
unsigned cpu = SchedGetCpuNum();
```
* **Header**: `<sys/neutrino.h>`
* **Implementation**: Matches the hardware CPU index (0 to 3 on Raspberry Pi 4).

### Task Priority and Affinity
* **Priority Configuration**: Real-time threads use `pthread_setschedparam()` with the `SCHED_FIFO` policy to ensure deterministic priority-driven scheduling.
* **Affinity Pinning**: We use the QNX-specific `ThreadCtl` call with the `_NTO_TCTL_RUNMASK` command:
  ```c
  unsigned mask = (1u << cpu_id);
  ThreadCtl(_NTO_TCTL_RUNMASK, &mask);
  ```

---

## 2. Kernel Event Tracing (`tracelogger`)

QNX implements a high-performance instrumented kernel capable of logging scheduling transitions, interrupts, and system calls to a ring buffer in memory.

### Capture Mechanism
* **Execution**: Schedulix runs QNX `tracelogger` as a background process to write raw trace buffers to disk:
  ```bash
  tracelogger -d -c -f /tmp/schedulix.kev
  ```
* **Permissions**: Access to the instrumented kernel is a privileged operation. Schedulix must be executed with **root privileges (`uid=0`)** to configure the tracer. If run as non-root, `qnx_tracer_start` returns non-zero, and the system falls back to semantic TraceEvent analysis with an `INFERRED` evidence level.

---

## 3. GPIO & Physical Register Maps

On the Raspberry Pi 4 (BCM2711), the peripheral base is mapped to memory via QNX `mmap_device_io()` or direct `/dev/mem` mmap to enable sub-microsecond physical timing marker pulses on the expansion header:
* **Physical Base Address**: `0xFE200000`
* **Address Range**: `0x100` bytes
* **Pins Used**: BCM 4 (Pin 7), BCM 17 (Pin 11), BCM 27 (Pin 13)
* **Access Level**: Requires root privileges. Defaults to software simulation timestamps if physical memory mapping is denied.
