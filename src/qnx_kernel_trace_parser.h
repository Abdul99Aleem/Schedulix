#ifndef QNX_KERNEL_TRACE_PARSER_H
#define QNX_KERNEL_TRACE_PARSER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Number of trace events RETAINED for the correlator and for CLI display.
 *
 * This used to be a hard ceiling that silently truncated the stream: the first
 * MAX_KERNEL_EVENTS events were kept and everything after was discarded, which
 * on a 24 MB capture discarded well over 95% of the trace (and all of the
 * `_NTO_TRACE_START` state dump sat at the head, so the decode appeared to show
 * nothing but PID 1).
 *
 * It is now a RING: the most recent N events are kept, and all switch counting
 * is done incrementally as callbacks arrive. No event count in this trace can
 * run out of memory to be counted. Callers must use
 * qnx_kernel_trace_get_switches() for the count, not the length of the array. */
#define KERNEL_TRACE_RING 32768

typedef struct {
    uint64_t timestamp_cycles;
    uint64_t timestamp_ns;
    uint32_t cpu;
    uint32_t event_type; /* a STATE_* value from <sys/states.h> */
    uint32_t pid;
    uint32_t tid;
} KernelTraceEvent;

int qnx_kernel_trace_parse(const char *kev_path);

/* Drive the counting path with a fixed synthetic stream, bypassing
 * libtraceparser. Exists so the retention / counting / dump-exclusion logic can
 * be exercised ON THE TARGET, where the host mock never compiles (__QNX__ is
 * defined) and a .kev file may not be present. Uses the same absorb() path the
 * real callback uses, so it tests the shipped logic rather than a copy. */
void qnx_kernel_trace_test_feed(void);

/* Absorb a caller-supplied event. Same path the traceparser callback uses.
 * Lets a test build any event stream it likes (rollover, per-CPU overrun gap,
 * state dump, ring overflow) and then read the results back through the normal
 * accessors. */
void qnx_kernel_trace_test_emit(uint32_t cpu, uint32_t event_type, uint32_t pid,
                                uint32_t tid, uint64_t cycles);

/* Test setup: fix the clock rate so gap detection and the dump-burst window are
 * deterministic. */
void qnx_kernel_trace_test_set_clock(uint64_t cycles_per_sec, uint64_t clk_base,
                                     uint32_t clk_msb, bool have_msb);

/* Clear all accumulated state, so each test starts from a known baseline. */
void qnx_kernel_trace_test_reset(void);

/* Retained (most recent) events. Returns the retained count, and copies
 * oldest-to-newest into out_events. With out_events==NULL this is a query. */
size_t qnx_kernel_trace_get_events(KernelTraceEvent *out_events, size_t max_events);

/* Total thread events seen in the file, including any not retained. */
uint64_t qnx_kernel_trace_get_events_seen(void);

/* Authoritative context-switch count: RUNNING transitions where the running tid
 * on a CPU changed, counted incrementally across the whole file. */
uint64_t qnx_kernel_trace_get_switches(void);

/* TRUE when the count is defensible as a measurement, i.e. no events were lost
 * and no backward timestamp step was observed. When false, the count is only
 * meaningful as a lower bound and must be reported as such. */
bool qnx_kernel_trace_is_lossless(void);

/* Number of per-CPU forward timestamp discontinuities. Per the SAT guide's
 * "Buffer overruns" chapter, when a ring buffer overruns the kernel "simply
 * drops all events logged for that CPU until that next buffer becomes free" and
 * the resumed timestamps "show a discontinuity". Each of these is therefore a
 * window in which that CPU's events are MISSING -- this is data loss, which is
 * why a count across one is a lower bound, not an upper bound. */
size_t qnx_kernel_trace_get_gaps(void);

/* Total cycles lost across all detected gaps, as a lower bound on the missing
 * interval. */
uint64_t qnx_kernel_trace_get_dropped_cycles(void);

/* Backward timestamp steps observed during decode. With _TRACEPARSER_SORT the
 * callbacks are already delivered in chronological order, so this indicates a
 * clock-anchor problem in the reconstruction rather than file reordering. */
size_t qnx_kernel_trace_get_regressions(void);

uint64_t qnx_kernel_trace_get_cycles_per_sec(void);

/* Human-readable name for a STATE_* thread event value, for CLI output.
 * Returns "UNKNOWN" for values outside the state range. */
const char* thread_state_name(uint32_t event_type);

/* Number of leading events skipped as the _NTO_TRACE_START state dump (a
 * snapshot of every live thread, not scheduling activity). */
size_t qnx_kernel_trace_get_dump_events(void);

/* Timestamp of the first event after the state dump, so callers can present
 * times relative to the start of real scheduling instead of to the dump. */
uint64_t qnx_kernel_trace_get_counting_start_cycles(void);

/* Width in cycles of the opening burst emitted by _NTO_TRACE_START (the
 * initial system-state dump). Events inside this window at the head of a trace
 * are a thread snapshot, not scheduling. */
uint64_t qnx_kernel_trace_dump_burst_cycles(void);

#endif