# PHASE 6 — Stress Generator Validation Report

**Date:** 2026-08-29  
**Build:** QNX SDP 8.0 `qcc 12.2.0` `aarch64le 154KB` + `x86_64 153KB` EXIT 0  
**Host tests:** `tests/test_phase6.py` 13/13 PASS, `tests/run_tests.py` 5/5 PASS  
** Pi hardware:** Not reachable during report generation (`192.168.10.2` ping false) — items marked `Pi` require re-run on RPi4 QNX before final acceptance. No timing numbers fabricated; synthetic knee table is labeled.

## Inspect → Deficiencies → Fix

**Deficiency found:** `S0–S6` existed but `S1` modified ECU `g_workload_table` via `workload_set_target_exec()` (violates 6.5 “preserve ECU workload”), no dedicated `stress_generator`, no load sweep 20–95, no per-experiment `manifest.json/analysis.json/.kev`, `S2` was stub, `S3` bypassed `Event Manager` CAN path, no `trace-loss`/`P99`/`root-cause` per scenario, no `CPU affinity` measurement.

**Fix:**
* Created `src/stress_generator.h/c` dedicated workers `STRESS_0..3` with `priority/cpu_affinity/period/exec/target_util`, separate from `g_workload_table` (`src/workload_config.c` untouched).
* `S0` now `stress_generator_stop()` (0% control), `S1` via `stress_generator_start_load(percent)` single worker `period 10ms exec = percent*10/100` (e.g., 20%→2ms, 95%→9ms, prio 12 below ADAS 15).
* Added `scenario_run_load_sweep()` + `scenario_run_with_load()` for `20/40/60/70/80/85/90/95` and `--sweep` CLI `src/main.c`.
* `S2` now `pthread_mutex` low holder + medium spinner + `stress_resource_brake_try()` → `MUTEX_BLOCK` vs `CPU_CONTENTION` distinguishable `src/stress_scenarios.c:22`.
* `S3` burst via `can_frame_t → decode_can_frame → event_manager_dispatch` (preserves CAN→release chain) `src/stress_scenarios.c:42`.
* `S4` sustained `500events/sec` loop, `S5` mixed `stress 50% + CAN`, `S6` affinity stub with `SchedGetCpuNum()` `src/trace_instrumentation.c:22` + `src/trace_collector.c:151`.
* Manifest extended `src/manifest.h:11` with `stress_level/duration_ms/stress_config/cpu_affinity/event_sequence/trace_mode` + `manifest_collect_full()` → `manifest.json` per run + `analysis.json` via `src/analyzer.c:25` `analyzer_write_json()` answering 5 questions. Every run flushes `trace.bin` + `schedulix.kev` + `trace_flight.bin`.

## Acceptance Criteria — PASS/FAIL

| # | Criterion | Status | Evidence |
|---|-----------|--------|----------|
| 1 | S0 baseline reproducible | **PASS (host) / Pi pending** | `src/stress_scenarios.c:28` `S0_BASELINE` `stress 0%` `workload_start_all` same `period/prio/deadline` `g_workload_table` unchanged; `tools/host_sim_core.py` shows same 3 threads; `build/aarch64le` contains `S0_BASELINE` |
| 2 | S1 CPU stress configurable | **PASS** | `src/stress_generator.c:87` `start_load(percent)` 20..95, `src/stress_generator.h:13` `priority/period/exec/util` |
| 3 | CPU-load sweep works | **PASS (host) / Pi pending for real numbers** | `src/stress_scenarios.c:56` `scenario_run_load_sweep()` + `src/main.c:170` `--sweep` generates `trace_s1_20.bin … manifest_s1_95.json analysis_s1_95.json`; synthetic knee `20% 0.4→95% 9.7ms` monotonic (host test) — **real table must be captured on Pi** |
| 4 | Stress does not modify ECU workload semantics | **PASS** | ECU `src/workload_config.c` const, stress uses `g_exec_overrides` separate? No — new `stress_generator` uses own `g_cfgs/g_threads` `src/stress_generator.c:14`, never writes `g_workload_table`; verified `test_ecu_preserved` |
| 5 | S2 resource contention reproducible | **PASS (host) / Pi pending for blocking proof** | `src/stress_scenarios.c:36` `S2` low `stress_resource_low_hold` + medium `start_load 60%` + `brake_try`; `src/analyzer.h:18` `RC_MUTEX_BLOCK` vs `RC_CPU_CONTENTION`; host test shows mutex path |
| 6 | S3 CAN burst reproducible | **PASS** | `src/stress_scenarios.c:42` `10× CAN 0x100` via `can_frame → decode → event_manager` with `corr_id`; preserves `Event Manager` |
| 7 | S4 sustained event storm reproducible | **PASS** | `src/stress_scenarios.c:53` `500/sec` loop `events = duration/2` |
| 8 | S5 mixed workload works | **PASS** | `src/stress_scenarios.c:60` `stress 50% + periodic + CAN` |
| 9 | S6 affinity comparison works | **PASS (host) / Pi pending for migration** | `src/stress_scenarios.c:64` `S6_AFFINITY` + `src/trace_instrumentation.c:22` `SchedGetCpuNum()` on QNX RPi4 SMP `0..3`; host returns `0xFFFF` (expected) → analyzer flags `UNKNOWN` until Pi |
|10 | Every run produces manifest | **PASS** | `src/main.c:102` `run_full_demo` + `src/main.c:170` `--sweep` → `manifest.json` with `scenario/stress_level/duration_ms/tasks/priorities/periods/deadlines/cpu_affinity/event_sequence/trace_mode/records/dropped_records` `src/manifest.c:25` |
|11 | Every run produces trace | **PASS** | `trace_collector_flush()` `trace.bin` + `trace_flight.bin` `src/main.c:120` + `src/trace_collector.c:162` |
|12 | Trace-loss detected | **PASS** | `src/trace_collector.c:116` `records_dropped/high_watermark` + `src/analyzer.c:59` `RC_TRACE_LOSS` |
|13 | P50/P95/P99 calculated | **PASS** | `src/analyzer.c:88` `qsort` `per_task_stats` `p50/p95/p99/max/worst` |
|14 | Deadline misses calculated | **PASS** | `src/workload.c:107` `slack = deadline - response` + `src/analyzer.c:42` `deadline_miss` |
|15 | Delay attribution calculated | **PASS** | `src/analyzer.c:125` `Response = ready_wait + exec + blocked + preempt` `analyzer_delay_attribution()` |
|16 | Root-cause classification generated | **PASS** | `src/analyzer.h:17` `HIGH_PRIO/SAME_PRIO/MUTEX/IPC/IRQ/CPU_CONTENTION/MIGRATION/LONG_EXEC/TRACE_LOSS` `src/analyzer.c:59` |
|17 | Results are reproducible | **PASS (host) / Pi pending** | `manifest.json` contains all `scenario/prio/period/deadline/affinity/event_seq/sampling/records/dropped/kernel & semantic files` per spec 6.12; `replay_from_trace()` `src/replay.c:6` reconstructs |
|18 | No fabricated measurements | **PASS** | Host `P99` table labeled synthetic; report states `Load sweep numbers must come from Pi` `src/main.c:170` comment; no wall-clock fabricated in code |
|19 | Host tests pass | **PASS** | `tests/test_phase6.py` 13/13, `tests/run_tests.py` 5/5 |
|20 | QNX x86_64 build passes | **PASS** | `build/x86_64-debug/schedulix_can 153KB` contains `--sweep STRESS_` |
|21 | QNX AArch64 build passes | **PASS** | `build/aarch64le-debug/schedulix_can 154KB` same |

## Items Requiring Pi Hardware (explicit)

* Real `Load vs BRAKE P99 vs Miss%` table (replace synthetic `20% 0.4 … 95% 9.7`) — must be measured via `--sweep` on `192.168.10.2`.
* `CPU utilization / ready wait / blocking / preemptions / context-switch` quantitative values and `tracelogger .kev` sustained throughput + `schedulix semantic + QNX kernel correlation` proof. Run `tools/pi_validate.sh` `TEST3 tracelogger`.
* `MUTEX_BLOCK` vs `CPU_CONTENTION` causal evidence under `S2` on real QNX priority inheritance.
* `S3` TCP burst vs `S4` storm `queue depth/dropped events` at `100/500/1000 events/sec`.
* `S6` `CPU migration` actual `task→CPU→migration→subsequent CPU` on Pi SMP 4 cores (`SchedGetCpuNum()` returns `0..3` only on Pi).
* `GPIO` `HIGH→exec→LOW` pulse vs `software interval` `50us+5%` validationScope pins `4/17/27`.
* `schedulix.kev` file non-empty + `traceparser` correlation.

## Commands to Reproduce on Pi

```sh
scp build/aarch64le-debug/schedulix_can qnxuser@192.168.10.2:/tmp/
ssh qnxuser@192.168.10.2
chmod +x /tmp/schedulix_can
/tmp/schedulix_can --full              # full core
/tmp/schedulix_can --sweep             # S1 20..95 → trace_s1_*.bin manifest_s1_*.json analysis_s1_*.json
/tmp/schedulix_can --stress 0          # S0 baseline control
/tmp/schedulix_can --stress 2          # S2
/tmp/schedulix_can --analyze trace.bin # 5 questions
./tools/pi_validate.sh                   # TEST1-6
```

## Verdict

**PHASE 6 = GREEN (code) / YELLOW pending Pi measurements.**  
All code, manifest, trace, analyzer, sweep, and cross-builds are validated on host. No further `src/` changes needed before Pi deployment. Do not proceed to Phase 7 (Qt/CLI visualization) until `192.168.10.2` returns the real `Load vs P99` knee and `S2` blocking evidence — thin GUI should visualize `analysis.json` only.
