#ifndef TRACE_COLLECTOR_H
#define TRACE_COLLECTOR_H

#include "trace_schema.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Phase 3: Shared-memory ring buffer collector.
 * Principle: workload threads never block on flush/printf.
 * Local atomic write_index, flush on threshold/period/shutdown/deadline.
 *
 * Automotive best practice: per-CPU/per-producer lock-minimized, fixed-size,
 * overflow accounting, flight-recorder trigger.
 */

/* Default SHM object name (POSIX shm_open). On QNX same API. */
#define TRACE_SHM_DEFAULT_NAME "/schedulix_trace"

int  trace_collector_init(const char *shm_name, uint32_t capacity, trace_mode_t mode, uint32_t sampling_n);
void trace_collector_shutdown(void);
int  trace_collector_is_initialized(void);

/* Non-blocking record — returns 0 on stored, -1 dropped, -2 not inited */
int trace_collector_record(const sched_trace_record_t *rec);
int trace_collector_record_simple(uint32_t event_type, uint32_t task_id, uint32_t activation_id, uint32_t corr_id, uint64_t arg0, uint64_t arg1);

/* Flush to file (binary replay). Does not block producer beyond memcpy. */
int trace_collector_flush(const char *path);
int trace_collector_flush_range(const char *path, uint32_t max_records); /* rolling window */

/* For deadline flight-recorder: freeze last N records */
int trace_collector_snapshot(const char *path, uint32_t window_records);

/* Stats */
void trace_collector_get_header(trace_shm_header_t *out);
uint64_t trace_collector_dropped(void);
uint32_t trace_collector_high_watermark(void);

/* Sampling control */
void trace_collector_set_mode(trace_mode_t mode, uint32_t n);

/* Direct SHM access for analyzer (zero-copy) */
const trace_shm_header_t* trace_collector_header_ro(void);
const sched_trace_record_t* trace_collector_records_ro(void);

/* For host tests: in-memory fallback when shm_open not available */
int trace_collector_init_heap(uint32_t capacity, trace_mode_t mode, uint32_t sampling_n);

#ifdef __cplusplus
}
#endif

#endif /* TRACE_COLLECTOR_H */
