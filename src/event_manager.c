#include "event_manager.h"
#include "sched_event.h"
#include "trace_instrumentation.h"
#include "trace_schema.h"
#include "workload.h"
#include <stdio.h>
#include <inttypes.h>
#include <stdatomic.h>

static event_manager_stats_t g_stats = {0};
static _Atomic uint32_t g_correlation_seq = 1;
static _Atomic int g_quiet = 0;
void event_manager_set_quiet(int q){ atomic_store(&g_quiet, q?1:0); }
int event_manager_is_quiet(void){ return atomic_load(&g_quiet); }

/* Map CAN-decoded event to workload task */
static uint32_t event_to_task_id(event_type_t t){
    switch(t){
        case EVENT_BRAKE: return TASK_ID_BRAKE;
        case EVENT_ADAS: return TASK_ID_ADAS;
        case EVENT_DIAGNOSTIC: return TASK_ID_DIAG;
        default: return 0;
    }
}

void event_manager_dispatch(const sched_event_t *event) {
    if (!event) {
        if(!atomic_load(&g_quiet)){
            printf("EVENT MANAGER\n");
            printf("-------------\n");
            printf("DISPATCH  : null event (reject)\n");
        }
        return;
    }

    g_stats.total++;

    if(!atomic_load(&g_quiet)){
        printf("EVENT MANAGER\n");
        printf("-------------\n");
    }

    /* Correlation ID for CAN → release chain */
    uint32_t corr = atomic_fetch_add(&g_correlation_seq, 1);

    /* Semantic trace: EXTERNAL_EVENT_RX and EVENT_DECODED */
    trace_external_event_rx(1 /*CAN*/, event->can_id, event->data_len, corr);
    uint32_t task_id = event_to_task_id(event->type);
    if (task_id) trace_event_decoded(task_id, corr, event->can_id);

    switch (event->type) {
        case EVENT_BRAKE:
            g_stats.brake_count++;
            if(!atomic_load(&g_quiet)) printf("DISPATCH  : brake event\n");
            if (task_id) {
                workload_release_by_id(task_id, corr);
                trace_workload_release(task_id, 0, corr, 0);
            }
            break;
        case EVENT_ADAS:
            g_stats.adas_count++;
            if(!atomic_load(&g_quiet)) printf("DISPATCH  : ADAS event\n");
            if (task_id) {
                workload_release_by_id(task_id, corr);
                trace_workload_release(task_id, 0, corr, 0);
            }
            break;
        case EVENT_DIAGNOSTIC:
            g_stats.diagnostic_count++;
            if(!atomic_load(&g_quiet)) printf("DISPATCH  : diagnostic event\n");
            if (task_id) {
                workload_release_by_id(task_id, corr);
                trace_workload_release(task_id, 0, corr, 0);
            }
            break;
        case EVENT_UNKNOWN:
        default:
            g_stats.unknown_count++;
            if(!atomic_load(&g_quiet)) printf("DISPATCH  : UNKNOWN event - log/reject (ID=0x%03X)\n",
                   (unsigned)event->can_id);
            break;
    }

    /* Optional: verbose trace for analysis engine later */
#if 0
    printf("[trace] type=%s can_id=0x%03X ts=%" PRIu64 " ns\n",
           event_type_to_string(event->type),
           (unsigned)event->can_id,
           event->timestamp_ns);
#endif
}

void event_manager_get_stats(event_manager_stats_t *out) {
    if (out) *out = g_stats;
}

void event_manager_reset_stats(void) {
    g_stats.brake_count = 0;
    g_stats.adas_count = 0;
    g_stats.diagnostic_count = 0;
    g_stats.unknown_count = 0;
    g_stats.total = 0;
}
