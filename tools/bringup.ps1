<#
    Schedulix — QNX Pi 4 bring-up launcher  (Windows)

    Plug in the Ethernet cable, run this, unplug when it finishes.
    Everything it learns is written to bringup_report.txt so you can
    paste it back after you switch to WiFi.

    Usage:   powershell -ExecutionPolicy Bypass -File bringup.ps1
    Or edit $PiIp below if the address changes.
#>

$ErrorActionPreference = "Continue"
$Repo    = "C:\Users\User\ide-8.0.3-workspace\schedulix_can"
$PiIp    = "192.168.10.5"
$PiUser  = "qnxuser"          # BSP reference image uses "root"
$Report  = "$Repo\bringup_report.txt"

Remove-Item $Report -ErrorAction SilentlyContinue
function Say($m) {
    Write-Host $m
    Add-Content -Path $Report -Value $m
}

Say "=========================================================="
Say " Schedulix QNX bring-up   target: $PiUser@$PiIp"
Say " $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
Say "=========================================================="
Say ""

# ---------------------------------------------------------------- 1. ping
Say "--- [1/5] reachability ---"
$ping = Test-Connection -Target $PiIp -Count 2 -Quiet -ErrorAction SilentlyContinue
if ($ping) {
    Say "  OK   $PiIp responds to ICMP"
} else {
    Say "  GAP  $PiIp does not respond to ICMP"
    Say "       - Is the board powered?"
    Say "       - Is the Ethernet link light on?"
    Say "       - Is the SD card inserted?"
    Say "       Continuing anyway - the board may block ICMP."
}
Say ""

# ---------------------------------------------------------------- 2. ssh
Say "--- [2/5] SSH login as $PiUser ---"
ssh -o ConnectTimeout=10 -o StrictHostKeyChecking=no $PiUser@$PiIp "echo SSH_OK; hostname" 2>&1 |
    ForEach-Object { Say "  $_" }
Say ""
Say "  (If the above failed, try:  ssh root@$PiIp )"
Say ""

# ---------------------------------------------------------------- 3. copy
Say "--- [3/5] copy files to the board ---"
$files = @{
    "$Repo\tools\qnx_bringup_check.sh"        = "/tmp/qnx_bringup_check.sh"
    "$Repo\tools\spi.conf.mcp2515"            = "/tmp/spi.conf.mcp2515"
    "$Repo\tools\qnx_can_install.sh"          = "/tmp/qnx_can_install.sh"
    "$Repo\third_party\can-mcp2515\aarch64le\bin\can-mcp2515" = "/tmp/can-mcp2515"
}
foreach ($src in $files.Keys) {
    if (-not (Test-Path $src)) {
        Say "  MISSING LOCAL FILE: $src"
        continue
    }
    $dst = $files[$src]
    Say "  -> $dst"
    scp -o ConnectTimeout=10 -o StrictHostKeyChecking=no $src "${PiUser}@${PiIp}:$dst" 2>&1 |
        ForEach-Object { if ($_ -notmatch "^\s*$") { Say "     $_" } }
}
Say ""

# ---------------------------------------------------------------- 4. audit
Say "--- [4/5] bring-up audit ---"
Say "  Full output follows."
Say ""
ssh -o ConnectTimeout=10 -o StrictHostKeyChecking=no $PiUser@$PiIp "sh /tmp/qnx_bringup_check.sh" 2>&1 |
    ForEach-Object { Say $_ }
Say ""

# ---------------------------------------------------------------- 5. deps
Say "--- [5/5] dependency check for the CAN install ---"
ssh -o ConnectTimeout=10 -o StrictHostKeyChecking=no $PiUser@$PiIp `
    "echo -n 'write /tmp     : '; ([ -w /tmp ] && echo YES) || echo NO; " `
    "echo -n 'write /system  : '; ([ -w /system/bin ] && echo YES) || echo NO; "`
    "echo -n 'id             : '; id; " `
    "echo -n 'SPI driver     : '; (pidin ar 2>/dev/null | grep -c spi); "`
    "echo -n 'dev/mem        : '; ([ -r /dev/mem ] && echo readable) || echo 'not readable'; "`
    "echo -n 'free /tmp      : '; (df -h /tmp 2>/dev/null | tail -1)" 2>&1 |
    ForEach-Object { Say $_ }
Say ""

Say "=========================================================="
Say " SESSION COMPLETE"
Say " Report saved to: $Report"
Say ""
Say " NEXT:"
Say "   1. Unplug the Ethernet cable, reconnect WiFi"
Say "   2. Paste the contents of bringup_report.txt"
Say "   3. Do NOT run the CAN installer yet - read the audit first"
Say "=========================================================="
