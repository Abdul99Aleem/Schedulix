#include "trace_instrumentation.h"
#include "trace_collector.h"
#include "can_frame.h"
#include <string.h>
#include <time.h>
#include <sched.h>

#if defined(__QNX__)
#include <sys/trace.h>
#include <sys/neutrino.h>
#endif

static uint64_t mono_ns(void){
#if defined(_WIN32)
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts); return (uint64_t)ts.tv_sec*1e9+ts.tv_nsec;
#else
    struct timespec ts; if(clock_gettime(CLOCK_MONOTONIC,&ts)==0) return (uint64_t)ts.tv_sec*1000000000ULL+ts.tv_nsec; return 0;
#endif
}

static int cpu_now(void){
#if defined(__QNX__)
    /* QNX 8: SchedGetCpuNum() from <sys/neutrino.h>, SMP RPi4 0..3 */
    unsigned int c = SchedGetCpuNum();
    if (c < 256) return (int)c;
    return 0xFFFF;
#elif defined(__linux__)
    int c=sched_getcpu(); return c>=0?c:0xFFFF;
#else
    return 0xFFFF;
#endif
}

void trace_instr_init(void){ /* placeholder for TraceEvent class enable */ }

int trace_emit(uint32_t event_type, uint32_t task_id, uint32_t activation_id, uint32_t corr_id, uint64_t arg0, uint64_t arg1){
    sched_trace_record_t rec;
    memset(&rec,0,sizeof(rec));
    rec.timestamp_ns = mono_ns();
    rec.event_type = event_type;
    rec.task_id = task_id;
    rec.activation_id = activation_id;
    rec.correlation_id = corr_id;
    rec.arg0 = arg0;
    rec.arg1 = arg1;
    rec.cpu = (uint16_t)cpu_now();
    rec.priority = 0; /* filled by workload if known */
    int rc = trace_collector_record(&rec);
#if defined(__QNX__)
    /* QNX 8 user trace: insert structured user string event
     * Class _NTO_TRACE_USER, event _NTO_TRACE_INSERTSUSEREVENT lets System Profiler decode via xml. */
    // Best-effort, never block workload
    TraceEvent(_NTO_TRACE_INSERTSUSEREVENT, rec.event_type, rec.task_id, rec.activation_id, rec.correlation_id);
    (void)rc;
#endif
    return rc;
}

int trace_external_event_rx(uint32_t source, uint32_t can_id, uint32_t dlc, uint32_t corr_id){
    return trace_emit(TRACE_EXTERNAL_EVENT_RX, 0, 0, corr_id, ((uint64_t)can_id<<32)|dlc, source);
}
int trace_event_decoded(uint32_t task_id, uint32_t corr_id, uint32_t can_id){
    return trace_emit(TRACE_EVENT_DECODED, task_id, 0, corr_id, can_id, 0);
}
int trace_workload_release(uint32_t task_id, uint32_t activation_id, uint32_t corr_id, uint64_t deadline_ns){
    return trace_emit(TRACE_WORKLOAD_RELEASE, task_id, activation_id, corr_id, deadline_ns, 0);
}
int trace_workload_ready(uint32_t task_id, uint32_t activation_id, uint32_t corr_id){
    return trace_emit(TRACE_WORKLOAD_READY, task_id, activation_id, corr_id, 0,0);
}
int trace_workload_start(uint32_t task_id, uint32_t activation_id, uint32_t corr_id){
    return trace_emit(TRACE_WORKLOAD_START, task_id, activation_id, corr_id, 0,0);
}
int trace_workload_end(uint32_t task_id, uint32_t activation_id, uint32_t corr_id, uint64_t exec_ns){
    return trace_emit(TRACE_WORKLOAD_END, task_id, activation_id, corr_id, exec_ns,0);
}
int trace_workload_deadline(uint32_t task_id, uint32_t activation_id, uint32_t corr_id, int64_t slack_ns, uint64_t deadline_ns){
    uint32_t type = (slack_ns < 0) ? TRACE_WORKLOAD_DEADLINE_MISS : TRACE_WORKLOAD_DEADLINE;
    return trace_emit(type, task_id, activation_id, corr_id, (uint64_t)slack_ns, deadline_ns);
}
int trace_workload_block_begin(uint32_t task_id, uint32_t activation_id, uint32_t corr_id, uint32_t reason){
    return trace_emit(TRACE_WORKLOAD_BLOCK_BEGIN, task_id, activation_id, corr_id, reason,0);
}
int trace_workload_block_end(uint32_t task_id, uint32_t activation_id, uint32_t corr_id){
    return trace_emit(TRACE_WORKLOAD_BLOCK_END, task_id, activation_id, corr_id, 0,0);
}
