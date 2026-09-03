#ifndef SCHEDULER_CORRELATOR_H
#define SCHEDULER_CORRELATOR_H

#include "qnx_kernel_trace_parser.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define MAX_CORRELATOR_FACTS 65536
#define MAX_CPUS 32

typedef enum {
    FACT_RUNNING,    /* scheduled to run */
    FACT_READY,      /* moved to ready queue */
    FACT_BLOCKED,    /* blocked (mutex/condvar/sem) */
    FACT_TERMINATED  /* thread destroyed */
} fact_type_t;

typedef struct {
    uint64_t timestamp_ns;
    uint32_t cpu;
    uint32_t fact_type;
    uint32_t pid;
    uint32_t tid;
    uint32_t prev_pid;
    uint32_t prev_tid;
} SchedulingFact;

int scheduler_correlator_process(const KernelTraceEvent *events, size_t n);
size_t scheduler_correlator_get_facts(SchedulingFact *out_facts, size_t max_facts);
uint32_t scheduler_correlator_get_cswitches(void);
uint32_t scheduler_correlator_get_preemptions(void);

/* Helper to get the execution state of a thread at a given timestamp */
int scheduler_correlator_find_preemption_overlap(uint32_t target_pid, uint32_t target_tid,
                                                uint64_t start_ns, uint64_t end_ns,
                                                uint32_t *interfering_pid, uint32_t *interfering_tid,
                                                uint64_t *overlap_start_ns, uint64_t *overlap_end_ns);

#endif
