#define _POSIX_C_SOURCE 200809L
#include "qnx_kernel_trace_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__QNX__)
#include <sys/traceparser.h>
#include <sys/trace.h>
#endif

/* Matches MAX_CPUS in scheduler_correlator.h, which cannot be included here
 * because it includes this header. */
#define KERNEL_TRACE_MAX_CPUS 32

/* Retained window. Counting is done incrementally, so this only bounds the
 * memory used for the correlator's input and for CLI display. */
static KernelTraceEvent g_ring[KERNEL_TRACE_RING];
static size_t g_ring_head = 0;   /* index of the oldest retained event when full */
static size_t g_ring_count = 0;  /* retained events, <= KERNEL_TRACE_RING */

static uint64_t g_events_seen = 0;
static uint64_t g_switches = 0;
static uint64_t g_cycles_per_sec = 0;   /* resolved from the trace on first event */
static size_t   g_dump_events = 0;
static uint64_t g_counting_start_cycles = 0;
static bool     g_have_counting_start = false;

/* Per-CPU state for incremental switch counting. */
static uint32_t g_cur_tid[KERNEL_TRACE_MAX_CPUS];
static bool     g_have_cur[KERNEL_TRACE_MAX_CPUS];
static uint64_t g_cpu_last_cycles[KERNEL_TRACE_MAX_CPUS];
static bool     g_have_cpu_last[KERNEL_TRACE_MAX_CPUS];

/* Data-loss accounting. */
static size_t   g_gaps = 0;
static uint64_t g_dropped_cycles = 0;
static size_t   g_regressions = 0;
static uint64_t g_prev_cycles = 0;
static bool     g_have_prev = false;

/* Clock reassembly.
 *
 * Per the SAT guide's "Timestamps" chapter, the timestamp stored with each event
 * is only the 32 Least Significant Bits of the 64-bit cycle counter, and an
 * _NTO_TRACE_CONTROLTIME control event carrying the 32 Most Significant Bits is
 * issued *whenever the 32-bit portion rolls over* -- "A 1-GHz clock rolls over
 * every 4.29 seconds". It is therefore a rollover anchor, NOT a per-event base.
 *
 * The previous code built a single anchor the first time it saw one
 * (msb | time) and then added each event's own LSB on top of it
 * (base + time), double-counting the LSB and pinning every later event to that
 * one rollover. Correct reconstruction is a bitwise OR against the current MSB. */
static uint64_t g_clk_msb = 0;
static bool     g_have_msb = false;
static uint64_t g_clk_base = 0;  /* cycle count at trace start, before any rollover */
static bool     g_have_clk_base = false;

/* Timestamp of the very first event, needed to locate the end of the opening
 * state-dump burst even after the retained ring has wrapped. */
static uint64_t g_first_cycles = 0;
static bool     g_have_first_cycles = false;

/* Gap threshold: a per-CPU forward jump larger than this is treated as a buffer
 * overrun window. One millisecond of cycles is generous for a machine with an
 * idle thread, but it is deliberately coarse -- a false positive costs one
 * "lossy" verdict, a false negative would report a lossy trace as clean. */
static uint64_t g_gap_cycles = 0;

static void reset_state(void) {
    g_ring_head = 0;
    g_ring_count = 0;
    g_events_seen = 0;
    g_switches = 0;
    g_dump_events = 0;
    g_counting_start_cycles = 0;
    g_have_counting_start = false;
    g_gaps = 0;
    g_dropped_cycles = 0;
    g_regressions = 0;
    g_prev_cycles = 0;
    g_have_prev = false;
    g_clk_msb = 0;
    g_have_msb = false;
    g_clk_base = 0;
    g_have_clk_base = false;
    g_first_cycles = 0;
    g_have_first_cycles = false;
    g_gap_cycles = 0;
    memset(g_cur_tid, 0, sizeof(g_cur_tid));
    memset(g_have_cur, 0, sizeof(g_have_cur));
    memset(g_cpu_last_cycles, 0, sizeof(g_cpu_last_cycles));
    memset(g_have_cpu_last, 0, sizeof(g_have_cpu_last));
}

static void ring_push(const KernelTraceEvent *ev) {
    if (g_ring_count < KERNEL_TRACE_RING) {
        g_ring[g_ring_count++] = *ev;
    } else {
        g_ring[g_ring_head] = *ev;
        g_ring_head = (g_ring_head + 1) % KERNEL_TRACE_RING;
    }
}

#if defined(__QNX__)

/* _NTO_TRACE_CONTROL class, event _NTO_TRACE_CONTROLTIME. The payload word is
 * the 32-bit MSB of the cycle counter at the moment of rollover. */
static int control_time_callback(struct traceparser_state* state_ptr, void* user_data,
                                 unsigned header, unsigned time, unsigned* buffer, unsigned buffer_len) {
    (void)state_ptr; (void)user_data; (void)header; (void)time;
    if (buffer_len >= 1) {
        g_clk_msb = (uint64_t)buffer[0] << 32;
        g_have_msb = true;
    }
    return 0;
}

/* Absorb one thread-state event. This is the whole of the counting logic, and it
 * is deliberately factored out of the libtraceparser callback so the same code
 * can be driven by the synthetic feed below -- that is what makes the behaviour
 * testable on the target, not just on a host with no .kev file. */
static void absorb(uint32_t cpu, uint32_t event_type, uint32_t pid, uint32_t tid,
                   uint64_t full_cycles) {
    /* Integer arithmetic only; the previous double round-trip lost precision
     * around 1.9e11 cycles. */
    uint64_t timestamp_ns = 0;
    if (g_cycles_per_sec) {
        timestamp_ns = (full_cycles / g_cycles_per_sec) * 1000000000ULL
                     + ((full_cycles % g_cycles_per_sec) * 1000000000ULL) / g_cycles_per_sec;
    }

    /* Backward step. Note _TRACEPARSER_SORT means callbacks arrive already
     * ordered, so this signals a clock-anchor problem, not file reordering. */
    if (g_have_prev && full_cycles < g_prev_cycles) {
        g_regressions++;
    }
    g_prev_cycles = full_cycles;
    g_have_prev = true;

    /* Per-CPU forward discontinuity == a buffer-overrun window in which this
     * CPU's events were dropped by the kernel. This is the loss signal. */
    uint32_t c = (cpu < KERNEL_TRACE_MAX_CPUS) ? cpu : 0;
    if (g_have_cpu_last[c] && g_gap_cycles && full_cycles > g_cpu_last_cycles[c]) {
        uint64_t jump = full_cycles - g_cpu_last_cycles[c];
        if (jump > g_gap_cycles) {
            g_gaps++;
            g_dropped_cycles += jump;
        }
    }
    g_cpu_last_cycles[c] = full_cycles;
    g_have_cpu_last[c] = true;

    g_events_seen++;

    KernelTraceEvent ev = {
        full_cycles, timestamp_ns, cpu, event_type, pid, tid
    };
    ring_push(&ev);

    /* Skip the _NTO_TRACE_START state dump: a snapshot of every live thread,
     * emitted as a burst at the head of the file, not scheduling activity. It is
     * bounded by the opening burst window. This only works because the
     * timestamps above are reconstructed correctly.
     *
     * g_first_cycles is kept separately: once the ring wraps, g_ring[0] is no
     * longer the first event of the file. */
    if (!g_have_first_cycles) {
        g_first_cycles = full_cycles;
        g_have_first_cycles = true;
    }

    if (!g_have_counting_start) {
        uint64_t burst = qnx_kernel_trace_dump_burst_cycles();
        if (g_events_seen > 1 && burst) {
            uint64_t span = (full_cycles >= g_first_cycles)
                          ? (full_cycles - g_first_cycles) : 0;
            if (span > burst) {
                /* Past the opening burst: everything before here is the dump. */
                g_dump_events = (size_t)(g_events_seen - 1);
                g_counting_start_cycles = full_cycles;
                g_have_counting_start = true;
            }
        }
    }

    /* Incremental context-switch counting: a RUNNING event whose tid differs
     * from the thread currently running on that CPU. */
    if (event_type == 1 /* STATE_RUNNING */) {
        if (g_have_counting_start) {
            if (g_have_cur[c] && g_cur_tid[c] != tid) {
                g_switches++;
            }
            g_cur_tid[c] = tid;
            g_have_cur[c] = true;
        }
    }
}

#if defined(__QNX__)
static int thread_state_callback(struct traceparser_state* state_ptr, void* user_data,
                                 unsigned header, unsigned time, unsigned* buffer, unsigned buffer_len) {
    (void)user_data;

    uint32_t event_type = _NTO_TRACE_GETEVENT(header);
    uint32_t cpu = _NTO_TRACE_GETCPU(header);
    uint32_t pid = 0;
    uint32_t tid = 0;

    /* Thread state events carry the process and thread IDs in BOTH fast and wide
     * mode -- the SAT guide's "Current Trace Events and Data" appendix lists them
     * as "Fast: pid, tid" and "Wide: pid, tid, priority, policy, ...". So there
     * is no reason to pay for wide mode, and doing so multiplies the file size
     * for data this parser discards. */
    if (buffer_len >= 2) {
        pid = buffer[0];
        tid = buffer[1];
    }

    /* _TRACEPARSER_INFO_CYCLES_PER_SEC returns unsigned*, NOT uint64_t*.
     * Casting to uint64_t* read 8 bytes across two 4-byte fields. */
    if (g_cycles_per_sec == 0) {
        const unsigned *cps = (const unsigned*)traceparser_get_info(state_ptr,
                        _TRACEPARSER_INFO_CYCLES_PER_SEC, NULL);
        if (cps && *cps > 0) g_cycles_per_sec = (uint64_t)*cps;
    }
    if (g_gap_cycles == 0 && g_cycles_per_sec) {
        g_gap_cycles = g_cycles_per_sec / 1000ULL; /* 1 ms */
    }

    /* The trace's starting cycle count is only known once the file header has
     * been read, so this is resolved lazily on the first event. Before the first
     * rollover the event LSB is an offset from that base; afterwards the MSB from
     * the CONTROLTIME anchor is authoritative. */
    if (!g_have_clk_base) {
        const uint64_t *ci = (const uint64_t*)traceparser_get_info(state_ptr,
                        _TRACEPARSER_INFO_CLK_INTIAL, NULL);
        if (!ci) {
            ci = (const uint64_t*)traceparser_get_info(state_ptr,
                        _TRACEPARSER_INFO_CLK, NULL);
        }
        if (ci) {
            g_clk_base = *ci;
            g_have_clk_base = true;
        }
    }

    uint64_t full_cycles;
    if (g_have_msb) {
        full_cycles = g_clk_msb | (uint64_t)time;
    } else if (g_have_clk_base) {
        full_cycles = g_clk_base + (uint64_t)time;
    } else {
        full_cycles = (uint64_t)time;
    }

    absorb(cpu, event_type, pid, tid, full_cycles);
    return 0;
}
#endif /* __QNX__ */

int qnx_kernel_trace_parse(const char *kev_path) {
    reset_state();

    /* Guard before handing the path to libtraceparser. traceparser() does not
     * tolerate a NULL filename and faults inside the library, so the rejection
     * has to happen here. */
    if (!kev_path) return -1;

    struct traceparser_state *state = traceparser_init(NULL);
    if (!state) return -1;

    /* Sort so callbacks are delivered in chronological order. The SAT guide is
     * explicit that on a buffer overrun "the timestamps will definitely be
     * scrambled, with entire blocks of events out of chronological order", and
     * sorting is what lets the per-CPU gap detector below see them as gaps
     * rather than as ordering noise. */
    traceparser_sort(state, _TRACEPARSER_SORT);

    /* External class/event numbers from <sys/trace.h>, where _NTO_TRACE_THREAD is
     * the Thread class and its events are the STATE_* values. A range over the
     * whole class captures every state change, which is what counting needs. */
    traceparser_cs_range(state, NULL, thread_state_callback,
                         _NTO_TRACE_THREAD, 0, _TRACE_MAX_TH_STATE_NUM);

    /* Rollover anchor, so the MSB is known before the first event that needs it. */
    traceparser_cs(state, NULL, control_time_callback,
                   _NTO_TRACE_CONTROL, _NTO_TRACE_CONTROLTIME);

    int rc = traceparser(state, NULL, kev_path);
    traceparser_destroy(&state);

    /* Nothing was ever outside an opening burst (short or empty capture). */
    if (!g_have_counting_start) {
        g_dump_events = (size_t)g_events_seen;
        g_have_counting_start = true;
    }
    if (g_cycles_per_sec == 0) g_cycles_per_sec = 1000000000ULL;
    return rc;
}

#endif /* __QNX__ */

/* Synthetic stream. Compiled on BOTH host and QNX, because the counting logic
 * under test (ring retention, incremental switch counting, dump exclusion, gap
 * detection) is identical on either. Tests use this to drive the logic without
 * needing a .kev file, which is what makes it testable ON THE TARGET -- the
 * host-only mock branch never compiles there, because __QNX__ is defined.
 *
 * These feed the same absorb() path the real libtraceparser callback uses, so
 * they exercise the shipped logic rather than a copy of it. */
void qnx_kernel_trace_test_emit(uint32_t cpu, uint32_t event_type, uint32_t pid,
                                uint32_t tid, uint64_t cycles) {
    absorb(cpu, event_type, pid, tid, cycles);
}

void qnx_kernel_trace_test_set_clock(uint64_t cycles_per_sec, uint64_t clk_base,
                                     uint32_t clk_msb, bool have_msb) {
    g_cycles_per_sec = cycles_per_sec;
    g_clk_base = clk_base;
    g_have_clk_base = true;
    g_clk_msb = (uint64_t)clk_msb << 32;
    g_have_msb = have_msb;
    g_gap_cycles = cycles_per_sec / 1000ULL;
}

void qnx_kernel_trace_test_reset(void) {
    reset_state();
    g_cycles_per_sec = 1000000000ULL;
    g_gap_cycles = g_cycles_per_sec / 1000ULL;
    g_have_clk_base = true;
    g_clk_base = 0;
}

#if !defined(__QNX__)
/* On a host build there is no libtraceparser to drive, so parse() has nothing
 * real to read. Reset and leave the state empty rather than faking a parse. */
int qnx_kernel_trace_parse(const char *kev_path) {
    (void)kev_path;
    qnx_kernel_trace_test_reset();
    return 0;
}
#endif

size_t qnx_kernel_trace_get_events(KernelTraceEvent *out_events, size_t max_events) {
    if (!out_events) return g_ring_count;
    size_t n = g_ring_count < max_events ? g_ring_count : max_events;
    for (size_t i = 0; i < n; i++) {
        size_t idx = g_ring_count < KERNEL_TRACE_RING ? i
                                                      : (g_ring_head + i) % KERNEL_TRACE_RING;
        out_events[i] = g_ring[idx];
    }
    return n;
}

uint64_t qnx_kernel_trace_get_events_seen(void) { return g_events_seen; }
uint64_t qnx_kernel_trace_get_switches(void)     { return g_switches; }
size_t   qnx_kernel_trace_get_gaps(void)         { return g_gaps; }
uint64_t qnx_kernel_trace_get_dropped_cycles(void){ return g_dropped_cycles; }
size_t   qnx_kernel_trace_get_regressions(void)  { return g_regressions; }
size_t   qnx_kernel_trace_get_dump_events(void)   { return g_dump_events; }
uint64_t qnx_kernel_trace_get_counting_start_cycles(void) { return g_counting_start_cycles; }

/* Lossless means no detected loss and no clock-anchor trouble. If either is
 * present the switch count spans missing intervals, so it is a lower bound. */
bool qnx_kernel_trace_is_lossless(void) {
    return (g_gaps == 0) && (g_regressions == 0);
}

uint64_t qnx_kernel_trace_dump_burst_cycles(void) {
    uint64_t cps = g_cycles_per_sec ? g_cycles_per_sec : 1000000000ULL;
    return cps / 1000000ULL; /* 1 ms worth of cycles */
}

/* STATE_* values from <sys/states.h>. Thread events are reported using these
 * numbers directly. Kept in step with states.h, which defines up to
 * STATE_SEND_NOTIFY (30) with STATE_MAX == 63. */
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
    case 26: return "MUON_MUTEX";
    case 27: return "TRACEBUFFER";
    case 28: return "INTR_ATTACH_EV";
    case 29: return "TIMER_DELEGATE";
    case 30: return "SEND_NOTIFY";
    default: return "UNKNOWN";
    }
}

uint64_t qnx_kernel_trace_get_cycles_per_sec(void) { return g_cycles_per_sec; }