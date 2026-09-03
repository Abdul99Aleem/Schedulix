#!/usr/bin/env python3
"""
Windows laptop -> QNX TCP CAN injector client.
Sends lines like "100 1 01" over TCP port 5000 to the QNX/VM target.

Usage:
  python tools/can_sender.py --host 192.168.1.50 --port 5000
  python tools/can_sender.py --demo              # sends the 5 required test frames
  python tools/can_sender.py                      # interactive: type lines manually

Lines format (same as can_frame_from_string):
  <id> <dlc> <data...>
  id   – decimal or hex with 0x prefix (e.g. 100, 0x100, 256)
  dlc  – 0..8
  data – hex bytes (e.g. 01, AA, BB)
Examples:
  100 1 01
  0x100 1 01
  0x100 3 AA BB CC
  0x200 1 01
  0x999 1 01
"""
import argparse, socket, sys, time

DEMO_FRAMES = [
    "0x100 1 01",
    "0x200 1 01",
    "0x300 1 01",
    "0x999 1 01",
    "0x100 3 AA BB CC",
]

def send_lines(host, port, lines, delay=0.2):
    print(f"[sender] Connecting to {host}:{port} ...")
    try:
        s = socket.create_connection((host, port), timeout=5)
    except Exception as e:
        print(f"[sender] connect failed: {e}")
        print("[sender] Is the QNX target running? ./schedulix_can --tcp 5000")
        sys.exit(1)
    print(f"[sender] Connected. Sending {len(lines)} frame(s).")
    for ln in lines:
        line = ln.strip()
        if not line or line.startswith("#"):
            continue
        print(f"  -> \"{line}\"")
        s.sendall((line + "\n").encode())
        time.sleep(delay)
    print("[sender] Done. Keeping connection open 1s then closing.")
    time.sleep(1)
    s.close()

def main():
    ap = argparse.ArgumentParser(description="Schedulix CAN TCP injector")
    ap.add_argument("--host", default="127.0.0.1", help="QNX target IP (default 127.0.0.1)")
    ap.add_argument("--port", type=int, default=5000, help="TCP port (default 5000)")
    ap.add_argument("--demo", action="store_true", help="send the 5 required demo frames")
    ap.add_argument("--file", help="file with one frame per line")
    ap.add_argument("frames", nargs="*", help="frames on command line, e.g. \"0x100 1 01\"")
    args = ap.parse_args()

    lines = []
    if args.demo:
        lines = DEMO_FRAMES
    elif args.file:
        with open(args.file) as f:
            lines = [l.strip() for l in f if l.strip() and not l.strip().startswith("#")]
    elif args.frames:
        # Each positional arg is one full line; if user passes separate tokens, join heuristic not needed.
        # Expect quoted lines: python can_sender.py "0x100 1 01" "0x200 1 01"
        lines = args.frames
    else:
        print(f"[sender] Interactive mode — type frames, empty line to send, Ctrl-C to quit")
        print(f"[sender] Example: 0x100 1 01")
        print(f"[sender] Target {args.host}:{args.port}")
        # For interactive, keep connection open and send as typed
        try:
            s = socket.create_connection((args.host, args.port), timeout=5)
            print(f"[sender] Connected to {args.host}:{args.port}")
        except Exception as e:
            print(f"[sender] connect failed: {e}")
            sys.exit(1)
        try:
            while True:
                try:
                    ln = input("> ").strip()
                except EOFError:
                    break
                if not ln:
                    continue
                s.sendall((ln + "\n").encode())
                print(f"  sent: {ln}")
        except KeyboardInterrupt:
            pass
        finally:
            s.close()
        return

    if not lines:
        ap.print_help()
        sys.exit(0)

    send_lines(args.host, args.port, lines)

if __name__ == "__main__":
    main()
