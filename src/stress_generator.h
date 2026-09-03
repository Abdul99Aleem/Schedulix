#ifndef STRESS_GENERATOR_H
#define STRESS_GENERATOR_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STRESS_MAX_WORKERS 4
#define STRESS_NAME_LEN 16

typedef struct {
    char     name[STRESS_NAME_LEN];
    int      priority;        /* QNX prio */
    int      cpu_affinity;    /* -1 = none, 0..n */
    uint32_t period_ms;       /* period */
    uint32_t exec_ms;         /* execution budget per period */
    uint32_t target_util;     /* percent 0..100 (exec/period) */
} stress_worker_config_t;

/* dedicated stress workers — never modifies g_workload_table */
int stress_generator_init(void);
int stress_generator_start(const stress_worker_config_t *configs, int n);
int stress_generator_start_load(int percent); /* S1-A system-wide */
int stress_generator_start_load_pinned(int percent, int cpu); /* S1-B same-CPU */
int stress_generator_start_workers(int n, int base_prio, int period_ms, int exec_ms, int cpu_affinity);
int stress_generator_stop(void);
int stress_generator_active(void);
int stress_generator_get_configs(stress_worker_config_t *out, int max_n);

/* S2 resource contention helpers */
int stress_resource_init(void);
int stress_resource_low_hold(uint32_t hold_ms); /* LOW_TASK holds mutex */
int stress_resource_brake_try(uint32_t timeout_ms); /* BRAKE blocks */
void stress_resource_shutdown(void);

/* For manifest */
int stress_generator_describe(char *buf, size_t len);
int stress_generator_utilization(void); /* current target % */

#ifdef __cplusplus
}
#endif

#endif
