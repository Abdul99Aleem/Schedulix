#include "trace_schema.h"

const char* trace_event_to_string(uint32_t type) {
    switch(type){
        case TRACE_EXTERNAL_EVENT_RX: return "EXTERNAL_EVENT_RX";
        case TRACE_EVENT_DECODED: return "EVENT_DECODED";
        case TRACE_WORKLOAD_RELEASE: return "WORKLOAD_RELEASE";
        case TRACE_WORKLOAD_READY: return "WORKLOAD_READY";
        case TRACE_WORKLOAD_START: return "WORKLOAD_START";
        case TRACE_WORKLOAD_END: return "WORKLOAD_END";
        case TRACE_WORKLOAD_DEADLINE: return "WORKLOAD_DEADLINE";
        case TRACE_WORKLOAD_DEADLINE_MISS: return "WORKLOAD_DEADLINE_MISS";
        case TRACE_WORKLOAD_ABORT: return "WORKLOAD_ABORT";
        case TRACE_WORKLOAD_BLOCK_BEGIN: return "WORKLOAD_BLOCK_BEGIN";
        case TRACE_WORKLOAD_BLOCK_END: return "WORKLOAD_BLOCK_END";
        case TRACE_PREEMPTION: return "PREEMPTION";
        case TRACE_CPU_MIGRATION: return "CPU_MIGRATION";
        case TRACE_GPIO_MARKER_HIGH: return "GPIO_MARKER_HIGH";
        case TRACE_GPIO_MARKER_LOW:  return "GPIO_MARKER_LOW";
        case TRACE_EXTERNAL_EVENT_TX:    return "EXTERNAL_EVENT_TX";
        default: return "UNKNOWN";
    }
}
