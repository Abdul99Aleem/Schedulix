# Incident — SPI driver will not start (Phase 4 bring-up)

**Status:** OPEN — root cause unresolved
**Target:** QNX 8.0.0 `qnxpi`, Raspberry Pi 4, `192.168.10.5`
**Impact:** Phase 4 blocked. `/dev/can0` never created. **A power blockage on
the target hardware then halted further on-target verification.** Phases 1–3
are unaffected and all their evidence stands.

---

## 1. Summary

The Phase 4 installer completed steps 1–3 successfully, then failed at step 4
because `spi-bcm2711` would not start.

**The failure is NOT caused by the `spi.conf` we wrote.** Restoring the
original stock file reproduces the failure exactly. That single experiment
eliminated the leading hypothesis and is the most useful result in this
incident.

While diagnosing it, the target board entered a power blockage: it stopped
booting its OS and became unreachable over Ethernet and serial. Remaining
on-target work could not continue. Everything needed to resume is committed to
the repository and documented in [BRINGUP_GUIDE.md](BRINGUP_GUIDE.md).

---

## 2. Verified working before this incident

| Item | Evidence |
| --- | --- |
| Serial console, root shell | `root@console:/#`, passwordless, 115200 8N1 |
| SSH as `qnxuser` | `ssh -m hmac-sha2-256 qnxuser@192.168.10.5` |
| SSH as `root` | after `PermitRootLogin yes` |
| GPIO markers | `REAL PHYSICAL`, fsel 0→1 on all 3 pins, **LED observed**, 200.9 ms vs 200 ms nominal |
| UART | `uart status` → `physical`, `/dev/ser1` @ 115200, 21 bytes on the wire before its own `printf` |
| `can-mcp2515` build | 148,840 bytes, from commit `0fd11af` |
| Crystal frequency | `EAS12.000` = **12 MHz**, read directly off the board |

---

## 3. Timeline

### Step A — installer's root check could never pass (installer bug, fixed)

```
root@qnxpi:/data/home/qnxuser# sh /tmp/qnx_can_install.sh
ERROR: must run as root. QSTI has no sudo ...
```

**Cause:** the guard compared the whole `id` output against a literal string:

```sh
[ "$(id)" = "root" ] || die "must run as root"
```

`$(id)` expands to `uid=0(root) gid=0(root) groups=0(root)`, which never
equals `root`. **The check could never pass, for anyone** — including root.
Fixed to `[ "$(id -u)" = "0" ]`.

**Lesson:** the command had been documented as the procedure for several
sessions without anyone ever running it. Documented-as-working and
verified-working are different things.

### Step B — steps 1–3 succeeded

```
== 1/8 verify prerequisites
   SPI driver: 2 instance(s)

== 2/8 install driver binary
-rwxr-xr-x  1 root root 148840 1970-01-01 00:11 /system/bin/can-mcp2515

== 3/8 rewrite spi.conf for MCP2515
   backed up stock spi.conf
```

The installed binary matched the locally built one at 148,840 bytes.

### Step C — step 4 failed

```
== 4/8 restart the SPI driver to pick up new settings
ERROR: /dev/io-spi/spi0/dev0 did not come back after SPI restart
```

### Step D — suspected our own config (hypothesis was WRONG)

The rewritten file was smaller than stock (556 B vs 651 B) because it **deleted
two sections**:

| | Stock | Our version |
| --- | --- | --- |
| `spi0/dev0` | `cpha=1 word_width=32 clock=5M` | `cpha=0 word_width=8 clock=10M` |
| `spi0/dev1` | present | **deleted** |
| `spi3` bus (`base=0xfe204600 irq=151`) + dev | present | **deleted** |

A real defect — but not the cause.

### Step E — stock config restored, driver STILL fails (decisive)

```
root@qnxpi:# cp /system/etc/config/spi/spi.conf.stock.681 /system/etc/config/spi/spi.conf
root@qnxpi:# spi-bcm2711 /system/etc/config/spi/spi.conf &
[1] 1548358
root@qnxpi:# ls -l /dev/io-spi/spi0/
ls: /dev/io-spi/spi0/: No such file or directory
[1]+  Exit 1                  spi-bcm2711 /system/etc/config/spi/spi.conf
```

**Exit 1, immediately, no output at all** — despite `verbose=5` in the config.
The driver never reached config parsing.

**This rules out the config.** Whatever is wrong is in the driver's startup.

### Step F — board degradation

SSH sessions began dropping (`Connection reset by peer`), then PTY allocation
started failing, then the board entered a power blockage (section 5).

---

## 4. New finding from the QNX CAN DDK

Retrieved later from the official documentation:

<https://www.qnx.com/developers/docs/qnxeverywhere/com.qnx.doc.ddk/topic/can/sample_quickstart.html>

> **"Reboot your target device, wait for the reboot to complete, and log in."**

The DDK's procedure for applying an `spi.conf` change is to **reboot the
target**. Our installer instead did `slay spi-bcm2711` followed by a manual
`spi-bcm2711 &`. That is not the documented method, and a manually restarted
resource manager is a plausible way to hit `Exit 1` with no message — the
process may collide with state the normal boot sequence would have
initialised.

**This does not explain the stock-config failure**, because the stock file was
tested *after* several manual restarts had already left the driver in an
unknown state. It does mean the restart-based approach may itself have been
contributing.

**Action taken:** the installer now treats a reboot as the supported path.
Step 4 prints the `shutdown && reboot` instruction, attempts the in-place
restart only as a convenience, and — if `dev0` does not reappear — exits with
code **2** and an explicit "reboot and re-run" message rather than a bare
failure. Re-running is safe.

Confirmed from the same page: `/dev/can0` exposes `rx0`, `rx1`, `tx2`, `tx3`,
`tx4`, and with `--mid=eid` their MIDs are `0`–`4`.

---

## 5. The power blockage

### What was observed

| Symptom | Reading |
| --- | --- |
| HAT power LED, first power-up after fitting the HAT | solid on |
| HAT power LED, next power cycle | blinked, went off |
| HAT power LED, later cycles | off, did not even blink |
| Pi red LED | on — 5 V rail present |
| Pi green activity LED | never lit — OS not booting |
| SoC temperature | at ambient — processor not executing |
| Ethernet | no link; PC reports `Ethernet Disconnected 0 bps` |
| `ping` | `General failure` / `Destination host unreachable` |

**Interpretation.** 5 V present but the SoC never executing, with the 3.3 V-fed
HAT LED dark across three power cycles, is consistent with the 3.3 V rail
collapsing. The Pi 4 generates 3.3 V from 5 V in its power management chip, and
that rail powers the SoC core — so with it gone, the bootloader never runs, the
green LED never lights, and the CPU stays cold. A red LED still lights because
it is fed from 5 V.

The progressive signature — **working, then degrading, then dead** across three
power cycles — is characteristic of a short developing on a rail rather than a
clean component failure.

### Contributing factor: hand-wired SPI signals

The MCP2515 HAT's SPI signals were connected with individual jumper wires
rather than seating the board on the 40-pin header. That creates a direct path
for a supply or ground connection onto MISO/MOSI/SCK/CS/INT. A 40-pin HAT is
designed to seat directly on the header, where the silkscreen guarantees the
pin mapping; hand-wiring removes that guarantee.

**Standing guidance, now in [BRINGUP_GUIDE.md §12.1](BRINGUP_GUIDE.md): seat the
HAT on the header. Do not hand-wire it.**

### Confirmed by measurement?

No. The definitive test is a multimeter on pin 1 to pin 6:

| Reading | Verdict |
| --- | --- |
| ~3.3 V | Rail intact — look elsewhere |
| ~0 V | PMIC fault |

No multimeter was available, so the 3.3 V rail reading is **inferred from the
LED pattern and SoC temperature, not measured.** It is recorded here as the
leading explanation, not as a confirmed diagnosis.

### Recovery state

Everything needed to resume is committed and pushed:

- Driver built and committed: `third_party/can-mcp2515/aarch64le/bin/can-mcp2515` (148,840 bytes)
- Installer defects fixed and regression-tested
- Stock `spi.conf` preserved on the board as `spi.conf.stock.681`
- Full resume procedure in [BRINGUP_GUIDE.md](BRINGUP_GUIDE.md) §9

No software work was lost. Phases 1–3 evidence remains valid.

---

## 6. "PTY allocation request failed on channel 0"

```
$ ssh qnxuser@192.168.10.5
(qnxuser@192.168.10.5) Password:
PTY allocation request failed on channel 0
Connection to 192.168.10.5 closed.
```

**Authentication succeeded.** The failure happens afterwards, when the board
cannot allocate a pseudo-terminal to run an interactive shell. This is a
resource-exhaustion symptom, **not** a credential or SSH problem.

QNX allocates a `ptyp*`/`ttyp*` pair per session; repeated failed sessions and
the backgrounded `spi-bcm2711 &` jobs leaked them. Related on-target checks:

```sh
pidin info
free
```

A power cycle clears it. Recorded here because the error message invites the
wrong diagnosis — the password was accepted.

---

## 7. Outstanding diagnostics

Not yet run — the power blockage intervened.

```sh
# THE key one - QNX drivers log to slog, not the terminal
slog2info | grep -i spi | tail -30

# Is anything holding the SPI peripheral?
pidin ar | grep -iE 'spi|mbox'

# Does the mailbox driver exist and run?
ls -l /dev/mailbox*

# Reboot-based apply, per the DDK, rather than a manual restart
shutdown && reboot
# then, after login:
ls -l /dev/io-spi/spi0/
```

The slog output is the highest-value item. `Exit 1` with zero terminal output
while `verbose=5` is set means the driver bailed **before config parsing**, and
slog is where that would be recorded.

---

## 8. Ruled out — do not re-investigate

| Suspect | Verdict |
| --- | --- |
| The rewritten `spi.conf` | **Ruled out** — the stock file fails identically |
| Crystal frequency | **Ruled out** — `EAS12.000` = 12 MHz, read off the board |
| Driver binary | **Unlikely** — installed at the expected 148,840 bytes; the failure is upstream of the CAN driver entirely |
| Missing `/dev/mem` | Unrelated — that was the GPIO Phase 2 issue, resolved |
| Credentials / SSH | **Ruled out** — authentication succeeds; failure is after |
| The `spi3` / `spi0/dev1` deletion | **Not the cause** — stock config fails the same way. Real defect, now fixed |

---

## 9. Defects found and fixed during this incident

Independent of the unresolved SPI failure, three genuine bugs surfaced:

| Defect | Fix | Guard |
| --- | --- | --- |
| Root check could never pass | `[ "$(id -u)" = "0" ]` | — |
| `spi.conf` replaced wholesale, dropping `spi0/dev1` and the whole `spi3` bus | Section-aware awk edit touching only `spi0/dev0` | `tests/test_spi_conf_edit.py`, 13/13 |
| Backup re-stamped on every run, so a second run overwrote the stock copy with the already-modified file | Back up once, then keep the original | — |

Plus one process correction: the driver restart is no longer presented as the
supported way to apply `spi.conf`. The DDK specifies a reboot, and the
installer now says so.

---

## 10. Impact

| Phase | Status |
| --- | --- |
| 1 — UART adapter | ✅ Done, verified on hardware |
| 2 — GPIO markers | ✅ Done, verified on hardware, LED observed |
| 3 — UART TX trace marker | ✅ Done, compile- and test-verified |
| **4 — CAN driver install** | **🔴 Blocked at step 4/8** |
| 5 — `qnx_can_adapter.c` | Not started — `.stub` not enabled; needs `/dev/can0/tx2` and `/dev/can0/rx0` |
| 6 — Qt JSON loader | Not started — GUI still on `MockProvider` |

---

## 11. Next actions on a working board

1. **Seat the CAN HAT on the 40-pin header.** Do not hand-wire it.
2. Run `slog2info | grep -i spi | tail -30` **first** — highest-value diagnostic.
3. Apply `spi.conf` changes with `shutdown && reboot`, not a manual restart.
4. Re-run `sh /tmp/qnx_can_install.sh`; it is idempotent and safe to re-run.
5. Confirm `/dev/can0/rx0 rx1 tx2 tx3 tx4` appears.
6. If frames still do not flow, re-read the crystal marking (§9.2 of the guide).

Full procedure: [BRINGUP_GUIDE.md](BRINGUP_GUIDE.md)