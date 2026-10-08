# Schedulix — Quick Runbook

Everything you need to get onto the board and run things. Short, current, no history.

Board: **QNX 8.0.0**, Raspberry Pi 4, IP **192.168.10.5**, hostname **qnxpi**.

---

## 1. Passwords

| Where | User | Password |
| --- | --- | --- |
| SSH | `qnxuser` | `qnxuser` |
| SSH (after enabling) | `root` | `root` |
| Serial console | `root` | *(none — no password asked)* |

Change any of them with `passwd`.

---

## 2. Getting onto the board

### Option A — SSH (normal way, needs Ethernet)

```powershell
ssh -m hmac-sha2-256 qnxuser@192.168.10.5
```

Enter `qnxuser` at the prompt.

**The `-m hmac-sha2-256` is mandatory.** The board's sshd lists broken `-etm` MAC
algorithms first, so a default connection fails with `Corrupted MAC on input`.

Stop typing it — create `C:\Users\User\.ssh\config` once. **One directive per line:**

```
Host qnxpi
   HostName 192.168.10.5
   User qnxuser
   MACs hmac-sha2-256
```

Then just `ssh qnxpi`.

**Trap:** do **not** write `Host qnxpi 192.168.10.5` on a single line. That is
parsed as *two host patterns*, so SSH matches nothing useful and silently falls
back to your local Windows username. Until the config is right, use the
explicit form `ssh -m hmac-sha2-256 qnxuser@192.168.10.5`.

### Option B — Serial console (no network needed)

Plug the USB-TTL in **before** powering the board. Press Enter. You land at
`root@console:/#` as root, no password.

Use this whenever SSH is broken — it is the recovery path.

### Option C — Momentics IDE Run button

Build, deploy and run from the IDE. Deploys over `qconn`, not SSH, so it works
regardless of the MAC problem. Output appears in the IDE console.

Set **program arguments** to choose what runs, e.g. `gpio test`.

---

## 3. Becoming root

Over SSH:

```bash
su
```

Enter **`root`** (not `qnxuser` — that is why it says `Authentication error`).

Over serial you are already root.

### Enable root over SSH permanently

Once root:

```bash
sed -i 's/^#*PermitRootLogin.*/PermitRootLogin yes/' /usr/etc/ssh/sshd_config
slay -f sshd
/usr/bin/sshd -f /usr/etc/ssh/sshd_config
```

Then `ssh root@qnxpi` works. Needed for `mmap(MAP_PHYS)` and the CAN installer.

---

## 4. Moving files to the board

### SCP — the normal way

```powershell
scp -o "MACs=hmac-sha2-256" <local-file> qnxpi:/tmp/
```

The `-o` flag is required for the same MAC reason. Use an absolute path on the
target; a `~/bin` directory has not been verified to exist.

### Serial upload — when SSH is down

```powershell
powershell -ExecutionPolicy Bypass -File tools\serial_console.ps1 `
  -UploadFile <local-file> -RemotePath /tmp/filename
```

Slow (≈1 min per 160 KB) and unreliable above ~330 KB. **Prefer SCP.**

---

## 5. Building

QNX SDP 8.0 lives at `C:\Users\User\qnx800`. (`C:\QNX` is only the IDE — not a compiler.)

```powershell
cd C:\Users\User\ide-8.0.3-workspace\schedulix_can
$env:QNX_HOST   = 'C:\Users\User\qnx800\host\win64\x86_64'
$env:QNX_TARGET = 'C:\Users\User\qnx800\target\qnx'
$env:PATH       = "$env:QNX_HOST\usr\bin;$env:QNX_TARGET\usr\bin;$env:PATH"

& "$env:QNX_HOST\usr\bin\make.exe" rebuild
```

Output: `build/aarch64le-debug/schedulix_can`.

**Do not deploy from `deploy/`** — every binary there is stale and predates the
`uart` and `gpio` subcommands.

Optional, shrinks the binary for upload:

```powershell
& "$env:QNX_HOST\usr\bin\ntoaarch64-strip.exe" build\aarch64le-debug\schedulix_can
```

---

## 6. Commands on the board

### Check the system is alive

| Command | What it tells you |
| --- | --- |
| `id` | Who you are and your privileges |
| `uname -a` | OS, version, hardware |
| `pidin info` | CPU count, memory, uptime |
| `ifconfig genet0` | The board's IP address |
| `df -h /tmp` | Free space on `/tmp` |

### Run the analyzer

| Command | What it does |
| --- | --- |
| *(no args)* | CAN simulation demo — five hardcoded frames. **Does not touch GPIO.** |
| `gpio test` | Toggles all three marker pins 200 ms apart, reports `fsel`, readback and pulse width |
| `uart status` | Shows whether the UART adapter got a real device or fell back to simulated |
| `uart send <data>` | Transmits over `/dev/ser1` |
| `--manifest` | Prints the workload table |
| `--workloads` | Spawns the three ECU workloads |
| `--full` | Workloads + trace + CAN + analysis |
| `--stress <id>` | Stress scenario S0–S6 |
| `--sweep` | CPU load sweep 20–95 % |

### GPIO verification

```bash
gpio-bcm2711 get 17
```

`fsel=1` means output, `fsel=0` means input. `gpio test` should change it to `1`.

---

## 7. Hardware wiring

### Serial console (already done)

```
Pi pin 6  (GND)     -> adapter GND
Pi pin 8  (GPIO14)  -> adapter RX          (Pi transmits)
adapter TX          -> 330R -> tap -> Pi pin 10 (GPIO15)
                             tap
                            330R
                             |
                            330R -> adapter GND
```

TX/RX **must cross over**. Pins 1 and 2 (3V3/5V) stay **unconnected**.

### LED marker test

```
Pi pin 11 (GPIO 17) -> 330R -> LED long leg (+)
LED short leg (-)   -> Pi pin 6 (GND)
```

The resistor is mandatory — a bare LED will destroy the pin.

Use pin 11, not pin 7. GPIO 4 is the SoC's **TXD3** pin and may be claimed by a
driver.

### CAN HAT — not yet mounted

Solder the marker wires to pins **7, 11, 13** *before* fitting the HAT; the
40-pin HAT covers them.

---

## 8. Troubleshooting

### SSH fails

| Message | Cause | Fix |
| --- | --- | --- |
| `Algorithm negotiation fail` | **Momentics IDE terminal only** — its client offers no MAC the board accepts, and it has no way to pass `-m` | Use an external `ssh`; the IDE Run button is unaffected |
| `Corrupted MAC on input` | Board picked a broken `-etm` MAC | Add `-m hmac-sha2-256` |
| `Permission denied (publickey,keyboard-interactive)` | `PermitRootLogin no` | Use `qnxuser`, or enable root as in §3 |
| `su: Authentication error` | Wrong password for root | Use `root`, not `qnxuser` |
| `No route to host` | Wrong network adapter | See below |

### Two Ethernet adapters

The PC has both `192.168.10.1` (board) and `192.168.56.1`. The second confuses
routing:

```powershell
Get-NetIPAddress -AddressFamily IPv4 | Where-Object { $_.IPAddress -notlike '127.*' }
Disable-NetAdapter -Name "Ethernet 2" -Confirm:$false
```

Run PowerShell commands in PowerShell, not Git Bash.

### Serial shows nothing

Power-cycle with the USB-TTL plugged in **first**. The console can end up bound
to nothing if the board boots before the adapter is attached.

---

## 9. Scripts in `tools/`

**These are QNX shell scripts. Transfer them to the board and run them there.**

```bash
sh /tmp/qnx_bringup_check.sh      # overall bring-up state
sh /tmp/qnx_bringup_check.sh can  # just the CAN block
sh /tmp/qnx_ssh_diag.sh           # why SSH is failing
```

Running them in Git Bash on Windows produces garbage — MSYS maps `/tmp` to
`C:\Users\User\AppData\Local\Temp`, so every check reports a false failure.

---

## 10. Where things stand

| Phase | Status |
| --- | --- |
| 1 — UART adapter | Done. Physical device, TX proven. RX confounded (`/dev/ser1` is the console). |
| 2 — GPIO markers | **Verified on hardware.** `mmap(MAP_PHYS)` works, pins switch to output, readback confirms, LED observed. |
| 3 — UART TX trace marker | Next. `uart_adapter.c:151` records a transmit as `TRACE_EXTERNAL_EVENT_RX`. |
| 4 — CAN driver install | Unblocked now that SCP works. Needs the HAT mounted + crystal frequency. |

Known gotchas: deploy only from `build/`, never `deploy/`. `/tmp` currently holds
only stale binaries plus debris from failed serial uploads.