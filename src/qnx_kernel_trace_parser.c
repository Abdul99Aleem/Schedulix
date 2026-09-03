#define _POSIX_C_SOURCE 200809L
#include "qnx_kernel_trace_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static KernelTraceEvent g_events[MAX_KERNEL_EVENTS];
static size_t g_event_count = 0;
static bool g_is_complete = true;
static uint64_t g_cycles_per_sec = 1000000000ULL;

#if defined(__QNX__)
#include <sys/traceparser.h>
#include <sys/trace.h>

static uint64_t g_last_time_64 = 0;
static bool g_base_time_set = false;

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

    // Timestamp reconstruction (rollover handling)
    uint64_t full_cycles = 0;
    const uint64_t *clk = (const uint64_t*)traceparser_get_info(state_ptr, _TRACEPARSER_INFO_CLK, NULL);
    if (!g_base_time_set && clk) {
        g_last_time_64 = *clk;
        g_base_time_set = true;
    }

    if (g_base_time_set) {
        full_cycles = (g_last_time_64 & 0xFFFFFFFF00000000ULL) | time;
        if (time < (g_last_time_64 & 0xFFFFFFFFULL)) {
            full_cycles += 0x100000000ULL;
        }
        g_last_time_64 = full_cycles;
    } else {
        full_cycles = time;
    }

    uint64_t cps_val = 1000000000ULL;
    const uint64_t *cps = (const uint64_t*)traceparser_get_info(state_ptr, _TRACEPARSER_INFO_CYCLES_PER_SEC, NULL);
    if (cps && *cps > 0) {
        cps_val = *cps;
        g_cycles_per_sec = *cps;
    }
    uint64_t timestamp_ns = (uint64_t)(((double)full_cycles / (double)cps_val) * 1000000000.0);

    KernelTraceEvent *ev = &g_events[g_event_count++];
    ev->timestamp_cycles = full_cycles;
    ev->timestamp_ns = timestamp_ns;
    ev->cpu = cpu;
    ev->event_type = event_type;
    ev->pid = pid;
    ev->tid = tid;

    return 0;
}

int qnx_kernel_trace_parse(const char *kev_path) {
    g_event_count = 0;
    g_is_complete = true;
    g_last_time_64 = 0;
    g_base_time_set = false;

    struct traceparser_state *state = traceparser_init(NULL);
    if (!state) return -1;

    traceparser_sort(state, _TRACEPARSER_SORT);

    traceparser_cs_range(state, NULL, thread_state_callback, _TRACE_PR_TH_C >> 10, 0, 255);
    traceparser_cs_range(state, NULL, thread_state_callback, _TRACE_SYSTEM_C >> 10, 0, 255);
    traceparser_cs(state, NULL, thread_state_callback, _TRACE_PR_TH_C >> 10, _NTO_TRACE_THRUNNING);
    traceparser_cs(state, NULL, thread_state_callback, _TRACE_PR_TH_C >> 10, _NTO_TRACE_THREADY);

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

size_t qnx_kernel_trace_get_events(KernelTraceEvent *out_events, size_t max_events) {
    size_t n = g_event_count < max_events ? g_event_count : max_events;
    if (out_events && n > 0) {
        memcpy(out_events, g_events, n * sizeof(KernelTraceEvent));
    }
    return n;
}

bool qnx_kernel_trace_is_complete(void) {
    return g_is_complete;
}

uint64_t qnx_kernel_trace_get_cycles_per_sec(void) {
    return g_cycles_per_sec;
}
