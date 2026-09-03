#ifndef ANALYZER_H
#define ANALYZER_H

#include "trace_schema.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Phase 4/5: Correlation + Analysis Engine.
 * Consumes A) Schedulix semantic trace, B) QNX kernel trace (via tracelogger .kev)
 * For host/CI, kernel trace is synthetic/mocked via kernel_trace_mock.
 */

typedef enum {
    RC_HIGH_PRIO_PREEMPT = 1,
    RC_SAME_PRIO_CONTENTION,
    RC_MUTEX_BLOCK,
    RC_IPC_WAIT,
    RC_IRQ_INTERFERENCE,
    RC_CPU_CONTENTION,
    RC_CPU_MIGRATION,
    RC_LONG_EXEC,
    RC_RELEASE_DELAY,
    RC_TRACE_LOSS,
    RC_UNKNOWN
} root_cause_t;

const char* root_cause_to_string(root_cause_t rc);

typedef enum {
    EVIDENCE_UNKNOWN = 0,
    EVIDENCE_INFERRED = 1,
    EVIDENCE_CONFIRMED = 2
} evidence_level_t;

typedef struct {
    uint32_t task_id;
    uint32_t activation_id;
    uint32_t correlation_id;
    uint64_t external_event_time;
    uint64_t release_time;
    uint64_t ready_time;
    uint64_t first_run_time;
    uint64_t finish_time;
    uint64_t deadline_ns;
    uint64_t execution_ns; /* sum RUNNING */
    uint64_t response_ns;  /* finish - release */
    int64_t  slack_ns;     /* deadline - response */
    int      deadline_miss;
    uint64_t ready_wait_ns; /* first_run - ready */
    uint64_t blocked_ns;
    uint64_t preempt_ns;
    uint32_t preemptions;
    uint32_t cswitches;
    int      cpu_first;
    int      cpu_last;
    int      priority;
    root_cause_t root_cause;
    int      confidence; /* 0-100 */
    uint32_t evidence_level; /* evidence_level_t */
    uint32_t interfering_pid;
    uint32_t interfering_tid;
    uint32_t interfering_priority;
    uint64_t lateness_ns;
} activation_analysis_t;

typedef struct {
    uint32_t task_id;
    uint64_t activations;
    uint64_t misses;
    double   miss_ratio;
    double   mean_response_ms;
    double   p50_ms, p95_ms, p99_ms, max_ms;
    double   worst_ms;
} per_task_stats_t;

/* Analyzer API */
int analyzer_load_trace_file(const char *path); /* binary trace from collector */
int analyzer_get_header(trace_shm_header_t *out);
int analyzer_set_kernel_trace(const sched_trace_record_t *krecs, size_t n); /* optional */
size_t analyzer_correlate(activation_analysis_t **out, size_t *out_n); /* returns array malloc'd */
void analyzer_free(activation_analysis_t *p);

int analyzer_per_task_stats(const activation_analysis_t *acts, size_t n, per_task_stats_t *stats, size_t *stats_n);
int analyzer_print_report(const activation_analysis_t *acts, size_t n, const trace_shm_header_t *hdr);

/* Delay attribution helper */
void analyzer_delay_attribution(const activation_analysis_t *a, char *buf, size_t len);

/* Stress helpers */
int analyzer_load_vs_latency(const activation_analysis_t *acts, size_t n);
int analyzer_write_json(const char *path, const activation_analysis_t *acts, size_t n, const trace_shm_header_t *hdr);

#ifdef __cplusplus
}
#endif

#endif
