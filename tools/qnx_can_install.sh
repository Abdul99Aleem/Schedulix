#!/bin/sh
# Schedulix - install and start the MCP2515 CAN driver on the QNX Pi 4.
#
# Tailored for the QSTI image confirmed on this board:
#   QNX 8.0.0, qnxuser/qnxuser, spi-bcm2711 already running,
#   /dev/io-spi/spi0/{dev0,dev1} already present,
#   canctl at /system/bin/canctl, root filesystem read-only (100% full).
#
# Needs root. The QSTI image has NO sudo, so escalate by running as root over
# ssh, or from a root serial console. Example:
#   ssh root@192.168.10.5 'sh /tmp/qnx_can_install.sh'
#
# IMPORTANT: this script REWRITES /system/etc/config/spi/spi.conf.
# The stock QSTI config uses cpha=1/cpol=0/word_width=32, which is wrong for
# the MCP2515 (needs cpha=0/cpol=0/word_width=8). A backup is kept.

SPI_CONF=/system/etc/config/spi/spi.conf
DRV_SRC=/tmp/can-mcp2515
DRV_DST=/system/bin/can-mcp2515

# --- tune these -------------------------------------------------------------
# Crystal on your MCP2515 module. WRONG VALUE = the driver starts cleanly and
# then receives zero frames, with no error message. That failure mode is why
# this is worth reading off the board rather than assuming.
#
# For the Waveshare RS485 CAN HAT (SKU 14882), per the Waveshare wiki:
#   - current boards      12000000  (12 MHz)
#   - pre-Aug-2019 boards  8000000  ( 8 MHz)
# The SKU alone does not disambiguate; the product page does not state it.
#
# CONFIRMED on our boards: the crystal is marked "EAS12.000", i.e. 12 MHz.
# Read the marking on the silver can-shaped crystal to be sure - it sits next
# to the MCP2515. "EAS" is the maker's code, "12.000" the frequency.
#
# Generic bare MCP2515 modules are usually 8 MHz.
CLOCK_HZ=12000000

# CAN bitrate. Must match the other node on the bus.
BPS=500000

# MCP2515 INT line. Wired to physical pin 22 = BCM GPIO 25.
INT_GPIO=25

# SPI node. dev0 = physical pin 24 (CE0).
SPI_DEV=/dev/io-spi/spi0/dev0

# 0 = leave spi3 alone. 1 = also fix spi3's word_width (not needed for CAN).
FIX_SPI3=0
# -----------------------------------------------------------------------------

die() { echo "ERROR: $*"; exit 1; }

[ "$(id)" = "root" ] || die "must run as root. QSTI has no sudo - use: ssh root@192.168.10.5 'sh $0'"

echo "== 1/8 verify prerequisites"
[ -f "$DRV_SRC" ] || die "$DRV_SRC not found - upload can-mcp2515 first"
[ -d /dev/io-spi/spi0 ] || die "/dev/io-spi/spi0 missing - SPI driver not running"
echo "   SPI driver: $(pidin ar 2>/dev/null | grep -c spi-bcm2711) instance(s)"
[ -e "$SPI_DEV" ] || die "$SPI_DEV missing - check spi.conf for dev0"

echo
echo "== 2/8 install driver binary"
mkdir -p /system/bin
cp "$DRV_SRC" "$DRV_DST"
chmod 755 "$DRV_DST"
ls -l "$DRV_DST"

echo
echo "== 3/8 rewrite spi.conf for MCP2515"
[ -f "$SPI_CONF" ] && cp "$SPI_CONF" "$SPI_CONF.stock.$(date +%s 2>/dev/null || echo backup)" && echo "   backed up stock spi.conf"

cat > "$SPI_CONF" <<EOF
# Schedulix - MCP2515 CAN configuration
# Rewritten from the stock QSTI config. The stock spi0/dev0 entry used
# cpha=1 cpol=0 word_width=32; the MCP2515 requires cpha=0 cpol=0
# word_width=8 per the QNX CAN DDK reference configuration.

[globals]
verbose=5

[bus]
busno=0
name=spi0
base=0xfe204000
irq=150
input_clock=500000000
bs=rpanic=48,tpanic=16
dma_attach_opts=num_cbs=256,range_min=0,range_max=14,typed_mem=sysram&below1G
dma_thld=4

[dev]
parent_busno=0
devno=0
name=dev0
clock_rate=10000000
cpha=0
cpol=0
bit_order=msb
word_width=8
idle_insert=1
EOF
echo "   wrote $SPI_CONF"
echo "   NOTE: dev0 keeps its original SPI settings (10 MHz, mode 0, 8-bit)."
echo "         Restarting SPI now."

echo
echo "== 4/8 restart the SPI driver to pick up new settings"
slay spi-bcm2711 2>/dev/null || true
sleep 2
spi-bcm2711 &
sleep 3
ls -l /dev/io-spi/spi0/ 2>/dev/null | sed 's/^/   /'
[ -e "$SPI_DEV" ] || die "$SPI_DEV did not come back after SPI restart"

echo
echo "== 5/8 stop any running CAN driver"
slay can-mcp2515 2>/dev/null || true
rm -f /dev/can0 /dev/can1 2>/dev/null
sleep 1

echo
echo "== 6/8 start can-mcp2515"
echo "   spi   : $SPI_DEV"
echo "   clock : ${CLOCK_HZ} Hz   (MCP2515 crystal - VERIFY THIS)"
echo "   gpio  : ${INT_GPIO}"
echo "   bps   : ${BPS}"
"$DRV_DST" --mid=eid -s "$SPI_DEV" -c "$CLOCK_HZ" -g "$INT_GPIO" -B "$BPS" &
DRVPID=$!
sleep 4

echo
echo "== 7/8 verify CAN device"
if [ -e /dev/can0 ]; then
    echo "   /dev/can0 PRESENT"
    echo "   mailbox nodes:"
    ls -d /dev/can0/*/ 2>/dev/null | sed 's#/$##; s/^/     /'
else
    echo "   /dev/can0 MISSING"
    echo
    echo "   --- diagnostic log ---"
    slog2info 2>/dev/null | tail -30
    echo
    echo "   Most likely causes:"
    echo "     1. No MCP2515 connected yet - expected if hardware is not wired"
    echo "     2. Wrong CLOCK_HZ - Waveshare RS485 CAN HAT (SKU 14882) ships with a"
    echo "        12 MHz crystal on current boards; pre-Aug-2019 boards have 8 MHz."
    echo "        Read the marking on the silver can. Try the other value."
    echo "     3. INT GPIO does not match your wiring"
    echo "     4. SPI mode still wrong - check step 3 output"
    echo
    echo "   Driver process alive? $(kill -0 $DRVPID 2>/dev/null && echo yes || echo no)"
    echo "   Start it in the foreground with -D to see live logging:"
    echo "     $DRV_DST --mid=eid -s $SPI_DEV -c ${CLOCK_HZ} -g ${INT_GPIO} -B ${BPS} -D"
    exit 1
fi

echo
echo "== 8/8 set catch-all receive filter"
canctl -u 0,rx0 -m 0 2>&1 | sed 's/^/   /'
canctl -u 0,rx0 -f 0 2>&1 | sed 's/^/   /'

echo
echo "==========================================================="
echo " INSTALL COMPLETE"
echo "==========================================================="
echo " CAN nodes:"
ls -d /dev/can0/*/ 2>/dev/null | sed 's#/$##; s/^/   /'
echo
echo " Self-test in two terminals:"
echo "   T1 (receive):   canctl -u 0,rx0 -R 2000"
echo "   T2 (transmit):  canctl -u 0,tx2 -w 0x123,3,ABCDEF"
echo
echo " Or verify against the external node on Pi 2 instead:"
echo "   Pi 2:  cansend can0 123#ABCDEF"
echo "   Pi 1:  canctl -u 0,rx0 -R 2000"
echo
echo " If nothing arrives, change CLOCK_HZ at the top of this script"
echo " and re-run. A wrong crystal value is the usual cause."
echo "==========================================================="