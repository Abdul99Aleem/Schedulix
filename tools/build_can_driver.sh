#!/bin/sh
# Schedulix - build the QNX can-mcp2515 driver from vendored source.
#
# Source: https://gitlab.com/qnx/projects/drivers/can-mcp2515
#         vendored under third_party/can-mcp2515, commit 0fd11af.
#
# Run from the repository root:
#     sh tools/build_can_driver.sh
#
# Output:
#     third_party/can-mcp2515/aarch64le/bin/can-mcp2515      (release)
#     third_party/can-mcp2515/aarch64le/bin/can-mcp2515_g    (debug)
#     third_party/can-mcp2515/aarch64le/lib/libmcp2515.a
#     third_party/can-mcp2515/usr/include/mcp2515/*.h
#
# -------------------------------------------------------------------------
# WHY THIS SCRIPT HAS TO COPY FILES AROUND FIRST
#
# The driver uses QNX's recursive make, whose Makefiles contain bare
# `include recurse.mk` and `include qconfig.mk` statements. GNU make resolves
# an unqualified include against the CURRENT WORKING DIRECTORY, not against
# the directory holding the including file. Since the build recurses down
# through lib/nto/aarch64/a.le and driver/nto/aarch64/le.g, those files have
# to be present in EVERY directory that contains a Makefile.
#
# None of them ship in the git repository. They come from the SDP:
#
#     $QNX_TARGET/usr/include/recurse.mk
#     $QNX_TARGET/usr/include/mk/qconfig.mk
#     $QNX_TARGET/usr/include/mk/qconf-{win64,linux,nto}.mk
#
# and MKFILES_ROOT must point at $QNX_TARGET/usr/include/mk, because
# lib/common.mk does `include $(MKFILES_ROOT)/qtargets.mk`.
#
# Without this, the build dies at:
#     Makefile:16: recurse.mk: No such file or directory
# -------------------------------------------------------------------------

set -e

REPO=$(cd "$(dirname "$0")/.." && pwd)
DRV="$REPO/third_party/can-mcp2515"
SDP="${QNX_SDP:-C:/Users/User/qnx800}"
QNX_HOST="$SDP/host/win64/x86_64"
QNX_TARGET="$SDP/target/qnx"
MKROOT="$QNX_TARGET/usr/include/mk"

if [ ! -d "$SDP" ]; then
    echo "ERROR: SDP not found at $SDP" >&2
    echo "       set QNX_SDP=/path/to/qnx800 and re-run" >&2
    exit 1
fi

MAKE="$QNX_HOST/usr/bin/make.exe"
[ -x "$MAKE" ] || MAKE=$(command -v make)

export QNX_HOST QNX_TARGET
export PATH="$QNX_HOST/usr/bin:$QNX_TARGET/usr/bin:$PATH"
export MKFILES_ROOT="$MKROOT"

echo "== seeding make infrastructure from the SDP"
count=0
for d in $(find "$DRV" -name Makefile -exec dirname {} \;); do
    cp -f "$QNX_TARGET/usr/include/recurse.mk" "$d/recurse.mk"
    cp -f "$MKROOT/qconfig.mk"             "$d/qconfig.mk"
    mkdir -p "$d/mk"
    cp -f "$MKROOT/qconfig.mk"             "$d/mk/qconfig.mk"
    for f in "$MKROOT"/qconf-*.mk; do
        cp -f "$f" "$d/$(basename "$f")"
    done
    count=$((count + 1))
done
echo "   seeded $count directories"

echo
echo "== building (release + debug, aarch64le)"
cd "$DRV"
"$MAKE" "INSTALL_ROOT_nto=$DRV/build" "USE_INSTALL_ROOT=1" hinstall install

echo
echo "== result"
ls -l "$DRV/aarch64le/bin/" 2>/dev/null || true

if [ ! -f "$DRV/aarch64le/bin/can-mcp2515" ]; then
    echo "ERROR: can-mcp2515 was not produced" >&2
    exit 1
fi

echo
echo "Driver built. Copy it to the target:"
echo "  scp -o \"MACs=hmac-sha2-256\" $DRV/aarch64le/bin/can-mcp2515 root@qnxpi:/tmp/"