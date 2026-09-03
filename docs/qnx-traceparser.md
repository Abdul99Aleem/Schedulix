# QNX libtraceparser Event Mapping Specification

This document defines the QNX-native trace classes, scheduler events, payload formats, and timestamp/priority extraction mechanisms utilized by Schedulix.

---

## 1. Trace Classes and Callback Registration

We register callbacks with the `libtraceparser` library to parse QNX kernel event files (`.kev`). The relevant class for scheduler and thread state transitions is:

```c
#define _TRACE_PR_TH_C  ((unsigned)0x00000004u<<10)
```

We register callbacks via:
```c
traceparser_cs(parser_state, user_data, callback_function, _TRACE_PR_TH_C, event_id);
```

---

## 2. Scheduler & Thread Event Constants

From `<sys/trace.h>`:

| Event Constant | Value / State | Event Meaning |
| :--- | :--- | :--- |
| `_NTO_TRACE_THRUNNING` | `STATE_RUNNING` (1) | Thread enters RUNNING state (is scheduled on CPU) |
| `_NTO_TRACE_THREADY` | `STATE_READY` (2) | Thread enters READY state (runnable but waiting) |
| `_NTO_TRACE_THMUTEX` | `STATE_MUTEX` (11) | Thread enters MUTEX blocked state |
| `_NTO_TRACE_THCONDVAR` | `STATE_CONDVAR` (12) | Thread enters CONDVAR blocked state |
| `_NTO_TRACE_THSEM` | `STATE_SEM` (15) | Thread enters SEMAPHORE blocked state |
| `_NTO_TRACE_THDESTROY` | `STATE_DESTROY` (23) | Thread is terminated/destroyed |

---

## 3. Payload Format and Priority Source

For thread state events (`_TRACE_PR_TH_C` class), the parameters in the callback buffer are:
* **PID**: `buffer[0]`
* **TID**: `buffer[1]`
* **Priority**: Not present in the thread state transition payload. 
  * *Resolution*: We map the `(PID, TID)` tuple to a thread metadata map populated by:
    1. Application-level TraceEvents (which log `task_id` and priority when threads start).
    2. Workload configuration tables (where priorities are known beforehand).
  * If the priority cannot be matched or resolved from these sources, `priority_known = false`, and the RCA engine will not attribute a priority-confirmed preemption.

---

## 4. CPU Extraction

The CPU ID is retrieved from the callback header parameter:
```c
unsigned cpu = _NTO_TRACE_GETCPU(header);
```

---

## 5. Timestamp Reconstruction and Rollover Handling

The callback `time` parameter contains only the **lower 32 bits** of the event clock cycles. 

To reconstruct the full 64-bit cycles stamp (`KernelTraceEvent.timestamp_cycles`), the parser tracks the upper 32 bits using a sequential rollover filter:
1. At initialization (or upon parsing header clock keywords like `_TRACEPARSER_INFO_CLK_INTIAL`), `g_last_time_64` is set.
2. For each event callback with `uint32_t time`:
   ```c
   uint64_t full_time = (g_last_time_64 & 0xFFFFFFFF00000000ULL) | time;
   if (time < (g_last_time_64 & 0xFFFFFFFFULL)) {
       // 32-bit rollover occurred
       full_time += 0x100000000ULL;
   }
   g_last_time_64 = full_time;
   ```
3. Conversion to nanoseconds:
   ```c
   const uint64_t *cps = (const uint64_t*)traceparser_get_info(state_ptr, _TRACEPARSER_INFO_CYCLES_PER_SEC, NULL);
   uint64_t timestamp_ns = (full_time * 1000000000ULL) / (cps ? *cps : 1);
   ```

---

## 6. Preemption Evidence Rules

We classify preemption using ordered scheduling events on the same CPU:
* **CONFIRMED_PREEMPTION**:
  1. Thread `A` is running on CPU `c` (`THRUNNING(A)` observed).
  2. Context switch occurs on CPU `c` to thread `B` (`THRUNNING(B)` observed).
  3. Thread `A` transitions to `READY` (`THREADY(A)` observed) with `cpu(A) == cpu(B) == c` and correct chronological order.
* **VOLUNTARY_BLOCK**: Thread `A` transitions from `THRUNNING` to `THMUTEX` or `THCONDVAR` or `THSEM`.
* **CPU_MIGRATION**: Thread `A` transitions from `THRUNNING` on CPU `c1` and subsequently enters `THRUNNING` on CPU `c2`.
* **SCHEDULER_DELAY**: Thread `A` is in `READY` state and waiting, but was not running immediately prior to B scheduling.
* **UNKNOWN**: Truncated trace logs or missing events.
