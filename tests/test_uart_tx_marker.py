#!/usr/bin/env python3
"""Regression: UART transmit must be traceable as a transmit, not as a receive.

Defect (src/uart_adapter.c:151): uart_adapter_trace_tx() recorded a
TRANSMIT as TRACE_EXTERNAL_EVENT_RX. The direction was carried in arg1
(1 = rx, 2 = tx) but nothing read it, so every UART transmit was
indistinguishable from an arrival in the trace and in the analyzer.

Fixed 2026-10-07 by adding TRACE_EXTERNAL_EVENT_TX = 16.

These tests pin the fix in place. They are source-level checks plus a
behavioural check of the enum layout, which is what actually matters:
the trace format is binary, so a new value must be APPENDED or every
record written by an older build changes meaning.
"""
import pathlib, re, sys

ROOT = pathlib.Path(__file__).parent.parent
SRC = ROOT / "src"

failures = []


def check(cond, msg):
    if not cond:
        failures.append(msg)


def test_tx_uses_dedicated_event_type():
    """The actual defect: TX must not be recorded as RX."""
    txt = (SRC / "uart_adapter.c").read_text()

    tx_fn = re.search(
        r"int uart_adapter_trace_tx\([^)]*\)\s*\{(.*?)\n\}", txt, re.S)
    assert tx_fn, "uart_adapter_trace_tx() not found"
    body = tx_fn.group(1)

    check("TRACE_EXTERNAL_EVENT_TX" in body,
          "uart_adapter_trace_tx() must record TRACE_EXTERNAL_EVENT_TX")
    check("TRACE_EXTERNAL_EVENT_RX" not in body,
          "uart_adapter_trace_tx() still records TRACE_EXTERNAL_EVENT_RX - "
          "this is the original defect")

    rx_fn = re.search(
        r"int uart_adapter_trace_rx\([^)]*\)\s*\{(.*?)\n\}", txt, re.S)
    assert rx_fn, "uart_adapter_trace_rx() not found"
    check("TRACE_EXTERNAL_EVENT_RX" in rx_fn.group(1),
          "uart_adapter_trace_rx() must keep recording TRACE_EXTERNAL_EVENT_RX")
    check("TRACE_EXTERNAL_EVENT_TX" not in rx_fn.group(1),
          "uart_adapter_trace_rx() must not record a transmit type")
    print("PASS uart TX and RX use distinct trace event types")


def test_event_type_is_appended_not_inserted():
    """Trace records are binary. Inserting mid-enum renumbers every type
    after it, so an old trace file would decode to different events and
    the Qt frontend's mapping of 0..13 would break."""
    hdr = (SRC / "trace_schema.h").read_text()
    enum_body = re.search(r"typedef enum \{(.*?)\}\s*trace_event_type_t",
                          hdr, re.S)
    assert enum_body, "trace_event_type_t enum not found"

    entries = re.findall(r"(TRACE_\w+)\s*=\s*(\d+)", enum_body.group(1))
    assert entries, "no enum members parsed"

    names = [n for n, _ in entries]
    values = [int(v) for _, v in entries]

    check("TRACE_EXTERNAL_EVENT_TX" in names,
          "TRACE_EXTERNAL_EVENT_TX missing from the enum")

    # Values must be unique and ascending in declaration order.
    check(len(set(values)) == len(values),
          f"duplicate event type values: {values}")
    check(values == sorted(values),
          f"event types must be declared in ascending order: {values}")

    # TX must be the highest value, i.e. appended at the end.
    tx_val = values[names.index("TRACE_EXTERNAL_EVENT_TX")]
    check(tx_val == max(values),
          f"TRACE_EXTERNAL_EVENT_TX={tx_val} must be the highest value "
          f"(append-only), max is {max(values)}")

    # Everything the binary format already defined must keep its value.
    for fixed_name, fixed_val in [
        ("TRACE_EXTERNAL_EVENT_RX", 1),
        ("TRACE_EVENT_DECODED", 2),
        ("TRACE_WORKLOAD_RELEASE", 3),
        ("TRACE_CPU_MIGRATION", 13),
        ("TRACE_GPIO_MARKER_HIGH", 14),
        ("TRACE_GPIO_MARKER_LOW", 15),
    ]:
        if fixed_name in names:
            got = values[names.index(fixed_name)]
            check(got == fixed_val,
                  f"{fixed_name} changed from {fixed_val} to {got} - this "
                  f"breaks existing binary traces and the Qt frontend")

    print(f"PASS event types are append-only "
          f"(0..{max(values)}, TX={tx_val})")


def test_event_type_has_name_string():
    txt = (SRC / "trace_schema.c").read_text()
    check("TRACE_EXTERNAL_EVENT_TX" in txt,
          "trace_event_to_string() has no case for TRACE_EXTERNAL_EVENT_TX")
    check("EXTERNAL_EVENT_TX" in txt,
          "TRACE_EXTERNAL_EVENT_TX missing from trace_event_to_string()")
    print("PASS TX has a name string")


def test_tx_survives_event_only_filtering():
    """A transmit is a real I/O boundary event. Dropping it in
    TRACE_MODE_EVENT_ONLY would hide the very thing this fix makes
    visible."""
    txt = (SRC / "trace_collector.c").read_text()
    fn = re.search(r"static int need_drop\(.*?\n\}", txt, re.S)
    assert fn, "need_drop() not found"
    body = fn.group(0)
    check("TRACE_EXTERNAL_EVENT_TX" in body,
          "need_drop() drops TRACE_EXTERNAL_EVENT_TX in EVENT_ONLY mode")
    print("PASS TX is retained by the EVENT_ONLY filter")


def test_analyzer_does_not_treat_tx_as_arrival():
    """external_event_time means 'an external arrival released this
    workload'. A transmit has the opposite causality - treating it as an
    arrival would invert the analysis."""
    txt = (SRC / "analyzer.c").read_text()
    case = re.search(
        r"case TRACE_EXTERNAL_EVENT_TX:(.*?)(?:case |default:)", txt, re.S)
    assert case, "analyzer.c has no case for TRACE_EXTERNAL_EVENT_TX"
    body = case.group(1)

    check("external_event_time" not in body,
          "analyzer sets external_event_time from a UART transmit - that "
          "inverts the causality, since TX is something we sent")
    check("uart_tx_count" in body,
          "analyzer should count transmits so TX is visible in the report")
    print("PASS analyzer treats TX as an outbound event, not an arrival")


if __name__ == "__main__":
    for fn in (test_tx_uses_dedicated_event_type,
               test_event_type_is_appended_not_inserted,
               test_event_type_has_name_string,
               test_tx_survives_event_only_filtering,
               test_analyzer_does_not_treat_tx_as_arrival):
        try:
            fn()
        except AssertionError as e:
            failures.append(f"{fn.__name__}: {e}")

    print()
    if failures:
        print(f"FAIL - {len(failures)} problem(s):")
        for f in failures:
            print(f"  - {f}")
        sys.exit(1)
    print("5/5 UART TX trace marker tests PASS")