#ifndef MANIFEST_H
#define MANIFEST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char     scenario_id[32];
    char     qnx_version[64];
    char     bsp_version[64];
    char     version[16]; /* schedulix version */
    char     run_id[64];  /* timestamp-pid */
    uint32_t cpu_count;
    uint32_t sampling_mode;
    uint32_t sampling_n;
    uint32_t trace_capacity;
    uint64_t trace_duration_ns;
    uint32_t duration_ms;
    int      stress_level; /* -1 none, 0..95 */
    char     stress_config[256];
    char     cpu_affinity[64];
    char     event_sequence[256];
    char     trace_mode_str[16];
    uint64_t records_written;
    uint64_t records_dropped;
    char     kernel_trace_file[256];
    char     semantic_trace_file[256];
    char     analysis_output_file[256];
    char     kernel_provenance[256]; /* .kev size / generated_by_current_run */
    char     gpio_mode[16]; /* mock vs real */
} manifest_t;

void manifest_collect(const char *scenario_id, const char *semantic_trace, const char *kernel_trace, uint64_t duration_ns);
void manifest_collect_full(const char *scenario_id, int stress_level, uint32_t duration_ms, const char *semantic_trace, const char *kernel_trace, uint64_t duration_ns, const char *event_seq);
int  manifest_write_json(const char *path);
int  manifest_write_text(const char *path);
const manifest_t* manifest_get(void);

#ifdef __cplusplus
}
#endif

#endif
