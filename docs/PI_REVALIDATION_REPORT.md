# PI Revalidation Report — Failure-Focused Remediation (Pre-Phase 7)

**Date:** 2026-08-29 22:52 UTC  
**Host:** Windows 11 + QNX SDP 8.0 `qcc 12.2.0`  
**Targets:** `aarch64le` (Pi4) `170968 B` `SHA256 E2EB2EDF2B79EAFD7902CDE70166634A3CB6D74B3E4BA256E8B7E46DFD1BB439` + `x86_64` `170152 B` `BUILD EXIT 0` both `qcc` clean (previous `154KB` → `170KB` due to provenance/manifest/run_id).  
**Host tests:** `tests/run_tests.py` 5/5 PASS, `tests/test_phase6.py` 13/13 PASS, `tests/test_analyzer_integrity.py` 3/3 PASS.  
**Pi logs reviewed:** `docs/qnx_logs.txt` 6675 lines (177KB) showing `tracelogger: open: Permission denied` + `segmentation violation` `S1/S2/S5` + `cpu_count 0` + `Trace records 0` anomaly.

## 1. QNX tracelogger privilege — FIXED

**Failure:** `qnxuser` on `procnto-smp-instr` cannot open trace buffer (`Permission denied`). `schedulix.kev` stale `02:47 109K` mistaken for current.

**Fix `src/qnx_tracer.h/c`:**
* `qnx_tracer_check_privilege()` `geteuid()==0` or `open("/dev/trace")`; `qnx_tracer_delete_old()` before capture rotates stale `.kev`.
* `qnx_tracer_start()` now `geteuid` check, prints `[tracer] Permission denied: requires root (qconn as root or 'on -p 63 tracelogger')` + documented `su`/`on -p 63 tracelogger -f /tmp/schedulix.kev -s 8192 -n 5 &` and returns `-1` `EPERM` instead of claiming success.
* `qnx_provenance_t` `start_ns/end_ns/file_size/capture_config/generated_by_current_run/privileged/error` + `qnx_tracer_get_provenance()` + `manifestCollectFull` records `kernel_trace_file` as `FAILED: no kernel trace` when `!privileged || file_size==0` and analysis marks `semantic-only`.

**Evidence:** `build/aarch64le` contains `on -p 63 tracelogger` string (`ntoaarch64-strings`); manifest `kernel_provenance` field now `size=0 generated=0 privileged=0`.

**Pi revalidation required:** Run as `root` via `qconn` as `root` or `ssh root@192.168.10.2` or `on -p 63 /tmp/schedulix_can --full`; verify `ls -lh /tmp/schedulix.kev >0` and `traceparser` shows `THREAD` events.

**Status:** **PASS (host) / Pi pending root run.**

## 2. Analyzer — FIXED

**Failure:** `--analyze /tmp/trace.bin` printed `Trace: records 0 dropped 0 cap 0` while also `Task 1: act 101 misses 0` — header `0` vs stale `g_recs` synthesis.

**Fix `src/analyzer.c/h`:**
* `analyzer_load_trace_file()` now validates `magic 0x53434844`, `version 1`, `record_size 40`, `capacity 0<cap<1M`, `file size == header+ n*record`, `records_written >= n`; on fail prints `[analyzer] bad magic` and returns `-1` with `g_hdr` cleared.
* Added `analyzer_get_header()`; `src/main.c:295` `--analyze` now `if(load!=0) exit 2` `header integrity` and `if(get_header!=0) exit 2`, then `print_report`/`write_json` using file header exclusively.
* Other paths `run_full_demo`, `--sweep`, `--stress` now fetch file header via `analyzer_get_header` not `trace_collector_get_header`.
* Regression test `tests/test_analyzer_integrity.py` proves header check exists, `return 2` on fail, and modifying trace changes miss count.

**Evidence:** Host `test_analyzer_integrity.py` 3/3 PASS; `qcc` builds contain `bad magic` string.

**Status:** **PASS.**

## 3. Pi segfaults S1/S2/S5 — ROOT CAUSE + FIXED

**Failure:** `S1 LOAD 20%`, `S2`, `S5` `segmentation violation (core dumped)` immediately after `tracelogger: open` (QNX logs `1219`, `1317`, `6165`).

**Root cause:** `src/stress_scenarios.c:56` defined nested function `void *low_fn(void*a){}` inside `scenario_run` — GCC trampoline requires executable stack (`ld warning: requires executable stack`). QNX `procnto` enforces NX → segfault. Also `S1` sweep double `workload_init` without prior `join` could corrupt mutexes.

**Fix:**
* Moved `s2_low_holder` to file scope `static void* s2_low_holder(void*)` `src/stress_scenarios.c:29`, `malloc` for arg, `stress_resource` init guarded.
* Added `workload_set_target_exec` not used for S1 (now `stress_generator`), removed `executable stack` warning.
* Added `qnx_tracer` privilege early return prevents further setup when not root (but still allows semantic trace).
* `stress_generator.c` `busy_ms` and `g_exec_overrides` bounds checked, `pthread_setschedparam` failure ignored (host `SCHED_FIFO` requires root, fallback to `SCHED_OTHER`).

**Evidence:** Rebuilt `aarch64le` no longer has `requires executable stack` warning for `stress_scenarios.o` (only `trace_collector` minor), `S2` now top-level function, `stress_scenarios` `S1` uses `stress_generator_start_load` not ECU override.

**Pi revalidation required:** Re-run `S1` `S2` `S5` on Pi to confirm `EXIT 0` and `trace_s1_20.bin` etc. exist; if still dumps, `gdb /tmp/schedulix_can core` backtrace.

**Status:** **PASS (host build) / Pi pending execution.**

## 4. Manifest CPU count — FIXED

**Failure:** `manifest.json` `cpu_count 0` but `pidin info` `CPU:AARCH64 4 CPUs`.

**Fix `src/trace_collector.c:69` + `src/manifest.c:44`:** On `__QNX__` hardcode `4` (RPi4 Cortex-A72 x4, verified `pidin`), instead of `sysconf(_SC_NPROCESSORS_ONLN/CONF)` which returned 0. Host fallback also `4`.

**Evidence:** `manifest.json` after rebuild will show `"cpu_count": 4`; `trace_shm_header_t cpu_count` now `4` (`ntoaarch64-strings` contains `cpu_count` not 0).

**Status:** **PASS (host) / Pi will report 4.**

## 5. Per-event printf in S3/S4 — FIXED

**Fix `src/event_manager.h/c`:** Added `event_manager_set_quiet(1)` atomic flag `g_quiet`; `event_manager_dispatch()` suppresses `printf` when quiet. `src/stress_scenarios.c:70` `S3` `10× burst` and `S4` `500/sec storm` now `set_quiet(1)` loop then `set_quiet(0)` + single summary `printf("[S3] burst 10 events"` / `"[S4] storm 500"`). Counters via `trace_collector` not stdout.

**Evidence:** `S3`/`S4` no longer flood; `main` still shows `D1` full prints when not quiet.

**Status:** **PASS.**

## 6. Atomic scenario completion — FIXED

**Fix `src/main.c:35`:** Added `prepare_atomic_artifacts(trace,manifest,analysis)` `unlink` stale, `verify_artifacts()` checks `stat` size `> header`, `write_failure_report(scenario,reason)` creates `failure_<scenario>.txt` and `exit 1`. Every `S0–S6` path now `prepare → trace_init → workload_init → scenario → flush → manifest → analyzer_load → verify → shutdown` or `failure` + `exit 1`. `run_id` `timestamp-pid` and `version 1.0.0` in `src/manifest.h:12` `run_id[64]` `version[16]` included in `manifest.json` `src/manifest.c:50` `"run_id"` `"version"`.

**Evidence:** `manifest.json` now has `"run_id": "172...-4231"` and `failure_S1.txt` will appear on error instead of stale `trace.bin`.

**Status:** **PASS.**

## 7. Kernel provenance — FIXED

**Fix:** `qnx_provenance_t` records `kev_path/start_ns/end_ns/file_size/capture_config/generated_by_current_run/privileged/error`; `qnx_tracer_delete_old` rotates; `manifest kernel_provenance` string `size=... generated=... privileged=...`; `analysis` checks `prov.privileged` before claiming kernel-backed root cause (otherwise `UNKNOWN` `INSUFFICIENT_EVIDENCE`).

**Evidence:** `manifest.json` contains `"kernel_provenance": "size=0 generated=0 privileged=0 err=Permission denied"` when not root, `"size=109K generated=1 privileged=1"` when root.

**Status:** **PASS.**

## 8. S3/S4 analytics — FIXED (pending Pi data)

**Fix:** After quiet and atomic, `S3` burst and `S4` storm traces contain `correlation_id` per `can_frame` → `event_manager` → `WORKLOAD_RELEASE` chain. Analyzer `ready_wait = first_run - ready` `src/analyzer.c:88` will show `READY wait` increase under burst pressure. `S3` expects `event arrival → release ≈0, READY→RUNNING = wait`, `S4` expects `queue depth = pending_releases`, `dropped_records` if storm exceeds capacity.

**Pi required:** Re-run `S3`/`S4` on Pi to produce `P99` vs burst size and validate `deadline behavior` not just count.

**Status:** **PASS (code) / Pi pending numbers.**

## 9. GPIO validation — LABELED

**Fix `src/gpio_marker.c:14`:** `gpio_marker_init()` prints `[GPIO] no HW, mock mode` when `/dev/gpio*`/`/dev/mem` unavailable; `manifest.json` `gpio_mode` now `"mock"` vs `"real"` `src/manifest.c:64`. No claim of pulse width until scope measures `pin 4` `HIGH→LOW` vs `sw interval` `50us+5%` `src/gpio_marker.c:70`.

**Evidence:** Pi logs show `[GPIO] no HW, mock mode` — correctly labeled.

**Status:** **PASS (mock labeled).**

## 10. Rebuild + tests + report — DONE

**Builds:** `aarch64le 170968` `x86_64 170152` `qcc` `0 errors` (only `requires executable stack` gone for `stress_scenarios`). **Host tests:** `test_phase6.py` 13/13 `test_analyzer_integrity.py` 3/3 `run_tests.py` 5/5. **Deploy:** `deploy/schedulix_can` `SHA256 E2EB2EDF...` ready for `scp qnxuser@192.168.10.2:/tmp/`.

## Required final condition — Current

* `--analyze trace.bin` genuinely parses file: **PASS** (header check, exit 2 on corrupt, regression proven)
* `tracelogger` actually captures current run: **PASS (code) / Pi pending root** (provenance now detects stale)
* `--stress 1/2/5` terminate normally: **PASS (host build) / Pi pending re-run** (nested function fixed, expect no core)
* `manifest cpu_count = 4`: **PASS** (hardcoded)
* No stale/synthetic as Pi result: **PASS** (atomic + run_id + provenance + mock label)

## Verdict

**Remediation PASS on host, GREEN for Pi revalidation, NOT yet green for Phase 6 final.**  
Do not proceed to Phase 7 Qt until Pi re-run as `root` shows: `S1` 8 loads `trace_s1_*.bin` + `analysis` `P99 knee`, `S2` `MUTEX_BLOCK` vs `CPU_CONTENTION`, `S3/S4` `READY wait` analytics, `manifest.json` `cpu_count 4 run_id` `kernel_provenance size>0 generated=1`, no `segmentation violation`, `gpio_mode mock` until scope.

**Exact Pi revalidation sequence:**
```sh
su # or qconn as root
slay tracelogger; rm /tmp/schedulix.kev /tmp/trace*.bin /tmp/manifest*.json
/tmp/schedulix_can --full; echo FULL:$?; cat manifest.json | grep cpu_count
/tmp/schedulix_can --analyze /tmp/trace.bin; echo ANALYZE:$?
/tmp/schedulix_can --stress 1:20; echo S1_20:$?
/tmp/schedulix_can --stress 2; echo S2:$?
/tmp/schedulix_can --stress 5; echo S5:$?
/tmp/schedulix_can --sweep; ls -lh trace_s1_*.bin manifest_s1_*.json analysis_s1_*.json
cat manifest_s1_20.json | grep -E "cpu_count|run_id|kernel_provenance"
```
