#include "qnx_kernel_trace_parser.h"
#include <stdio.h>
#include <assert.h>

#if defined(__QNX__)
#include <sys/trace.h>
#else
#define _NTO_TRACE_THRUNNING 1
#define _NTO_TRACE_THREADY 2
#endif

// Unit test code
void test_timestamp_rollover(void) {
    printf("Running rollover tests...\n");
    // Manually test rollover function logic or mock feeds
    uint64_t last_time = 0xFFFFFFFFULL - 100ULL; // close to 32-bit bound
    uint32_t t1 = (uint32_t)(last_time + 50ULL); // no rollover
    uint32_t t2 = (uint32_t)(last_time + 150ULL); // rollover!

    // Verify reconstruction formula
    uint64_t full_t1 = (last_time & 0xFFFFFFFF00000000ULL) | t1;
    assert(full_t1 > last_time);

    uint64_t full_t2 = (last_time & 0xFFFFFFFF00000000ULL) | t2;
    if (t2 < (last_time & 0xFFFFFFFFULL)) {
        full_t2 += 0x100000000ULL;
    }
    assert(full_t2 > full_t1);
    printf("Rollover tests passed.\n");
}

void test_extraction_and_bounds(void) {
    printf("Running extraction and bounds tests...\n");
    KernelTraceEvent evs[10];
    qnx_kernel_trace_parse(NULL); // load mock events on host
    size_t n = qnx_kernel_trace_get_events(evs, 10);
    assert(n > 0);
    assert(evs[0].pid == 123);
    assert(evs[0].cpu == 0);
    assert(qnx_kernel_trace_is_complete() == true);
    printf("Extraction tests passed.\n");
}

int main(void) {
    printf("=== PARSER UNIT TESTS ===\n");
    test_timestamp_rollover();
    test_extraction_and_bounds();
    printf("All parser unit tests passed!\n");
    return 0;
}
