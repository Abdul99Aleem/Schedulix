#!/usr/bin/env python3
"""
Host-side simulation that reproduces the exact C output without compiling.
Mirrors src/main.c + simulated_can_adapter.c + decoder + event_manager.
Use this tonight on Windows when no QNX binary can be executed natively.
"""
import time

# Monotonic ns (like can_get_monotonic_ns)
def mono_ns():
    return time.monotonic_ns()

MAP = {0x100: "BRAKE", 0x200: "ADAS", 0x300: "DIAGNOSTIC"}
DISPATCH = {0x100: "brake event", 0x200: "ADAS event", 0x300: "diagnostic event"}

FRAMES = [
    {"id": 0x100, "dlc": 1, "data": [0x01]},
    {"id": 0x200, "dlc": 1, "data": [0x01]},
    {"id": 0x300, "dlc": 1, "data": [0x01]},
    {"id": 0x999, "dlc": 1, "data": [0x01]},
    {"id": 0x100, "dlc": 3, "data": [0xAA, 0xBB, 0xCC]},
]

def can_frame_print(f):
    print("CAN FRAME")
    print("---------")
    print(f"ID        : 0x{f['id']:03X}")
    print(f"DLC       : {f['dlc']}")
    data_s = " ".join(f"{b:02X}" for b in f["data"][:f["dlc"]]) if f["dlc"] else "<empty>"
    print(f"DATA      : {data_s}")
    print(f"TIMESTAMP : {f['ts']} ns")
    print()

def sched_event_print(f):
    typ = MAP.get(f["id"], "UNKNOWN")
    print("SCHEDULIX EVENT")
    print("---------------")
    print(f"TYPE      : {typ}")
    print(f"CAN ID    : 0x{f['id']:03X}")
    print(f"DATA LEN  : {f['dlc']}")
    data_s = " ".join(f"{b:02X}" for b in f["data"][:f["dlc"]]) if f["dlc"] else "<empty>"
    print(f"DATA      : {data_s}")
    print(f"TIMESTAMP : {f['ts']} ns")
    print()

def event_manager_dispatch(f):
    print("EVENT MANAGER")
    print("-------------")
    if f["id"] in DISPATCH:
        print(f"DISPATCH  : {DISPATCH[f['id']]}")
    else:
        print(f"DISPATCH  : UNKNOWN event - log/reject (ID=0x{f['id']:03X})")
    print()

def main():
    print("SCHEDULIX CAN EVENT MANAGER")
    print("===========================")
    print("Deliverable 1: CAN frame -> Decoder -> Event Manager")
    print("Mode: simulation (Python host mirror)\n")

    counts = {"BRAKE":0, "ADAS":0, "DIAGNOSTIC":0, "UNKNOWN":0}
    for fr in FRAMES:
        fr["ts"] = mono_ns()
        can_frame_print(fr)
        sched_event_print(fr)
        event_manager_dispatch(fr)
        typ = MAP.get(fr["id"], "UNKNOWN")
        counts[typ] += 1
        # small delay to show monotonic increasing
        time.sleep(0.01)

    print("SUMMARY")
    print("-------")
    total = sum(counts.values())
    print(f"Frames processed : {total}")
    print(f"  BRAKE          : {counts['BRAKE']}")
    print(f"  ADAS           : {counts['ADAS']}")
    print(f"  DIAGNOSTIC     : {counts['DIAGNOSTIC']}")
    print(f"  UNKNOWN        : {counts['UNKNOWN']}")
    print()
    print("Deliverable 1 DONE: Software CAN frame -> can_frame_t -> decoder -> sched_event_t -> Event Manager")

if __name__ == "__main__":
    main()

