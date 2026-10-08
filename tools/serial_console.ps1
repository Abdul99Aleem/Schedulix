<#
.SYNOPSIS
    Schedulix - QNX serial console client (Windows / PowerShell 5.1+)

.DESCRIPTION
    Opens the QNX board's UART console and gives you an interactive shell, or
    sends a single command and captures the reply. This is the serial-console
    equivalent of tools\qnx_ssh.py, used when Ethernet is unavailable or when
    root privileges are required.

    Wiring (Raspberry Pi 40-pin header):
        pin 6  GND      -> adapter GND
        pin 8  GPIO14   -> adapter RX     (Pi transmits)
        adapter TX       -> [divider] -> pin 10 (GPIO15, Pi receives)

    NOTE ON UPLOADS: the console driver runs in a mode where Ctrl+D is not
    delivered as end-of-file and Ctrl+C is not delivered as SIGINT, so any
    approach that parks the shell in a foreground reader (`cat > file`) will
    wedge the console and require a power cycle. The upload below therefore
    uses one short shell command per chunk and never blocks the prompt.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\serial_console.ps1
    Interactive console. Ctrl+C to exit.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\serial_console.ps1 -Command "id; hostname"
    Run one command, print the reply.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\serial_console.ps1 -UploadFile build\aarch64le-debug\schedulix_can -RemotePath /tmp/sx
    Ship a binary to the board. Use build\, not deploy\ -- every binary in
    deploy\ predates the uart/gpio subcommands.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\serial_console.ps1 -Port COM4 -Baud 57600
    Different port or baud rate.
#>
[CmdletBinding()]
param(
    [string] $Port = 'COM6',
    [int]    $Baud = 115200,
    [string] $Command,
    [int]    $CaptureSeconds = 8,

    # Binary upload over the console. The payload is gzipped, then base64
    # encoded, then streamed as fixed-width lines into a FOREGROUND
    # `head -c N > file` reader, whose byte budget is set well above the
    # payload so it does not stop early and truncate the tail.
    #
    # KNOWN-BAD sinks on this console. Do not substitute them:
    #   * `cat > file`      -- never ends; Ctrl+D is not delivered as EOF and
    #                          Ctrl+C is not delivered as SIGINT, so it wedges
    #                          the shell and the board needs a power cycle.
    #   * `dd bs=N count=M`  -- tested; never returned the prompt and wedged
    #                          the console the same way. Do not reintroduce.
    #   * anything with `&` -- a background job reading the tty takes SIGTTIN
    #                          and is stopped.
    #
    # head is quirky: it consumes input line by line and stops once its budget
    # is spent rather than blocking for the full count, so the budget must be
    # a generous multiple of the payload. See the sink comment in the upload
    # section for the measurements behind this.
    #
    # Do not replace the stream with per-chunk `echo '<chunk>' >> file` either:
    # one dropped byte leaves an unterminated quote and ksh then swallows every
    # remaining chunk as string content.
    #
    # Chunking is not optional. Hardware flow control is not wired, so a single
    # large Write() overruns the Pi's UART receive FIFO and silently drops
    # bytes. The echo that comes back must also be drained on every iteration
    # or the receive buffer overruns in the other direction.
    [string] $UploadFile,
    [string] $RemotePath,
    [int]    $ChunkSize = 64,
    [int]    $ChunkDelayMs = 12
)

$ErrorActionPreference = 'Stop'

# COMx on the Pi is 115200 8N1 with hardware flow control disabled.
$serial = New-Object System.IO.Ports.SerialPort $Port, $Baud, 'None', 8, 'One'
$serial.ReadTimeout = 500

try {
    $serial.Open()
}
catch {
    Write-Host "ERROR: could not open ${Port} - $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "Another terminal (PuTTY?) may still be holding the port." -ForegroundColor Yellow
    exit 1
}

# Drain any pending input. The console driver echoes everything we send.
function Clear-Input {
    if ($serial.BytesToRead -gt 0) { $null = $serial.ReadExisting() }
}

# ---------------------------------------------------------------------------
# Upload mode
# ---------------------------------------------------------------------------
if ($UploadFile) {
    if (-not $RemotePath) { $RemotePath = "/tmp/$(Split-Path $UploadFile -Leaf)" }
    $staging = "$RemotePath.b64"

    $local = (Resolve-Path $UploadFile).Path
    $bytes = [System.IO.File]::ReadAllBytes($local)

    # Gzip before base64. The target ships /system/bin/gunzip, and cutting the
    # payload roughly thirds the wire time on an ungated 115200 link.
    $mem = New-Object System.IO.MemoryStream
    $gz  = New-Object System.IO.Compression.GZipStream($mem, [System.IO.Compression.CompressionMode]::Compress)
    $gz.Write($bytes, 0, $bytes.Length)
    $gz.Close()
    $packed = $mem.ToArray()
    $b64    = [Convert]::ToBase64String($packed)

    # The line discipline does NOT fold CRLF into LF, so every line costs two
    # bytes. This is not assumed -- staging sizes come back at exactly
    # base64Length + (lines * 2) + 400, which confirms it.
    $lines = [math]::Ceiling($b64.Length / $ChunkSize)
    $dataBytesMax = $b64.Length + ($lines * 2)

    # The reader's budget must exceed the real stream, otherwise it truncates
    # the tail (see the sink note). head consumes line by line and stops once
    # its budget runs out, so a 3x margin is used. Anything past the end of the
    # stream is whitespace that tr removes before decode.
    $headBytes = [int]([math]::Ceiling($dataBytesMax * 3)) + 8192

    Write-Host 'Serial upload' -ForegroundColor Green
    Write-Host "  source  : $local" -ForegroundColor DarkGray
    Write-Host "  target  : $RemotePath" -ForegroundColor DarkGray
    Write-Host "  payload : $($bytes.Length) B -> $($packed.Length) B gzipped -> $($b64.Length) base64 chars in $lines chunks" -ForegroundColor DarkGray

    $started = Get-Date
    $done    = $false

    for ($attempt = 1; $attempt -le 3 -and -not $done; $attempt++) {
        if ($attempt -gt 1) {
            Write-Host "  retry $attempt ..." -ForegroundColor Yellow
            Start-Sleep -Seconds 3
        }

        Clear-Input

        # Open a FOREGROUND `head -c N` reader. On this console:
        #   * `cat > file` never ends -- Ctrl+D is not delivered as EOF and
        #     Ctrl+C is not delivered as SIGINT, so it wedges the shell and
        #     the board needs a power cycle.
        #   * a backgrounded reader (`... &`) takes SIGTTIN and is stopped.
        #   * `dd bs=N count=M of=f` was tried and is WORSE: it never returned
        #     the prompt and left the console wedged. Do not reintroduce it.
        #
        # KEEP `headBytes` WELL ABOVE the real stream. Measured: `head -c 200`
        # returned after the first line and wrote only 1 byte, so head consumes
        # input line by line and stops when its budget runs out rather than
        # blocking for the full count. If the budget is close to the payload
        # length, the tail gets truncated. Use a generous multiple.
        $serial.Write("rm -f $staging; head -c $headBytes > $staging`r`n")
        Start-Sleep -Milliseconds 800

        for ($i = 0; $i -lt $lines; $i++) {
            $start = $i * $ChunkSize
            $len   = [math]::Min($ChunkSize, $b64.Length - $start)

            $serial.Write($b64.Substring($start, $len) + "`r`n")
            Start-Sleep -Milliseconds $ChunkDelayMs

            # Drain the console echo every iteration. Left to accumulate it
            # overruns the receive buffer, and the transfer silently dies.
            Clear-Input

            if ($i -gt 0 -and $i % 60 -eq 0) {
                $pct     = [math]::Round(100 * $i / $lines)
                $elapsed = [math]::Max(0.1, ((Get-Date) - $started).TotalSeconds)
                $rate    = [math]::Round((($i * $ChunkSize) / 1KB) / $elapsed, 1)
                Write-Host ("  {0,3}%  {1}/{2} chunks  {3} KB/s" -f $pct, $i, $lines, $rate) -ForegroundColor DarkGray
            }
        }

        # Pad with whitespace so head keeps consuming past the end of the payload
        # until its budget is spent, then exits. The surplus is discarded by tr
        # before decode. Keep this comfortably large for the same reason the
        # head budget is: a short pad lets head stop early and truncate.
        $serial.Write("`n" * 4000)
        Start-Sleep -Seconds 3

        # Decode and unpack. The FINAL byte count is the authoritative check.
        $serial.Write("echo -n UP_SIZE_; wc -c < $staging; echo _UP_END; tr -d '\r\n' < $staging | base64 -d | gunzip > $RemotePath; rm -f $staging; chmod +x $RemotePath; echo -n FINAL_; wc -c < $RemotePath; echo _FINAL_END`r`n")

        $buffer   = New-Object System.Text.StringBuilder
        $deadline = (Get-Date).AddSeconds(90)
        while ((Get-Date) -lt $deadline) {
            if ($serial.BytesToRead -gt 0) {
                [void] $buffer.Append($serial.ReadExisting())
                $text = $buffer.ToString()

                $sm = [regex]::Matches($text, 'UP_SIZE_(\d+)\s*_UP_END')
                if ($sm.Count -gt 0) {
                    $sBytes = [int64]$sm[$sm.Count - 1].Groups[1].Value
                    $expect = if ($sBytes -ge $dataBytesMax) { $dataBytesMax } else { $b64.Length + $lines }
                    $delta  = $sBytes - $expect
                    $note   = if ($delta -ge 0) { "staging $sBytes B (+$delta padding)" } else { "staging $sBytes B, short by $($delta * -1)" }
                    Write-Host "  $note" -ForegroundColor DarkGray
                    if ($delta -lt 0) {
                        Write-Host '  stream looks truncated' -ForegroundColor Yellow
                    }
                }

                $fm = [regex]::Matches($text, 'FINAL_(\d+)\s*_FINAL_END')
                if ($fm.Count -gt 0) {
                    $got = [int64]$fm[$fm.Count - 1].Groups[1].Value
                    if ($got -eq $bytes.Length) {
                        $elapsed = [math]::Round(((Get-Date) - $started).TotalSeconds, 1)
                        Write-Host "OK  $got bytes on target, matches source  (${elapsed}s)" -ForegroundColor Green
                        $done = $true
                    } else {
                        Write-Host "  decode gave $got bytes, expected $($bytes.Length)" -ForegroundColor Yellow
                    }
                    break
                }
            }
            Start-Sleep -Milliseconds 80
        }

        if (-not $done) { Write-Host '  no FINAL_ report' -ForegroundColor Yellow }
    }

    if (-not $done) {
        Write-Host 'TRANSFER INCOMPLETE after 3 attempts.' -ForegroundColor Red
        $serial.Close()
        exit 1
    }

    $serial.Close()
    exit 0
}

# ---------------------------------------------------------------------------
# One-shot mode: send a command, capture the reply, exit.
# ---------------------------------------------------------------------------
if ($Command) {
    Write-Host "-- ${Port} @ ${Baud} 8N1, sending: ${Command}" -ForegroundColor DarkGray
    $serial.Write($Command + "`r`n")

    $buffer = New-Object System.Text.StringBuilder
    $deadline = (Get-Date).AddSeconds($CaptureSeconds)
    while ((Get-Date) -lt $deadline) {
        if ($serial.BytesToRead -gt 0) {
            [void] $buffer.Append($serial.ReadExisting())

            # Return as soon as a shell prompt sits at the end of the stream
            # instead of always burning the whole capture window. The timeout
            # still caps the wait for commands that produce no prompt.
            if ($buffer.ToString() -match '(?m)[\r\n][^\r\n]*[#$]\s*$') { break }
        }
        Start-Sleep -Milliseconds 80
    }
    $serial.Close()
    Write-Output $buffer.ToString()
    exit 0
}

# ---------------------------------------------------------------------------
# Interactive mode: mirror serial output, forward keystrokes back.
# ---------------------------------------------------------------------------
Clear-Host
Write-Host 'Schedulix QNX serial console' -ForegroundColor Green
Write-Host "  port   : $Port" -ForegroundColor DarkGray
Write-Host "  format : ${Baud} 8N1, no flow control" -ForegroundColor DarkGray
Write-Host '  exit   : Ctrl+C' -ForegroundColor DarkGray
Write-Host ''
Write-Host 'The board stays silent until you type. Press Enter to get a prompt.' -ForegroundColor Yellow
Write-Host ''

try {
    while ($true) {
        if ($serial.BytesToRead -gt 0) {
            Write-Host -NoNewline $serial.ReadExisting()
        }

        # KeyAvailable throws when stdin is redirected; swallow that and keep
        # mirroring output so the script still works as a dumb logger.
        try {
            while ([Console]::KeyAvailable) {
                $key = [Console]::ReadKey($true)
                switch ($key.Key) {
                    'Enter'     { $serial.Write("`r`n") }
                    'Backspace' { $serial.Write([char]0x7F) }
                    'Escape'    { $serial.Write([char]0x1B) }
                    default     { $serial.Write($key.KeyChar) }
                }
            }
        }
        catch { }

        Start-Sleep -Milliseconds 20
    }
}
finally {
    $serial.Close()
    Write-Host ''
    Write-Host "${Port} closed." -ForegroundColor DarkGray
}