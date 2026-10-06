#ifndef TRACE_SCHEMA_H
#define TRACE_SCHEMA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* User trace event types — structured, not "Brake started" strings.
 * Mirrors AUTOSAR Log&Trace concept: schema + dynamic data.
 * QNX User class _NTO_TRACE_USER will carry these as payload.
 */
typedef enum {
    TRACE_INVALID              = 0,
    TRACE_EXTERNAL_EVENT_RX    = 1,  /* CAN/UART/synthetic received */
    TRACE_EVENT_DECODED        = 2,  /* CAN→event mapping */
    TRACE_WORKLOAD_RELEASE     = 3,  /* workload became eligible */
    TRACE_WORKLOAD_READY       = 4,
    TRACE_WORKLOAD_START       = 5,  /* first RUNNING */
    TRACE_WORKLOAD_END         = 6,  /* completion */
    TRACE_WORKLOAD_DEADLINE    = 7,  /* deadline check (arg0=slack ns, arg1=deadline) */
    TRACE_WORKLOAD_DEADLINE_MISS = 8,
    TRACE_WORKLOAD_ABORT       = 9,
    TRACE_WORKLOAD_BLOCK_BEGIN = 10, /* mutex/IPC block */
    TRACE_WORKLOAD_BLOCK_END   = 11,
    TRACE_PREEMPTION           = 12,
    TRACE_CPU_MIGRATION        = 13,
    /* Appended, never inserted: trace records are binary and the Qt
     * frontend may already map 0..13. */
    TRACE_GPIO_MARKER_HIGH     = 14, /* marker pin driven high */
    TRACE_GPIO_MARKER_LOW      = 15  /* marker pin driven low  */
} trace_event_type_t;

const char* trace_event_to_string(uint32_t type);

/* Fixed-size trace record — SHM ring buffer element.
 * Keep fixed-size for lock-free ring math and replay.
 */
typedef struct {
    uint64_t timestamp_ns;   /* monotonic */
    uint32_t event_type;     /* trace_event_type_t */
    uint32_t task_id;        /* 0 = system/external */
    uint32_t activation_id;  /* per-task monotonic activation counter */
    uint16_t cpu;            /* sched_getcpu() or 0xFFFF unknown */
    uint16_t priority;
    uint32_t correlation_id; /* CAN sequence → release → execution chain */
    uint64_t arg0;           /* e.g., can_id, exec_time, slack */
    uint64_t arg1;           /* e.g., payload len, deadline, block reason */
    uint32_t sequence;       /* MPSC publication sequence number */
    uint32_t reserved;       /* alignment pad */
} sched_trace_record_t;

/* No packing pragmas — natural alignment kept, 40 bytes on 64-bit.
 * Ensure size is multiple of 8 for mmap.
 */

typedef enum {
    TRACE_MODE_FULL       = 0, /* every task event */
    TRACE_MODE_SAMPLED    = 1, /* every Nth activation */
    TRACE_MODE_EVENT_ONLY = 2  /* external + misses only */
} trace_mode_t;

/* Shared-memory header — first bytes of SHM object */
#define TRACE_SHM_MAGIC   0x53434844u /* 'SCHD' */
#define TRACE_SHM_VERSION 1u

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t record_size;   /* sizeof(sched_trace_record_t) */
    uint32_t capacity;      /* records */
    uint32_t write_index;   /* monotonic, modulo capacity */
    uint32_t read_index;    /* for consumer */
    uint64_t records_written;
    uint64_t records_dropped;
    uint32_t high_watermark; /* max used slots */
    uint32_t sampling_n;    /* for SAMPLED */
    uint32_t mode;          /* trace_mode_t */
    uint64_t start_time_ns;
    uint32_t cpu_count;
    uint32_t reserved;
} trace_shm_header_t;

#ifdef __cplusplus
}
#endif

#endif /* TRACE_SCHEMA_H */
