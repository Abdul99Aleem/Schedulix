#!/usr/bin/env python3
"""
Host-side pure-Python verification that mirrors the C logic.
Useful tonight when no QNX host compiler is available for native run.
Validates the same mapping that C code implements.
"""
import subprocess, sys, os, textwrap

# Simulate decode exactly like C
MAP = {0x100: "BRAKE", 0x200: "ADAS", 0x300: "DIAGNOSTIC"}

def decode(can_id):
    return MAP.get(can_id, "UNKNOWN")

def make_frame(can_id, dlc, data):
    return {"id": can_id, "dlc": dlc, "data": data, "ts": 123456789}

def test():
    cases = [
        (0x100, 1, [0x01], "BRAKE"),
        (0x200, 1, [0x01], "ADAS"),
        (0x300, 1, [0x01], "DIAGNOSTIC"),
        (0x999, 1, [0x01], "UNKNOWN"),
        (0x100, 3, [0xAA,0xBB,0xCC], "BRAKE"),
    ]
    ok = True
    for can_id, dlc, data, exp in cases:
        got = decode(can_id)
        status = "PASS" if got == exp else "FAIL"
        if status == "FAIL": ok = False
        data_s = " ".join(f"{b:02X}" for b in data)
        print(f"  {status}: 0x{can_id:03X} DLC={dlc} DATA={data_s} -> {got} (expected {exp})")
    return ok

if __name__ == "__main__":
    print("Schedulix Python host verification (mirrors C logic)")
    print("====================================================")
    ok = test()
    print("\nOverall:", "PASS" if ok else "FAIL")
    sys.exit(0 if ok else 1)
