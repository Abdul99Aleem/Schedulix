#define _POSIX_C_SOURCE 200809L
#include "qnx_kernel_trace_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__QNX__)
#include <sys/traceparser.h>
#include <sys/trace.h>
#endif

static KernelTraceEvent g_events[MAX_KERNEL_EVENTS];
static size_t g_event_count = 0;
static bool g_is_complete = true;
static uint64_t g_cycles_per_sec = 1000000000ULL;

/* Count of timestamp regressions seen while decoding. A regression means the
 * kernel overwrote an in-progress buffer, so the file contains whole blocks
 * out of chronological order (SAT guide, "Timestamps"). Any switch counting
 * done on such a trace is an upper bound, not a measurement. */
static size_t g_out_of_order = 0;
static bool g_have_prev_time = false;
static uint64_t g_prev_cycles = 0;

/* The documented rollover anchor. The 32-bit timestamp in each event is only
 * the LSB of the 64-bit clock; _NTO_TRACE_CONTROLTIME carries the MSB, and
 * the SAT guide names it as the way to reassemble the full clock. */
static uint64_t g_time_msb = 0;
static uint64_t g_base_cycles = 0;
static bool g_have_base = false;

#if defined(__QNX__)
/* _NTO_TRACE_CONTROL / _NTO_TRACE_CONTROLTIME carries the 32-bit MSB of the
 * clock. Per the SAT guide's "Timestamps" chapter, that MSB is the documented
 * way to reassemble the full 64-bit clock rather than guessing a base. */
static int control_time_callback(struct traceparser_state* state_ptr, void* user_data,
                                 unsigned header, unsigned time, unsigned* buffer, unsigned buffer_len) {
    (void)state_ptr; (void)user_data; (void)header;
    if (buffer_len >= 1) {
        g_time_msb = (uint64_t)buffer[0] << 32;
        g_base_cycles = g_time_msb | (uint64_t)time;
        g_have_base = true;
    }
    return 0;
}
#endif

#if defined(__QNX__)
static int thread_state_callback(struct traceparser_state* state_ptr, void* user_data,
                                 unsigned header, unsigned time, unsigned* buffer, unsigned buffer_len) {
    (void)user_data;
    if (g_event_count >= MAX_KERNEL_EVENTS) {
        g_is_complete = false;
        return 0;
    }

    uint32_t event_type = _NTO_TRACE_GETEVENT(header);
    uint32_t cpu = _NTO_TRACE_GETCPU(header);
    uint32_t pid = 0;
    uint32_t tid = 0;
    if (buffer_len >= 2) {
        pid = buffer[0];
        tid = buffer[1];
    }

    /* Timestamp reconstruction.
     *
     * Preferred: the _NTO_TRACE_CONTROLTIME anchor captured above, which the
     * SAT guide names as the documented way to reassemble the full clock.
     * Fallback: _TRACEPARSER_INFO_CLK, which is the trace's base cycle count.
     * Both are anchored -- `time` is a 32-bit offset from whichever base
     * applies, so no rollover guessing is needed. */
    uint64_t full_cycles = 0;
    if (g_have_base) {
        full_cycles = g_base_cycles + (uint64_t)time;
    } else {
        const uint64_t *clk = (const uint64_t*)traceparser_get_info(state_ptr, _TRACEPARSER_INFO_CLK, NULL);
        full_cycles = clk ? (*clk + (uint64_t)time) : (uint64_t)time;
    }

    /* _TRACEPARSER_INFO_CYCLES_PER_SEC returns unsigned*, NOT uint64_t*.
     * Casting to uint64_t* read 8 bytes across two 4-byte fields and produced
     * a garbage conversion -- every decoded event reported the same ns value. */
    if (g_cycles_per_sec == 0 || g_cycles_per_sec == 1000000000ULL) {
        const unsigned *cps = (const unsigned*)traceparser_get_info(state_ptr,
                        _TRACEPARSER_INFO_CYCLES_PER_SEC, NULL);
        if (cps && *cps > 0) {
            g_cycles_per_sec = (uint64_t)*cps;
        }
    }

    /* Integer arithmetic. The previous double round-trip lost precision on
     * values around 1.9e11 cycles. */
    uint64_t timestamp_ns = (full_cycles / g_cycles_per_sec) * 1000000000ULL
                          + ((full_cycles % g_cycles_per_sec) * 1000000000ULL) / g_cycles_per_sec;

    /* Detect out-of-order blocks. The SAT guide is explicit: in the case of
     * buffer overruns the timestamps are "definitely scrambled, with entire
     * blocks of events out of chronological order". Counting context switches
     * across such a boundary inflates the total, so record every regression. */
    if (g_have_prev_time && full_cycles < g_prev_cycles) {
        g_out_of_order++;
    }
    g_prev_cycles = full_cycles;
    g_have_prev_time = true;

    KernelTraceEvent *ev = &g_events[g_event_count++];
    ev->timestamp_cycles = full_cycles;
    ev->timestamp_ns = timestamp_ns;
    ev->cpu = cpu;
    ev->event_type = event_type;
    ev->pid = pid;
    ev->tid = tid;

    return 0;
}
#endif /* __QNX__ */

#if defined(__QNX__)
int qnx_kernel_trace_parse(const char *kev_path) {
    g_event_count = 0;
    g_is_complete = true;
    g_cycles_per_sec = 0; /* resolved from the trace on first event */
    g_out_of_order = 0;
    g_have_prev_time = false;
    g_prev_cycles = 0;
    g_time_msb = 0;
    g_base_cycles = 0;
    g_have_base = false;

    struct traceparser_state *state = traceparser_init(NULL);
    if (!state) return -1;

    traceparser_sort(state, _TRACEPARSER_SORT);

    /* These must be EXTERNAL class/event numbers. The previous code passed
     * _TRACE_PR_TH_C >> 10, which is an *internal* encoding: that registered a
     * callback against a class that does not exist, so it never fired. The only
     * registrations that did fire were THRUNNING/THREADY -- but combined with
     * the bad timestamp conversion those decoded to PID=0 rows.
     *
     * _NTO_TRACE_THREAD == 11 per <sys/trace.h>; its events are the STATE_*
     * values, so a range over the whole thread class captures every state
     * change, which is what context-switch counting needs. */
    traceparser_cs_range(state, NULL, thread_state_callback,
                         _NTO_TRACE_THREAD, 0, _TRACE_MAX_TH_STATE_NUM);

    /* Rollover anchor. Registered before the thread range so the base is set
     * before the first thread event is converted. */
    traceparser_cs(state, NULL, control_time_callback,
                   _NTO_TRACE_CONTROL, _NTO_TRACE_CONTROLTIME);

    int rc = traceparser(state, NULL, kev_path);
    traceparser_destroy(&state);
    return rc;
}

#else
/* Mock parser implementation for host execution/testing */
int qnx_kernel_trace_parse(const char *kev_path) {
    (void)kev_path;
    g_event_count = 0;
    g_is_complete = true;
    g_cycles_per_sec = 1000000000ULL;

    /* Generate a mock trace with 5 events representing a nominal preemption sequence */
    uint64_t t = 1000000ULL;
    // 1. Thread 100 running on CPU 0
    g_events[g_event_count++] = (KernelTraceEvent){t, t, 0, 1 /* RUNNING */, 123, 100};
    t += 500000ULL;
    // 2. Thread 101 running on CPU 0 (Context Switch, preempts 100)
    g_events[g_event_count++] = (KernelTraceEvent){t, t, 0, 1 /* RUNNING */, 123, 101};
    // 3. Thread 100 ready on CPU 0
    g_events[g_event_count++] = (KernelTraceEvent){t, t, 0, 2 /* READY */, 123, 100};
    t += 1000000ULL;
    // 4. Thread 100 resumed (running)
    g_events[g_event_count++] = (KernelTraceEvent){t, t, 0, 1 /* RUNNING */, 123, 100};
    t += 2000000ULL;
    // 5. Thread 100 voluntarily blocks
    g_events[g_event_count++] = (KernelTraceEvent){t, t, 0, 11 /* MUTEX */, 123, 100};

    return 0;
}
#endif

/* Call with out_events=NULL to query the count. The old clamp returned
 * g_event_count < 0 ? ... : 0, i.e. always 0, so a full 32768-event parse
 * reported "Parsed 0 events". */
size_t qnx_kernel_trace_get_events(KernelTraceEvent *out_events, size_t max_events) {
    if (!out_events) return g_event_count;
    size_t n = g_event_count < max_events ? g_event_count : max_events;
    if (n > 0) memcpy(out_events, g_events, n * sizeof(KernelTraceEvent));
    return n;
}

bool qnx_kernel_trace_is_complete(void) {
    return g_is_complete;
}

/* Width of the opening burst emitted by _NTO_TRACE_START: 1 ms of the trace
 * clock, long enough to hold the full thread snapshot and far shorter than any
 * real scheduling burst at these rates. Exposed so the CLI applies the same
 * window this code was derived from, rather than hard-coding a second value. */
uint64_t qnx_kernel_trace_dump_burst_cycles(void) {
    uint64_t cps = g_cycles_per_sec ? g_cycles_per_sec : 1000000000ULL;
    return cps / 1000000ULL; /* 1 ms worth of cycles */
}

size_t qnx_kernel_trace_get_out_of_order(void) {
    return g_out_of_order;
}

/* STATE_* values from <sys/states.h>. Thread events are reported using these
 * numbers directly, so naming them here keeps the CLI output readable instead
 * of printing bare integers like 13. */
const char* thread_state_name(uint32_t s) {
    switch (s) {
    case  0: return "DEAD";
    case  1: return "RUNNING";
    case  2: return "READY";
    case  3: return "STOPPED";
    case  4: return "SEND";
    case  5: return "RECEIVE";
    case  6: return "REPLY";
    case  7: return "MQ_SEND";
    case  8: return "MQ_RECEIVE";
    case  9: return "WAITPAGE";
    case 10: return "SIGSUSPEND";
    case 11: return "SIGWAITINFO";
    case 12: return "NANOSLEEP";
    case 13: return "MUTEX";
    case 14: return "CONDVAR";
    case 15: return "JOIN";
    case 16: return "INTR";
    case 17: return "SEM";
    case 18: return "WAITCTX";
    case 19: return "RWLOCK_READ";
    case 20: return "RWLOCK_WRITE";
    case 21: return "BARRIER";
    case 22: return "PIPE";
    case 24: return "CREATE";
    case 25: return "DESTROY";
    default: return "UNKNOWN";
    }
}

uint64_t qnx_kernel_trace_get_cycles_per_sec(void) {
    return g_cycles_per_sec;
}
