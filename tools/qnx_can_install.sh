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
# IMPORTANT: this script EDITS /system/etc/config/spi/spi.conf IN PLACE.
# It changes only the spi0/dev0 settings and leaves every other section
# untouched. An earlier version replaced the whole file, which silently
# dropped the spi0/dev1 device and the entire spi3 bus - do not do that.
# See docs/BRINGUP_GUIDE.md section 7.

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

# -----------------------------------------------------------------------------

die() { echo "ERROR: $*"; exit 1; }

# Check the numeric uid, not the whole `id` output. The previous check compared
# "$(id)" - which expands to "uid=0(root) gid=0(root) groups=0(root)" - against
# the literal string "root", so it could NEVER succeed, even for uid 0.
# QSTI has no sudo, so this is the only privilege gate.
[ "$(id -u)" = "0" ] || die "must run as root (uid 0). QSTI has no sudo - use: su, or ssh root@192.168.10.5 'sh $0'"

command -v awk >/dev/null 2>&1 || die "awk not found on this target - cannot edit spi.conf safely.
       Edit it by hand instead; docs/BRINGUP_GUIDE.md section 7.3 shows the
       exact three lines to change in the spi0/dev0 block."

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
echo "== 3/8 retune spi0/dev0 in spi.conf for MCP2515"
[ -f "$SPI_CONF" ] || die "$SPI_CONF not found"

# Back up ONCE. The previous version stamped a new backup on every run, so a
# second run saved the ALREADY-MODIFIED file and destroyed the stock copy.
BACKUP=
for f in "$SPI_CONF".stock.*; do
    [ -f "$f" ] && BACKUP="$f"
done
if [ -z "$BACKUP" ]; then
    cp "$SPI_CONF" "$SPI_CONF.stock.$(date +%s 2>/dev/null || echo backup)"
    for f in "$SPI_CONF".stock.*; do
        [ -f "$f" ] && BACKUP="$f"
    done
    echo "   backed up stock spi.conf -> $BACKUP"
else
    echo "   stock backup already exists, keeping it: $BACKUP"
fi

# Targeted, section-aware edit. Writes every line through unchanged EXCEPT the
# three keys inside the block where busno=0 and devno=0:
#
#   cpha=1        -> cpha=0        (MCP2515 supports SPI mode 0 and mode 3 only;
#   cpol=0        -> cpol=0            stock mode 1 is not supported by the part)
#   word_width=32 -> word_width=8  (MCP2515 is an 8-bit SPI device)
#
# The clock is deliberately left at the stock value. MCP2515 tolerates up to
# 10 MHz, and the stock 5 MHz is within spec, so changing it buys nothing and
# risks a signal-integrity regression on flying wires.
cat > /tmp/spi_edit.awk <<'AWKEOF'
{
    line = $0

    if (line ~ /^[[:space:]]*\[/) {
        s = line
        gsub(/[[:space:]]/, "", s)
        if (s == "[bus]")      { bus = "?"; dev = "?" }
        else if (s == "[dev]") { dev = "?" }
        print line
        next
    }

    if (line ~ /^[[:space:]]*(#|$)/) { print line; next }

    if (line ~ /^[[:space:]]*busno[[:space:]]*=/) {
        split(line, a, "="); gsub(/[[:space:]]/, "", a[2]); bus = a[2]
        print line; next
    }
    if (line ~ /^[[:space:]]*devno[[:space:]]*=/) {
        split(line, a, "="); gsub(/[[:space:]]/, "", a[2]); dev = a[2]
        print line; next
    }

    if (bus == "0" && dev == "0") {
        if (line ~ /^[[:space:]]*cpha[[:space:]]*=/)       { print "cpha=0";       next }
        if (line ~ /^[[:space:]]*cpol[[:space:]]*=/)       { print "cpol=0";       next }
        if (line ~ /^[[:space:]]*word_width[[:space:]]*=/) { print "word_width=8"; next }
    }

    print line
}
AWKEOF

awk -f /tmp/spi_edit.awk "$SPI_CONF" > /tmp/spi.new 2>/dev/null \
    || die "awk edit failed - $SPI_CONF left untouched"
[ -s /tmp/spi.new ] || die "awk produced an empty file - $SPI_CONF left untouched"

cp /tmp/spi.new "$SPI_CONF"

# Verify the edit actually landed. Without this check a malformed stock file
# would silently produce an unmodified spi.conf and we would debug the wrong
# thing for an hour.
if grep -A6 '^devno=0' "$SPI_CONF" | grep -q 'cpha=0' \
   && grep -A6 '^devno=0' "$SPI_CONF" | grep -q 'word_width=8'; then
    echo "   spi0/dev0 now: cpha=0 cpol=0 word_width=8  (MCP2515 mode 0, 8-bit)"
else
    echo "   WARNING: could not confirm the dev0 edit landed. Check by hand:"
    sed -n '/devno=0/,/^$/p' "$SPI_CONF" | sed 's/^/     /'
fi

echo "   Untouched: every other [bus] and [dev] section, including spi0/dev1"
echo "              and the spi3 bus. Restore with: cp $BACKUP $SPI_CONF"

echo
echo "== 4/8 apply the SPI config change"
cat <<NOTE
   The QNX CAN DDK says to REBOOT the target after editing spi.conf, so that
   is the supported path:

       shutdown && reboot

   Then run this script again - it is safe to re-run. Step 3 will keep the
   original stock backup, step 4 will find SPI already up, and it will
   continue from step 5.

   Attempting an in-place driver restart below instead. If the SPI devices do
   not come back, that is expected: REBOOT and re-run.
NOTE

slay spi-bcm2711 2>/dev/null || true
sleep 2
spi-bcm2711 &
sleep 3
ls -l /dev/io-spi/spi0/ 2>/dev/null | sed 's/^/   /'

if [ ! -e "$SPI_DEV" ]; then
    echo
    echo "   $SPI_DEV did not come back from an in-place restart."
    echo "   This is not necessarily a fault - the DDK expects a reboot:"
    echo
    echo "       shutdown && reboot"
    echo "       sh $0"
    echo
    echo "   Driver log (slog, if any):"
    slog2info 2>/dev/null | grep -i spi | tail -15 | sed 's/^/     /'
    exit 2
fi

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
    echo "   --- diagnostic log (this is where the driver actually logs) ---"
    slog2info 2>/dev/null | tail -30
    echo
    echo "   Most likely causes:"
    echo "     1. Wrong CLOCK_HZ - Waveshare RS485 CAN HAT (SKU 14882) ships with a"
    echo "        12 MHz crystal on current boards; pre-Aug-2019 boards have 8 MHz."
    echo "        Read the marking on the silver can. Try the other value."
    echo "     2. INT GPIO does not match your wiring (expect BCM GPIO 25, pin 22)"
    echo "     3. CS is not on CE0 (BCM GPIO 8, pin 24)"
    echo "     4. SPI mode still wrong - re-check the step 3 output"
    echo "     5. The MCP2515 is not seated on the 40-pin header. Flying wires on"
    echo "        this HAT are a known source of failure; seat it properly."
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
echo " Or verify against the external node on the second Pi instead:"
echo "   Pi 2:  cansend can0 123#ABCDEF"
echo "   Pi 1:  canctl -u 0,rx0 -R 2000"
echo
echo " If nothing arrives, change CLOCK_HZ at the top of this script"
echo " and re-run. A wrong crystal value is the usual cause."
echo "==========================================================="