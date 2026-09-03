# Schedulix — Real-Time Timing Model

This document specifies the timing formulas, measurements, and definitions utilized in Schedulix latency and jitter analyses.

## 1. Absolute Periodic Workload Model

To avoid cumulative drift across multiple activation iterations, the periodic timer schedule is derived mathematically from the initial thread release time $t_0$:

$$T_{\text{ideal}}[n] = t_0 + n \times P$$

Where:
* $t_0$ is the MONOTONIC timestamp of the first activation release ($n = 0$).
* $P$ is the task's configured period (e.g., 10 ms).
* $n$ is the activation index (0, 1, 2, ...).

### Timing Metrics Calculations

| Metric | Formula | Definition | Source |
| :--- | :--- | :--- | :--- |
| **Ideal Release** | $T_{\text{ideal}}[n] = t_0 + n \times P$ | Absolute target release time | Monotonic reference |
| **Actual Release** | $T_{\text{actual}}[n]$ | Real clock time when thread timed out / woke | Monotonic clock |
| **Release Jitter** | $O_{\text{release}}[n] = T_{\text{actual}}[n] - T_{\text{ideal}}[n]$ | Delay relative to ideal schedule | Monotonic clock |
| **Ready Wait** | $W_{\text{ready}}[n] = T_{\text{start}}[n] - T_{\text{ready}}[n]$ | Pre-execution scheduler queue delay | Monotonic clock |
| **Response Latency** | $L_{\text{response}}[n] = T_{\text{finish}}[n] - T_{\text{actual}}[n]$ | Total time from wake to end | Monotonic clock |
| **Deadline Slack** | $S[n] = D - L_{\text{response}}[n]$ | Margin until deadline violation | Monotonic clock |
| **Lateness** | $L_{\text{late}}[n] = \max(0, L_{\text{response}}[n] - D)$ | Delay beyond relative deadline $D$ | Monotonic clock |

---

## 2. Jitter Metrics

* **Response Jitter**: Calculated per-task over the entire capture history as:
  $$J_{\text{response}} = T_{\text{max\_response}} - T_{\text{min\_response}}$$
* **Percentiles Calculation**: Metrics reports calculate p50, p95, and p99 response percentiles by applying a standard `qsort` comparison to response time arrays.
* **Release Jitter**: Calculated per-task as:
  $$J_{\text{release}} = T_{\text{max\_release\_offset}} - T_{\text{min\_release\_offset}}$$
  This isolates the scheduler timer resolution and interrupt handling latency from the application thread execution time.
