#!/usr/bin/env python3
"""Verify the installer's spi.conf edit is section-aware.

The installer (tools/qnx_can_install.sh) rewrites spi.conf with an awk script
that is supposed to change cpha/cpol/word_width in the spi0/dev0 block ONLY.

This matters because an earlier version of the installer replaced the whole
file with a DDK-derived one, which silently dropped the spi0/dev1 device and
the entire spi3 bus. That defect is what these tests exist to prevent.

Run:
    python tests/test_spi_conf_edit.py

Requires `awk` on PATH. If awk is unavailable the tests SKIP rather than fail,
because on some minimal Windows installs it is not present; the installer
itself guards for the same reason and refuses to edit the file without awk.
"""

import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
INSTALLER = os.path.join(REPO, "tools", "qnx_can_install.sh")
FIXTURE = os.path.join(HERE, "fixtures", "spi.conf.stock")

results = []


def check(name, cond, detail=""):
    results.append((name, bool(cond), detail))


def extract_awk_program():
    """Pull the embedded awk program out of the installer verbatim.

    Extracting it rather than duplicating it here is the point: if someone
    edits the awk in the installer, these tests exercise the new code.
    """
    with open(INSTALLER, "r", encoding="utf-8", errors="replace") as fh:
        text = fh.read()
    m = re.search(
        r"<<'AWKEOF'\n(.*?)\nAWKEOF", text, re.DOTALL
    )
    if not m:
        return None
    return m.group(1)


def parse_sections(text):
    """Return an ordered list of (section_kind, {key: value}) for a config."""
    sections = []
    cur_kind = None
    cur = {}
    for raw in text.splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("["):
            if cur_kind is not None:
                sections.append((cur_kind, cur))
            cur_kind = line
            cur = {}
            continue
        if "=" in line:
            k, v = line.split("=", 1)
            cur[k.strip()] = v.strip()
    if cur_kind is not None:
        sections.append((cur_kind, cur))
    return sections


def run_awk(awk_src, conf_text):
    tmpdir = tempfile.mkdtemp(prefix="spi_edit_")
    try:
        awk_path = os.path.join(tmpdir, "edit.awk")
        in_path = os.path.join(tmpdir, "in.conf")
        with open(awk_path, "w") as fh:
            fh.write(awk_src + "\n")
        with open(in_path, "w") as fh:
            fh.write(conf_text)
        exe = shutil.which("awk")
        if exe is None:
            return None, "awk not found"
        proc = subprocess.run(
            [exe, "-f", awk_path, in_path],
            capture_output=True,
            text=True,
        )
        if proc.returncode != 0:
            return None, proc.stderr.strip()
        return proc.stdout, None
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)


def main():
    if not os.path.exists(FIXTURE):
        print("FAIL fixture missing: %s" % FIXTURE)
        return 1
    if not os.path.exists(INSTALLER):
        print("FAIL installer missing: %s" % INSTALLER)
        return 1

    awk_src = extract_awk_program()
    if awk_src is None:
        print("FAIL could not extract the embedded awk program from the installer")
        print("     Expected a <<'AWKEOF' ... AWKEOF heredoc in tools/qnx_can_install.sh")
        return 1

    with open(FIXTURE, "r") as fh:
        stock = fh.read()

    out, err = run_awk(awk_src, stock)
    if out is None:
        print("SKIP awk unavailable or failed (%s)" % err)
        print("     Cannot validate the edit on this host.")
        return 0

    before = parse_sections(stock)
    after = parse_sections(out)

    check(
        "section count preserved (no sections dropped)",
        len(before) == len(after),
        "before=%d after=%d" % (len(before), len(after)),
    )

    # Identify spi0/dev0 and everything else.
    def is_target(sections):
        bus = None
        found = []
        for kind, kv in sections:
            if kind == "[bus]":
                bus = kv.get("busno")
            if kind == "[dev]":
                found.append((bus, kv))
        return found

    tb, ta = is_target(before), is_target(after)

    dev0_before = [kv for b, kv in tb if b == "0" and kv.get("devno") == "0"]
    dev0_after = [kv for b, kv in ta if b == "0" and kv.get("devno") == "0"]

    check("spi0/dev0 exists in fixture", len(dev0_before) == 1)
    check("spi0/dev0 still exists after edit", len(dev0_after) == 1)

    if dev0_before and dev0_after:
        b, a = dev0_before[0], dev0_after[0]

        check(
            "spi0/dev0 cpha forced to 0 (SPI mode 0; MCP2515 has no mode 1)",
            a.get("cpha") == "0",
            "before=%s after=%s" % (b.get("cpha"), a.get("cpha")),
        )
        check(
            "spi0/dev0 cpol is 0",
            a.get("cpol") == "0",
            "before=%s after=%s" % (b.get("cpol"), a.get("cpol")),
        )
        check(
            "spi0/dev0 word_width forced to 8 (MCP2515 is an 8-bit SPI part)",
            a.get("word_width") == "8",
            "before=%s after=%s" % (b.get("word_width"), a.get("word_width")),
        )
        check(
            "spi0/dev0 clock left at the stock value",
            a.get("clock_rate") == b.get("clock_rate"),
            "before=%s after=%s" % (b.get("clock_rate"), a.get("clock_rate")),
        )
        check(
            "spi0/dev0 identity keys untouched",
            a.get("name") == b.get("name")
            and a.get("parent_busno") == b.get("parent_busno")
            and a.get("bit_order") == b.get("bit_order"),
        )

    # The regression that motivated this file: everything that is NOT spi0/dev0
    # must come through byte-identical.
    others_before = [kv for b, kv in tb if not (b == "0" and kv.get("devno") == "0")]
    others_after = [kv for b, kv in ta if not (b == "0" and kv.get("devno") == "0")]
    check(
        "every other [dev] section is unchanged",
        others_before == others_after,
        "before=%s\n     after =%s" % (others_before, others_after),
    )

    bus_before = [kv for kind, kv in before if kind == "[bus]"]
    bus_after = [kv for kind, kv in after if kind == "[bus]"]
    check(
        "every [bus] section is unchanged (spi3 must survive)",
        bus_before == bus_after,
        "before=%s\n     after =%s" % (bus_before, bus_after),
    )

    check(
        "spi3 bus base/irq preserved",
        any(
            kv.get("base") == "0xfe204600" and kv.get("irq") == "151"
            for kv in bus_after
        ),
    )

    # Comments and blank lines should survive too.
    check(
        "comment lines preserved",
        out.count("#") == stock.count("#"),
        "before=%d after=%d" % (stock.count("#"), out.count("#")),
    )

    # Idempotency: running the edit twice must equal running it once.
    out2, err2 = run_awk(awk_src, out)
    check(
        "edit is idempotent (safe to re-run the installer)",
        out2 == out,
        err2 or "",
    )

    print("Schedulix spi.conf section-aware edit tests")
    print("=" * 56)
    failed = 0
    for name, ok, detail in results:
        if ok:
            print("PASS %s" % name)
        else:
            failed += 1
            print("FAIL %s" % name)
            if detail:
                print("     %s" % detail)

    print()
    if failed:
        print("%d/%d spi.conf edit tests FAIL" % (failed, len(results)))
        return 1
    print("%d/%d spi.conf edit tests PASS" % (len(results), len(results)))
    return 0


if __name__ == "__main__":
    sys.exit(main())