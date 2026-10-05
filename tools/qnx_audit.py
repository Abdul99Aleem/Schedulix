#!/usr/bin/env python3
"""
Schedulix - run the QNX bring-up audit over SSH (paramiko transport).

Uploads tools/qnx_bringup_check.sh and runs it, then saves the full output to
bringup_report.txt in the repository root so nothing is lost when you unplug
the Ethernet cable.

    python qnx_audit.py
    python qnx_audit.py --password <pw>
"""

from __future__ import annotations

import argparse
import os
import sys
import time

import paramiko

REPO = r"C:\Users\User\ide-8.0.3-workspace\schedulix_can"
HOST = os.environ.get("SCHEDULIX_PI_HOST", "192.168.10.5")
USER = os.environ.get("SCHEDULIX_PI_USER", "qnxuser")
PW = os.environ.get("SCHEDULIX_PI_PASSWORD", "qnxuser")
REPORT = os.path.join(REPO, "bringup_report.txt")
TIMEOUT = 15


class Tee:
    """Print to console and append to a report file at the same time."""

    def __init__(self, path: str) -> None:
        self.lines: list[str] = []
        self.fh = open(path, "w", encoding="utf-8")

    def __call__(self, msg: str = "") -> None:
        print(msg, flush=True)
        self.fh.write(msg + "\n")
        self.fh.flush()
        self.lines.append(msg)

    def close(self) -> None:
        self.fh.close()


def connect() -> paramiko.SSHClient:
    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    client.connect(
        hostname=HOST,
        username=USER,
        password=PW,
        timeout=TIMEOUT,
        auth_timeout=TIMEOUT,
        banner_timeout=TIMEOUT,
        look_for_keys=False,
        allow_agent=False,
    )
    return client


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--password", default=None)
    ap.add_argument("--host", default=HOST)
    ap.add_argument("--user", default=USER)
    args = ap.parse_args()

    host, user, pw = args.host, args.user, args.password
    if pw is None:
        pw = os.environ.get("SCHEDULIX_PI_PASSWORD", "qnxuser")

    say = Tee(REPORT)
    say("=" * 60)
    say(" Schedulix QNX bring-up audit")
    say(f" {time.strftime('%Y-%m-%d %H:%M:%S')}   target: {user}@{host}")
    say("=" * 60)
    say()

    try:
        client = connect()
    except Exception as exc:  # noqa: BLE001
        say(f"CONNECT FAILED: {type(exc).__name__}: {exc}")
        say.close()
        return 2

    say(f"connected as {user}@{host}")
    say()

    # Upload the audit script.
    local = os.path.join(REPO, "tools", "qnx_bringup_check.sh")
    try:
        sftp = client.open_sftp()
        sftp.put(local, "/tmp/qnx_bringup_check.sh")
        sftp.chmod("/tmp/qnx_bringup_check.sh", 0o755)
        sftp.close()
        say(f"uploaded {os.path.basename(local)}")
    except Exception as exc:  # noqa: BLE001
        say(f"upload failed: {exc}")
        client.close()
        say.close()
        return 1

    say()
    say("--- audit output ---")
    say()

    # Run it in blocks so a hang in one section cannot lose the others.
    blocks = ["plat", "net", "ser", "spi", "can", "gpio", "deps"]
    for block in blocks:
        say(f"########## block: {block} ##########")
        cmd = f"sh /tmp/qnx_bringup_check.sh {block} 2>&1"
        try:
            _, stdout, stderr = client.exec_command(cmd, timeout=90)
            out = stdout.read().decode("utf-8", "replace")
            err = stderr.read().decode("utf-8", "replace")
            for line in out.rstrip().splitlines():
                say(line)
            if err.strip():
                for line in err.rstrip().splitlines():
                    say(f"[stderr] {line}")
        except Exception as exc:  # noqa: BLE001
            say(f"[block {block} failed] {type(exc).__name__}: {exc}")
        say()

    # Extra probes the shell script cannot express reliably.
    say("########## extra probes ##########")
    probes = [
        ("qconn / network stack", "pidin ar 2>/dev/null | grep -iE 'io-sock|qconn' | head -5"),
        ("SPI driver detail", "pidin ar 2>/dev/null | grep -i spi | head -5"),
        ("gpio utility present", "ls -l /system/bin/gpio-bcm2711 2>&1 | head -2"),
        ("can utilities present", "ls /system/bin/can* 2>&1 | head -5"),
        ("canctl present", "command -v canctl 2>&1; ls /system/bin/cantcl 2>&1 | head -2"),
        ("/dev/mem access", "ls -l /dev/mem 2>&1; head -c 4 /dev/mem > /dev/null 2>&1 && echo 'readable' || echo 'blocked'"),
        ("sudo available", "command -v sudo 2>&1; sudo -n true 2>&1 | head -2"),
        ("tracelogger present", "command -v tracelogger 2>&1"),
        ("root fs space", "df -h 2>&1 | head -8"),
        ("CPU governor / clock", "pidin -F '%a %b' info 2>&1 | head -4"),
    ]
    for title, cmd in probes:
        say(f"--- {title} ---")
        try:
            _, stdout, stderr = client.exec_command(cmd, timeout=45)
            out = stdout.read().decode("utf-8", "replace").rstrip()
            err = stderr.read().decode("utf-8", "replace").rstrip()
            if out:
                for line in out.splitlines():
                    say(f"  {line}")
            if err:
                for line in err.splitlines()[:3]:
                    say(f"  [stderr] {line}")
            if not out and not err:
                say("  (no output)")
        except Exception as exc:  # noqa: BLE001
            say(f"  [failed] {type(exc).__name__}: {exc}")
        say()

    client.close()

    say("=" * 60)
    say(" AUDIT COMPLETE")
    say(f" report: {REPORT}")
    say("=" * 60)
    say.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())