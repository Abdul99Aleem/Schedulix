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

# --- locate the SDP ----------------------------------------------------------
# Order of preference:
#   1. An already-sourced SDP environment (QNX_HOST/QNX_TARGET exported by
#      qnxsdp-env). This is the right answer on every platform and needs no
#      machine-specific path.
#   2. $QNX_SDP, for people who have not sourced the environment.
#   3. A last-resort guess at the default install location.
#
# Hardcoding a single absolute path here would make this script work on exactly
# one machine and nowhere else.
if [ -n "$QNX_TARGET" ] && [ -d "$QNX_TARGET/usr/include/mk" ]; then
    SDP=$(cd "$QNX_TARGET/.." && pwd)
    SDPSRC="inherited from the sourced SDP environment"
elif [ -n "$QNX_SDP" ] && [ -d "$QNX_SDP" ]; then
    SDP="$QNX_SDP"
    SDPSRC="from \$QNX_SDP"
else
    for cand in /opt/qnx800 "$HOME/qnx800" "C:/Users/$USERNAME/qnx800" "C:/qnx800"; do
        [ -d "$cand" ] && { SDP="$cand"; SDPSRC="guessed"; break; }
    done
fi

if [ -z "$SDP" ] || [ ! -d "$SDP" ]; then
    cat >&2 <<'EOF'
ERROR: could not locate the QNX SDP.

Source the SDP environment first, then re-run:

    # Windows (cmd)
    call C:\Users\<you>\qnx800\qnxsdp-env.bat
    sh tools/build_can_driver.sh

    # Linux / macOS
    . ~/qnx800/qnxsdp-env.sh
    sh tools/build_can_driver.sh

or point at it explicitly:

    QNX_SDP=/path/to/qnx800 sh tools/build_can_driver.sh
EOF
    exit 1
fi

QNX_HOST="$SDP/host/win64/x86_64"
# A Linux/macOS host uses host/<uname>, so prefer whatever the environment
# already said if it looks right.
if [ -n "$QNX_HOST_OVERRIDE" ]; then
    QNX_HOST="$QNX_HOST_OVERRIDE"
elif [ -d "$SDP/host/$(uname -s | tr 'A-Z' 'a-z')" ] 2>/dev/null; then
    QNX_HOST="$SDP/host/$(uname -s | tr 'A-Z' 'a-z')"
fi
QNX_TARGET="$SDP/target/qnx"
MKROOT="$QNX_TARGET/usr/include/mk"

if [ ! -d "$SDP" ]; then
    echo "ERROR: SDP not found at $SDP ($SDPSRC)" >&2
    exit 1
fi
if [ ! -d "$MKROOT" ]; then
    echo "ERROR: $MKROOT not found - is $SDP really an SDP 8.0 install?" >&2
    exit 1
fi

MAKE="$QNX_HOST/usr/bin/make.exe"
[ -x "$MAKE" ] || MAKE="$QNX_HOST/usr/bin/make"
[ -x "$MAKE" ] || MAKE=$(command -v make)
[ -n "$MAKE" ] || { echo "ERROR: no make found" >&2; exit 1; }

echo "== SDP"
echo "   root    : $SDP  ($SDPSRC)"
echo "   host    : $QNX_HOST"
echo "   target  : $QNX_TARGET"

# ---------------------------------------------------------------------------
# SEED BEFORE TOUCHING PATH - this ordering is load-bearing.
#
# $QNX_HOST/usr/bin ships its OWN find.exe, dirname.exe, cp.exe, mkdir.exe.
# Prepending it to PATH makes those shadow the MSYS/GNU tools, and the SDP
# copies do not understand MSYS paths like /c/Users/... - they report
# "No such file or directory" for directories that plainly exist.
#
# That failure is silent: the find runs inside a `for` word-list, so `set -e`
# never sees it, and the loop simply seeds nothing. The build then limps on
# with whatever stale make infrastructure happens to be lying around.
#
# So: seed with the original tools, then switch PATH over for make alone.
# ---------------------------------------------------------------------------
echo
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

# A zero here means the seeding silently did nothing. Fail loudly rather than
# letting the build fail later with a confusing recurse.mk error.
if [ "$count" -eq 0 ]; then
    echo >&2
    echo "ERROR: found no Makefiles under $DRV - the seeding step did nothing." >&2
    echo "       This is almost always a PATH/tooling problem: the SDP's own" >&2
    echo "       find.exe and cp.exe cannot handle MSYS-style paths." >&2
    exit 1
fi

# Now, and only now, switch the tools over to the SDP for the build itself.
export QNX_HOST QNX_TARGET
export PATH="$QNX_HOST/usr/bin:$QNX_TARGET/usr/bin:$PATH"
export MKFILES_ROOT="$MKROOT"

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