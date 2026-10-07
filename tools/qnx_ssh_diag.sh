#!/bin/sh
# Schedulix - QNX Pi 4 SSH diagnostics
#
# Read-only. Explains WHY ssh fails instead of just reporting that it did.
# The three real failure modes seen in practice:
#
#   1. "Algorithm negotiation fail" - client and sshd share no common MAC.
#      Windows clients need `-m hmac-sha2-256`. Momentics IDE cannot pass
#      that flag, so its terminal always fails. Credentials are irrelevant
#      here: negotiation fails BEFORE authentication.
#   2. "Permission denied (publickey,keyboard-interactive)" - sshd offered
#      NO password method, so every password fails identically. Config
#      problem, not a wrong-password problem.
#   3. Root disabled - QNX ships PermitRootLogin no.
#
# Usage, from the serial console (the only channel that works without ssh):
#     sh /tmp/qnx_ssh_diag.sh
#
# Writes everything to /tmp/ssh_diag.txt as well as stdout, so the result
# can be read back over serial without scrolling a terminal.
#
# To paste this over a serial console in one go, create it first:
#     (serial) sh /tmp/qnx_ssh_diag.sh

OUT=/tmp/ssh_diag.txt
exec > "$OUT" 2>&1

say() { printf '\n=== %s ===\n' "$1"; }

printf 'Schedulix SSH diagnostics\nhost: %s\ndate: %s\n' \
    "$(hostname 2>/dev/null)" "$(date 2>/dev/null)"

say "IDENTITY"
id 2>/dev/null | sed 's/^/  /'

say "NETWORK"
ifconfig 2>/dev/null | sed 's/^/  /'

say "SSHD PROCESS"
if pidin ar 2>/dev/null | grep -i sshd; then :; else
    printf '  *** sshd NOT running - nothing can connect on port 22 ***\n'
fi

say "SSHD CONFIG - active directives"
CFG=/usr/etc/ssh/sshd_config
if [ -r "$CFG" ]; then
    grep -vE '^[[:space:]]*#|^[[:space:]]*$' "$CFG" 2>/dev/null | sed 's/^/  /'
else
    printf '  *** %s missing or unreadable ***\n' "$CFG"
    ls -l /usr/etc/ssh/ 2>/dev/null | sed 's/^/    /'
fi

say "SSHD CONFIG - auth directives (incl. commented defaults)"
grep -nE '^[[:space:]]*#?[[:space:]]*(PermitRootLogin|PasswordAuthentication|ChallengeResponseAuthentication|KbdInteractiveAuthentication|UsePAM|PubkeyAuthentication)' \
    "$CFG" 2>/dev/null | sed 's/^/  /'

say "VERDICT"
[ -r "$CFG" ] && {
    PRL=$(grep -iE '^[[:space:]]*PermitRootLogin'       "$CFG" 2>/dev/null | head -1)
    PWA=$(grep -iE '^[[:space:]]*PasswordAuthentication' "$CFG" 2>/dev/null | head -1)
    printf '  PermitRootLogin       = %s\n' "${PRL:-<unset>}"
    printf '  PasswordAuthentication= %s\n' "${PWA:-<unset>}"
    case "$PRL" in *no*) printf '  -> root login DISABLED: no password can work for root.\n' ;;
                   "")   printf '  -> unset; QNX images default this to no.\n' ;;
                   *)    printf '  -> root login enabled.\n' ;; esac
    case "$PWA" in *no*) printf '  -> password auth DISABLED: only pubkey/keyboard-interactive.\n' ;;
                   *)    printf '  -> password auth enabled.\n' ;; esac
}

say "ACCOUNTS"
grep -E '^(root|qnxuser):' /etc/passwd 2>/dev/null | sed 's/^/  /'

say "FIX (run on the board)"
cat <<'EOF'
  sed -i 's/^[[:space:]]*#\?PermitRootLogin.*/PermitRootLogin yes/;
          s/^[[:space:]]*#\?PasswordAuthentication.*/PasswordAuthentication yes/' \
      /usr/etc/ssh/sshd_config
  slay -f sshd
  /usr/bin/sshd -f /usr/etc/ssh/sshd_config

  Then from WINDOWS always add the MAC algorithm:
      ssh -m hmac-sha2-256 qnxuser@192.168.10.5
  Make it permanent in C:\Users\<you>\.ssh\config :
      Host qnxpi 192.168.10.5
         User qnxuser
           MACs hmac-sha2-256
EOF

printf '\n=== END ===\n'
printf '\n########## also written to %s ##########\n' "$OUT"