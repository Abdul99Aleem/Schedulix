#ifndef QNX_TRACER_H
#define QNX_TRACER_H

#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Phase 4: QNX kernel trace control.
 * Wraps tracelogger and TraceEvent configuration.
 * On non-QNX/host, provides mock/synthetic hooks.
 */

int qnx_tracer_start(const char *kev_path, uint32_t flags); /* starts tracelogger */
int qnx_tracer_stop(void);
int qnx_tracer_is_running(void);
int qnx_tracer_check_privilege(void); /* 0 if root capable, else -1 */

/* Provenance: record .kev generation for manifest and to prevent stale data */
typedef struct {
    char kev_path[256];
    uint64_t start_ns;
    uint64_t end_ns;
    uint64_t file_size;
    uint32_t capture_config;
    int generated_by_current_run; /* 1 if mtime within run window */
    int privileged;
    int running;
    char error[128];
} qnx_provenance_t;

int qnx_tracer_get_provenance(qnx_provenance_t *out);
int qnx_tracer_delete_old(const char *kev_path); /* unlink stale */

/* Event capture flags (subset of _NTO_TRACE_* ) */
#define QNX_TRACE_THREAD   0x01
#define QNX_TRACE_INT      0x02
#define QNX_TRACE_KERNCALL 0x04
#define QNX_TRACE_IPC      0x08

/* For analyzer: load .kev (stub — on host synthesize) */
int qnx_tracer_load_kev(const char *path, void **out_buf, size_t *out_len);

/* Mock kernel trace injection for CI without QNX */
int qnx_tracer_inject_mock_thread_state(uint32_t tid, int state, uint64_t ts, int cpu);

#ifdef __cplusplus
}
#endif

#endif
