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
$Report  = "$Repo\bringup_report.txt"

Remove-Item $Report -ErrorAction SilentlyContinue
function Say($m) {
    Write-Host $m
    Add-Content -Path $Report -Value $m
}

# ------------------------------------------------------- SSH algorithm sets
# Modern OpenSSH (9.x) dropped SHA-1 / group1 / CBC by default. QNX sshd is
# older and only offers some of those. Try progressively looser sets until
# one authenticates.
$Algos = @(
    @{ name = "default";                 opts = "" },
    @{ name = "etm MACs";                opts = "-o MACs=+hmac-sha2-256-etm@openssh.com,hmac-sha2-512-etm@openssh.com,hmac-sha1,hmac-sha1-96,hmac-md5" },
    @{ name = "etm + group1 + ssh-rsa";  opts = "-o MACs=+hmac-sha2-256-etm@openssh.com,hmac-sha2-512-etm@openssh.com,hmac-sha1,hmac-sha1-96,hmac-md5 -o KexAlgorithms=+diffie-hellman-group1-sha1,diffie-hellman-group14-sha1,ecdh-sha2-nistp256,curve25519-sha256 -o HostKeyAlgorithms=+ssh-rsa,ssh-dss,ecdsa-sha2-nistp256 -o PubkeyAcceptedAlgorithms=+ssh-rsa" },
    @{ name = "3des + hmac-sha1";        opts = "-o Ciphers=3des-cbc,aes128-cbc,aes256-cbc -o MACs=hmac-sha1 -o KexAlgorithms=diffie-hellman-group14-sha1,diffie-hellman-group1-sha1 -o HostKeyAlgorithms=+ssh-rsa,ssh-dss" },
    @{ name = "maximum legacy";          opts = "-o Ciphers=+3des-cbc,aes128-cbc,aes192-cbc,aes256-cbc,aes128-ctr,aes192-ctr,aes256-ctr,arcfour256 -o MACs=+hmac-sha1,hmac-sha1-96,hmac-md5,hmac-md5-96,hmac-ripemd160,hmac-sha2-256,hmac-sha2-512,hmac-sha2-256-etm@openssh.com,hmac-sha2-512-etm@openssh.com -o KexAlgorithms=+diffie-hellman-group1-sha1,diffie-hellman-group14-sha1,diffie-hellman-group16-sha512,diffie-hellman-group18-sha512,ecdh-sha2-nistp256,curve25519-sha256 -o HostKeyAlgorithms=+ssh-rsa,ssh-dss,ecdsa-sha2-nistp256,ecdsa-sha2-nistp384,ecdsa-sha2-nistp521 -o PubkeyAcceptedAlgorithms=+ssh-rsa" }
)

function Try-Ssh($user, $opts) {
    $cmd = "ssh -o ConnectTimeout=8 -o StrictHostKeyChecking=no -o UserKnownHostsFile=NUL -o NumberOfPasswordPrompts=1 "
    if ($opts -ne "") { $cmd += "$opts " }
    $cmd += "${user}@${PiIp} `"echo SSH_OK; uname -a; id`" 2>&1"
    return (Invoke-Expression $cmd 2>&1 | Out-String)
}

Say "=========================================================="
Say " Schedulix QNX bring-up   target: $PiIp"
Say " $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
Say "=========================================================="
Say ""

# ---------------------------------------------------------------- 1. ping
Say "--- [1/5] reachability ---"
$ping = Test-Connection -Target $PiIp -Count 2 -Quiet -ErrorAction SilentlyContinue
if ($ping) {
    Say "  OK   $PiIp responds to ICMP"
} else {
    Say "  NOTE $PiIp did not answer ICMP (QNX may drop it - not fatal)"
}
Say ""

# ---------------------------------------------------------------- 2. find working login
Say "--- [2/5] SSH login (trying algorithm sets x users) ---"
$Work = $null
foreach ($u in @("qnxuser", "root")) {
    foreach ($a in $Algos) {
        Say "  try  $u  /  $($a.name)"
        $r = Try-Ssh $u $a.opts
        if ($r -match "SSH_OK") {
            Say "  >>> SUCCESS with user '$u' and set '$($a.name)'"
            Say "  >>> OPTIONS: $($a.opts)"
            Say ""
            Say "  --- board response ---"
            $r.Trim() -split "`n" | ForEach-Object { Say "  $_" }
            Say ""
            $Work = @{ user = $u; opts = $a.opts }
            break
        } else {
            $first = ($r.Trim() -split "`n" | Where-Object { $_ -match "\S" } | Select-Object -First 1)
            if ($first) { Say "      $($first.Trim())" }
        }
    }
    if ($Work) { break }
}

if (-not $Work) {
    Say ""
    Say "  >>> NO COMBINATION WORKED."
    Say "      Use PuTTY instead - it supports algorithms modern OpenSSH refuses:"
    Say "        1. Download putty.exe from putty.org"
    Say "        2. Session > Serial for the console, OR Session > Telnet-free SSH:"
    Say "        3. Host = $PiIp, Port = 22, SSH"
    Say "        4. Connection > SSH > Preferred SSH algorithm = SSH-2"
    Say "        5. Connection > SSH > Enable SHA2/PuTTY compatibility modes"
    Say "      Paste whatever PuTTY shows."
    Say ""
    Say "=========================================================="
    Say " SESSION ENDED EARLY - login failed"
    Say " Report: $Report"
    Say "=========================================================="
    exit 0
}

$PiUser = $Work.user
$OptStr = $Work.opts
function PiSsh($remote) {
    $cmd = "ssh -o ConnectTimeout=10 -o StrictHostKeyChecking=no -o UserKnownHostsFile=NUL "
    if ($OptStr -ne "") { $cmd += "$OptStr " }
    $cmd += "${PiUser}@${PiIp} `"$remote`" 2>&1"
    return (Invoke-Expression $cmd | Out-String)
}

# ---------------------------------------------------------------- 3. copy
Say "--- [3/5] copy files to the board ---"
$files = [ordered]@{
    "$Repo\tools\qnx_bringup_check.sh" = "/tmp/qnx_bringup_check.sh"
    "$Repo\tools\spi.conf.mcp2515"     = "/tmp/spi.conf.mcp2515"
    "$Repo\tools\qnx_can_install.sh"   = "/tmp/qnx_can_install.sh"
    "$Repo\third_party\can-mcp2515\aarch64le\bin\can-mcp2515" = "/tmp/can-mcp2515"
}
foreach ($src in $files.Keys) {
    if (-not (Test-Path $src)) { Say "  MISSING LOCAL FILE: $src"; continue }
    $dst = $files[$src]
    $cmd = "scp -o ConnectTimeout=10 -o StrictHostKeyChecking=no -o UserKnownHostsFile=NUL "
    if ($OptStr -ne "") { $cmd += "$OptStr " }
    $cmd += "`"$src`" `"${PiUser}@${PiIp}:$dst`" 2>&1"
    Say "  -> $dst"
    (Invoke-Expression $cmd | Out-String).Trim() -split "`n" |
        ForEach-Object { if ($_.Trim() -ne "") { Say "     $($_.Trim())" } }
}
Say ""

# ---------------------------------------------------------------- 4. audit
Say "--- [4/5] bring-up audit ---"
Say ""
(PiSsh "sh /tmp/qnx_bringup_check.sh").Trim() -split "`n" | ForEach-Object { Say $_ }
Say ""

# ---------------------------------------------------------------- 5. deps
Say "--- [5/5] dependency check for the CAN install ---"
(PiSsh "echo -n 'write /tmp    : '; ([ -w /tmp ] && echo YES) || echo NO; echo -n 'write /system/bin : '; ([ -w /system/bin ] && echo YES) || echo NO; echo -n 'uid    : '; id; echo -n 'SPI driver count : '; pidin ar 2>/dev/null | grep -c spi; echo -n 'dev/mem : '; ([ -r /dev/mem ] && echo readable) || echo 'NOT readable'; df -h /tmp 2>/dev/null | tail -1").Trim() -split "`n" | ForEach-Object { Say $_ }
Say ""

Say "=========================================================="
Say " SESSION COMPLETE"
Say " Report: $Report"
Say ""
Say " NEXT:"
Say "   1. Unplug Ethernet, reconnect WiFi"
Say "   2. Paste bringup_report.txt"
Say "=========================================================="
