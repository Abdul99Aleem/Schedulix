#ifndef QNX_KERNEL_TRACE_PARSER_H
#define QNX_KERNEL_TRACE_PARSER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define MAX_KERNEL_EVENTS 32768

typedef struct {
    uint64_t timestamp_cycles;
    uint64_t timestamp_ns;
    uint32_t cpu;
    uint32_t event_type; /* a STATE_* value from <sys/states.h> */
    uint32_t pid;
    uint32_t tid;
} KernelTraceEvent;

int qnx_kernel_trace_parse(const char *kev_path);
size_t qnx_kernel_trace_get_events(KernelTraceEvent *out_events, size_t max_events);
bool qnx_kernel_trace_is_complete(void);
uint64_t qnx_kernel_trace_get_cycles_per_sec(void);

/* Human-readable name for a STATE_* thread event value, for CLI output.
 * Returns "UNKNOWN" for values outside the state range. */
const char* thread_state_name(uint32_t event_type);

/* Number of timestamp regressions found while decoding. Non-zero means the
 * kernel overwrote in-progress event buffers, so the file has blocks out of
 * chronological order and any switch count derived from it is an UPPER BOUND,
 * not a measurement. See the SAT guide's "Timestamps" chapter. */
size_t qnx_kernel_trace_get_out_of_order(void);

#endif