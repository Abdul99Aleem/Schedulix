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
    uint32_t event_type; /* _NTO_TRACE_THRUNNING, etc. */
    uint32_t pid;
    uint32_t tid;
} KernelTraceEvent;

int qnx_kernel_trace_parse(const char *kev_path);
size_t qnx_kernel_trace_get_events(KernelTraceEvent *out_events, size_t max_events);
bool qnx_kernel_trace_is_complete(void);
uint64_t qnx_kernel_trace_get_cycles_per_sec(void);

#endif
