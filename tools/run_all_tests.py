#!/usr/bin/env python3
"""Run every Schedulix host test suite and summarise.

    python tools/run_all_tests.py

Each suite is a standalone script; this just runs them all and aggregates the
result, so the pass/fail state of the project can be checked in one command on
any host, with no board attached.

Some suites need `awk` on PATH (test_spi_conf_edit.py). Those SKIP rather than
fail when awk is missing, because a bare Windows install may not have it. A SKIP
is reported separately from a PASS so it cannot be mistaken for coverage.
"""

import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TESTS = os.path.join(os.path.dirname(HERE), "tests")

SUITES = [
    ("run_tests.py", "CAN frame decode"),
    ("test_phase6.py", "stress scenarios + analyzer"),
    ("test_analyzer_integrity.py", "trace integrity"),
    ("test_uart_tx_marker.py", "UART TX trace marker"),
    ("test_spi_conf_edit.py", "installer spi.conf edit"),
]

# Matches "13/13 ... PASS", "Overall: PASS", "3/3 analyzer integrity tests PASS"
COUNT_RE = re.compile(r"(\d+)\s*/\s*(\d+)[^\n]*\bPASS\b")
OVERALL_RE = re.compile(r"^Overall:\s*PASS\s*$", re.MULTILINE)
SKIP_RE = re.compile(r"^SKIP\b", re.MULTILINE)


def main():
    total_pass = 0
    total_total = 0
    skipped = []
    failed = []

    print("Schedulix host test suites")
    print("=" * 64)

    for script, desc in SUITES:
        path = os.path.join(TESTS, script)
        if not os.path.exists(path):
            failed.append((script, "suite file missing"))
            print("FAIL %-28s %s  (missing)" % (script, desc))
            continue

        proc = subprocess.run(
            [sys.executable, path],
            capture_output=True,
            text=True,
        )
        out = (proc.stdout or "") + (proc.stderr or "")
        line = out.strip().splitlines()[-1] if out.strip() else ""

        counts = COUNT_RE.findall(out)

        # Decide between "whole suite skipped" and "suite ran with some cases
        # skipped" by looking for a results line FIRST. A suite can legitimately
        # print "SKIP <case>" for one case and still pass the rest, so treating
        # any SKIP line as a whole-suite skip would under-report real coverage.
        if counts:
            # The final match is the suite total.
            p, t = counts[-1]
            total_pass += int(p)
            total_total += int(t)
            ok = proc.returncode == 0
            # A suite may report "12/13 ... PASS (1 skipped: ...)". That is a
            # pass with a prerequisite absent, not a failure, so do not require
            # p == t when the suite says it skipped something deliberately.
            deliberate_skip = "skipped" in line.lower()
            if ok and deliberate_skip:
                print("PASS %-28s %-28s %s/%s (%s)"
                      % (script, desc, p, t, line.split("(", 1)[-1].rstrip(")")))
            else:
                ok = ok and int(p) == int(t)
                print("%s %-28s %-28s %s/%s" % (
                    "PASS" if ok else "FAIL", script, desc, p, t))
            if not ok:
                failed.append((script, line or "suite failed"))
        elif SKIP_RE.search(out):
            # No results line at all, and the suite said SKIP: it could not run
            # (e.g. a required host tool is missing).
            skipped.append((script, desc))
            print("SKIP %-28s %s" % (script, desc))
            continue
        elif OVERALL_RE.search(out):
            # No "N/N" summary line. Count the individual case lines instead of
            # reporting a single pass, so the aggregate total stays honest.
            cases = len(re.findall(r"^\s*PASS:", out, re.MULTILINE))
            cases = cases or 1
            total_pass += cases
            total_total += cases
            ok = proc.returncode == 0
            print("%s %-28s %-28s %d case(s)" % (
                "PASS" if ok else "FAIL", script, desc, cases))
            if not ok:
                failed.append((script, "suite reported FAIL"))
        else:
            failed.append((script, "no recognisable result line"))
            print("FAIL %-28s %-28s (unrecognised output)" % (script, desc))

    print("=" * 64)
    if skipped:
        print()
        print("Skipped (host tool missing, not a code failure):")
        for script, desc in skipped:
            print("  %-28s %s" % (script, desc))
        print("  test_spi_conf_edit.py needs `awk`. Run it from Git Bash on Windows,")
        print("  or from Linux/macOS, to actually validate the installer's config edit.")

    print()
    if failed:
        print("FAILURES:")
        for script, why in failed:
            print("  %-28s %s" % (script, why))
        print()
        print("%d/%d host tests PASS, %d suite(s) failed"
              % (total_pass, total_total, len(failed)))
        return 1

    print("%d/%d host tests PASS" % (total_pass, total_total))
    return 0


if __name__ == "__main__":
    sys.exit(main())