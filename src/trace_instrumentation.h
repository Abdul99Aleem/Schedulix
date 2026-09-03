#ifndef TRACE_INSTRUMENTATION_H
#define TRACE_INSTRUMENTATION_H

#include "trace_schema.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Phase 2: Semantic instrumentation.
 * Wrappers that emit to BOTH:
 *  - QNX TraceEvent() _NTO_TRACE_USER (when on QNX and enabled)
 *  - trace_collector SHM (always, for replay/analysis)
 * Never block workload thread on flush/printf.
 */

void trace_instr_init(void); /* must be after trace_collector_init */

int trace_emit(uint32_t event_type, uint32_t task_id, uint32_t activation_id, uint32_t corr_id, uint64_t arg0, uint64_t arg1);

/* Convenience semantic helpers */
int trace_external_event_rx(uint32_t source, uint32_t can_id, uint32_t dlc, uint32_t corr_id);
int trace_event_decoded(uint32_t task_id, uint32_t corr_id, uint32_t can_id);
int trace_workload_release(uint32_t task_id, uint32_t activation_id, uint32_t corr_id, uint64_t deadline_ns);
int trace_workload_ready(uint32_t task_id, uint32_t activation_id, uint32_t corr_id);
int trace_workload_start(uint32_t task_id, uint32_t activation_id, uint32_t corr_id);
int trace_workload_end(uint32_t task_id, uint32_t activation_id, uint32_t corr_id, uint64_t exec_ns);
int trace_workload_deadline(uint32_t task_id, uint32_t activation_id, uint32_t corr_id, int64_t slack_ns, uint64_t deadline_ns);
int trace_workload_block_begin(uint32_t task_id, uint32_t activation_id, uint32_t corr_id, uint32_t reason);
int trace_workload_block_end(uint32_t task_id, uint32_t activation_id, uint32_t corr_id);

#ifdef __cplusplus
}
#endif

#endif
