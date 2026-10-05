#!/usr/bin/env python3
"""
Schedulix - QNX Pi bring-up over SSH using paramiko.

Paramiko is used instead of Windows OpenSSH because QNX sshd advertises legacy
algorithms (SHA-1 MACs, diffie-hellman-group1, CBC ciphers, ssh-rsa host keys)
that OpenSSH 9.x refuses by default with no clean way to re-enable.

This script does four things:
  1. probe      - find a working (user, algorithm) combination, report WHY others fail
  2. audit      - upload and run qnx_bringup_check.sh
  3. install    - upload and run qnx_can_install.sh
  4. report     - upload schedulix_can binary

Usage:
    python qnx_ssh.py probe
    python qnx_ssh.py audit
    python qnx_ssh.py install
    python qnx_ssh.py report

Credentials: set SCHEDULIX_PI_PASSWORD, or pass --password.
If empty, the script tries a small list of common QNX defaults.
"""

from __future__ import annotations

import argparse
import getpass
import os
import posixpath
import stat
import sys

import paramiko

HOST = os.environ.get("SCHEDULIX_PI_HOST", "192.168.10.5")
REPO = r"C:\Users\User\ide-8.0.3-workspace\schedulix_can"
TIMEOUT = 12

USERS = ["qnxuser", "root"]

# Default passwords to try when none is supplied.
PASSWORDS = [
    None,          # keyboard-interactive / trusted key / no password
    "qnxuser",
    "root",
    "",
]

# Algorithm sets, tried in order.
ALGO_SETS: list[tuple[str, dict]] = [
    ("paramiko default", {}),
    (
        "etm MACs + legacy kex",
        {
            "disabled_algorithms": {
                "keys": [],
                "macs": [
                    "hmac-sha2-512-etm@openssh.com",
                    "hmac-sha2-256-etm@openssh.com",
                    "hmac-sha2-512",
                    "hmac-sha2-256",
                ],
            }
        },
    ),
    (
        "sha1 MACs + group1 kex + ssh-rsa",
        {
            "disabled_algorithms": {
                "keys": ["ssh-ed25519", "ecdsa-sha2-nistp256"],
                "macs": [
                    "hmac-sha2-512-etm@openssh.com",
                    "hmac-sha2-256-etm@openssh.com",
                    "hmac-sha2-512",
                    "hmac-sha2-256",
                ],
                "kex": [
                    "curve25519-sha256",
                    "curve25519-sha256@libssh.org",
                    "ecdh-sha2-nistp521",
                    "ecdh-sha2-nistp384",
                    "ecdh-sha2-nistp256",
                    "diffie-hellman-group-exchange-sha256",
                ],
            }
        },
    ),
    (
        "maximum legacy",
        {
            "disabled_algorithms": {
                "keys": ["ssh-ed25519", "ecdsa-sha2-nistp256", "ecdsa-sha2-nistp384",
                         "ecdsa-sha2-nistp521"],
                "macs": [
                    "hmac-sha2-512-etm@openssh.com",
                    "hmac-sha2-256-etm@openssh.com",
                ],
                "kex": [
                    "curve25519-sha256",
                    "ecdh-sha2-nistp521",
                    "ecdh-sha2-nistp384",
                    "ecdh-sha2-nistp256",
                    "diffie-hellman-group18-sha512",
                    "diffie-hellman-group17-sha512",
                    "diffie-hellman-group16-sha512",
                ],
            }
        },
    ),
    (
        "no algorithms disabled (force all)",
        {
            "disabled_algorithms": {
                "keys": [],
                "macs": [],
                "kex": [],
                "ciphers": [],
            }
        },
    ),
]


def log(msg: str = "") -> None:
    print(msg, flush=True)


def host_key_policy():
    # Accept the board's key without prompting. QNX images regenerate keys on
    # reflashing, so a pinned key would break every re-run.
    return paramiko.AutoAddPolicy()


def try_connect(user: str, password: str | None, opts: dict):
    """Attempt one connection. Returns (client, None) or (None, reason)."""
    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(host_key_policy())
    kwargs = {
        "hostname": HOST,
        "username": user,
        "timeout": TIMEOUT,
        "auth_timeout": TIMEOUT,
        "banner_timeout": TIMEOUT,
        "look_for_keys": False,
        "allow_agent": False,
    }
    if password is not None:
        kwargs["password"] = password
    if opts:
        kwargs.update(opts)
    try:
        client.connect(**kwargs)
        return client, None
    except paramiko.AuthenticationException as exc:
        client.close()
        return None, f"auth rejected: {exc}"
    except paramiko.BadHostKeyException as exc:
        client.close()
        return None, f"bad host key: {exc}"
    except paramiko.SSHException as exc:
        client.close()
        return None, f"{type(exc).__name__}: {exc}"
    except Exception as exc:  # noqa: BLE001 - socket errors, EOF, etc.
        try:
            client.close()
        except Exception:
            pass
        return None, f"{type(exc).__name__}: {exc}"


def probe(passwords: list[str | None]) -> tuple[str, str | None, dict] | None:
    log("=== probing for a working SSH combination ===")
    log(f"target {HOST}")
    log("")
    for name, opts in ALGO_SETS:
        for user in USERS:
            for pw in passwords:
                label = "none" if pw is None else (pw if pw else "<empty>")
                client, err = try_connect(user, pw, opts)
                if client:
                    log(f"  SUCCESS  user={user:<8} pwd={label:<10} algo={name}")
                    return user, pw, opts
                log(f"  fail     user={user:<8} pwd={label:<10} algo={name}")
                log(f"             {err}")
    return None


def run(client, cmd: str, timeout: int = 90) -> tuple[int, str, str]:
    """Run a remote command. Returns (status, stdout, stderr)."""
    _, stdout, stderr = client.exec_command(cmd, timeout=timeout)
    out = stdout.read().decode("utf-8", "replace")
    err = stderr.read().decode("utf-8", "replace")
    status = stdout.channel.recv_exit_status()
    return status, out, err


def put(client, local: str, remote: str, mode: int = 0o755) -> bool:
    """Upload a file, then chmod it on the target."""
    if not os.path.isfile(local):
        log(f"  MISSING LOCAL FILE: {local}")
        return False
    sftp = client.open_sftp()
    try:
        sftp.put(local, remote)
        sftp.chmod(remote, mode)
    finally:
        sftp.close()
    log(f"  uploaded {local} -> {remote}")
    return True


def banner(c: paramiko.SSHClient) -> None:
    """Print what the board says about itself."""
    _, out, _ = run(c, "pidin info 2>/dev/null | head -12; echo '---'; uname -a 2>/dev/null; echo '---'; id")
    log(out.rstrip())


def cmd_probe(c, _a):
    banner(c)


def cmd_audit(c, _a):
    src = os.path.join(REPO, "tools", "qnx_bringup_check.sh")
    log("=== uploading audit script ===")
    if not put(c, src, "/tmp/qnx_bringup_check.sh"):
        return 1
    log("")
    log("=== bring-up audit ===")
    log("")
    status, out, err = run(c, "sh /tmp/qnx_bringup_check.sh", timeout=180)
    log(out.rstrip())
    if err.strip():
        log("")
        log("--- stderr ---")
        log(err.rstrip())
    log("")
    log(f"audit exit status: {status}")
    return status


def cmd_install(c, _a):
    uploads = [
        (os.path.join(REPO, "third_party", "can-mcp2515", "aarch64le", "bin", "can-mcp2515"),
         "/tmp/can-mcp2515", 0o755),
        (os.path.join(REPO, "tools", "spi.conf.mcp2515"), "/tmp/spi.conf.mcp2515", 0o644),
        (os.path.join(REPO, "tools", "qnx_can_install.sh"), "/tmp/qnx_can_install.sh", 0o755),
    ]
    log("=== uploading CAN driver and config ===")
    for src, dst, mode in uploads:
        if not put(c, src, dst, mode):
            return 1
    log("")
    log("=== installing and starting can-mcp2515 ===")
    log("")
    status, out, err = run(c, "sh /tmp/qnx_can_install.sh", timeout=300)
    log(out.rstrip())
    if err.strip():
        log("")
        log("--- stderr ---")
        log(err.rstrip())
    log("")
    log(f"install exit status: {status}")
    return status


def cmd_report(c, _a):
    cands = [
        os.path.join(REPO, "build", "aarch64le-debug", "schedulix_can"),
        os.path.join(REPO, "deploy", "schedulix_can-aarch64le-debug"),
        os.path.join(REPO, "deploy", "schedulix_can"),
    ]
    src = next((p for p in cands if os.path.isfile(p)), None)
    if not src:
        log("No schedulix_can binary found. Tried:")
        for p in cands:
            log(f"    {p}")
        log("Build it first:  make PLATFORM=aarch64le BUILD_PROFILE=debug")
        return 1
    log(f"=== deploying {src} ===")
    if not put(c, src, "/tmp/schedulix_can"):
        return 1
    status, out, err = run(c, "/tmp/schedulix_can status", timeout=60)
    log("")
    log(out.rstrip())
    if err.strip():
        log("--- stderr ---")
        log(err.rstrip())
    return status


COMMANDS = {
    "probe": cmd_probe,
    "audit": cmd_audit,
    "install": cmd_install,
    "report": cmd_report,
}


def get_passwords(override: str | None) -> list[str | None]:
    if override is not None:
        return [override]
    env = os.environ.get("SCHEDULIX_PI_PASSWORD")
    if env is not None:
        return [env]
    print("No password supplied. Trying common QNX defaults.")
    print("If none work, re-run with:  python qnx_ssh.py audit --password <pw>")
    return PASSWORDS


def main() -> int:
    global HOST

    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("command", choices=sorted(COMMANDS))
    ap.add_argument("--host", default=HOST)
    ap.add_argument("--password", default=None)
    ap.add_argument("--user", default=None)
    args = ap.parse_args()

    HOST = args.host

    log("")
    log("Schedulix QNX bring-up (paramiko transport)")
    log(f"host {HOST}   user-supplied password: "
        f"{'yes' if args.password or os.environ.get('SCHEDULIX_PI_PASSWORD') else 'no'}")
    log("")

    passwords = [args.password] if args.user else get_passwords(args.password)
    users = [args.user] if args.user else USERS

    found = None
    for name, opts in ALGO_SETS:
        for user in users:
            for pw in passwords:
                label = "none" if pw is None else (pw if pw else "<empty>")
                client, err = try_connect(user, pw, opts)
                if client:
                    log(f"  SUCCESS  user={user:<8} pwd={label:<10} algo={name}")
                    found = (client, user, pw)
                    break
                log(f"  fail     user={user:<8} pwd={label:<10} algo={name}")
                log(f"             {err}")
            if found:
                break
        if found:
            break

    if not found:
        log("")
        log("!!! No combination authenticated.")
        log("")
        log("Diagnose which stage fails:")
        log("  - 'NoValidConnectionsError' / timeout  -> board not reachable. Check power,")
        log("    Ethernet link lights, and that the SD card boots.")
        log("  - 'Error reading SSH protocol banner'   -> sshd is not running. Needs a")
        log("    serial console or monitor.")
        log("  - 'Error negotiating key exchange'      -> algorithm mismatch not covered")
        log("    by the sets above. Add one to ALGO_SETS.")
        log("  - 'Authentication failed' but other    -> password wrong. Re-run with")
        log("    combos got further                    --password.")
        log("")
        log("Serial console fallback (no network needed):")
        log("  USB-TTL TX -> pin 8, RX -> pin 10, GND -> pin 6, VCC NOT connected")
        log("  115200 8N1. Gives a root shell with no login.")
        return 2

    client, user, _pw = found
    log("")
    log(f"connected as {user}@{HOST}")
    log("")
    try:
        rc = COMMANDS[args.command](client, args)
    finally:
        client.close()
    log("")
    log(f"--- {args.command} finished, rc={rc} ---")
    return rc


if __name__ == "__main__":
    sys.exit(main())