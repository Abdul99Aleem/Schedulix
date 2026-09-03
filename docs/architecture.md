# Schedulix — Architecture Overview

Schedulix is an observability and performance-analysis engine for safety-critical workloads running on QNX RTOS.

## 1. System Pipeline

```
[ Workload Threads ] ──(Trace Events)──> [ MPSC Ring Buffer ]
                                                 │
                                                 ▼ (Post-mortem Flush)
                                           [ trace.bin ]
                                                 │
[ QNX Instrumented Kernel ]                      ▼
       │                                  [ Analyzer Engine ] <── [ correlation ]
       ▼ (tracelogger)                           │
[ schedulix.kev ] ──(libtraceparser)──> [ scheduler_correlator ]
                                                 │
                                                 ▼
                                          [ analysis.json ]
                                                 │
                                                 ▼
                                          [ Qt Dashboard ]
```

## 2. Decoupled Processing Architecture
1. **Parser (`qnx_kernel_trace_parser.c`)**: Interface to QNX `libtraceparser`. Callback functions process process/thread trace events to extract timestamp cycles, CPU ID, PID, and TID.
2. **Correlator (`scheduler_correlator.c`)**: Chronologically resolves raw events per CPU into state transition facts (`FACT_RUNNING`, `FACT_READY`, `FACT_BLOCKED`, `FACT_TERMINATED`). Evaluates ownership changes and preemption durations.
3. **RCA Engine (`analyzer.c`)**: Combines semantic TraceEvents and SchedulingFacts to calculate deadline misses, release offsets, lateness, and assign root causes with explicit evidence levels.
