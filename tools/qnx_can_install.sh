#!/bin/sh
# Schedulix — install and start the MCP2515 CAN driver on the QNX Pi 4.
#
# Assumes the driver binary and spi.conf have already been scp'd to /tmp.
# Run as root.
#
#   scp third_party/can-mcp2515/aarch64le/bin/can-mcp2515 root@<ip>:/tmp/
#   scp tools/spi.conf.mcp2515                      root@<ip>:/tmp/
#   scp tools/qnx_can_install.sh                    root@<ip>:/tmp/
#   ssh root@<ip> 'sh /tmp/qnx_can_install.sh'

set -e

SPI_CONF=/system/etc/config/spi/spi.conf
DRV=/tmp/can-mcp2515

# Crystal frequency. Generic MCP2515 modules are usually 8 MHz.
# Waveshare 2-CH CAN HAT is 16 MHz.
# Change ONLY this line. Wrong value = driver starts cleanly, zero frames.
CLOCK_HZ=8000000

# INT GPIO for the module. Wired to physical pin 22 = BCM GPIO 25.
INT_GPIO=25

# SPI device node created by the [dev] section named dev0 on spi0.
SPI_DEV=/dev/io-spi/spi0/dev0

echo "== 1. install driver binary"
cp "$DRV" /system/bin/can-mcp2515
chmod +x /system/bin/can-mcp2515
ls -l /system/bin/can-mcp2515

echo
echo "== 2. install spi.conf"
mkdir -p /system/etc/config/spi
if [ -f "$SPI_CONF" ]; then
    cp "$SPI_CONF" "${SPI_CONF}.bak.$(date +%s 2>/dev/null || echo old)"
    echo "   backed up existing spi.conf"
fi
cp /tmp/spi.conf.mcp2515 "$SPI_CONF"
echo "   installed to $SPI_CONF"

echo
echo "== 3. stop any running driver"
slay can-mcp2515 2>/dev/null || true
sleep 1

echo
echo "== 4. ensure SPI driver is up"
if pidin ar 2>/dev/null | grep -q spi; then
    echo "   SPI resource manager already running"
else
    echo "   starting spi-bcm2711"
    spi-bcm2711 &
    sleep 2
fi

echo
echo "== 5. wait for SPI device node"
i=0
while [ $i -lt 10 ]; do
    if [ -e "$SPI_DEV" ]; then
        echo "   $SPI_DEV present"
        break
    fi
    sleep 1
    i=$((i+1))
done
if [ ! -e "$SPI_DEV" ]; then
    echo "   ERROR: $SPI_DEV never appeared."
    echo "   Check: pidin ar | grep spi"
    echo "   Check: cat $SPI_CONF"
    exit 1
fi
ls -l /dev/io-spi/spi0/

echo
echo "== 6. start can-mcp2515"
echo "   clock=${CLOCK_HZ}  gpio=${INT_GPIO}"
/system/bin/can-mcp2515 --mid=eid -s "$SPI_DEV" -c "$CLOCK_HZ" -g "$INT_GPIO" &
sleep 3

echo
echo "== 7. verify CAN devices"
if [ -e /dev/can0 ]; then
    echo "   /dev/can0 PRESENT"
    echo "   mailboxes:"
    ls /dev/can0/ | sed 's/^/     /'
    for m in /dev/can0/*/; do
        [ -d "$m" ] && echo "     $m" | sed 's/\/$//'
    done
else
    echo "   ERROR: /dev/can0 missing."
    echo "   Dump the log:"
    echo "     slog2info | tail -40"
    exit 1
fi

echo
echo "== 8. set catch-all receive filter"
canctl -u 0,rx0 -m 0 || true
canctl -u 0,rx0 -f 0 || true

echo
echo "== 9. self-test: loop CAN0 back to CAN1"
echo "   Wiring: connect CAN0-H to CAN1-H and CAN0-L to CAN1-L"
echo "   with a 120 ohm resistor at each end."
echo
echo "   Terminal 1 (receive):   canctl -u 0,rx0 -R 2000"
echo "   Terminal 2 (transmit):  canctl -u 0,tx2 -w 0x123,3,ABCDEF"
echo
echo "   If the frame appears in terminal 1, the CAN path works."
echo
echo "   For a SINGLE MCP2515 module there is only can0, so loop back"
echo "   CAN_H to CAN_L through 120 ohm and use RAW/listen-only mode,"
echo "   or simply verify against the external node on Pi 2 instead."
echo
echo "== install complete"