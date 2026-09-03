#!/bin/sh
# Pi validation sequence TEST 1-6 — run on Raspberry Pi 4 QNX
# Usage: chmod +x pi_validate.sh && ./pi_validate.sh
set -e
BIN=/tmp/schedulix_can
[ -x /tmp/schedulix_can ] || BIN=./build/aarch64le-debug/schedulix_can
[ -x "$BIN" ] || { echo "binary not found"; exit 1; }

echo "=== TEST 1: workloads ==="
$BIN --workloads
echo "TEST1 workloads done; check: ECU threads started, trace_phase1.bin, manifest_phase1.json"
ls -lh trace_phase1.bin manifest_phase1.json
echo

echo "=== TEST 2: full core ==="
$BIN --full
echo "TEST2 full done; check trace.bin, trace_flight.bin, manifest.json, .kev"
ls -lh trace.bin trace_flight.bin manifest.json manifest.txt /tmp/schedulix.kev
echo

echo "=== TEST 3: tracelogger ==="
if [ -f /tmp/schedulix.kev ]; then
  echo "tracelogger .kev exists: $(stat -c %s /tmp/schedulix.kev 2>/dev/null || wc -c < /tmp/schedulix.kev) bytes"
  echo "Verify correlation: schedulix semantic + QNX kernel events in same window"
  # traceprinter or traceparser check
  # traceparser -f /tmp/schedulix.kev 2>&1 | head -20
else
  echo "SKIP: /tmp/schedulix.kev missing — run: tracelogger -f /tmp/schedulix.kev -s 8192 -n 5 &"
fi
echo

echo "=== TEST 4: analyze ==="
$BIN --analyze trace.bin
echo "Check per-activation ready_wait / response / slack / root-cause"
echo

echo "=== TEST 5: stress knee ==="
echo "S0 baseline (light) vs S1 saturation (heavy) should show knee"
# Run S0 and S1 separately and analyze
$BIN --stress 0
mv trace_stress.bin trace_s0.bin
$BIN --analyze trace_s0.bin > analysis_s0.txt; cat analysis_s0.txt
$BIN --stress 1
mv trace_stress.bin trace_s1.bin
$BIN --analyze trace_s1.bin > analysis_s1.txt; cat analysis_s1.txt
echo "Compare P99: S0 low, S1 high, expect deadline miss in S1 with READY contention"
echo

echo "=== TEST 6: GPIO ==="
if $BIN --full 2>&1 | grep -q "GPIO"; then echo "GPIO marker attempted"; fi
# On Pi, scope GPIO pins 4 (BRAKE),17 (ADAS),27 (DIAG): HIGH at START, LOW at END
# Validate: software interval vs pulse width within 50us+5%
echo "Check dmesg / slog2info for [GPIO] mmap /dev/gpio"
echo "On scope: BRAKE pin 4 pulse should match software exec 2ms ± tolerance"
echo

echo "=== Manifest ==="
cat manifest.json
echo
echo "All 6 tests completed. If no crashes, traces exist, and S1 shows higher P99/miss than S0, CORE is green for Qt/CLI."
