#!/usr/bin/env python3
"""
Schedulix - install and start the MCP2515 CAN driver on the QNX Pi.

The stock QSTI image has no sudo and the root filesystem is read-only, so the
installer must run as root. This script finds a working root login (root with an
empty password is common on the QSTI image) and escalates over SSH.

    python qnx_can_install.py
    python qnx_can_install.py --root-password <pw>

It:
  1. uploads can-mcp2515, spi.conf template, and the installer
  2. runs the installer as root
  3. captures slog2info output
  4. writes everything to can_install_report.txt
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
ROOT_PW = os.environ.get("SCHEDULIX_PI_ROOT_PASSWORD", "")
REPORT = os.path.join(REPO, "can_install_report.txt")
TIMEOUT = 15

# Empty password first - that is what QSTI root usually accepts.
ROOT_CANDIDATES = ["", "root", "toor", "qnxuser"]


class Tee:
    def __init__(self, path: str) -> None:
        self.fh = open(path, "w", encoding="utf-8")

    def __call__(self, msg: str = "") -> None:
        print(msg, flush=True)
        self.fh.write(msg + "\n")
        self.fh.flush()

    def close(self) -> None:
        self.fh.close()


def connect(host: str, user: str, pw: str) -> paramiko.SSHClient:
    c = paramiko.SSHClient()
    c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    c.connect(
        hostname=host, username=user, password=pw,
        timeout=TIMEOUT, auth_timeout=TIMEOUT, banner_timeout=TIMEOUT,
        look_for_keys=False, allow_agent=False,
    )
    return c


def sh(c: paramiko.SSHClient, cmd: str, timeout: int = 240) -> tuple[int, str, str]:
    _, so, se = c.exec_command(cmd, timeout=timeout)
    out = so.read().decode("utf-8", "replace")
    err = se.read().decode("utf-8", "replace")
    return so.channel.recv_exit_status(), out, err


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", default=HOST)
    ap.add_argument("--password", default=None, help="qnxuser password")
    ap.add_argument("--root-password", default=None, help="root password (empty is common)")
    args = ap.parse_args()

    host = args.host
    user_pw = args.password if args.password is not None else PW
    root_candidates = [args.root_password] if args.root_password is not None else ROOT_CANDIDATES

    say = Tee(REPORT)
    say("=" * 62)
    say(" Schedulix - CAN driver install")
    say(f" {time.strftime('%Y-%m-%d %H:%M:%S')}   target: {host}")
    say("=" * 62)
    say()

    # ---------------------------------------------------- upload as qnxuser
    say("--- [1/4] upload files as qnxuser ---")
    try:
        c = connect(host, USER, user_pw)
    except Exception as exc:  # noqa: BLE001
        say(f"cannot connect as {USER}: {type(exc).__name__}: {exc}")
        say.close()
        return 2

    uploads = [
        (os.path.join(REPO, "third_party", "can-mcp2515", "aarch64le", "bin", "can-mcp2515"),
         "/tmp/can-mcp2515"),
        (os.path.join(REPO, "tools", "qnx_can_install.sh"), "/tmp/qnx_can_install.sh"),
    ]
    ok = True
    try:
        sftp = c.open_sftp()
        for src, dst in uploads:
            if not os.path.isfile(src):
                say(f"  MISSING: {src}")
                ok = False
                continue
            sftp.put(src, dst)
            sftp.chmod(dst, 0o755)
            say(f"  uploaded {os.path.basename(src)} -> {dst}")
        sftp.close()
    except Exception as exc:  # noqa: BLE001
        say(f"  upload failed: {type(exc).__name__}: {exc}")
        ok = False
    say()

    # ---------------------------------------------------- find root
    say("--- [2/4] locate a root login ---")
    root_pw = None
    for cand in root_candidates:
        label = "empty" if cand == "" else cand
        try:
            rc = connect(host, "root", cand)
            root_pw = cand
            say(f"  root login works with password: {label}")
            break
        except paramiko.AuthenticationException:
            say(f"  root rejected password: {label}")
        except Exception as exc:  # noqa: BLE001
            say(f"  root/{label}: {type(exc).__name__}: {exc}")
    if root_pw is None:
        say()
        say("  No root access found.")
        say("  The QSTI image root filesystem is read-only and there is no sudo,")
        say("  so the driver cannot be installed from a non-root shell.")
        say()
        say("  Options:")
        say("   1. Try a specific root password:")
        say("        python qnx_can_install.py --root-password <pw>")
        say("   2. Serial console as root (no password on serial):")
        say("        USB-TTL TX -> pin 8, RX -> pin 10, GND -> pin 6, VCC NOT connected")
        say("        115200 8N1. Then run:  sh /tmp/qnx_can_install.sh")
        say("        (the files are already in /tmp from step 1)")
        say("   3. Reboot into the BSP reference image, which allows root login.")
        c.close()
        say.close()
        return 3
    say()

    # ---------------------------------------------------- run installer
    say("--- [3/4] run installer as root ---")
    say()
    try:
        rc = connect(host, "root", root_pw)
    except Exception as exc:  # noqa: BLE001
        say(f"  root reconnect failed: {exc}")
        c.close()
        say.close()
        return 3

    status, out, err = sh(rc, "sh /tmp/qnx_can_install.sh", timeout=300)
    say(out.rstrip())
    if err.strip():
        say("")
        say("--- installer stderr ---")
        say(err.rstrip())
    say("")
    say(f"installer exit status: {status}")
    say()

    # ---------------------------------------------------- post checks
    say("--- [4/4] post-install state ---")
    checks = [
        ("CAN nodes", "ls -d /dev/can0/*/ 2>&1 | head -10"),
        ("driver process", "pidin ar 2>/dev/null | grep -i mcp"),
        ("spi.conf dev0 block", "sed -n '/^\\[dev\\]/,/^$/p' /system/etc/config/spi/spi.conf | head -12"),
        ("driver log", "slog2info 2>/dev/null | grep -iE 'mcp2515|can' | tail -20"),
    ]
    for title, cmd in checks:
        say(f"  --- {title} ---")
        _, o, e = sh(rc, cmd, timeout=60)
        o = o.rstrip()
        if o:
            for line in o.splitlines():
                say(f"    {line}")
        else:
            say("    (empty)")
        if e.strip():
            for line in e.rstrip().splitlines()[:2]:
                say(f"    [err] {line}")
        say()

    rc.close()
    c.close()

    say("=" * 62)
    say(" DONE - report: " + REPORT)
    say("=" * 62)
    say.close()
    return status


if __name__ == "__main__":
    sys.exit(main())