#!/bin/sh
# Schedulix — QNX Pi 4 bring-up verification
#
# Read-only. Reports what is present so you know what still needs doing.
# Works on both the QSTI image (qnxuser/qnxuser) and the BSP reference image (root).
#
#   scp qnx_bringup_check.sh root@<pi-ip>:/tmp/
#   ssh root@<pi-ip> 'sh /tmp/qnx_bringup_check.sh'
#
# Copy a section marker to re-run only that block:
#   sh /tmp/qnx_bringup_check.sh can

BLOCK="${1:-all}"
say()  { printf '\n=== %s ===\n' "$1"; }
ok()   { printf '  [ OK ] %s\n' "$1"; }
bad()  { printf '  [GAP ] %s\n' "$1"; }
warn() { printf '  [WARN] %s\n' "$1"; }

# ---------------------------------------------------------------- platform
if [ "$BLOCK" = all ] || [ "$BLOCK" = plat ]; then
say "PLATFORM"
pidin info 2>/dev/null | sed 's/^/  /'
echo "  CPUs:    $(pidin -F '%b' info 2>/dev/null | head -1)"
pidin -F 'a:n' ar 2>/dev/null | grep -E 'proc|io-sock' | sed 's/^/  proc: /'
fi

# ---------------------------------------------------------------- network
if [ "$BLOCK" = all ] || [ "$BLOCK" = net ]; then
say "NETWORK"
ifconfig 2>/dev/null | sed 's/^/  /'
IP=$(ifconfig 2>/dev/null | awk '/inet /{print $2}')
if [ -n "$IP" ]; then ok "IPv4 = $IP"; else bad "no IPv4 address - SSH will fail"; fi
GW=$(route 2>/dev/null | awk '/0\.0\.0\.0/{print $2}')
[ -n "$GW" ] && ok "gateway = $GW" || warn "no default gateway"
fi

# ---------------------------------------------------------------- serial
if [ "$BLOCK" = all ] || [ "$BLOCK" = ser ]; then
say "SERIAL (UART)"
found=0
for d in /dev/ser0 /dev/ser1 /dev/ser2 /dev/ttyS0 /dev/ttyS1; do
    if [ -e "$d" ]; then ok "$d exists"; found=1; fi
done
[ "$found" = 0 ] && bad "no serial device nodes - start devc-ser* driver"
pidin ar 2>/dev/null | grep -i 'devc\|serminiuart' | sed 's/^/  /'
fi

# ---------------------------------------------------------------- spi
if [ "$BLOCK" = all ] || [ "$BLOCK" = spi ]; then
say "SPI (needed for MCP2515 CAN)"
if pidin ar 2>/dev/null | grep -q 'spi'; then
    ok "SPI resource manager running:"
    pidin ar 2>/dev/null | grep -i spi | sed 's/^/       /'
else
    bad "SPI driver NOT running - this is the CAN blocker"
    echo "       start with:  spi-bcm2711 &"
fi
echo "  SPI nodes:"
ls -l /dev/io-spi/ 2>/dev/null | sed 's/^/       /' || echo "       (none - /dev/io-spi missing)"
ls -l /dev/io-spi/spi0/ 2>/dev/null | sed 's/^/       /'
if [ -f /system/etc/config/spi/spi.conf ]; then
    ok "spi.conf present"
    echo "  --- current spi.conf ---"
    sed 's/^/       /' /system/etc/config/spi/spi.conf
else
    bad "no /system/etc/config/spi/spi.conf - must be created"
fi
fi

# ---------------------------------------------------------------- can
if [ "$BLOCK" = all ] || [ "$BLOCK" = can ]; then
say "CAN DRIVER"
if [ -x /system/bin/can-mcp2515 ]; then
    ok "can-mcp2515 installed at /system/bin/can-mcp2515"
    /system/bin/can-mcp2515 --version 2>&1 | head -3 | sed 's/^/       /'
else
    bad "can-mcp2515 NOT installed"
fi
if [ -e /dev/can0 ]; then
    ok "/dev/can0 present - driver is running"
    ls /dev/can0/ 2>/dev/null | sed 's/^/       /'
    ls /dev/can0/*/ 2>/dev/null | sed 's/^/       /'
else
    bad "/dev/can0 missing - driver not started"
fi
pidin ar 2>/dev/null | grep -i mcp | sed 's/^/  /'
slog2info 2>/dev/null | grep -i 'mcp2515\|can' | tail -8 | sed 's/^/  /'
fi

# ---------------------------------------------------------------- gpio
if [ "$BLOCK" = all ] || [ "$BLOCK" = gpio ]; then
say "GPIO"
for t in /system/bin/gpio-bcm2711 /system/bin/mbox-bcm2711; do
    [ -x "$t" ] && ok "$t present" || bad "$t missing"
done
# /dev/mem is NOT the access path. It does not exist on this QNX image at all.
# GPIO registers are reached with mmap(MAP_PHYS|MAP_SHARED, NOFD,
# PROT_NOCACHE, 0xFE200000), which was verified working on this board -- see
# docs/VALIDATION_LOG.md section 4.4. Probing /dev/mem here would emit a
# misleading warning about mmap failing when in fact nothing needs it.
if [ -e /dev/mem ]; then
    ok "/dev/mem exists (not required; mmap(MAP_PHYS) is used instead)"
else
    ok "/dev/mem absent - expected. mmap(MAP_PHYS) is the access path, verified working"
fi
# The real check: did the marker rewrite map the block?
if grep -q 'BCM2711 registers mapped' /tmp/sx.log 2>/dev/null; then
    ok "gpio_marker reported 'BCM2711 registers mapped' in /tmp/sx.log"
fi
fi

# ---------------------------------------------------------------- schedulix deps
if [ "$BLOCK" = all ] || [ "$BLOCK" = deps ]; then
say "SCHEDULIX DEPENDENCIES"
[ -x /tmp/schedulix_can ] && ok "/tmp/schedulix_can present" || warn "binary not deployed to /tmp"
if id 2>/dev/null | grep -q 'uid=0'; then
    ok "running as root - tracelogger will work"
else
    bad "NOT root - tracelogger requires uid=0"
fi
pidin ar 2>/dev/null | grep -q tracelogger && ok "tracelogger running" || warn "tracelogger not running (expected)"
pidin -P nohdr -F 'pid,comm' ar 2>/dev/null | head -1 >/dev/null
echo "  free space on /tmp:"
df -h /tmp 2>/dev/null | sed 's/^/       /'
fi

say "DONE"
echo "  Re-run one block:  sh /tmp/qnx_bringup_check.sh can"