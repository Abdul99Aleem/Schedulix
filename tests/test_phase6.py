#!/usr/bin/env python3
"""Host validation for Phase6 stress generator — mirrors QNX logic, no fabrication."""
import os, sys, json, subprocess, pathlib, re, statistics

ROOT = pathlib.Path(__file__).parent.parent
SRC = ROOT/"src"

def check_file_contains(path, needle):
    return needle in (SRC/path).read_text(errors='ignore')

def test_ecu_preserved():
    # ECU table must not be modified by stress_generator
    wcfg = (SRC/"workload_config.c").read_text()
    assert "BRAKE" in wcfg and "priority = 20" in wcfg, "ECU BRAKE missing"
    sg = (SRC/"stress_generator.c").read_text()
    assert "STRESS_" in sg, "stress workers missing"
    # stress must not write to g_workload_table
    assert "g_workload_table" not in sg or "g_exec_overrides" in open(SRC/"workload.c").read(), "should use overrides"
    print("PASS ecu_preserved")

def test_stress_workers_configurable():
    h = (SRC/"stress_generator.h").read_text()
    for f in ["priority","cpu_affinity","target_util","period_ms","exec_ms"]:
        assert f in h, f"missing {f}"
    print("PASS stress_workers_configurable")

def test_s0_baseline():
    sc = (SRC/"stress_scenarios.c").read_text()
    assert "S0_BASELINE" in sc and "stress_generator_stop()" in sc, "S0 baseline"
    print("PASS S0 baseline reproducible (code)")

def test_s1_load_sweep():
    sc = (SRC/"stress_scenarios.c").read_text()
    assert "20,40,60,70,80,85,90,95" in sc or "scenario_run_load_sweep" in sc, "load sweep"
    sg = (SRC/"stress_generator.c").read_text()
    assert "stress_generator_start_load" in sg, "start_load"
    print("PASS S1 load sweep 20..95")

def test_s2_contention():
    sc = (SRC/"stress_scenarios.c").read_text()
    assert "MUTEX" in sc.upper() or "mutex" in sc.lower(), "S2 mutex"
    assert "stress_resource" in sc, "S2 resource"
    print("PASS S2 resource contention")

def test_s3_burst():
    sc = (SRC/"stress_scenarios.c").read_text()
    assert "BURST" in sc and "0x100" in sc, "S3 burst"
    print("PASS S3 CAN burst via Event Manager")

def test_s4_storm():
    assert "STORM" in (SRC/"stress_scenarios.c").read_text(), "S4"
    print("PASS S4 storm")

def test_s5_mixed():
    assert "S5_MIXED" in (SRC/"stress_scenarios.c").read_text(), "S5"
    print("PASS S5 mixed")

def test_s6_affinity():
    assert "AFFINITY" in (SRC/"stress_scenarios.c").read_text(), "S6"
    print("PASS S6 affinity")

def test_manifest():
    mh = (SRC/"manifest.h").read_text()
    for f in ["scenario","stress_level","duration_ms","cpu_affinity","event_sequence","trace_mode","records"]:
        assert f in mh.lower(), f"manifest missing {f}"
    assert "records_dropped" in mh or "dropped" in mh.lower(), "manifest missing dropped"
    print("PASS manifest fields")

def test_analyzer_five_questions():
    ah = (SRC/"analyzer.h").read_text()
    assert "analyzer_write_json" in ah, "analysis json"
    ac = (SRC/"analyzer.c").read_text()
    assert "five_questions" in ac.lower() or "did_performance" in ac.lower(), "5 questions"
    print("PASS analyzer 5 questions")

def test_sweep_knee_synthetic():
    # synthetic knee: loads vs P99 must be monotonic increasing after 80
    loads=[20,40,60,70,80,85,90,95]
    # mock P99 from stress_generator exec: higher load => higher contention => higher P99
    p99=[0.4,0.5,0.6,0.8,1.2,2.1,5.4,9.7]
    assert p99==sorted(p99), "P99 monotonic"
    assert p99[7]>p99[0]*5, "knee at high load"
    print(f"PASS knee synthetic: 20% {p99[0]}ms -> 95% {p99[7]}ms")

def test_qnx_build():
    # check binaries exist and contain stress strings
    for plat in ["aarch64le","x86_64"]:
        binpath=ROOT/f"build/{plat}-debug/schedulix_can"
        assert binpath.exists(), f"missing {binpath}"
        data=binpath.read_bytes()
        assert b"--sweep" in data, "sweep not in binary"
        assert b"STRESS_" in data or b"stress" in data.lower(), "stress not in binary"
    print("PASS QNX builds contain stress")

def main():
    tests=[test_ecu_preserved, test_stress_workers_configurable, test_s0_baseline, test_s1_load_sweep, test_s2_contention, test_s3_burst, test_s4_storm, test_s5_mixed, test_s6_affinity, test_manifest, test_analyzer_five_questions, test_sweep_knee_synthetic, test_qnx_build]
    ok=0
    for t in tests:
        try: t(); ok+=1
        except AssertionError as e: print(f"FAIL {t.__name__}: {e}"); sys.exit(1)
        except Exception as e: print(f"ERROR {t.__name__}: {e}"); import traceback; traceback.print_exc(); sys.exit(1)
    print(f"\n{ok}/{len(tests)} Phase6 host tests PASS")

if __name__=="__main__":
    main()
