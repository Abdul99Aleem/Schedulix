# Schedulix — Benchmark Experiment Runner

This document outlines the S0-S6 benchmark scenarios implemented in Schedulix to test RTOS behaviors under load.

## 1. Benchmark Scenarios
Each benchmark runs for 5 seconds by default and captures performance metrics.

* **S0 Baseline**: Nominal execution environment. Runs workloads without any background stress.
* **S1 CPU Saturation**: Configurable CPU saturation loads (20% to 95% load sweep) to study resource contention.
* **S2 Mutex Contention / Priority Inversion**: Spawns a low-priority thread holding a shared mutex and a medium-priority spinner task, causing priority inversion on the `BRAKE_CTL` task.
* **S3 CAN Burst**: Simulates an incoming burst of 10 high-frequency CAN events to trigger task release latency spikes.
* **S4 Sustained Storm**: Event storm (500 events/second) that saturates the Event Manager and causes intentional deadline violations.
* **S5 Mixed Workload**: Runs background CPU stress (50% utilization) combined with high-frequency CAN interrupt events.
* **S6 CPU Affinity Configuration**: Pins stress threads to specific cores to observe scheduling behavior and migration.

## 2. Metrics Output
Each experiment produces three files in `/tmp` (or current directory):
1. `trace_s<id>.bin`: Raw binary trace record buffer.
2. `manifest_s<id>.json`: Run configuration, hardware properties, and system settings metadata.
3. `analysis_s<id>.json`: Correlated performance analysis (p50/p95/p99 response latency, jitter, context switches, and root causes).
